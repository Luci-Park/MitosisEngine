/**
 * @file Mesh.h
 * @author Rahul Nair
 * @brief Vertex layout and the handle that names an uploaded mesh.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace mts
{
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    struct MeshHandle
    {
        static constexpr uint32_t kNullIndex = UINT32_MAX;

        uint32_t mIndex = kNullIndex;
        uint32_t mGeneration = 0;

        constexpr bool IsNull() const { return mIndex == kNullIndex; }
        constexpr bool operator==(const MeshHandle &) const = default;
    };

    inline constexpr MeshHandle kNullMesh{};
}
