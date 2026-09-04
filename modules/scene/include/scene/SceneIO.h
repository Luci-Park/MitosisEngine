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

namespace mts
{
    class World;

    /**
     * Writes `sceneDir` as scene.json plus entities/<id>.json, one file per
     * entity in `scene.mEntities`. See docs/decisions/0030-scene-file-layout.md
     * and 0031-component-serialization.md.
     *
     * `scene.mEntities`' StableId assignment is exactly what gets written, in
     * id order (see LoadedScene) - `scene.mNextId` goes into the manifest too,
     * so a later LoadScene continues allocating past every id this save ever
     * used, including ones for entities no longer present. Build `scene` with
     * NewScene/AllocateStableId/CreateSceneEntity, or reuse one a prior
     * LoadScene returned.
     *
     * A component is written only through its registered ComponentOps /
     * FieldDesc table (ComponentRegistry) - a component with no field table
     * round-trips as present but empty. An EntityRef field pointing outside
     * `scene.mEntities` is written as kNullStableId; cross-scene references
     * are not representable yet.
     *
     * Returns false on any file I/O failure. Overwrites an existing sceneDir.
     */
    bool SaveScene(World &world, const std::filesystem::path &sceneDir, const LoadedScene &scene);

    /**
     * Reads `sceneDir` and spawns its entities into `world`, in three passes:
     *
     *  1. Spawn - create every entity, add every component it lists (unknown
     *     component names are skipped, not fatal - forward compatibility),
     *     and write every non-EntityRef field. EntityRef fields are recorded
     *     for pass 3, not written yet: their StableId target may not have an
     *     Entity until every entity in the file exists.
     *  2. Parent - resolve each entity's parent StableId through the map
     *     pass 1 built, via mts::SetParent. Safe after every AddCopy in pass
     *     1: hierarchy is a World resource (0020), so this never moves an
     *     entity between archetypes.
     *  3. EntityRef patch - resolve every recorded EntityRef field's stored
     *     StableId and write it into the now-stable component pointer.
     *
     * A dangling parent or EntityRef StableId (not present in this scene)
     * resolves to null, matching how it was written.
     *
     * The returned LoadedScene's mNextId comes from the manifest (defensively
     * clamped above every id actually present, in case the file was hand-
     * edited), so passing it straight into CreateSceneEntity/SaveScene
     * continues allocating without reissuing anything this file ever used.
     */
    LoadedScene LoadScene(World &world, const std::filesystem::path &sceneDir);

    /// Destroys every entity `loaded` created (each guarded by IsAlive, since
    /// destroying a parent already cascades to its children - 0020). Does not
    /// touch entities any other loaded scene owns.
    void UnloadScene(World &world, const LoadedScene &loaded);
}
