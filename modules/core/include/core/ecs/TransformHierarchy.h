/**
 * @file TransformHierarchy.h
 * @author Sumin Park
 * @brief Resolving Transform + Hierarchy into WorldTransform.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/Entity.h"
#include "core/ecs/System.h"
#include "core/ecs/World.h"
#include "core/ecs/components/Hierarchy.h"
#include "core/ecs/components/Transform.h"
#include "core/ecs/components/WorldTransform.h"

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <vector>

namespace mts
{
    /// Chains deeper than this are treated as a bug (usually a parent cycle).
    inline constexpr uint32_t kMaxHierarchyDepth = 64;

    /**
     * World matrix of `entity`, refreshing it and every stale ancestor first.
     *
     * This is the answer to "something asks between schedules": a read is what
     * triggers the recompute, so a read cannot observe a stale value. Correct
     * at any point in the frame, in any phase, with no ordering discipline
     * required from the caller.
     *
     * Cost is O(depth). A clean chain costs two integer compares per level and
     * touches no matrix maths. Entities with a WorldTransform keep the result;
     * entities without one still get a correct matrix, just uncached.
     *
     * A parent that is not alive is treated as no parent. With the destroy
     * hook installed nothing outlives its parent, but the check is kept so a
     * World that never called InstallHierarchyHooks degrades to resolving in
     * place rather than following a dangling handle.
     */
    glm::mat4 ResolveWorld(World &world, Entity entity);

    /// Adds Transform, Hierarchy (rooted unless `parent` is given) and the
    /// WorldTransform cache together. Adding a bare Transform still works - it
    /// just resolves uncached, and is always treated as a root.
    ///
    /// Idempotent: calling it again overwrites the Transform and re-links, so
    /// it is safe on an entity that already exists in a hierarchy. The link
    /// goes through SetParent and gets the same cycle and depth rejection.
    Transform &AddTransform(World &world,
                            Entity entity,
                            const Transform &transform = Transform{},
                            Entity parent = kNullEntity);

    /**
     * Parents `child` to `parent`, or roots it when `parent` is null.
     *
     * O(1): unlink from the old parent's chain, push onto the front of the new
     * one. Neither direction moves the entity between archetypes.
     *
     * Rejects, rather than only asserting on, a dead parent, self-parenting, a
     * cycle, and a chain that would pass kMaxHierarchyDepth. Those checks are
     * what let every upward walk in this header assume a bounded acyclic graph.
     * Note the depth bound looks only at the new parent, so moving an already
     * tall subtree under a deep node can still overshoot; ResolveWorld asserts
     * if it ever sees that.
     *
     * The child's local Transform is left untouched, so it snaps to being
     * relative to its new parent rather than holding its world pose. Preserving
     * world pose across a reparent is a separate operation, not yet written.
     */
    void SetParent(World &world, Entity child, Entity parent);

    /// True if `ancestor` is `entity` or any transitive parent of it.
    bool IsAncestorOf(World &world, Entity ancestor, Entity entity);

    /**
     * Makes World::DestroyEntity cascade: destroying an entity destroys
     * everything below it and unlinks it from its parent's chain.
     *
     * Installed as a World destroy hook rather than built into World, which
     * knows nothing about Hierarchy. There is no separate DestroyHierarchy
     * because there is no longer anything for it to do that the primitive does
     * not - and that includes CommandBuffer::Destroy, which flushes through the
     * same call.
     *
     * Idempotent and called from AddTransform and SetParent, so a World that
     * has ever held a hierarchy already has this; call it directly only to arm
     * a World before the first entity is built.
     */
    void InstallHierarchyHooks(World &world);

    /**
     * Calls `fn(Entity)` for each direct child of `parent`, nearest insertion
     * last. Safe to reparent or destroy the visited child inside `fn` - the
     * next link is read before `fn` runs.
     */
    template <typename Fn>
    void ForEachChild(World &world, Entity parent, Fn &&fn)
    {
        const Hierarchy *node = world.Get<Hierarchy>(parent);
        if (node == nullptr)
            return;

        Entity child = node->FirstChild();
        while (!child.IsNull())
        {
            const Hierarchy *link = world.Get<Hierarchy>(child);
            const Entity next = link ? link->NextSibling() : kNullEntity;

            fn(child);
            child = next;
        }
    }

    /**
     * Calls `fn(Entity)` for every entity below `root`, parents before
     * children, excluding `root` itself.
     *
     * The whole subtree is collected before `fn` sees any of it, so `fn` may
     * reparent or destroy freely without invalidating the walk.
     */
    template <typename Fn>
    void ForEachDescendant(World &world, Entity root, Fn &&fn)
    {
        std::vector<Entity> pending;
        ForEachChild(world, root, [&pending](Entity child) { pending.push_back(child); });

        for (std::size_t i = 0; i < pending.size(); ++i)
            ForEachChild(world, pending[i], [&pending](Entity child) { pending.push_back(child); });

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
