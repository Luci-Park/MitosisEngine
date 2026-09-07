/**
 * @file InputState.h
 * @author Rahul Nair
 * @brief Resolved per-action input, written each frame by InputSystem and
 *        read by everything else (Lua bindings, gameplay code, editor).
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <input/InputTypes.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace mir
{
    struct ActionValue
    {
        bool down = false;
        bool justPressed = false;
        bool justReleased = false;
        float x = 0.0f; // Axis1D's only component; Axis2D's horizontal one
        float y = 0.0f; // Axis2D only
    };

    // A World resource (see World::EmplaceResource) - one lives on the App's
    // World, written by InputSystem in SystemPhase::PreUpdate before
    // ScriptSystem runs, so scripts see this frame's settled state.
    class InputState
    {
    public:
        const ActionValue &Get(std::string_view action) const
        {
            static const ActionValue kZero{};
            const auto it = mActions.find(std::string(action));
            return it != mActions.end() ? it->second : kZero;
        }

        bool IsDown(std::string_view action) const { return Get(action).down; }
        bool JustPressed(std::string_view action) const { return Get(action).justPressed; }
        bool JustReleased(std::string_view action) const { return Get(action).justReleased; }
        float Axis(std::string_view action) const { return Get(action).x; }

        DeviceKind ActiveDevice() const { return mActiveDevice; }

        // InputSystem-only write access; everyone else reads through Get()/
        // IsDown()/etc above.
        std::unordered_map<std::string, ActionValue> &Actions() { return mActions; }
        void SetActiveDevice(DeviceKind device) { mActiveDevice = device; }

    private:
        std::unordered_map<std::string, ActionValue> mActions;
        DeviceKind mActiveDevice = DeviceKind::Keyboard;
    };
}
