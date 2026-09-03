/**
 * @file ComponentRegistry.h
 * @author Sumin Park
 * @brief Name -> component operations, for callers that have no C++ type
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "CommandBuffer.h"
#include "ComponentAsserts.h"
#include "ComponentFields.h"
#include "Entity.h"
#include "Signature.h"
#include "StorageInfo.h"
#include "TypeId.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace mts
{
    /**
     * Everything the engine can do to one component type, with the type erased.
     *
     * Each function pointer is instantiated for a concrete T at registration
     * (or bound to the erased World::*Raw path for a script-declared type), so
     * the templates stay the only code that knows T and a scripting module can
     * do its whole job without naming a single component.
     *
     * Every operation is **total**: a dead entity, a missing component or a
     * duplicate add is answered rather than asserted on. That is deliberate and
     * it is where this differs from World's typed API. Engine code touching a
     * destroyed entity is a bug worth stopping for; a script holding a handle
     * across the frame that destroyed it is ordinary, and an assert would turn
     * routine gameplay into a debug-build stop.
     *
     * The `const ComponentOps &` first parameter is what lets one signature
     * serve both kinds: a native thunk ignores it, while a runtime thunk reads
     * the TypeId, size and alignment it needs out of it. A plain function
     * pointer has nowhere else to carry them.
     */
    struct ComponentOps
    {
        TypeId mType{};
        uint32_t mSize = 0;
        uint32_t mAlign = 0;
        StorageKind mStorage = StorageKind::Table;
        bool mRuntime = false; ///< declared by data rather than by C++

        void *(*mGet)(const ComponentOps &, World &, Entity) = nullptr;
        bool (*mHas)(const ComponentOps &, const World &, Entity) = nullptr;
        void (*mAddCopy)(const ComponentOps &, World &, Entity, const void *) = nullptr;
        void (*mRemove)(const ComponentOps &, World &, Entity) = nullptr;
        void (*mDeferAdd)(const ComponentOps &, CommandBuffer &, Entity, const void *) = nullptr;
        void (*mDeferRemove)(const ComponentOps &, CommandBuffer &, Entity) = nullptr;

        std::span<const FieldDesc> mFields;

        /// Bytes of a freshly default-constructed value, owned by the registry.
        /// `T{}` for a native type, all-zero for a script-declared one.
        std::span<const std::byte> mDefaultValue;

        void *Get(World &world, Entity entity) const { return mGet(*this, world, entity); }

        bool Has(const World &world, Entity entity) const { return mHas(*this, world, entity); }
        void AddCopy(World &world, Entity entity, const void *value) const { mAddCopy(*this, world, entity, value); }
        void AddDefault(World &world, Entity entity) const { AddCopy(world, entity, mDefaultValue.data()); }
        void Remove(World &world, Entity entity) const { mRemove(*this, world, entity); }

        void DeferAdd(CommandBuffer &commands, Entity entity, const void *value) const
        {
            mDeferAdd(*this, commands, entity, value);
        }

        void DeferAddDefault(CommandBuffer &commands, Entity entity) const
        {
            DeferAdd(commands, entity, mDefaultValue.data());
        }

        void DeferRemove(CommandBuffer &commands, Entity entity) const { mDeferRemove(*this, commands, entity); }

        /// Null when there is no such field. Linear: components have few fields.
        const FieldDesc *FindField(std::string_view name) const
        {
            for (const FieldDesc &field : mFields)
            {
                if (field.mName == name)
                    return &field;
            }
            return nullptr;
        }
    };

    /// One field of a component a script is declaring. Offsets are computed by
    /// the registry, so a script never states a layout.
    struct RuntimeFieldDecl
    {
        std::string_view mName;
        FieldKind mKind = FieldKind::Float;
    };

    /**
     * The process-wide map from component name to ComponentOps.
     *
     * **Process-wide, not per world.** `TypeId::seq` already comes from a global
     * counter, and the hot paths that use it - ComponentBit, SparseStorageFor,
     * Archetype::FindColumn - read it out of a function-local static. A
     * per-world registry would turn every one of those into a lookup, and would
     * make that static caching outright wrong, because the tests construct many
     * Worlds in one process. Ops take a `World &`, so nothing here is per-world
     * anyway.
     *
     * **A name keeps its seq for the life of the process.** Registering a name
     * that is already known returns the existing entry rather than allocating a
     * second bit. That is what makes script hot-reload free: the budget is
     * charged per distinct name ever seen, not per registration, so reloading a
     * script a hundred times costs one bit and the archetypes already built out
     * of it keep meaning what they meant. The alternative - allocate fresh and
     * reclaim with a free list - needs every archetype holding the bit torn
     * down and every cached query invalidated first, and a query that survived
     * with a recycled bit would silently match the wrong tables.
     *
     * **Register native components before loading any script.** A name is
     * claimed by whoever registers it first, and the two paths allocate their
     * seq differently (`TypeIdOf<T>` for native, `SeqForHash` here for
     * runtime). Registering a native component whose name a script already took
     * is refused rather than papered over.
     *
     * Not thread safe. Registration is a boot-time and script-load-time
     * activity; lookups after that are const and may be shared.
     */
    class ComponentRegistry
    {
    public:
        static ComponentRegistry &Instance();

        /// Registers a C++ component. Idempotent: the second call with the same
        /// T returns the first result, so an installer may be called from every
        /// entry point without tracking whether it already ran.
        template <typename T>
        const ComponentOps &Register(std::span<const FieldDesc> fields = {})
        {
            MTS_ASSERT_COMPONENT(T);
            static_assert(std::is_default_constructible_v<T>,
                          "ComponentRegistry::Register: T must be default constructible - the registry "
                          "captures a default value so a script can add the component without supplying one");

            ComponentOps ops{};
            ops.mType = TypeIdOf<T>();
            ops.mSize = sizeof(T);
            ops.mAlign = alignof(T);
            ops.mStorage = ComponentStorageInfo<T>::kValue;
            ops.mRuntime = false;
            ops.mFields = fields;

            ops.mGet = [](const ComponentOps &, World &world, Entity entity) -> void *
            { return world.IsAlive(entity) ? world.Get<T>(entity) : nullptr; };

            ops.mHas = [](const ComponentOps &, const World &world, Entity entity)
            { return world.IsAlive(entity) && world.Has<T>(entity); };

            ops.mAddCopy = [](const ComponentOps &, World &world, Entity entity, const void *value)
            {
                if (!world.IsAlive(entity))
                    return;

                // overwrite rather than assert on a duplicate, matching
                // CommandBuffer::ApplyAdd - two callers adding the same
                // component in one frame is legitimate
                if (T *existing = world.Get<T>(entity))
                    *existing = *static_cast<const T *>(value);
                else
                    world.AddComponent<T>(entity, *static_cast<const T *>(value));
            };

            ops.mRemove = [](const ComponentOps &, World &world, Entity entity)
            {
                if (world.IsAlive(entity) && world.Has<T>(entity))
                    world.RemoveComponent<T>(entity);
            };

            ops.mDeferAdd = [](const ComponentOps &, CommandBuffer &commands, Entity entity, const void *value)
            { commands.Add<T>(entity, *static_cast<const T *>(value)); };

            ops.mDeferRemove = [](const ComponentOps &, CommandBuffer &commands, Entity entity)
            { commands.Remove<T>(entity); };

            const T defaultValue{};
            return InsertNative(ops, &defaultValue);
        }

        /**
         * Declares a component from data: the registry lays the fields out,
         * owns the name, and binds the erased World::*Raw operations.
         *
         * Re-declaring a name with an identical field list is the hot-reload
         * case and succeeds, returning the existing entry. Re-declaring it with
         * a *different* field list is refused: live archetypes hold rows of the
         * old size, and migrating them is not implemented.
         *
         * Table storage only. A sparse store is `std::vector<T>` and cannot be
         * built without T, and sparse is an optimisation for churny components
         * rather than anything scripting needs.
         */
        const ComponentOps &RegisterRuntime(std::string_view name, std::span<const RuntimeFieldDecl> fields);

        const ComponentOps *Find(std::string_view name) const;
        const ComponentOps *FindByHash(uint32_t hash) const;

        /// For a TypeId recovered from an archetype column or a signature bit.
        const ComponentOps *FindBySeq(uint32_t seq) const;

        std::size_t Count() const { return mOps.size(); }

    private:
        const ComponentOps &InsertNative(const ComponentOps &ops, const void *defaultValue);

        /// The existing seq for this name, or a freshly allocated one. The
        /// MTS_CHECK is where the component budget is actually enforced: the
        /// name can come from a data file, so exceeding it must stop a release
        /// build with a message rather than corrupt the signature bitset.
        uint32_t SeqForHash(uint32_t hash, std::string_view name);

        std::string_view Intern(std::string_view name);

        /// The entry for `hash`, after checking that it really is `name` and
        /// not a different name that hashes the same.
        ComponentOps *FindChecked(uint32_t hash, std::string_view name);

        // deque, not vector: Find hands out `const ComponentOps *` that callers
        // hold across later registrations, and TypeId::name points into
        // mInternedNames. A vector would reallocate and dangle both. A
        // std::string's own buffer moves with the object when it is short
        // enough for SSO, so interning into a vector<string> is not safe either.
        std::deque<ComponentOps> mOps;
        std::deque<std::string> mInternedNames;
        std::deque<std::vector<FieldDesc>> mRuntimeFields;
        std::deque<std::vector<std::byte>> mDefaultValues;

        std::unordered_map<uint32_t, ComponentOps *> mByHash;
        std::unordered_map<uint32_t, ComponentOps *> mBySeq;
    };

    /// Registers every component `core` defines, with its field tables.
    /// Idempotent; call it from the composition root before loading scripts.
    void RegisterCoreComponents();
}
