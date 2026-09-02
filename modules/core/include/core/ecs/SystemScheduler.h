/**
 * @file SystemScheduler.h
 * @author Sumin Park
 * @brief Owns systems and runs them in phase order
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "CommandBuffer.h"
#include "System.h"
#include "World.h"
#include "core/log/Assert.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace mts
{
    /**
     * Runs registered systems once per frame, phase by phase, flushing the
     * command buffer at each phase boundary.
     *
     * Flush granularity is per phase, not per system: systems in one phase see
     * the same world, and a spawn is observable at the next phase. An entity
     * created this phase has a valid handle immediately but no visible
     * components until the boundary.
     */
    class SystemScheduler
    {
    public:
        SystemScheduler() = default;

        SystemScheduler(const SystemScheduler &) = delete;
        SystemScheduler &operator=(const SystemScheduler &) = delete;
        SystemScheduler(SystemScheduler &&) = delete;
        SystemScheduler &operator=(SystemScheduler &&) = delete;

        // Constructs S in place and returns it, so a caller can keep a typed
        // reference for configuration. Ownership stays here.
        template <typename S, typename... Args>
        S &Add(SystemPhase phase, Args &&...args)
        {
            static_assert(std::is_base_of_v<ISystem, S>, "SystemScheduler::Add: S must derive from ISystem");
            MTS_ASSERT(!mStarted, "SystemScheduler::Add: systems must be registered before Start");

            auto system = std::make_unique<S>(std::forward<Args>(args)...);
            S &ref = *system;
            mPhases[Index(phase)].push_back(std::move(system));
            return ref;
        }

        void Start(SystemContext &context)
        {
            MTS_ASSERT(!mStarted, "SystemScheduler::Start: already started");
            mStarted = true;

            for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
            {
                for (const std::unique_ptr<ISystem> &system : mPhases[phase])
                    system->OnStart(context);

                context.commands.Flush(context.world);
            }
        }

        // will call all systems in all phases in order
        void Update(SystemContext &context)
        {
            MTS_ASSERT(mStarted, "SystemScheduler::Update: Start was never called");

            for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
            {
                for (const std::unique_ptr<ISystem> &system : mPhases[phase])
                    system->OnUpdate(context);

                context.commands.Flush(context.world);
            }
        }

        // reverse of Start
        void Stop(SystemContext &context)
        {
            if (!mStarted)
                return;

            for (std::size_t phase = kPhaseCount; phase-- > 0;)
            {
                for (std::size_t i = mPhases[phase].size(); i-- > 0;)
                    mPhases[phase][i]->OnStop(context);

                context.commands.Flush(context.world);
            }

            mStarted = false;
        }

        /// Drops every registered system so Add and Start may be used again.
        /// Stop has to have run first: the systems are about to be destroyed,
        /// and OnStop is their only chance to release anything.
        void Reset()
        {
            MTS_ASSERT(!mStarted, "SystemScheduler::Reset: Stop must run before Reset");

            for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
                mPhases[phase].clear();
        }

        bool Started() const { return mStarted; }

        std::size_t SystemCount(SystemPhase phase) const { return mPhases[Index(phase)].size(); }

        std::size_t SystemCount() const
        {
            std::size_t total = 0;
            for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
                total += mPhases[phase].size();
            return total;
        }

    private:
        static constexpr std::size_t kPhaseCount = static_cast<std::size_t>(SystemPhase::kCount);

        static constexpr std::size_t Index(SystemPhase phase) { return static_cast<std::size_t>(phase); }

        std::vector<std::unique_ptr<ISystem>> mPhases[kPhaseCount];
        bool mStarted = false;
    };
}
