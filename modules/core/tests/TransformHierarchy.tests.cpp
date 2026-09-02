/**
 * @file TransformHierarchy.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/TransformHierarchy.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/TransformHierarchy.h>

#include <core/ecs/CommandBuffer.h>
#include <core/ecs/SystemScheduler.h>
#include <core/ecs/World.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <vector>

namespace
{
    using Catch::Approx;
    using mts::Entity;
    using mts::Transform;
    using mts::World;
    using mts::WorldTransform;

    void RequireNear(const glm::vec3 &actual, const glm::vec3 &expected)
    {
        CHECK(actual.x == Approx(expected.x).margin(1e-4));
        CHECK(actual.y == Approx(expected.y).margin(1e-4));
        CHECK(actual.z == Approx(expected.z).margin(1e-4));
    }

    glm::vec3 OriginOf(const glm::mat4 &m) { return glm::vec3(m[3]); }

    Entity MakeAt(World &world, const glm::vec3 &position)
    {
        const Entity entity = world.CreateEntity();
        mts::AddTransform(world, entity, Transform{position});
        return entity;
    }

    glm::quat QuarterTurnZ() { return glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f)); }

    bool Contains(const std::vector<Entity> &entities, Entity entity)
    {
        return std::find(entities.begin(), entities.end(), entity) != entities.end();
    }
}

TEST_CASE("Hierarchy components stay memcpy-safe", "[ecs][transform][hierarchy]")
{
    STATIC_REQUIRE(std::is_trivially_copyable_v<mts::Hierarchy>);
    STATIC_REQUIRE(std::is_standard_layout_v<mts::Hierarchy>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<WorldTransform>);
    STATIC_REQUIRE(std::is_standard_layout_v<WorldTransform>);
    STATIC_REQUIRE(alignof(WorldTransform) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

TEST_CASE("Transform bumps its version only on mutation", "[ecs][transform]")
{
    Transform t;
    const uint32_t start = t.Version();

    CHECK(t.Matrix() == t.Matrix());
    CHECK(t.Version() == start); // Matrix() is a read

    t.SetPosition(glm::vec3(1.0f));
    CHECK(t.Version() != start);

    const uint32_t afterMove = t.Version();
    t.Translate(glm::vec3(1.0f));
    CHECK(t.Version() != afterMove);
}

TEST_CASE("A root resolves to its own local transform", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(3.0f, 0.0f, 0.0f));

    RequireNear(OriginOf(mts::ResolveWorld(world, root)), glm::vec3(3.0f, 0.0f, 0.0f));
}

TEST_CASE("A child composes with its parent", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 5.0f, 0.0f));

    mts::SetParent(world, child, parent);

    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 5.0f, 0.0f));
}

TEST_CASE("A parent rotation carries the child around it", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(0.0f));
    const Entity child = MakeAt(world, glm::vec3(2.0f, 0.0f, 0.0f));
    mts::SetParent(world, child, parent);

    world.Get<Transform>(parent)->SetRotation(QuarterTurnZ());

    // child sits at +2X in the parent space, so a +90 degree turn puts it at +2Y
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(0.0f, 2.0f, 0.0f));
}

TEST_CASE("A mid-frame read after a parent write is never stale", "[ecs][transform][hierarchy]")
{
    // The whole point of the version stamps: no propagate pass runs here.
    World world;
    const Entity parent = MakeAt(world, glm::vec3(0.0f));
    const Entity child = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));
    mts::SetParent(world, child, parent);

    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(1.0f, 0.0f, 0.0f));

    world.Get<Transform>(parent)->SetPosition(glm::vec3(0.0f, 100.0f, 0.0f));

    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(1.0f, 100.0f, 0.0f));
}

TEST_CASE("Staleness reaches a whole chain without touching the subtree", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity a = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));
    const Entity b = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));
    const Entity c = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));
    mts::SetParent(world, b, a);
    mts::SetParent(world, c, b);

    RequireNear(OriginOf(mts::ResolveWorld(world, c)), glm::vec3(3.0f, 0.0f, 0.0f));

    // one O(1) write at the root, three levels down still correct
    world.Get<Transform>(a)->SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    RequireNear(OriginOf(mts::ResolveWorld(world, c)), glm::vec3(12.0f, 0.0f, 0.0f));
}

TEST_CASE("A clean resolve does not rebuild", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));

    mts::ResolveWorld(world, root);
    const uint32_t settled = world.Get<WorldTransform>(root)->Version();

    mts::ResolveWorld(world, root);
    mts::ResolveWorld(world, root);
    CHECK(world.Get<WorldTransform>(root)->Version() == settled);

    world.Get<Transform>(root)->SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));
    mts::ResolveWorld(world, root);
    CHECK(world.Get<WorldTransform>(root)->Version() != settled);
}

TEST_CASE("Reparenting is not fooled by matching versions", "[ecs][transform][hierarchy]")
{
    // Both parents are freshly built, so their WorldTransform versions collide.
    // Only the explicit invalidation in SetParent catches this.
    World world;
    const Entity first = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity second = MakeAt(world, glm::vec3(-10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));

    mts::SetParent(world, child, first);
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 1.0f, 0.0f));

    mts::SetParent(world, child, second);
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(-10.0f, 1.0f, 0.0f));
}

TEST_CASE("Rooting a child returns it to world space", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));
    mts::SetParent(world, child, parent);
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 1.0f, 0.0f));

    mts::SetParent(world, child, mts::kNullEntity);

    // the component stays; being unparented is a null link, not an absence
    CHECK(world.Has<mts::Hierarchy>(child));
    CHECK(world.Get<mts::Hierarchy>(child)->IsRoot());
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST_CASE("AddTransform gives every entity a rooted Hierarchy", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity entity = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));

    CHECK(world.Has<mts::Hierarchy>(entity));
    CHECK(world.Get<mts::Hierarchy>(entity)->IsRoot());
    CHECK_FALSE(world.Get<mts::Hierarchy>(entity)->HasChildren());
    CHECK(world.Has<WorldTransform>(entity));
}

TEST_CASE("AddTransform can parent on creation", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));

    const Entity child = world.CreateEntity();
    mts::AddTransform(world, child, Transform{glm::vec3(0.0f, 1.0f, 0.0f)}, parent);

    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 1.0f, 0.0f));
}

TEST_CASE("Reparenting never moves the entity between archetypes", "[ecs][transform][hierarchy]")
{
    // This is what the always-present Parent buys: parenting and rooting are
    // field writes, so no Transform or WorldTransform is memcpy'd to a new table.
    World world;
    const Entity first = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity second = MakeAt(world, glm::vec3(-10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));

    const mts::Archetype *before = world.ArchetypeOf(child);
    const std::size_t archetypesBefore = world.ArchetypeCount();

    mts::SetParent(world, child, first);
    mts::SetParent(world, child, second);
    mts::SetParent(world, child, mts::kNullEntity);

    CHECK(world.ArchetypeOf(child) == before);
    CHECK(world.ArchetypeCount() == archetypesBefore);
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST_CASE("Destroying a parent destroys its children", "[ecs][transform][hierarchy][destroy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));
    mts::SetParent(world, child, parent);

    world.DestroyEntity(parent);

    CHECK_FALSE(world.IsAlive(parent));
    CHECK_FALSE(world.IsAlive(child));
}

TEST_CASE("DestroyEntity takes the whole subtree", "[ecs][transform][hierarchy][destroy]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f));
    const Entity grandchild = MakeAt(world, glm::vec3(0.0f));
    const Entity bystander = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, child, root);
    mts::SetParent(world, grandchild, child);

    world.DestroyEntity(root);

    CHECK_FALSE(world.IsAlive(root));
    CHECK_FALSE(world.IsAlive(child));
    CHECK_FALSE(world.IsAlive(grandchild));
    CHECK(world.IsAlive(bystander));
}

TEST_CASE("Destroying a middle node spares its parent", "[ecs][transform][hierarchy][destroy]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity middle = MakeAt(world, glm::vec3(0.0f));
    const Entity leaf = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, middle, root);
    mts::SetParent(world, leaf, middle);

    world.DestroyEntity(middle);

    CHECK(world.IsAlive(root));
    CHECK_FALSE(world.IsAlive(middle));
    CHECK_FALSE(world.IsAlive(leaf));
}

TEST_CASE("A deferred CommandBuffer destroy cascades too", "[ecs][transform][hierarchy][destroy]")
{
    // CommandBuffer::Destroy flushes through World::DestroyEntity, so the hook
    // covers the deferred route without knowing about it.
    World world;
    mts::CommandBuffer commands;

    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f));
    const Entity grandchild = MakeAt(world, glm::vec3(0.0f));
    const Entity bystander = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, child, root);
    mts::SetParent(world, grandchild, child);

    commands.Destroy(root);
    CHECK(world.IsAlive(child)); // still deferred

    commands.Flush(world);

    CHECK_FALSE(world.IsAlive(root));
    CHECK_FALSE(world.IsAlive(child));
    CHECK_FALSE(world.IsAlive(grandchild));
    CHECK(world.IsAlive(bystander));
}

TEST_CASE("A cascading hook may relocate the row being destroyed", "[ecs][hooks]")
{
    // The children are created *before* the parent, so the parent holds the
    // last row of the archetype. Destroying a child then swap-removes the
    // parent into the child's slot, mid-cascade.
    //
    // This is why DestroyEntity reads its EntityRecord after the hooks run: a
    // record captured beforehand would name a row that now belongs to someone
    // else, or no longer exists at all.
    World world;
    const Entity first = MakeAt(world, glm::vec3(0.0f));
    const Entity second = MakeAt(world, glm::vec3(0.0f));
    const Entity root = MakeAt(world, glm::vec3(0.0f)); // last row
    const Entity bystander = MakeAt(world, glm::vec3(1.0f, 2.0f, 3.0f));

    mts::SetParent(world, first, root);
    mts::SetParent(world, second, root);

    world.DestroyEntity(root);

    CHECK_FALSE(world.IsAlive(root));
    CHECK_FALSE(world.IsAlive(first));
    CHECK_FALSE(world.IsAlive(second));

    // The survivor must still be intact and findable: removing the wrong row
    // would corrupt it rather than fail loudly.
    REQUIRE(world.IsAlive(bystander));
    RequireNear(OriginOf(mts::ResolveWorld(world, bystander)), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_CASE("A destroy hook fires with components still readable", "[ecs][hooks]")
{
    // The ordering DestroyEntity depends on: hooks run before teardown, so a
    // hook can still inspect what it is about to lose.
    World world;
    static int seen = 0;
    seen = 0;

    world.AddDestroyHook([](World &w, Entity e, void *)
                         { seen += (w.Get<Transform>(e) != nullptr) ? 1 : 0; });

    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, b, a);

    world.DestroyEntity(a);

    CHECK(seen == 2); // the parent, and the child the cascade reached
}

TEST_CASE("The same destroy hook is only installed once", "[ecs][hooks]")
{
    // One function pointer added repeatedly, not two lambdas: each captureless
    // lambda is its own type with its own address, so writing it out twice
    // would be two different hooks and would not test the dedupe at all.
    World world;
    static int calls = 0;
    calls = 0;

    void (*hook)(World &, Entity, void *) = [](World &, Entity, void *) { ++calls; };
    world.AddDestroyHook(hook);
    world.AddDestroyHook(hook);
    world.AddDestroyHook(hook);

    world.DestroyEntity(world.CreateEntity());

    CHECK(calls == 1);
}

TEST_CASE("Repeated AddTransform installs one hierarchy hook", "[ecs][hooks]")
{
    // AddTransform arms the hook on every call, so the dedupe is what keeps a
    // scene of N entities from cascading N times per destroy.
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, a);

    // A second cascade over an already-dead subtree would assert or double-free
    // rather than pass quietly.
    world.DestroyEntity(root);

    CHECK_FALSE(world.IsAlive(a));
    CHECK_FALSE(world.IsAlive(b));
}

TEST_CASE("ForEachChild visits every direct child and no deeper", "[ecs][transform][hierarchy][walk]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    const Entity c = MakeAt(world, glm::vec3(0.0f));
    const Entity grandchild = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, root);
    mts::SetParent(world, c, root);
    mts::SetParent(world, grandchild, a);

    std::vector<Entity> seen;
    mts::ForEachChild(world, root, [&seen](Entity e) { seen.push_back(e); });

    CHECK(seen.size() == 3);
    CHECK(Contains(seen, a));
    CHECK(Contains(seen, b));
    CHECK(Contains(seen, c));
    CHECK_FALSE(Contains(seen, grandchild));
}

TEST_CASE("ForEachChild on a leaf visits nothing", "[ecs][transform][hierarchy][walk]")
{
    World world;
    const Entity leaf = MakeAt(world, glm::vec3(0.0f));

    std::size_t count = 0;
    mts::ForEachChild(world, leaf, [&count](Entity) { ++count; });

    CHECK(count == 0);
}

TEST_CASE("Unlinking repairs the chain around it", "[ecs][transform][hierarchy][walk]")
{
    // Removing the middle of a chain is what mPrevSibling exists for; if the
    // splice were wrong, the survivors would go missing from the walk.
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    const Entity c = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, root);
    mts::SetParent(world, c, root);

    SECTION("middle")
    {
        mts::SetParent(world, b, mts::kNullEntity);
    }
    SECTION("head")
    {
        mts::SetParent(world, c, mts::kNullEntity);
    }
    SECTION("tail")
    {
        mts::SetParent(world, a, mts::kNullEntity);
    }

    std::vector<Entity> seen;
    mts::ForEachChild(world, root, [&seen](Entity e) { seen.push_back(e); });
    CHECK(seen.size() == 2);
}

TEST_CASE("Reparenting moves a child between chains", "[ecs][transform][hierarchy][walk]")
{
    World world;
    const Entity first = MakeAt(world, glm::vec3(0.0f));
    const Entity second = MakeAt(world, glm::vec3(0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f));

    mts::SetParent(world, child, first);
    mts::SetParent(world, child, second);

    std::vector<Entity> fromFirst;
    std::vector<Entity> fromSecond;
    mts::ForEachChild(world, first, [&fromFirst](Entity e) { fromFirst.push_back(e); });
    mts::ForEachChild(world, second, [&fromSecond](Entity e) { fromSecond.push_back(e); });

    CHECK(fromFirst.empty());
    CHECK(fromSecond.size() == 1);
    CHECK(fromSecond.front() == child);
}

TEST_CASE("ForEachDescendant reaches the whole subtree", "[ecs][transform][hierarchy][walk]")
{
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    const Entity deep = MakeAt(world, glm::vec3(0.0f));
    const Entity bystander = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, a);
    mts::SetParent(world, deep, b);

    std::vector<Entity> seen;
    mts::ForEachDescendant(world, root, [&seen](Entity e) { seen.push_back(e); });

    CHECK(seen.size() == 3);
    CHECK(Contains(seen, a));
    CHECK(Contains(seen, b));
    CHECK(Contains(seen, deep));
    CHECK_FALSE(Contains(seen, root));
    CHECK_FALSE(Contains(seen, bystander));
}

TEST_CASE("A child may be destroyed inside ForEachChild", "[ecs][transform][hierarchy][walk]")
{
    // The next link is read before fn runs, so the walk survives fn destroying
    // the entity it was just handed.
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, root);

    std::size_t visited = 0;
    mts::ForEachChild(world, root,
                      [&world, &visited](Entity child)
                      {
                          ++visited;
                          world.DestroyEntity(child);
                      });

    CHECK(visited == 2);
    CHECK_FALSE(world.IsAlive(a));
    CHECK_FALSE(world.IsAlive(b));
}

TEST_CASE("Destroying a child leaves its siblings walkable", "[ecs][transform][hierarchy][destroy]")
{
    // The hook splices the dying entity out on the way down, so no dead handle
    // is left in a chain to truncate iteration. The middle case is the one that
    // would silently lose a sibling if the splice were skipped.
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    const Entity c = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, a, root);
    mts::SetParent(world, b, root);
    mts::SetParent(world, c, root);

    Entity removed = b;
    SECTION("middle") { removed = b; }
    SECTION("head") { removed = c; }
    SECTION("tail") { removed = a; }

    world.DestroyEntity(removed);

    std::vector<Entity> seen;
    mts::ForEachChild(world, root, [&seen](Entity e) { seen.push_back(e); });

    CHECK(seen.size() == 2);
    CHECK_FALSE(Contains(seen, removed));
}

TEST_CASE("A wide subtree dies without walking a stale chain", "[ecs][transform][hierarchy][destroy]")
{
    // Draining from the head, not walking NextSibling: each child splices
    // itself out as it dies, rewriting the chain on every step.
    World world;
    const Entity root = MakeAt(world, glm::vec3(0.0f));

    std::vector<Entity> children;
    for (int i = 0; i < 64; ++i)
    {
        const Entity child = MakeAt(world, glm::vec3(0.0f));
        mts::SetParent(world, child, root);
        children.push_back(child);
    }

    world.DestroyEntity(root);

    for (const Entity child : children)
        CHECK_FALSE(world.IsAlive(child));
}

TEST_CASE("AddTransform is idempotent", "[ecs][transform][hierarchy]")
{
    // The second call must overwrite rather than add a second Transform:
    // World::AddComponent only asserts against a duplicate, so in a release
    // build the entity would end up with a record naming a row that no longer
    // exists and every later Get would read past the end of the column.
    World world;
    const Entity entity = world.CreateEntity();

    mts::AddTransform(world, entity, Transform{glm::vec3(1.0f, 0.0f, 0.0f)});
    mts::AddTransform(world, entity, Transform{glm::vec3(0.0f, 2.0f, 0.0f)});

    const Entity bystander = MakeAt(world, glm::vec3(9.0f, 9.0f, 9.0f));

    RequireNear(world.Get<Transform>(entity)->Position(), glm::vec3(0.0f, 2.0f, 0.0f));
    RequireNear(OriginOf(mts::ResolveWorld(world, entity)), glm::vec3(0.0f, 2.0f, 0.0f));
    RequireNear(OriginOf(mts::ResolveWorld(world, bystander)), glm::vec3(9.0f, 9.0f, 9.0f));
}

TEST_CASE("Parenting to a bare Transform still links", "[ecs][transform][hierarchy]")
{
    // A parent built with plain AddComponent has no Hierarchy of its own. That
    // shape is supported, so SetParent has to give it one rather than drop the
    // edge after it has already detached the child.
    World world;
    const Entity parent = world.CreateEntity();
    world.AddComponent<Transform>(parent, Transform{glm::vec3(10.0f, 0.0f, 0.0f)});

    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));
    mts::SetParent(world, child, parent);

    CHECK(world.Has<mts::Hierarchy>(parent));
    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 1.0f, 0.0f));

    std::vector<Entity> seen;
    mts::ForEachChild(world, parent, [&seen](Entity e) { seen.push_back(e); });
    CHECK(seen.size() == 1);
}

TEST_CASE("A pivot with no Transform is not fooled by matching versions", "[ecs][transform][hierarchy]")
{
    // The pivot has a WorldTransform but no Transform, so it stamps
    // localVersion 0 as its legitimate value. Reusing 0 as the "invalidated"
    // sentinel would make this reparent invisible whenever the two parents
    // happen to share a version - which two freshly built ones do.
    World world;
    const Entity first = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity second = MakeAt(world, glm::vec3(-10.0f, 0.0f, 0.0f));

    const Entity pivot = world.CreateEntity();
    world.AddComponent<mts::WorldTransform>(pivot, mts::WorldTransform{});
    mts::SetParent(world, pivot, first);
    RequireNear(OriginOf(mts::ResolveWorld(world, pivot)), glm::vec3(10.0f, 0.0f, 0.0f));

    mts::SetParent(world, pivot, second);
    RequireNear(OriginOf(mts::ResolveWorld(world, pivot)), glm::vec3(-10.0f, 0.0f, 0.0f));
}

TEST_CASE("AddTransform relinks an already-parented entity", "[ecs][transform][hierarchy]")
{
    // AddTransform is idempotent, so it can legitimately land on an entity that
    // is already in a chain. Attaching without detaching first would leave it
    // in both parents at once.
    World world;
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity c = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));

    mts::SetParent(world, b, a);
    mts::AddTransform(world, b, Transform{glm::vec3(1.0f, 0.0f, 0.0f)}, c);

    std::vector<Entity> fromA;
    std::vector<Entity> fromC;
    mts::ForEachChild(world, a, [&fromA](Entity e) { fromA.push_back(e); });
    mts::ForEachChild(world, c, [&fromC](Entity e) { fromC.push_back(e); });

    CHECK(fromA.empty());
    CHECK(fromC.size() == 1);
    CHECK(world.Get<mts::Hierarchy>(b)->Parent() == c);
}



TEST_CASE("IsAncestorOf walks the chain", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity a = MakeAt(world, glm::vec3(0.0f));
    const Entity b = MakeAt(world, glm::vec3(0.0f));
    const Entity c = MakeAt(world, glm::vec3(0.0f));
    mts::SetParent(world, b, a);
    mts::SetParent(world, c, b);

    CHECK(mts::IsAncestorOf(world, a, c));
    CHECK(mts::IsAncestorOf(world, b, c));
    CHECK(mts::IsAncestorOf(world, c, c)); // reflexive, which is what the cycle check needs
    CHECK_FALSE(mts::IsAncestorOf(world, c, a));
}

TEST_CASE("A Transform without a WorldTransform still resolves", "[ecs][transform][hierarchy]")
{
    World world;
    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));

    const Entity child = world.CreateEntity();
    world.AddComponent<Transform>(child, Transform{glm::vec3(0.0f, 1.0f, 0.0f)}); // no cache component
    mts::SetParent(world, child, parent);

    RequireNear(OriginOf(mts::ResolveWorld(world, child)), glm::vec3(10.0f, 1.0f, 0.0f));
    CHECK_FALSE(world.Has<WorldTransform>(child));
}

TEST_CASE("An uncached ancestor does not let a descendant go stale", "[ecs][transform][hierarchy]")
{
    // middle has no WorldTransform, so leaf has no version to trust and must
    // recompute every resolve rather than believe a stamp that cannot move
    World world;
    const Entity root = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));

    const Entity middle = world.CreateEntity();
    world.AddComponent<Transform>(middle, Transform{glm::vec3(1.0f, 0.0f, 0.0f)});
    mts::SetParent(world, middle, root);

    const Entity leaf = MakeAt(world, glm::vec3(1.0f, 0.0f, 0.0f));
    mts::SetParent(world, leaf, middle);

    RequireNear(OriginOf(mts::ResolveWorld(world, leaf)), glm::vec3(3.0f, 0.0f, 0.0f));

    world.Get<Transform>(middle)->SetPosition(glm::vec3(100.0f, 0.0f, 0.0f));
    RequireNear(OriginOf(mts::ResolveWorld(world, leaf)), glm::vec3(102.0f, 0.0f, 0.0f));
}

TEST_CASE("TransformPropagateSystem refreshes every cache", "[ecs][transform][hierarchy][system]")
{
    World world;
    mts::CommandBuffer commands;
    mts::SystemScheduler scheduler;
    scheduler.Add<mts::TransformPropagateSystem>(mts::SystemPhase::PostUpdate);

    const Entity parent = MakeAt(world, glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(world, glm::vec3(0.0f, 1.0f, 0.0f));
    mts::SetParent(world, child, parent);

    mts::SystemContext context{world, commands};
    scheduler.Start(context);
    scheduler.Update(context);

    // read the cache directly, with no resolve call - what a downstream
    // read-only system is allowed to do once the pass has run
    RequireNear(OriginOf(world.Get<WorldTransform>(child)->Matrix()), glm::vec3(10.0f, 1.0f, 0.0f));

    world.Get<Transform>(parent)->SetPosition(glm::vec3(0.0f, 0.0f, 7.0f));
    scheduler.Update(context);

    RequireNear(OriginOf(world.Get<WorldTransform>(child)->Matrix()), glm::vec3(0.0f, 1.0f, 7.0f));
}
