/**
 * @file InputSnapshot.h
 * @author Rahul Nair
 * @brief Raw per-frame device state - current physical state only, no edge
 *        detection or action mapping (that's the input module's job).
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include "window/InputCodes.h"

#include <array>
#include <cstddef>

namespace mir
{
    struct GamepadState
    {
        bool connected = false;
        std::array<bool, kGamepadButtonCount> buttons{};
        std::array<float, kGamepadAxisCount> axes{};

        bool IsDown(GamepadButton button) const { return buttons[static_cast<size_t>(button)]; }
        float Axis(GamepadAxis axis) const { return axes[static_cast<size_t>(axis)]; }
    };

    struct RawInputSnapshot
    {
        std::array<bool, kKeyCount> keys{};
        std::array<bool, kMouseButtonCount> mouseButtons{};

        double mouseX = 0.0;
        double mouseY = 0.0;

        double scrollX = 0.0;
        double scrollY = 0.0;

        std::array<GamepadState, kGamepadSlotCount> gamepads{};

        bool IsDown(Key key) const { return keys[static_cast<size_t>(key)]; }
        bool IsDown(MouseButton button) const { return mouseButtons[static_cast<size_t>(button)]; }
    };
}
