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
        // Flat, facing +Z
        constexpr glm::vec3 n{0.0f, 0.0f, 1.0f};
        return MeshData{
            {{{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, n},
             {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, n},
             {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, n}},
            {0, 1, 2}};
    }

    MeshData MakeQuad()
    {
        constexpr glm::vec3 n{0.0f, 0.0f, 1.0f};
        return MeshData{
            {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, n},
             {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, n},
             {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, n},
             {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, n}},
            {0, 1, 2, 0, 2, 3}};
    }

    MeshData MakeCube()
    {
        constexpr float h = 0.5f;

        const glm::vec3 faceColors[6]{
            {1.0f, 0.2f, 0.2f}, // +Z front
            {0.2f, 1.0f, 0.2f}, // -Z back
            {0.2f, 0.2f, 1.0f}, // +X right
            {1.0f, 1.0f, 0.2f}, // -X left
            {1.0f, 0.2f, 1.0f}, // +Y top
            {0.2f, 1.0f, 1.0f}, // -Y bottom
        };

        const glm::vec3 corners[6][4]{
            {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},     // +Z
            {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}, // -Z
            {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},     // +X
            {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}, // -X
            {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},     // +Y
            {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}, // -Y
        };

        // One outward normal per face, same order as corners/faceColors above.
        const glm::vec3 faceNormals[6]{
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            {1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
        };

        MeshData mesh;
        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        for (uint32_t face = 0; face < 6; ++face)
        {
            const uint32_t base = face * 4;

            for (uint32_t corner = 0; corner < 4; ++corner)
                mesh.vertices.push_back(Vertex{corners[face][corner], faceColors[face], faceNormals[face]});

            mesh.indices.insert(mesh.indices.end(),
                                {base + 0, base + 1, base + 2,
                                 base + 0, base + 2, base + 3});
        }

        return mesh;
    }
}
