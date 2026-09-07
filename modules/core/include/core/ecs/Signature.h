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
#include <type_traits>

namespace mts
{
    // because we use bitset, we need limited number of componentTypes.
    inline constexpr std::size_t kMaxComponentTypes = 256;
    using Signature = std::bitset<kMaxComponentTypes>;

    // check seq -> bitset idx (< kMaxComponentTypes)
    inline std::size_t ComponentBitOf(uint32_t seq)
    {
        MTS_CHECK(seq < kMaxComponentTypes,
                  "ComponentBitOf: component type {} is past kMaxComponentTypes ({}). Raise "
                  "kMaxComponentTypes in Signature.h, or declare fewer component types.",
                  seq, kMaxComponentTypes);
        return seq;
    }

    inline std::size_t ComponentBitOf(TypeId type) { return ComponentBitOf(type.seq); }

    template <typename T>
    std::size_t ComponentBit()
    {
        return ComponentBitOf(TypeIdOf<T>().seq);
    }

    // Signature of multiple combinations of components
    // const is stripped
    template <typename... Ts>
    Signature SignatureOf()
    {
        Signature signature;
        (signature.set(ComponentBit<std::remove_const_t<Ts>>()), ...);
        return signature;
    }
}
