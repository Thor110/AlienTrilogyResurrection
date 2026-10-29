#pragma once

#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI       // avoid windows.h macro-clobbering DrawText/Rectangle/etc.
#define NOMINMAX    // avoid windows.h macro-clobbering std::min/std::max/std::clamp
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#include <vector>
#elif defined(__linux__)
#include <system_error>
#endif
#include <cstdlib>
#include <vector>

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
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size + 1, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) == 0)
        {
            std::error_code ec;
            auto path = std::filesystem::canonical(buffer.data(), ec);
            if (!ec) { return path.parent_path(); }
            return std::filesystem::path(buffer.data()).parent_path();
        }
#elif defined(__linux__)
        std::error_code ec;
        auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) { return path.parent_path(); }
#endif
        return std::filesystem::current_path();
    }

    // Where removable media and other drives appear on this system: Windows
    // drive letters are listed by the callers themselves; elsewhere these are
    // the folders a disc or a mounted image turns up under (macOS /Volumes,
    // Linux /media/<user>, /run/media/<user> and /mnt).
    inline std::vector<std::filesystem::path> MountRoots()
    {
        std::vector<std::filesystem::path> roots;
#if defined(__APPLE__)
        roots.push_back("/Volumes");
#elif !defined(_WIN32)
        if (const char* user = std::getenv("USER"))
        {
            roots.push_back(std::filesystem::path("/media") / user);
            roots.push_back(std::filesystem::path("/run/media") / user);
        }
        roots.push_back("/media");
        roots.push_back("/mnt");
#endif
        return roots;
    }
}
