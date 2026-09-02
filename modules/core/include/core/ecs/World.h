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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mts
{
    class World;

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

            if constexpr (kIsSparseComponent<T>)
                SparseStorageFor<T>().Remove(entity);
            else
                RemoveTableComponent<T>(entity);
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

        const std::unordered_map<Signature, std::unique_ptr<Archetype>> &Archetypes() const { return mArchetypes; }

        // adding to archetype
        // move entityset to different table
        template <typename T>
        T &AddTableComponent(Entity entity, const T &value)
        {
            // get archetype
            const EntityRecord from = mRecords[entity.mIndex];

            Signature signature = from.archetype->GetSignature();
            // signature if added target component
            signature.set(ComponentBit<T>());

            Archetype &target = GetOrCreateAdded(signature, *from.archetype, ComponentColumn::For<T>());
            const uint32_t row = target.AddRow(entity);
            CopySharedColumns(*from.archetype, from.row, target, row);

            T *slot = static_cast<T *>(target.FindColumn(TypeIdOf<T>())->At(row));
            *slot = value;

            RemoveRow(*from.archetype, from.row);
            mRecords[entity.mIndex] = EntityRecord{&target, row};
            return *slot;
        }

        // removing from archetype
        // move entityset to different table
        template <typename T>
        void RemoveTableComponent(Entity entity)
        {
            const EntityRecord from = mRecords[entity.mIndex];
            Signature signature = from.archetype->GetSignature();
            // signature if removed target component
            signature.reset(ComponentBit<T>());

            Archetype &target = GetOrCreateRemoved(signature, *from.archetype, TypeIdOf<T>());
            const uint32_t row = target.AddRow(entity);

            CopySharedColumns(*from.archetype, from.row, target, row);

            RemoveRow(*from.archetype, from.row);
            mRecords[entity.mIndex] = EntityRecord{&target, row};
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
        std::size_t mArchetypeGeneration = 0;
    };
}

// Query needs a complete World, and World's query members need a complete Query.
// Both headers are #pragma once, so whichever is included first pulls in the
// other and this trailing include is a no-op on the way back up.
#include "Query.h"
