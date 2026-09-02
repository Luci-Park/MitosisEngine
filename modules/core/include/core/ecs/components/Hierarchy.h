/**
 * @file Hierarchy.h
 * @author Sumin Park
 * @brief Scene-graph links: parent, first child, and the sibling chain.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/Entity.h"
#include "core/ecs/StorageInfo.h"

namespace mts
{
    namespace detail
    {
        struct HierarchyLinker;
    }

    /**
     * All four scene-graph links in one component.
     *
     * The upward link alone would be enough to resolve a transform, but not to
     * answer "what is below me" without scanning every entity. The children of
     * a node are stored as an intrusive singly-linked chain instead of a list:
     * a std::vector cannot be a component here (ComponentColumn relocates rows
     * with memcpy, so components must stay trivially copyable), and a fixed
     * array would pick a fan-out limit.
     *
     *     parent.mFirstChild -> a -> b -> c -> null      (via mNextSibling)
     *
     * mPrevSibling is redundant for walking but makes unlinking a child O(1)
     * rather than a scan from the head.
     *
     * The fields are private because they encode the same edges twice: the
     * chains must agree with every mParent, and the only code allowed to change
     * them is SetParent (through HierarchyLinker). Handing out a mutable
     * reference would make consistency something a caller has to remember,
     * which is exactly what this component exists to avoid.
     *
     * Child order is insertion order reversed and is not stable across a
     * RepairHierarchy, so nothing may depend on it.
     */
    class Hierarchy
    {
    public:
        Hierarchy() = default;

        Entity Parent() const { return mParent; }
        Entity FirstChild() const { return mFirstChild; }
        Entity NextSibling() const { return mNextSibling; }
        Entity PrevSibling() const { return mPrevSibling; }

        bool IsRoot() const { return mParent.IsNull(); }
        bool HasChildren() const { return !mFirstChild.IsNull(); }

    private:
        friend struct detail::HierarchyLinker;

        Entity mParent;
        Entity mFirstChild;
        Entity mNextSibling;
        Entity mPrevSibling;
    };

    MTS_ASSERT_COMPONENT(Hierarchy);
}

// Table storage, and added to every transform entity whether it is parented or
// not. The 32 bytes a lone root "wastes" buy two things:
//
//   - Reparenting never changes an archetype. If the component came and went
//     with the link, the first SetParent and every rooting would move the
//     entity to another table and memcpy its Transform and WorldTransform
//     along with it. With it always present, both are field writes.
//   - One archetype shape for scene objects instead of one per combination of
//     present/absent links, so queries over transforms stay unfragmented.
