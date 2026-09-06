/**
 * @file ComponentColumn.h
 * @author Sumin Park
 * @brief Type-erased column of POD components, one per component type
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "ComponentAsserts.h"
#include "TypeId.h"
#include "core/log/Assert.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mts
{
    class ComponentColumn
    {
    public:
        // ComponentColumn factory that creates type erased column for archetype
        template <typename T>
        static ComponentColumn For()
        {
            MTS_ASSERT_COMPONENT(T);
            return ComponentColumn(TypeIdOf<T>(), sizeof(T), alignof(T));
        }

        ComponentColumn(TypeId type, uint32_t elementSize, uint32_t alignment)
            : mType(type), mElementSize(elementSize), mAlignment(alignment)
        {
            // max_align_t is different between MSVC & GCC/Clang => use __STDCPP_DEFAULT_NEW_ALIGNMENT(16)
            MTS_ASSERT(alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                       "ComponentColumn: component \"{}\" is over-aligned ({}), unsupported", type.name, alignment);
        }

        TypeId Type() const { return mType; }
        uint32_t ElementSize() const { return mElementSize; }
        uint32_t Alignment() const { return mAlignment; }
        uint32_t Count() const { return static_cast<uint32_t>(mBytes.size()) / mElementSize; }

        // for table creation
        ComponentColumn CloneEmpty() const { return ComponentColumn(mType, mElementSize, mAlignment); }

        // component of row's entity
        // casting to T should be done outside of this class
        void *GetComponent(uint32_t row)
        {
            MTS_ASSERT(row < Count(), "ComponentColumn::GetComponent: row {} out of range ({})", row, Count());
            return mBytes.data() + static_cast<std::size_t>(row) * mElementSize;
        }

        // component of row's entity
        // casting to T should be done outside of this class
        const void *GetComponent(uint32_t row) const
        {
            MTS_ASSERT(row < Count(), "ComponentColumn::GetComponent: row {} out of range ({})", row, Count());
            return mBytes.data() + static_cast<std::size_t>(row) * mElementSize;
        }

        // push new unitialized row
        uint32_t PushBackUninitialized()
        {
            const uint32_t row = Count();
            mBytes.resize(mBytes.size() + mElementSize);
            return row;
        }

        // push new row with value
        uint32_t PushBackFrom(const void *value)
        {
            const uint32_t row = PushBackUninitialized();
            std::memcpy(GetComponent(row), value, mElementSize);
            return row;
        }

        // when a row needs to be removed, swap with last row then remove
        void SwapRemove(uint32_t row)
        {
            MTS_ASSERT(row < Count(), "ComponentColumn::SwapRemove: row {} out of range ({})", row, Count());

            const uint32_t lastRow = Count() - 1;
            if (row != lastRow)
                std::memcpy(GetComponent(row), GetComponent(lastRow), mElementSize);

            mBytes.resize(mBytes.size() - mElementSize);
        }

    private:
        std::vector<std::byte> mBytes; // Count() elements of mElementSize
        TypeId mType;
        uint32_t mElementSize;
        uint32_t mAlignment;
    };
}
