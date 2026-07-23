#include "Installer.h"
#include "../Bootstrap/FsUtil.h"

#include <system_error>

namespace ALTEngine::Formats
{
    namespace
    {
        bool CopyOneFile(const std::filesystem::path& sourceDir, const std::filesystem::path& destDir, const std::string& name)
        {
            auto sourcePath = ALTEngine::Bootstrap::FindEntryCaseInsensitive(sourceDir, name);
            if (!sourcePath.has_value()) { return false; }

            std::error_code ec;
            std::filesystem::create_directories(destDir, ec);

            std::filesystem::copy_file(*sourcePath, destDir / name, std::filesystem::copy_options::overwrite_existing, ec);
            return !ec;
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
                        if (entry.is_regular_file()) { ++totalFiles; }
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
                    std::string filename = entry.path().filename().string();

                    std::error_code copyEc;
                    std::filesystem::copy_file(entry.path(), destFolder / filename,
                                                std::filesystem::copy_options::overwrite_existing, copyEc);
                    if (copyEc) { result.failedFiles.push_back(folderName + "/" + filename); }
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
