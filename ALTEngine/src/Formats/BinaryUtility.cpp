#include "BinaryUtility.h"
#include "RetryOpen.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace ALTEngine::Formats
{
    namespace
    {
        // Defensive, regardless of how the file got here (Installer
        // already does this at copy-time, but patching shouldn't depend
        // on that other code path having run) - disc-sourced files are
        // inherently read-only media, and a plain copy can carry that
        // attribute over to the destination. That's a permanent file
        // attribute, not a transient lock - RetryOnTransientLock alone
        // never fixes this case, hence clearing it explicitly first.
        void ClearReadOnly(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::permissions(path,
                std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write,
                std::filesystem::perm_options::add, ec);
        }
    }

    uint8_t BinaryUtility::ReadByteAtOffset(const std::filesystem::path& path, uint64_t offset)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
        {
            throw std::runtime_error("BinaryUtility::ReadByteAtOffset: could not open " + path.string());
        }
        stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        char byte = 0;
        stream.read(&byte, 1);
        if (!stream)
        {
            throw std::runtime_error("BinaryUtility::ReadByteAtOffset: read past end of " + path.string());
        }
        return static_cast<uint8_t>(byte);
    }

    std::vector<uint8_t> BinaryUtility::ReadBytesAtOffset(const std::filesystem::path& path, uint64_t offset, size_t length)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
        {
            throw std::runtime_error("BinaryUtility::ReadBytesAtOffset: could not open " + path.string());
        }
        stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        std::vector<uint8_t> buffer(length);
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(length));
        if (!stream)
        {
            throw std::runtime_error("BinaryUtility::ReadBytesAtOffset: read past end of " + path.string());
        }
        return buffer;
    }

    void BinaryUtility::ReplaceByte(const std::filesystem::path& path, uint64_t offset, uint8_t value)
    {
        ClearReadOnly(path);

        std::fstream stream;
        bool opened = RetryOnTransientLock([&]() {
            stream.open(path, std::ios::binary | std::ios::in | std::ios::out);
            return stream.is_open();
        });
        if (!opened)
        {
            throw std::runtime_error(
                "BinaryUtility::ReplaceByte: could not open " + path.string() +
                " (" + std::strerror(errno) + ") - already tried clearing the read-only attribute and "
                "retrying briefly. Check antivirus real-time scanning or Program Files permissions.");
        }
        stream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
        char byte = static_cast<char>(value);
        stream.write(&byte, 1);
    }

    void BinaryUtility::ReplaceBytes(const std::filesystem::path& path, const std::vector<BinaryEdit>& edits)
    {
        // Mirrors BinaryUtility.cs's Replace(): read the whole file into
        // memory, apply every edit in list order (so later edits win on
        // overlap), write the whole file back once.
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in.is_open())
        {
            throw std::runtime_error(
                "BinaryUtility::ReplaceBytes: could not open " + path.string() +
                " (" + std::strerror(errno) + ")");
        }
        std::streamoff rawSize = in.tellg();
        if (rawSize < 0)
        {
            throw std::runtime_error("BinaryUtility::ReplaceBytes: could not determine size of " + path.string());
        }
        auto size = static_cast<size_t>(rawSize);
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!in)
        {
            throw std::runtime_error("BinaryUtility::ReplaceBytes: failed reading " + path.string());
        }
        in.close();

        for (const auto& edit : edits)
        {
            if (edit.offset + edit.bytes.size() > data.size())
            {
                throw std::runtime_error(
                    "BinaryUtility::ReplaceBytes: edit at offset " + std::to_string(edit.offset) +
                    " (" + std::to_string(edit.bytes.size()) + " bytes) exceeds file size " +
                    std::to_string(data.size()) + " for " + path.string());
            }
            std::copy(edit.bytes.begin(), edit.bytes.end(), data.begin() + static_cast<ptrdiff_t>(edit.offset));
        }

        ClearReadOnly(path);

        std::ofstream out;
        bool opened = RetryOnTransientLock([&]() {
            out.open(path, std::ios::binary | std::ios::trunc);
            return out.is_open();
        });
        if (!opened)
        {
            throw std::runtime_error(
                "BinaryUtility::ReplaceBytes: could not reopen " + path.string() +
                " for writing (" + std::strerror(errno) + ") - already tried clearing the read-only "
                "attribute and retrying briefly. Check antivirus real-time scanning, or Program Files "
                "permissions (run elevated).");
        }
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
}
