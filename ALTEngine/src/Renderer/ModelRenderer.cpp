#include "ModelRenderer.h"
#include "Mat4.h"
#include "../Bootstrap/PlatformPaths.h"
#include "../Formats/BndParser.h"
#include "../Formats/BndTextureLoader.h"
#include "../Formats/ModelLoader.h"
#include "../Formats/RenderMesh.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace ALTEngine::Renderer
{
    namespace
    {
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
            SDL_GPUTexture* texture = nullptr;
            float centerX = 0, centerY = 0, centerZ = 0; // AABB center, for auto-framing the camera
            float radius = 1.0f;                          // AABB bounding-sphere radius
            float baseRotationRadians = 0.0f;              // fixed per-model orientation offset - see LoadModel's doc comment
        };

        SDL_GPUDevice* device = nullptr;
        SDL_GPUGraphicsPipeline* pipeline = nullptr;
        SDL_GPUSampler* sampler = nullptr;
        SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        std::unordered_map<std::string, LoadedModel> loadedModels;

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

        SDL_GPUShader* LoadShader(SDL_GPUDevice* dev, const std::filesystem::path& path, SDL_GPUShaderStage stage,
                                   Uint32 numSamplers, Uint32 numUniformBuffers)
        {
            std::vector<uint8_t> code = ReadFile(path);
            if (code.empty())
            {
                SDL_Log("ModelRenderer: could not read shader %s", path.string().c_str());
                return nullptr;
            }

            SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(dev);
            SDL_GPUShaderFormat format;
            if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) { format = SDL_GPU_SHADERFORMAT_SPIRV; }
            else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) { format = SDL_GPU_SHADERFORMAT_DXIL; }
            else
            {
                SDL_Log("ModelRenderer: no supported shader format (need SPIRV or DXIL)");
                return nullptr;
            }

            SDL_GPUShaderCreateInfo info{};
            info.code = code.data();
            info.code_size = code.size();
            info.entrypoint = "main";
            info.format = format;
            info.stage = stage;
            info.num_samplers = numSamplers;
            info.num_uniform_buffers = numUniformBuffers;

            return SDL_CreateGPUShader(dev, &info);
        }
    }

    bool ModelRenderer::Initialize()
    {
        device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
        if (!device)
        {
            SDL_Log("ModelRenderer::Initialize: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return false;
        }

        // Shader format determines which compiled file to load - .spv or
        // .dxil, matching the two-track approach (HLSL shipped/compiled
        // to DXIL for the real Windows build; SPIR-V used for local
        // testing here, same source logic, different compiler).
        SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
        std::filesystem::path shaderDir = ExecutableDirectory() / "data" / "shaders";
        std::string ext = (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) ? ".spv" : ".dxil";

        SDL_GPUShader* vertexShader = LoadShader(device, shaderDir / ("model.vert" + ext), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader* fragmentShader = LoadShader(device, shaderDir / ("model.frag" + ext), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
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
        vbDesc.pitch = sizeof(float) * 5; // x,y,z,u,v
        vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[2]{};
        attrs[0] = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 };
        attrs[1] = { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, sizeof(float) * 3 };

        SDL_GPUColorTargetDescription colorDesc{};
        colorDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
        pipelineInfo.vertex_input_state.vertex_attributes = attrs;
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorDesc;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = depthFormat;
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        // NONE rather than BACK - the winding direction of the quads'
        // vertex order (A,B,C,D) hasn't been confirmed against the
        // original renderer, and getting that wrong with backface
        // culling on would silently show an empty/black result, which
        // is a much worse failure mode than just not culling for now.
        // Revisit once basic correctness is confirmed on real hardware.
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);

        if (!pipeline)
        {
            SDL_Log("ModelRenderer::Initialize: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
            SDL_DestroyGPUDevice(device);
            device = nullptr;
            return false;
        }

        // NEAREST filtering - matches the "Original" (PS1-authentic,
        // pixelated) render fidelity, the current default (see
        // RenderSettings) - not the "Smoothed" alternative.
        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler = SDL_CreateGPUSampler(device, &samplerInfo);

        return sampler != nullptr;
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

        if (colorTarget) { SDL_ReleaseGPUTexture(device, colorTarget); colorTarget = nullptr; }
        if (depthTarget) { SDL_ReleaseGPUTexture(device, depthTarget); depthTarget = nullptr; }
        if (downloadBuffer) { SDL_ReleaseGPUTransferBuffer(device, downloadBuffer); downloadBuffer = nullptr; }
        if (sampler) { SDL_ReleaseGPUSampler(device, sampler); sampler = nullptr; }
        if (pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, pipeline); pipeline = nullptr; }

        SDL_DestroyGPUDevice(device);
        device = nullptr;
    }

    bool ModelRenderer::LoadModel(const std::string& cacheKey, int meshNumber,
                                   const std::filesystem::path& objBndPath, const std::filesystem::path& gfxBndPath,
                                   std::optional<std::array<uint8_t, 3>> transparentRgb,
                                   float baseRotationRadians)
    {
        if (!device) { return false; }
        if (loadedModels.count(cacheKey)) { return true; }

        std::vector<ALTEngine::Formats::ModelMesh> meshes;
        BndTextureSet textureSet;
        try
        {
            meshes = ModelLoader::Load(objBndPath);
            textureSet = BndTextureLoader::Load(gfxBndPath, transparentRgb);
        }
        catch (const std::exception& e)
        {
            SDL_Log("ModelRenderer::LoadModel(%s): %s", cacheKey.c_str(), e.what());
            return false;
        }

        const auto* mesh = ModelLoader::FindByNumber(meshes, meshNumber);
        if (!mesh)
        {
            SDL_Log("ModelRenderer::LoadModel(%s): no section for mesh number %d in %s", cacheKey.c_str(), meshNumber, objBndPath.string().c_str());
            return false;
        }

        // Auto-detected texture scheme: exactly one BX section means
        // every model in this file shares that one texture+UV group
        // (confirmed for PICKGFX.BND, used by both PICKMOD and OBJ3D) -
        // otherwise each model uses the texture/BX section matching its
        // own number (confirmed for OPTGFX.BND, one per OPTOBJ model).
        const ALTEngine::Formats::BndTexture* tex = nullptr;
        const std::vector<ALTEngine::Formats::BxRectangle>* uvRects = nullptr;

        if (textureSet.uvSections.size() == 1 && !textureSet.textures.empty())
        {
            tex = &textureSet.textures[0];
            uvRects = &textureSet.uvSections[0];
        }
        else
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%02d", meshNumber);
            std::string targetIndex(buf);
            for (size_t i = 0; i < textureSet.textures.size() && i < textureSet.uvSections.size(); ++i)
            {
                if (textureSet.textures[i].index == targetIndex)
                {
                    tex = &textureSet.textures[i];
                    uvRects = &textureSet.uvSections[i];
                    break;
                }
            }
        }

        if (!tex || !uvRects)
        {
            SDL_Log("ModelRenderer::LoadModel(%s): no matching texture for mesh number %d in %s", cacheKey.c_str(), meshNumber, gfxBndPath.string().c_str());
            return false;
        }

        RenderMesh renderMesh = BuildRenderMesh(*mesh, *uvRects);
        if (renderMesh.vertices.empty() || renderMesh.indices.empty())
        {
            SDL_Log("ModelRenderer::LoadModel(%s): model has no geometry", cacheKey.c_str());
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

        size_t vbSize = renderMesh.vertices.size() * sizeof(float) * 5;
        size_t ibSize = renderMesh.indices.size() * sizeof(uint32_t);
        size_t texSize = static_cast<size_t>(tex->width) * tex->height * 4;

        SDL_GPUBufferCreateInfo vbInfo{ SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(vbSize) };
        SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(device, &vbInfo);
        SDL_GPUBufferCreateInfo ibInfo{ SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(ibSize) };
        SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(device, &ibInfo);

        SDL_GPUTextureCreateInfo texInfo{};
        texInfo.type = SDL_GPU_TEXTURETYPE_2D;
        texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texInfo.width = static_cast<Uint32>(tex->width);
        texInfo.height = static_cast<Uint32>(tex->height);
        texInfo.layer_count_or_depth = 1;
        texInfo.num_levels = 1;
        texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);

        if (!vertexBuffer || !indexBuffer || !texture)
        {
            SDL_Log("ModelRenderer::LoadModel: buffer/texture creation failed: %s", SDL_GetError());
            if (vertexBuffer) { SDL_ReleaseGPUBuffer(device, vertexBuffer); }
            if (indexBuffer) { SDL_ReleaseGPUBuffer(device, indexBuffer); }
            if (texture) { SDL_ReleaseGPUTexture(device, texture); }
            return false;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(vbSize + ibSize + texSize);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

        uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false));
        std::memcpy(mapped, renderMesh.vertices.data(), vbSize);
        std::memcpy(mapped + vbSize, renderMesh.indices.data(), ibSize);
        std::memcpy(mapped + vbSize + ibSize, tex->rgba.data(), texSize);
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
        texDst.w = static_cast<Uint32>(tex->width);
        texDst.h = static_cast<Uint32>(tex->height);
        texDst.d = 1;
        SDL_UploadToGPUTexture(copyPass, &texSrc, &texDst, false);

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);

        LoadedModel model;
        model.vertexBuffer = vertexBuffer;
        model.indexBuffer = indexBuffer;
        model.indexCount = static_cast<uint32_t>(renderMesh.indices.size());
        model.texture = texture;
        model.centerX = cx; model.centerY = cy; model.centerZ = cz;
        model.radius = radius;
        model.baseRotationRadians = baseRotationRadians;
        loadedModels[cacheKey] = model;

        return true;
    }

    std::vector<uint8_t> ModelRenderer::RenderToRgba(const std::string& cacheKey, float angleRadians, int width, int height)
    {
        if (!device || !pipeline) { return {}; }
        auto it = loadedModels.find(cacheKey);
        if (it == loadedModels.end()) { return {}; }
        const LoadedModel& model = it->second;

        if (width != targetWidth || height != targetHeight || !colorTarget)
        {
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
                SDL_Log("ModelRenderer::RenderToRgba: render target creation failed: %s", SDL_GetError());
                return {};
            }
        }

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

        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_GPUBufferBinding vbBinding{ model.vertexBuffer, 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);
        SDL_GPUBufferBinding ibBinding{ model.indexBuffer, 0 };
        SDL_BindGPUIndexBuffer(renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_GPUTextureSamplerBinding texBinding{ model.texture, sampler };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);
        SDL_PushGPUVertexUniformData(cmd, 0, mvp.m.data(), sizeof(float) * 16);
        SDL_DrawGPUIndexedPrimitives(renderPass, model.indexCount, 1, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);

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
        std::vector<uint8_t> result(static_cast<size_t>(width) * height * 4);
        std::memcpy(result.data(), mapped, result.size());
        SDL_UnmapGPUTransferBuffer(device, downloadBuffer);

        return result;
    }
}
