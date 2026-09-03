/**
 * @file TransformHierarchy.h
 * @author Sumin Park
 * @brief Resolving Transform through the scene graph into WorldTransform.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/Entity.h"
#include "core/ecs/HierarchyIndex.h"
#include "core/ecs/System.h"
#include "core/ecs/World.h"
#include "core/ecs/components/Transform.h"
#include "core/ecs/components/WorldTransform.h"

#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <vector>

namespace mts
{
    /**
     * World matrix of `entity`, refreshing it and every stale ancestor first.
     *
     * This is the answer to "something asks between schedules": a read is what
     * triggers the recompute, so a read cannot observe a stale value. Correct
     * at any point in the frame, in any phase, with no ordering discipline
     * required from the caller.
     *
     * Cost is O(depth). A clean chain costs a flag and two integer compares per
     * level and touches no matrix maths. Entities with a WorldTransform keep
     * the result; entities without one still get a correct matrix, just
     * uncached.
     */
    glm::mat4 ResolveWorld(World &world, Entity entity);

    /// Adds Transform and its WorldTransform cache, optionally parented.
    /// Adding a bare Transform still works - it just resolves uncached.
    ///
    /// Idempotent: calling it again overwrites the Transform and re-links, so
    /// it is safe on an entity already in the graph.
    Transform &AddTransform(World &world,
                            Entity entity,
                            const Transform &transform = Transform{},
                            Entity parent = kNullEntity);

    /**
     * Parents `child` to `parent`, or roots it when `parent` is null.
     *
     * Refuses, rather than only asserting on, a dead parent, self-parenting, a
     * cycle, or a chain that would pass kMaxHierarchyDepth - see
     * HierarchyIndex::SetParent. Returns false when refused.
     *
     * The child's local Transform is left untouched, so it snaps to being
     * relative to its new parent rather than holding its world pose. Preserving
     * world pose across a reparent is a separate operation, not yet written.
     */
    bool SetParent(World &world, Entity child, Entity parent);

    /// Parent of `entity`, or null. Reads the graph without creating it.
    Entity ParentOf(const World &world, Entity entity);

    /// True if `ancestor` is `entity` or any transitive parent of it.
    bool IsAncestorOf(const World &world, Entity ancestor, Entity entity);

    /**
     * Installs the HierarchyIndex resource and the destroy hook that makes
     * World::DestroyEntity cascade: destroying an entity destroys everything
     * below it and detaches it from its parent.
     *
     * A hook rather than something built into World, which knows nothing about
     * the scene graph. There is no separate DestroyHierarchy because the
     * primitive already does the job - including CommandBuffer::Destroy, which
     * flushes through the same call.
     *
     * Idempotent, and called from AddTransform and SetParent, so a world that
     * has ever held a hierarchy already has it.
     */
    HierarchyIndex &InstallHierarchy(World &world);

    /**
     * Calls `fn(Entity)` for each direct child of `parent`, in insertion order.
     *
     * `fn` may reparent or destroy anything, including children other than the
     * one it was handed: the list is snapshotted first, and every entry is
     * re-checked against the live graph before it is visited. Children added
     * during the walk are not visited. That snapshot costs one allocation.
     */
    template <typename Fn>
    void ForEachChild(World &world, Entity parent, Fn &&fn)
    {
        HierarchyIndex *index = world.TryResource<HierarchyIndex>();
        if (index == nullptr)
            return;

        // Snapshotted rather than walked by index. Resuming by position can
        // only recover from a single removal at or before the cursor; if `fn`
        // removes two, the next child is silently skipped - a wrong answer with
        // no diagnostic, which is worth an allocation to rule out.
        const std::span<const Entity> children = index->ChildrenOf(parent);
        std::vector<Entity> snapshot(children.begin(), children.end());

        for (const Entity child : snapshot)
        {
            // Re-checked because `fn` may have destroyed or reparented this one
            // on an earlier iteration.
            if (world.IsAlive(child) && index->ParentOf(child) == parent)
                fn(child);
        }
    }

    /**
     * Calls `fn(Entity)` for every entity below `root`, parents before
     * children, excluding `root` itself.
     *
     * The whole subtree is collected before `fn` sees any of it, so `fn` may
     * reparent or destroy freely. That snapshot costs one allocation per call.
     */
    template <typename Fn>
    void ForEachDescendant(World &world, Entity root, Fn &&fn)
    {
        const HierarchyIndex *index = world.TryResource<HierarchyIndex>();
        if (index == nullptr)
            return;

        std::vector<Entity> pending;
        const std::span<const Entity> roots = index->ChildrenOf(root);
        pending.insert(pending.end(), roots.begin(), roots.end());

        for (std::size_t i = 0; i < pending.size(); ++i)
        {
            // The span points into the index, which this loop never mutates -
            // only `pending` grows - so it stays valid across the insert.
            const std::span<const Entity> children = index->ChildrenOf(pending[i]);
            pending.insert(pending.end(), children.begin(), children.end());
        }

        for (const Entity entity : pending)
            fn(entity);
    }

    /**
     * Refreshes every WorldTransform once per frame, so that read-only systems
     * downstream may touch WorldTransform::Matrix() directly - const, no
     * mutation, safe to run in parallel later.
     *
     * This is a batching optimisation, not the correctness mechanism: the
     * version stamps are what guarantee a fresh read, and ResolveWorld stays
     * available for random-access queries between phases. Entities are visited
     * in archetype order; no sort is needed because ResolveWorld refreshes a
     * node's ancestors before the node itself.
     */
    class TransformPropagateSystem final : public ISystem
    {
    public:
        void OnStart(SystemContext &context) override;
        void OnUpdate(SystemContext &context) override;

    private:
        Query<Transform, WorldTransform> *mQuery = nullptr;
    };
}
