/**
 * @file Shapes.cpp
 * @author Sumin Park
 * @brief Built-in primitive geometry, as plain data.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "renderer/Shapes.h"

namespace mts
{
    MeshData MakeTriangle()
    {
        return MeshData{
            {{{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
             {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
             {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}},
            {0, 1, 2}};
    }

    MeshData MakeQuad()
    {
        // Four corners, six indices: the two triangles share the 0-2 diagonal,
        // which is the whole reason an index buffer arrives at this rung.
        return MeshData{
            {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
             {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
             {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
             {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}}},
            {0, 1, 2, 0, 2, 3}};
    }

    MeshData MakeCube()
    {
        // 24 vertices, not 8: a shared corner would have to carry one colour
        // for three faces. Duplicating per face is what lets each face be flat
        // coloured, and it is the same duplication a normal or a UV will force
        // later anyway.
        constexpr float h = 0.5f;

        const glm::vec3 faceColors[6]{
            {1.0f, 0.2f, 0.2f}, // +Z front
            {0.2f, 1.0f, 0.2f}, // -Z back
            {0.2f, 0.2f, 1.0f}, // +X right
            {1.0f, 1.0f, 0.2f}, // -X left
            {1.0f, 0.2f, 1.0f}, // +Y top
            {0.2f, 1.0f, 1.0f}, // -Y bottom
        };

        // Each row is one face's four corners, ordered counter-clockwise as
        // seen from outside the cube. Verified by (v1-v0) x (v2-v0) pointing
        // along the face's outward normal - do that check by hand if you ever
        // edit this table, because a flipped face is invisible with culling
        // off and only appears as a hole once rung 4 turns culling back on.
        const glm::vec3 corners[6][4]{
            {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},     // +Z
            {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}, // -Z
            {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},     // +X
            {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}, // -X
            {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},     // +Y
            {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}, // -Y
        };

        MeshData mesh;
        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        for (uint32_t face = 0; face < 6; ++face)
        {
            const uint32_t base = face * 4;

            for (uint32_t corner = 0; corner < 4; ++corner)
                mesh.vertices.push_back(Vertex{corners[face][corner], faceColors[face]});

            // Same fan every face, because every row above is ordered the same
            // way. One winding mistake to make instead of six.
            mesh.indices.insert(mesh.indices.end(),
                                {base + 0, base + 1, base + 2,
                                 base + 0, base + 2, base + 3});
        }

        return mesh;
    }
}
