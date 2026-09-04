/**
 * @file SceneIO.tests.cpp
 * @author Sumin Park
 * @brief Round-trip tests for scene save/load and StableId authoring (0030, 0031).
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <scene/SceneIO.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/ecs/World.h>
#include <core/ecs/components/Transform.h>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace
{
    using mts::AllocateStableId;
    using mts::ComponentOps;
    using mts::ComponentRegistry;
    using mts::CreateSceneEntity;
    using mts::Entity;
    using mts::FieldKind;
    using mts::kNullStableId;
    using mts::LoadedScene;
    using mts::NewScene;
    using mts::RuntimeFieldDecl;
    using mts::StableId;
    using mts::Transform;
    using mts::World;

    // A fresh temp directory per test, removed on scope exit - tests never
    // leave a scene directory behind for the next run to trip over.
    struct TempSceneDir
    {
        std::filesystem::path mPath;

        explicit TempSceneDir(std::string_view name)
            : mPath(std::filesystem::temp_directory_path() / "mts_scene_tests" / name)
        {
            std::filesystem::remove_all(mPath);
        }

        ~TempSceneDir() { std::filesystem::remove_all(mPath); }
    };

    // Distinct from every other test's runtime components: the registry is
    // process-wide (0022).
    const ComponentOps &SceneRefOps()
    {
        static const RuntimeFieldDecl fields[] = {{"target", FieldKind::EntityRef}};
        return ComponentRegistry::Instance().RegisterRuntime("SceneIOTestRef", fields);
    }
}

TEST_CASE("SaveScene then LoadScene round-trips a Transform", "[scene]")
{
    mts::RegisterCoreComponents();

    World world;
    LoadedScene scene = NewScene("test");
    Entity entity = CreateSceneEntity(world, scene);
    mts::AddTransform(world, entity, Transform{glm::vec3(1.0f, 2.0f, 3.0f)});

    TempSceneDir dir("transform_roundtrip");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);

    REQUIRE(loaded.mName == "test");
    REQUIRE(loaded.mEntities.size() == 1);
    Entity loadedEntity = loaded.mEntities.at(1);

    const Transform *transform = loadedWorld.Get<Transform>(loadedEntity);
    REQUIRE(transform != nullptr);
    CHECK(transform->Position() == glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(transform->Rotation() == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    CHECK(transform->Scale() == glm::vec3(1.0f));
}

TEST_CASE("SaveScene then LoadScene round-trips parent/child structure", "[scene]")
{
    mts::RegisterCoreComponents();

    World world;
    LoadedScene scene = NewScene("test");
    Entity parent = CreateSceneEntity(world, scene);
    Entity child = CreateSceneEntity(world, scene, parent);
    mts::AddTransform(world, parent, Transform{});
    mts::AddTransform(world, child, Transform{}, parent);

    TempSceneDir dir("parent_child_roundtrip");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);

    Entity loadedParent = loaded.mEntities.at(1);
    Entity loadedChild = loaded.mEntities.at(2);
    CHECK(mts::ParentOf(loadedWorld, loadedChild) == loadedParent);
    CHECK(mts::ParentOf(loadedWorld, loadedParent).IsNull());
}

TEST_CASE("SaveScene then LoadScene round-trips an EntityRef field", "[scene]")
{
    mts::RegisterCoreComponents();
    const ComponentOps &sceneRefOps = SceneRefOps();

    World world;
    LoadedScene scene = NewScene("test");
    Entity target = CreateSceneEntity(world, scene);
    Entity referrer = CreateSceneEntity(world, scene);
    sceneRefOps.AddDefault(world, referrer);
    sceneRefOps.FindField("target")->Write(sceneRefOps.Get(world, referrer), &target);

    TempSceneDir dir("entity_ref_roundtrip");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);

    Entity loadedTarget = loaded.mEntities.at(1);
    Entity loadedReferrer = loaded.mEntities.at(2);

    Entity resolved{};
    sceneRefOps.FindField("target")->Read(sceneRefOps.Get(loadedWorld, loadedReferrer), &resolved);
    CHECK(resolved == loadedTarget);
}

TEST_CASE("SaveScene writes an unset EntityRef as kNullStableId, and it loads back null", "[scene]")
{
    mts::RegisterCoreComponents();
    const ComponentOps &sceneRefOps = SceneRefOps();

    World world;
    LoadedScene scene = NewScene("test");
    Entity referrer = CreateSceneEntity(world, scene);
    sceneRefOps.AddDefault(world, referrer); // target left at its default: null Entity

    TempSceneDir dir("entity_ref_null");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);
    Entity loadedReferrer = loaded.mEntities.at(1);

    Entity resolved{};
    resolved.mIndex = 0; // poison, so a no-op Write would be caught
    sceneRefOps.FindField("target")->Read(sceneRefOps.Get(loadedWorld, loadedReferrer), &resolved);
    CHECK(resolved.IsNull());
}

TEST_CASE("LoadScene skips a component name it does not recognise", "[scene]")
{
    mts::RegisterCoreComponents();

    TempSceneDir dir("unknown_component");
    std::filesystem::create_directories(dir.mPath / "entities");

    {
        std::ofstream manifest(dir.mPath / "scene.json");
        manifest << R"({"name":"test","nextId":2,"entities":[1]})";
    }
    {
        std::ofstream entityFile(dir.mPath / "entities" / "1.json");
        entityFile << R"({
            "id": 1,
            "parent": 0,
            "components": [
                {"type": "NoSuchComponent", "fields": {"x": 1}},
                {"type": "Transform", "fields": {"position": [4.0, 5.0, 6.0]}}
            ]
        })";
    }

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);

    REQUIRE(loaded.mEntities.size() == 1);
    Entity loadedEntity = loaded.mEntities.at(1);
    const Transform *transform = loadedWorld.Get<Transform>(loadedEntity);
    REQUIRE(transform != nullptr);
    CHECK(transform->Position() == glm::vec3(4.0f, 5.0f, 6.0f));
}

TEST_CASE("UnloadScene destroys exactly the entities it loaded", "[scene]")
{
    mts::RegisterCoreComponents();

    World world;
    LoadedScene scene = NewScene("test");
    Entity parent = CreateSceneEntity(world, scene);
    Entity child = CreateSceneEntity(world, scene, parent);
    mts::AddTransform(world, parent, Transform{});
    mts::AddTransform(world, child, Transform{}, parent);
    Entity outsider = world.CreateEntity(); // not part of the saved scene

    TempSceneDir dir("unload_scoped");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    LoadedScene loaded = mts::LoadScene(world, dir.mPath);
    REQUIRE(loaded.mEntities.size() == 2);

    mts::UnloadScene(world, loaded);

    CHECK_FALSE(world.IsAlive(loaded.mEntities.at(1)));
    CHECK_FALSE(world.IsAlive(loaded.mEntities.at(2))); // cascaded via parent destroy (0020)
    CHECK(world.IsAlive(outsider));
}

TEST_CASE("AllocateStableId never reissues an id, even one whose entity was deleted", "[scene]")
{
    World world;
    LoadedScene scene = NewScene("test");

    Entity first = CreateSceneEntity(world, scene);  // id 1
    Entity second = CreateSceneEntity(world, scene); // id 2
    (void)second;

    // Drop the first entity from the scene, as an editor would on delete -
    // its id must never come back, or a stale EntityRef elsewhere would
    // silently start pointing at whatever reuses it.
    scene.mEntities.erase(1);
    world.DestroyEntity(first);

    StableId third = AllocateStableId(scene);
    CHECK(third == 3);
}

TEST_CASE("LoadScene's nextId survives a save, even past a deleted entity's id", "[scene]")
{
    mts::RegisterCoreComponents();

    World world;
    LoadedScene scene = NewScene("test");
    Entity first = CreateSceneEntity(world, scene);  // id 1
    CreateSceneEntity(world, scene);                 // id 2, kept

    scene.mEntities.erase(1);
    world.DestroyEntity(first);

    TempSceneDir dir("next_id_survives_save");
    REQUIRE(mts::SaveScene(world, dir.mPath, scene));

    World loadedWorld;
    LoadedScene loaded = mts::LoadScene(loadedWorld, dir.mPath);
    REQUIRE(loaded.mEntities.size() == 1); // only id 2 was ever written

    mts::StableId next = AllocateStableId(loaded);
    CHECK(next == 3); // not 2 (already used) and not reset to 1
}
