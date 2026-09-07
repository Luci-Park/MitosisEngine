/**
 * @file Camera.h
 * @author Rahul Nair
 * @brief Marks an entity as the viewpoint RenderSystem projects from.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"

#include <cstddef>

namespace mir
{

    struct Camera
    {
        float mFovYDegrees = 60.0f;
        float mNearZ = 0.1f;
        float mFarZ = 100.0f;
    };

    MIR_ASSERT_COMPONENT(Camera);

    inline constexpr FieldDesc kCameraFields[] = {
        {"fovYDegrees", FieldKind::Float, offsetof(Camera, mFovYDegrees)},
        {"nearZ", FieldKind::Float, offsetof(Camera, mNearZ)},
        {"farZ", FieldKind::Float, offsetof(Camera, mFarZ)},
    };
}
