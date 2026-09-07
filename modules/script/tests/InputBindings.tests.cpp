/**
 * @file InputBindings.tests.cpp
 * @author Rahul Nair
 * @brief Tests for the Lua world:is_down/axis/axis2d/active_device bindings.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <script/ScriptHost.h>

#include <core/ecs/CommandBuffer.h>
#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/World.h>
#include <input/InputState.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

namespace
{
    using mir::ActionValue;
    using mir::CommandBuffer;
    using mir::ComponentOps;
    using mir::ComponentRegistry;
    using mir::DeviceKind;
    using mir::Entity;
    using mir::FieldKind;
    using mir::InputState;
    using mir::RuntimeFieldDecl;
    using mir::ScriptHost;
    using mir::World;

    const ComponentOps &ProbeOps()
    {
        static const RuntimeFieldDecl fields[] = {
            {"down", FieldKind::Bool},
            {"justPressed", FieldKind::Bool},
            {"move", FieldKind::Float},
            {"lookX", FieldKind::Float},
            {"lookY", FieldKind::Float},
            {"deviceIsGamepad", FieldKind::Bool},
        };
        return ComponentRegistry::Instance().RegisterRuntime("InputBindingsProbe", fields);
    }

    template <typename T>
    T ReadProbeField(const ComponentOps &probe, World &world, Entity entity, std::string_view field)
    {
        T value{};
        probe.FindField(field)->Read(probe.GetComponent(world, entity), &value);
        return value;
    }

    constexpr std::string_view kProbeSource = R"LUA(
local T = {}

function T.NewInstanceData()
    return {}
end

function T.OnStart(self, world, entity)
    world:set(entity, "InputBindingsProbe", "down", world:is_down("Jump"))
    world:set(entity, "InputBindingsProbe", "justPressed", world:just_pressed("Jump"))
    world:set(entity, "InputBindingsProbe", "move", world:axis("Move"))

    local x, y = world:axis2d("Look")
    world:set(entity, "InputBindingsProbe", "lookX", x)
    world:set(entity, "InputBindingsProbe", "lookY", y)

    world:set(entity, "InputBindingsProbe", "deviceIsGamepad", world:active_device() == "Gamepad")
end

return T
)LUA";
}

TEST_CASE("world:is_down/axis/axis2d/active_device read the InputState resource", "[script][input]")
{
    const ComponentOps &probe = ProbeOps();

    World world;
    CommandBuffer commands;

    InputState &state = world.EmplaceResource<InputState>();
    state.Actions()["Jump"] = ActionValue{.down = true, .justPressed = true};
    state.Actions()["Move"] = ActionValue{.x = 0.75f};
    state.Actions()["Look"] = ActionValue{.x = -0.5f, .y = 0.25f};
    state.SetActiveDevice(DeviceKind::Gamepad);

    const Entity entity = world.CreateEntity();
    probe.AddDefault(world, entity);

    ScriptHost host;
    REQUIRE(host.LoadScriptSource("input_probe", kProbeSource));

    const int32_t instance = host.CreateInstance("input_probe");
    REQUIRE(instance != -1);

    host.CallOnStart(world, commands, entity, instance);
    commands.Flush(world);

    CHECK(ReadProbeField<bool>(probe, world, entity, "down"));
    CHECK(ReadProbeField<bool>(probe, world, entity, "justPressed"));
    CHECK(ReadProbeField<float>(probe, world, entity, "move") == 0.75f);
    CHECK(ReadProbeField<float>(probe, world, entity, "lookX") == -0.5f);
    CHECK(ReadProbeField<float>(probe, world, entity, "lookY") == 0.25f);
    CHECK(ReadProbeField<bool>(probe, world, entity, "deviceIsGamepad"));

    host.DestroyInstance(instance);
}

TEST_CASE("world:is_down and world:axis are false/zero with no InputState resource", "[script][input]")
{
    const ComponentOps &probe = ProbeOps();

    World world; // no InputState emplaced
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    probe.AddDefault(world, entity);

    ScriptHost host;
    REQUIRE(host.LoadScriptSource("input_probe_missing", kProbeSource));

    const int32_t instance = host.CreateInstance("input_probe_missing");
    REQUIRE(instance != -1);

    host.CallOnStart(world, commands, entity, instance);
    commands.Flush(world);

    CHECK_FALSE(ReadProbeField<bool>(probe, world, entity, "down"));
    CHECK(ReadProbeField<float>(probe, world, entity, "move") == 0.0f);
    CHECK_FALSE(ReadProbeField<bool>(probe, world, entity, "deviceIsGamepad"));

    host.DestroyInstance(instance);
}
