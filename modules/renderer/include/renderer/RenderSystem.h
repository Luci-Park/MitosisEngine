/**
 * @file RenderSystem.h
 * @author Sumin Park
 * @brief Drives the renderer from the ECS, in SystemPhase::Render.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "renderer/VulkanRenderer.h"

#include <core/ecs/Query.h>
#include <core/ecs/System.h>
#include <core/ecs/components/WorldTransform.h>
#include <renderer/components/MeshRenderer.h>

#include <vector>

namespace mts
{
    /**
     * Replaces App::CollectDrawInstances + the direct DrawFrame call: the
     * renderer no longer needs App to know it exists, only that Render runs
     * after TransformPropagateSystem (PostUpdate), so every WorldTransform
     * read here is already current for this frame.
     *
     * Holds the renderer by reference, not by resource lookup - the same
     * pattern SpinSystem uses for its target Entity. A World resource would
     * make the renderer reachable from a script; that is not wanted yet.
     */
    class RenderSystem final : public ISystem
    {
    public:
        explicit RenderSystem(VulkanRenderer &renderer)
            : mRenderer(renderer)
        {
        }

        void OnStart(SystemContext &context) override
        {
            mQuery = &context.world.GetOrCreateQuery<const WorldTransform, const MeshRenderer>();
        }

        void OnUpdate(SystemContext &) override
        {
            mDrawItems.clear();

            mQuery->ForEach([this](Entity, const WorldTransform &world, const MeshRenderer &renderer)
                            { mDrawItems.push_back(DrawItem{renderer.mesh, world.Matrix(), renderer.tint}); });

            mRenderer.DrawFrame(mDrawItems);
        }

    private:
        VulkanRenderer &mRenderer;
        Query<const WorldTransform, const MeshRenderer> *mQuery = nullptr;

        /// Rebuilt every frame but keeps its capacity to steady allocation,
        /// same reasoning as the mDrawInstances it replaces in App.
        std::vector<DrawItem> mDrawItems;
    };
}
