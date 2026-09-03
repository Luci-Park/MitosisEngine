/**
 * @file Camera.h
 * @author Sumin Park
 * @brief Marks an entity as the viewpoint RenderSystem projects from.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"

#include <cstddef>

namespace mts
{

    struct Camera
    {
        float mFovYDegrees = 60.0f;
        float mNearZ = 0.1f;
        float mFarZ = 100.0f;
    };

    MTS_ASSERT_COMPONENT(Camera);

    inline constexpr FieldDesc kCameraFields[] = {
        {"fovYDegrees", FieldKind::Float, offsetof(Camera, mFovYDegrees)},
        {"nearZ", FieldKind::Float, offsetof(Camera, mNearZ)},
        {"farZ", FieldKind::Float, offsetof(Camera, mFarZ)},
    };
}
