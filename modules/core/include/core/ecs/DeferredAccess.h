/**
 * @file DeferredAccess.h
 * @author Sumin Park
 * @brief Structural change from a caller that does not know if a walk is live
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "CommandBuffer.h"
#include "ComponentRegistry.h"
#include "Entity.h"
#include "World.h"

namespace mir
{
    // CommandBuffer used for each frame
    struct FrameCommands
    {
        CommandBuffer *mBuffer = nullptr;
    };

    // The published frame buffer, or nullptr when none was installed.
    CommandBuffer *FrameCommandBuffer(World &world);

    bool AddComponentOrDefer(World &world, Entity entity, const ComponentOps &ops, const void *value);

    bool AddDefaultComponentOrDefer(World &world, Entity entity, const ComponentOps &ops);

    bool RemoveComponentOrDefer(World &world, Entity entity, const ComponentOps &ops);

    bool DestroyEntityOrDefer(World &world, Entity entity);
}
