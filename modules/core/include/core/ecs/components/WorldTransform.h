/**
 * @file WorldTransform.h
 * @author Sumin Park
 * @brief Derived world-space matrix plus the stamps that detect staleness.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"

#include <cstdint>
#include <glm/mat4x4.hpp>

namespace mts
{
    namespace detail
    {
        struct TransformResolver;
    }

    /**
     * Cache of `parentWorld * localTransform`, never authored by a game.
     *
     * The two stamps are what make a write O(1). A moved parent bumps only its
     * own Transform::Version; it does not walk its subtree. A child notices on
     * its next read that the parent's Version() no longer matches the one this
     * matrix was built from, and rebuilds then. Staleness therefore flows down
     * lazily, along the chains something actually asks about.
     *
     * Read it through ResolveWorld (core/ecs/TransformHierarchy.h), which
     * refreshes the chain first. Matrix() alone returns whatever was last
     * computed, which is only current after the propagate system has run.
     */
    class WorldTransform
    {
    public:
        WorldTransform() = default;

        /// Last computed world matrix. Current only after a resolve.
        const glm::mat4 &Matrix() const { return mMatrix; }

        /// Bumped whenever mMatrix changes; this is what children stamp against.
        uint32_t Version() const { return mVersion; }

    private:
        friend struct detail::TransformResolver;

        glm::mat4 mMatrix{1.0f};

        /// Transform::Version() this matrix was built from.
        uint32_t mLocalVersion = 0;

        /// Parent's WorldTransform::Version() at build time. 0 = was a root.
        uint32_t mParentVersion = 0;

        /// Never 0, so a child can tell "root" from a real parent version.
        uint32_t mVersion = 1;

        /// Forces the next resolve to rebuild regardless of the stamps. Set on
        /// construction and on reparent, where the stamps alone cannot tell
        /// that anything changed.
        bool mDirty = true;
    };

    MTS_ASSERT_COMPONENT(WorldTransform);

    /// Read-only, expressed as a getter with no setter: a game never authors a
    /// world matrix, it authors a Transform and a parent. Writing here would be
    /// overwritten by the next resolve anyway, so the absent setter says so
    /// instead of letting a script discover it.
    inline constexpr FieldDesc kWorldTransformFields[] = {
        {"matrix", FieldKind::Mat4, 0,
         [](const void *component, void *out)
         { *static_cast<glm::mat4 *>(out) = static_cast<const WorldTransform *>(component)->Matrix(); },
         nullptr},
    };
}

// Table storage, same reasoning as Transform: read by every renderer and
// culling sweep, so it wants to sit in the dense column next to Transform.
