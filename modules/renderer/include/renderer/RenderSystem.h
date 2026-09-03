/**
 * @file RenderSystem.h
 * @author Sumin Park
 * @brief Drives the renderer from the ECS, in SystemPhase::Render.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once
#include "renderer/CameraMath.h"
#include "renderer/VulkanRenderer.h"

#include <core/ecs/Query.h>
#include <core/ecs/System.h>
#include <core/ecs/components/WorldTransform.h>
#include <core/log/Log.h>
#include <renderer/components/Camera.h>
#include <renderer/components/MeshRenderer.h>

#include <vector>

namespace mts
{
    class RenderSystem final : public ISystem
    {
    public:
        explicit RenderSystem(VulkanRenderer &renderer)
            : mRenderer(renderer)
        {
        }

        void OnStart(SystemContext &context) override
        {
            mMeshQuery = &context.world.GetOrCreateQuery<const WorldTransform, const MeshRenderer>();
            mCameraQuery = &context.world.GetOrCreateQuery<const WorldTransform, const Camera>();
        }

        void OnUpdate(SystemContext &) override
        {
            // First match wins
            bool haveCamera = false;
            glm::mat4 viewProj{1.0f};

            mCameraQuery->ForEach([&](Entity, const WorldTransform &world, const Camera &camera)
                                  {
                if (haveCamera)
                    return;

                viewProj = MakeViewProjection(world.Matrix(), camera.mFovYDegrees,
                                              camera.mNearZ, camera.mFarZ,
                                              mRenderer.AspectRatio());
                haveCamera = true; });

            mDrawItems.clear();

            if (haveCamera)
            {
                mMeshQuery->ForEach([this, &viewProj](Entity, const WorldTransform &world, const MeshRenderer &renderer)
                                    { mDrawItems.push_back(DrawItem{renderer.mesh, viewProj * world.Matrix(), renderer.tint}); });
            }
            else if (!mWarnedNoCamera)
            {
                MTS_LOG_WARN("RenderSystem: no Camera entity in the world; nothing will be drawn");
                mWarnedNoCamera = true;
            }

            mRenderer.DrawFrame(mDrawItems);
        }

    private:
        VulkanRenderer &mRenderer;
        Query<const WorldTransform, const MeshRenderer> *mMeshQuery = nullptr;
        Query<const WorldTransform, const Camera> *mCameraQuery = nullptr;

        /// Rebuilt every frame but keeps its capacity to steady allocation,
        std::vector<DrawItem> mDrawItems;

        bool mWarnedNoCamera = false;
    };
}
