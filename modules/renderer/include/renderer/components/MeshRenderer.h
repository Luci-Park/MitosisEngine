/**
 * @file MeshRenderer.h
 * @author Sumin Park
 * @brief Marks an entity as drawing a mesh.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"

#include <cstddef>
#include <glm/vec4.hpp>

namespace mts
{
    // the mesh render component
    // null mesh is skipped
    struct MeshRenderer
    {
        MeshHandle mesh;
        glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};

        MaterialHandle material;
    };

    MTS_ASSERT_COMPONENT(MeshRenderer);

    /// Fields for scripts or inspectors
    inline constexpr FieldDesc kMeshRendererFields[] = {
        {"mesh", FieldKind::Handle, offsetof(MeshRenderer, mesh)},
        {"tint", FieldKind::Vec4, offsetof(MeshRenderer, tint)},
        {"material", FieldKind::Handle, offsetof(MeshRenderer, material)},
    };
}

// Table storage: dense like Transform, since most drawable entities carry
// one and RenderSystem sweeps every row.
