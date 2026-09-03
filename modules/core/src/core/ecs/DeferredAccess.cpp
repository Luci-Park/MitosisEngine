/**
 * @file DeferredAccess.cpp
 * @author Sumin Park
 * @brief Structural change from a caller that does not know if a walk is live
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/DeferredAccess.h"

#include "core/log/Assert.h"

namespace mts
{
    namespace
    {
        /// The buffer, complaining if a defer is needed and there is none. The
        /// assert is aimed at whoever wired the application up, not at the
        /// script that tripped it: a binding reached during a walk with no
        /// buffer installed has no correct action left.
        CommandBuffer *RequireFrameCommands(World &world)
        {
            CommandBuffer *commands = FrameCommandBuffer(world);

            MTS_ASSERT(commands != nullptr,
                       "DeferredAccess: a structural change was requested during a query walk, but no "
                       "FrameCommands resource is installed. Emplace one pointing at the frame's "
                       "CommandBuffer during setup.");

            return commands;
        }
    }

    CommandBuffer *FrameCommandBuffer(World &world)
    {
        FrameCommands *frame = world.TryResource<FrameCommands>();
        return frame == nullptr ? nullptr : frame->mBuffer;
    }

    bool AddComponentOrDefer(World &world, Entity entity, const ComponentOps &ops, const void *value)
    {
        if (!world.IsAlive(entity))
            return false;

        if (!world.IsIterating())
        {
            ops.AddCopy(world, entity, value);
            return true;
        }

        CommandBuffer *commands = RequireFrameCommands(world);
        if (commands == nullptr)
            return false;

        ops.DeferAdd(*commands, entity, value);
        return true;
    }

    bool AddDefaultComponentOrDefer(World &world, Entity entity, const ComponentOps &ops)
    {
        return AddComponentOrDefer(world, entity, ops, ops.mDefaultValue.data());
    }

    bool RemoveComponentOrDefer(World &world, Entity entity, const ComponentOps &ops)
    {
        if (!world.IsAlive(entity))
            return false;

        if (!world.IsIterating())
        {
            ops.Remove(world, entity);
            return true;
        }

        CommandBuffer *commands = RequireFrameCommands(world);
        if (commands == nullptr)
            return false;

        ops.DeferRemove(*commands, entity);
        return true;
    }

    bool DestroyEntityOrDefer(World &world, Entity entity)
    {
        if (!world.IsAlive(entity))
            return false;

        if (!world.IsIterating())
        {
            world.DestroyEntity(entity);
            return true;
        }

        CommandBuffer *commands = RequireFrameCommands(world);
        if (commands == nullptr)
            return false;

        commands->Destroy(entity);
        return true;
    }
}
