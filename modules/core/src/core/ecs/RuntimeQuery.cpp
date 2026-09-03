/**
 * @file RuntimeQuery.cpp
 * @author Sumin Park
 * @brief A query whose terms are chosen at runtime rather than by template
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/RuntimeQuery.h"

#include "core/ecs/ComponentRegistry.h"
#include "core/ecs/Signature.h"
#include "core/log/Assert.h"

namespace mts
{
    namespace
    {
        /// Every term has to be a registered table component, checked here
        /// rather than left to fail quietly: a sparse component owns no
        /// signature bit, so requiring its bit would match no archetype at all
        /// and the query would simply return nothing.
        void CheckQueryable(TypeId type)
        {
            const ComponentOps *ops = ComponentRegistry::Instance().FindBySeq(type.seq);

            MTS_CHECK(ops != nullptr,
                      "RuntimeQuery: \"{}\" is not registered. Runtime queries resolve through "
                      "ComponentRegistry, so every term must be registered first.",
                      type.name);

            MTS_CHECK(ops->mStorage == StorageKind::Table,
                      "RuntimeQuery: \"{}\" is a sparse component. A sparse component has no signature "
                      "bit, so it cannot be a runtime query term.",
                      type.name);
        }

        /// Rebuilding the match list mid-walk drops the very tables the walk is
        /// standing on: EnsureFresh would clear mTables and mColumns while the
        /// outer ForEach still holds an `Archetype &` and a column pointer into
        /// them. Debug would fire EnsureFresh's assert, whose message blames
        /// archetype creation and would send the reader somewhere else entirely.
        void CheckNotIterating(uint32_t depth, const char *what)
        {
            MTS_ASSERT(depth == 0,
                       "RuntimeQuery::{}: a filter cannot be added while this query is iterating - the "
                       "match list it would rebuild is what the walk is reading from",
                       what);
        }

        Signature SignatureOfTerms(std::span<const TypeId> types)
        {
            Signature signature;
            for (TypeId type : types)
                signature.set(ComponentBitOf(type));
            return signature;
        }
    }

    RuntimeQuery::RuntimeQuery(World &world, std::span<const TypeId> terms) : mWorld(&world)
    {
        MTS_CHECK(!terms.empty(), "RuntimeQuery: needs at least one component term");

        mTerms.reserve(terms.size());
        for (TypeId type : terms)
        {
            CheckQueryable(type);

            for (TypeId seen : mTerms)
            {
                MTS_CHECK(seen.seq != type.seq, "RuntimeQuery: \"{}\" listed twice as a data term", type.name);
            }

            mTerms.push_back(type);
        }

        mMatcher.RequireAll(SignatureOfTerms(mTerms));
    }

    RuntimeQuery &RuntimeQuery::With(TypeId type)
    {
        CheckNotIterating(mIterationDepth, "With");
        CheckQueryable(type);
        mMatcher.RequireAll(SignatureOfTerms(std::span<const TypeId>(&type, 1)));
        mMatcher.Invalidate();
        return *this;
    }

    RuntimeQuery &RuntimeQuery::Without(TypeId type)
    {
        CheckNotIterating(mIterationDepth, "Without");
        CheckQueryable(type);
        mMatcher.RequireNone(SignatureOfTerms(std::span<const TypeId>(&type, 1)));
        mMatcher.Invalidate();
        return *this;
    }

    RuntimeQuery &RuntimeQuery::WithAny(std::span<const TypeId> types)
    {
        CheckNotIterating(mIterationDepth, "WithAny");
        MTS_CHECK(!types.empty(),
                  "RuntimeQuery::WithAny: an empty clause can never be satisfied, so it would reject "
                  "every archetype");

        for (TypeId type : types)
            CheckQueryable(type);

        mMatcher.RequireAny(SignatureOfTerms(types));
        mMatcher.Invalidate();
        return *this;
    }

    std::size_t RuntimeQuery::MatchedArchetypeCount()
    {
        EnsureFresh();
        return mTables.size();
    }

    void RuntimeQuery::EnsureFresh()
    {
        if (!mMatcher.NeedsRefresh(*mWorld))
            return;

        MTS_ASSERT(mIterationDepth == 0,
                   "RuntimeQuery::EnsureFresh: archetypes changed while this query is iterating; a "
                   "ForEach callback must not create archetypes and then re-run the same query");

        mTables.clear();
        mColumns.clear();

        // A matched table is guaranteed to hold every term's column - that is
        // what the signature mask tested - so the resolved pointers are never
        // null and the walk needs no per-term check.
        mMatcher.Refresh(*mWorld,
                         [this](Archetype *table)
                         {
                             mTables.push_back(table);
                             for (TypeId term : mTerms)
                                 mColumns.push_back(table->FindColumn(term));
                         });
    }
}
