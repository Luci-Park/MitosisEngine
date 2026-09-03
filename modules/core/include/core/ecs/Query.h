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
#include <tuple>
#include <utility>
#include <vector>

namespace mts
{
    namespace detail
    {
        // binds SparseSetStorage<T>::Has to the erased SparseFilterCheck
        template <typename T>
        bool SparseHasThunk(const void *storage, Entity entity)
        {
            return static_cast<const SparseSetStorage<T> *>(storage)->Has(entity);
        }

        /**
         * Held for the duration of a table walk, by every walker.
         *
         * It stops the owning query rebuilding its match list under the walk,
         * and tells the world to refuse structural changes while references
         * into a table are live. It is a type rather than a pair of calls
         * because it is the *only* way to raise World's iteration depth - see
         * the friend declarations in World - so a walker cannot forget the
         * matching decrement, and no caller outside these types can walk
         * archetypes with the guard down.
         *
         * A depth rather than a flag: the same query may be re-entered from its
         * own callback for a pairwise scan, and a flag would let the inner
         * walk's destructor declare the outer one finished.
         */
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

            uint32_t &mDepth;
            World &mWorld;
        };

        /**
         * The archetype-level match test and its generation cache, shared by
         * Query<Ts...> and RuntimeQuery.
         *
         * Both need the same three things - fold the terms into signature
         * masks once, reject whole tables by mask, and rescan only when the
         * world grew an archetype - and the two differ only in what they cache
         * per match (a fixed std::array of columns against a runtime-sized
         * one). Keeping the test here means a fix to the Or-clause semantics or
         * the rescan trigger lands in one place rather than in two copies that
         * drift.
         *
         * This is also the only walker of World::Archetypes, which is why that
         * accessor can stay protected.
         */
        class ArchetypeMatcher
        {
        public:
            /// Every listed bit must be present. Data terms and With members
            /// impose the identical test, so they share one mask.
            void RequireAll(const Signature &signature) { mAll |= signature; }

            /// No listed bit may be present.
            void RequireNone(const Signature &signature) { mNone |= signature; }

            /// One clause: at least one of its bits must be present. Members
            /// within a clause OR together, clauses AND together, so (A|B) AND
            /// (C|D) is two calls. Merging them into one mask would silently
            /// widen that to any-of-all-four.
            void RequireAny(const Signature &clause) { mOrClauses.push_back(clause); }

            /// A sparse component has no signature bit, so its filter becomes a
            /// per-row test instead of being dropped.
            void AddSparseCheck(const SparseFilterCheck &check) { mSparseChecks.push_back(check); }

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

            bool PassesSparseChecks(Entity entity) const
            {
                for (const SparseFilterCheck &check : mSparseChecks)
                {
                    if (check.has(check.storage, entity) != check.wantPresent)
                        return false;
                }
                return true;
            }

            bool NeedsRefresh(const World &world) const { return world.Generation() != mSeenGeneration; }

            /// Forces the next NeedsRefresh to say yes. Needed by any owner
            /// that may add a term after the first walk - the generation stamp
            /// tracks the world's archetypes, not this matcher's own masks.
            void Invalidate() { mSeenGeneration = static_cast<std::size_t>(-1); }

            /// Calls `onMatch(Archetype *)` for every matching table, then
            /// stamps the generation. The caller owns the cache it fills.
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
            std::vector<SparseFilterCheck> mSparseChecks;
            std::size_t mSeenGeneration = static_cast<std::size_t>(-1); // never equal to a real generation
        };
    }

    /**
     * A query over data terms Ts..., narrowed by With/Without/Or filters.
     *
     * Owned by World (see World::GetOrCreateQuery) for caching.
     * Reused until a new archetype appears, updates after world generation is bumped
     *
     * May request const T, which yields a const T& in the callback.
     */
    template <typename... Ts>
    class Query final : public detail::IQuery
    {
        static_assert(sizeof...(Ts) > 0, "Query: needs at least one component term");

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
        // calls fn(Entity, Ts&...) for every live entity in this query.
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

        template <typename... Filters>
        explicit Query(World &world, Filters... filters)
            : mWorld(&world)
        {
            mMatcher.RequireAll(detail::TableSignatureOf<Ts...>());

            // filters are fixed for the life of a Query instance, so fold them
            // into the masks here rather than on every ForEach call
            (ApplyFilter(filters), ...);
            ResolveSparseStorages(std::index_sequence_for<Ts...>{});
        }

        // -- archetype-level match ----------------------------------------------

        template <typename... Es>
        void ApplyFilter(With<Es...>)
        {
            mMatcher.RequireAll(detail::TableSignatureOf<Es...>());
            (AddSparseFilter<Es>(true), ...);
        }

        template <typename... Es>
        void ApplyFilter(Without<Es...>)
        {
            mMatcher.RequireNone(detail::TableSignatureOf<Es...>());
            (AddSparseFilter<Es>(false), ...);
        }

        // Each Or term is its own clause. Members within a clause OR together,
        // clauses AND together, so (A|B) AND (C|D) is Or<A, B>{}, Or<C, D>{}.
        // Merging every term into one mask instead would silently widen that to
        // any-of-all-four.
        template <typename... Es>
        void ApplyFilter(Or<Es...>)
        {
            // a zero clause signature makes the none() test below always fire,
            // which would reject every archetype
            static_assert(sizeof...(Es) > 0,
                          "Query: Or<> needs at least one component - an empty clause can never be satisfied");

            // a sparse member has no bit, so it could never make the mask test
            // pass; honouring it would mean demoting the clause to a per-row
            // test. Rejected loudly rather than silently dropped.
            static_assert((!kIsSparseComponent<detail::Bare<Es>> && ...),
                          "Query: Or<> members must be dense components - a sparse component has no "
                          "signature bit, so it cannot take part in an archetype-level or-test");

            mMatcher.RequireAny(detail::TableSignatureOf<Es...>());
        }

        // dense members are already covered by the masks; a sparse member has no
        // bit, so it becomes a per-row Has() test instead of being dropped
        template <typename E>
        void AddSparseFilter(bool wantPresent)
        {
            if constexpr (kIsSparseComponent<detail::Bare<E>>)
            {
                using BareE = detail::Bare<E>;
                mMatcher.AddSparseCheck(detail::SparseFilterCheck{
                    &detail::SparseHasThunk<BareE>, &mWorld->SparseStorageFor<BareE>(), wantPresent});
            }
        }

        // -- cache --------------------------------------------------------------

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
            return Columns{(kIsSparseComponent<detail::Bare<Ts>>
                                ? nullptr
                                : table->FindColumn(TypeIdOf<detail::Bare<Ts>>()))...};
        }

        template <std::size_t... Is>
        void ResolveSparseStorages(std::index_sequence<Is...>)
        {
            (ResolveSparseStorage<Is, Ts>(), ...);
        }

        template <std::size_t I, typename T>
        void ResolveSparseStorage()
        {
            if constexpr (kIsSparseComponent<detail::Bare<T>>)
                std::get<I>(mSparseStorages) = &mWorld->SparseStorageFor<detail::Bare<T>>();
        }

        // -- iteration ----------------------------------------------------------

        // table walk, split from the row walk so a ForEachChunk can be added as a
        // sibling later without touching ForEach. Internal only.
        template <typename Fn>
        void ForEachMatchedTable(Fn &&fn)
        {
            EnsureFresh();

            // index rather than iterator, and the guard makes a rebuild under the
            // walk a loud failure instead of a dangling reference
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
                                    // Clamped against both the count at entry
                                    // and the live one. The first stops a
                                    // callback that spawns entities from
                                    // walking rows it just created - CreateEntity
                                    // stays legal mid-walk. The second stops a
                                    // release build, where the structural-change
                                    // assert is compiled out, from running past
                                    // the end of a table something shortened.
                                    const uint32_t rows = table.RowCount();
                                    for (uint32_t row = 0; row < rows && row < table.RowCount(); ++row)
                                    {
                                        const Entity entity = table.EntityAt(row);

                                        // sparse data terms are not in the signature, so presence is
                                        // a per-row test. The || short-circuits for dense terms,
                                        // whose storage pointer is null.
                                        if (!((!kIsSparseComponent<detail::Bare<Ts>> ||
                                               std::get<Is>(mSparseStorages)->Has(entity)) &&
                                              ...))
                                            continue;

                                        if (!mMatcher.PassesSparseChecks(entity))
                                            continue;

                                        fn(entity, ResolveRef<Ts>(columns[Is], std::get<Is>(mSparseStorages),
                                                                  entity, row)...);
                                    } });
        }

        // table -> the cached column row slot; sparse -> dense lookup by entity.
        // Storage lookups use Bare<T>; the cast and the return type keep T, so a
        // const T term hands the callback a const T& with no extra machinery.
        template <typename T, typename Storage>
        static T &ResolveRef(ComponentColumn *column, Storage storage, Entity entity, uint32_t row)
        {
            if constexpr (kIsSparseComponent<detail::Bare<T>>)
                return *storage->Get(entity);
            else
                return *static_cast<T *>(column->At(row));
        }

        World *mWorld;
        detail::ArchetypeMatcher mMatcher;
        std::vector<Match> mMatches;
        uint32_t mIterationDepth = 0;
        std::tuple<SparseSetStorage<detail::Bare<Ts>> *...> mSparseStorages{};
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
