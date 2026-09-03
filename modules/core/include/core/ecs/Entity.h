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

    /**
     * The one encoding for handing an Entity to a scripting VM or a save file.
     *
     * Both halves matter: mGeneration is the ABA guard, so an encoding that
     * drops it turns a stale handle into one that reports itself alive. That is
     * the failure mode to watch for on the binding side - a VM whose only
     * numeric type is a double carries 53 significant bits and would silently
     * truncate the generation. Pass this through an integer slot (userdata, a
     * boxed value, Lua 5.3+ integers), never a floating-point one.
     */
    constexpr uint64_t PackEntity(Entity entity)
    {
        return (static_cast<uint64_t>(entity.mGeneration) << 32) | static_cast<uint64_t>(entity.mIndex);
    }

    constexpr Entity UnpackEntity(uint64_t packed)
    {
        return Entity{static_cast<uint32_t>(packed), static_cast<uint32_t>(packed >> 32)};
    }
}
