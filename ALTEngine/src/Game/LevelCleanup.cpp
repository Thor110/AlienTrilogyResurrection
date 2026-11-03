#include "LevelCleanup.h"

#include "LevelFileLayout.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace ALTEngine::Game::LevelCleanup
{
    using namespace LevelFileLayout;

    namespace
    {
        // The first cell's run begins at record 0: the face the file holds
        // there is the cell's first, though the cell's start of 0 - which the
        // walk and the drawing both take as "no faces" - hides it with the rest
        // of the run. Record 0 has to stay where it is, as the record the cells
        // left empty start at, so a copy of it goes in at 1, at the head of the
        // run the plan gives back, its end bit as it was (clear in the 35 levels
        // whose run goes on into record 1; set in the 10 whose first cell has
        // record 0 alone). Every face record after it moves up one;
        // nothing else changes. Only where the plan says (recordZero: the plan
        // judged record 0 seen, as it judges every face); false otherwise.
        bool CopyRecordZero(std::vector<uint8_t>& bytes, const Layout& lay, const Plan& plan)
        {
            if (!plan.recordZero || plan.firstCell < 0 || size_t(plan.firstCell) >= lay.cells || lay.recordCount < 2 || lay.recordCount == 0xffff) { return false; }
            if (lay.CellStart(bytes, size_t(plan.firstCell)) != 0 || lay.RecordA(bytes, 0) == -1) { return false; }

            std::vector<uint8_t> out(bytes.begin(), bytes.begin() + std::ptrdiff_t(lay.records + RECORD_SIZE));
            out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.records), bytes.begin() + std::ptrdiff_t(lay.records + RECORD_SIZE));
            out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.records + RECORD_SIZE), bytes.end());
            PutLe16(out, lay.data + 2, uint16_t(lay.recordCount + 1));
            const size_t grid = lay.grid + RECORD_SIZE;
            for (size_t c = 0; c < lay.cells; ++c)
            {
                const uint32_t start = Le32(out, grid + c * CELL_SIZE);
                if (start != NO_RUN && start != 0) { PutLe32(out, grid + c * CELL_SIZE, start + 1); }
            }
            PutBe32(out, lay.map0 + 4, uint32_t(lay.map0Size + RECORD_SIZE));
            PutBe32(out, 4, uint32_t(out.size() - 8));
            bytes = std::move(out);
            return true;
        }
    }

    namespace
    {
    // The removals (and record 0's copy): everything but the additions.
    bool Remove(std::vector<uint8_t>& bytes, const Plan& planIn, Report& report, std::string& error,
                std::vector<uint32_t>* oldRecordOfNew)
    {
        report = {};
        report.bytesBefore = report.bytesAfter = bytes.size();
        Layout lay;
        if (!Read(bytes, lay, error)) { return false; }
        const size_t cells = lay.cells;
        for (uint32_t r : planIn.remove)
        {
            if (r == 0 || r >= lay.recordCount) { error = "the plan names face record " + std::to_string(r) + ", which the file does not have"; return false; }
        }
        for (uint32_t c : planIn.clear)
        {
            if (c >= cells) { error = "the plan names cell " + std::to_string(c) + ", which the file does not have"; return false; }
        }
        if (planIn.firstCell >= int(cells)) { error = "the plan's first cell is past the map"; return false; }

        // Record 0's copy, when the first cell is given back (CopyRecordZero):
        // the plan's records from 1 on move up one with it.
        std::vector<uint8_t> original = bytes;
        Plan plan = planIn;
        const bool copied = CopyRecordZero(bytes, lay, planIn);
        if (copied)
        {
            for (uint32_t& r : plan.remove) { ++r; }
            if (!Read(bytes, lay, error)) { bytes = std::move(original); return false; }
        }
        const size_t recordCount = lay.recordCount;
        const uint16_t vertexCount = lay.vertexCount;

        // Each cell's start, the first cell's taken as 1 when it is given back.
        std::vector<uint32_t> start(cells);
        for (size_t c = 0; c < cells; ++c) { start[c] = lay.CellStart(bytes, c); }
        // Record 1 is the cell's only when record 0's run goes on into it (or
        // is the copy); on the levels where record 0 ends its run alone, 1
        // is another cell's.
        if (plan.firstCell >= 0 && start[size_t(plan.firstCell)] == 0 && recordCount > 1 && (copied || !(lay.Light(bytes, 0) & RUN_END)))
        {
            start[size_t(plan.firstCell)] = 1;
        }

        auto recordA = [&](size_t r) { return lay.RecordA(bytes, r); };
        auto light = [&](size_t r) { return lay.Light(bytes, r); };
        std::vector<int> owners(recordCount, 0);
        for (size_t c = 0; c < cells; ++c)
        {
            if (start[c] == NO_RUN || start[c] == 0 || start[c] >= recordCount) { continue; }
            for (size_t r = start[c]; r < recordCount; ++r)
            {
                if (recordA(r) == -1) { break; }
                ++owners[r];
                if (light(r) & RUN_END) { break; }
            }
        }

        std::vector<uint8_t> keep(recordCount, 1);
        for (uint32_t r : plan.remove) { keep[r] = 0; }
        std::vector<uint8_t> cleared(cells, 0);
        for (uint32_t c : plan.clear) { cleared[c] = 1; }

        // Each listed record's run end. Runs that share records share their
        // end, so a run that loses its last faces ends at its last kept one,
        // the same for every cell listing it.
        std::vector<uint32_t> runEnd(recordCount, NO_RUN);
        for (size_t r = 1; r < recordCount;)
        {
            if (recordA(r) == -1 || owners[r] == 0) { ++r; continue; }
            size_t e = r;
            while (e < recordCount && recordA(e) != -1 && owners[e] != 0 && !(light(e) & RUN_END)) { ++e; }
            if (e >= recordCount || recordA(e) == -1 || owners[e] == 0) { e = e - 1; }   // ran off the end: no end bit
            for (size_t k = r; k <= e; ++k) { runEnd[k] = uint32_t(e); }
            r = e + 1;
        }
        std::vector<uint8_t> endHere(recordCount, 0);
        for (size_t r = 1; r < recordCount; ++r)
        {
            if (runEnd[r] == NO_RUN || !keep[r]) { continue; }
            const size_t e = runEnd[r];
            size_t last = r;
            for (size_t k = r + 1; k <= e; ++k) { if (keep[k]) { last = k; } }
            // Its own end keeps its bit as it was (a run can end at a sentinel
            // instead); one that ends a run in place of removed faces gets it.
            if (last == r) { endHere[r] = r == e ? uint8_t((light(r) & RUN_END) != 0) : 1; }
        }
        auto keptStart = [&](size_t s) -> size_t {
            if (s == 0 || s >= recordCount || runEnd[s] == NO_RUN) { return 0; }
            for (size_t k = s; k <= runEnd[s]; ++k) { if (keep[k]) { return k; } }
            return 0;
        };

        // The new numbering.
        std::vector<uint32_t> newRecord(recordCount, NO_RUN);
        uint32_t nextRecord = 0;
        for (size_t r = 0; r < recordCount; ++r) { if (keep[r]) { newRecord[r] = nextRecord++; } }
        std::vector<uint8_t> vertexUsed(vertexCount, 0);
        // Record 0 too: it draws nothing, but its corners stay valid.
        for (size_t r = 0; r < recordCount; ++r)
        {
            if (!keep[r] || recordA(r) == -1) { continue; }
            for (int k = 0; k < 4; ++k)
            {
                const int32_t v = lay.Corner(bytes, r, k);
                if (v >= 0 && v < vertexCount) { vertexUsed[size_t(v)] = 1; }
            }
        }
        std::vector<int32_t> newVertex(vertexCount, -1);
        int32_t nextVertex = 0;
        for (size_t v = 0; v < vertexCount; ++v) { if (vertexUsed[v]) { newVertex[v] = nextVertex++; } }

        // The cells' new starts.
        std::vector<uint32_t> now(cells);
        bool startsMoved = false;
        for (size_t c = 0; c < cells; ++c)
        {
            const uint32_t was = lay.CellStart(bytes, c);
            uint32_t to = was;
            if (start[c] != NO_RUN && start[c] != 0 && start[c] < recordCount)
            {
                const size_t first = keptStart(start[c]);
                to = first ? newRecord[first] : (cleared[c] ? NO_RUN : 0u);
            }
            else if (start[c] == 0 && cleared[c])
            {
                to = NO_RUN;
            }
            now[c] = to;
            startsMoved = startsMoved || to != was;
        }
        bool endsMoved = false;
        for (size_t r = 1; r < recordCount; ++r)
        {
            if (keep[r] && runEnd[r] != NO_RUN && (endHere[r] != 0) != ((light(r) & RUN_END) != 0)) { endsMoved = true; }
        }

        report.facesRemoved = int(recordCount) - int(nextRecord);
        report.verticesRemoved = int(vertexCount) - nextVertex;
        if (plan.firstCell >= 0 && start[size_t(plan.firstCell)] == 1)
        {
            for (size_t r = 1; r <= runEnd[1] && runEnd[1] != NO_RUN; ++r) { report.firstCellFaces += keep[r]; }
            report.firstCellRestored = report.firstCellFaces > 0;
        }
        if (oldRecordOfNew)
        {
            oldRecordOfNew->clear();
            // As the input numbered them: the copy came from record 0.
            for (size_t r = 0; r < recordCount; ++r)
            {
                if (keep[r]) { oldRecordOfNew->push_back(uint32_t(copied && r > 0 ? r - 1 : r)); }
            }
        }
        if (!report.Changed() && !endsMoved && !startsMoved) { return true; }

        // Rebuild MAP0's data: header, kept vertices, kept records, the grid,
        // and everything after it as it was.
        std::vector<uint8_t> out(bytes.begin(), bytes.begin() + std::ptrdiff_t(lay.vertices));
        PutLe16(out, lay.data + 0, uint16_t(nextVertex));
        PutLe16(out, lay.data + 2, uint16_t(nextRecord));
        for (size_t v = 0; v < vertexCount; ++v)
        {
            if (!vertexUsed[v]) { continue; }
            const auto at = bytes.begin() + std::ptrdiff_t(lay.vertices + v * VERTEX_SIZE);
            out.insert(out.end(), at, at + VERTEX_SIZE);
        }
        for (size_t r = 0; r < recordCount; ++r)
        {
            if (!keep[r]) { continue; }
            const size_t at = out.size();
            out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.records + r * RECORD_SIZE),
                       bytes.begin() + std::ptrdiff_t(lay.records + (r + 1) * RECORD_SIZE));
            if (recordA(r) == -1) { continue; }
            if (r > 0 && runEnd[r] != NO_RUN) { out[at + 19] = uint8_t((out[at + 19] & ~RUN_END) | (endHere[r] ? RUN_END : 0)); }
            for (int k = 0; k < 4; ++k)
            {
                const int32_t v = static_cast<int32_t>(Le32(out, at + size_t(k) * 4));
                if (v >= 0 && v < vertexCount) { PutLe32(out, at + size_t(k) * 4, uint32_t(newVertex[size_t(v)])); }
            }
        }
        const size_t newGrid = out.size();
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.grid), bytes.begin() + std::ptrdiff_t(lay.gridEnd));
        for (size_t c = 0; c < cells; ++c) { PutLe32(out, newGrid + c * CELL_SIZE, now[c]); }
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.gridEnd), bytes.begin() + std::ptrdiff_t(lay.data + lay.map0Size));
        const uint32_t newMap0Size = uint32_t(out.size() - lay.data);
        PutBe32(out, lay.map0 + 4, newMap0Size);
        if (newMap0Size & 1u) { out.push_back(0); }
        // The chunks after MAP0, as they were.
        const size_t oldMap0End = lay.data + lay.map0Size + (lay.map0Size & 1u);
        if (oldMap0End < bytes.size()) { out.insert(out.end(), bytes.begin() + std::ptrdiff_t(oldMap0End), bytes.end()); }
        PutBe32(out, 4, uint32_t(out.size() - 8));

        // Every start written names a kept record; check rather than trust it.
        for (size_t c = 0; c < cells; ++c)
        {
            if (now[c] != NO_RUN && now[c] >= nextRecord) { error = "internal: a cell's run start names no record"; bytes = std::move(original); return false; }
        }

        report.bytesAfter = out.size();
        bytes = std::move(out);
        return true;
    }

    // One face put in (Addition): its corners matched to vertices or added,
    // the record written at the head of its cell's run, and everything that
    // names a record past it moved up one.
    bool AddFace(std::vector<uint8_t>& bytes, const Addition& add, Report& report, std::string& error, size_t& at)
    {
        Layout lay;
        if (!Read(bytes, lay, error)) { return false; }
        if (add.corners.size() < 3 || add.corners.size() > 4) { error = "a face to add needs 3 or 4 corners"; return false; }
        if (add.cellX < 0 || add.cellZ < 0 || add.cellX >= lay.mapLength || add.cellZ >= lay.mapWidth)
        {
            error = "a face to add names a cell past the map"; return false;
        }
        const size_t cell = size_t(add.cellZ) * size_t(lay.mapLength) + size_t(add.cellX);
        const uint32_t start = lay.CellStart(bytes, cell);
        if (start == NO_RUN || start == 0 || start >= lay.recordCount)
        {
            error = "the cell (" + std::to_string(add.cellX) + ", " + std::to_string(add.cellZ) + ") a face is added to has no run to join";
            return false;
        }
        if (lay.recordCount >= 0xffff) { error = "the level has no room for another face"; return false; }

        std::vector<std::array<int, 3>> fresh;      // corners with no vertex yet
        int32_t index[4] = { -1, -1, -1, -1 };
        for (size_t k = 0; k < add.corners.size(); ++k)
        {
            const auto& p = add.corners[k];
            for (int axis = 0; axis < 3; ++axis)
            {
                if (p[size_t(axis)] < -32768 || p[size_t(axis)] > 32767) { error = "a face to add has a corner out of range"; return false; }
            }
            for (size_t v = 0; v < lay.vertexCount && index[k] < 0; ++v)
            {
                if (lay.Coordinate(bytes, v, 0) == p[0] && lay.Coordinate(bytes, v, 1) == p[1] && lay.Coordinate(bytes, v, 2) == p[2]) { index[k] = int32_t(v); }
            }
            for (size_t f = 0; f < fresh.size() && index[k] < 0; ++f)
            {
                if (fresh[f] == p) { index[k] = int32_t(lay.vertexCount + f); }
            }
            if (index[k] < 0) { index[k] = int32_t(lay.vertexCount + fresh.size()); fresh.push_back(p); }
        }
        if (size_t(lay.vertexCount) + fresh.size() > 0xffff) { error = "the level has no room for more vertices"; return false; }

        std::vector<uint8_t> out(bytes.begin(), bytes.begin() + std::ptrdiff_t(lay.records));
        for (const auto& p : fresh)
        {
            const size_t v = out.size();
            out.resize(v + VERTEX_SIZE, 0);
            for (int axis = 0; axis < 3; ++axis) { PutLe16(out, v + size_t(axis) * 2, uint16_t(int16_t(p[size_t(axis)]))); }
        }
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.records), bytes.begin() + std::ptrdiff_t(lay.records + start * RECORD_SIZE));
        {
            const size_t r = out.size();
            out.resize(r + RECORD_SIZE, 0);
            for (int k = 0; k < 4; ++k) { PutLe32(out, r + size_t(k) * 4, uint32_t(index[k])); }
            PutLe16(out, r + 16, uint16_t(add.texture));
            out[r + 18] = uint8_t(add.routine);
            out[r + 19] = uint8_t(add.light & ~RUN_END);
        }
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(lay.records + start * RECORD_SIZE), bytes.end());
        PutLe16(out, lay.data + 0, uint16_t(lay.vertexCount + fresh.size()));
        PutLe16(out, lay.data + 2, uint16_t(lay.recordCount + 1));
        const size_t grid = lay.grid + fresh.size() * VERTEX_SIZE + RECORD_SIZE;
        for (size_t c = 0; c < lay.cells; ++c)
        {
            const uint32_t s = Le32(out, grid + c * CELL_SIZE);
            if (s == NO_RUN || s == 0 || s < start || (s == start && c == cell)) { continue; }
            PutLe32(out, grid + c * CELL_SIZE, s + 1);
        }
        const size_t grown = fresh.size() * VERTEX_SIZE + RECORD_SIZE;
        PutBe32(out, lay.map0 + 4, uint32_t(lay.map0Size + grown));
        PutBe32(out, 4, uint32_t(out.size() - 8));
        bytes = std::move(out);
        at = start;
        ++report.facesAdded;
        report.verticesAdded += int(fresh.size());
        return true;
    }
    }

    bool Apply(std::vector<uint8_t>& bytes, const Plan& plan, Report& report, std::string& error,
               std::vector<uint32_t>* oldRecordOfNew)
    {
        // The removals first, then the faces added: the order the work can be
        // read back in.
        std::vector<uint8_t> original = bytes;
        if (!Remove(bytes, plan, report, error, oldRecordOfNew)) { return false; }
        for (const Addition& add : plan.add)
        {
            size_t at = 0;
            if (!AddFace(bytes, add, report, error, at)) { bytes = std::move(original); return false; }
            if (oldRecordOfNew && at <= oldRecordOfNew->size()) { oldRecordOfNew->insert(oldRecordOfNew->begin() + std::ptrdiff_t(at), NO_RUN); }
        }
        report.bytesAfter = bytes.size();
        return true;
    }

    std::string WriteAdditions(const std::vector<Addition>& add)
    {
        nlohmann::ordered_json list = nlohmann::ordered_json::array();
        for (const Addition& a : add)
        {
            nlohmann::ordered_json e;
            e["corners"] = nlohmann::ordered_json::array();
            for (const auto& p : a.corners) { e["corners"].push_back({ p[0], p[1], p[2] }); }
            e["texture"] = a.texture;
            e["routine"] = a.routine;
            e["light"] = a.light;
            e["cell"] = { a.cellX, a.cellZ };
            list.push_back(e);
        }
        return list.dump();
    }

    bool ReadAdditions(const std::string& json, std::vector<Addition>& add, std::string& error)
    {
        add.clear();
        nlohmann::json list;
        try { list = nlohmann::json::parse(json); }
        catch (...) { error = "the faces to add cannot be read"; return false; }
        if (!list.is_array()) { error = "the faces to add are not a list"; return false; }
        auto number = [](const nlohmann::json& v, int& out) {
            if (!v.is_number_integer()) { return false; }
            out = v.get<int>();
            return true;
        };
        for (const auto& e : list)
        {
            Addition a;
            if (!e.is_object() || !e.contains("corners") || !e["corners"].is_array() || !e.contains("cell") || !e["cell"].is_array()
                || e["cell"].size() != 2 || !number(e["cell"][0], a.cellX) || !number(e["cell"][1], a.cellZ))
            {
                error = "a face to add lacks its corners or cell"; return false;
            }
            for (const auto& p : e["corners"])
            {
                std::array<int, 3> c{};
                if (!p.is_array() || p.size() != 3 || !number(p[0], c[0]) || !number(p[1], c[1]) || !number(p[2], c[2]))
                {
                    error = "a face to add has a corner that is not [x, y, z]"; return false;
                }
                a.corners.push_back(c);
            }
            if (a.corners.size() < 3 || a.corners.size() > 4) { error = "a face to add needs 3 or 4 corners"; return false; }
            if ((e.contains("texture") && !number(e["texture"], a.texture)) || (e.contains("routine") && !number(e["routine"], a.routine))
                || (e.contains("light") && !number(e["light"], a.light)))
            {
                error = "a face to add has a texture, routine or light that is not a number"; return false;
            }
            add.push_back(a);
        }
        return true;
    }

    std::string EncodeList(const std::vector<uint32_t>& values)
    {
        std::vector<uint32_t> v = values;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        std::string out;
        for (size_t i = 0; i < v.size();)
        {
            size_t j = i;
            while (j + 1 < v.size() && v[j + 1] == v[j] + 1) { ++j; }
            if (!out.empty()) { out += ','; }
            out += std::to_string(v[i]);
            if (j > i) { out += '-'; out += std::to_string(v[j]); }
            i = j + 1;
        }
        return out;
    }

    bool DecodeList(const std::string& text, std::vector<uint32_t>& values)
    {
        values.clear();
        size_t i = 0;
        auto number = [&](uint32_t& n) {
            if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) { return false; }
            uint64_t x = 0;
            while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
            {
                x = x * 10 + uint64_t(text[i++] - '0');
                if (x > 0xffffffffull) { return false; }
            }
            n = uint32_t(x);
            return true;
        };
        while (i < text.size())
        {
            uint32_t a = 0, b = 0;
            if (!number(a)) { return false; }
            b = a;
            if (i < text.size() && text[i] == '-') { ++i; if (!number(b) || b < a || b - a > 1000000) { return false; } }
            for (uint32_t x = a;; ++x) { values.push_back(x); if (x == b) { break; } }
            if (i < text.size()) { if (text[i] != ',') { return false; } ++i; if (i == text.size()) { return false; } }
        }
        return true;
    }

    std::vector<std::filesystem::path> LevelFiles(const std::filesystem::path& cdDirectory)
    {
        std::vector<std::filesystem::path> files;
        std::error_code ec;
        for (const auto& sect : std::filesystem::directory_iterator(cdDirectory, ec))
        {
            if (!sect.is_directory()) { continue; }
            const std::string dir = sect.path().filename().string();
            if (dir.rfind("SECT", 0) != 0) { continue; }
            for (const auto& f : std::filesystem::directory_iterator(sect.path(), ec))
            {
                const std::string name = f.path().filename().string();
                if (name.size() == 11 && name[0] == 'L' && name.substr(4) == "LEV.MAP")
                {
                    files.push_back(std::filesystem::path(dir) / name);
                }
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }
}
