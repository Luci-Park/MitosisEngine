/**
 * @file TriangleRenderer.h
 * @author Sumin Park
 * @brief Marks an entity as drawing the built-in triangle.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/ComponentAsserts.h"

namespace mts
{
    /**
     * Placeholder for the MeshRenderer this becomes once geometry is an asset:
     * there is exactly one mesh in the engine right now, so the component only
     * has to say "draw it here" and the transform comes from WorldTransform.
     *
     * A tag with no fields. It exists so the draw list is a query result rather
     * than every entity that happens to own a Transform - a camera or a spawn
     * point has a transform and must not be drawn.
     */
    struct TriangleRenderer
    {
    };

    MTS_ASSERT_COMPONENT(TriangleRenderer);
}

// Table storage even though it carries no data: a tag in the archetype
// signature is matched once per table, so the draw query rejects whole
// archetypes at a time instead of testing every entity individually.
