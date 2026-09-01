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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mts
{
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

        // reset record, remove from both storages, remove from pool
        void DestroyEntity(Entity entity)
        {
            MTS_ASSERT(mPool.IsAlive(entity), "World::DestroyEntity: entity is not alive");

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

        // const world: every term must be const, so the query can never hand
        // back a mutable component reference
        template <typename... Ts, typename... Filters>
        const Query<Ts...> &GetOrCreateQuery(Filters... filters) const;

        // Use with GetOrCreateQuery<Ts...>().ForEach(cb); defined in Query.h.
        template <typename... Ts, typename Fn>
        void ForEach(Fn &&cb);

        template <typename... Ts, typename Fn>
        void ForEach(Fn &&cb) const;

    protected:
        template <typename...>
        friend class Query;

        template <typename... Ts, typename... Filters>
        Query<Ts...> &FindOrMakeQuery(Filters... filters) const;

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

        // lazy creation.
        // const: an absent storage and an empty one are indistinguishable to
        // every observer, so materialising one is not a logical change.
        template <typename T>
        SparseSetStorage<T> &SparseStorageFor() const
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
        // caches, not state: both are filled on demand by queries, so a const
        // World may still populate them
        mutable std::unordered_map<uint32_t, std::unique_ptr<detail::ISparseStorage>> mSparseStorages; // by TypeId::seq
        mutable std::unordered_map<uint32_t, std::unique_ptr<detail::IQuery>> mQueries;                // by detail::QueryKeyOf
        std::size_t mArchetypeGeneration = 0;
    };
}

// Query needs a complete World, and World's query members need a complete Query.
// Both headers are #pragma once, so whichever is included first pulls in the
// other and this trailing include is a no-op on the way back up.
#include "Query.h"
