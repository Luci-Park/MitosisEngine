/**
 * @file SceneAsset.h
 * @author Sumin Park
 * @brief The save-file identity for a scene and its entities.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "core/ecs/Entity.h"
#include "core/ecs/TransformHierarchy.h"
#include "core/ecs/World.h"

#include <cstdint>
#include <map>
#include <string>

namespace mts
{
    /**
     * Save-file-only identity for an entity, distinct from Entity.
     *
     * Entity (Entity.h) is a runtime handle: mIndex is a slot that gets
     * reused, mGeneration guards against reading a stale one. Neither survives
     * a process restart with the same meaning, so a scene file cannot
     * reference entities by Entity. StableId is assigned once, at authoring
     * time, and never reused - see docs/decisions/0030-scene-file-layout.md.
     */
    using StableId = uint64_t;

    /// The "no entity" value for a StableId field - an unset parent, or an
    /// EntityRef that was null (or pointed outside the scene) when saved.
    inline constexpr StableId kNullStableId = 0;

    /**
     * A scene loaded into (or being authored into) a World.
     *
     * mEntities is a std::map, not unordered_map, deliberately: SaveScene
     * writes the manifest in key order, and an unordered_map's iteration order
     * is not guaranteed stable between runs. Writing the manifest from one
     * would reshuffle it on every save even with no actual change, defeating
     * the diff-friendliness 0030 exists for. Sorted by StableId also means a
     * newly added entity - always a higher id than anything already saved,
     * see AllocateStableId - appends at the end of the array instead of
     * reordering it.
     *
     * The map is also what makes an unload scoped - UnloadScene destroys
     * exactly the entities this load created, not anything another loaded
     * scene owns.
     */
    struct LoadedScene
    {
        std::string mName;
        std::map<StableId, Entity> mEntities;

        /// High-water mark: the next id AllocateStableId hands out. Persisted
        /// in scene.json across saves so a deleted entity's id is never
        /// reissued, even though it no longer appears in mEntities - a stale
        /// reference to it elsewhere (an EntityRef in another entity file that
        /// hasn't been resaved yet) must resolve to "gone", never to whatever
        /// new entity happens to reuse the id.
        StableId mNextId = 1;
    };

    /// An empty scene ready for authoring - AllocateStableId/CreateSceneEntity
    /// then SaveScene. Not backed by any file until the first save.
    inline LoadedScene NewScene(std::string name)
    {
        LoadedScene scene;
        scene.mName = std::move(name);
        return scene;
    }

    /// A fresh id for `scene`, bumping its high-water mark so it is never
    /// handed out again - not by this scene, even if the entity holding it is
    /// later deleted.
    inline StableId AllocateStableId(LoadedScene &scene) { return scene.mNextId++; }

    /**
     * Creates an entity, gives it a fresh StableId, and registers both into
     * `scene` - the one call an editor's "add entity" needs, so allocation and
     * registration can never happen for one without the other.
     *
     * `parent`, if not null, must already be in `scene.mEntities` - reparenting
     * to an entity this scene doesn't know about would save a dangling parent
     * StableId once `parent` itself is added to some scene, possibly a
     * different one. Refused the same way SetParent refuses a bad parent: on
     * failure, the entity is still created and registered, just left
     * unparented.
     */
    inline Entity CreateSceneEntity(World &world, LoadedScene &scene, Entity parent = kNullEntity)
    {
        Entity entity = world.CreateEntity();
        scene.mEntities[AllocateStableId(scene)] = entity;
        if (!parent.IsNull())
            SetParent(world, entity, parent);
        return entity;
    }
}
