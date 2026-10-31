#include "ModelRenderer.h"
#include "Mat4.h"
#include "../Bootstrap/PlatformPaths.h"
#include "../Formats/BndParser.h"
#include "../Formats/BndTextureLoader.h"
#include "../Formats/ModelLoader.h"
#include "../Formats/RenderMesh.h"
#include "../Formats/ObjLoader.h"

#include <cctype>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace ALTEngine::Renderer
{
    namespace
    {
        // For logging only - ModelCacheKey itself stays a cheap
        // catalog+int pair, not a string, but SDL_Log's %s calls still
        // want something printable.
        std::string DescribeCacheKey(ModelCacheKey key)
        {
            const char* catalogName = (key.catalog == ModelCatalog::Optobj) ? "OPTOBJ"
                                     : (key.catalog == ModelCatalog::Pickmod) ? "PICKMOD"
                                     : "OBJ3D";
            return std::string(catalogName) + ":" + std::to_string(key.modelIndex);
        }

        // Not M_PI - that's a POSIX/GNU extension, not standard C++, and
        // MSVC only defines it if _USE_MATH_DEFINES is set before
        // <cmath> is included, which is easy to get wrong across
        // translation units. Just define our own.
        constexpr float PI = 3.14159265358979323846f;
    }

    using ALTEngine::Bootstrap::ExecutableDirectory;
    using ALTEngine::Formats::BndParser;
    using ALTEngine::Formats::BndTextureLoader;
    using ALTEngine::Formats::BndTextureSet;
    using ALTEngine::Formats::ModelLoader;
    using ALTEngine::Formats::RenderMesh;
    using ALTEngine::Formats::BuildRenderMesh;

    namespace
    {
        struct LoadedModel
        {
            SDL_GPUBuffer* vertexBuffer = nullptr;
            SDL_GPUBuffer* indexBuffer = nullptr;
            uint32_t indexCount = 0;
            // Indices from here on are the see-through faces (RenderMesh::
            // translucentStart); equal to indexCount when there are none.
            uint32_t translucentFirst = 0;
            SDL_GPUTexture* texture = nullptr;
            float centerX = 0, centerY = 0, centerZ = 0; // AABB center, for auto-framing the camera
            float radius = 1.0f;
        std::vector<ModelRenderer::ModelFace> faces;                          // AABB bounding-sphere radius
            float baseRotationRadians = 0.0f;              // fixed per-model orientation offset - see LoadModel's doc comment
            bool useDoubleSided = false;                   // true for colour-key cutout models - see LoadModel's doc comment
        };

        struct LevelSubGroup
        {
            SDL_GPUBuffer* vertexBuffer = nullptr;
            SDL_GPUBuffer* indexBuffer = nullptr;
            uint32_t indexCount = 0;
            SDL_GPUTexture* texture = nullptr; // nullptr if this group is unused by the level (0 quads)

            // Drawing only some cells (SetLevelDrawnCells): the indices and
            // the face each triangle came from, kept on the CPU, and a second
            // index buffer holding just the triangles of the cells drawn.
            std::vector<uint32_t> cpuIndices;
            std::vector<uint32_t> triangleFace;
            SDL_GPUBuffer* visibleIndexBuffer = nullptr;
            uint32_t visibleCount = 0;
            uint64_t builtSerial = ~0ull;

            // The vertices as uploaded, with each one's light and corner
            // (RenderMesh::vertexLight), so a light change only recolours
            // them (TickLevelLights).
            std::vector<ALTEngine::Formats::RenderVertex> cpuVertices;
            std::vector<uint8_t> vertexLight;
            std::vector<uint8_t> vertexCorner;
        };

        struct LoadedLevel
        {
            std::array<LevelSubGroup, 5> groups;

            // Retained so the light colours can be re-resolved and the
            // vertex buffers rebuilt when a blink record changes state.
            // The alternative would be storing the light colour in a
            // uniform, but a draw call covers a whole texture page and
            // therefore many different light records, so it genuinely has
            // to live in the vertex data.
            ALTEngine::Formats::LevelGeometry geometry;
            std::vector<ALTEngine::Formats::BxRectangle> uvRects;
            std::vector<ALTEngine::Formats::FaceUvRotation> rotations;
            ALTEngine::Formats::LightTable lights;
            ALTEngine::Formats::TextureAnimator animator;

            // Last resolved entry table, to detect whether a tick
            // actually changed anything. Blink countdowns are tens of
            // ticks long, so the great majority of ticks change nothing
            // and must not trigger a re-upload of ~45k vertices.
            std::vector<ALTEngine::Formats::LightTable::Entry> lastEntries;

            // Same idea for the texture animators: only rebuild when an
            // animator's output descriptor actually changed.
            std::vector<uint16_t> lastAnimatorOutputs;

            // Vertex counts at load. An animated face redirected to a
            // descriptor on a DIFFERENT page would move between per-page
            // buffers and change these, which the in-place vertex re-upload
            // cannot express. Every animator in L111 keeps all of its output
            // frames on one page, so this does not happen there - but it is
            // checked rather than assumed.
            std::array<size_t, 5> groupVertexCounts{};

            // The faces of the other two fills (Formats::FaceFill), one
            // sub-group per page each, with their own copy of the page keyed
            // on palette word 0: cut-out faces (routines 4-6, 26, 28) drawn
            // with the ordinary pipeline, see-through ones (12, 13, 34, 30,
            // 32) with the half-blend pipeline after everything else.
            std::array<LevelSubGroup, 5> cutoutGroups;
            std::array<LevelSubGroup, 5> translucentGroups;
            std::array<size_t, 5> cutoutVertexCounts{};
            std::array<size_t, 5> translucentVertexCounts{};

            // -GAP override geometry. Hand-made faces that fill holes the
            // original level data leaves - most visibly the slit above a door
            // frame, which the original could never show because it had no
            // free look. One sub-group per texture page used.
            std::array<LevelSubGroup, 5> gapGroups;

            // Which cell each level face belongs to, and the cells to draw
            // (empty: every face). The serial moves when the set does.
            std::vector<int32_t> faceCell;
            std::vector<uint8_t> drawnCells;
            uint64_t drawnSerial = 0;

            // Set when an override .obj replaced the level's geometry. The
            // light tick must not rebuild vertex buffers in that case - the
            // .MAP quads it would rebuild from are no longer what is on screen.
            bool geometryOverridden = false;
        };

        // Uploaded sprite sheets, by key. Sprites reuse the model pipeline
        // wholesale - same vertex format, same shaders - so nothing here needs a
        // new pipeline or a new shader binary.
        struct SpriteSheet
        {
            SDL_GPUTexture* texture = nullptr;
            int width = 0;
            int height = 0;
        };
        std::unordered_map<std::string, SpriteSheet> spriteSheets;

        struct OverlayPixels { std::vector<uint8_t> rgba; int width; int height; };
        std::unordered_map<std::string, OverlayPixels> overlayPixels;
        std::unordered_map<std::string, SDL_Texture*> overlayTextures;

        // One dynamic vertex/index buffer, grown on demand, refilled each frame
        // with the billboard quads. Sprites are transient by nature, so there is
        // nothing to cache between frames.
        SDL_GPUBuffer* spriteVertexBuffer = nullptr;
        SDL_GPUBuffer* spriteIndexBuffer = nullptr;
        // The sprites' per-frame upload, kept and cycled rather than made
        // and released every frame; grown as the sprite buffers are.
        SDL_GPUTransferBuffer* spriteTransfer = nullptr;
        size_t spriteTransferSize = 0;
        // The level's culled index lists' upload (RenderLevelToRgba).
        SDL_GPUTransferBuffer* cullUpload = nullptr;
        size_t cullUploadSize = 0;
        size_t spriteBufferCapacity = 0;   // in quads

        SDL_GPUDevice* device = nullptr;
        SDL_GPUGraphicsPipeline* pipeline = nullptr;           // cull_mode=BACK - correct for solid, opaque geometry
        SDL_GPUGraphicsPipeline* doubleSidedPipeline = nullptr; // cull_mode=NONE - see LoadModel's doc comment on why colour-key cutout models need this
        // The see-through fill (draw routines 12, 13, 34, 30, 32): the texel
        // and what is already there averaged, (src + dst) / 2, as span routine
        // 0x5db67 does - a constant blend of one half. No depth write, so it
        // never hides what is drawn behind it afterwards.
        SDL_GPUGraphicsPipeline* translucentPipeline = nullptr;
        // Two samplers, chosen by the Graphics > Quality option. `sampler`
        // points at whichever is active; every binding site uses it, so
        // switching is a single pointer assignment with no pipeline rebuild.
        SDL_GPUSampler* sampler = nullptr;         // active
        SDL_GPUSampler* samplerLinear = nullptr;   // Smoothed
        SDL_GPUSampler* samplerNearest = nullptr;  // Original - hard texel edges

        // Draw-distance fade enable, pushed to the fragment shader each pass.
        // Default ON, i.e. the original's darkness, matching the Render
        // Distance modern feature defaulting off.
        float fogEnabled = 1.0f;

        // Where the fade starts and how far it takes to reach black, in world
        // units.
        //
        // The SHAPE is exactly FUN_0004ecdc's:
        //     factor = 0xff - clamp(dist / 0x50 - 0xaa, 0, 0xff)
        // a linear ramp beginning once dist/0x50 passes 0xaa and reaching full
        // black 0xff steps later. So in the original's own distance units the
        // ramp runs from 0xaa * 0x50 to (0xaa + 0xff) * 0x50.
        //
        // THE UNIT CONVERSION, TRACED. The level face walker (0x25880) has
        // FUN_00048490 store the average of the face's four view depths (the
        // camera-space z it projects by, in world units), shifts it left by 2
        // (0x258d1) and hands that to FUN_0004ecdc. So the ramp runs over view
        // depth from 0xaa * 0x50 / 4 = 3400 units (about 6.6 cells) to
        // (0xaa + 0xff) * 0x50 / 4 = 8500 (about 16.6 cells). The shader's
        // distance is the clip-space W, the same view depth.
        //
        // This was 8 before, matched by eye to a screenshot of the original
        // (black about 8-10 cells down a corridor) - the corridor's own
        // lighting will have darkened it further than the fade alone.
        static constexpr float FOG_DISTANCE_DIVISOR = 4.0f;
        float fogStart = (0xaa * 0x50) / FOG_DISTANCE_DIVISOR;          // 3400 units, ~6.6 cells
        float fogRange = (0xff * 0x50) / FOG_DISTANCE_DIVISOR;          // 5100 units, black at ~16.6 cells
        SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        std::unordered_map<ModelCacheKey, LoadedModel, ModelCacheKeyHash> loadedModels;
        std::unordered_map<std::string, LoadedLevel> loadedLevels;

        // Offscreen render target cache - recreated only if the requested
        // size changes, since the menu is expected to call RenderToRgba
        // with a constant size every frame.
        SDL_GPUTexture* colorTarget = nullptr;
        SDL_GPUTexture* depthTarget = nullptr;
        SDL_GPUTransferBuffer* downloadBuffer = nullptr;
        int targetWidth = 0, targetHeight = 0;

        std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open()) { return {}; }
            auto size = static_cast<size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            return data;
        }

        // One shader format per backend: DXIL for Direct3D 12 (Windows),
        // SPIR-V for Vulkan (Linux, and Windows if SDL picks it) and MSL for
        // Metal (macOS). DXIL first keeps Windows on what it has always used.
        SDL_GPUShaderFormat PickShaderFormat(SDL_GPUShaderFormat backendFormats)
        {
            if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) { return SDL_GPU_SHADERFORMAT_DXIL; }
            if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) { return SDL_GPU_SHADERFORMAT_SPIRV; }
            if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) { return SDL_GPU_SHADERFORMAT_MSL; }
            return SDL_GPU_SHADERFORMAT_INVALID;
        }

        SDL_GPUShader* LoadShader(SDL_GPUDevice* dev, const std::filesystem::path& path, SDL_GPUShaderStage stage,
                                   Uint32 numSamplers, Uint32 numUniformBuffers)
        {
            std::vector<uint8_t> code = ReadFile(path);
            if (code.empty())
            {
                SDL_Log("ModelRenderer: could not read shader %s", path.string().c_str());
                return nullptr;
            }

            const SDL_GPUShaderFormat format = PickShaderFormat(SDL_GetGPUShaderFormats(dev));
            if (format == SDL_GPU_SHADERFORMAT_INVALID)
            {
                SDL_Log("ModelRenderer: no supported shader format (need DXIL, SPIR-V or MSL)");
                return nullptr;
            }

            SDL_GPUShaderCreateInfo info{};
            info.code = code.data();
            info.code_size = code.size();
            // spirv-cross names the MSL entry point main0: "main" isn't
            // allowed in Metal.
            info.entrypoint = format == SDL_GPU_SHADERFORMAT_MSL ? "main0" : "main";
            info.format = format;
            info.stage = stage;
            info.num_samplers = numSamplers;
            info.num_uniform_buffers = numUniformBuffers;

            return SDL_CreateGPUShader(dev, &info);
        }

        // Uploads a CPU-side mesh + decoded texture to GPU buffers -
        // shared by LoadModel and LoadLevel (each of a level's up to 5
        // sub-groups needs the exact same vertex/index/texture upload
        // sequence a single model does). Returns a zeroed LevelSubGroup
        // (all nullptr) on failure - callers should check
        // texture/vertexBuffer/indexBuffer before trusting the result.
        // Re-uploads ONLY sub-groups' vertex data, reusing their buffers.
        // Used by the light tick: the geometry, indices and texture are
        // unchanged, and a group whose vertex count stays the same keeps
        // a buffer of the right size.
        // The light tick's re-uploads all go through one transfer buffer kept
        // from tick to tick (cycled on each map, so the GPU can still be
        // reading the last one) and one command buffer, instead of a new
        // buffer and a submit per page and fill.
        SDL_GPUTransferBuffer* vertexUploadBuffer = nullptr;
        size_t vertexUploadSize = 0;

        using VertexUpload = std::pair<const LevelSubGroup*, const std::vector<ALTEngine::Formats::RenderVertex>*>;
        void ReuploadVerticesBatch(const std::vector<VertexUpload>& uploads)
        {
            size_t total = 0;
            for (const auto& [group, vertices] : uploads) { total += vertices->size() * sizeof(ALTEngine::Formats::RenderVertex); }
            if (total == 0) { return; }
            if (vertexUploadBuffer && vertexUploadSize < total)
            {
                SDL_ReleaseGPUTransferBuffer(device, vertexUploadBuffer);
                vertexUploadBuffer = nullptr;
            }
            const bool cycle = vertexUploadBuffer != nullptr;
            if (!vertexUploadBuffer)
            {
                SDL_GPUTransferBufferCreateInfo transferInfo{};
                transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                transferInfo.size = static_cast<Uint32>(total);
                vertexUploadBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
                if (!vertexUploadBuffer) { vertexUploadSize = 0; return; }
                vertexUploadSize = total;
            }
            auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, vertexUploadBuffer, cycle));
            if (!mapped) { return; }
            size_t offset = 0;
            for (const auto& [group, vertices] : uploads)
            {
                const size_t size = vertices->size() * sizeof(ALTEngine::Formats::RenderVertex);
                std::memcpy(mapped + offset, vertices->data(), size);
                offset += size;
            }
            SDL_UnmapGPUTransferBuffer(device, vertexUploadBuffer);

            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
            if (!cmd) { return; }
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
            offset = 0;
            for (const auto& [group, vertices] : uploads)
            {
                const size_t size = vertices->size() * sizeof(ALTEngine::Formats::RenderVertex);
                SDL_GPUTransferBufferLocation src{ vertexUploadBuffer, static_cast<Uint32>(offset) };
                SDL_GPUBufferRegion dst{ group->vertexBuffer, 0, static_cast<Uint32>(size) };
                SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
                offset += size;
            }
            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(cmd);
        }


        // Stamps a light record's resolved colour over every vertex of a mesh.
        // lightId < 0 leaves the mesh alone (white, i.e. unlit).
        void ApplyLightToMesh(ALTEngine::Formats::RenderMesh& mesh, int lightId,
                              const ALTEngine::Formats::LightTable& lights)
        {
            if (lightId < 0) { return; }
            const ALTEngine::Formats::LightTable::Entry& entry = lights.ColourFor(lightId);
            // Corner 0 for all vertices: gouraud (mode 4) needs a per-corner
            // assignment that an arbitrary OBJ triangle has no ordering for, so
            // an override face is flat-lit. Worth revisiting if a replacement
            // ever needs gouraud.
            float r = static_cast<float>(entry.corner[0].r) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
            float g = static_cast<float>(entry.corner[0].g) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
            float b = static_cast<float>(entry.corner[0].b) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
            for (ALTEngine::Formats::RenderVertex& v : mesh.vertices) { v.r = r; v.g = g; v.b = b; }
        }

        // Case-insensitive lookup of a child entry inside a directory.
        // Returns an empty path when there is no match.
        //
        // The original game's folders are all upper case (CD, GFX, SECT11...)
        // so OVERRIDE is written that way throughout, but Linux and any
        // case-sensitive mount would otherwise turn a capitalisation slip into
        // a silently missing override. Scanning the directory costs nothing at
        // level load and makes the case genuinely not matter.
        std::filesystem::path FindEntryCaseInsensitive(const std::filesystem::path& parent, const std::string& name)
        {
            std::error_code ec;
            if (!std::filesystem::is_directory(parent, ec)) { return {}; }

            // Exact hit first - no scan needed in the common case.
            std::filesystem::path exact = parent / name;
            if (std::filesystem::exists(exact, ec)) { return exact; }

            auto lower = [](std::string v) {
                for (char& c : v) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
                return v;
            };
            std::string wanted = lower(name);

            for (const auto& entry : std::filesystem::directory_iterator(parent, ec))
            {
                if (lower(entry.path().filename().string()) == wanted) { return entry.path(); }
            }
            return {};
        }

        // Resolves an override .obj for a level file, following the convention
        // Edward's real install tree uses: OVERRIDE lives INSIDE CD, and the
        // category is the level's own section folder.
        //
        //   .../CD/SECT11/L111LEV.MAP  ->  .../CD/OVERRIDE/SECT11/L111LEV<suffix>.obj
        //
        // `suffix` is "-GAP" for gap fillers, or empty for a full replacement
        // of the level geometry. Every path component is matched
        // case-insensitively, so OVERRIDE / Override / override all work and
        // so does any casing of the section folder and file name. Falls back to
        // the level's own folder last, which is handy while authoring.
        // Returns an empty path when nothing is there - the normal case.
        std::filesystem::path FindOverrideObj(const std::filesystem::path& mapPath, const std::string& suffix)
        {
            std::string name = mapPath.stem().string() + suffix + ".obj";
            std::string section = mapPath.parent_path().filename().string();
            std::filesystem::path cdRoot = mapPath.parent_path().parent_path();

            std::filesystem::path overrideDir = FindEntryCaseInsensitive(cdRoot, "OVERRIDE");
            if (!overrideDir.empty())
            {
                std::filesystem::path sectionDir = FindEntryCaseInsensitive(overrideDir, section);
                if (!sectionDir.empty())
                {
                    std::filesystem::path file = FindEntryCaseInsensitive(sectionDir, name);
                    if (!file.empty() && ALTEngine::Formats::ObjLoader::Exists(file)) { return file; }
                }
            }

            std::filesystem::path beside = FindEntryCaseInsensitive(mapPath.parent_path(), name);
            if (!beside.empty() && ALTEngine::Formats::ObjLoader::Exists(beside)) { return beside; }

            return {};
        }

        // Uploads a mesh's vertex/index buffers and nothing else. The caller
        // supplies the texture, which for -GAP geometry is borrowed from the
        // level's own pages rather than owned.
        LevelSubGroup UploadMeshBuffersOnly(const RenderMesh& renderMesh)
        {
            LevelSubGroup group;
            if (renderMesh.vertices.empty() || renderMesh.indices.empty()) { return group; }

            size_t vbSize = renderMesh.vertices.size() * sizeof(ALTEngine::Formats::RenderVertex);
            size_t ibSize = renderMesh.indices.size() * sizeof(uint32_t);

            SDL_GPUBufferCreateInfo vbInfo{};
            vbInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            vbInfo.size = static_cast<Uint32>(vbSize);
            group.vertexBuffer = SDL_CreateGPUBuffer(device, &vbInfo);

            SDL_GPUBufferCreateInfo ibInfo{};
            ibInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
            ibInfo.size = static_cast<Uint32>(ibSize);
            group.indexBuffer = SDL_CreateGPUBuffer(device, &ibInfo);

            if (!group.vertexBuffer || !group.indexBuffer)
            {
                if (group.vertexBuffer) { SDL_ReleaseGPUBuffer(device, group.vertexBuffer); group.vertexBuffer = nullptr; }
                if (group.indexBuffer) { SDL_ReleaseGPUBuffer(device, group.indexBuffer); group.indexBuffer = nullptr; }
                return group;
            }

            SDL_GPUTransferBufferCreateInfo transferInfo{};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = static_cast<Uint32>(vbSize + ibSize);
            SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
            if (!transfer) { return group; }

            uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false));
            if (mapped)
            {
                std::memcpy(mapped, renderMesh.vertices.data(), vbSize);
                std::memcpy(mapped + vbSize, renderMesh.indices.data(), ibSize);
                SDL_UnmapGPUTransferBuffer(device, transfer);

                SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
                SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
                SDL_GPUTransferBufferLocation vSrc{ transfer, 0 };
                SDL_GPUBufferRegion vDst{ group.vertexBuffer, 0, static_cast<Uint32>(vbSize) };
                SDL_UploadToGPUBuffer(copyPass, &vSrc, &vDst, false);
                SDL_GPUTransferBufferLocation iSrc{ transfer, static_cast<Uint32>(vbSize) };
                SDL_GPUBufferRegion iDst{ group.indexBuffer, 0, static_cast<Uint32>(ibSize) };
                SDL_UploadToGPUBuffer(copyPass, &iSrc, &iDst, false);
                SDL_EndGPUCopyPass(copyPass);
                SDL_SubmitGPUCommandBuffer(cmd);
            }
            SDL_ReleaseGPUTransferBuffer(device, transfer);

            group.indexCount = static_cast<uint32_t>(renderMesh.indices.size());
            return group;
        }

        LevelSubGroup UploadMeshWithTexture(const RenderMesh& renderMesh, const ALTEngine::Formats::BndTexture& tex)
        {
            LevelSubGroup group;
            if (renderMesh.vertices.empty() || renderMesh.indices.empty()) { return group; }

            size_t vbSize = renderMesh.vertices.size() * sizeof(ALTEngine::Formats::RenderVertex);
            size_t ibSize = renderMesh.indices.size() * sizeof(uint32_t);
            size_t texSize = static_cast<size_t>(tex.width) * tex.height * 4;

            SDL_GPUBufferCreateInfo vbInfo{ SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(vbSize), 0 };
            SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(device, &vbInfo);
            SDL_GPUBufferCreateInfo ibInfo{ SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(ibSize), 0 };
            SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(device, &ibInfo);

            SDL_GPUTextureCreateInfo texInfo{};
            texInfo.type = SDL_GPU_TEXTURETYPE_2D;
            texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            texInfo.width = static_cast<Uint32>(tex.width);
            texInfo.height = static_cast<Uint32>(tex.height);
            texInfo.layer_count_or_depth = 1;
            texInfo.num_levels = 1;
            texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);

            if (!vertexBuffer || !indexBuffer || !texture)
            {
                SDL_Log("UploadMeshWithTexture: buffer/texture creation failed: %s", SDL_GetError());
                if (vertexBuffer) { SDL_ReleaseGPUBuffer(device, vertexBuffer); }
                if (indexBuffer) { SDL_ReleaseGPUBuffer(device, indexBuffer); }
                if (texture) { SDL_ReleaseGPUTexture(device, texture); }
                return group;
            }

            SDL_GPUTransferBufferCreateInfo transferInfo{};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = static_cast<Uint32>(vbSize + ibSize + texSize);
            SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

            uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false));
            std::memcpy(mapped, renderMesh.vertices.data(), vbSize);
            std::memcpy(mapped + vbSize, renderMesh.indices.data(), ibSize);
            std::memcpy(mapped + vbSize + ibSize, tex.rgba.data(), texSize);
            SDL_UnmapGPUTransferBuffer(device, transfer);

            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

            SDL_GPUTransferBufferLocation vbSrc{ transfer, 0 };
            SDL_GPUBufferRegion vbDst{ vertexBuffer, 0, static_cast<Uint32>(vbSize) };
            SDL_UploadToGPUBuffer(copyPass, &vbSrc, &vbDst, false);

            SDL_GPUTransferBufferLocation ibSrc{ transfer, static_cast<Uint32>(vbSize) };
            SDL_GPUBufferRegion ibDst{ indexBuffer, 0, static_cast<Uint32>(ibSize) };
            SDL_UploadToGPUBuffer(copyPass, &ibSrc, &ibDst, false);

            SDL_GPUTextureTransferInfo texSrc{};
            texSrc.transfer_buffer = transfer;
            texSrc.offset = static_cast<Uint32>(vbSize + ibSize);
            SDL_GPUTextureRegion texDst{};
            texDst.texture = texture;
            texDst.w = static_cast<Uint32>(tex.width);
            texDst.h = static_cast<Uint32>(tex.height);
            texDst.d = 1;
            SDL_UploadToGPUTexture(copyPass, &texSrc, &texDst, false);

            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(cmd);
            SDL_ReleaseGPUTransferBuffer(device, transfer);

            group.vertexBuffer = vertexBuffer;
            group.indexBuffer = indexBuffer;
            group.indexCount = static_cast<uint32_t>(renderMesh.indices.size());
            group.texture = texture;
            return group;
        }

        // Keeps what a level group needs to draw only some cells' faces: its
        // indices and their faces, and a GPU index buffer of the same size
        // for the culled list. A mesh without face data (an override) is
        // always drawn whole.
        void AttachCulling(LevelSubGroup& group, const RenderMesh& mesh)
        {
            if (group.visibleIndexBuffer) { SDL_ReleaseGPUBuffer(device, group.visibleIndexBuffer); group.visibleIndexBuffer = nullptr; }
            group.cpuIndices.clear();
            group.triangleFace.clear();
            group.builtSerial = ~0ull;
            const bool lit = mesh.trackLights && mesh.vertexLight.size() == mesh.vertices.size()
                          && mesh.vertexCorner.size() == mesh.vertices.size();
            group.cpuVertices = lit ? mesh.vertices : std::vector<ALTEngine::Formats::RenderVertex>{};
            group.vertexLight = lit ? mesh.vertexLight : std::vector<uint8_t>{};
            group.vertexCorner = lit ? mesh.vertexCorner : std::vector<uint8_t>{};
            if (mesh.indices.empty() || mesh.triangleFace.size() * 3 != mesh.indices.size()) { return; }
            SDL_GPUBufferCreateInfo info{ SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(mesh.indices.size() * sizeof(uint32_t)), 0 };
            group.visibleIndexBuffer = SDL_CreateGPUBuffer(device, &info);
            if (!group.visibleIndexBuffer) { return; }
            group.cpuIndices = mesh.indices;
            group.triangleFace = mesh.triangleFace;
        }

        void ReleaseCulling(LevelSubGroup& group)
        {
            if (group.visibleIndexBuffer) { SDL_ReleaseGPUBuffer(device, group.visibleIndexBuffer); group.visibleIndexBuffer = nullptr; }
            group.cpuIndices.clear();
            group.triangleFace.clear();
            group.cpuVertices.clear();
            group.vertexLight.clear();
            group.vertexCorner.clear();
        }

        // Ensures colorTarget/depthTarget/downloadBuffer exist and match
        // the requested size, recreating them if not - shared by
        // RenderToRgba (small model previews) and RenderLevelToRgba
        // (a full level view, typically much larger). Returns false
        // (logs why) if creation fails.
        bool EnsureRenderTarget(int width, int height)
        {
            if (width == targetWidth && height == targetHeight && colorTarget) { return true; }

            if (colorTarget) { SDL_ReleaseGPUTexture(device, colorTarget); }
            if (depthTarget) { SDL_ReleaseGPUTexture(device, depthTarget); }
            if (downloadBuffer) { SDL_ReleaseGPUTransferBuffer(device, downloadBuffer); }

            SDL_GPUTextureCreateInfo colorInfo{};
            colorInfo.type = SDL_GPU_TEXTURETYPE_2D;
            colorInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            colorInfo.width = static_cast<Uint32>(width);
            colorInfo.height = static_cast<Uint32>(height);
            colorInfo.layer_count_or_depth = 1;
            colorInfo.num_levels = 1;
            colorInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            colorTarget = SDL_CreateGPUTexture(device, &colorInfo);

            SDL_GPUTextureCreateInfo depthInfo{};
            depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
            depthInfo.format = depthFormat;
            depthInfo.width = static_cast<Uint32>(width);
            depthInfo.height = static_cast<Uint32>(height);
            depthInfo.layer_count_or_depth = 1;
            depthInfo.num_levels = 1;
            depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            depthTarget = SDL_CreateGPUTexture(device, &depthInfo);

            SDL_GPUTransferBufferCreateInfo dlInfo{};
            dlInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            dlInfo.size = static_cast<Uint32>(width * height * 4);
            downloadBuffer = SDL_CreateGPUTransferBuffer(device, &dlInfo);

            targetWidth = width;
            targetHeight = height;

            if (!colorTarget || !depthTarget || !downloadBuffer)
            {
                SDL_Log("EnsureRenderTarget: render target creation failed: %s", SDL_GetError());
                return false;
            }
            return true;
        }

        // Downloads colorTarget (after a completed render pass in `cmd`)
        // to CPU as tightly-packed RGBA8 bytes - shared final step of
        // both RenderToRgba and RenderLevelToRgba.
        bool DownloadColorTarget(SDL_GPUCommandBuffer* cmd, int width, int height, std::vector<uint8_t>& result)
        {
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTextureRegion srcRegion{};
            srcRegion.texture = colorTarget;
            srcRegion.w = static_cast<Uint32>(width);
            srcRegion.h = static_cast<Uint32>(height);
            srcRegion.d = 1;
            SDL_GPUTextureTransferInfo dlDst{};
            dlDst.transfer_buffer = downloadBuffer;
            dlDst.offset = 0;
            SDL_DownloadFromGPUTexture(copyPass, &srcRegion, &dlDst);
            SDL_EndGPUCopyPass(copyPass);

            SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
            SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);

            void* mapped = SDL_MapGPUTransferBuffer(device, downloadBuffer, false);
            if (!mapped) { result.clear(); return false; }
            // Resizing a buffer the caller keeps from frame to frame touches
            // nothing once it is the right size (a fresh one each frame was
            // a whole frame's worth of zeroing and new pages).
            result.resize(static_cast<size_t>(width) * height * 4);
            std::memcpy(result.data(), mapped, result.size());
            SDL_UnmapGPUTransferBuffer(device, downloadBuffer);
            return true;
        }
        std::vector<uint8_t> DownloadColorTarget(SDL_GPUCommandBuffer* cmd, int width, int height)
        {
            std::vector<uint8_t> result;
            DownloadColorTarget(cmd, width, height, result);
            return result;
        }
    }

    bool ModelRenderer::Initialize()
    {
        // Idempotent - safe (and cheap) to call every time a screen
        // needs a model, rather than each screen caching its own "did I
        // already try" flag. Those per-screen flags are what caused
        // "Options no longer displays models after a gameplay session" -
        // GameplayScreen calls Shutdown() when it exits (correctly, see
        // its own comment on why), but MenuController's own cached
        // "already initialized" flag had no way to know that happened,
        // so it never re-initialized and every subsequent LoadModel call
        // silently failed against a null device (Edward, 2026).
        if (device) { return true; }

        device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
                                     false, nullptr);
        if (!device)
        {
            SDL_Log("ModelRenderer::Initialize: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return false;
        }

        // The shader format decides which compiled file to load: .dxil from
        // the HLSL (dxc), .spv from the GLSL twin (glslangValidator) and .msl
        // converted from that SPIR-V (spirv-cross). See data/shaders.
        const SDL_GPUShaderFormat shaderFormat = PickShaderFormat(SDL_GetGPUShaderFormats(device));
        std::filesystem::path shaderDir = ExecutableDirectory() / "data" / "shaders";
        std::string ext = shaderFormat == SDL_GPU_SHADERFORMAT_DXIL ? ".dxil"
                        : shaderFormat == SDL_GPU_SHADERFORMAT_SPIRV ? ".spv" : ".msl";

        SDL_GPUShader* vertexShader = LoadShader(device, shaderDir / ("model.vert" + ext), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader* fragmentShader = LoadShader(device, shaderDir / ("model.frag" + ext), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader) { SDL_ReleaseGPUShader(device, vertexShader); }
            if (fragmentShader) { SDL_ReleaseGPUShader(device, fragmentShader); }
            SDL_DestroyGPUDevice(device);
            device = nullptr;
            return false;
        }

        if (SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        {
            depthFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        }
        else
        {
            depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        }

        SDL_GPUVertexBufferDescription vbDesc{};
        vbDesc.slot = 0;
        vbDesc.pitch = sizeof(float) * 8; // x,y,z,u,v,r,g,b
        vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        // Attribute 2 is the per-vertex light colour (see RenderVertex).
        // Kept as FLOAT3 rather than a packed UNORM byte triple: the
        // vertex buffers here are rebuilt rarely and small enough that
        // the 4 bytes per vertex saved are not worth the packing code.
        SDL_GPUVertexAttribute attrs[3]{};
        attrs[0] = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 };
        attrs[1] = { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, sizeof(float) * 3 };
        attrs[2] = { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, sizeof(float) * 5 };

        SDL_GPUColorTargetDescription colorDesc{};
        colorDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 3;
        pipelineInfo.vertex_input_state.vertex_attributes = attrs;
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorDesc;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = depthFormat;
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        // BACK culling, front_face=CCW - empirically confirmed (Edward,
        // 2026): rendered the speaker model (thin, open cone geometry -
        // a good test case unlike mostly-closed shapes like Keyboard,
        // where culling barely changes anything either way) with
        // CULLMODE_NONE, FRONTFACE_CCW+BACK, and FRONTFACE_CW+BACK.
        // CCW+BACK came back virtually pixel-identical to no culling at
        // all (same bounding box, 1-pixel rounding difference) - meaning
        // it only ever discards genuinely-hidden back faces. CW+BACK
        // incorrectly discarded ~half the visible surface (bounding box
        // shrank by roughly half, 16771 pixels of real geometry went
        // missing). This was the root cause behind "the backside of the
        // speaker and much of the computer is misaligned" - both faces
        // rendering simultaneously with no culling, fighting via depth,
        // looked like misaligned UVs but was actually a missing-culling
        // problem. Same pipeline renders levels too, so this should
        // address the level texture misalignment as well.
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

        // Same pipeline, just cull_mode=NONE - needed for colour-key
        // cutout models (the Music/SFX speakers, Multitap), which are
        // thin/open shapes where the "inside" surface (a back-facing
        // triangle from certain angles) legitimately needs to be visible
        // through a cutout gap. With normal backface culling, that
        // inside surface gets discarded entirely, making the cutout
        // incorrectly show background instead of material (Edward,
        // 2026: "we need transparent textures to be double sided so
        // they are not transparent from the back"). Every OTHER model
        // and level geometry is solid/opaque, where single-sided
        // culling is correct (confirmed empirically - see the cull_mode
        // comment above), so this is opt-in per model, not a global
        // setting.
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        doubleSidedPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

        // The half blend. Culled like the level's other faces: a two-sided
        // see-through face carries both windings (RenderMesh), and only the
        // one facing the camera survives, so it is blended once.
        {
            SDL_GPUColorTargetDescription blendDesc = colorDesc;
            blendDesc.blend_state.enable_blend = true;
            blendDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
            blendDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
            blendDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            blendDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            blendDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blendDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            SDL_GPUGraphicsPipelineCreateInfo blendInfo = pipelineInfo;
            blendInfo.target_info.color_target_descriptions = &blendDesc;
            blendInfo.depth_stencil_state.enable_depth_write = false;
            blendInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
            translucentPipeline = SDL_CreateGPUGraphicsPipeline(device, &blendInfo);
            if (!translucentPipeline)
            {
                // Not fatal: the see-through faces are then drawn as cut-outs.
                SDL_Log("ModelRenderer::Initialize: the see-through pipeline failed: %s", SDL_GetError());
            }
        }

        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);

        if (!pipeline || !doubleSidedPipeline)
        {
            SDL_Log("ModelRenderer::Initialize: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
            SDL_DestroyGPUDevice(device);
            device = nullptr;
            return false;
        }

        // LINEAR (bilinear) filtering - Edward, 2026: a direct
        // side-by-side comparison against a real screenshot of the
        // original game showed smooth, non-blocky texture surfaces, not
        // the blocky pixelation NEAREST filtering was assumed to
        // authentically reproduce here. That assumption was wrong - this
        // game's renderer (PS1 hardware supports optional bilinear
        // texture filtering per-game, and evidently this one used it)
        // smooths texture sampling at the source, which is the actual
        // fix for the "dithering pattern" complaint: PS1 textures often
        // use ordered dithering to fake more colours than the palette
        // has, and that pattern is meant to blend into a gradient via
        // filtering, not render as hard flat blocks. An earlier attempt
        // at this (rendering small, then post-process blurring the
        // whole final image via SDL_SCALEMODE_LINEAR at the 2D
        // composite stage) was the wrong layer to fix it at - it
        // blurred geometry edges/silhouettes that didn't need it
        // without actually addressing the texture sampling itself
        // (Edward: "that just made it blurry, it didn't actually change
        // the rendered result").
        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        // LINEAR min/mag for actual bilinear texel smoothing (see the
        // comment on why NEAREST was wrong for this game), but NEAREST
        // mipmap mode - textures here only ever have a single mip level
        // (num_levels=1, see UploadMeshWithTexture), so LINEAR mipmap
        // mode was mismatched against that and produced a genuine
        // artifact: Edward, 2026 found a regular, ~4px-period
        // checkerboard pattern in the rendered CRT screen, in a region
        // independently confirmed (by directly inspecting the source
        // texture data) to be a single uniform palette index with zero
        // internal variation. That's a sampler/mip-level mismatch
        // artifact, not real dithering - NEAREST mipmap mode is correct
        // here since there's only one level to sample from anyway.
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerLinear = SDL_CreateGPUSampler(device, &samplerInfo);

        // The Original counterpart: point sampling, so a magnified texel is a
        // hard square rather than a gradient. Everything else about the
        // sampler is identical, so the two differ ONLY in texel filtering -
        // which keeps the comparison honest when judging which one matches a
        // real screenshot.
        //
        // Note the reasoning above for LINEAR still stands on its own terms;
        // this does not overturn it. It makes the choice the Graphics >
        // Quality option's job instead of a hardcoded decision, which is what
        // that option was always supposed to control (its enum documented
        // "Original" as the authentic-hardware path but nothing read it).
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerNearest = SDL_CreateGPUSampler(device, &samplerInfo);

        // Default to Original (blocky). RenderSettings::Get() defaults the
        // same way, so an absent config key and an explicit "Original" agree.
        sampler = samplerNearest;

        return samplerLinear != nullptr && samplerNearest != nullptr;
    }

    void ModelRenderer::UnloadLevelModels()
    {
        if (!device) { return; }
        for (auto it = loadedModels.begin(); it != loadedModels.end();)
        {
            if (it->first.catalog != ModelCatalog::LevelDoor && it->first.catalog != ModelCatalog::LevelLift
                && it->first.catalog != ModelCatalog::LevelObject) { ++it; continue; }
            if (it->second.vertexBuffer) { SDL_ReleaseGPUBuffer(device, it->second.vertexBuffer); }
            if (it->second.indexBuffer) { SDL_ReleaseGPUBuffer(device, it->second.indexBuffer); }
            if (it->second.texture) { SDL_ReleaseGPUTexture(device, it->second.texture); }
            it = loadedModels.erase(it);
        }
    }

    void ModelRenderer::UnloadLevels()
    {
        if (!device) { return; }
        UnloadLevelModels();

        for (auto& [index, level] : loadedLevels)
        {
            // -GAP buffers first. Their `texture` is BORROWED from
            // level.groups and must not be released here, or the level's own
            // page texture would be freed twice.
            for (auto& group : level.gapGroups)
            {
                if (group.vertexBuffer) { SDL_ReleaseGPUBuffer(device, group.vertexBuffer); }
                if (group.indexBuffer) { SDL_ReleaseGPUBuffer(device, group.indexBuffer); }
                group.texture = nullptr;
            }

            for (auto* groups : { &level.groups, &level.cutoutGroups, &level.translucentGroups })
            {
                for (auto& group : *groups)
                {
                    if (group.vertexBuffer) { SDL_ReleaseGPUBuffer(device, group.vertexBuffer); }
                    if (group.indexBuffer) { SDL_ReleaseGPUBuffer(device, group.indexBuffer); }
                    if (group.texture) { SDL_ReleaseGPUTexture(device, group.texture); }
                    ReleaseCulling(group);
                }
            }
        }
        loadedLevels.clear();
    }

    void ModelRenderer::Shutdown()
    {
        if (!device) { return; }

        for (auto& [index, model] : loadedModels)
        {
            if (model.vertexBuffer) { SDL_ReleaseGPUBuffer(device, model.vertexBuffer); }
            if (model.indexBuffer) { SDL_ReleaseGPUBuffer(device, model.indexBuffer); }
            if (model.texture) { SDL_ReleaseGPUTexture(device, model.texture); }
        }
        loadedModels.clear();

        // Same idea as loadedModels above - without this, a cached
        // LoadedLevel's GPU handles go dangling the moment the device is
        // destroyed below, but LoadLevel's "already loaded" cache check
        // would still report success on a later re-entry, handing back
        // garbage buffers. Same class of bug as the "Options no longer
        // displays models" issue this Shutdown/Initialize idempotency
        // fix addresses, just for levels instead.
        UnloadLevels();

        if (colorTarget) { SDL_ReleaseGPUTexture(device, colorTarget); colorTarget = nullptr; }
        if (depthTarget) { SDL_ReleaseGPUTexture(device, depthTarget); depthTarget = nullptr; }
        if (downloadBuffer) { SDL_ReleaseGPUTransferBuffer(device, downloadBuffer); downloadBuffer = nullptr; }
        if (vertexUploadBuffer) { SDL_ReleaseGPUTransferBuffer(device, vertexUploadBuffer); vertexUploadBuffer = nullptr; vertexUploadSize = 0; }
        if (spriteTransfer) { SDL_ReleaseGPUTransferBuffer(device, spriteTransfer); spriteTransfer = nullptr; spriteTransferSize = 0; }
        if (cullUpload) { SDL_ReleaseGPUTransferBuffer(device, cullUpload); cullUpload = nullptr; cullUploadSize = 0; }
        // The sprite buffers and sheets belong to this device too; a later
        // Initialize must not find them and draw with the old handles.
        if (spriteVertexBuffer) { SDL_ReleaseGPUBuffer(device, spriteVertexBuffer); spriteVertexBuffer = nullptr; }
        if (spriteIndexBuffer) { SDL_ReleaseGPUBuffer(device, spriteIndexBuffer); spriteIndexBuffer = nullptr; }
        spriteBufferCapacity = 0;
        for (auto& [key, sheet] : spriteSheets)
        {
            if (sheet.texture) { SDL_ReleaseGPUTexture(device, sheet.texture); }
        }
        spriteSheets.clear();
        if (samplerLinear) { SDL_ReleaseGPUSampler(device, samplerLinear); samplerLinear = nullptr; }
        if (samplerNearest) { SDL_ReleaseGPUSampler(device, samplerNearest); samplerNearest = nullptr; }
        sampler = nullptr;
        if (pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, pipeline); pipeline = nullptr; }
        if (doubleSidedPipeline) { SDL_ReleaseGPUGraphicsPipeline(device, doubleSidedPipeline); doubleSidedPipeline = nullptr; }
        if (translucentPipeline) { SDL_ReleaseGPUGraphicsPipeline(device, translucentPipeline); translucentPipeline = nullptr; }

        SDL_DestroyGPUDevice(device);
        device = nullptr;
    }

    namespace
    {
        std::vector<ModelRenderer::ModelFace> ExtractFaces(const RenderMesh& mesh, const ALTEngine::Formats::ModelMesh& source)
        {
            std::vector<ModelRenderer::ModelFace> faces;
            for (const auto& e : ALTEngine::Formats::ExtractMeshFaces(mesh, source))
            {
                ModelRenderer::ModelFace f{};
                f.cx = e.cx; f.cy = e.cy; f.cz = e.cz;
                f.halfWidth = e.halfWidth; f.halfHeight = e.halfHeight;
                f.u0 = e.u0; f.v0 = e.v0; f.u1 = e.u1; f.v1 = e.v1;
                for (int c = 0; c < 4; ++c)
                {
                    f.px[c] = e.px[c]; f.py[c] = e.py[c]; f.pz[c] = e.pz[c]; f.pu[c] = e.pu[c]; f.pv[c] = e.pv[c];
                }
                faces.push_back(f);
            }
            return faces;
        }
    }

    bool ModelRenderer::LoadModel(ModelCacheKey cacheKey, int meshNumber,
                                   const std::filesystem::path& objBndPath, const std::filesystem::path& gfxBndPath,
                                   std::optional<std::array<uint8_t, 3>> transparentRgb,
                                   float baseRotationRadians)
    {
        if (!device) { return false; }
        if (loadedModels.count(cacheKey)) { return true; }

        // A catalog is loaded one model at a time, all from the same two
        // files, so the last files read are kept: re-reading and re-parsing
        // the whole model file and its texture pages for each of its models
        // was most of the briefing screen's loading time.
        static std::filesystem::path cachedObjPath;
        static std::vector<ALTEngine::Formats::ModelMesh> cachedMeshes;
        static std::filesystem::path cachedGfxPath;
        static std::optional<std::array<uint8_t, 3>> cachedTransparent;
        static BndTextureSet cachedTextures;
        try
        {
            if (cachedMeshes.empty() || cachedObjPath != objBndPath)
            {
                cachedMeshes = ModelLoader::Load(objBndPath);
                cachedObjPath = objBndPath;
            }
            if (cachedTextures.textures.empty() || cachedGfxPath != gfxBndPath || cachedTransparent != transparentRgb)
            {
                cachedTextures = BndTextureLoader::Load(gfxBndPath, transparentRgb);
                cachedGfxPath = gfxBndPath;
                cachedTransparent = transparentRgb;
            }
        }
        catch (const std::exception& e)
        {
            cachedMeshes.clear();
            cachedTextures = BndTextureSet{};
            SDL_Log("ModelRenderer::LoadModel(%s): %s", DescribeCacheKey(cacheKey).c_str(), e.what());
            return false;
        }
        const std::vector<ALTEngine::Formats::ModelMesh>& meshes = cachedMeshes;
        const BndTextureSet& textureSet = cachedTextures;

        const auto* mesh = ModelLoader::FindByNumber(meshes, meshNumber);
        if (!mesh)
        {
            SDL_Log("ModelRenderer::LoadModel(%s): no section for mesh number %d in %s", DescribeCacheKey(cacheKey).c_str(), meshNumber, objBndPath.string().c_str());
            return false;
        }

        // Auto-detected texture scheme: exactly one BX section means
        // every model in this file shares that one texture+UV group
        // (confirmed for PICKGFX.BND, used by both PICKMOD and OBJ3D) -
        // otherwise each model uses the texture/BX section matching its
        // own number (confirmed for OPTGFX.BND, one per OPTOBJ model).
        //
        // uvRects is now a flat array spanning every BX section, each
        // entry stamped with its own `page` (Edward, 2026 - Ghidra
        // deep-dive; see RenderMesh.h/BxParser.h) - filtering by page
        // reconstructs "the rects belonging to one specific BX section",
        // same as the old per-section uvSections[i] did, but sourced
        // from one flat list instead of a vector-of-vectors. `page` is
        // parsed from the same "NN" suffix meshNumber/targetIndex
        // already represents, so a plain int comparison replaces the
        // old string comparison for this part.
        const ALTEngine::Formats::BndTexture* tex = nullptr;
        std::vector<ALTEngine::Formats::BxRectangle> filteredUvRects;

        if (textureSet.textures.size() == 1 && !textureSet.textures.empty())
        {
            tex = &textureSet.textures[0];
            filteredUvRects = textureSet.uvRects;
        }
        else
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%02d", meshNumber);
            std::string targetIndex(buf);
            for (size_t i = 0; i < textureSet.textures.size(); ++i)
            {
                if (textureSet.textures[i].index == targetIndex)
                {
                    tex = &textureSet.textures[i];
                    for (const auto& rect : textureSet.uvRects)
                    {
                        if (rect.page == meshNumber) { filteredUvRects.push_back(rect); }
                    }
                    break;
                }
            }
        }

        const std::vector<ALTEngine::Formats::BxRectangle>* uvRects = &filteredUvRects;
        if (!tex || filteredUvRects.empty())
        {
            SDL_Log("ModelRenderer::LoadModel(%s): no matching texture for mesh number %d in %s", DescribeCacheKey(cacheKey).c_str(), meshNumber, gfxBndPath.string().c_str());
            return false;
        }

        RenderMesh renderMesh = BuildRenderMesh(*mesh, *uvRects,
            cacheKey.catalog == ModelCatalog::Obj3d && ALTEngine::Formats::Obj3DIsFlatSeeThrough(meshNumber));
        if (renderMesh.vertices.empty() || renderMesh.indices.empty())
        {
            SDL_Log("ModelRenderer::LoadModel(%s): model has no geometry", DescribeCacheKey(cacheKey).c_str());
            return false;
        }

        // AABB -> bounding sphere, for auto-framing the camera without
        // needing to know the game's arbitrary coordinate scale ahead
        // of time.
        float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
        float minY = minX, maxY = maxX, minZ = minX, maxZ = maxX;
        for (const auto& v : renderMesh.vertices)
        {
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
        }
        float cx = (minX + maxX) / 2, cy = (minY + maxY) / 2, cz = (minZ + maxZ) / 2;
        float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
        float radius = std::sqrt(dx * dx + dy * dy + dz * dz) / 2.0f;
        if (radius < 1.0f) { radius = 1.0f; }

        LevelSubGroup uploaded = UploadMeshWithTexture(renderMesh, *tex);
        if (!uploaded.vertexBuffer || !uploaded.indexBuffer || !uploaded.texture)
        {
            SDL_Log("ModelRenderer::LoadModel(%s): upload failed", DescribeCacheKey(cacheKey).c_str());
            return false;
        }

        LoadedModel model;
        model.vertexBuffer = uploaded.vertexBuffer;
        model.indexBuffer = uploaded.indexBuffer;
        model.indexCount = uploaded.indexCount;
        model.translucentFirst = renderMesh.translucentStart < renderMesh.indices.size()
            ? static_cast<uint32_t>(renderMesh.translucentStart) : model.indexCount;
        if (model.translucentFirst < model.indexCount)
        {
            SDL_Log("ModelRenderer: %s has %u see-through triangles", DescribeCacheKey(cacheKey).c_str(),
                    (model.indexCount - model.translucentFirst) / 3);
        }
        model.texture = uploaded.texture;
        model.centerX = cx; model.centerY = cy; model.centerZ = cz;
        model.radius = radius;
        model.baseRotationRadians = baseRotationRadians;
        model.useDoubleSided = transparentRgb.has_value();

        // Keep the faces so the object can be torn into them when destroyed -
        // the same unit the original shatters (FUN_000464ec turns ONE face
        // into four shards).
        model.faces = ExtractFaces(renderMesh, *mesh);

        // And the model's own texture as a sprite sheet, so a shard can carry
        // the crate's artwork rather than a stand-in.
        if (!renderMesh.vertices.empty() && tex && !tex->rgba.empty())
        {
            UploadSpriteSheet(ModelSheetKey(cacheKey), tex->rgba, tex->width, tex->height);
        }

        loadedModels[cacheKey] = model;

        return true;
    }

    bool ModelRenderer::RegisterMesh(ModelCacheKey cacheKey,
                                     const ALTEngine::Formats::RenderMesh& renderMesh,
                                     const ALTEngine::Formats::BndTexture& texture)
    {
        if (!Initialize()) { return false; }
        if (renderMesh.vertices.empty() || renderMesh.indices.empty()) { return false; }
        if (loadedModels.find(cacheKey) != loadedModels.end()) { return true; }

        LevelSubGroup group = UploadMeshWithTexture(renderMesh, texture);
        if (!group.vertexBuffer || group.indexCount == 0) { return false; }

        LoadedModel model;
        model.vertexBuffer = group.vertexBuffer;
        model.indexBuffer = group.indexBuffer;
        model.indexCount = group.indexCount;
        model.translucentFirst = renderMesh.translucentStart < renderMesh.indices.size()
            ? static_cast<uint32_t>(renderMesh.translucentStart) : group.indexCount;
        model.texture = group.texture;
        loadedModels[cacheKey] = model;
        return true;
    }

    void ModelRenderer::PreloadBatch(const std::vector<PreloadRequest>& requests)
    {
        if (!device) { return; }

        // Phase 1: CPU-side work per model (parse, build render mesh,
        // compute AABB) - identical to LoadModel's own logic, just
        // collected into a list instead of uploaded immediately, so the
        // GPU work below can be batched into one shared transfer buffer/
        // command buffer/submit instead of one full cycle per model.
        struct PendingUpload
        {
            ModelCacheKey cacheKey;
            RenderMesh renderMesh;
            std::vector<uint8_t> textureRgba;
            int textureWidth = 0, textureHeight = 0;
            float centerX = 0, centerY = 0, centerZ = 0, radius = 1.0f;
            float baseRotationRadians = 0.0f;
            bool useDoubleSided = false;
            size_t vbSize = 0, ibSize = 0, texSize = 0;
        };

        std::vector<PendingUpload> pending;
        pending.reserve(requests.size());

        for (const auto& req : requests)
        {
            if (loadedModels.count(req.cacheKey)) { continue; }

            std::vector<ALTEngine::Formats::ModelMesh> meshes;
            BndTextureSet textureSet;
            try
            {
                meshes = ModelLoader::Load(req.objBndPath);
                textureSet = BndTextureLoader::Load(req.gfxBndPath, req.transparentRgb);
            }
            catch (const std::exception& e)
            {
                SDL_Log("ModelRenderer::PreloadBatch(%s): %s", DescribeCacheKey(req.cacheKey).c_str(), e.what());
                continue;
            }

            const auto* mesh = ModelLoader::FindByNumber(meshes, req.meshNumber);
            if (!mesh) { continue; }

            const ALTEngine::Formats::BndTexture* tex = nullptr;
            std::vector<ALTEngine::Formats::BxRectangle> filteredUvRects;

            if (textureSet.textures.size() == 1 && !textureSet.textures.empty())
            {
                tex = &textureSet.textures[0];
                filteredUvRects = textureSet.uvRects;
            }
            else
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%02d", req.meshNumber);
                std::string targetIndex(buf);
                for (size_t i = 0; i < textureSet.textures.size(); ++i)
                {
                    if (textureSet.textures[i].index == targetIndex)
                    {
                        tex = &textureSet.textures[i];
                        for (const auto& rect : textureSet.uvRects)
                        {
                            if (rect.page == req.meshNumber) { filteredUvRects.push_back(rect); }
                        }
                        break;
                    }
                }
            }

            const std::vector<ALTEngine::Formats::BxRectangle>* uvRects = &filteredUvRects;
            if (!tex || filteredUvRects.empty()) { continue; }

            RenderMesh renderMesh = BuildRenderMesh(*mesh, *uvRects,
                req.cacheKey.catalog == ModelCatalog::Obj3d && ALTEngine::Formats::Obj3DIsFlatSeeThrough(req.meshNumber));
            if (renderMesh.vertices.empty() || renderMesh.indices.empty()) { continue; }

            float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
            float minY = minX, maxY = maxX, minZ = minX, maxZ = maxX;
            for (const auto& v : renderMesh.vertices)
            {
                minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
                minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
                minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
            }
            float cx = (minX + maxX) / 2, cy = (minY + maxY) / 2, cz = (minZ + maxZ) / 2;
            float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
            float radius = std::sqrt(dx * dx + dy * dy + dz * dz) / 2.0f;
            if (radius < 1.0f) { radius = 1.0f; }

            PendingUpload up;
            up.cacheKey = req.cacheKey;
            up.textureRgba = tex->rgba; // copied, not referenced - textureSet goes out of scope this iteration
            up.textureWidth = tex->width;
            up.textureHeight = tex->height;
            up.centerX = cx; up.centerY = cy; up.centerZ = cz; up.radius = radius;
            up.baseRotationRadians = req.baseRotationRadians;
            up.useDoubleSided = req.transparentRgb.has_value();
            up.vbSize = renderMesh.vertices.size() * sizeof(ALTEngine::Formats::RenderVertex);
            up.ibSize = renderMesh.indices.size() * sizeof(uint32_t);
            up.texSize = static_cast<size_t>(up.textureWidth) * up.textureHeight * 4;
            up.renderMesh = std::move(renderMesh);
            pending.push_back(std::move(up));
        }

        if (pending.empty()) { return; }

        // Phase 2: create GPU buffer/texture objects up front - still
        // one CreateGPUBuffer/CreateGPUTexture call per model (each
        // needs its own unique handle, these can't be shared), but no
        // per-model transfer buffer or command buffer anymore.
        struct CreatedResources { SDL_GPUBuffer* vb = nullptr; SDL_GPUBuffer* ib = nullptr; SDL_GPUTexture* tex = nullptr; };
        std::vector<CreatedResources> created(pending.size());
        std::vector<size_t> offsets(pending.size(), 0);
        size_t totalTransferSize = 0;

        for (size_t i = 0; i < pending.size(); i++)
        {
            auto& up = pending[i];
            SDL_GPUBufferCreateInfo vbInfo{ SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(up.vbSize), 0 };
            SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(device, &vbInfo);
            SDL_GPUBufferCreateInfo ibInfo{ SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(up.ibSize), 0 };
            SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(device, &ibInfo);

            SDL_GPUTextureCreateInfo texInfo{};
            texInfo.type = SDL_GPU_TEXTURETYPE_2D;
            texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            texInfo.width = static_cast<Uint32>(up.textureWidth);
            texInfo.height = static_cast<Uint32>(up.textureHeight);
            texInfo.layer_count_or_depth = 1;
            texInfo.num_levels = 1;
            texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);

            if (!vertexBuffer || !indexBuffer || !texture)
            {
                SDL_Log("ModelRenderer::PreloadBatch(%s): buffer/texture creation failed: %s", DescribeCacheKey(up.cacheKey).c_str(), SDL_GetError());
                if (vertexBuffer) { SDL_ReleaseGPUBuffer(device, vertexBuffer); }
                if (indexBuffer) { SDL_ReleaseGPUBuffer(device, indexBuffer); }
                if (texture) { SDL_ReleaseGPUTexture(device, texture); }
                continue;
            }

            created[i] = { vertexBuffer, indexBuffer, texture };
            offsets[i] = totalTransferSize;
            totalTransferSize += up.vbSize + up.ibSize + up.texSize;
        }

        if (totalTransferSize == 0) { return; }

        // Phase 3: one shared transfer buffer for everything, mapped
        // once - each model's data copied to its own offset within it.
        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(totalTransferSize);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (!transfer)
        {
            SDL_Log("ModelRenderer::PreloadBatch: shared transfer buffer creation failed: %s", SDL_GetError());
            for (auto& c : created)
            {
                if (c.vb) { SDL_ReleaseGPUBuffer(device, c.vb); }
                if (c.ib) { SDL_ReleaseGPUBuffer(device, c.ib); }
                if (c.tex) { SDL_ReleaseGPUTexture(device, c.tex); }
            }
            return;
        }

        uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false));
        for (size_t i = 0; i < pending.size(); i++)
        {
            if (!created[i].vb) { continue; }
            auto& up = pending[i];
            uint8_t* dst = mapped + offsets[i];
            std::memcpy(dst, up.renderMesh.vertices.data(), up.vbSize);
            std::memcpy(dst + up.vbSize, up.renderMesh.indices.data(), up.ibSize);
            std::memcpy(dst + up.vbSize + up.ibSize, up.textureRgba.data(), up.texSize);
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);

        // Phase 4: one command buffer, one copy pass containing every
        // model's upload calls, one submit - this is the actual fix.
        // Previously each model paid for its own AcquireGPUCommandBuffer
        // + SubmitGPUCommandBuffer + transfer buffer create/destroy
        // cycle (~13 separate GPU API calls per model, ~520 total across
        // 40 models) - real GPU drivers have meaningful fixed overhead
        // per submission/resource creation regardless of payload size,
        // which is why ~960KB of combined data was taking 2-3 seconds:
        // the call count was the cost, not the bytes.
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

        for (size_t i = 0; i < pending.size(); i++)
        {
            if (!created[i].vb) { continue; }
            auto& up = pending[i];
            size_t off = offsets[i];

            SDL_GPUTransferBufferLocation vbSrc{ transfer, static_cast<Uint32>(off) };
            SDL_GPUBufferRegion vbDst{ created[i].vb, 0, static_cast<Uint32>(up.vbSize) };
            SDL_UploadToGPUBuffer(copyPass, &vbSrc, &vbDst, false);

            SDL_GPUTransferBufferLocation ibSrc{ transfer, static_cast<Uint32>(off + up.vbSize) };
            SDL_GPUBufferRegion ibDst{ created[i].ib, 0, static_cast<Uint32>(up.ibSize) };
            SDL_UploadToGPUBuffer(copyPass, &ibSrc, &ibDst, false);

            SDL_GPUTextureTransferInfo texSrc{};
            texSrc.transfer_buffer = transfer;
            texSrc.offset = static_cast<Uint32>(off + up.vbSize + up.ibSize);
            SDL_GPUTextureRegion texDst{};
            texDst.texture = created[i].tex;
            texDst.w = static_cast<Uint32>(up.textureWidth);
            texDst.h = static_cast<Uint32>(up.textureHeight);
            texDst.d = 1;
            SDL_UploadToGPUTexture(copyPass, &texSrc, &texDst, false);
        }

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);

        // Phase 5: register every successfully-uploaded model in the
        // same cache LoadModel uses - identical downstream behaviour,
        // RenderToRgba/DrawModelPreview callers can't tell the
        // difference between a model preloaded this way vs individually.
        for (size_t i = 0; i < pending.size(); i++)
        {
            if (!created[i].vb) { continue; }
            auto& up = pending[i];
            LoadedModel model;
            model.vertexBuffer = created[i].vb;
            model.indexBuffer = created[i].ib;
            model.indexCount = static_cast<uint32_t>(up.renderMesh.indices.size());
            model.translucentFirst = up.renderMesh.translucentStart < up.renderMesh.indices.size()
                ? static_cast<uint32_t>(up.renderMesh.translucentStart) : model.indexCount;
            if (model.translucentFirst < model.indexCount)
            {
                SDL_Log("ModelRenderer: %s has %u see-through triangles", DescribeCacheKey(up.cacheKey).c_str(),
                        (model.indexCount - model.translucentFirst) / 3);
            }
            model.texture = created[i].tex;
            model.centerX = up.centerX; model.centerY = up.centerY; model.centerZ = up.centerZ;
            model.radius = up.radius;
            model.baseRotationRadians = up.baseRotationRadians;
            model.useDoubleSided = up.useDoubleSided;
            loadedModels[up.cacheKey] = model;
        }
    }

    std::vector<uint8_t> ModelRenderer::RenderToRgba(ModelCacheKey cacheKey, float angleRadians, int width, int height)
    {
        if (!device || !pipeline) { return {}; }
        auto it = loadedModels.find(cacheKey);
        if (it == loadedModels.end()) { return {}; }
        const LoadedModel& model = it->second;

        if (!EnsureRenderTarget(width, height)) { return {}; }

        // Auto-framed camera: distance chosen so the model's bounding
        // sphere comfortably fits the vertical FOV, looking at its
        // center, model itself spun by angleRadians around Y.
        float fovY = 45.0f * (PI / 180.0f);
        float distance = model.radius / std::sin(fovY / 2.0f) * 1.3f; // 1.3x = a little breathing room
        Mat4 proj = Mat4::Perspective(fovY, static_cast<float>(width) / static_cast<float>(height), 1.0f, distance * 4.0f + model.radius);
        Mat4 view = Mat4::LookAt(model.centerX, model.centerY, model.centerZ + distance, model.centerX, model.centerY, model.centerZ, 0, 1, 0);
        Mat4 rotation = Mat4::RotationY(model.baseRotationRadians + angleRadians);
        Mat4 mvp = Mat4::Multiply(proj, Mat4::Multiply(view, rotation));

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

        SDL_GPUColorTargetInfo colorInfo{};
        colorInfo.texture = colorTarget;
        colorInfo.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f }; // transparent - composited over the menu's own background
        colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthInfo{};
        depthInfo.texture = depthTarget;
        depthInfo.clear_depth = 1.0f;
        depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmd, &colorInfo, 1, &depthInfo);

        SDL_BindGPUGraphicsPipeline(renderPass, model.useDoubleSided ? doubleSidedPipeline : pipeline);
        SDL_GPUBufferBinding vbBinding{ model.vertexBuffer, 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
        SDL_GPUBufferBinding ibBinding{ model.indexBuffer, 0 };
        SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_GPUTextureSamplerBinding texBinding{ model.texture, sampler };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
        SDL_PushGPUVertexUniformData(cmd, 0, mvp.m.data(), sizeof(float) * 16);
            {
                float fog[8] = { fogEnabled, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
            }
        SDL_DrawGPUIndexedPrimitives(renderPass, model.indexCount, 1, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);

        return DownloadColorTarget(cmd, width, height);
    }

    bool ModelRenderer::LoadLevel(const std::string& cacheKey, const std::filesystem::path& mapPath, const std::filesystem::path& gfxPath)
    {
        if (!device) { return false; }
        if (loadedLevels.count(cacheKey)) { return true; }

        // Transparency is the face's, not the page's: the draw routine decides
        // (Formats::ClassifyDrawRoutine), and the cut-out and see-through fills
        // skip texels whose raw palette word is 0. So the pages load keyed on
        // that, and the opaque faces get a copy with the alpha ignored. This
        // replaced an old per-level index table (since removed), which made
        // every face on a page a cut-out.

        ALTEngine::Formats::LevelGeometry level;
        BndTextureSet textureSet;
        try
        {
            level = ALTEngine::Formats::LevelLoader::Load(mapPath);
            textureSet = BndTextureLoader::LoadLevelPages(gfxPath);
        }
        catch (const std::exception& e)
        {
            SDL_Log("ModelRenderer::LoadLevel(%s): %s", cacheKey.c_str(), e.what());
            return false;
        }

        if (textureSet.textures.size() != 5)
        {
            SDL_Log("ModelRenderer::LoadLevel(%s): expected 5 texture groups in %s, got %zu",
                    cacheKey.c_str(), gfxPath.string().c_str(), textureSet.textures.size());
            return false;
        }

        // Per-face UV overrides for mis-authored faces, matched on vertex
        // indices. Optional - a missing manifest just yields none.
        auto allRotations = ALTEngine::Formats::FaceUvPatchLoader::Load("data/Patches.json");
        auto rotations = ALTEngine::Formats::FaceUvPatchLoader::ForLevelFile(
            allRotations, mapPath.filename().string());
        if (!rotations.empty())
        {
            SDL_Log("ModelRenderer::LoadLevel(%s): applying %zu face UV override(s)", cacheKey.c_str(), rotations.size());
        }

        LoadedLevel loadedLevel;

        // Lights must be resolved BEFORE the mesh is built - the vertex
        // colours are baked into the vertex buffer.
        loadedLevel.geometry = level;
        loadedLevel.uvRects = textureSet.uvRects;
        loadedLevel.rotations = rotations;
        loadedLevel.lights.Reset(level.lights);
        loadedLevel.lastEntries = loadedLevel.lights.Entries();
        loadedLevel.animator.Reset(level);
        loadedLevel.lastAnimatorOutputs = loadedLevel.animator.OutputSnapshot();

        SDL_Log("ModelRenderer::LoadLevel(%s): %zu texture animators, %zu light records",
                cacheKey.c_str(), loadedLevel.animator.Count(), level.lights.size());

        namespace FM = ALTEngine::Formats;
        auto allFills = FM::BuildLevelRenderMeshAllFills(
            level, textureSet.uvRects, rotations, &loadedLevel.lights, &loadedLevel.animator);
        auto& perGroupMeshes = allFills[0];
        auto& cutoutMeshes = allFills[1];
        auto& translucentMeshes = allFills[2];

        for (size_t i = 0; i < 5; ++i)
        {
            loadedLevel.groupVertexCounts[i] = perGroupMeshes[i].vertices.size();
            loadedLevel.cutoutVertexCounts[i] = cutoutMeshes[i].vertices.size();
            loadedLevel.translucentVertexCounts[i] = translucentMeshes[i].vertices.size();
        }

        // Uploaded before the -GAP and override geometry below, which borrow
        // these page textures (they used to look for them before they were
        // made, and so never loaded).
        bool anyGroupUsed = false;
        for (size_t i = 0; i < 5; ++i)
        {
            if (!cutoutMeshes[i].vertices.empty())
            {
                loadedLevel.cutoutGroups[i] = UploadMeshWithTexture(cutoutMeshes[i], textureSet.textures[i]);
                if (loadedLevel.cutoutGroups[i].vertexBuffer) { anyGroupUsed = true; AttachCulling(loadedLevel.cutoutGroups[i], cutoutMeshes[i]); }
            }
            if (!translucentMeshes[i].vertices.empty())
            {
                loadedLevel.translucentGroups[i] = UploadMeshWithTexture(translucentMeshes[i], textureSet.textures[i]);
                if (loadedLevel.translucentGroups[i].vertexBuffer) { anyGroupUsed = true; AttachCulling(loadedLevel.translucentGroups[i], translucentMeshes[i]); }
            }
            // The opaque faces' copy: every texel drawn, palette word 0 as the
            // black it is. Uploaded even if no opaque face uses the page, since
            // override geometry may.
            ALTEngine::Formats::BndTexture opaque = textureSet.textures[i];
            for (size_t p = 3; p < opaque.rgba.size(); p += 4) { opaque.rgba[p] = 255; }
            if (!perGroupMeshes[i].vertices.empty())
            {
                loadedLevel.groups[i] = UploadMeshWithTexture(perGroupMeshes[i], opaque);
                if (loadedLevel.groups[i].vertexBuffer) { anyGroupUsed = true; AttachCulling(loadedLevel.groups[i], perGroupMeshes[i]); }
            }
        }

        // -GAP override, if the user has authored one. Sits beside the level
        // .MAP under CD/OVERRIDE, e.g.
        //   CD/OVERRIDE/SECT11/L111LEV-GAP.obj
        // Absent is the normal case and is not an error.
        //
        // Material names select which of the level's own texture pages a face
        // draws with: "page0" .. "page4". That keeps the override on the
        // textures already uploaded for the level rather than needing its own,
        // and means a gap filler can be authored purely in Blender with five
        // materials named after the pages.
        //
        // NOTE FOR LATER: this is also where a higher-resolution replacement
        // level model would arrive, and it is worth being explicit that the
        // faceUvRotations patches in Patches.json do NOT reach it. Those
        // patches are keyed on .MAP vertex indices and applied while building
        // the .MAP's own mesh; an OBJ that replaces that geometry carries its
        // own UVs and simply bypasses them. Any fix baked into a replacement
        // model has to be baked into the model.
        {
            std::filesystem::path chosen = FindOverrideObj(mapPath, "-GAP");

            if (!chosen.empty())
            {
                ALTEngine::Formats::ObjModel gap = ALTEngine::Formats::ObjLoader::Load(chosen);
                for (const std::string& w : gap.warnings)
                {
                    SDL_Log("ModelRenderer::LoadLevel(%s): -GAP %s: %s",
                            cacheKey.c_str(), chosen.filename().string().c_str(), w.c_str());
                }

                int uploaded = 0;
                for (const ALTEngine::Formats::ObjFaceGroup& group : gap.groups)
                {
                    int page = -1;
                    if (group.materialName.rfind("page", 0) == 0)
                    {
                        page = std::atoi(group.materialName.c_str() + 4);
                    }
                    if (page < 0 || page >= 5)
                    {
                        SDL_Log("ModelRenderer::LoadLevel(%s): -GAP material '%s' does not name a "
                                "texture page (expected page0..page4) - %zu triangles skipped",
                                cacheKey.c_str(), group.materialName.c_str(), group.indices.size() / 3);
                        continue;
                    }
                    if (!loadedLevel.groups[static_cast<size_t>(page)].texture)
                    {
                        SDL_Log("ModelRenderer::LoadLevel(%s): -GAP wants page %d but the level does "
                                "not use it - %zu triangles skipped",
                                cacheKey.c_str(), page, group.indices.size() / 3);
                        continue;
                    }

                    ALTEngine::Formats::RenderMesh mesh;
                    mesh.vertices = group.vertices;
                    mesh.indices = group.indices;

                    // alt_light binds this face to one of the level's own light
                    // records, so override geometry lights the same way the
                    // .MAP's does. The loader leaves the vertices white; this
                    // stamps the resolved colour over them.
                    ApplyLightToMesh(mesh, group.lightId, loadedLevel.lights);

                    LevelSubGroup sub = UploadMeshBuffersOnly(mesh);
                    if (!sub.vertexBuffer) { continue; }
                    // Borrows the level's page texture; not owned, so it must
                    // NOT be released with this sub-group.
                    sub.texture = loadedLevel.groups[static_cast<size_t>(page)].texture;
                    loadedLevel.gapGroups[static_cast<size_t>(page)] = sub;
                    uploaded += static_cast<int>(group.indices.size() / 3);
                    if (group.animatorOrdinal >= 0)
                    {
                        SDL_Log("ModelRenderer::LoadLevel(%s): -GAP face group requests animator %d - "
                                "animation of override geometry is not applied yet, it will draw its "
                                "authored texture",
                                cacheKey.c_str(), group.animatorOrdinal);
                    }
                }
                SDL_Log("ModelRenderer::LoadLevel(%s): -GAP loaded from %s, %d triangle(s)",
                        cacheKey.c_str(), chosen.string().c_str(), uploaded);
            }
        }

        // Full level geometry override. Same convention as -GAP but without
        // the suffix, e.g. CD/OVERRIDE/SECT11/L111LEV.obj, and it REPLACES the
        // .MAP's render geometry rather than adding to it.
        //
        // Replaces rendering only. Collision, floor heights, triggers, doors,
        // object placement and lighting all still come from the .MAP, because
        // they are gameplay and an art replacement must not change them. That
        // also means a replacement model has to keep the same shape as the
        // level it replaces or the player will walk through walls that look
        // solid.
        //
        // IMPORTANT: the faceUvRotations entries in Patches.json do NOT reach
        // this geometry. They are keyed on .MAP vertex indices and applied
        // while building the .MAP's mesh; an OBJ carries its own UVs and
        // bypasses them entirely. Any such fix has to be baked into the
        // replacement model.
        {
            std::filesystem::path levelObj = FindOverrideObj(mapPath, "");
            if (!levelObj.empty())
            {
                ALTEngine::Formats::ObjModel model = ALTEngine::Formats::ObjLoader::Load(levelObj);
                for (const std::string& w : model.warnings)
                {
                    SDL_Log("ModelRenderer::LoadLevel(%s): level override %s: %s",
                            cacheKey.c_str(), levelObj.filename().string().c_str(), w.c_str());
                }

                if (model.Empty())
                {
                    SDL_Log("ModelRenderer::LoadLevel(%s): level override %s has no usable faces - "
                            "keeping the original geometry",
                            cacheKey.c_str(), levelObj.string().c_str());
                }
                else
                {
                    // Build the replacement first, and only swap once every
                    // group has uploaded. A half-applied override would leave
                    // the level partly invisible with no way back.
                    std::array<LevelSubGroup, 5> replacement{};
                    bool ok = true;
                    int uploaded = 0;

                    for (const ALTEngine::Formats::ObjFaceGroup& group : model.groups)
                    {
                        int page = -1;
                        if (group.materialName.rfind("page", 0) == 0)
                        {
                            page = std::atoi(group.materialName.c_str() + 4);
                        }
                        if (page < 0 || page >= 5 || !loadedLevel.groups[static_cast<size_t>(page)].texture)
                        {
                            SDL_Log("ModelRenderer::LoadLevel(%s): level override material '%s' does not "
                                    "name a usable texture page (expected page0..page4 that the level "
                                    "uses) - %zu triangles skipped",
                                    cacheKey.c_str(), group.materialName.c_str(), group.indices.size() / 3);
                            continue;
                        }

                        ALTEngine::Formats::RenderMesh mesh;
                        mesh.vertices = group.vertices;
                        mesh.indices = group.indices;
                        ApplyLightToMesh(mesh, group.lightId, loadedLevel.lights);

                        LevelSubGroup sub = UploadMeshBuffersOnly(mesh);
                        if (!sub.vertexBuffer) { ok = false; break; }
                        sub.texture = loadedLevel.groups[static_cast<size_t>(page)].texture;
                        replacement[static_cast<size_t>(page)] = sub;
                        uploaded += static_cast<int>(group.indices.size() / 3);
                    }

                    if (!ok || uploaded == 0)
                    {
                        for (auto& sub : replacement)
                        {
                            if (sub.vertexBuffer) { SDL_ReleaseGPUBuffer(device, sub.vertexBuffer); }
                            if (sub.indexBuffer) { SDL_ReleaseGPUBuffer(device, sub.indexBuffer); }
                        }
                        SDL_Log("ModelRenderer::LoadLevel(%s): level override upload failed - keeping "
                                "the original geometry", cacheKey.c_str());
                    }
                    else
                    {
                        // Swap in. The page textures are shared, so release
                        // only the original buffers and carry the textures
                        // across onto the replacement's groups.
                        for (size_t i = 0; i < 5; ++i)
                        {
                            SDL_GPUTexture* pageTexture = loadedLevel.groups[i].texture;
                            if (loadedLevel.groups[i].vertexBuffer) { SDL_ReleaseGPUBuffer(device, loadedLevel.groups[i].vertexBuffer); }
                            if (loadedLevel.groups[i].indexBuffer) { SDL_ReleaseGPUBuffer(device, loadedLevel.groups[i].indexBuffer); }
                            ReleaseCulling(loadedLevel.groups[i]);

                            loadedLevel.groups[i].vertexBuffer = replacement[i].vertexBuffer;
                            loadedLevel.groups[i].indexBuffer = replacement[i].indexBuffer;
                            loadedLevel.groups[i].indexCount = replacement[i].indexCount;
                            loadedLevel.groups[i].texture = pageTexture;
                            loadedLevel.groupVertexCounts[i] = 0;

                            // The replacement is all of the level's geometry:
                            // the .MAP's cut-out and see-through faces go too.
                            for (auto* other : { &loadedLevel.cutoutGroups[i], &loadedLevel.translucentGroups[i] })
                            {
                                if (other->vertexBuffer) { SDL_ReleaseGPUBuffer(device, other->vertexBuffer); }
                                if (other->indexBuffer) { SDL_ReleaseGPUBuffer(device, other->indexBuffer); }
                                if (other->texture) { SDL_ReleaseGPUTexture(device, other->texture); }
                                ReleaseCulling(*other);
                                *other = LevelSubGroup{};
                            }
                        }
                        loadedLevel.geometryOverridden = true;
                        SDL_Log("ModelRenderer::LoadLevel(%s): level geometry replaced from %s, "
                                "%d triangle(s). Lighting animation is disabled for this level - "
                                "override geometry carries its own vertex colours.",
                                cacheKey.c_str(), levelObj.string().c_str(), uploaded);
                    }
                }
            }
        }

        if (!anyGroupUsed)
        {
            SDL_Log("ModelRenderer::LoadLevel(%s): level has no renderable geometry", cacheKey.c_str());
            return false;
        }

        loadedLevels[cacheKey] = loadedLevel;
        return true;
    }

    void ModelRenderer::SetDrawDistanceFade(bool enabled)
    {
        fogEnabled = enabled ? 1.0f : 0.0f;
    }

    bool ModelRenderer::DrawDistanceFade() { return fogEnabled > 0.5f; }

    void ModelRenderer::SetTextureSmoothing(bool smoothed)
    {
        if (!samplerLinear || !samplerNearest) { return; }
        sampler = smoothed ? samplerLinear : samplerNearest;
    }

    bool ModelRenderer::TextureSmoothing()
    {
        return sampler == samplerLinear;
    }

    bool ModelRenderer::TickLevelLights(const std::string& cacheKey, int randomBits)
    {
        if (!device) { return false; }
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return false; }
        LoadedLevel& level = it->second;

        level.lights.Tick(randomBits);
        level.animator.Tick(randomBits);

        // Nothing to do unless a resolved light colour or an animator's output
        // descriptor actually moved. That is the common case by a wide margin -
        // blink durations and frame holds are both tens of ticks - and skipping
        // it here is what keeps this cheap enough to call every tick.
        // An override .obj replaced the geometry, so there is nothing to
        // rebuild from - the light machines still tick (scripts may read
        // them) but no vertex data is touched.
        if (level.geometryOverridden) { return false; }

        bool lightsMoved = !(level.lights.Entries() == level.lastEntries);
        std::vector<uint16_t> outputs = level.animator.OutputSnapshot();
        bool animatorsMoved = (outputs != level.lastAnimatorOutputs);
        if (!lightsMoved && !animatorsMoved) { return false; }

        level.lastEntries = level.lights.Entries();
        level.lastAnimatorOutputs = outputs;

        // Only lights moved: nothing but the vertex colours changes, so the
        // kept vertices are recoloured from the table (as LevelQuadColours
        // does) and sent up, without building the level again.
        if (!animatorsMoved)
        {
            std::vector<VertexUpload> recoloured;
            bool complete = true;
            for (auto* groups : { &level.groups, &level.cutoutGroups })
            {
                for (LevelSubGroup& group : *groups)
                {
                    if (!group.texture || !group.vertexBuffer) { continue; }
                    if (group.cpuVertices.empty()
                        || group.vertexLight.size() != group.cpuVertices.size()
                        || group.vertexCorner.size() != group.cpuVertices.size())
                    {
                        complete = false;
                        continue;
                    }
                    for (size_t v = 0; v < group.cpuVertices.size(); ++v)
                    {
                        const uint8_t light = group.vertexLight[v];
                        if (light == ALTEngine::Formats::RenderMesh::NO_LIGHT) { continue; }
                        const auto& corner = level.lights.ColourFor(light).corner[group.vertexCorner[v] & 3];
                        auto& vertex = group.cpuVertices[v];
                        vertex.r = static_cast<float>(corner.r) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                        vertex.g = static_cast<float>(corner.g) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                        vertex.b = static_cast<float>(corner.b) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                    }
                    recoloured.emplace_back(&group, &group.cpuVertices);
                }
            }
            if (complete)
            {
                ReuploadVerticesBatch(recoloured);
                return true;
            }
        }

        namespace FM = ALTEngine::Formats;
        struct Pass
        {
            std::array<LevelSubGroup, 5>* groups;
            std::array<size_t, 5>* counts;
        };
        const Pass passes[] = {
            { &level.groups, &level.groupVertexCounts },
            { &level.cutoutGroups, &level.cutoutVertexCounts },
            { &level.translucentGroups, &level.translucentVertexCounts },
        };
        // One walk over the faces builds all three fills.
        auto allFills = FM::BuildLevelRenderMeshAllFills(
            level.geometry, level.uvRects, level.rotations, &level.lights, &level.animator);
        std::vector<VertexUpload> uploads;
        for (size_t p = 0; p < 3; ++p)
        {
            const Pass& pass = passes[p];
            auto& meshes = allFills[p];

            for (size_t i = 0; i < 5; ++i)
            {
                LevelSubGroup& group = (*pass.groups)[i];
                // A page this fill never had faces on has no texture to draw
                // with; anything animating onto it stays where it was.
                if (!group.texture) { continue; }

                // An animated face whose new frame sits on a different page
                // moves between the per-page buffers, so this page's face
                // count changes: build its buffers afresh (the texture stays).
                if (meshes[i].vertices.size() != (*pass.counts)[i])
                {
                    LevelSubGroup rebuilt = UploadMeshBuffersOnly(meshes[i]);
                    if (!meshes[i].vertices.empty() && !rebuilt.vertexBuffer) { continue; }
                    if (group.vertexBuffer) { SDL_ReleaseGPUBuffer(device, group.vertexBuffer); }
                    if (group.indexBuffer) { SDL_ReleaseGPUBuffer(device, group.indexBuffer); }
                    group.vertexBuffer = rebuilt.vertexBuffer;
                    group.indexBuffer = rebuilt.indexBuffer;
                    group.indexCount = rebuilt.indexCount;
                    (*pass.counts)[i] = meshes[i].vertices.size();
                    AttachCulling(group, meshes[i]);
                    continue;
                }
                if (!group.vertexBuffer || meshes[i].vertices.empty()) { continue; }
                // Kept for the next light-only change.
                group.cpuVertices = meshes[i].vertices;
                group.vertexLight = meshes[i].vertexLight;
                group.vertexCorner = meshes[i].vertexCorner;
                uploads.emplace_back(&group, &group.cpuVertices);
            }
        }
        ReuploadVerticesBatch(uploads);
        return true;
    }

    void ModelRenderer::ToggleLevelLight(const std::string& cacheKey, int lightIndex, int delta)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        if (lightIndex < 0) { return; }
        it->second.lights.ToggleLight(static_cast<size_t>(lightIndex), delta);
    }

    void ModelRenderer::FlashLevelLights(const std::string& cacheKey)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        it->second.lights.RequestFlash();
    }

    bool ModelRenderer::LevelLightColour(const std::string& cacheKey, int lightId, float rgb[3])
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end() || lightId < 0) { return false; }
        const auto& entry = it->second.lights.ColourFor(lightId);
        float sum[3] = { 0.0f, 0.0f, 0.0f };
        for (const auto& c : entry.corner) { sum[0] += c.r; sum[1] += c.g; sum[2] += c.b; }
        for (int k = 0; k < 3; ++k) { rgb[k] = sum[k] / (4.0f * ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL); }
        return true;
    }

    bool ModelRenderer::TakeLevelLightFull(const std::string& cacheKey)
    {
        auto it = loadedLevels.find(cacheKey);
        return it != loadedLevels.end() && it->second.lights.TakeEquipmentFull();
    }

    bool ModelRenderer::LevelLightCorner(const std::string& cacheKey, int lightId, uint8_t rgb[3])
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end() || lightId < 0) { return false; }
        const auto& corner = it->second.lights.ColourFor(lightId).corner[0];
        rgb[0] = corner.r; rgb[1] = corner.g; rgb[2] = corner.b;
        return true;
    }

    void ModelRenderer::SetLevelFaceCells(const std::string& cacheKey, std::vector<int32_t> faceCells)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        it->second.faceCell = std::move(faceCells);
        it->second.drawnSerial++;
    }

    void ModelRenderer::SetLevelDrawnCells(const std::string& cacheKey, const std::vector<uint8_t>& drawnCells)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        if (it->second.drawnCells == drawnCells) { return; }
        it->second.drawnCells = drawnCells;
        it->second.drawnSerial++;
    }

    void ModelRenderer::SetLevelLightEquipment(const std::string& cacheKey, int kind)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        using E = ALTEngine::Formats::LightTable::Equipment;
        if (kind == 1) { it->second.lights.StartEquipment(E::Lamp); }
        else if (kind == 2) { it->second.lights.StartEquipment(E::Goggles); }
        else { it->second.lights.StopEquipment(); }
    }

    void ModelRenderer::ChangeLevelTexture(const std::string& cacheKey, int animatorIndex, int delta)
    {
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return; }
        if (animatorIndex < 0) { return; }
        it->second.animator.ChangeTexture(static_cast<size_t>(animatorIndex), delta);
    }

    float ModelRenderer::ModelBaseRotation(const ModelCacheKey& cacheKey)
    {
        auto it = loadedModels.find(cacheKey);
        return it == loadedModels.end() ? 0.0f : it->second.baseRotationRadians;
    }

    const std::vector<ModelRenderer::ModelFace>* ModelRenderer::ModelFaces(const ModelCacheKey& cacheKey)
    {
        auto it = loadedModels.find(cacheKey);
        if (it == loadedModels.end() || it->second.faces.empty()) { return nullptr; }
        return &it->second.faces;
    }

    std::string ModelRenderer::ModelSheetKey(const ModelCacheKey& cacheKey)
    {
        return "model:" + DescribeCacheKey(cacheKey);
    }

    bool ModelRenderer::DrawSpriteSheet2D(SDL_Renderer* renderer, const std::string& key,
                                          float x, float y, float width, float height)
    {
        // The GPU sheets live on the GPU device, not the SDL_Renderer, so this
        // keeps a parallel SDL texture per key - created on first use from the
        // same RGBA. Overlays are few and small, so the duplication is cheap.
        auto it = overlayTextures.find(key);
        if (it == overlayTextures.end())
        {
            auto pixels = overlayPixels.find(key);
            if (pixels == overlayPixels.end()) { return false; }

            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STATIC,
                                                     pixels->second.width, pixels->second.height);
            if (!texture) { return false; }
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

            // NEAREST, not linear. SDL defaults to linear filtering, which blends
            // across the seam where the two halves of the face crawl meet and
            // softens every edge - the blur Edward saw down the middle. Everything
            // else in this port is pixel-exact and this has to match.
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_UpdateTexture(texture, nullptr, pixels->second.rgba.data(),
                              pixels->second.width * 4);
            it = overlayTextures.emplace(key, texture).first;
        }

        SDL_FRect dst{ x, y, width, height };
        return SDL_RenderTexture(renderer, it->second, nullptr, &dst);
    }

    bool ModelRenderer::UploadSpriteSheet(const std::string& key, const std::vector<uint8_t>& rgba,
                                          int width, int height)
    {
        if (!device || width <= 0 || height <= 0) { return false; }
        const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        if (rgba.size() < expected) { return false; }

        auto existing = spriteSheets.find(key);
        if (existing != spriteSheets.end())
        {
            if (existing->second.texture) { SDL_ReleaseGPUTexture(device, existing->second.texture); }
            spriteSheets.erase(existing);
        }

        SDL_GPUTextureCreateInfo texInfo{};
        texInfo.type = SDL_GPU_TEXTURETYPE_2D;
        texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texInfo.width = static_cast<Uint32>(width);
        texInfo.height = static_cast<Uint32>(height);
        texInfo.layer_count_or_depth = 1;
        texInfo.num_levels = 1;
        texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);
        if (!texture)
        {
            SDL_Log("UploadSpriteSheet(%s): texture creation failed: %s", key.c_str(), SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(expected);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (!transfer) { SDL_ReleaseGPUTexture(device, texture); return false; }

        void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        std::memcpy(mapped, rgba.data(), expected);
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo texSrc{};
        texSrc.transfer_buffer = transfer;
        texSrc.offset = 0;
        SDL_GPUTextureRegion texDst{};
        texDst.texture = texture;
        texDst.w = static_cast<Uint32>(width);
        texDst.h = static_cast<Uint32>(height);
        texDst.d = 1;
        SDL_UploadToGPUTexture(copyPass, &texSrc, &texDst, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);

        spriteSheets[key] = SpriteSheet{ texture, width, height };

        // Keep the pixels too, so DrawSpriteSheet2D can make an SDL texture for
        // the same art when it is needed as a flat overlay.
        overlayPixels[key] = OverlayPixels{ rgba, width, height };
        return true;
    }

    std::vector<uint8_t> ModelRenderer::RenderLevelToRgba(const std::string& cacheKey, const FpsCamera& camera, int width, int height,
                                                             const std::vector<PlacedObject>& objects,
                                                             const std::vector<PlacedSprite>& sprites)
    {
        std::vector<uint8_t> pixels;
        RenderLevelToRgba(cacheKey, camera, width, height, objects, sprites, pixels);
        return pixels;
    }

    bool ModelRenderer::RenderLevelToRgba(const std::string& cacheKey, const FpsCamera& camera, int width, int height,
                                          const std::vector<PlacedObject>& objects,
                                          const std::vector<PlacedSprite>& sprites, std::vector<uint8_t>& pixels)
    {
        pixels.clear();
        if (!device || !pipeline) { return {}; }
        auto it = loadedLevels.find(cacheKey);
        if (it == loadedLevels.end()) { return {}; }
        LoadedLevel& level = it->second;

        if (!EnsureRenderTarget(width, height)) { return {}; }

        // Real explorable FPS camera - NOT auto-framed like the model
        // previews. Forward vector convention: yaw=0 looks down -Z,
        // positive yaw turns toward +X (standard right-handed FPS
        // convention) - this needs to stay consistent with whatever
        // movement code drives `camera`, but doesn't need to match
        // RenderToRgba's unrelated RotationY-based model-spin convention.
        float forwardX = std::sin(camera.yaw) * std::cos(camera.pitch);
        float forwardY = std::sin(camera.pitch);
        float forwardZ = -std::cos(camera.yaw) * std::cos(camera.pitch);

        // Up vector, tilted by roll around the forward axis. Left as the
        // world up when roll is zero so the common path is untouched.
        float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
        if (camera.roll != 0.0f)
        {
            // right = normalize(forward x worldUp); up = worldUp*cos + right*sin
            float sx = forwardZ, sy = 0.0f, sz = -forwardX; // forward x (0,1,0)
            float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
            if (slen > 0.0001f)
            {
                sx /= slen; sy /= slen; sz /= slen;
                float c = std::cos(camera.roll);
                float sn = std::sin(camera.roll);
                upX = sx * sn;
                upY = c;
                upZ = sz * sn;
            }
        }

        Mat4 view = Mat4::LookAt(camera.x, camera.y, camera.z,
                                  camera.x + forwardX, camera.y + forwardY, camera.z + forwardZ,
                                  upX, upY, upZ);
        // Near/far chosen generously for the level's own coordinate
        // scale - confirmed real L111LEV.MAP vertex coordinates span up
        // to roughly +-27000 units, so this comfortably covers that with
        // margin rather than being tuned to an assumed "normal" scene
        // size.
        Mat4 proj = Mat4::Perspective(camera.fovYRadians, static_cast<float>(width) / static_cast<float>(height), 20.0f, 60000.0f);
        Mat4 vp = Mat4::Multiply(proj, view);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

        SDL_GPUColorTargetInfo colorInfo{};
        colorInfo.texture = colorTarget;
        colorInfo.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f }; // opaque black - a full level view, not composited over anything
        colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthInfo{};
        depthInfo.texture = depthTarget;
        depthInfo.clear_depth = 1.0f;
        depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        // The cells to draw. When the set has changed since a group's list was
        // built, its list of the drawn cells' triangles is rebuilt and copied
        // up before the frame is drawn - one copy pass for all of them.
        // Not with the Render Distance option on: the walk stops 15 cells
        // out, which the original's fade hides and an unlimited view would not.
        const bool culling = fogEnabled > 0.5f && !level.drawnCells.empty() && !level.faceCell.empty();
        if (culling)
        {
            struct Pending { LevelSubGroup* group; size_t offset; size_t count; };
            std::vector<Pending> pending;
            std::vector<uint32_t> staging;
            for (auto* groups : { &level.groups, &level.cutoutGroups, &level.translucentGroups })
            {
                for (LevelSubGroup& group : *groups)
                {
                    if (!group.visibleIndexBuffer || group.builtSerial == level.drawnSerial) { continue; }
                    const size_t offset = staging.size();
                    for (size_t t = 0; t < group.triangleFace.size(); ++t)
                    {
                        const uint32_t face = group.triangleFace[t];
                        const int32_t cell = face < level.faceCell.size() ? level.faceCell[face] : -1;
                        const bool drawn = cell < 0 || (static_cast<size_t>(cell) < level.drawnCells.size()
                                                        && level.drawnCells[static_cast<size_t>(cell)] != 0);
                        if (!drawn) { continue; }
                        staging.insert(staging.end(), group.cpuIndices.begin() + static_cast<std::ptrdiff_t>(t * 3),
                                       group.cpuIndices.begin() + static_cast<std::ptrdiff_t>(t * 3 + 3));
                    }
                    pending.push_back({ &group, offset, staging.size() - offset });
                    group.builtSerial = level.drawnSerial;
                }
            }
            if (!pending.empty())
            {
                // One upload buffer kept between frames (cycled, as the last
                // one may still be in use), grown when a set needs more.
                const size_t bytes = staging.size() * sizeof(uint32_t);
                if (cullUpload && cullUploadSize < bytes)
                {
                    SDL_ReleaseGPUTransferBuffer(device, cullUpload);
                    cullUpload = nullptr;
                }
                const bool cycle = cullUpload != nullptr;
                if (!cullUpload && bytes > 0)
                {
                    SDL_GPUTransferBufferCreateInfo info{};
                    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                    info.size = static_cast<Uint32>(bytes + bytes / 2);
                    cullUpload = SDL_CreateGPUTransferBuffer(device, &info);
                    cullUploadSize = cullUpload ? bytes + bytes / 2 : 0;
                }
                SDL_GPUTransferBuffer* transfer = bytes > 0 ? cullUpload : nullptr;
                void* mapped = transfer ? SDL_MapGPUTransferBuffer(device, transfer, cycle) : nullptr;
                if (mapped)
                {
                    std::memcpy(mapped, staging.data(), staging.size() * sizeof(uint32_t));
                    SDL_UnmapGPUTransferBuffer(device, transfer);
                    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
                    for (const Pending& p : pending)
                    {
                        p.group->visibleCount = static_cast<uint32_t>(p.count);
                        if (p.count == 0) { continue; }
                        SDL_GPUTransferBufferLocation src{ transfer, static_cast<Uint32>(p.offset * sizeof(uint32_t)) };
                        SDL_GPUBufferRegion dst{ p.group->visibleIndexBuffer, 0, static_cast<Uint32>(p.count * sizeof(uint32_t)) };
                        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
                    }
                    SDL_EndGPUCopyPass(copy);
                }
                else if (bytes == 0)
                {
                    for (const Pending& p : pending) { p.group->visibleCount = 0; }
                }
                else
                {
                    for (const Pending& p : pending) { p.group->visibleCount = 0; p.group->builtSerial = ~0ull; }
                }
            }
        }
        // The index buffer and count a level group draws with.
        auto levelIndices = [&](const LevelSubGroup& group, SDL_GPUBuffer*& buffer, uint32_t& count) {
            if (culling && group.visibleIndexBuffer && group.builtSerial == level.drawnSerial)
            {
                buffer = group.visibleIndexBuffer;
                count = group.visibleCount;
            }
            else
            {
                buffer = group.indexBuffer;
                count = group.indexCount;
            }
        };

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmd, &colorInfo, 1, &depthInfo);
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

        // The opaque faces, then the cut-outs - same pipeline, the keyed page.
        // The see-through faces wait until everything else is down.
        auto levelGroups = { &level.groups, &level.cutoutGroups };
        for (const auto* groups : levelGroups)
        for (const auto& group : *groups)
        {
            if (!group.vertexBuffer || group.indexCount == 0) { continue; } // unused texture group for this level
            SDL_GPUBuffer* indexBuffer = nullptr;
            uint32_t indexCount = 0;
            levelIndices(group, indexBuffer, indexCount);
            if (indexCount == 0) { continue; }

            SDL_GPUBufferBinding vbBinding{ group.vertexBuffer, 0 };
            SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
            SDL_GPUBufferBinding ibBinding{ indexBuffer, 0 };
            SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_GPUTextureSamplerBinding texBinding{ group.texture, sampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
            SDL_PushGPUVertexUniformData(cmd, 0, vp.m.data(), sizeof(float) * 16);
            {
                float fog[8] = { fogEnabled, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
            }
            SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, 1, 0, 0, 0);
        }

        // -GAP override faces. Same pipeline and the level's own page
        // textures; they are ordinary world geometry that simply came from an
        // .obj instead of the .MAP. Drawn after the level so they sit on top
        // where they overlap, which is what a hole filler wants.
        for (const auto& group : level.gapGroups)
        {
            if (!group.vertexBuffer || group.indexCount == 0 || !group.texture) { continue; }

            SDL_GPUBufferBinding vbBinding{ group.vertexBuffer, 0 };
            SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
            SDL_GPUBufferBinding ibBinding{ group.indexBuffer, 0 };
            SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_GPUTextureSamplerBinding texBinding{ group.texture, sampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
            SDL_PushGPUVertexUniformData(cmd, 0, vp.m.data(), sizeof(float) * 16);
            {
                float fog[8] = { fogEnabled, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
            }
            SDL_DrawGPUIndexedPrimitives(renderPass, group.indexCount, 1, 0, 0, 0);
        }

        // Placed objects (crates, monsters, pickups, doors, lifts...) -
        // same pass, same pipeline, each with its own model matrix
        // combined into the level's vp (Edward, 2026: "all of the level
        // objects spawning"). Silently skipped if a requested object's
        // model was never actually loaded - matches this function's own
        // existing "level must already be loaded" convention, rather
        // than failing the whole level render over one missing model.
        // Models' see-through faces, drawn after everything else (below).
        struct SeeThroughDraw { const LoadedModel* model; Mat4 mvp; };
        std::vector<SeeThroughDraw> seeThroughDraws;
        for (const auto& object : objects)
        {
            if (!object.visible) { continue; }
            auto modelIt = loadedModels.find(object.cacheKey);
            if (modelIt == loadedModels.end()) { continue; }
            const LoadedModel& model = modelIt->second;
            if (!model.vertexBuffer || model.indexCount == 0) { continue; }

            Mat4 modelMatrix = Mat4::Multiply(
                Mat4::Translation(object.x, object.y, object.z),
                Mat4::Multiply(Mat4::RotationX(object.pitchRadians),
                Mat4::Multiply(Mat4::RotationY(object.rotationRadians + model.baseRotationRadians),
                               Mat4::Multiply(Mat4::RotationZ(object.rollRadians),
                                              Mat4::Scale(object.scaleX, object.scaleY, object.scaleZ)))));
            Mat4 mvp = Mat4::Multiply(vp, modelMatrix);

            SDL_BindGPUGraphicsPipeline(renderPass, model.useDoubleSided ? doubleSidedPipeline : pipeline);
            SDL_GPUBufferBinding vbBinding{ model.vertexBuffer, 0 };
            SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
            SDL_GPUBufferBinding ibBinding{ model.indexBuffer, 0 };
            SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_GPUTextureSamplerBinding texBinding{ model.texture, sampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
            SDL_PushGPUVertexUniformData(cmd, 0, mvp.m.data(), sizeof(float) * 16);
            {
                // The light of the cell it stands in: its byte 0x0e picks an
                // entry of the same table the level's faces use, and every
                // face of the model takes it (FUN_000269b8, FUN_000256ac,
                // FUN_000260cc - the objects', creatures', doors', lifts' and
                // thrown models' draws). Flat: the four corners averaged.
                float tint[3] = { 1.0f, 1.0f, 1.0f };
                if (object.lightId >= 0)
                {
                    const auto& entry = level.lights.ColourFor(object.lightId);
                    float sum[3] = { 0.0f, 0.0f, 0.0f };
                    for (const auto& c : entry.corner) { sum[0] += c.r; sum[1] += c.g; sum[2] += c.b; }
                    for (int k = 0; k < 3; ++k) { tint[k] = sum[k] / (4.0f * ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL); }
                }
                float fog[8] = { fogEnabled, fogStart, fogRange, 0.0f, tint[0], tint[1], tint[2], 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
            }
            const uint32_t opaqueCount = std::min(model.translucentFirst, model.indexCount);
            if (opaqueCount > 0) { SDL_DrawGPUIndexedPrimitives(renderPass, opaqueCount, 1, 0, 0, 0); }
            if (opaqueCount < model.indexCount) { seeThroughDraws.push_back({ &model, mvp }); }
        }

        // ---- world-space sprites -------------------------------------
        //
        // Built fresh each frame into one dynamic buffer and drawn grouped by
        // sheet. The pipeline is the model one: the fragment shader's
        // clip(a - 0.5) gives a hard cutout with a correct depth write, which is
        // exactly what a sprite needs, and the double-sided variant is used
        // because a billboard has no meaningful winding.
        if (!sprites.empty() && device)
        {
            // Y-axis billboard basis. The quad turns about the vertical only, so
            // a sprite stays upright when the player looks up or down - the
            // original's sprites have no pitch, and tipping them to face the
            // camera would look wrong under Free Look.
            float forwardX = std::sin(camera.yaw);
            float forwardZ = -std::cos(camera.yaw);
            float rightX = forwardZ;
            float rightZ = -forwardX;
            const float rightLen = std::sqrt(rightX * rightX + rightZ * rightZ);
            if (rightLen > 0.0001f) { rightX /= rightLen; rightZ /= rightLen; }

            size_t drawable = 0;
            for (const PlacedSprite& sprite : sprites)
            {
                if (sprite.visible && spriteSheets.count(sprite.textureKey)) { drawable++; }
            }

            if (drawable > 0)
            {
                // Grow the shared buffer if this frame needs more room. It is
                // never shrunk - sprite counts spike during a firefight and
                // settle afterwards, and reallocating on the way back down would
                // just churn.
                if (drawable > spriteBufferCapacity)
                {
                    if (spriteVertexBuffer) { SDL_ReleaseGPUBuffer(device, spriteVertexBuffer); }
                    if (spriteIndexBuffer) { SDL_ReleaseGPUBuffer(device, spriteIndexBuffer); }
                    spriteBufferCapacity = drawable + 64;

                    SDL_GPUBufferCreateInfo vbInfo{ SDL_GPU_BUFFERUSAGE_VERTEX,
                        static_cast<Uint32>(spriteBufferCapacity * 4 * sizeof(ALTEngine::Formats::RenderVertex)), 0 };
                    spriteVertexBuffer = SDL_CreateGPUBuffer(device, &vbInfo);
                    SDL_GPUBufferCreateInfo ibInfo{ SDL_GPU_BUFFERUSAGE_INDEX,
                        static_cast<Uint32>(spriteBufferCapacity * 6 * sizeof(uint32_t)), 0 };
                    spriteIndexBuffer = SDL_CreateGPUBuffer(device, &ibInfo);
                }

                if (spriteVertexBuffer && spriteIndexBuffer)
                {
                    // Grouped by sheet so each sheet costs one draw call. A
                    // contiguous run per group means the index range can just be
                    // an offset and a count.
                    std::vector<ALTEngine::Formats::RenderVertex> verts;
                    std::vector<uint32_t> indices;
                    verts.reserve(drawable * 4);
                    indices.reserve(drawable * 6);

                    struct SpriteBatch { SDL_GPUTexture* texture; uint32_t first; uint32_t count; bool ownFade; };
                    std::vector<SpriteBatch> batches;

                    // Batched by sheet, and by whether the sprite fades itself.
                    for (int fadePass = 0; fadePass < 2; ++fadePass)
                    for (const auto& sheetEntry : spriteSheets)
                    {
                        const uint32_t firstIndex = static_cast<uint32_t>(indices.size());
                        for (const PlacedSprite& sprite : sprites)
                        {
                            if (!sprite.visible || sprite.textureKey != sheetEntry.first) { continue; }
                            if (sprite.ownFade != (fadePass == 1)) { continue; }

                            const uint32_t base = static_cast<uint32_t>(verts.size());
                            float px[4], py[4], pz[4];

                            if (sprite.billboard)
                            {
                                const float dx = rightX * sprite.halfWidth;
                                const float dz = rightZ * sprite.halfWidth;
                                const float dy = sprite.halfHeight;
                                // top-left, top-right, bottom-right, bottom-left
                                px[0] = sprite.x - dx; px[1] = sprite.x + dx; px[2] = sprite.x + dx; px[3] = sprite.x - dx;
                                py[0] = sprite.y + dy; py[1] = sprite.y + dy; py[2] = sprite.y - dy; py[3] = sprite.y - dy;
                                pz[0] = sprite.z - dz; pz[1] = sprite.z + dz; pz[2] = sprite.z + dz; pz[3] = sprite.z - dz;
                            }
                            else
                            {
                                // World-oriented: build the quad flat on XY and
                                // turn it by the sprite's own angles. A tumbling
                                // shard needs this - a billboard would keep it
                                // permanently face-on and it would never look
                                // like it was spinning.
                                const float cxr = std::cos(sprite.rotX), sxr = std::sin(sprite.rotX);
                                const float cyr = std::cos(sprite.rotY), syr = std::sin(sprite.rotY);
                                const float czr = std::cos(sprite.rotZ), szr = std::sin(sprite.rotZ);

                                float lx[4] = { -sprite.halfWidth,  sprite.halfWidth,
                                                 sprite.halfWidth, -sprite.halfWidth };
                                float ly[4] = {  sprite.halfHeight, sprite.halfHeight,
                                                -sprite.halfHeight, -sprite.halfHeight };
                                float lz[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                                if (sprite.corners)
                                {
                                    for (int corner = 0; corner < 4; ++corner)
                                    {
                                        lx[corner] = sprite.cornerX[corner];
                                        ly[corner] = sprite.cornerY[corner];
                                        lz[corner] = sprite.cornerZ[corner];
                                    }
                                }

                                for (int corner = 0; corner < 4; ++corner)
                                {
                                    // Z, then Y, then X.
                                    float ax = lx[corner] * czr - ly[corner] * szr;
                                    float ay = lx[corner] * szr + ly[corner] * czr;
                                    float az = lz[corner];

                                    float bx = ax * cyr + az * syr;
                                    float bz = -ax * syr + az * cyr;

                                    float cy2 = ay * cxr - bz * sxr;
                                    float cz2 = ay * sxr + bz * cxr;

                                    px[corner] = sprite.x + bx;
                                    py[corner] = sprite.y + cy2;
                                    pz[corner] = sprite.z + cz2;
                                }
                            }
                            float u[4] = { sprite.u0, sprite.u1, sprite.u1, sprite.u0 };
                            float v[4] = { sprite.v0, sprite.v0, sprite.v1, sprite.v1 };
                            if (sprite.corners && !sprite.billboard)
                            {
                                for (int corner = 0; corner < 4; ++corner) { u[corner] = sprite.cornerU[corner]; v[corner] = sprite.cornerV[corner]; }
                            }

                            for (int corner = 0; corner < 4; ++corner)
                            {
                                ALTEngine::Formats::RenderVertex vertex{};
                                vertex.x = px[corner]; vertex.y = py[corner]; vertex.z = pz[corner];
                                vertex.u = u[corner];  vertex.v = v[corner];
                                vertex.r = sprite.r;   vertex.g = sprite.g;  vertex.b = sprite.b;
                                verts.push_back(vertex);
                            }
                            indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
                            indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
                        }
                        const uint32_t added = static_cast<uint32_t>(indices.size()) - firstIndex;
                        if (added > 0) { batches.push_back({ sheetEntry.second.texture, firstIndex, added, fadePass == 1 }); }
                    }

                    if (!batches.empty())
                    {
                        const size_t vbSize = verts.size() * sizeof(ALTEngine::Formats::RenderVertex);
                        const size_t ibSize = indices.size() * sizeof(uint32_t);

                        if (spriteTransfer && spriteTransferSize < vbSize + ibSize)
                        {
                            SDL_ReleaseGPUTransferBuffer(device, spriteTransfer);
                            spriteTransfer = nullptr;
                        }
                        const bool cycle = spriteTransfer != nullptr;
                        if (!spriteTransfer)
                        {
                            // Room for the whole sprite buffers, so it grows with them.
                            const size_t room = std::max(vbSize + ibSize, spriteBufferCapacity
                                * (4 * sizeof(ALTEngine::Formats::RenderVertex) + 6 * sizeof(uint32_t)));
                            SDL_GPUTransferBufferCreateInfo transferInfo{};
                            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                            transferInfo.size = static_cast<Uint32>(room);
                            spriteTransfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
                            spriteTransferSize = spriteTransfer ? room : 0;
                        }
                        SDL_GPUTransferBuffer* transfer = spriteTransfer;
                        uint8_t* mapped = transfer ? static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, cycle)) : nullptr;
                        if (mapped)
                        {
                            std::memcpy(mapped, verts.data(), vbSize);
                            std::memcpy(mapped + vbSize, indices.data(), ibSize);
                            SDL_UnmapGPUTransferBuffer(device, transfer);

                            // The copy has to happen outside the render pass, so
                            // this closes the pass, uploads, and opens a second
                            // pass that LOADs the existing colour and depth
                            // rather than clearing them.
                            SDL_EndGPURenderPass(renderPass);

                            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
                            SDL_GPUTransferBufferLocation vbSrc{ transfer, 0 };
                            SDL_GPUBufferRegion vbDst{ spriteVertexBuffer, 0, static_cast<Uint32>(vbSize) };
                            SDL_UploadToGPUBuffer(copyPass, &vbSrc, &vbDst, true);
                            SDL_GPUTransferBufferLocation ibSrc{ transfer, static_cast<Uint32>(vbSize) };
                            SDL_GPUBufferRegion ibDst{ spriteIndexBuffer, 0, static_cast<Uint32>(ibSize) };
                            SDL_UploadToGPUBuffer(copyPass, &ibSrc, &ibDst, true);
                            SDL_EndGPUCopyPass(copyPass);

                            SDL_GPUColorTargetInfo spriteColor{};
                            spriteColor.texture = colorTarget;
                            spriteColor.load_op = SDL_GPU_LOADOP_LOAD;
                            spriteColor.store_op = SDL_GPU_STOREOP_STORE;
                            SDL_GPUDepthStencilTargetInfo spriteDepth{};
                            spriteDepth.texture = depthTarget;
                            spriteDepth.load_op = SDL_GPU_LOADOP_LOAD;
                            spriteDepth.store_op = SDL_GPU_STOREOP_STORE;
                            spriteDepth.stencil_load_op = SDL_GPU_LOADOP_LOAD;
                            spriteDepth.stencil_store_op = SDL_GPU_STOREOP_STORE;

                            renderPass = SDL_BeginGPURenderPass(cmd, &spriteColor, 1, &spriteDepth);
                            if (renderPass)
                            {
                                SDL_BindGPUGraphicsPipeline(renderPass, doubleSidedPipeline);
                                SDL_GPUBufferBinding vbBinding{ spriteVertexBuffer, 0 };
                                SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
                                SDL_GPUBufferBinding ibBinding{ spriteIndexBuffer, 0 };
                                SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                                SDL_PushGPUVertexUniformData(cmd, 0, vp.m.data(), sizeof(float) * 16);
                                float fog[8] = { fogEnabled, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
                                bool fogOff = false;

                                for (const SpriteBatch& batch : batches)
                                {
                                    if (batch.ownFade != fogOff)
                                    {
                                        fogOff = batch.ownFade;
                                        fog[0] = fogOff ? 0.0f : fogEnabled;
                                        SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
                                    }
                                    SDL_GPUTextureSamplerBinding texBinding{ batch.texture, sampler };
                                    SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
                                    SDL_DrawGPUIndexedPrimitives(renderPass, batch.count, 1, batch.first, 0, 0);
                                }
                            }
                        }
                    }
                }
            }
        }

        // The see-through faces last, over everything they stand in front of:
        // glass, a cryopod's cover. Half the texel and half what is there
        // (span routine 0x5db67). Not sorted among themselves - nor were the
        // original's, which drew in its own face order.
        if (renderPass)
        {
            bool bound = false;
            for (const auto& group : level.translucentGroups)
            {
                if (!group.vertexBuffer || group.indexCount == 0) { continue; }
                SDL_GPUBuffer* indexBuffer = nullptr;
                uint32_t indexCount = 0;
                levelIndices(group, indexBuffer, indexCount);
                if (indexCount == 0) { continue; }
                if (!bound)
                {
                    SDL_BindGPUGraphicsPipeline(renderPass, translucentPipeline ? translucentPipeline : pipeline);
                    const SDL_FColor half{ 0.5f, 0.5f, 0.5f, 0.5f };
                    SDL_SetGPUBlendConstants(renderPass, half);
                    bound = true;
                }
                SDL_GPUBufferBinding vbBinding{ group.vertexBuffer, 0 };
                SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
                SDL_GPUBufferBinding ibBinding{ indexBuffer, 0 };
                SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                SDL_GPUTextureSamplerBinding texBinding{ group.texture, sampler };
                SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
                SDL_PushGPUVertexUniformData(cmd, 0, vp.m.data(), sizeof(float) * 16);
                // No distance fade: the original applies it to the light
                // colour (FUN_0004ecdc), which this fill never reads - span
                // 0x5db67 takes the texel's own colour (table 0x2273c8).
                float fog[8] = { 0.0f, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
                SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, 1, 0, 0, 0);
            }
            for (const auto& draw : seeThroughDraws)
            {
                if (!bound)
                {
                    SDL_BindGPUGraphicsPipeline(renderPass, translucentPipeline ? translucentPipeline : pipeline);
                    const SDL_FColor half{ 0.5f, 0.5f, 0.5f, 0.5f };
                    SDL_SetGPUBlendConstants(renderPass, half);
                    bound = true;
                }
                const LoadedModel& model = *draw.model;
                SDL_GPUBufferBinding vbBinding{ model.vertexBuffer, 0 };
                SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
                SDL_GPUBufferBinding ibBinding{ model.indexBuffer, 0 };
                SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                SDL_GPUTextureSamplerBinding texBinding{ model.texture, sampler };
                SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
                SDL_PushGPUVertexUniformData(cmd, 0, draw.mvp.m.data(), sizeof(float) * 16);
                float fog[8] = { 0.0f, fogStart, fogRange, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
                SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
                SDL_DrawGPUIndexedPrimitives(renderPass, model.indexCount - model.translucentFirst, 1, model.translucentFirst, 0, 0);
            }
        }

        if (renderPass) { SDL_EndGPURenderPass(renderPass); }

        return DownloadColorTarget(cmd, width, height, pixels);
    }
}
