/**
 * @file MeshRenderer.h
 * @author Rahul Nair
 * @brief Marks an entity as drawing a mesh.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"
#include "renderer/Mesh.h"

#include <cstddef>
#include <glm/vec4.hpp>

namespace mts
{
    /**
     * Replaces TriangleRenderer: a mesh handle plus a tint, instead of a bare
     * tag naming the one built-in shape. Lives in the renderer module, not
     * core - core must not learn what a mesh is, and engine_renderer already
     * links mts::core publicly, so the dependency only goes one way.
     *
     * A null mesh is not an error at the component level: RenderSystem simply
     * skips the item, the same way FindMesh returns nullptr for a handle that
     * does not resolve. An entity may carry this component before its mesh
     * has loaded.
     */
    struct MeshRenderer
    {
        MeshHandle mesh;
        glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    };

    MTS_ASSERT_COMPONENT(MeshRenderer);

    /**
     * MeshRenderer as a script or an inspector sees it.
     *
     * Offset-backed, not accessor-backed like Transform's field table: both
     * members are plain public data with no invariant to protect, so a
     * memcpy at the member's offset is correct and is what FieldDesc does by
     * default when mGet/mSet are left null.
     *
     * "mesh" is FieldKind::Handle rather than a type of its own - see that
     * kind's comment in ComponentFields.h for why it is not EntityRef. A
     * script that reads it back gets the raw {index, generation} pair, not a
     * usable handle on its own; resolving a mesh by name is future work, once
     * meshes are assets rather than something only CreateMesh can produce.
     */
    inline constexpr FieldDesc kMeshRendererFields[] = {
        {"mesh", FieldKind::Handle, offsetof(MeshRenderer, mesh)},
        {"tint", FieldKind::Vec4, offsetof(MeshRenderer, tint)},
    };
}

// Table storage: dense like Transform, since most drawable entities carry
// one and RenderSystem sweeps every row.
