#include "TextureUse.h"

#include "LevelFileLayout.h"
#include "../Formats/LevelLoader.h"
#include "../Formats/RenderMesh.h"

namespace ALTEngine::Game::TextureUse
{
    bool Scan(const std::vector<uint8_t>& levelBytes, const std::vector<Formats::ModelMesh>& models, size_t descriptors,
              Result& result, std::string& error)
    {
        result = {};
        result.use.assign(descriptors, 0);
        LevelFileLayout::Layout lay;
        if (!LevelFileLayout::Read(levelBytes, lay, error)) { return false; }
        Formats::LevelGeometry level;
        try { level = Formats::LevelLoader::Parse(levelBytes, "texture use"); }
        catch (const std::exception& e) { error = e.what(); return false; }

        auto mark = [&](size_t tex, uint8_t why) {
            if (tex < descriptors) { result.use[tex] |= why; } else { ++result.outOfRange; }
        };
        // The frame programs, by group: listB words are { uint16 group, uint16 entry }.
        std::vector<std::vector<uint16_t>> programs;
        for (uint32_t word : level.animationFrames)
        {
            const uint16_t group = uint16_t(word & 0xffff), entry = uint16_t(word >> 16);
            if (group >= programs.size()) { programs.resize(size_t(group) + 1); }
            programs[group].push_back(entry);
            if ((entry & 0x8000) == 0) { mark(entry, FRAME); }
        }
        // An animated face's animator: listA words are { int16 program, ... }.
        auto animatorFrames = [&](size_t ordinal, uint8_t why) {
            if (ordinal >= level.textureAnimators.size()) { ++result.outOfRange; return; }
            const int16_t program = int16_t(level.textureAnimators[ordinal] & 0xffff);
            if (program < 0 || size_t(program) >= programs.size()) { return; }
            for (uint16_t entry : programs[size_t(program)]) { if ((entry & 0x8000) == 0) { mark(entry, why); } }
        };

        for (size_t r = 0; r < lay.recordCount; ++r)
        {
            if (lay.RecordA(levelBytes, r) == -1) { continue; }
            const uint16_t tex = lay.Texture(levelBytes, r);
            if (Formats::ClassifyDrawRoutine(lay.Routine(levelBytes, r)).animated) { animatorFrames(tex, FACE); }
            else { mark(tex, FACE); }
        }
        for (const auto& mesh : models)
        {
            for (const auto& q : mesh.quads)
            {
                if (Formats::ClassifyDrawRoutine(q.flags).animated) { animatorFrames(q.texIndex, MODEL); }
                else { mark(q.texIndex, MODEL); }
            }
        }
        return true;
    }
}
