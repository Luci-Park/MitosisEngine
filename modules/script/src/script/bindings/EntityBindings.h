#pragma once

#include <sol/forward.hpp>

namespace mir
{
    // Registers the Entity usertype
    void RegisterEntityBindings(sol::state &lua);
}
