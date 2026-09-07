/**
 * @file InputTypes.h
 * @author Rahul Nair
 * @brief The data model an InputMap is made of: actions and their bindings.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <window/InputCodes.h>

#include <string>
#include <vector>

namespace mir
{
    enum class DeviceKind
    {
        Keyboard,
        Mouse,
        Gamepad,
    };

    enum class InputActionType
    {
        Button,
        Axis1D,
        Axis2D,
    };

    // Which axis a binding feeds - meaningless for Button/Axis1D actions
    // (Axis1D always reads X). A gamepad stick bound to an Axis2D action is
    // two bindings, one per channel, same as a WASD-style Axis1D composite -
    // no separate "2D binding" shape needed.
    enum class AxisChannel
    {
        X,
        Y,
    };

    // One physical source contributing to an action. Only the fields
    // relevant to `device` (and, for Gamepad, to `useGamepadAxis`) matter;
    // the rest sit at their default and are ignored.
    struct InputBinding
    {
        DeviceKind device = DeviceKind::Keyboard;
        AxisChannel channel = AxisChannel::X;

        Key key = Key::Unknown;
        MouseButton mouseButton = MouseButton::Left;

        bool useGamepadAxis = false;
        GamepadButton gamepadButton = GamepadButton::A;
        GamepadAxis gamepadAxis = GamepadAxis::LeftX;
        float deadzone = 0.15f; // only applies when useGamepadAxis is true

        // Sign/magnitude this binding contributes to an Axis1D/Axis2D action
        // when it's a digital source (e.g. D: +1.0, A: -1.0 both feeding
        // "Move"). Ignored for Button actions and for an analog gamepad axis.
        float scale = 1.0f;
    };

    struct InputAction
    {
        std::string name;
        InputActionType type = InputActionType::Button;
        std::vector<InputBinding> bindings;
    };
}
