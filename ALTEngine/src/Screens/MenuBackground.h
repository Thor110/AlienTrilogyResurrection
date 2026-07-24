#pragma once

#include <filesystem>

#include <SDL3/SDL.h>

namespace ALTEngine::Screens
{
    // Loads one of LOGOSGFX's 2 images (imageIndex 0 = main menu
    // background, 1 = options/generic-menu background) as an SDL_Texture,
    // ready to draw. Factored out of MenuController.cpp, which had its
    // own private copy of this before other screens (Load/Save Game,
    // Multiplayer, etc) needed the exact same thing - every menu-style
    // screen shares this one background, confirmed against the reference
    // screenshots (all showing the same spine-textured backdrop).
    //
    // Returns nullptr (logs why) if the file can't be found or fails to
    // decode - callers should keep working without a background rather
    // than treat this as fatal.
    SDL_Texture* LoadMenuBackground(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer, int imageIndex, int& outW, int& outH);

    // Clears to black, then draws `texture` centered and scaled to fit
    // the current render output size (letterboxed, aspect preserved).
    // Safe to call with texture == nullptr (just clears).
    void DrawMenuBackground(SDL_Renderer* renderer, SDL_Texture* texture, int texW, int texH);
}
