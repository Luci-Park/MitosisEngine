/**
 * @file Shapes.h
 * @author Sumin Park
 * @brief Built-in primitive geometry
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <renderer/Mesh.h>

#include <vector>

namespace mts
{
    // Temporary for sending MeshData -> CreateMesh
    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    /**
     * All three are authored in right-handed, Y-up space, wound
     * counter-clockwise seen from outside. That is the space the rung-4 camera
     * will project from; until it exists they render vertically mirrored and
     * back-face culling must be off. Unit-sized and centred on the origin, so
     * Transform::Scale is the only thing that sets their size.
     */
    MeshData MakeTriangle();
    MeshData MakeQuad();
    MeshData MakeCube();
}
