#include "assets/AssetCache.h"

#include "assets/AssetFileIo.h"
#include "core/log/Log.h"

#include <optional>

namespace mts
{
    AssetCache::AssetCache(const AssetManifest *manifest, std::filesystem::path cookedRoot)
        : mManifest(manifest), mCookedRoot(std::move(cookedRoot))
    {
    }

    const AssetBlobView *AssetCache::Load(AssetId id)
    {
        if (const AssetBlobView *cached = Get(id))
            return cached;

        if (mFailedLoads.contains(id.value))
            return nullptr;

        const AssetManifestEntry *entry = mManifest->Find(id);
        if (entry == nullptr)
        {
            MTS_LOG_ERROR("AssetCache::Load: no manifest entry for asset {:#x}", id.value);
            mFailedLoads.insert(id.value);
            return nullptr;
        }

        const std::filesystem::path path = mCookedRoot / mManifest->PathOf(*entry);
        std::optional<std::vector<std::byte>> raw = ReadFileBytes(path);
        if (!raw.has_value())
        {
            mFailedLoads.insert(id.value);
            return nullptr;
        }

        const std::optional<AssetBlobView> view = ParseAssetBlob(*raw);
        if (!view.has_value())
        {
            mFailedLoads.insert(id.value);
            return nullptr;
        }

        if (view->header.typeTag != entry->typeTag || view->header.contentVersion != entry->contentVersion)
        {
            MTS_LOG_ERROR("AssetCache::Load: {} does not match its manifest entry", path.string());
            mFailedLoads.insert(id.value);
            return nullptr;
        }

        AssetCacheEntry cacheEntry{std::move(*raw), *view};
        return &mLoaded.emplace(id.value, std::move(cacheEntry)).first->second.view;
    }

    const AssetBlobView *AssetCache::Get(AssetId id) const
    {
        auto it = mLoaded.find(id.value);
        if (it == mLoaded.end())
            return nullptr;
        return &it->second.view;
    }
}
