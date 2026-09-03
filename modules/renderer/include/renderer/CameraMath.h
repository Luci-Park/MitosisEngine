/**
 * @file CameraMath.h
 * @author Rahul Nair
 * @brief Builds the view-projection matrix RenderSystem premultiplies into
 *        every draw item.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <glm/mat4x4.hpp>

namespace mts
{
    glm::mat4 MakeViewProjection(const glm::mat4 &cameraWorld,
                                 float fovYDegrees, float nearZ, float farZ,
                                 float aspectRatio);
}
