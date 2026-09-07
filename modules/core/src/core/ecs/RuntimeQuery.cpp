/**
 * @file RuntimeQuery.cpp
 * @author Sumin Park
 * @brief Query defined at runtime
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
        const ComponentOps &CheckRegistered(TypeId type)
        {
            const ComponentOps *ops = ComponentRegistry::Instance().FindBySeq(type.seq);

            MTS_CHECK(ops != nullptr,
                      "RuntimeQuery: \"{}\" is not registered. Runtime queries resolve through "
                      "ComponentRegistry, so every term must be registered first.",
                      type.name);

            return *ops;
        }

        void CheckQueryable(TypeId type)
        {
            const ComponentOps &ops = CheckRegistered(type);

            MTS_CHECK(ops.mSize != 0,
                      "RuntimeQuery: \"{}\" is a tag, so there is no component to hand back for it. "
                      "Pass it to With(), Without() or WithAny() instead.",
                      type.name);
        }

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
        CheckRegistered(type);
        mMatcher.RequireAll(SignatureOfTerms(std::span<const TypeId>(&type, 1)));
        mMatcher.Invalidate();
        return *this;
    }

    RuntimeQuery &RuntimeQuery::Without(TypeId type)
    {
        CheckNotIterating(mIterationDepth, "Without");
        CheckRegistered(type);
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
            CheckRegistered(type);

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

        mMatcher.Refresh(*mWorld,
                         [this](Archetype *table)
                         {
                             mTables.push_back(table);
                             for (TypeId term : mTerms)
                                 mColumns.push_back(table->FindColumn(term));
                         });
    }
}
