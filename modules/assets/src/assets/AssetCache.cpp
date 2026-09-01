#include "assets/AssetCache.h"

#include "core/log/Log.h"

#include <fstream>
#include <optional>

namespace mts
{
    namespace
    {
        std::optional<std::vector<std::byte>> ReadFileBytes(const std::filesystem::path &path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
            {
                MTS_LOG_ERROR("AssetCache: cannot open {}", path.string());
                return std::nullopt;
            }

            const std::streamsize size = file.tellg();
            if (size < 0)
            {
                MTS_LOG_ERROR("AssetCache: cannot determine size of {}", path.string());
                return std::nullopt;
            }
            file.seekg(0);

            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            if (!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), size))
            {
                MTS_LOG_ERROR("AssetCache: short read on {}", path.string());
                return std::nullopt;
            }

            return bytes;
        }
    }

    AssetCache::AssetCache(const AssetManifest *manifest, std::filesystem::path cookedRoot)
        : mManifest(manifest), mCookedRoot(std::move(cookedRoot))
    {
    }

    const AssetBlobView *AssetCache::Load(AssetId id)
    {
        if (const AssetBlobView *cached = Get(id))
            return cached;

        const AssetManifestEntry *entry = mManifest->Find(id);
        if (entry == nullptr)
        {
            MTS_LOG_ERROR("AssetCache::Load: no manifest entry for asset {:#x}", id.value);
            return nullptr;
        }

        const std::filesystem::path path = mCookedRoot / mManifest->PathOf(*entry);
        std::optional<std::vector<std::byte>> raw = ReadFileBytes(path);
        if (!raw.has_value())
            return nullptr;

        const std::optional<AssetBlobView> view = ParseAssetBlob(*raw);
        if (!view.has_value())
            return nullptr;

        if (view->header.typeTag != entry->typeTag || view->header.contentVersion != entry->contentVersion)
        {
            MTS_LOG_ERROR("AssetCache::Load: {} does not match its manifest entry", path.string());
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
