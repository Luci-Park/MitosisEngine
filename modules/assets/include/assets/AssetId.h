#pragma once

#include <cstdint>
#include <string_view>

namespace mts
{
    struct AssetId
    {
        uint64_t value = 0;

        constexpr bool IsNull() const { return value == 0; }
        constexpr bool operator==(const AssetId &) const = default;
    };

    inline constexpr AssetId kNullAssetId{};

    uint64_t Fnv1a64(std::string_view s);

    AssetId MakeAssetId(std::string_view path);
}
