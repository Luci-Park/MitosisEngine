// Vulkan clip space is 0..1, not OpenGL's -1..1: this must be defined before
// matrix_transform.hpp is parsed, in this translation unit specifically, so
// glm::perspective builds the matrix this engine's pipeline actually expects.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "renderer/CameraMath.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mts
{
    glm::mat4 MakeViewProjection(const glm::mat4 &cameraWorld,
                                 float fovYDegrees, float nearZ, float farZ,
                                 float aspectRatio)
    {
        const glm::mat4 view = glm::inverse(cameraWorld);

        glm::mat4 proj = glm::perspective(glm::radians(fovYDegrees), aspectRatio, nearZ, farZ);

        // glm = Y up
        // vulkan = Y down
        // flip accordingly
        proj[1][1] *= -1.0f;

        return proj * view;
    }
}
