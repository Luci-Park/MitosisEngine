#include "renderer/ComponentRegistration.h"

#include "renderer/components/Camera.h"
#include "renderer/components/MeshRenderer.h"

#include <core/ecs/ComponentRegistry.h>

namespace mts
{
    void RegisterRendererComponents()
    {
        ComponentRegistry::Instance().Register<MeshRenderer>(kMeshRendererFields);
        ComponentRegistry::Instance().Register<Camera>(kCameraFields);
    }
}
