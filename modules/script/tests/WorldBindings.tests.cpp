/**
 * @file WorldBindings.tests.cpp
 * @author Sumin Park
 * @brief Tests for the Lua world:* bindings.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <script/ScriptHost.h>

#include <core/ecs/CommandBuffer.h>
#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

namespace
{
    using mir::CommandBuffer;
    using mir::ComponentOps;
    using mir::ComponentRegistry;
    using mir::Entity;
    using mir::FieldKind;
    using mir::RuntimeFieldDecl;
    using mir::ScriptHost;
    using mir::World;

    // Distinct from every other test's runtime components: the registry is
    // process-wide (0022).
    const ComponentOps &ProbeOps()
    {
        static const RuntimeFieldDecl fields[] = {{"blocked", FieldKind::Int}, {"ran", FieldKind::Int}};
        return ComponentRegistry::Instance().RegisterRuntime("BindingsProbe", fields);
    }

    const ComponentOps &TagOps() { return ComponentRegistry::Instance().RegisterRuntime("BindingsTag", {}); }

    constexpr std::string_view kEachSource = R"LUA(
local T = {}

function T.NewInstanceData()
    return {}
end

function T.OnStart(self, world, entity)
    world:each("BindingsTag", function(e)
        world:set(entity, "BindingsProbe", "blocked", 1)
    end)

    world:each("BindingsProbe", function(e, probe)
        world:set(entity, "BindingsProbe", "ran", 1)
    end)
end

return T
)LUA";

    int32_t ReadProbe(const ComponentOps &probe, World &world, Entity entity, std::string_view field)
    {
        int32_t value = -1;
        probe.FindField(field)->Read(probe.GetComponent(world, entity), &value);
        return value;
    }
}

TEST_CASE("world:each rejects a tag term instead of aborting", "[script]")
{
    const ComponentOps &probe = ProbeOps();
    const ComponentOps &tag = TagOps();

    World world;
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    probe.AddDefault(world, entity);
    tag.AddDefault(world, entity);

    ScriptHost host;
    REQUIRE(host.LoadScriptSource("each_tag", kEachSource));

    const int32_t instance = host.CreateInstance("each_tag");
    REQUIRE(instance != -1);

    // a tag term used to reach RuntimeQuery's MIR_CHECK, which aborts the
    // process; it must be answered by a logged error and an early return
    host.CallOnStart(world, commands, entity, instance);
    commands.Flush(world);

    CHECK(ReadProbe(probe, world, entity, "blocked") == 0);
    CHECK(ReadProbe(probe, world, entity, "ran") == 1);

    host.DestroyInstance(instance);
}
