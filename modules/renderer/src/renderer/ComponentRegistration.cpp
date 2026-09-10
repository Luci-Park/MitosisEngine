/**
 * @file ComponentRegistration.cpp
 * @author Rahul Nair
 * @brief Registers every component the renderer module defines.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "renderer/ComponentRegistration.h"

#include "renderer/components/Camera.h"
#include "renderer/components/MeshRenderer.h"

#include <core/ecs/ComponentRegistry.h>

namespace mir
{
    void RegisterRendererComponents()
    {
        ComponentRegistry::Instance().Register<MeshRenderer>(kMeshRendererFields);
        ComponentRegistry::Instance().Register<Camera>(kCameraFields);
    }
}
