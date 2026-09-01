#include "assets/AssetId.h"

namespace mts
{
    uint64_t Fnv1a64(std::string_view s)
    {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : s)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    AssetId MakeAssetId(std::string_view path)
    {
        return AssetId{Fnv1a64(path)};
    }
}
