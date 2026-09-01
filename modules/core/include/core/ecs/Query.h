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
            : mWorld(&world), mDataMask(detail::TableSignatureOf<Ts...>())
        {
            // filters are fixed for the life of a Query instance, so fold them
            // into the masks here rather than on every ForEach call
            (ApplyFilter(filters), ...);
            ResolveSparseStorages(std::index_sequence_for<Ts...>{});
        }

        // set for the duration of a table walk, so EnsureFresh can refuse to
        // rebuild mMatches while the walk still holds references into it
        struct IterationGuard
        {
            explicit IterationGuard(bool &flag) : mFlag(flag) { mFlag = true; }
            ~IterationGuard() { mFlag = false; }

            IterationGuard(const IterationGuard &) = delete;
            IterationGuard &operator=(const IterationGuard &) = delete;

            bool &mFlag;
        };

        // -- archetype-level match ----------------------------------------------

        bool Matches(const Signature &signature) const
        {
            // every dense data term must be present
            if ((signature & mDataMask) != mDataMask)
                return false;
            // every dense With member must be present
            if ((signature & mWithMask) != mWithMask)
                return false;
            // no dense Without member may be present
            if ((signature & mWithoutMask).any())
                return false;
            // clauses AND together: each must be satisfied by one of its members
            for (const Signature &clause : mOrClauses)
            {
                if ((signature & clause).none())
                    return false;
            }
            return true;
        }

        template <typename... Es>
        void ApplyFilter(With<Es...>)
        {
            mWithMask |= detail::TableSignatureOf<Es...>();
            (AddSparseFilter<Es>(true), ...);
        }

        template <typename... Es>
        void ApplyFilter(Without<Es...>)
        {
            mWithoutMask |= detail::TableSignatureOf<Es...>();
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

            mOrClauses.push_back(detail::TableSignatureOf<Es...>());
        }

        // dense members are already covered by the masks; a sparse member has no
        // bit, so it becomes a per-row Has() test instead of being dropped
        template <typename E>
        void AddSparseFilter(bool wantPresent)
        {
            if constexpr (kIsSparseComponent<detail::Bare<E>>)
            {
                using BareE = detail::Bare<E>;
                mSparseFilterChecks.push_back(detail::SparseFilterCheck{
                    &detail::SparseHasThunk<BareE>, &mWorld->SparseStorageFor<BareE>(), wantPresent});
            }
        }

        bool PassesSparseFilters(Entity entity) const
        {
            for (const detail::SparseFilterCheck &check : mSparseFilterChecks)
            {
                if (check.has(check.storage, entity) != check.wantPresent)
                    return false;
            }
            return true;
        }

        // -- cache --------------------------------------------------------------

        void EnsureFresh()
        {
            if (mWorld->Generation() == mSeenGeneration)
                return;

            MTS_ASSERT(!mIterating,
                       "Query::EnsureFresh: archetypes changed while this query is iterating; a "
                       "ForEach callback must not create archetypes and then re-run the same query");

            mMatches.clear();
            for (const auto &[signature, archetype] : mWorld->Archetypes())
            {
                if (!Matches(signature))
                    continue;

                Archetype *table = archetype.get();
                mMatches.push_back(Match{table, ResolveColumns(table)});
            }
            mSeenGeneration = mWorld->Generation();
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
            const IterationGuard guard(mIterating);
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
                                    const uint32_t rows = table.RowCount();
                                    for (uint32_t row = 0; row < rows; ++row)
                                    {
                                        const Entity entity = table.EntityAt(row);

                                        // sparse data terms are not in the signature, so presence is
                                        // a per-row test. The || short-circuits for dense terms,
                                        // whose storage pointer is null.
                                        if (!((!kIsSparseComponent<detail::Bare<Ts>> ||
                                               std::get<Is>(mSparseStorages)->Has(entity)) &&
                                              ...))
                                            continue;

                                        if (!PassesSparseFilters(entity))
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
        std::vector<Match> mMatches;
        std::size_t mSeenGeneration = static_cast<std::size_t>(-1); // never equal to a real generation
        bool mIterating = false;
        std::tuple<SparseSetStorage<detail::Bare<Ts>> *...> mSparseStorages{};

        Signature mDataMask;
        Signature mWithMask;
        Signature mWithoutMask;
        std::vector<Signature> mOrClauses; // one per Or term; empty for most queries
        std::vector<detail::SparseFilterCheck> mSparseFilterChecks;
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
