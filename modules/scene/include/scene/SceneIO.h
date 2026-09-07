/**
 * @file SceneIO.h
 * @author Sumin Park
 * @brief Save and load a scene as a manifest plus one file per entity.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/Entity.h"
#include "scene/SceneAsset.h"

#include <filesystem>
#include <string>

namespace mir
{
    class World;

    bool SaveScene(World &world, const std::filesystem::path &sceneDir, const LoadedScene &scene);

    LoadedScene LoadScene(World &world, const std::filesystem::path &sceneDir);

    // Destroy all entity in scene
    void UnloadScene(World &world, const LoadedScene &loaded);
}
