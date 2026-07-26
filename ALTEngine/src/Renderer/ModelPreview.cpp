#include "ModelPreview.h"
#include "ModelRenderer.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace ALTEngine::Renderer
{
    bool DrawModelPreview(SDL_Renderer* renderer, const ModelPreviewSource& source,
                          int x, int y, int w, int h, float rotationAngle)
    {
        if (source.modelIndex < 0) { return false; }

        // Initialize() is idempotent (cheap no-op if already valid) -
        // calling it fresh every time rather than caching "did I
        // already try" avoids exactly the bug this fixed: another
        // screen (GameplayScreen) can call Shutdown() between menu
        // visits, which a cached flag here would have no way to know
        // about (Edward, 2026 - "Options no longer displays models"
        // after a gameplay session).
        if (!ModelRenderer::Initialize())
        {
            SDL_Log("DrawModelPreview: ModelRenderer unavailable");
            return false;
        }

        ModelCacheKey cacheKey = source.CacheKey();
        if (!ModelRenderer::LoadModel(cacheKey, source.modelIndex, source.objBndPath, source.gfxBndPath,
                                       source.transparentRgb, source.baseRotationRadians))
        {
            return false;
        }

        // Render directly at display size - LINEAR texture filtering
        // (see ModelRenderer::Initialize's sampler comment) smooths the
        // source texture sampling itself, which is where PS1-style
        // ordered dithering needs to blend. An earlier attempt at this
        // rendered small and post-process-blurred the final 2D image
        // instead - wrong layer, blurred geometry edges that didn't
        // need it without fixing the actual texture sampling (Edward,
        // 2026).
        int renderSize = std::min(w, h);
        if (renderSize < 64) { renderSize = 64; }
        std::vector<uint8_t> pixels = ModelRenderer::RenderToRgba(cacheKey, rotationAngle, renderSize, renderSize);
        if (pixels.empty()) { return false; }

        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, renderSize, renderSize);
        if (!texture) { return false; }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND); // the render's clear color is transparent - let it composite over the menu background
        SDL_UpdateTexture(texture, nullptr, pixels.data(), renderSize * 4);

        SDL_FRect dest{ static_cast<float>(x + (w - renderSize) / 2), static_cast<float>(y + (h - renderSize) / 2),
                        static_cast<float>(renderSize), static_cast<float>(renderSize) };
        SDL_RenderTexture(renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
        return true;
    }

    void PreloadOptobjModels(const std::filesystem::path& cdDirectory)
    {
        if (!ModelRenderer::Initialize())
        {
            SDL_Log("PreloadOptobjModels: ModelRenderer unavailable, skipping preload");
            return;
        }

        std::vector<PreloadRequest> requests;

        // OPTOBJ: confirmed gap-free, 14 models (0-13) - see
        // ModelLoader.h's own confirmation note.
        for (int i = 0; i < 14; i++)
        {
            ModelPreviewSource source = ModelPreviewSource::ForOptobj(cdDirectory, i);
            requests.push_back({ source.CacheKey(), i, source.objBndPath, source.gfxBndPath,
                                  source.transparentRgb, source.baseRotationRadians });
        }

        ModelRenderer::PreloadBatch(requests);
    }

    void PreloadGameplayModels(const std::filesystem::path& cdDirectory, ALTEngine::Bootstrap::Language language)
    {
        if (!ModelRenderer::Initialize())
        {
            SDL_Log("PreloadGameplayModels: ModelRenderer unavailable, skipping preload");
            return;
        }

        std::vector<PreloadRequest> requests;

        // PICKMOD: 0-27 (28 slots), confirmed gaps at 5 and 24, 26
        // actually present (Edward, 2026 - corrected from an earlier,
        // wrong 0-25 range that silently missed 26/27 entirely).
        // PreloadBatch already skips a missing section number gracefully
        // (logs, moves on) rather than failing the whole batch, so this
        // just tries the full 0-27 range without needing to special-
        // case the gaps itself.
        for (int i = 0; i <= 27; i++)
        {
            ModelPreviewSource source = ModelPreviewSource::ForPickmod(cdDirectory, i);
            requests.push_back({ source.CacheKey(), i, source.objBndPath, source.gfxBndPath,
                                  source.transparentRgb, source.baseRotationRadians });
        }

        // OBJ3D: 0-41 (42 slots, no known gaps - Edward, 2026).
        for (int i = 0; i <= 41; i++)
        {
            ModelPreviewSource source = ModelPreviewSource::ForObj3D(cdDirectory, i, language);
            requests.push_back({ source.CacheKey(), i, source.objBndPath, source.gfxBndPath,
                                  source.transparentRgb, source.baseRotationRadians });
        }

        ModelRenderer::PreloadBatch(requests);
    }
}
