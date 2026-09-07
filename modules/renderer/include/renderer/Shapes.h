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
     * All three are authored in right-handed, Y-up space
     * counter-clockwise seen from outside
     */
    MeshData MakeTriangle();
    MeshData MakeQuad();
    MeshData MakeCube();
}
