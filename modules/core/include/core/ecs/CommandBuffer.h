/**
 * @file CommandBuffer.h
 * @author Sumin Park
 * @brief Deferred structural changes, applied at a phase boundary
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "ComponentAsserts.h"
#include "Entity.h"
#include "World.h"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace mts
{
    /**
     * Records structural changes while a Query walk is in flight, and applies
     * them once the walk is over.
     *
     * Why this exists: AddComponent grows a ComponentColumn's byte vector, which
     * reallocates. Every T& a ForEach callback holds points into that vector, so
     * an immediate add during iteration dangles them - and it does so silently
     * whenever the target archetype already exists, because Query's generation
     * assert only fires when a *new* archetype is created.
     *
     * CreateEntity stays immediate and is not routed through here: a new entity
     * lands in the empty archetype, which has no columns to reallocate and which
     * no query can match. The handle is usable at once; only the component data
     * waits for the flush.
     */
    class CommandBuffer
    {
    public:
        /// Records a copy of @p value. Visible after the next Flush.
        /// Last writer wins if two commands add the same component to one entity.
        template <typename T>
        void Add(Entity entity, const T &value)
        {
            MTS_ASSERT_COMPONENT(T);

            // mStorage's base comes from plain operator new (std::byte has
            // alignment 1, so the align_val_t overload is never selected), which
            // guarantees exactly this much. Rounding inside the buffer cannot
            // recover more than the base has.
            static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                          "CommandBuffer: over-aligned component - the payload buffer only "
                          "guarantees default new alignment");

            // different component types interleave in one buffer, so the running
            // size is not a valid offset for T on its own
            const std::size_t offset = AlignUp(mStorage.size(), alignof(T));
            mStorage.resize(offset + sizeof(T));
            std::memcpy(mStorage.data() + offset, &value, sizeof(T));

            // offset, not pointer: the next Add resizes mStorage and would
            // dangle a stored pointer
            mCommands.push_back(Command{&ApplyAdd<T>, entity, offset});
        }

        template <typename T>
        void Remove(Entity entity)
        {
            mCommands.push_back(Command{&ApplyRemove<T>, entity, kNoPayload});
        }

        void Destroy(Entity entity)
        {
            mCommands.push_back(Command{&ApplyDestroy, entity, kNoPayload});
        }

        bool Empty() const { return mCommands.empty(); }
        std::size_t Size() const { return mCommands.size(); }

        /// Applies every recorded command in order, then clears.
        /// Called by SystemScheduler at a phase boundary - never mid-ForEach.
        void Flush(World &world)
        {
            for (const Command &command : mCommands)
            {
                // data() may be null while the buffer holds only payload-free
                // commands; the thunks ignore the argument in that case anyway
                void *payload = mStorage.empty() ? nullptr : mStorage.data() + command.payload;
                command.apply(world, command.entity, payload);
            }

            mCommands.clear();
            mStorage.clear(); // capacity is kept: steady-state frames stop allocating
        }

    private:
        static constexpr std::size_t kNoPayload = 0; // unread by the payload-free thunks

        // alignment is always a power of two, so the mask form is exact
        static constexpr std::size_t AlignUp(std::size_t n, std::size_t alignment)
        {
            return (n + alignment - 1) & ~(alignment - 1);
        }

        // One instantiation per component type. Its *address* is the erased type
        // handle, so T is recovered at flush with no RTTI and no virtual call.
        template <typename T>
        static void ApplyAdd(World &world, Entity entity, void *payload)
        {
            if (!world.IsAlive(entity))
                return; // destroyed by an earlier command in this same flush

            const T *value = static_cast<const T *>(payload);

            // World::AddComponent asserts on a duplicate. Two systems each
            // deferring an add to the same entity in one phase is legitimate, and
            // asserting from inside the flush loses the callsite, so overwrite.
            if (T *existing = world.Get<T>(entity))
                *existing = *value;
            else
                world.AddComponent<T>(entity, *value);
        }

        template <typename T>
        static void ApplyRemove(World &world, Entity entity, void *)
        {
            if (world.IsAlive(entity) && world.Has<T>(entity))
                world.RemoveComponent<T>(entity);
        }

        static void ApplyDestroy(World &world, Entity entity, void *)
        {
            if (world.IsAlive(entity))
                world.DestroyEntity(entity);
        }

        struct Command
        {
            void (*apply)(World &, Entity, void *);
            Entity entity;
            std::size_t payload; // byte offset into mStorage
        };

        std::vector<Command> mCommands;
        std::vector<std::byte> mStorage;
    };
}
