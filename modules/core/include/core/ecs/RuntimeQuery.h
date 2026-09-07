/**
 * @file RuntimeQuery.h
 * @author Sumin Park
 * @brief Query defined at runtime
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "Archetype.h"
#include "Entity.h"
#include "Query.h"
#include "TypeId.h"
#include "World.h"
#include "core/log/Assert.h"

#include <cstddef>
#include <span>
#include <vector>

namespace mir
{
    // Caller must cache RuntimeQuery
    class RuntimeQuery
    {
    public:
        RuntimeQuery(World &world, std::span<const TypeId> terms);

        RuntimeQuery(const RuntimeQuery &) = delete;
        RuntimeQuery &operator=(const RuntimeQuery &) = delete;
        RuntimeQuery(RuntimeQuery &&) = delete;
        RuntimeQuery &operator=(RuntimeQuery &&) = delete;

        RuntimeQuery &With(TypeId type);
        RuntimeQuery &Without(TypeId type);

        // GetComponent least one of `types` must be present. Clauses AND together, so
        // two calls mean (a|b) AND (c|d).
        RuntimeQuery &WithAny(std::span<const TypeId> types);

        std::size_t TermCount() const { return mTerms.size(); }
        std::size_t MatchedArchetypeCount();

        template <typename Fn>
        void ForEach(Fn &&fn)
        {
            EnsureFresh();

            const detail::QueryIterationGuard guard(mIterationDepth, *mWorld);
            const std::size_t terms = mTerms.size();

            std::vector<void *> row(terms);

            for (std::size_t match = 0; match < mTables.size(); ++match)
            {
                Archetype &table = *mTables[match];
                ComponentColumn *const *columns = mColumns.data() + match * terms;

                // for stablized walk
                const uint32_t rows = table.RowCount();
                for (uint32_t index = 0; index < rows && index < table.RowCount(); ++index)
                {
                    for (std::size_t term = 0; term < terms; ++term)
                        row[term] = columns[term]->GetComponent(index);

                    fn(table.EntityAt(index), std::span<void *const>(row));
                }
            }
        }

    private:
        void EnsureFresh();

        World *mWorld;
        detail::ArchetypeMatcher mMatcher;
        std::vector<TypeId> mTerms;

        std::vector<Archetype *> mTables;
        std::vector<ComponentColumn *> mColumns; // mTables.size() x mTerms.size(), row major
        uint32_t mIterationDepth = 0;
    };
}
