/**
 * @file Transform.h
 * @author Sumin Park
 * @brief Local translation/rotation/scale for an entity.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "core/ecs/ComponentAsserts.h"
#include "core/ecs/ComponentFields.h"

#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace mts
{
    class Transform
    {
    public:
        Transform() = default;

        explicit Transform(const glm::vec3 &position,
                           const glm::quat &rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           const glm::vec3 &scale = glm::vec3(1.0f))
            : mPosition(position), mRotation(rotation), mScale(scale)
        {
        }

        const glm::vec3 &Position() const { return mPosition; }
        const glm::quat &Rotation() const { return mRotation; }
        const glm::vec3 &Scale() const { return mScale; }

        // Bumped on every mutation. Never 0, so 0 can mean "no such transform".
        uint32_t Version() const { return mVersion; }

        void SetPosition(const glm::vec3 &position)
        {
            mPosition = position;
            Touch();
        }

        void SetRotation(const glm::quat &rotation)
        {
            mRotation = rotation;
            Touch();
        }

        void SetScale(const glm::vec3 &scale)
        {
            mScale = scale;
            Touch();
        }

        void Translate(const glm::vec3 &delta)
        {
            mPosition += delta;
            Touch();
        }

        // Applies `delta` on top of the current rotation, in local space.
        void Rotate(const glm::quat &delta)
        {
            mRotation = glm::normalize(mRotation * delta);
            Touch();
        }

        // Column-major TRS: translate * rotate * scale, applied right to left.
        glm::mat4 Matrix() const
        {
            glm::mat4 m = glm::mat4_cast(mRotation);
            m[0] *= mScale.x;
            m[1] *= mScale.y;
            m[2] *= mScale.z;
            m[3] = glm::vec4(mPosition, 1.0f);
            return m;
        }

    private:
        // 0 is "never written"
        void Touch()
        {
            if (++mVersion == 0)
                mVersion = 1;
        }

        glm::vec3 mPosition{0.0f};
        glm::quat mRotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z - identity
        glm::vec3 mScale{1.0f};
        uint32_t mVersion = 1;
    };

    MTS_ASSERT_COMPONENT(Transform);

    // Transform as a script or inspector sees it
    // Accessor thunks
    inline constexpr FieldDesc kTransformFields[] = {
        {"position", FieldKind::Vec3, 0,
         [](const void *component, void *out)
         { *static_cast<glm::vec3 *>(out) = static_cast<const Transform *>(component)->Position(); },
         [](void *component, const void *in)
         { static_cast<Transform *>(component)->SetPosition(*static_cast<const glm::vec3 *>(in)); }},

        {"rotation", FieldKind::Quat, 0,
         [](const void *component, void *out)
         { *static_cast<glm::quat *>(out) = static_cast<const Transform *>(component)->Rotation(); },
         [](void *component, const void *in)
         { static_cast<Transform *>(component)->SetRotation(*static_cast<const glm::quat *>(in)); }},

        {"scale", FieldKind::Vec3, 0,
         [](const void *component, void *out)
         { *static_cast<glm::vec3 *>(out) = static_cast<const Transform *>(component)->Scale(); },
         [](void *component, const void *in)
         { static_cast<Transform *>(component)->SetScale(*static_cast<const glm::vec3 *>(in)); }},
    };
}
