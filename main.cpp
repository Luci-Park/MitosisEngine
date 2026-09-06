#include <app/App.h>
#include <assets/AssetBlob.h>
#include <assets/AssetCache.h>
#include <assets/AssetId.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/log/Log.h>
#include <renderer/Shapes.h>
#include <renderer/components/Camera.h>
#include <renderer/components/MeshRenderer.h>
#include <scene/SceneAsset.h>
#include <script/components/ScriptRef.h>

#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
    /// The subset of a `game.json` main.cpp needs to stand up an App. There
    /// is no project *selection* yet - LoadGameProject is called with one
    /// hardcoded path below - this only establishes what a project's on-disk
    /// shape is: a games/<name>/ directory holding its own assets/, scenes/,
    /// and a manifest naming both, mirroring the SOURCE_ROOTS convention
    /// engine_cook_assets already documents (see cmake/Assets.cmake).
    struct GameProject
    {
        std::string mTitle = "MjolnirEngine";
        std::string mAssetsRoot = "assets";
        std::string mSceneDir = "scenes/default";
    };

    /// Falls back to GameProject{}'s defaults (with a logged reason) rather
    /// than failing main() outright - a missing or malformed game.json is a
    /// content problem, not a reason the engine shouldn't start.
    GameProject LoadGameProject(const std::filesystem::path &path)
    {
        GameProject project;

        std::ifstream in(path);
        if (!in)
        {
            MIR_LOG_ERROR("LoadGameProject: could not open '{}', using defaults", path.string());
            return project;
        }

        nlohmann::json json;
        try
        {
            in >> json;
        }
        catch (const nlohmann::json::parse_error &e)
        {
            MIR_LOG_ERROR("LoadGameProject: '{}' is not valid JSON ({}), using defaults", path.string(), e.what());
            return project;
        }

        project.mTitle = json.value("title", project.mTitle);
        project.mAssetsRoot = json.value("assetsRoot", project.mAssetsRoot);
        project.mSceneDir = json.value("sceneDir", project.mSceneDir);
        return project;
    }

    /// Loads a `.lua` asset by its cooked path and registers it with the
    /// script host under `scriptName`, then hands it to the reload watcher
    /// so an edit followed by a rebuild is picked up without restarting.
    /// False when assets aren't available or the file failed to load or
    /// parse - logged by AssetCache/ScriptHost already, so the caller just
    /// decides whether to attach the script.
    bool LoadScriptAsset(mir::App &app, std::string_view assetPath, std::string_view scriptName)
    {
        mir::AssetCache *cache = app.Assets();
        if (cache == nullptr)
            return false;

        const mir::AssetId id = mir::MakeAssetId(assetPath);
        const mir::AssetBlobView *blob = cache->Load(id);
        if (blob == nullptr)
            return false;

        if (!app.Scripts().LoadScriptSource(scriptName, mir::AsStringView(*blob)))
            return false;

        app.ScriptReload().Track(std::string(scriptName), id);
        return true;
    }

    /// Runs after a scene loads from disk: every ScriptRef LoadScene added has
    /// scriptName set (that's the field SceneIO round-trips) but instanceRef
    /// still at -1 (its default, deliberately not saved - see ScriptRef.h),
    /// because "spawn a live Lua instance" is a side-effecting call SceneIO's
    /// generic field-copy loop has no hook for. This is that hook, run once
    /// up here instead: for every such entity, load <assetsRoot>/scripts/
    /// <name>.lua by convention and wire up the instance ScriptSystem will
    /// then drive.
    void ResolveSceneScripts(mir::App &app, const GameProject &project)
    {
        mir::World &world = app.GetWorld();

        world.GetOrCreateQuery<mir::ScriptRef>().ForEach(
            [&](mir::Entity, mir::ScriptRef &ref)
            {
                if (ref.instanceRef >= 0 || ref.scriptName[0] == '\0')
                    return;

                const std::string name = ref.scriptName;
                const std::string assetPath = project.mAssetsRoot + "/scripts/" + name + ".lua";

                if (!LoadScriptAsset(app, assetPath, name))
                {
                    MIR_LOG_ERROR("ResolveSceneScripts: could not load '{}' for script '{}'", assetPath, name);
                    return;
                }

                ref.instanceRef = app.Scripts().CreateInstance(name);
            });
    }

    /// A mesh asset is just MeshData (Shapes.h) as JSON - vertices (position/
    /// color/normal) and indices, the same shape CreateMesh already takes.
    /// No importer, no binary format: hand- or tool-authored JSON is enough
    /// until there's a reason for more. Malformed JSON logs and returns
    /// nullopt rather than throwing - one bad mesh asset shouldn't take the
    /// whole load down.
    std::optional<mir::MeshData> ParseMeshAsset(std::string_view text, std::string_view assetPath)
    {
        nlohmann::json json;
        try
        {
            json = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            MIR_LOG_ERROR("ParseMeshAsset: '{}' is not valid JSON ({})", assetPath, e.what());
            return std::nullopt;
        }

        mir::MeshData mesh;
        for (const auto &v : json.value("vertices", nlohmann::json::array()))
        {
            mir::Vertex vertex{};
            const auto &p = v.at("position");
            const auto &c = v.at("color");
            const auto &n = v.at("normal");
            vertex.pos = glm::vec3(p.at(0).get<float>(), p.at(1).get<float>(), p.at(2).get<float>());
            vertex.color = glm::vec3(c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>());
            vertex.normal = glm::vec3(n.at(0).get<float>(), n.at(1).get<float>(), n.at(2).get<float>());
            mesh.vertices.push_back(vertex);
        }
        mesh.indices = json.value("indices", std::vector<uint32_t>{});
        return mesh;
    }

    /// Runs after a scene loads from disk, same shape as ResolveSceneScripts:
    /// meshName/materialShader are what SceneIO round-trips (see
    /// MeshRenderer.h), mesh/material are this run's live GPU handles, still
    /// null until this pass fills them in. Handles are cached by asset id /
    /// shader name so entities sharing one mesh or material asset share one
    /// upload instead of duplicating it per entity.
    void ResolveSceneMeshes(mir::App &app, const GameProject &project)
    {
        mir::World &world = app.GetWorld();
        mir::AssetCache *cache = app.Assets();

        std::unordered_map<uint64_t, mir::MeshHandle> meshCache;
        std::unordered_map<std::string, mir::MaterialHandle> materialCache;

        world.GetOrCreateQuery<mir::MeshRenderer>().ForEach(
            [&](mir::Entity, mir::MeshRenderer &renderer)
            {
                if (renderer.mesh.IsNull() && renderer.meshName[0] != '\0')
                {
                    const std::string assetPath = project.mAssetsRoot + "/" + renderer.meshName;
                    const mir::AssetId id = mir::MakeAssetId(assetPath);

                    if (auto it = meshCache.find(id.value); it != meshCache.end())
                    {
                        renderer.mesh = it->second;
                    }
                    else if (cache != nullptr)
                    {
                        if (const mir::AssetBlobView *blob = cache->Load(id); blob != nullptr)
                        {
                            if (auto mesh = ParseMeshAsset(mir::AsStringView(*blob), assetPath))
                            {
                                renderer.mesh = app.Renderer().CreateMesh(mesh->vertices, mesh->indices);
                                meshCache.emplace(id.value, renderer.mesh);
                            }
                        }
                        else
                        {
                            MIR_LOG_ERROR("ResolveSceneMeshes: could not load '{}'", assetPath);
                        }
                    }
                }

                if (renderer.material.IsNull() && renderer.materialShader[0] != '\0')
                {
                    const std::string shaderName = renderer.materialShader;
                    if (auto it = materialCache.find(shaderName); it != materialCache.end())
                        renderer.material = it->second;
                    else
                        renderer.material = materialCache
                                                 .emplace(shaderName, app.Renderer().CreateMaterial(
                                                                          mir::MaterialDesc{.shaderName = shaderName}))
                                                 .first->second;
                }
            });
    }

    void BuildScene(mir::App &app, const GameProject &project)
    {
        mir::World &world = app.GetWorld();
        mir::LoadedScene &scene = app.Scene();

        const mir::MeshData cube = mir::MakeCube();
        const mir::MeshHandle cubeMesh = app.Renderer().CreateMesh(cube.vertices, cube.indices);

        const mir::Entity cubeEntity = mir::CreateSceneEntity(world, scene);
        mir::AddTransform(world, cubeEntity, mir::Transform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)});
        mir::MeshRenderer cubeRenderer{.mesh = cubeMesh};
        std::snprintf(cubeRenderer.meshName, sizeof(cubeRenderer.meshName), "meshes/cube.mesh.json");
        world.AddComponent<mir::MeshRenderer>(cubeEntity, cubeRenderer);

        const std::string spinAssetPath = project.mAssetsRoot + "/scripts/spin.lua";
        if (LoadScriptAsset(app, spinAssetPath, "spin"))
        {
            const int32_t spinInstance = app.Scripts().CreateInstance("spin");
            mir::ScriptRef ref{.instanceRef = spinInstance};
            std::snprintf(ref.scriptName, sizeof(ref.scriptName), "spin");
            world.AddComponent<mir::ScriptRef>(cubeEntity, ref);
        }
        else
        {
            MIR_LOG_ERROR("BuildScene: could not load '{}' - cube will not spin", spinAssetPath);
        }

        const mir::MaterialHandle unlitMaterial = app.Renderer().CreateMaterial(mir::MaterialDesc{.shaderName = "unlit"});

        const mir::Entity unlitCube = mir::CreateSceneEntity(world, scene);
        mir::AddTransform(world, unlitCube, mir::Transform{glm::vec3(1.8f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.5f)});
        mir::MeshRenderer unlitRenderer{.mesh = cubeMesh, .material = unlitMaterial};
        std::snprintf(unlitRenderer.meshName, sizeof(unlitRenderer.meshName), "meshes/cube.mesh.json");
        std::snprintf(unlitRenderer.materialShader, sizeof(unlitRenderer.materialShader), "unlit");
        world.AddComponent<mir::MeshRenderer>(unlitCube, unlitRenderer);

        const mir::Entity camera = mir::CreateSceneEntity(world, scene);
        mir::AddTransform(world, camera, mir::Transform{glm::vec3(0.0f, 0.0f, 5.0f)});
        world.AddComponent<mir::Camera>(camera, mir::Camera{});
    }
}

int main()
{
    // Logging lives outside App so early construction failures are still visible.
    mir::InitLog();

    // Single hardcoded project for now - no discovery/switcher yet, see
    // GameProject's comment. This is what "games/HelloWorld/game.json exists"
    // actually buys today: naming its assets/scenes roots in one place
    // instead of stringing "games/HelloWorld/..." through main.cpp by hand.
    const GameProject project = LoadGameProject("games/HelloWorld/game.json");

    mir::App app;

    mir::AppDesc desc{};
    desc.mTitle = project.mTitle.c_str();
    desc.mSceneDir = project.mSceneDir;

    if (!app.Initialize(desc))
    {
        mir::FlushLog();
        return -1;
    }

    // A scene.json in mSceneDir means a previous run saved one - load it
    // instead of the hardcoded demo scene. First run (or a deleted/moved
    // scene dir) falls back to BuildScene so there's still something on
    // screen; LoadScene leaves the (empty, from Initialize) scene alone
    // when it fails, so this can't silently double up entities either way.
    if (app.LoadScene())
    {
        app.Splash().SetProgress("Resolving scripts...", 0.93f);
        ResolveSceneScripts(app, project);

        app.Splash().SetProgress("Resolving meshes...", 0.97f);
        ResolveSceneMeshes(app, project);
    }
    else
    {
        app.Splash().SetProgress("Building scene...", 0.95f);
        BuildScene(app, project);
    }

    // Only now is startup actually done - close explicitly rather than
    // leaning on WS_EX_TOPMOST z-order to hide the swap.
    app.Splash().SetProgress("Ready", 1.0f);
    app.Splash().Close();

    app.Run();
    app.Shutdown();

    mir::FlushLog();
    return 0;
}
