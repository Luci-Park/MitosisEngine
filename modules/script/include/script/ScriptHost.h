/**
 * @file ScriptHost.h
 * @author Rahul Nair
 * @brief Lua-backed IScriptHost
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <script/IScriptHost.h>

#include <memory>

namespace mir
{
    struct ScriptHostImpl;

    class ScriptHost final : public IScriptHost
    {
    public:
        ScriptHost();
        ~ScriptHost() override;

        ScriptHost(const ScriptHost &) = delete;
        ScriptHost &operator=(const ScriptHost &) = delete;
        ScriptHost(ScriptHost &&) = delete;
        ScriptHost &operator=(ScriptHost &&) = delete;

        bool LoadScriptSource(std::string_view name, std::string_view source) override;
        bool ReloadScriptSource(std::string_view name, std::string_view source) override;
        void UnloadScript(std::string_view name) override;

        int32_t CreateInstance(std::string_view scriptName) override;
        void DestroyInstance(int32_t instanceRef) override;

        void CallOnStart(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef) override;
        void CallOnUpdate(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef,
                           float dt) override;
        void CallOnStop(World &world, CommandBuffer &commands, Entity entity, int32_t instanceRef) override;

    private:
        std::unique_ptr<ScriptHostImpl> mImpl;
    };
}
