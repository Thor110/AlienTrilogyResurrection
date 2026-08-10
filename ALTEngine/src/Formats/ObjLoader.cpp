#include "ObjLoader.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace ALTEngine::Formats
{
    namespace
    {
        // An OBJ index is 1-based, and may be negative to count back from the
        // most recent element. Returns -1 when out of range so the caller can
        // drop the face rather than read out of bounds.
        int ResolveIndex(int raw, size_t count)
        {
            if (raw > 0)
            {
                int zeroBased = raw - 1;
                return (static_cast<size_t>(zeroBased) < count) ? zeroBased : -1;
            }
            if (raw < 0)
            {
                int zeroBased = static_cast<int>(count) + raw;
                return (zeroBased >= 0) ? zeroBased : -1;
            }
            return -1; // 0 is not a legal OBJ index
        }

        // Parses one face vertex reference: "3", "3/7", "3//9" or "3/7/9".
        // Missing components come back as 0, which ResolveIndex rejects.
        void ParseFaceRef(const std::string& token, int& v, int& vt, int& vn)
        {
            v = vt = vn = 0;
            size_t first = token.find('/');
            if (first == std::string::npos)
            {
                v = std::atoi(token.c_str());
                return;
            }
            v = std::atoi(token.substr(0, first).c_str());

            size_t second = token.find('/', first + 1);
            if (second == std::string::npos)
            {
                vt = std::atoi(token.substr(first + 1).c_str());
                return;
            }
            if (second > first + 1)
            {
                vt = std::atoi(token.substr(first + 1, second - first - 1).c_str());
            }
            vn = std::atoi(token.substr(second + 1).c_str());
        }

        void LoadMaterialLibrary(const std::filesystem::path& mtlPath, ObjModel& model)
        {
            std::ifstream file(mtlPath);
            if (!file)
            {
                model.warnings.push_back("mtllib not found: " + mtlPath.string());
                return;
            }

            std::string line;
            ObjMaterial current;
            bool have = false;
            while (std::getline(file, line))
            {
                std::istringstream in(line);
                std::string keyword;
                if (!(in >> keyword)) { continue; }

                if (keyword == "newmtl")
                {
                    if (have) { model.materials.push_back(current); }
                    current = ObjMaterial{};
                    in >> current.name;
                    have = true;
                }
                else if (keyword == "map_Kd" && have)
                {
                    // Take the remainder of the line: texture paths can
                    // contain spaces, and any leading option flags are not
                    // something this project's files use.
                    std::string rest;
                    std::getline(in, rest);
                    size_t start = rest.find_first_not_of(" \t");
                    if (start != std::string::npos) { current.diffuseTexture = rest.substr(start); }
                }
            }
            if (have) { model.materials.push_back(current); }
        }
    }

    bool ObjLoader::Exists(const std::filesystem::path& objPath)
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(objPath, ec);
    }

    ObjModel ObjLoader::Load(const std::filesystem::path& objPath)
    {
        ObjModel model;

        std::ifstream file(objPath);
        if (!file)
        {
            // Absence is normal - overrides are optional. No warning.
            return model;
        }

        std::vector<std::array<float, 3>> positions;
        std::vector<std::array<float, 2>> texCoords;

        // Face groups keyed by material, so all faces sharing a texture end
        // up in one draw call. Insertion order is preserved via `order`.
        std::unordered_map<std::string, size_t> groupIndex;
        std::string currentMaterial;
        int currentLight = -1;
        int currentAnim = -1;

        // Keyed on the full signature, not just the material, so a group is
        // uniform in texture, light and animator.
        auto groupFor = [&](const std::string& material, int light, int anim) -> ObjFaceGroup& {
            std::string key = material + "|" + std::to_string(light) + "|" + std::to_string(anim);
            auto it = groupIndex.find(key);
            if (it != groupIndex.end()) { return model.groups[it->second]; }
            ObjFaceGroup group;
            group.materialName = material;
            group.lightId = light;
            group.animatorOrdinal = anim;
            model.groups.push_back(std::move(group));
            groupIndex[key] = model.groups.size() - 1;
            return model.groups.back();
        };

        size_t lineNumber = 0;
        size_t droppedFaces = 0;

        std::string line;
        while (std::getline(file, line))
        {
            lineNumber++;
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            if (line.empty() || line[0] == '#') { continue; }

            std::istringstream in(line);
            std::string keyword;
            if (!(in >> keyword)) { continue; }

            if (keyword == "v")
            {
                std::array<float, 3> p{ 0, 0, 0 };
                in >> p[0] >> p[1] >> p[2];
                positions.push_back(p);
            }
            else if (keyword == "vt")
            {
                std::array<float, 2> t{ 0, 0 };
                in >> t[0] >> t[1];
                // OBJ's V axis runs bottom-up; the texture pages here are
                // top-down, same as every other UV in this project.
                t[1] = 1.0f - t[1];
                texCoords.push_back(t);
            }
            else if (keyword == "alt_light")
            {
                int value = -1;
                in >> value;
                if (value < -1 || value > 127)
                {
                    model.warnings.push_back("alt_light " + std::to_string(value)
                                             + " out of range (0..127, or -1 for unlit) - ignored");
                }
                else { currentLight = value; }
            }
            else if (keyword == "alt_anim")
            {
                int value = -1;
                in >> value;
                if (value < -1)
                {
                    model.warnings.push_back("alt_anim " + std::to_string(value) + " is negative - ignored");
                }
                else { currentAnim = value; }
            }
            else if (keyword == "usemtl")
            {
                currentMaterial.clear();
                in >> currentMaterial;
            }
            else if (keyword == "mtllib")
            {
                std::string name;
                std::getline(in, name);
                size_t start = name.find_first_not_of(" \t");
                if (start != std::string::npos)
                {
                    LoadMaterialLibrary(objPath.parent_path() / name.substr(start), model);
                }
            }
            else if (keyword == "f")
            {
                std::vector<RenderVertex> corners;
                std::string token;
                bool ok = true;

                while (in >> token)
                {
                    int rawV = 0, rawVt = 0, rawVn = 0;
                    ParseFaceRef(token, rawV, rawVt, rawVn);

                    int vi = ResolveIndex(rawV, positions.size());
                    if (vi < 0) { ok = false; break; }

                    RenderVertex vertex{};
                    vertex.x = positions[static_cast<size_t>(vi)][0];
                    vertex.y = positions[static_cast<size_t>(vi)][1];
                    vertex.z = positions[static_cast<size_t>(vi)][2];

                    int ti = ResolveIndex(rawVt, texCoords.size());
                    if (ti >= 0)
                    {
                        vertex.u = texCoords[static_cast<size_t>(ti)][0];
                        vertex.v = texCoords[static_cast<size_t>(ti)][1];
                    }

                    // White here; the renderer replaces this with the light
                    // record's colour when the face carries an alt_light, and
                    // leaves it alone when it does not. White is the neutral
                    // that means "leave the texel alone".
                    vertex.r = vertex.g = vertex.b = 1.0f;

                    corners.push_back(vertex);
                }

                if (!ok || corners.size() < 3)
                {
                    droppedFaces++;
                    continue;
                }

                // Fan-triangulate. Correct for convex faces, which is all a
                // hand-made gap filler needs; a concave face would need ear
                // clipping and is not worth carrying until something asks for
                // it.
                ObjFaceGroup& group = groupFor(currentMaterial, currentLight, currentAnim);
                uint32_t base = static_cast<uint32_t>(group.vertices.size());
                for (const RenderVertex& corner : corners) { group.vertices.push_back(corner); }
                for (size_t i = 1; i + 1 < corners.size(); ++i)
                {
                    group.indices.push_back(base);
                    group.indices.push_back(base + static_cast<uint32_t>(i));
                    group.indices.push_back(base + static_cast<uint32_t>(i + 1));
                }
            }
            // v n, o, g, s and anything else: ignored by design.
        }

        if (droppedFaces > 0)
        {
            model.warnings.push_back("dropped " + std::to_string(droppedFaces)
                                     + " face(s) with out-of-range or missing vertex indices");
        }

        return model;
    }
}
