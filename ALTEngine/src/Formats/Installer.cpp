#include "Installer.h"
#include "../Bootstrap/FsUtil.h"
#include "RetryOpen.h"

#include <system_error>

namespace ALTEngine::Formats
{
    namespace
    {
        // Disc-sourced files (physical CD or a mounted image) are
        // inherently read-only media, and copy_file on Windows can carry
        // that read-only attribute over to the destination copy - a
        // permanent file attribute, not a transient lock, so retrying
        // the write later never helps. Clear it explicitly right after
        // every copy.
        void ClearReadOnly(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::permissions(path,
                std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write,
                std::filesystem::perm_options::add, ec);
            // Deliberately not checked/reported - if this fails, the
            // subsequent patch write attempt will surface a clear error
            // anyway, and BinaryUtility does the same clear defensively
            // before every write regardless.
        }

        // Same transient-lock risk as BinaryUtility's reopen-for-write -
        // OneDrive/AV/Search indexer can briefly hold a file right after
        // it's created, which copy_file can hit just as easily as a
        // later reopen can.
        bool CopyFileWithRetry(const std::filesystem::path& source, const std::filesystem::path& dest)
        {
            bool copied = RetryOnTransientLock([&]() {
                std::error_code ec;
                std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing, ec);
                return !ec;
            });
            if (copied) { ClearReadOnly(dest); }
            return copied;
        }

        bool CopyOneFile(const std::filesystem::path& sourceDir, const std::filesystem::path& destDir, const std::string& name)
        {
            auto sourcePath = ALTEngine::Bootstrap::FindEntryCaseInsensitive(sourceDir, name);
            if (!sourcePath.has_value()) { return false; }

            std::error_code ec;
            std::filesystem::create_directories(destDir, ec);

            return CopyFileWithRetry(*sourcePath, destDir / name);
        }
    }

    InstallResult Installer::CopyFiles(
        const std::filesystem::path& discRoot,
        const std::filesystem::path& destinationRoot,
        const DiscManifest& manifest,
        const std::function<void(const InstallProgress&)>& onProgress)
    {
        InstallResult result;

        int totalFiles = static_cast<int>(manifest.rootFiles.size());
        for (const auto& [name, folder] : manifest.folders)
        {
            if (folder.copyAll)
            {
                auto sourceFolder = ALTEngine::Bootstrap::FindEntryCaseInsensitive(discRoot / "CD", name);
                if (sourceFolder.has_value())
                {
                    std::error_code ec;
                    for (const auto& entry : std::filesystem::directory_iterator(*sourceFolder, ec))
                    {
                        if (entry.is_regular_file() && !folder.IsExcluded(entry.path())) { ++totalFiles; }
                    }
                }
            }
            else
            {
                totalFiles += static_cast<int>(folder.files.size());
            }
        }

        int completed = 0;
        auto reportProgress = [&](const std::string& file) {
            ++completed;
            if (onProgress) { onProgress({ file, completed, totalFiles }); }
        };

        for (const auto& rootFile : manifest.rootFiles)
        {
            if (!CopyOneFile(discRoot, destinationRoot, rootFile))
            {
                result.failedFiles.push_back(rootFile);
            }
            reportProgress(rootFile);
        }

        for (const auto& [folderName, folder] : manifest.folders)
        {
            std::filesystem::path destFolder = destinationRoot / "CD" / folderName;
            auto sourceFolder = ALTEngine::Bootstrap::FindEntryCaseInsensitive(discRoot / "CD", folderName);

            if (!sourceFolder.has_value())
            {
                if (folder.copyAll)
                {
                    result.failedFiles.push_back(folderName + "/*");
                }
                else
                {
                    for (const auto& f : folder.files) { result.failedFiles.push_back(folderName + "/" + f); }
                }
                continue;
            }

            if (folder.copyAll)
            {
                std::error_code ec;
                std::filesystem::create_directories(destFolder, ec);
                for (const auto& entry : std::filesystem::directory_iterator(*sourceFolder, ec))
                {
                    if (!entry.is_regular_file()) { continue; }

                    // Skipped by extension - see DiscFolder::excludeExtensions.
                    // LANGUAGE uses this for *.BIN: the folder still has to be
                    // copied wholesale for its PNLGFX HUD files, but the language
                    // binaries are redundant now their text is in the packs.
                    if (folder.IsExcluded(entry.path())) { continue; }

                    std::string filename = entry.path().filename().string();

                    bool copied = CopyFileWithRetry(entry.path(), destFolder / filename);
                    if (!copied) { result.failedFiles.push_back(folderName + "/" + filename); }
                    reportProgress(folderName + "/" + filename);
                }
            }
            else
            {
                for (const auto& file : folder.files)
                {
                    if (!CopyOneFile(*sourceFolder, destFolder, file))
                    {
                        result.failedFiles.push_back(folderName + "/" + file);
                    }
                    reportProgress(folderName + "/" + file);
                }
            }
        }

        result.success = result.failedFiles.empty();
        return result;
    }
}
