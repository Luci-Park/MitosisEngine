/**
 * @file RuntimeQuery.h
 * @author Sumin Park
 * @brief A query whose terms are chosen at runtime rather than by template
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

namespace mts
{
    /**
     * `world:each("Transform", "Velocity")` - the same walk Query<Ts...> does,
     * for terms that are only known as TypeIds at runtime.
     *
     * Shares detail::ArchetypeMatcher with Query, so the archetype-level match
     * test, the Or-clause semantics and the "rescan only when an archetype
     * appeared" rule have one implementation between them. What differs is only
     * the per-match cache: Query knows its term count at compile time and keeps
     * a std::array of columns, this keeps a runtime-sized one.
     *
     * Owned by the caller, unlike Query, which World caches. A query keyed on
     * `Ts...` can be cached by the type system for free; a runtime one would
     * need a second map keyed by a hash of the term list, and the layer that
     * wants the caching - a script host that knows which queries its scripts
     * re-run - is in a better position to key it than World is.
     *
     * Table storage only, and every term must be registered: the constructor
     * checks both against ComponentRegistry. A sparse term would be accepted
     * silently and then match nothing, because a sparse component has no
     * signature bit.
     */
    class RuntimeQuery
    {
    public:
        RuntimeQuery(World &world, std::span<const TypeId> terms);

        RuntimeQuery(const RuntimeQuery &) = delete;
        RuntimeQuery &operator=(const RuntimeQuery &) = delete;
        RuntimeQuery(RuntimeQuery &&) = delete;
        RuntimeQuery &operator=(RuntimeQuery &&) = delete;

        /// Narrowing filters. Each invalidates the cached match list, so a query
        /// may be narrowed after it has already run - but never from inside its
        /// own walk, where dropping the match list would dangle the references
        /// the callback is holding. Asserted, not merely documented.
        RuntimeQuery &With(TypeId type);
        RuntimeQuery &Without(TypeId type);

        /// At least one of `types` must be present. Clauses AND together, so
        /// two calls mean (a|b) AND (c|d).
        RuntimeQuery &WithAny(std::span<const TypeId> types);

        std::size_t TermCount() const { return mTerms.size(); }
        std::size_t MatchedArchetypeCount();

        /**
         * Calls `fn(Entity, std::span<void *const>)` for every matching entity,
         * with one pointer per term in the order they were given.
         *
         * The span and the pointers in it are valid for the duration of the
         * call and no longer: the next row moves them, and a structural change
         * would invalidate them outright - which is why the walk holds the same
         * iteration guard Query does, so an add or a destroy inside `fn` is a
         * diagnosable stop instead of a dangling write. Defer them through the
         * CommandBuffer; see DeferredAccess.h.
         */
        template <typename Fn>
        void ForEach(Fn &&fn)
        {
            EnsureFresh();

            const detail::QueryIterationGuard guard(mIterationDepth, *mWorld);
            const std::size_t terms = mTerms.size();

            // Local rather than a member scratch buffer: one allocation per
            // ForEach, not per row, and the query stays re-entrant from inside
            // its own callback the way Query is.
            std::vector<void *> row(terms);

            for (std::size_t match = 0; match < mTables.size(); ++match)
            {
                Archetype &table = *mTables[match];
                ComponentColumn *const *columns = mColumns.data() + match * terms;

                // Clamped against both the count at entry and the live one, for
                // the same reasons as Query::ForEachImpl: an entity created in
                // the callback must not be walked, and a release build, where
                // the structural-change assert is compiled out, must not run
                // past the end of a shortened table.
                const uint32_t rows = table.RowCount();
                for (uint32_t index = 0; index < rows && index < table.RowCount(); ++index)
                {
                    for (std::size_t term = 0; term < terms; ++term)
                        row[term] = columns[term]->At(index);

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
