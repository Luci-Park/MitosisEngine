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

namespace mts
{
    /**
     * The frame's CommandBuffer, published as a resource.
     *
     * A system is handed one in its SystemContext, so it never needs this. A
     * binding does: a script function that adds a component may be called from
     * inside a ForEach - a script system walking entities - or from outside one
     * - a console, an input callback, a collision event. It cannot know which,
     * and it has no SystemContext either way. Publishing the buffer where any
     * caller with a World can find it is what lets one binding be correct from
     * both places.
     *
     * A pointer rather than the buffer itself: the buffer is owned by whoever
     * drives the frame (App), and the scheduler flushes it at every phase
     * boundary. A second buffer living in a resource would be flushed by
     * nobody.
     */
    struct FrameCommands
    {
        CommandBuffer *mBuffer = nullptr;
    };

    /// The published frame buffer, or nullptr when none was installed.
    CommandBuffer *FrameCommandBuffer(World &world);

    /**
     * Adds, removes or destroys - immediately when it is safe, and through the
     * frame's CommandBuffer when a query walk is in flight.
     *
     * `World::IsIterating` is what makes this exact rather than a guess: it is
     * a depth maintained by the iteration guard, so it stays correct under a
     * re-entered walk. Mutating during one is not merely discouraged - an add
     * reallocates a ComponentColumn and dangles every reference the callback is
     * holding, silently, whenever the target archetype already existed.
     *
     * False means nothing happened: the entity was already dead (routine for a
     * script holding a handle across frames), or a defer was needed and no
     * FrameCommands resource was installed (an integration bug, which also
     * asserts).
     */
    bool AddComponentOrDefer(World &world, Entity entity, const ComponentOps &ops, const void *value);

    /// Adds the registry's default value - `T{}` for a C++ component, zeroes
    /// for a script-declared one.
    bool AddDefaultComponentOrDefer(World &world, Entity entity, const ComponentOps &ops);

    bool RemoveComponentOrDefer(World &world, Entity entity, const ComponentOps &ops);

    bool DestroyEntityOrDefer(World &world, Entity entity);
}
