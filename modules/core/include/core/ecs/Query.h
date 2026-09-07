/**
 * @file Query.h
 * @author Sumin Park
 * @brief Persistent, cached component query over a World
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "World.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mts
{
    namespace detail
    {
        struct QueryIterationGuard
        {
            QueryIterationGuard(uint32_t &depth, World &world) : mDepth(depth), mWorld(world)
            {
                ++mDepth;
                mWorld.BeginQueryIteration();
            }

            ~QueryIterationGuard()
            {
                mWorld.EndQueryIteration();
                --mDepth;
            }

            QueryIterationGuard(const QueryIterationGuard &) = delete;
            QueryIterationGuard &operator=(const QueryIterationGuard &) = delete;

            uint32_t &mDepth; // for expressing nested iterations
            World &mWorld;
        };

        // Checks if given archetype matches query requirement
        class ArchetypeMatcher
        {
        public:
            // Every listed bit must be present (With)
            void RequireAll(const Signature &signature) { mAll |= signature; }

            // No listed bit may be present (Without)
            void RequireNone(const Signature &signature) { mNone |= signature; }

            // (And), (Or)
            void RequireAny(const Signature &clause) { mOrClauses.push_back(clause); }

            bool MatchesSignature(const Signature &signature) const
            {
                // every required bit must be present
                if ((signature & mAll) != mAll)
                    return false;
                // no excluded bit may be present
                if ((signature & mNone).any())
                    return false;
                // clauses AND together: each must be satisfied by one of its members
                for (const Signature &clause : mOrClauses)
                {
                    if ((signature & clause).none())
                        return false;
                }
                return true;
            }

            bool NeedsRefresh(const World &world) const { return world.Generation() != mSeenGeneration; }

            // forces refresh next round
            void Invalidate() { mSeenGeneration = static_cast<std::size_t>(-1); }

            // Build matches
            template <typename Fn>
            void Refresh(World &world, Fn &&onMatch)
            {
                for (const auto &[signature, archetype] : world.Archetypes())
                {
                    if (MatchesSignature(signature))
                        onMatch(archetype.get());
                }
                mSeenGeneration = world.Generation();
            }

        private:
            Signature mAll;
            Signature mNone;
            std::vector<Signature> mOrClauses; // one per Or term; empty for most queries
            std::size_t mSeenGeneration = static_cast<std::size_t>(-1);
        };
    }

    template <typename... Ts>
    class Query final : public detail::IQuery
    {
        static_assert(sizeof...(Ts) > 0, "Query: needs at least one component term");

        static_assert((!kIsTagComponent<detail::Bare<Ts>> && ...),
                      "Query: a tag has no fields, so it cannot be a data term - there would be no "
                      "reference to hand the callback. Put it in With<> or Without<> instead.");

        // number of components
        static constexpr std::size_t kTermCount = sizeof...(Ts);

        // array with all target columns
        using Columns = std::array<ComponentColumn *, kTermCount>;

        // for caching all archetypes that matches the query
        struct Match
        {
            Archetype *table;
            Columns columns;
        };

    public:
        // no order guaranteed.
        template <typename Fn>
        void ForEach(Fn &&fn)
        {
            ForEachImpl(fn, std::index_sequence_for<Ts...>{});
        }

        std::size_t MatchedArchetypeCount()
        {
            EnsureFresh();
            return mMatches.size();
        }

    private:
        friend class World;
#pragma region Query Creation
        template <typename... Filters>
        explicit Query(World &world, Filters... filters)
            : mWorld(&world)
        {
            // ArchetypeMatcher builds mask once
            mMatcher.RequireAll(SignatureOf<Ts...>());

            (ApplyFilter(filters), ...);
        }

        template <typename... Es>
        void ApplyFilter(With<Es...>)
        {
            mMatcher.RequireAll(SignatureOf<Es...>());
        }

        template <typename... Es>
        void ApplyFilter(Without<Es...>)
        {
            mMatcher.RequireNone(SignatureOf<Es...>());
        }

        template <typename... Es>
        void ApplyFilter(Or<Es...>)
        {
            static_assert(sizeof...(Es) > 0,
                          "Query: Or<> needs at least one component - an empty clause can never be satisfied");

            mMatcher.RequireAny(SignatureOf<Es...>());
        }
#pragma endregion

#pragma region Query Match Caching
        void EnsureFresh()
        {
            if (!mMatcher.NeedsRefresh(*mWorld))
                return;

            MTS_ASSERT(mIterationDepth == 0,
                       "Query::EnsureFresh: archetypes changed while this query is iterating; a "
                       "ForEach callback must not create archetypes and then re-run the same query");

            mMatches.clear();
            mMatcher.Refresh(*mWorld, [this](Archetype *table)
                             { mMatches.push_back(Match{table, ResolveColumns(table)}); });
        }

        static Columns ResolveColumns(Archetype *table)
        {
            return Columns{table->FindColumn(TypeIdOf<detail::Bare<Ts>>())...};
        }
#pragma endregion

#pragma region Query Iteration

        template <typename Fn>
        void ForEachMatchedTable(Fn &&fn)
        {
            EnsureFresh();

            // index rather than iterator in case of structural change
            const detail::QueryIterationGuard guard(mIterationDepth, *mWorld);
            for (std::size_t i = 0; i < mMatches.size(); ++i)
            {
                Match &match = mMatches[i];
                if (match.table->RowCount() == 0)
                    continue;
                fn(*match.table, match.columns);
            }
        }

        template <typename Fn, std::size_t... Is>
        void ForEachImpl(Fn &fn, std::index_sequence<Is...>)
        {
            ForEachMatchedTable([&](Archetype &table, Columns &columns)
                                {
                                    // limit iteration to current entities
                                    const uint32_t rows = table.RowCount();
                                    for (uint32_t row = 0; row < rows && row < table.RowCount(); ++row)
                                        fn(table.EntityAt(row), ResolveRef<Ts>(columns[Is], row)...); });
        }

        // Return target components
        template <typename T>
        static T &ResolveRef(ComponentColumn *column, uint32_t row)
        {
            return *static_cast<T *>(column->GetComponent(row));
        }
#pragma endregion

        World *mWorld;
        detail::ArchetypeMatcher mMatcher;
        std::vector<Match> mMatches;
        uint32_t mIterationDepth = 0;
    };

    template <typename... Ts, typename... Filters>
    Query<Ts...> &World::GetOrCreateQuery(Filters... filters)
    {
        const uint32_t key = detail::QueryKeyOf<detail::TypeList<Ts...>, Filters...>();

        auto it = mQueries.find(key);
        if (it == mQueries.end())
            // not make_unique: the Query constructor is private and World is the friend
            it = mQueries.emplace(key, std::unique_ptr<detail::IQuery>(new Query<Ts...>(*this, filters...))).first;

        return *static_cast<Query<Ts...> *>(it->second.get());
    }

    // Fn : function of fn(Entity, Ts&...)
    template <typename... Ts, typename Fn>
    void World::ForEach(Fn &&cb)
    {
        static_assert(sizeof...(Ts) > 0, "World::ForEach: needs at least one component");
        GetOrCreateQuery<Ts...>().ForEach(cb);
    }
}
