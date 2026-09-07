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

namespace mir
{
    namespace detail
    {
        struct TransformResolver;
    }

    // Read it through ResolveWorld (core/ecs/TransformHierarchy.h)
    class WorldTransform
    {
    public:
        WorldTransform() = default;

        // Last computed world matrix. Current only after a resolve.
        const glm::mat4 &Matrix() const { return mMatrix; }

        // Bumped whenever mMatrix changes; this is what children stamp against.
        uint32_t Version() const { return mVersion; }

    private:
        friend struct detail::TransformResolver;

        glm::mat4 mMatrix{1.0f};

        // Transform::Version() this matrix was built from.
        uint32_t mLocalVersion = 0;

        // Parent's WorldTransform::Version() at build time. 0 = was a root.
        uint32_t mParentVersion = 0;

        // Never 0, so a child can tell "root" from a real parent version.
        uint32_t mVersion = 1;

        // Forces the next resolve to rebuild regardless of the stamps.
        // Set on construction and on reparent
        bool mDirty = true;
    };

    MIR_ASSERT_COMPONENT(WorldTransform);

    // only getter, set is only to be done by Hierarchy System
    inline constexpr FieldDesc kWorldTransformFields[] = {
        {"matrix", FieldKind::Mat4, 0,
         [](const void *component, void *out)
         { *static_cast<glm::mat4 *>(out) = static_cast<const WorldTransform *>(component)->Matrix(); },
         nullptr},
    };
}
