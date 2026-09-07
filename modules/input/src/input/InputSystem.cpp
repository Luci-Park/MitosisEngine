#include "input/InputSystem.h"

#include <input/InputMap.h>
#include <input/InputState.h>

#include <core/ecs/World.h>
#include <window/Window.h>

#include <algorithm>
#include <cmath>

namespace mir
{
    namespace
    {
        constexpr float kDeviceSwitchDeadzone = 0.2f;

        // First connected pad only - couch-co-op multi-pad support is a
        // later addition (see the input-system plan's open questions).
        float ReadBinding(const InputBinding &binding, const RawInputSnapshot &raw)
        {
            switch (binding.device)
            {
            case DeviceKind::Keyboard:
                return raw.IsDown(binding.key) ? 1.0f : 0.0f;

            case DeviceKind::Mouse:
                return raw.IsDown(binding.mouseButton) ? 1.0f : 0.0f;

            case DeviceKind::Gamepad:
                for (const GamepadState &pad : raw.gamepads)
                {
                    if (!pad.connected)
                        continue;

                    if (binding.useGamepadAxis)
                    {
                        const float value = pad.Axis(binding.gamepadAxis);
                        return std::abs(value) < binding.deadzone ? 0.0f : value;
                    }
                    return pad.IsDown(binding.gamepadButton) ? 1.0f : 0.0f;
                }
                return 0.0f;
            }
            return 0.0f;
        }

        // scale only applies to a digital source composited into an axis
        // (e.g. D:+1/A:-1) - an analog gamepad axis is already signed.
        float ReadAxisContribution(const InputBinding &binding, const RawInputSnapshot &raw)
        {
            const float sourceValue = ReadBinding(binding, raw);
            return binding.useGamepadAxis ? sourceValue : sourceValue * binding.scale;
        }

        bool AnyKeyboardOrMouseActive(const RawInputSnapshot &raw)
        {
            if (std::any_of(raw.keys.begin(), raw.keys.end(), [](bool down)
                             { return down; }))
                return true;
            if (std::any_of(raw.mouseButtons.begin(), raw.mouseButtons.end(), [](bool down)
                             { return down; }))
                return true;
            return raw.scrollX != 0.0 || raw.scrollY != 0.0;
        }

        bool AnyGamepadActive(const RawInputSnapshot &raw)
        {
            for (const GamepadState &pad : raw.gamepads)
            {
                if (!pad.connected)
                    continue;
                if (std::any_of(pad.buttons.begin(), pad.buttons.end(), [](bool down)
                                 { return down; }))
                    return true;
                for (float axis : pad.axes)
                    if (std::abs(axis) > kDeviceSwitchDeadzone)
                        return true;
            }
            return false;
        }
    }

    void InputSystem::OnUpdate(SystemContext &context)
    {
        const RawInputSnapshot &raw = mWindow.RawInput();
        const InputMap &map = context.world.Resource<InputMap>();
        InputState &state = context.world.Resource<InputState>();

        // Seamless device switching: whichever device produced meaningful
        // input most recently is "active". No qualifying input this frame
        // leaves the last active device as-is, rather than flip-flopping.
        if (AnyGamepadActive(raw))
            state.SetActiveDevice(DeviceKind::Gamepad);
        else if (AnyKeyboardOrMouseActive(raw))
            state.SetActiveDevice(DeviceKind::Keyboard);

        for (const InputAction &action : map.Actions())
        {
            ActionValue &value = state.Actions()[action.name];
            const bool wasDown = value.down;

            switch (action.type)
            {
            case InputActionType::Button:
            {
                bool down = false;
                for (const InputBinding &binding : action.bindings)
                    down = down || ReadBinding(binding, raw) != 0.0f;

                value.down = down;
                value.x = down ? 1.0f : 0.0f;
                break;
            }

            case InputActionType::Axis1D:
            {
                float total = 0.0f;
                for (const InputBinding &binding : action.bindings)
                    total += ReadAxisContribution(binding, raw);

                value.x = std::clamp(total, -1.0f, 1.0f);
                value.down = value.x != 0.0f;
                break;
            }

            case InputActionType::Axis2D:
            {
                float totalX = 0.0f;
                float totalY = 0.0f;
                for (const InputBinding &binding : action.bindings)
                {
                    float &target = binding.channel == AxisChannel::Y ? totalY : totalX;
                    target += ReadAxisContribution(binding, raw);
                }

                value.x = std::clamp(totalX, -1.0f, 1.0f);
                value.y = std::clamp(totalY, -1.0f, 1.0f);
                value.down = value.x != 0.0f || value.y != 0.0f;
                break;
            }
            }

            value.justPressed = value.down && !wasDown;
            value.justReleased = !value.down && wasDown;
        }
    }
}
