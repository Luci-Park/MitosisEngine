/**
 * @file TransformHierarchy.cpp
 * @author Sumin Park
 * @brief Resolving Transform through the scene graph into WorldTransform.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/TransformHierarchy.h"

#include "core/ecs/CommandBuffer.h"
#include "core/ecs/Query.h"
#include "core/ecs/World.h"
#include "core/log/Assert.h"
#include "core/log/Log.h"

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

        // HierarchyIndex keeps its mutators private so that mts::SetParent is
        // the only route in - it is what pairs a structural change with the
        // WorldTransform invalidation. This is that route's key.
        struct HierarchyMutator
        {
            static bool SetParent(HierarchyIndex &index, Entity child, Entity parent)
            {
                return index.SetParent(child, parent);
            }

            static void Remove(HierarchyIndex &index, Entity entity) { index.Remove(entity); }

            static std::vector<Entity> TakeChildren(HierarchyIndex &index, Entity entity)
            {
                return index.TakeChildren(entity);
            }
        };
    }

    namespace
    {
        // The parent, treating a destroyed one as none. The destroy hook means
        // nothing normally outlives its parent; this keeps a world that never
        // installed the hook resolving in place instead of dangling.
        //
        // Takes the index rather than looking it up: this runs once per level
        // of every resolve, and every resolve runs once per entity per frame,
        // so a hash lookup in here would dwarf the compares it exists to guard.
        Entity LiveParentOf(World &world, const HierarchyIndex *index, Entity entity)
        {
            if (index == nullptr)
                return kNullEntity;

            const Entity parent = index->ParentOf(entity);
            return world.IsAlive(parent) ? parent : kNullEntity;
        }

        void InvalidateCache(World &world, Entity entity)
        {
            if (WorldTransform *cache = world.Get<WorldTransform>(entity))
                detail::TransformResolver::Invalidate(*cache);
        }

        /**
         * Runs before any entity is torn down: destroys everything below it,
         * then drops it from the graph.
         *
         * Children are drained from the front rather than iterated, because
         * each recursive destroy removes itself from this entity's child list
         * and so rewrites the vector underneath the walk. Re-reading the span
         * every step is what makes that safe.
         */
        void OnEntityDestroyed(World &world, Entity entity, void *)
        {
            HierarchyIndex *index = world.TryResource<HierarchyIndex>();
            if (index == nullptr)
                return;

            // Detached in one pass rather than drained one at a time. Removing
            // them individually would make each child's Unlink scan and shift
            // this vector, turning a wide subtree into O(N^2).
            const std::vector<Entity> children = detail::HierarchyMutator::TakeChildren(*index, entity);

            for (const Entity child : children)
            {
                // The graph only ever holds handles put there by SetParent, and
                // Remove takes them out again, so a dead one here would mean
                // the graph and the world had already diverged. A cascade
                // arriving from another direction is the one benign case.
                MTS_ASSERT(world.IsAlive(child), "OnEntityDestroyed: dead entity left in the scene graph");

                if (world.IsAlive(child))
                    world.DestroyEntity(child);
            }

            detail::HierarchyMutator::Remove(*index, entity);
        }
    }

    HierarchyIndex &InstallHierarchy(World &world)
    {
        // Not EmplaceResource unconditionally: that replaces, which would throw
        // the whole scene graph away on the second call.
        if (!world.HasResource<HierarchyIndex>())
            world.EmplaceResource<HierarchyIndex>();

        // AddDestroyHook ignores a duplicate, so arming it from every entry
        // point costs a short scan and removes any way to forget it.
        world.AddDestroyHook(&OnEntityDestroyed);

        return world.Resource<HierarchyIndex>();
    }

    Entity ParentOf(const World &world, Entity entity)
    {
        const HierarchyIndex *index = world.TryResource<HierarchyIndex>();
        return index ? index->ParentOf(entity) : kNullEntity;
    }

    bool IsAncestorOf(const World &world, Entity ancestor, Entity entity)
    {
        // Reflexive before the lookup: an entity is its own ancestor whether or
        // not a graph was ever installed, and callers write reparent guards
        // against that contract.
        if (ancestor.IsNull() || entity.IsNull())
            return false;

        if (ancestor == entity)
            return true;

        const HierarchyIndex *index = world.TryResource<HierarchyIndex>();
        return index != nullptr && index->IsAncestorOf(ancestor, entity);
    }

    glm::mat4 ResolveWorld(World &world, Entity entity)
    {
        if (!world.IsAlive(entity))
            return glm::mat4(1.0f);

        // Walk up first, recording the chain, because a node cannot be built
        // until its parent is. Iterative rather than recursive, and bounded by
        // the same cap the graph refuses to be built past.
        const HierarchyIndex *index = world.TryResource<HierarchyIndex>();

        Entity chain[kMaxHierarchyDepth];
        uint32_t depth = 0;

        for (Entity cursor = entity; !cursor.IsNull(); cursor = LiveParentOf(world, index, cursor))
        {
            MTS_ASSERT(depth < kMaxHierarchyDepth,
                       "ResolveWorld: hierarchy deeper than {} - the graph should have refused this",
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

    bool SetParent(World &world, Entity child, Entity parent)
    {
        // Refusal is a documented return value, so these are logged rather
        // than asserted. An assert here would halt a debug build on input the
        // function is specified to handle, and would make every refusal path
        // untestable in the configuration where asserts are live.
        if (!world.IsAlive(child))
        {
            MTS_LOG_WARN("SetParent: child is not alive");
            return false;
        }

        // Aliveness is the world's business; cycles and depth are the graph's.
        if (!parent.IsNull() && !world.IsAlive(parent))
        {
            MTS_LOG_WARN("SetParent: parent is not alive");
            return false;
        }

        HierarchyIndex &index = InstallHierarchy(world);
        if (!detail::HierarchyMutator::SetParent(index, child, parent))
        {
            MTS_LOG_WARN("SetParent: refused - self-parenting, a cycle, or deeper than "
                         "kMaxHierarchyDepth ({})",
                         kMaxHierarchyDepth);
            return false;
        }

        // Versions are per-entity counters, so the new parent's version can
        // coincidentally equal the stamp left by the old one - two freshly
        // created parents are both at 1. The stamp alone cannot see a reparent,
        // so the cache is invalidated explicitly here. Only the child needs it:
        // its descendants stamp against *its* version, which the rebuild bumps.
        InvalidateCache(world, child);
        return true;
    }

    Transform &AddTransform(World &world, Entity entity, const Transform &transform, Entity parent)
    {
        InstallHierarchy(world);

        // Guarded rather than added blindly: World::AddComponent only asserts
        // against a duplicate, so in a release build a second call would append
        // a row to an archetype the entity already occupies and leave its
        // record naming a row that no longer exists.
        if (world.Has<Transform>(entity))
            *world.Get<Transform>(entity) = transform;
        else
            world.AddComponent<Transform>(entity, transform);

        if (!world.Has<WorldTransform>(entity))
            world.AddComponent<WorldTransform>(entity, WorldTransform{});

        // Through SetParent, so a repeat call on an already-linked entity gets
        // the detach, the cycle rejection and the cache invalidation.
        if (!parent.IsNull())
            SetParent(world, entity, parent);

        // Re-fetched, not carried over from AddComponent: adding WorldTransform
        // moves the entity to another archetype and relocates the Transform row
        // the first reference pointed at.
        return *world.Get<Transform>(entity);
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
