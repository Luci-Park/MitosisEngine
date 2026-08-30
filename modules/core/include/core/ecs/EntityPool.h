/**
 * @file EntityPool.h
 * @author Sumin Park
 * @brief Entity handle allocator
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "Entity.h"
#include "core/log/Assert.h"

#include <cstdint>
#include <vector>

namespace engine
{
    // recycle ids, distinguish them with generations
    class EntityPool
    {
    public:
        Entity Create()
        {
            if (!mFreeList.empty())
            {
                const uint32_t index = mFreeList.back();
                mFreeList.pop_back();
                return Entity{index, mGenerations[index]};
            }

            const uint32_t index = static_cast<uint32_t>(mGenerations.size());
            MTS_ASSERT(index != Entity::kNullIndex, "EntityPool::Create: entity index space exhausted");
            mGenerations.push_back(0);
            return Entity{index, 0};
        }

        void Destroy(Entity entity)
        {
            MTS_ASSERT(IsAlive(entity), "EntityPool::Destroy: entity is not alive");

            ++mGenerations[entity.mIndex];
            mFreeList.push_back(entity.mIndex);
        }

        bool IsAlive(Entity entity) const
        {
            return entity.mIndex < mGenerations.size() && mGenerations[entity.mIndex] == entity.mGeneration;
        }

        uint32_t Capacity() const { return static_cast<uint32_t>(mGenerations.size()); }

    private:
        std::vector<uint32_t> mGenerations; // per index, bumped on Destroy
        std::vector<uint32_t> mFreeList;    // indices ready for reuse
    };
}
