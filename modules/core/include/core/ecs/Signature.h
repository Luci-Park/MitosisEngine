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

namespace mts
{
    // because we use bitset, we need limited number of componentTypes.
    inline constexpr std::size_t kMaxComponentTypes = 256;
    using Signature = std::bitset<kMaxComponentTypes>;

    // check seq < kMaxComponentTypes
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

    namespace detail
    {
        // mask of components in sparse set
        inline Signature &SparseSeqMask()
        {
            static Signature mask;
            return mask;
        }
    }

    // turns on bit of seq in SparseSeqMask
    inline void NoteSparseComponentSeq(uint32_t seq) { detail::SparseSeqMask().set(ComponentBitOf(seq)); }

    inline bool IsSparseComponentSeq(uint32_t seq)
    {
        return seq < kMaxComponentTypes && detail::SparseSeqMask().test(seq);
    }

    // Signature of multiple combinations of components
    template <typename... Ts>
    Signature SignatureOf()
    {
        Signature signature;
        (signature.set(ComponentBit<Ts>()), ...);
        return signature;
    }
}
