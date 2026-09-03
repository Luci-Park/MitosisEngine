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
    /**
     * Signature bits available to the whole process, C++ and script-declared
     * components together.
     *
     * 256 rather than 128 because component types are no longer a closed set
     * the author can count: a script may declare one.
     * The budget is charged per *distinct name* for the life of the process,
     * not per registration, so reloading a script costs nothing - see
     * ComponentRegistry::SeqForHash.
     *
     * Cost of the raise is a 32-byte Signature instead of 16: two more words
     * hashed per archetype map lookup and two more ANDs per match test.
     */
    inline constexpr std::size_t kMaxComponentTypes = 256;

    // archetype's map key made up of types of components it holds
    using Signature = std::bitset<kMaxComponentTypes>;

    /**
     * Bit index for an already-allocated component seq.
     *
     * MTS_CHECK, not MTS_ASSERT, even though this sits on every add and remove.
     * ComponentRegistry catches the overflow it allocates, but a C++ component
     * that is never registered draws its seq straight from `TypeIdOf<T>`, which
     * has no check at all - and now shares the counter with script-declared
     * types, so the ceiling is far more reachable than it was. Left as a debug
     * assert, the release build would instead reach `std::bitset::set` and throw
     * `std::out_of_range` out of `World::AddComponent`, in a codebase that does
     * not use exceptions. The cost is one compare against a constant, next to an
     * archetype move.
     */
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
        /**
         * Which component seqs belong to sparse-stored types.
         *
         * The erased `World::*Raw` API takes a TypeId and cannot recover
         * `ComponentStorageInfo<T>` from it, so without this it would happily
         * give a sparse component a table column - a second copy that
         * `World::Has<T>`, `Query<T>` and `RemoveComponent<T>` all keep missing,
         * because they only ever consult the sparse store. `HasRaw` cannot catch
         * it either: it tests the archetype signature, and a sparse component
         * never owns a signature bit, so it answers false every time.
         *
         * Process-wide for the same reason the registry is (0022): seq is
         * global, so this is too. Populated by ComponentRegistry at
         * registration, which is where storage kind and seq are both in hand -
         * and the only legitimate source of a TypeId for a caller that has no T.
         */
        inline Signature &SparseSeqMask()
        {
            static Signature mask;
            return mask;
        }
    }

    inline void NoteSparseComponentSeq(uint32_t seq) { detail::SparseSeqMask().set(ComponentBitOf(seq)); }

    inline bool IsSparseComponentSeq(uint32_t seq)
    {
        return seq < kMaxComponentTypes && detail::SparseSeqMask().test(seq);
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
