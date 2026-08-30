/**
 * @file SparseSetStorage.h
 * @author Sumin Park
 * @brief Sparse-set component storage
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "ComponentAsserts.h"
#include "Entity.h"
#include "core/log/Assert.h"

#include <cstdint>
#include <vector>

namespace engine
{
    template <typename T>
    class SparseSetStorage
    {
        MTS_ASSERT_COMPONENT(T);

    public:
        // (skeleton for now, filled in next chunks)
        void Add(Entity entity, const T &value);
        void Remove(Entity entity);
        bool Has(Entity entity) const;
        T *Get(Entity entity);
        const T *Get(Entity entity) const;

    private:
        std::vector<T> mDense;              // component in mSparse[entity.mIndex]
        std::vector<Entity> mDenseEntities; // entity in mSparse[entity.mIndex]
        std::vector<uint32_t> mSparse;      // idx or Entity::kNullIndex
    };

    template <typename T>
    void SparseSetStorage<T>::Add(Entity entity, const T &value)
    {
        const uint32_t idx = entity.mIndex;

        // lazy grow if entity idx is incorrect
        if (idx >= mSparse.size())
            mSparse.resize(idx + 1, Entity::kNullIndex);

        MTS_ASSERT(mSparse[idx] == Entity::kNullIndex, "SparseSetStorage::Add: entity already has this component");

        mSparse[idx] = static_cast<uint32_t>(mDense.size());
        mDense.push_back(value);
        mDenseEntities.push_back(entity);
    }

    template <typename T>
    void SparseSetStorage<T>::Remove(Entity entity)
    {
        const uint32_t idx = entity.mIndex;
        MTS_ASSERT(idx < mSparse.size() && mSparse[idx] != Entity::kNullIndex,
                   "SparseSetStorage::Remove: entity has no component to remove");

        const uint32_t slot = mSparse[idx];
        const uint32_t lastSlot = static_cast<uint32_t>(mDense.size()) - 1;

        // last element fills the hole, fix idx after
        mDense[slot] = mDense[lastSlot];
        mDenseEntities[slot] = mDenseEntities[lastSlot];
        mSparse[mDenseEntities[slot].mIndex] = slot;

        mDense.pop_back();
        mDenseEntities.pop_back();
        mSparse[idx] = Entity::kNullIndex;
    }

    template <typename T>
    bool SparseSetStorage<T>::Has(Entity entity) const
    {
        const uint32_t idx = entity.mIndex;
        return idx < mSparse.size() && mSparse[idx] != Entity::kNullIndex;
    }

    template <typename T>
    T *SparseSetStorage<T>::Get(Entity entity)
    {
        return Has(entity) ? &mDense[mSparse[entity.mIndex]] : nullptr;
    }

    template <typename T>
    const T *SparseSetStorage<T>::Get(Entity entity) const
    {
        return Has(entity) ? &mDense[mSparse[entity.mIndex]] : nullptr;
    }
}
