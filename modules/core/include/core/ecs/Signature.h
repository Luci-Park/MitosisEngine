/**
 * @file Signature.h
 * @author Sumin Park
 * @brief Component-set identity for archetypes
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "TypeId.h"
#include "core/log/Assert.h"

#include <bitset>
#include <cstddef>

namespace engine
{
    inline constexpr std::size_t kMaxComponentTypes = 128;

    // archetype's map key made up of types of components it holds
    using Signature = std::bitset<kMaxComponentTypes>;

    template <typename T>
    std::size_t ComponentBit()
    {
        const uint32_t seq = TypeIdOf<T>().seq;
        MTS_ASSERT(seq < kMaxComponentTypes,
                   "ComponentBit: component type count exceeded kMaxComponentTypes ({}); raise it", kMaxComponentTypes);
        return seq;
    }

    // for querying multiple components
    template <typename... Ts>
    Signature SignatureOf()
    {
        Signature signature;
        (signature.set(ComponentBit<Ts>()), ...);
        return signature;
    }
}
