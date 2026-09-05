#include "WorldBindings.h"

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/World.h>
#include <core/log/Log.h>

#include <glm/gtc/type_ptr.hpp>

#include <sol/sol.hpp>

namespace mts
{
    namespace
    {
        /// Mirrors FieldKind::Handle's layout ({index, generation}, 8 bytes) -
        /// core never names what a Handle points at, so neither does this.
        struct RawHandle
        {
            uint32_t index = 0;
            uint32_t generation = 0;
        };

        sol::object FieldToLua(sol::state_view lua, const FieldDesc &field, const void *component)
        {
            switch (field.mKind)
            {
            case FieldKind::Bool:
            {
                bool value = false;
                field.Read(component, &value);
                return sol::make_object(lua, value);
            }
            case FieldKind::Int:
            {
                int32_t value = 0;
                field.Read(component, &value);
                return sol::make_object(lua, value);
            }
            case FieldKind::Float:
            {
                float value = 0.0f;
                field.Read(component, &value);
                return sol::make_object(lua, value);
            }
            case FieldKind::Vec3:
            {
                glm::vec3 value{};
                field.Read(component, &value);
                sol::table out = lua.create_table();
                out["x"] = value.x;
                out["y"] = value.y;
                out["z"] = value.z;
                return out;
            }
            case FieldKind::Vec4:
            {
                glm::vec4 value{};
                field.Read(component, &value);
                sol::table out = lua.create_table();
                out["x"] = value.x;
                out["y"] = value.y;
                out["z"] = value.z;
                out["w"] = value.w;
                return out;
            }
            case FieldKind::Quat:
            {
                glm::quat value{};
                field.Read(component, &value);
                sol::table out = lua.create_table();
                out["x"] = value.x;
                out["y"] = value.y;
                out["z"] = value.z;
                out["w"] = value.w;
                return out;
            }
            case FieldKind::Mat4:
            {
                glm::mat4 value{};
                field.Read(component, &value);
                sol::table out = lua.create_table();
                const float *m = glm::value_ptr(value);
                for (int i = 0; i < 16; ++i)
                    out[i + 1] = m[i];
                return out;
            }
            case FieldKind::EntityRef:
            {
                Entity value{};
                field.Read(component, &value);
                return sol::make_object(lua, value);
            }
            case FieldKind::Handle:
            {
                RawHandle value{};
                field.Read(component, &value);
                sol::table out = lua.create_table();
                out["index"] = value.index;
                out["generation"] = value.generation;
                return out;
            }
            }
            return sol::nil;
        }

        bool WorldHas(World &world, Entity entity, std::string_view componentName)
        {
            const ComponentOps *ops = ComponentRegistry::Instance().Find(componentName);
            if (ops == nullptr)
            {
                MTS_LOG_ERROR("script: world:has unknown component '{}'", componentName);
                return false;
            }
            return ops->Has(world, entity);
        }

        sol::object WorldGet(World &world, Entity entity, std::string_view componentName, sol::this_state state)
        {
            sol::state_view lua(state);

            const ComponentOps *ops = ComponentRegistry::Instance().Find(componentName);
            if (ops == nullptr)
            {
                MTS_LOG_ERROR("script: world:get unknown component '{}'", componentName);
                return sol::nil;
            }

            void *component = ops->Get(world, entity);
            if (component == nullptr)
                return sol::nil;

            sol::table out = lua.create_table();
            for (const FieldDesc &field : ops->mFields)
                out[field.mName] = FieldToLua(lua, field, component);
            return out;
        }
    }

    void RegisterWorldBindings(sol::state &lua)
    {
        lua.new_usertype<World>("World",
                                 "has", &WorldHas,
                                 "get", &WorldGet);
    }
}
