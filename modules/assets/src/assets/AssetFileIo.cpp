#include "assets/AssetFileIo.h"

#include "core/log/Log.h"

#include <fstream>

namespace mts
{
    std::optional<std::vector<std::byte>> ReadFileBytes(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            MTS_LOG_ERROR("ReadFileBytes: cannot open {}", path.string());
            return std::nullopt;
        }

        const std::streamsize size = file.tellg();
        if (size < 0)
        {
            MTS_LOG_ERROR("ReadFileBytes: cannot determine size of {}", path.string());
            return std::nullopt;
        }
        file.seekg(0);

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        if (!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), size))
        {
            MTS_LOG_ERROR("ReadFileBytes: short read on {}", path.string());
            return std::nullopt;
        }

        return bytes;
    }

    bool WriteFileBytes(const std::filesystem::path &path, std::span<const std::byte> bytes)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            MTS_LOG_ERROR("WriteFileBytes: cannot open {}", path.string());
            return false;
        }

        if (!bytes.empty() &&
            !file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        {
            MTS_LOG_ERROR("WriteFileBytes: short write on {}", path.string());
            return false;
        }

        return true;
    }
}
