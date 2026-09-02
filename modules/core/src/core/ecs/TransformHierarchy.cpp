/**
 * @file TransformHierarchy.cpp
 * @author Sumin Park
 * @brief Resolving Transform + Hierarchy into WorldTransform.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/TransformHierarchy.h"

#include "core/ecs/CommandBuffer.h"
#include "core/ecs/Query.h"
#include "core/ecs/World.h"
#include "core/log/Assert.h"

namespace mts
{
    namespace detail
    {
        // WorldTransform's stamps are private so no game code can forge a
        // "clean" state; this is the one type allowed to write them.
        struct TransformResolver
        {
            static void Store(WorldTransform &target,
                              const glm::mat4 &matrix,
                              uint32_t localVersion,
                              uint32_t parentVersion)
            {
                target.mMatrix = matrix;
                target.mLocalVersion = localVersion;
                target.mParentVersion = parentVersion;
                target.mDirty = false;

                if (++target.mVersion == 0)
                    target.mVersion = 1;
            }

            static bool IsClean(const WorldTransform &target, uint32_t localVersion, uint32_t parentVersion)
            {
                return !target.mDirty &&
                       target.mLocalVersion == localVersion &&
                       target.mParentVersion == parentVersion;
            }

            // An explicit flag rather than a reserved version value: a node with
            // a WorldTransform but no Transform stamps localVersion 0 as its
            // legitimate value, so 0 could not also mean "invalid" for it.
            static void Invalidate(WorldTransform &target) { target.mDirty = true; }
        };

        /**
         * The only writer of Hierarchy's links. Every operation keeps the two
         * encodings of an edge - the child's mParent and the parent's chain -
         * in agreement, which is why the fields are not public.
         */
        struct HierarchyLinker
        {
            static void Detach(World &world, Entity child)
            {
                Hierarchy *node = world.Get<Hierarchy>(child);
                if (node == nullptr || node->mParent.IsNull())
                    return;

                // Splicing needs both neighbours, which is what mPrevSibling is
                // for: without it this would scan from the parent's head.
                if (Hierarchy *previous = world.Get<Hierarchy>(node->mPrevSibling))
                    previous->mNextSibling = node->mNextSibling;
                else if (Hierarchy *parent = world.Get<Hierarchy>(node->mParent))
                    parent->mFirstChild = node->mNextSibling; // child was the head

                if (Hierarchy *next = world.Get<Hierarchy>(node->mNextSibling))
                    next->mPrevSibling = node->mPrevSibling;

                node->mParent = kNullEntity;
                node->mNextSibling = kNullEntity;
                node->mPrevSibling = kNullEntity;
            }

            // Assumes `child` is already detached.
            static void Attach(World &world, Entity child, Entity parent)
            {
                Hierarchy *node = world.Get<Hierarchy>(child);
                Hierarchy *head = world.Get<Hierarchy>(parent);

                // Callers run EnsureHierarchy on both ends first. Reaching here
                // without one means the edge would be silently dropped - and
                // Detach has already run - so say so rather than leave the
                // child mysteriously rooted.
                MTS_ASSERT(node != nullptr && head != nullptr,
                           "HierarchyLinker::Attach: both entities need a Hierarchy component");

                if (node == nullptr || head == nullptr)
                    return;

                // Pushed onto the front, so linking stays O(1) and no tail
                // pointer has to be maintained. Child order is therefore
                // reverse insertion order, which nothing may rely on.
                node->mParent = parent;
                node->mPrevSibling = kNullEntity;
                node->mNextSibling = head->mFirstChild;

                if (Hierarchy *first = world.Get<Hierarchy>(head->mFirstChild))
                    first->mPrevSibling = child;

                head->mFirstChild = child;
            }
        };
    }

    namespace
    {
        // The parent of `entity`, or null if it has none, the link is stale, or
        // the parent has been destroyed. The destroy hook means nothing should
        // outlive its parent; the aliveness check keeps a World that never
        // installed the hook resolving in place instead of dangling.
        Entity ParentOf(World &world, Entity entity)
        {
            const Hierarchy *node = world.Get<Hierarchy>(entity);
            if (node == nullptr || node->Parent().IsNull() || !world.IsAlive(node->Parent()))
                return kNullEntity;

            return node->Parent();
        }

        // Both ends of an edge need the component before it can be linked.
        // Also arms the destroy hook, so a World that only ever went through
        // SetParent still cascades.
        void EnsureHierarchy(World &world, Entity entity)
        {
            InstallHierarchyHooks(world);

            if (!world.Has<Hierarchy>(entity))
                world.AddComponent<Hierarchy>(entity, Hierarchy{});
        }

        // Levels above `entity`, stopping at the cap so a malformed graph
        // cannot spin here.
        uint32_t DepthOf(World &world, Entity entity)
        {
            uint32_t depth = 0;
            for (Entity cursor = ParentOf(world, entity); !cursor.IsNull(); cursor = ParentOf(world, cursor))
            {
                if (++depth >= kMaxHierarchyDepth)
                    break;
            }

            return depth;
        }

        void InvalidateCache(World &world, Entity entity)
        {
            if (WorldTransform *cache = world.Get<WorldTransform>(entity))
                detail::TransformResolver::Invalidate(*cache);
        }

        /**
         * Runs before any entity is torn down. Unlinks it from its parent's
         * chain, then destroys everything below it.
         *
         * Children are drained from the head rather than walked with
         * NextSibling: each recursive destroy re-enters this hook and splices
         * itself out, so the chain is rewritten underneath us at every step.
         * Re-reading FirstChild each time is what makes that safe, and it also
         * re-fetches the component, whose row a nested destroy may have moved.
         */
        void OnEntityDestroyed(World &world, Entity entity, void *)
        {
            if (world.Get<Hierarchy>(entity) == nullptr)
                return;

            detail::HierarchyLinker::Detach(world, entity);

            while (true)
            {
                const Hierarchy *node = world.Get<Hierarchy>(entity);
                if (node == nullptr)
                    return;

                const Entity child = node->FirstChild();
                if (child.IsNull())
                    return;

                // Only Detach and Attach write the chains, and both keep them
                // in step with the live set, so a dead handle here would mean
                // the invariant is already broken. Bail rather than loop.
                MTS_ASSERT(world.IsAlive(child),
                           "OnEntityDestroyed: dead entity left in a child chain");

                if (!world.IsAlive(child))
                    return;

                world.DestroyEntity(child);
            }
        }
    }

    void InstallHierarchyHooks(World &world)
    {
        // AddDestroyHook ignores a duplicate, so calling this from every entry
        // point costs a short scan and removes any way to forget it.
        world.AddDestroyHook(&OnEntityDestroyed);
    }

    glm::mat4 ResolveWorld(World &world, Entity entity)
    {
        if (!world.IsAlive(entity))
            return glm::mat4(1.0f);

        // Walk up first, recording the chain, because a node cannot be built
        // until its parent is. Iterative rather than recursive so a cycle hits
        // the depth assert instead of the stack guard page.
        Entity chain[kMaxHierarchyDepth];
        uint32_t depth = 0;

        for (Entity cursor = entity; !cursor.IsNull(); cursor = ParentOf(world, cursor))
        {
            MTS_ASSERT(depth < kMaxHierarchyDepth,
                       "ResolveWorld: hierarchy deeper than {} - almost certainly a parent cycle",
                       kMaxHierarchyDepth);

            if (depth >= kMaxHierarchyDepth)
                break;

            chain[depth++] = cursor;
        }

        glm::mat4 accumulated(1.0f);

        // Version of the parent's WorldTransform, threaded down the walk.
        // 0 means "root", which is why WorldTransform::mVersion never takes it.
        uint32_t parentVersion = 0;

        // False once an ancestor has no WorldTransform to stamp. Its
        // descendants then have no version to compare against, so they must
        // recompute every time rather than trust a stamp that cannot move.
        bool cacheable = true;

        // Now back down, root first.
        for (uint32_t i = depth; i-- > 0;)
        {
            const Entity current = chain[i];
            const Transform *local = world.Get<Transform>(current);
            WorldTransform *cache = world.Get<WorldTransform>(current);

            // No Transform means the node contributes identity but still passes
            // its parent's space through - an empty pivot node.
            const uint32_t localVersion = local ? local->Version() : 0;

            if (cache == nullptr)
            {
                accumulated = local ? accumulated * local->Matrix() : accumulated;
                cacheable = false;
                continue;
            }

            if (cacheable && detail::TransformResolver::IsClean(*cache, localVersion, parentVersion))
            {
                accumulated = cache->Matrix();
                parentVersion = cache->Version();
                continue;
            }

            accumulated = local ? accumulated * local->Matrix() : accumulated;
            detail::TransformResolver::Store(*cache, accumulated, localVersion, parentVersion);
            parentVersion = cache->Version();
            cacheable = true;
        }

        return accumulated;
    }

    Transform &AddTransform(World &world, Entity entity, const Transform &transform, Entity parent)
    {
        InstallHierarchyHooks(world);

        // Guarded like the other two: World::AddComponent only asserts against
        // a duplicate, so in a release build a second call would append a row
        // to an archetype the entity already occupies and leave its record
        // naming a row that no longer exists.
        if (world.Has<Transform>(entity))
            *world.Get<Transform>(entity) = transform;
        else
            world.AddComponent<Transform>(entity, transform);

        if (!world.Has<Hierarchy>(entity))
            world.AddComponent<Hierarchy>(entity, Hierarchy{});

        if (!world.Has<WorldTransform>(entity))
            world.AddComponent<WorldTransform>(entity, WorldTransform{});

        // Routed through SetParent rather than calling Attach directly.
        // AddTransform is idempotent, so it can land on an entity that is
        // already in a chain, and Attach assumes an unlinked node - it would
        // leave the entity in its old parent's chain as well as the new one.
        // SetParent detaches first, rejects a cycle, and dirties the cache.
        if (!parent.IsNull())
            SetParent(world, entity, parent);

        // Re-fetched, not carried over from AddComponent: each further add,
        // and the Hierarchy that SetParent may give the parent, moves an
        // entity to another archetype and relocates rows.
        return *world.Get<Transform>(entity);
    }

    bool IsAncestorOf(World &world, Entity ancestor, Entity entity)
    {
        if (ancestor.IsNull() || entity.IsNull())
            return false;

        uint32_t depth = 0;
        for (Entity cursor = entity; !cursor.IsNull(); cursor = ParentOf(world, cursor))
        {
            if (cursor == ancestor)
                return true;

            // Incremented as its own statement: MTS_ASSERT compiles to
            // ((void)0) under NDEBUG, so a side effect inside it would simply
            // not happen in a release build and the bound below could never
            // fire.
            ++depth;

            MTS_ASSERT(depth < kMaxHierarchyDepth,
                       "IsAncestorOf: hierarchy deeper than {} - almost certainly a parent cycle",
                       kMaxHierarchyDepth);

            if (depth >= kMaxHierarchyDepth)
                break;
        }

        return false;
    }

    void SetParent(World &world, Entity child, Entity parent)
    {
        MTS_ASSERT(world.IsAlive(child), "SetParent: child is not alive");

        if (!parent.IsNull())
        {
            // Walking up from the proposed parent is what makes the cycle check
            // O(depth): if `child` is already above it, linking closes a loop.
            // Evaluated once, and only when the cheaper guards passed, since it
            // is the walk that a cycle would make non-terminating.
            const bool sane = world.IsAlive(parent) && child != parent;
            const bool cycle = sane && IsAncestorOf(world, child, parent);

            MTS_ASSERT(world.IsAlive(parent), "SetParent: parent is not alive");
            MTS_ASSERT(child != parent, "SetParent: an entity cannot be its own parent");
            MTS_ASSERT(!cycle,
                       "SetParent: would create a cycle - parent is already a descendant of child");

            // ResolveWorld and IsAncestorOf both stop at kMaxHierarchyDepth, so
            // a chain built past it resolves against a truncated ancestor list
            // - a silently wrong matrix in a release build - and IsAncestorOf
            // stops detecting cycles that close above the cap. Bounding the
            // graph where it is built is what keeps those walks honest.
            const bool tooDeep = sane && DepthOf(world, parent) + 1 >= kMaxHierarchyDepth;
            MTS_ASSERT(!tooDeep,
                       "SetParent: chain would exceed kMaxHierarchyDepth ({})",
                       kMaxHierarchyDepth);

            // Refused outright, not merely asserted: MTS_ASSERT is compiled out
            // under NDEBUG, and one cycle that reaches a release build makes
            // every upward walk in this file non-terminating. Rejecting the
            // edge keeps the graph acyclic and bounded in every build.
            if (!sane || cycle || tooDeep)
                return;
        }

        if (!world.Has<Hierarchy>(child) && parent.IsNull())
            return; // rooting something with no links is already true

        EnsureHierarchy(world, child);
        if (!parent.IsNull())
            EnsureHierarchy(world, parent);

        detail::HierarchyLinker::Detach(world, child);
        if (!parent.IsNull())
            detail::HierarchyLinker::Attach(world, child, parent);

        // Versions are per-entity counters, so the new parent's version can
        // coincidentally equal the stamp left by the old one - two freshly
        // created parents are both at 1. The stamp alone cannot see a reparent,
        // so the cache is invalidated explicitly here. Only the child needs it:
        // its descendants stamp against *its* version, which the rebuild bumps.
        InvalidateCache(world, child);
    }

    void TransformPropagateSystem::OnStart(SystemContext &context)
    {
        mQuery = &context.world.GetOrCreateQuery<Transform, WorldTransform>();
    }

    void TransformPropagateSystem::OnUpdate(SystemContext &context)
    {
        World &world = context.world;

        // ResolveWorld writes into existing rows only - never adds or removes a
        // component - so no archetype is created or resized while the query
        // holds column pointers.
        mQuery->ForEach([&world](Entity entity, Transform &, WorldTransform &)
                        { ResolveWorld(world, entity); });
    }
}
