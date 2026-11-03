#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Game::LevelFileLayout
{
    // Where things are in a level file (L*LEV.MAP), for the tools that edit
    // one in place (LevelCleanup, LevelCleanupPlan). FORM header (12 bytes),
    // then MAP0 first: tag, big-endian size, data. MAP0's data: a 36-byte
    // header (vertex count +0, record count +2 - record 0 the blank - map
    // length +4, map width +6), the vertices (8 bytes), the face records (20
    // bytes: a, b, c, d as int32, texture +16, routine +18, light +19 whose bit
    // 0x80 ends a run) and the cell grid (16 bytes a cell, bytes 0-3 its run's
    // first record, FF FF FF FF for none).
    inline uint32_t Be32(const std::vector<uint8_t>& b, size_t p)
    {
        return (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) | (uint32_t(b[p + 2]) << 8) | uint32_t(b[p + 3]);
    }
    inline void PutBe32(std::vector<uint8_t>& b, size_t p, uint32_t v)
    {
        b[p] = uint8_t(v >> 24); b[p + 1] = uint8_t(v >> 16); b[p + 2] = uint8_t(v >> 8); b[p + 3] = uint8_t(v);
    }
    inline uint32_t Le32(const std::vector<uint8_t>& b, size_t p)
    {
        return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
    }
    inline void PutLe32(std::vector<uint8_t>& b, size_t p, uint32_t v)
    {
        b[p] = uint8_t(v); b[p + 1] = uint8_t(v >> 8); b[p + 2] = uint8_t(v >> 16); b[p + 3] = uint8_t(v >> 24);
    }
    inline uint16_t Le16(const std::vector<uint8_t>& b, size_t p) { return uint16_t(b[p] | (b[p + 1] << 8)); }
    inline void PutLe16(std::vector<uint8_t>& b, size_t p, uint16_t v) { b[p] = uint8_t(v); b[p + 1] = uint8_t(v >> 8); }

    inline constexpr uint32_t NO_RUN = 0xffffffffu;
    inline constexpr size_t HEADER_SIZE = 36, VERTEX_SIZE = 8, RECORD_SIZE = 20, CELL_SIZE = 16;
    inline constexpr uint8_t RUN_END = 0x80;

    struct Layout
    {
        size_t map0 = 12, data = 20;
        uint32_t map0Size = 0;
        uint16_t vertexCount = 0, recordCount = 0;
        int mapLength = 0, mapWidth = 0;
        size_t vertices = 0, records = 0, grid = 0, cells = 0, gridEnd = 0;

        int32_t RecordA(const std::vector<uint8_t>& b, size_t r) const { return static_cast<int32_t>(Le32(b, records + r * RECORD_SIZE)); }
        int32_t Corner(const std::vector<uint8_t>& b, size_t r, int k) const
        {
            return static_cast<int32_t>(Le32(b, records + r * RECORD_SIZE + size_t(k) * 4));
        }
        uint16_t Texture(const std::vector<uint8_t>& b, size_t r) const { return Le16(b, records + r * RECORD_SIZE + 16); }
        uint8_t Routine(const std::vector<uint8_t>& b, size_t r) const { return b[records + r * RECORD_SIZE + 18]; }
        uint8_t Light(const std::vector<uint8_t>& b, size_t r) const { return b[records + r * RECORD_SIZE + 19]; }
        uint32_t CellStart(const std::vector<uint8_t>& b, size_t c) const { return Le32(b, grid + c * CELL_SIZE); }
        int16_t Coordinate(const std::vector<uint8_t>& b, size_t v, int axis) const
        {
            return int16_t(Le16(b, vertices + v * VERTEX_SIZE + size_t(axis) * 2));
        }
    };

    inline bool Read(const std::vector<uint8_t>& bytes, Layout& out, std::string& error)
    {
        if (bytes.size() < 20 || bytes[0] != 'F' || bytes[1] != 'O' || bytes[2] != 'R' || bytes[3] != 'M'
            || bytes[12] != 'M' || bytes[13] != 'A' || bytes[14] != 'P' || bytes[15] != '0')
        {
            error = "not a level file (no FORM with MAP0 first)";
            return false;
        }
        Layout l;
        l.map0Size = Be32(bytes, l.map0 + 4);
        if (l.data + l.map0Size > bytes.size() || l.map0Size < HEADER_SIZE) { error = "MAP0 runs past the end"; return false; }
        l.vertexCount = Le16(bytes, l.data + 0);
        l.recordCount = Le16(bytes, l.data + 2);
        l.mapLength = Le16(bytes, l.data + 4);
        l.mapWidth = Le16(bytes, l.data + 6);
        l.vertices = l.data + HEADER_SIZE;
        l.records = l.vertices + size_t(l.vertexCount) * VERTEX_SIZE;
        l.grid = l.records + size_t(l.recordCount) * RECORD_SIZE;
        l.cells = size_t(l.mapLength) * size_t(l.mapWidth);
        l.gridEnd = l.grid + l.cells * CELL_SIZE;
        if (l.gridEnd > l.data + l.map0Size) { error = "the cell grid is not where the header puts it"; return false; }
        out = l;
        return true;
    }
}
