/**
 * @file HierarchyIndex.h
 * @author Sumin Park
 * @brief The scene graph: parent and child edges, owned as a World resource.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/Entity.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mts
{
    namespace detail
    {
        struct HierarchyMutator;
    }

    /// Chains deeper than this are refused. Every upward walk is bounded by it,
    /// so nothing in the engine has to handle an unbounded chain.
    inline constexpr uint32_t kMaxHierarchyDepth = 64;

    /**
     * Parent and child edges for every entity that has one.
     *
     * This lives as a World resource rather than in components, and that is the
     * whole point of it. A parent link *and* a child list are two encodings of
     * the same edge, and while they were POD fields on a component, the generic
     * ECS could pull them apart without knowing it had: World::RemoveComponent
     * is a template that will happily delete an invariant it has never heard
     * of, and no hook could stop it. Here the redundancy is sealed inside one
     * class, with one set of invariants and nothing else able to reach in - and
     * being a resource, it is free to use std::vector, which the component
     * memcpy contract forbids.
     *
     * Refuses any edge that would create a cycle or put *any* node past
     * kMaxHierarchyDepth - the moved subtree's own height is counted, not just
     * the new parent's depth - so every walk in the engine may assume a bounded
     * acyclic graph in every build, not only where asserts are compiled in.
     *
     * Entity lifetime is not its business: it stores handles and never asks
     * whether they are alive. Remove is called by the destroy hook in
     * TransformHierarchy, which is the one place lifetime and structure meet.
     */
    class HierarchyIndex
    {
    public:
        /// Null when `entity` is a root or absent from the graph.
        Entity ParentOf(Entity entity) const;

        /// Direct children in insertion order, which is stable: unlike the
        /// intrusive chain this replaced, iteration order is reproducible
        /// across runs, so serialization and scripting may rely on it.
        ///
        /// Invalidated by any mutation of this entity's children.
        std::span<const Entity> ChildrenOf(Entity entity) const;

        /// True if `ancestor` is `entity` or any transitive parent of it.
        bool IsAncestorOf(Entity ancestor, Entity entity) const;

        /// Levels above `entity`. 0 for a root.
        uint32_t DepthOf(Entity entity) const;

        /// Levels below `entity`. 0 for a leaf.
        uint32_t HeightOf(Entity entity) const;

        /// Entities the graph currently holds a slot for. An entity that was
        /// linked and then rooted keeps its slot, so this is neither the
        /// world's entity count nor the count of non-root entities.
        std::size_t NodeCount() const;

    private:
        // Mutating is private so that mts::SetParent is the only way in. It
        // pairs every structural change with the WorldTransform invalidation
        // that a reparent needs - the version stamps cannot see a reparent on
        // their own - and a caller reaching the graph directly would silently
        // skip it and leave a permanently stale matrix.
        friend struct detail::HierarchyMutator;

        /**
         * Links `child` under `parent`, or roots it when `parent` is null.
         *
         * False when refused - self-parenting, a cycle, or a chain that would
         * pass kMaxHierarchyDepth - in which case nothing was changed.
         */
        bool SetParent(Entity child, Entity parent);

        /// Detaches `entity` and roots whatever was below it, then forgets it.
        /// The destroy hook calls this after it has cascaded, so in practice
        /// there are no children left to orphan.
        void Remove(Entity entity);

        /**
         * Roots every child of `entity` and hands the list over, leaving it
         * childless.
         *
         * This is what keeps a cascading destroy linear. Removing children one
         * at a time makes each Unlink scan and shift the parent's vector, so
         * tearing down a node with N children costs O(N^2) - 279ms for 40,000
         * on the machine this was written on. Detaching them in one pass, and
         * moving the buffer out rather than copying it, makes the whole cascade
         * O(total nodes) with no allocation.
         */
        std::vector<Entity> TakeChildren(Entity entity);

        struct Node
        {
            /// Null marks the slot unused. Holding the whole handle rather than
            /// a flag is what makes a recycled index self-invalidating: a stale
            /// entity compares unequal on generation and reads as absent.
            Entity owner;

            Entity parent;
            std::vector<Entity> children;
        };

        uint32_t HeightFrom(Entity entity, uint32_t guard) const;

        Node *Find(Entity entity);
        const Node *Find(Entity entity) const;

        /// The slot for `entity`, creating it if needed.
        Node &Obtain(Entity entity);

        void Unlink(Entity child, Entity parent);

        /// Dense, indexed by Entity::mIndex, so a lookup is a bounds check and
        /// a handle compare rather than a hash.
        std::vector<Node> mNodes;
    };
}
