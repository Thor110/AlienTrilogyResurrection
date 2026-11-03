#include "LevelCleanup.h"

#include "CreatureSight.h"
#include "PlayerMover.h"
#include "../Formats/LevelLoader.h"

#include <algorithm>
#include <exception>

namespace ALTEngine::Game::LevelCleanup
{
    namespace
    {
        uint32_t Be32(const std::vector<uint8_t>& b, size_t p)
        {
            return (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) | (uint32_t(b[p + 2]) << 8) | uint32_t(b[p + 3]);
        }
        void PutBe32(std::vector<uint8_t>& b, size_t p, uint32_t v)
        {
            b[p] = uint8_t(v >> 24); b[p + 1] = uint8_t(v >> 16); b[p + 2] = uint8_t(v >> 8); b[p + 3] = uint8_t(v);
        }
        uint32_t Le32(const std::vector<uint8_t>& b, size_t p)
        {
            return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
        }
        void PutLe32(std::vector<uint8_t>& b, size_t p, uint32_t v)
        {
            b[p] = uint8_t(v); b[p + 1] = uint8_t(v >> 8); b[p + 2] = uint8_t(v >> 16); b[p + 3] = uint8_t(v >> 24);
        }
        uint16_t Le16(const std::vector<uint8_t>& b, size_t p) { return uint16_t(b[p] | (b[p + 1] << 8)); }
        void PutLe16(std::vector<uint8_t>& b, size_t p, uint16_t v) { b[p] = uint8_t(v); b[p + 1] = uint8_t(v >> 8); }

        constexpr uint32_t NO_RUN = 0xffffffffu;
        constexpr size_t HEADER_SIZE = 36, VERTEX_SIZE = 8, RECORD_SIZE = 20, CELL_SIZE = 16;
        constexpr uint8_t RUN_END = 0x80;   // the face light byte's end-of-run bit (+0x13)
    }

    bool Clean(std::vector<uint8_t>& bytes, bool multiplayer, Report& report, std::string& error)
    {
        report = {};
        report.bytesBefore = report.bytesAfter = bytes.size();

        // FORM header (12 bytes), then MAP0 first: tag, big-endian size, data.
        if (bytes.size() < 20 || bytes[0] != 'F' || bytes[1] != 'O' || bytes[2] != 'R' || bytes[3] != 'M'
            || bytes[12] != 'M' || bytes[13] != 'A' || bytes[14] != 'P' || bytes[15] != '0')
        {
            error = "not a level file (no FORM with MAP0 first)";
            return false;
        }
        const size_t map0 = 12;
        const uint32_t map0Size = Be32(bytes, map0 + 4);
        const size_t data = map0 + 8;
        if (data + map0Size > bytes.size() || map0Size < HEADER_SIZE) { error = "MAP0 runs past the end"; return false; }

        Formats::LevelGeometry level;
        try { level = Formats::LevelLoader::Parse(bytes, "level"); }
        catch (const std::exception& e) { error = e.what(); return false; }

        const uint16_t vertexCount = Le16(bytes, data + 0);
        const uint16_t recordCount = Le16(bytes, data + 2);   // record 0 is the blank
        const int mapLength = Le16(bytes, data + 4), mapWidth = Le16(bytes, data + 6);
        const size_t vertices = data + HEADER_SIZE;
        const size_t records = vertices + size_t(vertexCount) * VERTEX_SIZE;
        const size_t grid = records + size_t(recordCount) * RECORD_SIZE;
        const size_t cells = size_t(mapLength) * size_t(mapWidth);
        const size_t gridEnd = grid + cells * CELL_SIZE;
        if (gridEnd > data + map0Size || cells != level.collisionGrid.size()) { error = "the cell grid is not where the header puts it"; return false; }

        auto recordA = [&](size_t r) { return static_cast<int32_t>(Le32(bytes, records + r * RECORD_SIZE)); };
        auto recordLight = [&](size_t r) { return bytes[records + r * RECORD_SIZE + 19]; };
        auto cellStart = [&](size_t c) { return Le32(bytes, grid + c * CELL_SIZE); };

        // Each cell's run: from its start to the record with the end bit.
        std::vector<int> owners(recordCount, 0);          // how many runs list a record
        std::vector<uint8_t> keep(recordCount, 0);
        auto forRun = [&](size_t start, auto&& visit) {
            for (size_t r = start; r < recordCount; ++r)
            {
                if (recordA(r) == -1) { break; }
                visit(r);
                if (recordLight(r) & RUN_END) { break; }
            }
        };

        // Where the player can be, and what the view walk ever takes in from there.
        std::vector<uint8_t> reach(cells, 0);
        std::vector<int> queue;
        auto seed = [&](int x, int z) {
            const int i = Formats::CellIndex(level, x, z);
            if (i >= 0 && !reach[size_t(i)]) { reach[size_t(i)] = 1; queue.push_back(i); }
        };
        seed(level.header.playerStartX, level.header.playerStartY);
        if (multiplayer) { for (const auto& n : level.pathNodes) { seed(n.x, n.y); } }
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int i = queue[head], x = i % mapLength, z = i / mapLength;
            static constexpr int STEP[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (const auto& d : STEP)
            {
                const int j = Formats::CellIndex(level, x + d[0], z + d[1]);
                if (j < 0 || reach[size_t(j)]) { continue; }
                if (PlayerMover::WallAttribute(level.collisionGrid[size_t(j)].attribute)) { continue; }
                reach[size_t(j)] = 1;
                queue.push_back(j);
            }
        }
        std::vector<uint8_t> drawable(cells, 0);
        for (size_t i = 0; i < cells; ++i)
        {
            if (!reach[i]) { continue; }
            const auto walk = CreatureSight::DrawnCells(level, int(i % size_t(mapLength)), int(i / size_t(mapLength)), 0, 0x800);
            for (size_t k = 0; k < cells && k < walk.size(); ++k) { drawable[k] |= walk[k]; }
        }

        for (size_t c = 0; c < cells; ++c)
        {
            const uint32_t start = cellStart(c);
            if (start == NO_RUN || start == 0 || start >= recordCount) { continue; }
            forRun(start, [&](size_t r) { owners[r]++; if (drawable[c]) { keep[r] = 1; } });
        }
        // Add faces: the first cell with faces whose start is 0, when records
        // 1.. are unlisted and end in the run bit. Its run is then judged as
        // any cell's: kept and drawn if the cell can be seen, trimmed if not
        // (L321-L324 and L902's first cells are out of every view).
        size_t firstCell = cells;
        for (size_t c = 0; c < cells; ++c) { if (cellStart(c) == 0) { firstCell = c; break; } }
        int firstRun = 0;
        if (firstCell < cells && recordCount > 1 && owners[1] == 0 && recordA(1) != -1)
        {
            bool ended = false;
            for (size_t r = 1; r < recordCount && !ended; ++r)
            {
                if (owners[r] != 0 || recordA(r) == -1) { break; }
                ++firstRun;
                ended = (recordLight(r) & RUN_END) != 0;
            }
            if (!ended) { firstRun = 0; }
        }
        for (int k = 1; k <= firstRun; ++k)
        {
            owners[size_t(k)] = 1;
            if (drawable[firstCell]) { keep[size_t(k)] = 1; }
        }
        if (firstRun > 0 && !drawable[firstCell]) { firstRun = -firstRun; }   // trimmed with it, not restored

        keep[0] = 1;                                         // the blank record
        for (size_t r = 1; r < recordCount; ++r)
        {
            if (owners[r] == 0 || recordA(r) == -1) { keep[r] = 1; }   // unlisted, or an end sentinel
        }

        // The new numbering.
        std::vector<uint32_t> newRecord(recordCount, NO_RUN);
        uint32_t nextRecord = 0;
        for (size_t r = 0; r < recordCount; ++r) { if (keep[r]) { newRecord[r] = nextRecord++; } }
        std::vector<uint8_t> vertexUsed(vertexCount, 0);
        for (size_t r = 1; r < recordCount; ++r)
        {
            if (!keep[r]) { continue; }
            for (int k = 0; k < 4; ++k)
            {
                const int32_t v = static_cast<int32_t>(Le32(bytes, records + r * RECORD_SIZE + size_t(k) * 4));
                if (v >= 0 && v < vertexCount) { vertexUsed[size_t(v)] = 1; }
            }
        }
        std::vector<int32_t> newVertex(vertexCount, -1);
        int32_t nextVertex = 0;
        for (size_t v = 0; v < vertexCount; ++v) { if (vertexUsed[v]) { newVertex[v] = nextVertex++; } }

        report.facesRemoved = int(recordCount) - int(nextRecord);
        report.verticesRemoved = int(vertexCount) - nextVertex;
        report.firstCellRestored = firstRun > 0;
        report.firstCellFaces = firstRun > 0 ? firstRun : 0;
        if (!report.Changed()) { return true; }

        // Rebuild MAP0's data: header, kept vertices, kept records, the grid,
        // and everything after it as it was.
        std::vector<uint8_t> out(bytes.begin(), bytes.begin() + std::ptrdiff_t(data));
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(data), bytes.begin() + std::ptrdiff_t(vertices));
        PutLe16(out, data + 0, uint16_t(nextVertex));
        PutLe16(out, data + 2, uint16_t(nextRecord));
        for (size_t v = 0; v < vertexCount; ++v)
        {
            if (!vertexUsed[v]) { continue; }
            const auto at = bytes.begin() + std::ptrdiff_t(vertices + v * VERTEX_SIZE);
            out.insert(out.end(), at, at + VERTEX_SIZE);
        }
        for (size_t r = 0; r < recordCount; ++r)
        {
            if (!keep[r]) { continue; }
            const size_t at = out.size();
            out.insert(out.end(), bytes.begin() + std::ptrdiff_t(records + r * RECORD_SIZE),
                       bytes.begin() + std::ptrdiff_t(records + (r + 1) * RECORD_SIZE));
            if (r == 0 || recordA(r) == -1) { continue; }
            for (int k = 0; k < 4; ++k)
            {
                const int32_t v = static_cast<int32_t>(Le32(out, at + size_t(k) * 4));
                if (v >= 0 && v < vertexCount) { PutLe32(out, at + size_t(k) * 4, uint32_t(newVertex[size_t(v)])); }
            }
        }
        const size_t newGrid = out.size();
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(grid), bytes.begin() + std::ptrdiff_t(gridEnd));
        for (size_t c = 0; c < cells; ++c)
        {
            const uint32_t start = cellStart(c);
            uint32_t now = start;
            if (start != NO_RUN && start != 0 && start < recordCount) { now = newRecord[start]; }
            if (c == firstCell && firstRun > 0) { now = newRecord[1]; }
            PutLe32(out, newGrid + c * CELL_SIZE, now);
        }
        out.insert(out.end(), bytes.begin() + std::ptrdiff_t(gridEnd), bytes.begin() + std::ptrdiff_t(data + map0Size));
        const uint32_t newMap0Size = uint32_t(out.size() - data);
        PutBe32(out, map0 + 4, newMap0Size);
        if (newMap0Size & 1u) { out.push_back(0); }
        // The chunks after MAP0, as they were.
        const size_t oldMap0End = data + map0Size + (map0Size & 1u);
        if (oldMap0End < bytes.size()) { out.insert(out.end(), bytes.begin() + std::ptrdiff_t(oldMap0End), bytes.end()); }
        PutBe32(out, 4, uint32_t(out.size() - 8));

        // A record a run started at was always kept with its run, so every
        // start maps; check rather than trust it.
        for (size_t c = 0; c < cells; ++c)
        {
            const uint32_t start = cellStart(c);
            if (start != NO_RUN && start != 0 && start < recordCount && drawable[c] && newRecord[start] == NO_RUN)
            {
                error = "internal: a drawn cell's run lost its first face";
                return false;
            }
        }

        report.bytesAfter = out.size();
        bytes = std::move(out);
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
