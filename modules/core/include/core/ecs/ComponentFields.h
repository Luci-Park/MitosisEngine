/**
 * @file ComponentFields.h
 * @author Sumin Park
 * @brief Named, typed field access into a component whose C++ type is erased
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "Entity.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace mts
{
    /// The value types a script or an inspector may read and write. Deliberately
    /// small: every one is trivially copyable and at most 4-byte aligned, so a
    /// component built out of them satisfies 0006 by construction and can never
    /// be over-aligned for ComponentColumn.
    enum class FieldKind : uint8_t
    {
        Bool,
        Int,
        Float,
        Vec3,
        Vec4,
        Quat,
        Mat4,
        EntityRef,

        /// A resource handle shaped {index, generation} - the same layout as
        /// Entity, deliberately kept separate from EntityRef. EntityRef names
        /// a live entity in this World; Handle names something outside the
        /// World entirely (today: a renderer MeshHandle). Conflating the two
        /// would mislead a future binding that special-cases EntityRef with
        /// World-specific behaviour - an "is this entity alive" check, an
        /// entity picker - none of which makes sense for a mesh handle. Sized
        /// directly as two uint32_t rather than naming any owning type, so
        /// core never has to know what a Handle points at; a future asset or
        /// texture handle reuses this kind as long as it keeps the same shape.
        Handle,
    };

    constexpr uint32_t FieldSize(FieldKind kind)
    {
        switch (kind)
        {
        case FieldKind::Bool:
            return sizeof(bool);
        case FieldKind::Int:
            return sizeof(int32_t);
        case FieldKind::Float:
            return sizeof(float);
        case FieldKind::Vec3:
            return sizeof(glm::vec3);
        case FieldKind::Vec4:
            return sizeof(glm::vec4);
        case FieldKind::Quat:
            return sizeof(glm::quat);
        case FieldKind::Mat4:
            return sizeof(glm::mat4);
        case FieldKind::EntityRef:
            return sizeof(Entity);
        case FieldKind::Handle:
            return 2 * sizeof(uint32_t);
        }
        return 0;
    }

    constexpr uint32_t FieldAlign(FieldKind kind)
    {
        switch (kind)
        {
        case FieldKind::Bool:
            return alignof(bool);
        case FieldKind::Int:
            return alignof(int32_t);
        case FieldKind::Float:
            return alignof(float);
        case FieldKind::Vec3:
            return alignof(glm::vec3);
        case FieldKind::Vec4:
            return alignof(glm::vec4);
        case FieldKind::Quat:
            return alignof(glm::quat);
        case FieldKind::Mat4:
            return alignof(glm::mat4);
        case FieldKind::EntityRef:
            return alignof(Entity);
        case FieldKind::Handle:
            return alignof(uint32_t);
        }
        return 1;
    }

    constexpr std::string_view FieldKindName(FieldKind kind)
    {
        switch (kind)
        {
        case FieldKind::Bool:
            return "bool";
        case FieldKind::Int:
            return "int";
        case FieldKind::Float:
            return "float";
        case FieldKind::Vec3:
            return "vec3";
        case FieldKind::Vec4:
            return "vec4";
        case FieldKind::Quat:
            return "quat";
        case FieldKind::Mat4:
            return "mat4";
        case FieldKind::EntityRef:
            return "entity";
        case FieldKind::Handle:
            return "handle";
        }
        return "?";
    }

    /**
     * One reachable value inside a component.
     *
     * Two backings, and the distinction is the whole reason this type is not
     * just an offset:
     *
     * - **Accessor-backed** (`mGet` set). The pair of thunks is the only way in.
     *   Transform is why: its members are private so that Version() cannot fall
     *   behind the data, and WorldTransform detects staleness in O(1) off that
     *   version. A generic offset write to mPosition would leave mVersion
     *   unchanged, so every world matrix downstream keeps the old value - no
     *   crash, no assert, just a wrong frame. Routing through SetPosition keeps
     *   the invariant where the class enforces it, and makes a read-only field
     *   expressible (`mSet == nullptr`) rather than a convention.
     *
     * - **Offset-backed** (`mGet` and `mSet` both null). A script-declared
     *   component is plain data laid out by ComponentRegistry, with no
     *   invariant to protect, so a memcpy at mOffset is both correct and the
     *   cheapest thing available.
     *
     * An aggregate with no user-declared constructors, so a C++ component's
     * table can be a `constexpr FieldDesc[]` sitting next to the class.
     */
    struct FieldDesc
    {
        std::string_view mName;
        FieldKind mKind = FieldKind::Float;

        uint32_t mOffset = 0; ///< byte offset into the component; used when mGet is null

        void (*mGet)(const void *component, void *out) = nullptr;
        void (*mSet)(void *component, const void *in) = nullptr;

        bool ReadOnly() const { return mGet != nullptr && mSet == nullptr; }

        /// Copies FieldSize(mKind) bytes of this field into `out`.
        void Read(const void *component, void *out) const
        {
            if (mGet != nullptr)
                mGet(component, out);
            else
                std::memcpy(out, static_cast<const std::byte *>(component) + mOffset, FieldSize(mKind));
        }

        /// False when the field is read-only, in which case nothing is written.
        bool Write(void *component, const void *in) const
        {
            if (mSet != nullptr)
            {
                mSet(component, in);
                return true;
            }
            if (mGet != nullptr)
                return false; // accessor-backed with no setter

            std::memcpy(static_cast<std::byte *>(component) + mOffset, in, FieldSize(mKind));
            return true;
        }
    };
}
