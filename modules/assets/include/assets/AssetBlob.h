#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace mts
{
    inline constexpr uint32_t kAssetBlobMagic = 0x4D545341u;
    inline constexpr uint32_t kAssetBlobFormatVersion = 1u;

    inline constexpr uint32_t kRawAssetTypeTag = 0x52415721u;
    inline constexpr uint32_t kRawAssetContentVersion = 1u;

    struct AssetBlobHeader
    {
        uint32_t magic = kAssetBlobMagic;
        uint32_t formatVersion = kAssetBlobFormatVersion;
        uint32_t typeTag = 0;
        uint32_t contentVersion = 0;
        uint64_t contentSize = 0;
        uint64_t contentHash = 0;
    };

    static_assert(std::is_trivially_copyable_v<AssetBlobHeader>);
    static_assert(std::is_standard_layout_v<AssetBlobHeader>);

    struct AssetBlobView
    {
        AssetBlobHeader header;
        std::span<const std::byte> content;
    };

    std::vector<std::byte> BuildAssetBlob(uint32_t typeTag, uint32_t contentVersion,
                                           std::span<const std::byte> content);

    std::optional<AssetBlobView> ParseAssetBlob(std::span<const std::byte> raw);
}
