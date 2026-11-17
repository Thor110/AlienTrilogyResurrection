#include "LevelTrim.h"

#include <array>
#include <cstring>

namespace ALTEngine::Formats::LevelTrim
{
    namespace
    {
        // A dropped byte: its offset in the full record and what it is put back as.
        enum class Kind { Fixed, Zeroed, CrateFlag };
        struct Dropped { int offset; Kind kind; uint8_t value; };

        struct RecordType
        {
            int size;                       // in MAP0
            std::vector<Dropped> dropped;   // ascending offsets
            int Trimmed() const { return size - static_cast<int>(dropped.size()); }
        };

        const RecordType HEADER{ 36, { { 28, Kind::Fixed, 0x40 }, { 29, Kind::Fixed, 0x40 }, { 34, Kind::Zeroed, 0 }, { 35, Kind::Zeroed, 0 } } };
        const RecordType VERTEX{ 8, { { 6, Kind::Fixed, 0 }, { 7, Kind::Fixed, 0 } } };
        const RecordType FACE{ 20, {} };
        const RecordType CELL{ 16, { { 4, Kind::Fixed, 0 }, { 7, Kind::Fixed, 0 }, { 12, Kind::Zeroed, 0 } } };
        const RecordType PATH{ 8, { { 2, Kind::Fixed, 0 } } };
        const RecordType MONSTER{ 20, { { 9, Kind::Zeroed, 0 } } };
        const RecordType PICKUP{ 8, { { 5, Kind::Fixed, 0 }, { 7, Kind::Zeroed, 0 } } };
        const RecordType OBJECT{ 16, { { 5, Kind::CrateFlag, 0 }, { 11, Kind::Fixed, 0 }, { 13, Kind::Fixed, 0 }, { 15, Kind::Fixed, 0 } } };
        const RecordType DOOR{ 8, { { 5, Kind::Fixed, 0 } } };
        const RecordType LIFT{ 16, { { 4, Kind::Fixed, 0 }, { 6, Kind::Fixed, 1 }, { 11, Kind::Fixed, 0 },
                                     { 13, Kind::Zeroed, 0 }, { 14, Kind::Zeroed, 0 }, { 15, Kind::Zeroed, 0 } } };
        const RecordType ACTION{ 4, { { 3, Kind::Fixed, 0 } } };
        const RecordType LOGIC{ 4, {} };
        const RecordType LIGHT{ 28, { { 3, Kind::Fixed, 0 }, { 7, Kind::Fixed, 0 }, { 11, Kind::Fixed, 0 }, { 15, Kind::Fixed, 0 } } };
        const RecordType WORD{ 4, {} };

        // An object's +5: 10 on the crate types, 0 on every other.
        uint8_t CrateFlag(uint8_t type) { return (type == 20 || type == 25 || type == 29 || type == 32) ? 10 : 0; }

        uint8_t ExpectedValue(const Dropped& d, const uint8_t* fullRecord)
        {
            return d.kind == Kind::CrateFlag ? CrateFlag(fullRecord[2]) : d.value;
        }

        uint32_t Be32(const std::vector<uint8_t>& b, size_t p)
        {
            return (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) | (uint32_t(b[p + 2]) << 8) | uint32_t(b[p + 3]);
        }
        void PutBe32(std::vector<uint8_t>& b, size_t p, uint32_t v)
        {
            b[p] = uint8_t(v >> 24); b[p + 1] = uint8_t(v >> 16); b[p + 2] = uint8_t(v >> 8); b[p + 3] = uint8_t(v);
        }

        // The file's level chunk: where its data starts and how long it is.
        bool FindChunk(const std::vector<uint8_t>& file, const char* tag, size_t& at, size_t& length)
        {
            if (file.size() < 12 || std::memcmp(file.data(), "FORM", 4) != 0) { return false; }
            size_t pos = 12;
            while (pos + 8 <= file.size())
            {
                const uint32_t n = Be32(file, pos + 4);
                if (pos + 8 + n > file.size()) { return false; }
                if (std::memcmp(file.data() + pos, tag, 4) == 0) { at = pos + 8; length = n; return true; }
                pos += 8 + n + (n & 1);
            }
            return false;
        }

        // The record lists in order, with their counts, read from a header
        // in either layout (`trimmed`: the 32-byte one).
        struct Section { const RecordType* type; size_t count; };
        std::vector<Section> Sections(const uint8_t* header, bool trimmed)
        {
            auto u16 = [&](int p) { return size_t(header[p] | (header[p + 1] << 8)); };
            const size_t frameLists = trimmed ? u16(28) : u16(30);
            return {
                { &HEADER, 1 },
                { &VERTEX, u16(0) },
                { &FACE, u16(2) },
                { &CELL, u16(4) * u16(6) },
                { &PATH, size_t(header[12]) },
                { &MONSTER, u16(14) },
                { &PICKUP, u16(16) },
                { &OBJECT, u16(18) },
                { &DOOR, u16(20) },
                { &LIFT, u16(22) },
                { &ACTION, 64 },
                { &LOGIC, 64 },
                { &LIGHT, 128 },
                { &WORD, u16(26) },
                { &WORD, frameLists },
            };
        }

        // The file with its level chunk replaced: the new tag and data, the
        // chunk padded to an even length, the FORM size following.
        std::vector<uint8_t> Replace(const std::vector<uint8_t>& file, size_t at, size_t length, const char* tag,
                                     const std::vector<uint8_t>& data)
        {
            std::vector<uint8_t> out(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(at - 8));
            out.insert(out.end(), tag, tag + 4);
            out.resize(out.size() + 4);
            PutBe32(out, out.size() - 4, static_cast<uint32_t>(data.size()));
            out.insert(out.end(), data.begin(), data.end());
            if (data.size() & 1) { out.push_back(0); }
            const size_t after = at + length + (length & 1);
            if (after < file.size()) { out.insert(out.end(), file.begin() + static_cast<std::ptrdiff_t>(after), file.end()); }
            PutBe32(out, 4, static_cast<uint32_t>(out.size() - 8));
            return out;
        }

        // MAP0 data to MAPT data. `normalize`: instead, MAP0 with the zeroed
        // bytes at 0 (what Expand gives back).
        bool TrimData(const uint8_t* data, size_t length, std::vector<uint8_t>& out, bool normalize, std::string& error)
        {
            if (length < 36) { error = "the level's header is cut short"; return false; }
            out.clear();
            out.reserve(length);
            size_t pos = 0;
            for (const Section& s : Sections(data, false))
            {
                const size_t size = static_cast<size_t>(s.type->size);
                if (pos + s.count * size > length) { error = "the level's records run past its MAP0 chunk"; return false; }
                for (size_t r = 0; r < s.count; ++r, pos += size)
                {
                    const uint8_t* record = data + pos;
                    size_t next = 0;
                    for (int k = 0; k < s.type->size; ++k)
                    {
                        const Dropped* d = next < s.type->dropped.size() && s.type->dropped[next].offset == k ? &s.type->dropped[next] : nullptr;
                        if (!d) { out.push_back(record[k]); continue; }
                        ++next;
                        if (d->kind != Kind::Zeroed && record[k] != ExpectedValue(*d, record))
                        {
                            error = "a byte the trim takes out is not its usual value (record of " + std::to_string(s.type->size)
                                    + " bytes, +" + std::to_string(k) + ")";
                            return false;
                        }
                        if (normalize) { out.push_back(d->kind == Kind::Zeroed ? 0 : record[k]); }
                    }
                }
            }
            if (pos != length) { error = "the level's MAP0 chunk does not end where its counts say"; return false; }
            return true;
        }
    }

    bool IsTrimmed(const std::vector<uint8_t>& file)
    {
        size_t at = 0, length = 0;
        return FindChunk(file, TRIMMED_TAG, at, length);
    }

    bool Trim(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error)
    {
        size_t at = 0, length = 0;
        if (!FindChunk(file, UNTRIMMED_TAG, at, length)) { error = IsTrimmed(file) ? "already trimmed" : "not a level (no MAP0)"; return false; }
        std::vector<uint8_t> data;
        if (!TrimData(file.data() + at, length, data, false, error)) { return false; }
        out = Replace(file, at, length, TRIMMED_TAG, data);
        return true;
    }

    bool Normalized(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error)
    {
        size_t at = 0, length = 0;
        if (!FindChunk(file, UNTRIMMED_TAG, at, length)) { error = "not a level (no MAP0)"; return false; }
        std::vector<uint8_t> data;
        if (!TrimData(file.data() + at, length, data, true, error)) { return false; }
        out = Replace(file, at, length, UNTRIMMED_TAG, data);
        return true;
    }

    bool Expand(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error)
    {
        size_t at = 0, length = 0;
        if (!FindChunk(file, TRIMMED_TAG, at, length)) { out = file; return true; }
        if (length < 32) { error = "the trimmed level's header is cut short"; return false; }
        const uint8_t* data = file.data() + at;
        std::vector<uint8_t> full;
        full.reserve(length + length / 8);
        size_t pos = 0;
        for (const Section& s : Sections(data, true))
        {
            const size_t size = static_cast<size_t>(s.type->Trimmed());
            if (pos + s.count * size > length) { error = "the trimmed level's records run past its chunk"; return false; }
            for (size_t r = 0; r < s.count; ++r, pos += size)
            {
                const size_t start = full.size();
                full.resize(start + static_cast<size_t>(s.type->size));
                size_t next = 0, from = pos;
                std::vector<const Dropped*> later;   // put in once the record's kept bytes are there
                for (int k = 0; k < s.type->size; ++k)
                {
                    if (next < s.type->dropped.size() && s.type->dropped[next].offset == k) { later.push_back(&s.type->dropped[next++]); continue; }
                    full[start + static_cast<size_t>(k)] = data[from++];
                }
                for (const Dropped* d : later)
                {
                    full[start + static_cast<size_t>(d->offset)] = d->kind == Kind::Zeroed ? 0 : ExpectedValue(*d, full.data() + start);
                }
            }
        }
        // A pad byte the chunk may end in is not data.
        if (pos != length) { error = "the trimmed level's chunk does not end where its counts say"; return false; }
        out = Replace(file, at, length, UNTRIMMED_TAG, full);
        return true;
    }
}
