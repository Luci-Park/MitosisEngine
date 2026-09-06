#include "EntityBindings.h"

#include <core/ecs/Entity.h>

#include <sol/sol.hpp>

namespace mts
{
    void RegisterEntityBindings(sol::state &lua)
    {
        lua.new_usertype<Entity>("Entity",
                                  "is_null", &Entity::IsNull,
                                  sol::meta_function::equal_to, [](const Entity &a, const Entity &b)
                                  { return a == b; });
    }
}
