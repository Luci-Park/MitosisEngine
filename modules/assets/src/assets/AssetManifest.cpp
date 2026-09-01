#include "assets/AssetManifest.h"

#include "assets/AssetBlob.h"
#include "core/log/Log.h"

#include <cstring>
#include <fstream>

namespace mts
{
    std::vector<std::byte> BuildAssetManifestBlob(std::span<const AssetManifestSourceEntry> entries)
    {
        std::vector<AssetManifestEntry> flatEntries;
        flatEntries.reserve(entries.size());
        std::string pathTable;

        for (const AssetManifestSourceEntry &src : entries)
        {
            AssetManifestEntry entry{};
            entry.id = src.id;
            entry.typeTag = src.typeTag;
            entry.contentVersion = src.contentVersion;
            entry.pathOffset = static_cast<uint32_t>(pathTable.size());
            entry.pathLength = static_cast<uint32_t>(src.path.size());
            pathTable.append(src.path);
            flatEntries.push_back(entry);
        }

        const uint64_t entryCount = flatEntries.size();
        const std::size_t entriesBytes = flatEntries.size() * sizeof(AssetManifestEntry);

        std::vector<std::byte> content(sizeof(entryCount) + entriesBytes + pathTable.size());
        std::size_t offset = 0;

        std::memcpy(content.data() + offset, &entryCount, sizeof(entryCount));
        offset += sizeof(entryCount);

        if (!flatEntries.empty())
        {
            std::memcpy(content.data() + offset, flatEntries.data(), entriesBytes);
            offset += entriesBytes;
        }

        if (!pathTable.empty())
            std::memcpy(content.data() + offset, pathTable.data(), pathTable.size());

        return BuildAssetBlob(kAssetManifestTypeTag, kAssetManifestContentVersion, content);
    }

    std::optional<AssetManifest> AssetManifest::Parse(std::span<const std::byte> raw)
    {
        const std::optional<AssetBlobView> blob = ParseAssetBlob(raw);
        if (!blob.has_value())
            return std::nullopt;

        if (blob->header.typeTag != kAssetManifestTypeTag)
        {
            MTS_LOG_ERROR("AssetManifest::Parse: type tag mismatch, expected {}, got {}",
                          kAssetManifestTypeTag, blob->header.typeTag);
            return std::nullopt;
        }

        const std::span<const std::byte> content = blob->content;
        if (content.size() < sizeof(uint64_t))
        {
            MTS_LOG_ERROR("AssetManifest::Parse: content too small for entry count");
            return std::nullopt;
        }

        uint64_t entryCount = 0;
        std::memcpy(&entryCount, content.data(), sizeof(entryCount));

        const std::size_t entriesBytes = static_cast<std::size_t>(entryCount) * sizeof(AssetManifestEntry);
        if (content.size() < sizeof(entryCount) + entriesBytes)
        {
            MTS_LOG_ERROR("AssetManifest::Parse: content too small for {} entries", entryCount);
            return std::nullopt;
        }

        AssetManifest manifest;
        manifest.mEntries.resize(entryCount);
        if (entryCount > 0)
            std::memcpy(manifest.mEntries.data(), content.data() + sizeof(entryCount), entriesBytes);

        const std::span<const std::byte> pathBytes = content.subspan(sizeof(entryCount) + entriesBytes);
        manifest.mPathTable.assign(reinterpret_cast<const char *>(pathBytes.data()), pathBytes.size());

        manifest.mIndex.reserve(manifest.mEntries.size());
        for (uint32_t i = 0; i < manifest.mEntries.size(); ++i)
        {
            const AssetManifestEntry &entry = manifest.mEntries[i];
            if (static_cast<std::size_t>(entry.pathOffset) + entry.pathLength > manifest.mPathTable.size())
            {
                MTS_LOG_ERROR("AssetManifest::Parse: entry {} path range out of bounds", i);
                return std::nullopt;
            }
            manifest.mIndex.emplace(entry.id.value, i);
        }

        return manifest;
    }

    std::optional<AssetManifest> AssetManifest::LoadFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            MTS_LOG_ERROR("AssetManifest::LoadFile: cannot open {}", path.string());
            return std::nullopt;
        }

        const std::streamsize size = file.tellg();
        if (size < 0)
        {
            MTS_LOG_ERROR("AssetManifest::LoadFile: cannot determine size of {}", path.string());
            return std::nullopt;
        }
        file.seekg(0);

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        if (!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), size))
        {
            MTS_LOG_ERROR("AssetManifest::LoadFile: short read on {}", path.string());
            return std::nullopt;
        }

        return Parse(bytes);
    }

    const AssetManifestEntry *AssetManifest::Find(AssetId id) const
    {
        auto it = mIndex.find(id.value);
        if (it == mIndex.end())
            return nullptr;
        return &mEntries[it->second];
    }

    std::string_view AssetManifest::PathOf(const AssetManifestEntry &entry) const
    {
        return std::string_view(mPathTable).substr(entry.pathOffset, entry.pathLength);
    }
}
