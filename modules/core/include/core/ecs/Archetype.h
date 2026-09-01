/**
 * @file Archetype.h
 * @author Sumin Park
 * @brief Table of entities - rows x col = entity x components
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "ComponentColumn.h"
#include "Entity.h"
#include "Signature.h"
#include "core/log/Assert.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace mts
{
    class Archetype
    {
    public:
        explicit Archetype(Signature signature) : mSignature(signature) {}

        const Signature &GetSignature() const { return mSignature; }
        uint32_t RowCount() const { return static_cast<uint32_t>(mEntities.size()); }
        Entity EntityAt(uint32_t row) const
        {
            MTS_ASSERT(row < RowCount(), "Archetype::EntityAt: row {} out of range ({})", row, RowCount());
            return mEntities[row];
        }

        const std::vector<Entity> &Entities() const { return mEntities; }
        std::vector<ComponentColumn> &Columns() { return mColumns; }
        const std::vector<ComponentColumn> &Columns() const { return mColumns; }

        void AddColumn(ComponentColumn column)
        {
            MTS_ASSERT(RowCount() == 0, "Archetype::AddColumn: columns must be added before any rows exist");

            // columns will be sorted by Type().seq
            const auto pos = std::lower_bound(mColumns.begin(), mColumns.end(), column.Type().seq,
                                              [](const ComponentColumn &c, uint32_t seq)
                                              { return c.Type().seq < seq; });
            mColumns.insert(pos, std::move(column));
        }

        // few columns per archetype, so a linear scan is faster
        ComponentColumn *FindColumn(TypeId type)
        {
            for (ComponentColumn &column : mColumns)
            {
                if (column.Type().seq == type.seq)
                    return &column;
            }
            return nullptr;
        }

        const ComponentColumn *FindColumn(TypeId type) const
        {
            for (const ComponentColumn &column : mColumns)
            {
                if (column.Type().seq == type.seq)
                    return &column;
            }
            return nullptr;
        }

        // creates row in table with unitialized components
        uint32_t AddRow(Entity entity)
        {
            const uint32_t row = RowCount();
            mEntities.push_back(entity);
            for (ComponentColumn &column : mColumns)
                column.PushBackUninitialized();
            return row;
        }

        // swap-remove across entities and every column at once.
        // returns the moved entity for info fixing
        Entity SwapRemoveRow(uint32_t row)
        {
            MTS_ASSERT(row < RowCount(), "Archetype::SwapRemoveRow: row {} out of range ({})", row, RowCount());

            const uint32_t lastRow = RowCount() - 1;
            const Entity moved = (row != lastRow) ? mEntities[lastRow] : kNullEntity;

            mEntities[row] = mEntities[lastRow];
            mEntities.pop_back();

            for (ComponentColumn &column : mColumns)
                column.SwapRemove(row);

            return moved;
        }

    private:
        Signature mSignature;
        std::vector<Entity> mEntities;         // row -> entity, for swap-remove fixup and iteration
        std::vector<ComponentColumn> mColumns; // sorted by TypeId::seq
    };
}
