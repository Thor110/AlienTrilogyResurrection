#include "Installer.h"
#include "../Bootstrap/FsUtil.h"
#include "RetryOpen.h"

#include <system_error>

namespace ALTEngine::Formats
{
    namespace
    {

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
            // A copy cut short (a disc read error, a full disk) must count as
            // failed, not leave a short file the game later cannot read.
            if (copied)
            {
                std::error_code sourceError, destError;
                const auto sourceSize = std::filesystem::file_size(source, sourceError);
                const auto destSize = std::filesystem::file_size(dest, destError);
                if (sourceError || destError || sourceSize != destSize) { return false; }
            }
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
                    std::filesystem::directory_iterator it(*sourceFolder, std::filesystem::directory_options::skip_permission_denied, ec);
                    for (const std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
                    {
                        std::error_code fileEc;
                        if (it->is_regular_file(fileEc) && !folder.IsExcluded(it->path())) { ++totalFiles; }
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

        // The root files (README.TXT) are the disc's readme: an install
        // made from an existing installation may not have kept it, and
        // nothing reads it, so a missing one does not fail the install.
        for (const auto& rootFile : manifest.rootFiles)
        {
            if (!CopyOneFile(discRoot, destinationRoot, rootFile))
            {
                result.skippedFiles.push_back(rootFile);
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
                std::filesystem::directory_iterator it(*sourceFolder, std::filesystem::directory_options::skip_permission_denied, ec);
                for (const std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
                {
                    const auto& entry = *it;
                    std::error_code fileEc;
                    if (!entry.is_regular_file(fileEc)) { continue; }

                    // Skipped by extension - see DiscFolder::excludeExtensions.
                    // LANGUAGE uses this for *.BIN: the folder still has to be
                    // copied wholesale for its PNLGFX HUD files, but the language
                    // binaries are redundant now their text is in the packs.
                    if (folder.IsExcluded(entry.path())) { continue; }

                    const std::string filename = ALTEngine::Bootstrap::PathText(entry.path().filename());

                    bool copied = CopyFileWithRetry(entry.path(), destFolder / entry.path().filename());
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
