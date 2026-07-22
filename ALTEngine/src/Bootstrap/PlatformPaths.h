#pragma once

#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI       // avoid windows.h macro-clobbering DrawText/Rectangle/etc.
#define NOMINMAX    // avoid windows.h macro-clobbering std::min/std::max/std::clamp
#include <windows.h>
#endif

namespace ALTEngine::Bootstrap
{
    inline std::filesystem::path ExecutableDirectory()
    {
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return std::filesystem::path(buffer).parent_path();
        }
#endif
        return std::filesystem::current_path();
    }
}
