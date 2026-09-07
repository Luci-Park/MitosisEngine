/**
 * @file InputBindings.h
 * @author Rahul Nair
 * @brief Extends the World usertype with world:is_down/world:axis/etc.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <sol/forward.hpp>

namespace mir
{
    // Must run after RegisterWorldBindings - extends its World usertype
    // rather than creating a new one. Reads InputSystem's InputState
    // resource lazily (via World::TryResource) on every call, so it works
    // regardless of registration-vs-resource-emplacement order.
    void RegisterInputBindings(sol::state &lua);
}
