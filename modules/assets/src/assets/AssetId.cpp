#include "assets/AssetId.h"

namespace mts
{
    uint64_t Fnv1a64(std::span<const std::byte> bytes)
    {
        uint64_t hash = 14695981039346656037ull;
        for (std::byte b : bytes)
        {
            hash ^= static_cast<unsigned char>(b);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    uint64_t Fnv1a64(std::string_view s)
    {
        return Fnv1a64(std::as_bytes(std::span<const char>(s.data(), s.size())));
    }

    AssetId MakeAssetId(std::string_view path)
    {
        return AssetId{Fnv1a64(path)};
    }
}
