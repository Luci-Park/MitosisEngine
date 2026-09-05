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

    // Entity encode/decode for scripting, saving, etc
    // always save in ints, never floating-points
    constexpr uint64_t PackEntity(Entity entity)
    {
        return (static_cast<uint64_t>(entity.mGeneration) << 32) | static_cast<uint64_t>(entity.mIndex);
    }

    constexpr Entity UnpackEntity(uint64_t packed)
    {
        return Entity{static_cast<uint32_t>(packed), static_cast<uint32_t>(packed >> 32)};
    }
}
