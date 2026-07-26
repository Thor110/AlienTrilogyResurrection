#include "ModelLoader.h"
#include "BinaryReadLE.h"
#include "BndParser.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace ALTEngine::Formats
{
    namespace
    {
        std::vector<ModelMesh> ParseMeshes(const std::filesystem::path& bndPath)
        {
            const std::vector<BndSection>& allSections = BndParser::LoadCached(bndPath);
            std::vector<BndSection> modelSections;
            for (auto& section : allSections)
            {
                if (section.name.rfind("M0", 0) == 0) { modelSections.push_back(section); }
            }

            std::vector<ModelMesh> meshes;
            meshes.reserve(modelSections.size());

            for (const auto& section : modelSections)
            {
                const std::vector<uint8_t>& data = section.data;
                size_t pos = 0;

                if (data.size() < 12)
                {
                    throw std::runtime_error("ModelLoader: " + section.name + " too small for its 12-byte header in " + bndPath.string());
                }

                std::string tag(reinterpret_cast<const char*>(data.data()), 4);
                if (tag != "OBJ1")
                {
                    throw std::runtime_error("ModelLoader: " + section.name + " missing OBJ1 tag (found '" + tag + "') in " + bndPath.string());
                }
                pos += 4;

                pos += 4; // padding, always zero per the format notes - not validated, just skipped

                ModelMesh mesh;
                mesh.sectionName = section.name;
                std::memcpy(mesh.identifier.data(), data.data() + pos, 4);
                pos += 4;

                if (pos + 8 > data.size())
                {
                    throw std::runtime_error("ModelLoader: " + section.name + " too small for quad/vertex counts in " + bndPath.string());
                }
                int32_t quadCount = ReadInt32LE(data, pos);
                pos += 4;
                int32_t vertexCount = ReadInt32LE(data, pos);
                pos += 4;

                if (quadCount < 0 || vertexCount < 0)
                {
                    throw std::runtime_error("ModelLoader: " + section.name + " has a negative quad/vertex count in " + bndPath.string());
                }

                size_t quadsSize = static_cast<size_t>(quadCount) * 20;
                size_t verticesSize = static_cast<size_t>(vertexCount) * 8;
                if (pos + quadsSize + verticesSize > data.size())
                {
                    throw std::runtime_error(
                        "ModelLoader: " + section.name + " declares more quad/vertex data than it contains in " + bndPath.string());
                }

                mesh.quads.reserve(static_cast<size_t>(quadCount));
                for (int32_t i = 0; i < quadCount; ++i)
                {
                    ModelQuad q;
                    q.a = ReadInt32LE(data, pos + 0);
                    q.b = ReadInt32LE(data, pos + 4);
                    q.c = ReadInt32LE(data, pos + 8);
                    q.d = ReadInt32LE(data, pos + 12);
                    q.texIndex = ReadUInt16LE(data, pos + 16);
                    q.flags = data[pos + 18];
                    q.reserved = data[pos + 19];
                    mesh.quads.push_back(q);
                    pos += 20;
                }

                mesh.vertices.reserve(static_cast<size_t>(vertexCount));
                for (int32_t i = 0; i < vertexCount; ++i)
                {
                    ModelVertex v;
                    v.x = ReadInt16LE(data, pos + 0);
                    v.y = ReadInt16LE(data, pos + 2);
                    v.z = ReadInt16LE(data, pos + 4);
                    v.marker = ReadUInt16LE(data, pos + 6);
                    mesh.vertices.push_back(v);
                    pos += 8;
                }

                meshes.push_back(std::move(mesh));
            }

            return meshes;
        }
    }

    std::vector<ModelMesh> ModelLoader::Load(const std::filesystem::path& bndPath)
    {
        // Cached by path, at the fully-parsed level - not just the raw
        // BND sections (BndParser::LoadCached already handles that
        // layer). Edward, 2026: navigating the options menu means every
        // cursor move requests a different model from the SAME file
        // (OPTOBJ.BND has all 14 models in one container), which without
        // this would still re-parse all 14 models' geometry from
        // (already-cached) bytes on every single navigation step, even
        // though only one model is actually needed each time.
        static std::unordered_map<std::string, std::vector<ModelMesh>> cache;

        std::string key = std::filesystem::absolute(bndPath).lexically_normal().string();
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        std::vector<ModelMesh> meshes = ParseMeshes(bndPath);
        auto [inserted, _] = cache.emplace(std::move(key), meshes);
        return inserted->second;
    }

    const ModelMesh* ModelLoader::FindByNumber(const std::vector<ModelMesh>& meshes, int number)
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "M%03d", number);
        std::string target(buf);
        for (const auto& mesh : meshes)
        {
            if (mesh.sectionName == target) { return &mesh; }
        }
        return nullptr;
    }
}
