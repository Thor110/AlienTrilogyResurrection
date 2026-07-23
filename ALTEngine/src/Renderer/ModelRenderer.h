#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ALTEngine::Renderer
{
    // Renders OPTOBJ (and, later, OBJ3D/PICKMOD/level) models via
    // SDL_GPU. Architecture note: the rest of the menu is drawn with the
    // existing SDL_Renderer 2D API, which doesn't directly share
    // resources with SDL_GPU - rather than rely on uncertain GPU-texture
    // interop between the two, this renders to an offscreen SDL_GPU
    // color target, downloads the result to CPU (SDL_DownloadFromGPUTexture),
    // and hands back plain RGBA bytes ready for SDL_UpdateTexture - the
    // exact same mechanism already used for the splash background images.
    // Costs a GPU->CPU->GPU round trip per frame, which is an acceptable
    // trade for reliability given this is one small menu preview, not
    // gameplay-critical performance.
    class ModelRenderer
    {
    public:
        // Creates the SDL_GPU device, loads shaders, builds the pipeline.
        // Call once. Returns false (logs why) on any failure - the menu
        // should keep working with placeholder boxes if this fails, not
        // crash the whole program over a missing GPU feature.
        static bool Initialize();
        static void Shutdown();

        // Loads and GPU-uploads a model by its OPTOBJ section index (see
        // Formats/ModelIndices.h), from the given OPTOBJ.BND/OPTGFX.BND
        // pair. Cached - repeated calls with the same index are cheap.
        // Returns false if the model/texture couldn't be loaded.
        static bool LoadModel(int modelIndex, const std::filesystem::path& objBndPath, const std::filesystem::path& gfxBndPath);

        // Renders `modelIndex` (must already be loaded via LoadModel),
        // rotated by `angleRadians` around Y, into a `width`x`height`
        // offscreen target, and returns the result as tightly-packed
        // RGBA8 bytes (width*height*4), or an empty vector on failure.
        static std::vector<uint8_t> RenderToRgba(int modelIndex, float angleRadians, int width, int height);
    };
}
