/**
 * @file World.h
 * @author Sumin Park
 * @brief Owns entities and their archetypes
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "Archetype.h"
#include "Entity.h"
#include "EntityPool.h"
#include "QueryTerms.h"
#include "Signature.h"
#include "SparseSetStorage.h"
#include "StorageInfo.h"
#include "core/log/Assert.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mts
{
    class World;

    namespace detail
    {
        class ArchetypeMatcher;
        struct QueryIterationGuard;
    }

    /**
     * Called just before an entity is torn down, while all of its components
     * are still readable. Registered with World::AddDestroyHook.
     *
     * A hook may destroy further entities - that is the point of it, and how
     * the scene hierarchy cascades - so it must tolerate being re-entered.
     * It must not add or remove hooks.
     */
    struct EntityDestroyHook
    {
        void (*fn)(World &world, Entity entity, void *user);
        void *user = nullptr;

        bool operator==(const EntityDestroyHook &) const = default;
    };

    // where an entity's components live: which table, and which row of it
    struct EntityRecord
    {
        Archetype *archetype = nullptr;
        uint32_t row = 0;
    };

    template <typename T>
    inline constexpr bool kIsSparseComponent = ComponentStorageInfo<T>::kValue == StorageKind::SparseSet;

    namespace detail
    {
        // Type-erased handle
        class ISparseStorage
        {
        public:
            virtual ~ISparseStorage() = default;
            virtual void RemoveIfPresent(Entity entity) = 0;
        };

        template <typename T>
        class SparseStorageHolder final : public ISparseStorage
        {
        public:
            void RemoveIfPresent(Entity entity) override
            {
                if (mStorage.Has(entity))
                    mStorage.Remove(entity);
            }

            SparseSetStorage<T> mStorage;
        };

        /**
         * Resources are keyed on their own counter rather than TypeIdOf.
         * ComponentBit uses TypeId::seq *directly* as a bitset index and
         * asserts it stays under kMaxComponentTypes, so every non-component
         * type that drew a seq would push real components toward that ceiling
         * - an order-dependent failure a long way from its cause.
         */
        inline uint32_t NextResourceId()
        {
            static std::atomic<uint32_t> counter{0};
            return counter.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * Normalized through remove_cvref_t so that TryResource<const T> names
         * the same resource as TryResource<T>. Without it the const spelling
         * is a separate instantiation with its own id, which compiles fine and
         * then always reports the resource as absent - a silent no-op in any
         * caller that treats nullptr as "no graph installed".
         */
        template <typename T>
        uint32_t ResourceIdOf()
        {
            static const uint32_t id = NextResourceId();
            return id;
        }

        template <typename T>
        uint32_t ResourceKeyOf()
        {
            return ResourceIdOf<std::remove_cvref_t<T>>();
        }

        // Type-erased owner, so World can destroy a resource it knows nothing
        // about. The virtual destructor is the whole point of the base.
        class IResource
        {
        public:
            virtual ~IResource() = default;
        };

        template <typename T>
        class ResourceHolder final : public IResource
        {
        public:
            template <typename... Args>
            explicit ResourceHolder(Args &&...args) : mValue(std::forward<Args>(args)...)
            {
            }

            T mValue;
        };

        // table bitmask
        template <typename... Ts>
        Signature TableSignatureOf()
        {
            Signature signature;

            // strip const to have clean bitmask
            ((kIsSparseComponent<Bare<Ts>> ? void() : void(signature.set(ComponentBit<Bare<Ts>>()))), ...);
            return signature;
        }
    }

    class World
    {
    public:
        World()
        {
            // every entity starts in a table with no columns
            mEmptyArchetype = &GetOrCreateArchetype(Signature{});
        }

        World(const World &) = delete;
        World &operator=(const World &) = delete;
        World(World &&) = delete;
        World &operator=(World &&) = delete;

        // Deliberately allowed during a query walk, unlike the mutators below.
        // The new entity lands in the empty archetype, so no matched table's
        // columns move; a sparse-only query does match that table, but its rows
        // are appended and Query::ForEach never walks past the count it started
        // with. Recording the component into a CommandBuffer is what defers.
        Entity CreateEntity()
        {
            const Entity entity = mPool.Create();

            if (entity.mIndex >= mRecords.size())
                mRecords.resize(entity.mIndex + 1);

            mRecords[entity.mIndex] = EntityRecord{mEmptyArchetype, mEmptyArchetype->AddRow(entity)};
            return entity;
        }

        /**
         * Registers a hook to run before each entity is torn down. Adding the
         * same fn+user twice is a no-op, so an installer can be called from
         * every entry point without tracking whether it already ran.
         *
         * World stays ignorant of what any hook means: this is how the scene
         * hierarchy makes destruction cascade without World learning what a
         * parent is.
         */
        void AddDestroyHook(void (*fn)(World &, Entity, void *), void *user = nullptr)
        {
            const EntityDestroyHook hook{fn, user};
            for (const EntityDestroyHook &existing : mDestroyHooks)
            {
                if (existing == hook)
                    return;
            }

            mDestroyHooks.push_back(hook);
        }

        // reset record, remove from both storages, remove from pool
        void DestroyEntity(Entity entity)
        {
            MTS_ASSERT(mPool.IsAlive(entity), "World::DestroyEntity: entity is not alive");
            AssertNoStructuralChange("DestroyEntity");

            // Hooks run first, while this entity's components are still
            // readable, and the record is read only afterwards. A hook may
            // destroy other entities, and a swap-remove in this archetype can
            // move this entity's row - a record captured before the hooks ran
            // would then name someone else's row.
            //
            // Indexed rather than ranged so the vector may not be reallocated
            // underneath the loop; hooks are documented not to add hooks.
            for (std::size_t i = 0; i < mDestroyHooks.size(); ++i)
            {
                // Re-checked every iteration, not just after the loop: an
                // earlier hook's cascade can reach this entity, and hooks are
                // promised an entity whose components are still readable.
                if (!mPool.IsAlive(entity))
                    return;

                mDestroyHooks[i].fn(*this, entity, mDestroyHooks[i].user);
            }

            // A cascade can reach this entity from another direction and
            // destroy it before we get here. Already gone is success.
            if (!mPool.IsAlive(entity))
                return;

            const EntityRecord record = mRecords[entity.mIndex];
            RemoveRow(*record.archetype, record.row);

            for (auto &[seq, storage] : mSparseStorages)
                storage->RemoveIfPresent(entity);

            mRecords[entity.mIndex] = EntityRecord{};
            mPool.Destroy(entity);
        }

        bool IsAlive(Entity entity) const { return mPool.IsAlive(entity); }

        /**
         * True while a Query is walking this world.
         *
         * Structural changes are forbidden in that window - see
         * AssertNoStructuralChange - so anything that mutates on behalf of a
         * caller it does not control (a script binding, an editor command)
         * should test this and record into a CommandBuffer instead.
         */
        bool IsIterating() const { return mQueryIterationDepth > 0; }

        // Get table of entity
        const Archetype *ArchetypeOf(Entity entity) const
        {
            MTS_ASSERT(mPool.IsAlive(entity), "World::ArchetypeOf: entity is not alive");
            return mRecords[entity.mIndex].archetype;
        }

        std::size_t ArchetypeCount() const { return mArchetypes.size(); }

        // bumped once per archetype creation
        // a query whose seen this generation needs no rescan
        std::size_t Generation() const { return mArchetypeGeneration; }

        template <typename T>
        bool Has(Entity entity) const
        {
            MTS_ASSERT(mPool.IsAlive(entity), "World::Has: entity is not alive");

            if constexpr (kIsSparseComponent<T>)
            {
                const SparseSetStorage<T> *storage = FindSparseStorage<T>();
                return storage != nullptr && storage->Has(entity);
            }
            else
            {
                return mRecords[entity.mIndex].archetype->GetSignature().test(ComponentBit<T>());
            }
        }

        template <typename T>
        T *Get(Entity entity)
        {
            if (!mPool.IsAlive(entity))
                return nullptr;

            if constexpr (kIsSparseComponent<T>)
            {
                SparseSetStorage<T> *storage = FindSparseStorage<T>();
                return storage ? storage->Get(entity) : nullptr;
            }
            else
            {
                const EntityRecord &record = mRecords[entity.mIndex];
                ComponentColumn *column = record.archetype->FindColumn(TypeIdOf<T>());
                return column ? static_cast<T *>(column->At(record.row)) : nullptr;
            }
        }

        template <typename T>
        const T *Get(Entity entity) const
        {
            if (!mPool.IsAlive(entity))
                return nullptr;

            if constexpr (kIsSparseComponent<T>)
            {
                const SparseSetStorage<T> *storage = FindSparseStorage<T>();
                return storage ? storage->Get(entity) : nullptr;
            }
            else
            {
                const EntityRecord &record = mRecords[entity.mIndex];
                const ComponentColumn *column = record.archetype->FindColumn(TypeIdOf<T>());
                return column ? static_cast<const T *>(column->At(record.row)) : nullptr;
            }
        }

        template <typename T>
        T &AddComponent(Entity entity, const T &value)
        {
            MTS_ASSERT_COMPONENT(T);
            MTS_ASSERT(mPool.IsAlive(entity), "World::AddComponent: entity is not alive");
            MTS_ASSERT(!Has<T>(entity), "World::AddComponent: entity already has this component");
            AssertNoStructuralChange("AddComponent");

            if constexpr (kIsSparseComponent<T>)
            {
                SparseSetStorage<T> &storage = SparseStorageFor<T>();
                storage.Add(entity, value);
                return *storage.Get(entity);
            }
            else
            {
                return AddTableComponent(entity, value);
            }
        }

        template <typename T>
        void RemoveComponent(Entity entity)
        {
            MTS_ASSERT(mPool.IsAlive(entity), "World::RemoveComponent: entity is not alive");
            MTS_ASSERT(Has<T>(entity), "World::RemoveComponent: entity does not have this component");
            AssertNoStructuralChange("RemoveComponent");

            if constexpr (kIsSparseComponent<T>)
                SparseStorageFor<T>().Remove(entity);
            else
                RemoveTableComponent<T>(entity);
        }

        // -- erased access -------------------------------------------------
        //
        // The same four operations without a C++ type, for callers that only
        // have a TypeId at runtime: a scripting binding, an editor inspector, a
        // deserializer. Table storage only - a sparse component's storage is
        // std::vector<T> and cannot be reached without T - so a caller that may
        // hold either kind should go through ComponentRegistry, which recorded
        // each type's StorageKind at registration and picks the right path.
        //
        // "Should" is not enough on a public API, so a sparse TypeId is refused
        // here rather than quietly mishandled. AddRaw would otherwise build a
        // table column shadowing the sparse store, and every typed reader would
        // go on seeing the old value; GetRaw and HasRaw would report the
        // component absent. IsSparseComponentSeq is what makes that detectable
        // from a TypeId alone - see Signature.h.

        /// The component's bytes, or nullptr when the entity is dead, or has
        /// no such component. Never asserts: unlike engine code, a script holds
        /// handles across frames and asking about a destroyed one is routine.
        void *GetRaw(Entity entity, TypeId type)
        {
            AssertNotSparse("GetRaw", type);

            if (!mPool.IsAlive(entity))
                return nullptr;

            const EntityRecord &record = mRecords[entity.mIndex];
            ComponentColumn *column = record.archetype->FindColumn(type);
            return column ? column->At(record.row) : nullptr;
        }

        const void *GetRaw(Entity entity, TypeId type) const
        {
            AssertNotSparse("GetRaw", type);

            if (!mPool.IsAlive(entity))
                return nullptr;

            const EntityRecord &record = mRecords[entity.mIndex];
            const ComponentColumn *column = record.archetype->FindColumn(type);
            return column ? column->At(record.row) : nullptr;
        }

        bool HasRaw(Entity entity, TypeId type) const
        {
            AssertNotSparse("HasRaw", type);
            return mPool.IsAlive(entity) && mRecords[entity.mIndex].archetype->GetSignature().test(ComponentBitOf(type));
        }

        /// Adds `type` to `entity` and copies `size` bytes from `value`.
        /// Returns the stored bytes. `size` and `align` must be the ones the
        /// type was registered with - a mismatch is a corrupt column, not a
        /// diagnosable error, so ComponentRegistry is the intended caller.
        void *AddRaw(Entity entity, TypeId type, uint32_t size, uint32_t align, const void *value)
        {
            // MTS_CHECK, unlike the read paths: this one corrupts rather than
            // misreports, and does so silently - the shadowed sparse value goes
            // on being returned to every typed reader.
            MTS_CHECK(!IsSparseComponentSeq(type.seq),
                      "World::AddRaw: \"{}\" is a sparse component. The erased path is table-only, and a "
                      "table row here would shadow the sparse one. Go through ComponentRegistry, which "
                      "knows the StorageKind.",
                      type.name);

            MTS_ASSERT(mPool.IsAlive(entity), "World::AddRaw: entity is not alive");
            MTS_ASSERT(!HasRaw(entity, type), "World::AddRaw: entity already has {}", type.name);
            AssertNoStructuralChange("AddRaw");

            return AddTableComponentRaw(entity, type, size, align, value);
        }

        void RemoveRaw(Entity entity, TypeId type)
        {
            MTS_CHECK(!IsSparseComponentSeq(type.seq),
                      "World::RemoveRaw: \"{}\" is a sparse component. The erased path is table-only. Go "
                      "through ComponentRegistry, which knows the StorageKind.",
                      type.name);

            MTS_ASSERT(mPool.IsAlive(entity), "World::RemoveRaw: entity is not alive");
            MTS_ASSERT(HasRaw(entity, type), "World::RemoveRaw: entity does not have {}", type.name);
            AssertNoStructuralChange("RemoveRaw");

            RemoveTableComponentRaw(entity, type);
        }

        /**
         * Installs this world's single instance of T, constructed in place, and
         * replaces any previous one.
         *
         * A resource is engine state that belongs to the world rather than to
         * an entity - a camera, an input snapshot, a hierarchy index, a script
         * VM. Unlike a component it is never relocated, so it carries none of
         * the trivially-copyable requirement: a resource may hold vectors,
         * strings, or anything else with a destructor.
         *
         * Replacing destroys the old value, which invalidates any pointer a
         * system cached from Resource() or TryResource(). Emplace during setup,
         * not mid-frame, unless every holder of that pointer is re-fetching.
         */
        template <typename T, typename... Args>
        T &EmplaceResource(Args &&...args)
        {
            static_assert(std::is_same_v<T, std::remove_cvref_t<T>>,
                          "World::EmplaceResource: T must be a plain value type, not a reference or cv-qualified");

            auto holder = std::make_unique<detail::ResourceHolder<T>>(std::forward<Args>(args)...);
            T &value = holder->mValue;


            // The value lives inside a heap-allocated holder, so rehashing the
            // map moves the unique_ptr and never the resource itself: a
            // pointer taken here survives any number of later emplacements of
            // *other* resources.
            mResources[detail::ResourceKeyOf<T>()] = std::move(holder);
            return value;
        }

        /// The resource, or nullptr when none has been emplaced.
        template <typename T>
        T *TryResource()
        {
            const auto it = mResources.find(detail::ResourceKeyOf<T>());
            if (it == mResources.end())
                return nullptr;

            using Bare = std::remove_cvref_t<T>;
            return &static_cast<detail::ResourceHolder<Bare> *>(it->second.get())->mValue;
        }

        template <typename T>
        const T *TryResource() const
        {
            const auto it = mResources.find(detail::ResourceKeyOf<T>());
            if (it == mResources.end())
                return nullptr;

            using Bare = std::remove_cvref_t<T>;
            return &static_cast<const detail::ResourceHolder<Bare> *>(it->second.get())->mValue;
        }

        /// The resource, which must exist. Use TryResource where absence is a
        /// case the caller handles rather than a bug.
        template <typename T>
        T &Resource()
        {
            T *value = TryResource<T>();

            // MTS_CHECK, not MTS_ASSERT: this returns a reference, so a missing
            // resource in a release build would be a null dereference rather
            // than a diagnosable stop.
            MTS_CHECK(value != nullptr, "World::Resource: no {} has been emplaced", TrimTypeName<T>());
            return *value;
        }

        template <typename T>
        const T &Resource() const
        {
            const T *value = TryResource<T>();
            MTS_CHECK(value != nullptr, "World::Resource: no {} has been emplaced", TrimTypeName<T>());
            return *value;
        }

        template <typename T>
        bool HasResource() const
        {
            return mResources.find(detail::ResourceKeyOf<T>()) != mResources.end();
        }

        /// Destroys the resource. True if there was one.
        template <typename T>
        bool RemoveResource()
        {
            return mResources.erase(detail::ResourceKeyOf<T>()) != 0;
        }

        // returns the world-owned query for this exact term + filter shape,
        // use queries to iterate over storages
        template <typename... Ts, typename... Filters>
        Query<Ts...> &GetOrCreateQuery(Filters... filters);

        // Use with GetOrCreateQuery<Ts...>().ForEach(cb); defined in Query.h.
        template <typename... Ts, typename Fn>
        void ForEach(Fn &&cb);

    protected:
        template <typename...>
        friend class Query;

        // The two helpers that reach past the public surface, and the reason
        // the archetype table is not simply public. ArchetypeMatcher is the
        // only walker of mArchetypes; QueryIterationGuard is the only way to
        // raise the iteration depth. A public accessor would let any caller
        // hold column pointers with no guard raised, which is exactly the
        // silent corruption AssertNoStructuralChange exists to catch - so the
        // walk stays inside types that pair the two by construction.
        friend class detail::ArchetypeMatcher;
        friend struct detail::QueryIterationGuard;

        const std::unordered_map<Signature, std::unique_ptr<Archetype>> &Archetypes() const { return mArchetypes; }

        // Depth, not a flag: a query may legitimately be re-entered from its own
        // callback (a pairwise scan), and the inner walk finishing must not
        // report the world as idle while the outer one is still holding
        // pointers into a table.
        void BeginQueryIteration() { ++mQueryIterationDepth; }

        void EndQueryIteration()
        {
            MTS_ASSERT(mQueryIterationDepth > 0, "World::EndQueryIteration: not iterating");
            --mQueryIterationDepth;
        }

        /**
         * Structural changes during a query walk corrupt it, and quietly.
         *
         * Query::ForEach hands its callback a reference derived from a cached
         * column pointer and a row index. Adding or removing a component moves
         * the entity to another table and swap-removes its old row, so an
         * unvisited entity slides into a row already passed - silently skipped -
         * while the walk runs on past the shortened table. Creating an entity
         * can reallocate a column outright and leave the cached pointer
         * dangling. Destroying now cascades, so one call can do all of that to
         * an arbitrary number of rows at once.
         *
         * Record into a CommandBuffer instead; it applies at the phase boundary,
         * which is what the boundary is for.
         */
        static void AssertNotSparse([[maybe_unused]] const char *what, [[maybe_unused]] TypeId type)
        {
            MTS_ASSERT(!IsSparseComponentSeq(type.seq),
                       "World::{}: \"{}\" is a sparse component, which the erased path cannot reach - it "
                       "would report the component absent. Go through ComponentRegistry, which knows the "
                       "StorageKind.",
                       what, type.name);
        }

        void AssertNoStructuralChange([[maybe_unused]] const char *what) const
        {
            MTS_ASSERT(mQueryIterationDepth == 0,
                       "World::{}: structural change while a Query is iterating. Record it into a "
                       "CommandBuffer instead - see World::IsIterating",
                       what);
        }

        /**
         * Moves an entity to the table that also holds `type`, and copies
         * `size` bytes of `value` into the new row.
         *
         * Erased rather than templated because a script-declared component has
         * no C++ type to instantiate against - only a TypeId, a size and an
         * alignment, which is exactly what ComponentColumn's constructor has
         * always taken. The templated overload below is a thin façade over
         * this, so there is one implementation of the archetype move rather
         * than two that drift.
         *
         * Callers are responsible for the preconditions; the public AddRaw and
         * AddComponent do that checking.
         */
        void *AddTableComponentRaw(Entity entity, TypeId type, uint32_t size, uint32_t align, const void *value)
        {
            // get archetype
            const EntityRecord from = mRecords[entity.mIndex];

            Signature signature = from.archetype->GetSignature();
            // signature if added target component
            signature.set(ComponentBitOf(type));

            Archetype &target = GetOrCreateAdded(signature, *from.archetype, ComponentColumn(type, size, align));
            const uint32_t row = target.AddRow(entity);
            CopySharedColumns(*from.archetype, from.row, target, row);

            void *slot = target.FindColumn(type)->At(row);
            std::memcpy(slot, value, size);

            RemoveRow(*from.archetype, from.row);
            mRecords[entity.mIndex] = EntityRecord{&target, row};
            return slot;
        }

        // removing from archetype
        // move entityset to different table
        void RemoveTableComponentRaw(Entity entity, TypeId type)
        {
            const EntityRecord from = mRecords[entity.mIndex];
            Signature signature = from.archetype->GetSignature();
            // signature if removed target component
            signature.reset(ComponentBitOf(type));

            Archetype &target = GetOrCreateRemoved(signature, *from.archetype, type);
            const uint32_t row = target.AddRow(entity);

            CopySharedColumns(*from.archetype, from.row, target, row);

            RemoveRow(*from.archetype, from.row);
            mRecords[entity.mIndex] = EntityRecord{&target, row};
        }

        // adding to archetype
        // move entityset to different table
        template <typename T>
        T &AddTableComponent(Entity entity, const T &value)
        {
            return *static_cast<T *>(
                AddTableComponentRaw(entity, TypeIdOf<T>(), sizeof(T), alignof(T), &value));
        }

        template <typename T>
        void RemoveTableComponent(Entity entity)
        {
            RemoveTableComponentRaw(entity, TypeIdOf<T>());
        }

        // lazy creation
        template <typename T>
        SparseSetStorage<T> &SparseStorageFor()
        {
            const uint32_t seq = TypeIdOf<T>().seq;
            auto it = mSparseStorages.find(seq);
            if (it == mSparseStorages.end())
                it = mSparseStorages.emplace(seq, std::make_unique<detail::SparseStorageHolder<T>>()).first;

            return static_cast<detail::SparseStorageHolder<T> *>(it->second.get())->mStorage;
        }

        // lookup only - use SparseStorageFor for writing
        template <typename T>
        SparseSetStorage<T> *FindSparseStorage()
        {
            auto it = mSparseStorages.find(TypeIdOf<T>().seq);
            if (it == mSparseStorages.end())
                return nullptr;
            return &static_cast<detail::SparseStorageHolder<T> *>(it->second.get())->mStorage;
        }

        // lookup only - use SparseStorageFor for writing
        template <typename T>
        const SparseSetStorage<T> *FindSparseStorage() const
        {
            auto it = mSparseStorages.find(TypeIdOf<T>().seq);
            if (it == mSparseStorages.end())
                return nullptr;
            return &static_cast<const detail::SparseStorageHolder<T> *>(it->second.get())->mStorage;
        }

        // like merging in merge sort
        static void CopySharedColumns(Archetype &from, uint32_t fromRow, Archetype &to, uint32_t toRow)
        {
            std::vector<ComponentColumn> &fromColumns = from.Columns();
            std::vector<ComponentColumn> &toColumns = to.Columns();

            std::size_t i = 0;
            std::size_t j = 0;
            while (i < fromColumns.size() && j < toColumns.size())
            {
                const uint32_t fromSeq = fromColumns[i].Type().seq;
                const uint32_t toSeq = toColumns[j].Type().seq;

                if (fromSeq == toSeq)
                {
                    std::memcpy(toColumns[j].At(toRow), fromColumns[i].At(fromRow), fromColumns[i].ElementSize());
                    ++i;
                    ++j;
                }
                else if (fromSeq < toSeq)
                {
                    ++i;
                }
                else
                {
                    ++j;
                }
            }
        }

        Archetype &Insert(const Signature &signature, std::unique_ptr<Archetype> archetype)
        {
            ++mArchetypeGeneration;
            return *mArchetypes.emplace(signature, std::move(archetype)).first->second;
        }

        // return existing or create empty table of signature
        Archetype &GetOrCreateAdded(const Signature &signature, const Archetype &source, ComponentColumn added)
        {
            auto it = mArchetypes.find(signature);

            // table with sig exists
            if (it != mArchetypes.end())
                return *it->second;

            // make new table
            auto archetype = std::make_unique<Archetype>(signature);
            // create columns of same type as original
            for (const ComponentColumn &column : source.Columns())
                archetype->AddColumn(column.CloneEmpty());
            archetype->AddColumn(std::move(added));

            return Insert(signature, std::move(archetype));
        }

        // return existing or create empty table of signature
        Archetype &GetOrCreateRemoved(const Signature &signature, const Archetype &source, TypeId removed)
        {
            auto it = mArchetypes.find(signature);
            if (it != mArchetypes.end())
                return *it->second;

            // make new table, skip the target column
            auto archetype = std::make_unique<Archetype>(signature);
            for (const ComponentColumn &column : source.Columns())
            {
                if (column.Type().seq != removed.seq)
                    archetype->AddColumn(column.CloneEmpty());
            }

            return Insert(signature, std::move(archetype));
        }

        Archetype &GetOrCreateArchetype(const Signature &signature)
        {
            auto it = mArchetypes.find(signature);
            if (it != mArchetypes.end())
                return *it->second;

            return Insert(signature, std::make_unique<Archetype>(signature));
        }

        // drops a row and repairs the record of whoever got swapped into it
        void RemoveRow(Archetype &archetype, uint32_t row)
        {
            const Entity moved = archetype.SwapRemoveRow(row);
            if (!moved.IsNull())
                mRecords[moved.mIndex].row = row;
        }

        EntityPool mPool;
        std::vector<EntityRecord> mRecords; // indexed by Entity::mIndex
        std::unordered_map<Signature, std::unique_ptr<Archetype>> mArchetypes;
        Archetype *mEmptyArchetype = nullptr;
        std::unordered_map<uint32_t, std::unique_ptr<detail::ISparseStorage>> mSparseStorages; // by TypeId::seq
        std::unordered_map<uint32_t, std::unique_ptr<detail::IQuery>> mQueries;                // by detail::QueryKeyOf
        std::vector<EntityDestroyHook> mDestroyHooks;

        // Declared last, so it is destroyed first: reverse declaration order
        // keeps entity storage alive while resources are torn down, which is
        // what a resource holding entity handles needs.
        //
        // That is not licence to call back into the world from a resource
        // destructor. ~World is already destroying this map, so DestroyEntity -
        // whose hooks look resources up again - would search a container whose
        // elements are being destroyed, in an order nothing defines. Release
        // handles before the world goes down, not during.
        std::unordered_map<uint32_t, std::unique_ptr<detail::IResource>> mResources; // by detail::ResourceIdOf
        std::size_t mArchetypeGeneration = 0;
        uint32_t mQueryIterationDepth = 0;
    };
}

// Query needs a complete World, and World's query members need a complete Query.
// Both headers are #pragma once, so whichever is included first pulls in the
// other and this trailing include is a no-op on the way back up.
#include "Query.h"
