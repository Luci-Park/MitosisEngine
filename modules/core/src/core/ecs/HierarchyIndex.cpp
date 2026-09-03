/**
 * @file HierarchyIndex.cpp
 * @author Sumin Park
 * @brief The scene graph: parent and child edges, owned as a World resource.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/HierarchyIndex.h"

#include "core/log/Assert.h"

#include <algorithm>

namespace mts
{
    HierarchyIndex::Node *HierarchyIndex::Find(Entity entity)
    {
        if (entity.IsNull() || entity.mIndex >= mNodes.size())
            return nullptr;

        Node &node = mNodes[entity.mIndex];
        return node.owner == entity ? &node : nullptr;
    }

    const HierarchyIndex::Node *HierarchyIndex::Find(Entity entity) const
    {
        if (entity.IsNull() || entity.mIndex >= mNodes.size())
            return nullptr;

        const Node &node = mNodes[entity.mIndex];
        return node.owner == entity ? &node : nullptr;
    }

    HierarchyIndex::Node &HierarchyIndex::Obtain(Entity entity)
    {
        if (entity.mIndex >= mNodes.size())
            mNodes.resize(entity.mIndex + 1);

        Node &node = mNodes[entity.mIndex];

        // A different owner means the slot belongs to a destroyed entity whose
        // index was recycled. Its edges died with it, so the slot is reset
        // rather than inherited.
        if (!(node.owner == entity))
        {
            node.owner = entity;
            node.parent = kNullEntity;
            node.children.clear();
        }

        return node;
    }

    Entity HierarchyIndex::ParentOf(Entity entity) const
    {
        const Node *node = Find(entity);
        return node ? node->parent : kNullEntity;
    }

    std::span<const Entity> HierarchyIndex::ChildrenOf(Entity entity) const
    {
        const Node *node = Find(entity);
        return node ? std::span<const Entity>(node->children) : std::span<const Entity>{};
    }

    uint32_t HierarchyIndex::DepthOf(Entity entity) const
    {
        uint32_t depth = 0;
        for (Entity cursor = ParentOf(entity); !cursor.IsNull(); cursor = ParentOf(cursor))
        {
            // Bounded even though SetParent refuses to build past the cap: this
            // is also what stops a walk if the graph is ever wrong.
            if (++depth >= kMaxHierarchyDepth)
                break;
        }

        return depth;
    }

    uint32_t HierarchyIndex::HeightFrom(Entity entity, uint32_t guard) const
    {
        const Node *node = Find(entity);
        if (node == nullptr || guard >= kMaxHierarchyDepth)
            return 0;

        uint32_t best = 0;
        for (const Entity child : node->children)
            best = std::max(best, 1 + HeightFrom(child, guard + 1));

        return best;
    }

    uint32_t HierarchyIndex::HeightOf(Entity entity) const
    {
        // Recursion is safe here only because the graph is already bounded:
        // nothing may be linked past kMaxHierarchyDepth, and the guard stops
        // the walk anyway if that were ever violated.
        return HeightFrom(entity, 0);
    }

    bool HierarchyIndex::IsAncestorOf(Entity ancestor, Entity entity) const
    {
        if (ancestor.IsNull() || entity.IsNull())
            return false;

        uint32_t depth = 0;
        for (Entity cursor = entity; !cursor.IsNull(); cursor = ParentOf(cursor))
        {
            if (cursor == ancestor)
                return true;

            if (++depth >= kMaxHierarchyDepth)
                break;
        }

        return false;
    }

    void HierarchyIndex::Unlink(Entity child, Entity parent)
    {
        Node *node = Find(parent);
        if (node == nullptr)
            return;

        // Order-preserving erase, not swap-and-pop: children stay in insertion
        // order, which is what makes iteration reproducible. Both are O(n) in
        // the child count anyway, since the search is the cost.
        const auto found = std::find(node->children.begin(), node->children.end(), child);
        if (found != node->children.end())
            node->children.erase(found);
    }

    bool HierarchyIndex::SetParent(Entity child, Entity parent)
    {
        if (child.IsNull())
            return false;

        if (!parent.IsNull())
        {
            if (child == parent)
                return false;

            // Refused, not asserted: one cycle would make every upward walk in
            // the engine non-terminating, and an assert is compiled out under
            // NDEBUG exactly where that matters most.
            if (IsAncestorOf(child, parent))
                return false;

            // The moved subtree's own height counts, not just the new
            // parent's depth: bounding one end only would let a tall subtree
            // land past the cap, and ResolveWorld would then silently truncate
            // its ancestor walk and return a matrix missing the top levels.
            // O(subtree), paid only when the moved node actually has children.
            if (DepthOf(parent) + 1 + HeightOf(child) >= kMaxHierarchyDepth)
                return false;
        }

        const Entity previous = ParentOf(child);
        if (previous == parent)
            return true;

        if (!previous.IsNull())
            Unlink(child, previous);

        // Obtain may resize mNodes, so the parent's slot is taken afterwards
        // and never held across the child's.
        Obtain(child).parent = parent;

        if (!parent.IsNull())
            Obtain(parent).children.push_back(child);

        return true;
    }

    std::vector<Entity> HierarchyIndex::TakeChildren(Entity entity)
    {
        Node *node = Find(entity);
        if (node == nullptr)
            return {};

        // Rooted first, so that destroying one of them afterwards finds a null
        // parent and skips the Unlink scan entirely.
        for (const Entity child : node->children)
        {
            if (Node *orphan = Find(child))
                orphan->parent = kNullEntity;
        }

        // Moved, not copied: the buffer transfers, so this allocates nothing.
        return std::move(node->children);
    }

    void HierarchyIndex::Remove(Entity entity)
    {
        Node *node = Find(entity);
        if (node == nullptr)
            return;

        const Entity parent = node->parent;

        if (!parent.IsNull())
            Unlink(entity, parent);

        // Iterated in place rather than copied out: nothing below resizes
        // mNodes - Unlink and Find only read the slot vector - so `node` stays
        // valid, and the graph is acyclic so no child can be `parent`.
        for (const Entity child : node->children)
        {
            if (Node *orphan = Find(child))
                orphan->parent = kNullEntity;
        }

        node->owner = kNullEntity;
        node->parent = kNullEntity;

        // clear() rather than shrinking: the slot will likely be reused by a
        // recycled index, and the capacity is worth keeping.
        node->children.clear();
    }

    std::size_t HierarchyIndex::NodeCount() const
    {
        std::size_t count = 0;
        for (const Node &node : mNodes)
        {
            if (!node.owner.IsNull())
                ++count;
        }

        return count;
    }
}
