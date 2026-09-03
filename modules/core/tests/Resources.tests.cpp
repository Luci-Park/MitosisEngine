/**
 * @file Resources.tests.cpp
 * @author Sumin Park
 * @brief Tests for World's resource storage.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
    using mts::Entity;
    using mts::World;

    struct Counter
    {
        int value = 0;
    };

    struct Settings
    {
        std::string name;
        int level = 0;
    };

    // Deliberately everything a component may not be: a heap-owning member and
    // a non-trivial destructor. Resources are never relocated, so none of the
    // ComponentColumn constraints apply to them.
    struct Tracked
    {
        explicit Tracked(int *destructions, std::size_t size)
            : items(size, 7), mDestructions(destructions)
        {
        }

        ~Tracked() { ++*mDestructions; }

        Tracked(const Tracked &) = delete;
        Tracked &operator=(const Tracked &) = delete;

        std::vector<int> items;

    private:
        int *mDestructions;
    };
}

TEST_CASE("A world starts with no resources", "[ecs][resources]")
{
    World world;

    CHECK_FALSE(world.HasResource<Counter>());
    CHECK(world.TryResource<Counter>() == nullptr);
}

TEST_CASE("EmplaceResource constructs in place and hands back the instance", "[ecs][resources]")
{
    World world;

    Settings &settings = world.EmplaceResource<Settings>("hard", 3);

    CHECK(settings.name == "hard");
    CHECK(settings.level == 3);
    CHECK(world.HasResource<Settings>());
    CHECK(&world.Resource<Settings>() == &settings);
}

TEST_CASE("Resources are keyed per type", "[ecs][resources]")
{
    World world;
    world.EmplaceResource<Counter>(5);
    world.EmplaceResource<Settings>("easy", 1);

    CHECK(world.Resource<Counter>().value == 5);
    CHECK(world.Resource<Settings>().level == 1);
}

TEST_CASE("A resource pointer survives emplacing other resources", "[ecs][resources]")
{
    // The value lives in a heap-allocated holder, so rehashing the map moves
    // the owning pointer and never the resource. Systems cache these.
    World world;
    Counter *counter = &world.EmplaceResource<Counter>(1);

    for (int i = 0; i < 64; ++i)
        world.EmplaceResource<Settings>("filler", i);

    counter->value = 42;
    CHECK(&world.Resource<Counter>() == counter);
    CHECK(world.Resource<Counter>().value == 42);
}

TEST_CASE("Emplacing the same resource twice replaces it", "[ecs][resources]")
{
    World world;
    world.EmplaceResource<Settings>("first", 1);
    world.EmplaceResource<Settings>("second", 2);

    CHECK(world.Resource<Settings>().name == "second");
}

TEST_CASE("A replaced resource is destroyed", "[ecs][resources]")
{
    int destructions = 0;
    {
        World world;
        world.EmplaceResource<Tracked>(&destructions, 4);
        CHECK(destructions == 0);

        world.EmplaceResource<Tracked>(&destructions, 4);
        CHECK(destructions == 1);
    }

    CHECK(destructions == 2); // and the survivor goes with the world
}

TEST_CASE("RemoveResource destroys it and reports whether there was one", "[ecs][resources]")
{
    int destructions = 0;
    World world;
    world.EmplaceResource<Tracked>(&destructions, 2);

    CHECK(world.RemoveResource<Tracked>());
    CHECK(destructions == 1);
    CHECK_FALSE(world.HasResource<Tracked>());
    CHECK_FALSE(world.RemoveResource<Tracked>());
}

TEST_CASE("A resource may own heap memory", "[ecs][resources]")
{
    // The point of resources over components: no trivially-copyable
    // requirement, because nothing ever memcpys one.
    int destructions = 0;
    World world;

    Tracked &tracked = world.EmplaceResource<Tracked>(&destructions, 3);

    REQUIRE(tracked.items.size() == 3);
    CHECK(tracked.items[0] == 7);
}

TEST_CASE("Resources do not consume component signature bits", "[ecs][resources]")
{
    // ComponentBit uses TypeId::seq directly as a bitset index. If resources
    // drew from that counter, enough of them would push a real component past
    // kMaxComponentTypes - so they must have their own.
    int destructions = 0;
    World world;
    world.EmplaceResource<Counter>(1);
    world.EmplaceResource<Settings>("x", 0);
    world.EmplaceResource<Tracked>(&destructions, 0);

    CHECK(mts::detail::ResourceIdOf<Counter>() < mts::kMaxComponentTypes);
    CHECK(mts::detail::ResourceIdOf<Settings>() != mts::detail::ResourceIdOf<Counter>());

    // and the world still works as an ECS afterwards
    const Entity entity = world.CreateEntity();
    world.AddComponent<Counter>(entity, Counter{9});
    CHECK(world.Get<Counter>(entity)->value == 9);

    world.RemoveResource<Tracked>();
}

TEST_CASE("A const-spelled type names the same resource", "[ecs][resources]")
{
    // ResourceIdOf<const T> is a separate instantiation with its own id, so
    // without normalization this compiles and then always reports the resource
    // as missing - which callers that treat nullptr as "not installed" turn
    // into a silent no-op.
    World world;
    Counter &counter = world.EmplaceResource<Counter>(3);

    CHECK(world.TryResource<const Counter>() == &counter);
    CHECK(world.HasResource<const Counter>());
    CHECK(world.Resource<const Counter>().value == 3);
    CHECK(world.RemoveResource<const Counter>());
    CHECK_FALSE(world.HasResource<Counter>());
}

TEST_CASE("A const world hands out const resources", "[ecs][resources]")
{
    World world;
    world.EmplaceResource<Counter>(11);

    const World &readOnly = world;
    REQUIRE(readOnly.TryResource<Counter>() != nullptr);
    CHECK(readOnly.Resource<Counter>().value == 11);
    CHECK(readOnly.TryResource<Settings>() == nullptr);
}
