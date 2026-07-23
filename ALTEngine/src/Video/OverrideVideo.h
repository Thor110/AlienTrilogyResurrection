#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace ALTEngine::Video
{
    // Checks cdDirectory/Override/AVI/{baseName}.{ext} for a few common
    // video extensions (mp4 first, since that's the confirmed upscale
    // format, but a couple of others too in case something else gets
    // used later). Keyed on the *original* (non-localized) base name -
    // e.g. "INTRO", not "FINTRO" - since there's no indication overrides
    // need to vary per language the way the originals occasionally do.
    //
    // No decoding happens here - VideoPlayer is already fully
    // container/codec-agnostic via FFmpeg, so an override is just a
    // different file path to hand it, same as the original.
    std::optional<std::filesystem::path> FindOverrideVideo(const std::filesystem::path& cdDirectory, const std::string& baseName);
}
