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

#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace mts
{
    /**
     * The transform a game *writes*. Stored as TRS rather than a mat4 because
     * TRS is what gameplay edits, is smaller, and cannot drift into a
     * non-affine or sheared state the way a hand-edited matrix can.
     *
     * Members are private and reached through mutators only so that Version()
     * cannot fall behind the data. World::Get<T> hands out a raw T*, so there
     * is no hook on the ECS side to stamp a write - the component is the last
     * place the invariant can be enforced rather than remembered. That version
     * is what lets WorldTransform detect staleness in O(1) instead of the
     * parent having to dirty its whole subtree.
     *
     * Still trivially copyable and standard layout: all members share one
     * access level, and copy/move/destroy stay defaulted, so ComponentColumn
     * may keep relocating rows with memcpy.
     */
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

        /// Bumped on every mutation. Never 0, so 0 can mean "no such transform".
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

        /// Applies `delta` on top of the current rotation, in local space.
        void Rotate(const glm::quat &delta)
        {
            mRotation = glm::normalize(mRotation * delta);
            Touch();
        }

        /// Column-major TRS: translate * rotate * scale, applied right to left.
        glm::mat4 Matrix() const
        {
            // mat4_cast builds the rotation basis, then each basis column is
            // scaled in place. Cheaper than mat4(1) * translate * rotate *
            // scale, which would be three full 4x4 multiplies for a result
            // whose bottom row is always (0,0,0,1).
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
        glm::quat mRotation{1.0f, 0.0f, 0.0f, 0.0f}; ///< w, x, y, z - identity
        glm::vec3 mScale{1.0f};
        uint32_t mVersion = 1;
    };

    MTS_ASSERT_COMPONENT(Transform);
}

// Storage is left at the Table default: a transform is dense - most entities
// have one and systems sweep every row - which is the case tables are for.
