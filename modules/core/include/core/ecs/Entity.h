/**
 * @file Entity.h
 * @author sumin.park
 * @brief Entity handle.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstdint>

namespace mts
{
    struct Entity
    {
        static constexpr uint32_t kNullIndex = UINT32_MAX; // never allocated

        uint32_t mIndex = kNullIndex; // slot index
        uint32_t mGeneration = 0;     // ABA guard, incremented in Free()

        constexpr bool IsNull() const { return mIndex == kNullIndex; }
        constexpr bool operator==(const Entity &) const = default;
    };

    inline constexpr Entity kNullEntity{};
}
