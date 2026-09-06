#pragma once

#include <sol/forward.hpp>

namespace mts
{
    // Registers the World usertype: world:has/world:get
    void RegisterWorldBindings(sol::state &lua);
}
