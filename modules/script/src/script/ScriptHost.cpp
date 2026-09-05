#include <script/ScriptHost.h>

#include <core/log/Log.h>

#include <sol/sol.hpp>

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
        }

        sol::state mLua;
        std::unordered_map<std::string, sol::object> mLoadedScripts;
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

    int32_t ScriptHost::CreateInstance(std::string_view /*scriptName*/)
    {
        // Per-entity instances start in the next stage, once ScriptRef exists.
        return -1;
    }

    void ScriptHost::DestroyInstance(int32_t /*instanceRef*/)
    {
    }

    void ScriptHost::CallOnStart(World &, CommandBuffer &, Entity, int32_t)
    {
    }

    void ScriptHost::CallOnUpdate(World &, CommandBuffer &, Entity, int32_t, float)
    {
    }

    void ScriptHost::CallOnStop(World &, CommandBuffer &, Entity, int32_t)
    {
    }
}
