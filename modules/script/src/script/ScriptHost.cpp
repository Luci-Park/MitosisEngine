#include <script/ScriptHost.h>

#include "bindings/EntityBindings.h"
#include "bindings/WorldBindings.h"

#include <core/ecs/World.h>
#include <core/log/Log.h>

#include <sol/sol.hpp>

#include <functional>
#include <string>
#include <unordered_map>

namespace mts
{
    struct ScriptHostImpl
    {
        ScriptHostImpl()
        {
            mLua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table);

            // Lua's own print writes to stdout, which nothing here reads -
            // route script output through the same log every other subsystem
            // uses instead.
            mLua.set_function("print", [](sol::variadic_args args)
            {
                sol::state_view lua(args.lua_state());
                const sol::function tostring = lua["tostring"];

                std::string line;
                bool first = true;
                for (const auto arg : args)
                {
                    if (!first)
                        line += '\t';
                    first = false;
                    line += tostring(arg.get<sol::object>()).get<std::string>();
                }
                MTS_LOG_INFO("[lua] {}", line);
            });

            RegisterEntityBindings(mLua);
            RegisterWorldBindings(mLua);
        }

        int32_t CreateInstance(std::string_view scriptName)
        {
            const auto it = mLoadedScripts.find(std::string(scriptName));
            if (it == mLoadedScripts.end() || !it->second.is<sol::table>())
            {
                MTS_LOG_ERROR("script: CreateInstance: '{}' is not a loaded script table", scriptName);
                return -1;
            }

            const int32_t ref = mNextInstanceRef++;
            mInstances.emplace(ref, it->second.as<sol::table>());
            return ref;
        }

        void DestroyInstance(int32_t instanceRef)
        {
            mInstances.erase(instanceRef);
        }

        template <typename... Args>
        void CallCallback(int32_t instanceRef, const char *callbackName, Args &&...args)
        {
            const auto it = mInstances.find(instanceRef);
            if (it == mInstances.end())
                return; // destroyed or never created - routine, not an error

            const sol::object callback = it->second[callbackName];
            if (!callback.is<sol::protected_function>())
                return;

            const sol::protected_function fn = callback;
            const sol::protected_function_result result = fn(std::forward<Args>(args)...);
            if (!result.valid())
            {
                const sol::error err = result;
                MTS_LOG_ERROR("script: {} failed: {}", callbackName, err.what());
            }
        }

        sol::state mLua;
        std::unordered_map<std::string, sol::object> mLoadedScripts;
        std::unordered_map<int32_t, sol::table> mInstances;
        int32_t mNextInstanceRef = 0;
    };

    ScriptHost::ScriptHost() : mImpl(std::make_unique<ScriptHostImpl>())
    {
    }

    ScriptHost::~ScriptHost() = default;

    bool ScriptHost::LoadScriptSource(std::string_view name, std::string_view source)
    {
        const sol::protected_function_result result =
            mImpl->mLua.safe_script(source, sol::script_pass_on_error, std::string(name));

        if (!result.valid())
        {
            const sol::error err = result;
            MTS_LOG_ERROR("script: failed to load '{}': {}", name, err.what());
            return false;
        }

        mImpl->mLoadedScripts[std::string(name)] = result.get<sol::object>();
        return true;
    }

    bool ScriptHost::ReloadScriptSource(std::string_view name, std::string_view source)
    {
        return LoadScriptSource(name, source);
    }

    void ScriptHost::UnloadScript(std::string_view name)
    {
        mImpl->mLoadedScripts.erase(std::string(name));
    }

    int32_t ScriptHost::CreateInstance(std::string_view scriptName)
    {
        return mImpl->CreateInstance(scriptName);
    }

    void ScriptHost::DestroyInstance(int32_t instanceRef)
    {
        mImpl->DestroyInstance(instanceRef);
    }

    void ScriptHost::CallOnStart(World &world, CommandBuffer &, Entity entity, int32_t instanceRef)
    {
        mImpl->CallCallback(instanceRef, "OnStart", std::ref(world), entity);
    }

    void ScriptHost::CallOnUpdate(World &world, CommandBuffer &, Entity entity, int32_t instanceRef, float dt)
    {
        mImpl->CallCallback(instanceRef, "OnUpdate", std::ref(world), entity, dt);
    }

    void ScriptHost::CallOnStop(World &world, CommandBuffer &, Entity entity, int32_t instanceRef)
    {
        mImpl->CallCallback(instanceRef, "OnStop", std::ref(world), entity);
    }
}
