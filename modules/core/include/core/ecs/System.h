/**
 * @file System.h
 * @author Sumin Park
 * @brief System interface, execution phases, and the per-frame context
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstdint>

namespace mts
{
    class World;
    class CommandBuffer;

    /// Ordering primitive : each system will execute in the order of assigned phase -> registrated order
    enum class SystemPhase : uint8_t
    {
        PreUpdate,
        Update,
        PostUpdate,
        Render, ///< reserved; the renderer is still driven directly by App
        kCount
    };

    /// Everything a system is handed for one tick.
    struct SystemContext
    {
        World &world;
        CommandBuffer &commands;
        float dt = 0.0f;      ///< seconds since the previous frame
        double elapsed = 0.0; ///< seconds since the first frame
        uint64_t frame = 0;
    };

    /// A unit of per-frame work over the World.
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        ISystem(const ISystem &) = delete;
        ISystem &operator=(const ISystem &) = delete;
        ISystem(ISystem &&) = delete;
        ISystem &operator=(ISystem &&) = delete;

        /// Once, before the first OnUpdate. Cache queries here.
        virtual void OnStart(SystemContext &) {}

        /// Called on target phase
        virtual void OnUpdate(SystemContext &context) = 0;

        /// Once, at teardown. Runs in reverse registration order.
        virtual void OnStop(SystemContext &) {}

    protected:
        ISystem() = default;
    };
}
