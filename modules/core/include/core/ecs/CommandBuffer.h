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
#include "TypeId.h"
#include "World.h"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace mts
{
    class CommandBuffer
    {
    public:
        // Records a copy of @p value. Visible after the next Flush.
        // Last writer wins if two commands add the same component to one entity.
        template <typename T>
        void Add(Entity entity, const T &value)
        {
            MTS_ASSERT_COMPONENT(T);
            static_assert(!kIsTagComponent<T>,
                          "CommandBuffer::Add: T is a tag - there is no value to record. Use AddTag<T>.");

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

        // Records a tag add. Payload-free, like Remove: a tag's whole value is
        // that it is there.
        template <typename T>
        void AddTag(Entity entity)
        {
            MTS_ASSERT_COMPONENT(T);
            static_assert(kIsTagComponent<T>,
                          "CommandBuffer::AddTag: T has fields, so it needs a value. Use Add<T>.");

            mCommands.push_back(Command{&ApplyAddTag<T>, entity, kNoPayload});
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

        /**
         * The erased twin of Add, for a component whose C++ type the caller
         * does not have - a script-declared one, or one named by TypeId at
         * runtime. Table storage only, same restriction as World::AddRaw.
         *
         * The type, size and alignment travel in a header written into the same
         * payload buffer, immediately before the value. They cannot live on
         * Command: TypeId carries a string_view, so folding it in would grow
         * every command - including the payload-free ones - by 24 bytes to
         * serve the rare case.
         */
        void AddRaw(Entity entity, TypeId type, uint32_t size, uint32_t align, const void *value)
        {
            MTS_ASSERT(align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                       "CommandBuffer::AddRaw: over-aligned component \"{}\" ({})", type.name, align);

            const std::size_t headerOffset = AlignUp(mStorage.size(), alignof(RawHeader));
            const std::size_t valueOffset = AlignUp(headerOffset + sizeof(RawHeader), align == 0 ? 1 : align);
            mStorage.resize(valueOffset + size);

            // written after every resize: an offset survives reallocation, a
            // pointer taken before it would not
            const RawHeader header{type, size, align, static_cast<uint32_t>(valueOffset - headerOffset)};
            std::memcpy(mStorage.data() + headerOffset, &header, sizeof(header));
            if (size != 0)
                std::memcpy(mStorage.data() + valueOffset, value, size);

            mCommands.push_back(Command{&ApplyAddRaw, entity, headerOffset});
        }

        void RemoveRaw(Entity entity, TypeId type)
        {
            const std::size_t headerOffset = AlignUp(mStorage.size(), alignof(RawHeader));
            mStorage.resize(headerOffset + sizeof(RawHeader));

            const RawHeader header{type, 0, 0, 0};
            std::memcpy(mStorage.data() + headerOffset, &header, sizeof(header));

            mCommands.push_back(Command{&ApplyRemoveRaw, entity, headerOffset});
        }

        bool Empty() const { return mCommands.empty(); }
        std::size_t Size() const { return mCommands.size(); }

        // Applies every recorded command in order, then clears.
        // Called by SystemScheduler at a phase boundary - never mid-ForEach.
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

        // Trivially copyable so it can be memcpy'd in and out of the byte
        // buffer; TypeId::name points at static or registry-interned storage,
        // which outlives the flush.
        struct RawHeader
        {
            TypeId type;
            uint32_t size;
            uint32_t align;
            uint32_t valueOffset; // bytes from this header to the value
        };

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
            if (T *existing = world.GetComponent<T>(entity))
                *existing = *value;
            else
                world.AddComponent<T>(entity, *value);
        }

        // Adding a tag twice in one flush is the same legitimate case ApplyAdd
        // absorbs, and there is no value to overwrite - so already-there is
        // simply done.
        template <typename T>
        static void ApplyAddTag(World &world, Entity entity, void *)
        {
            if (world.IsAlive(entity) && !world.Has<T>(entity))
                world.AddTag<T>(entity);
        }

        template <typename T>
        static void ApplyRemove(World &world, Entity entity, void *)
        {
            if (world.IsAlive(entity) && world.Has<T>(entity))
                world.RemoveComponent<T>(entity);
        }

        static void ApplyAddRaw(World &world, Entity entity, void *payload)
        {
            if (!world.IsAlive(entity))
                return;

            RawHeader header{};
            std::memcpy(&header, payload, sizeof(header));
            const void *value = static_cast<const std::byte *>(payload) + header.valueOffset;

            // overwrite rather than assert on a duplicate, for the same reason
            // ApplyAdd does.
            //
            // HasRaw, not a non-null GetRaw: a tag reads back as null whether
            // it is there or not, so testing the pointer would take the add
            // branch every time and assert on the duplicate.
            if (!world.HasRaw(entity, header.type))
                world.AddRaw(entity, header.type, header.size, header.align, value);
            else if (header.size != 0)
                std::memcpy(world.GetRaw(entity, header.type), value, header.size);
        }

        static void ApplyRemoveRaw(World &world, Entity entity, void *payload)
        {
            RawHeader header{};
            std::memcpy(&header, payload, sizeof(header));

            if (world.IsAlive(entity) && world.HasRaw(entity, header.type))
                world.RemoveRaw(entity, header.type);
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
