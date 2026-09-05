/**
 * @file IScriptHost.h
 * @author Rahul Nair
 * @brief Abstract seam between the engine and whatever VM runs gameplay scripts
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <core/ecs/Entity.h>

#include <cstdint>
#include <string_view>

namespace mts
{
    class World;
    class CommandBuffer;

    class IScriptHost
    {
    public:
        virtual ~IScriptHost() = default;

        virtual bool LoadScriptSource(std::string_view name, std::string_view source) = 0;

        virtual bool ReloadScriptSource(std::string_view name, std::string_view source) = 0;

        virtual void UnloadScript(std::string_view name) = 0;

        virtual int32_t CreateInstance(std::string_view scriptName) = 0;
        virtual void DestroyInstance(int32_t instanceRef) = 0;

        virtual void CallOnStart(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef) = 0;
        virtual void CallOnUpdate(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef,
                                   float dt) = 0;
        virtual void CallOnStop(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef) = 0;
    };
}
