#pragma once

#include "assets/AssetBlob.h"
#include "assets/AssetId.h"
#include "assets/AssetManifest.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mts
{
    struct AssetCacheEntry
    {
        std::vector<std::byte> raw;
        AssetBlobView view;
    };

    class AssetCache
    {
    public:
        AssetCache(const AssetManifest *manifest, std::filesystem::path cookedRoot);

        const AssetBlobView *Load(AssetId id);
        const AssetBlobView *Get(AssetId id) const;

    private:
        const AssetManifest *mManifest;
        std::filesystem::path mCookedRoot;
        std::unordered_map<uint64_t, AssetCacheEntry> mLoaded;
        std::unordered_set<uint64_t> mFailedLoads;
    };
}
