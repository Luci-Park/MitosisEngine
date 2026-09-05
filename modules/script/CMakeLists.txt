engine_add_module(script)

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

target_include_directories(engine_script PRIVATE ${LUA_INCLUDE_DIR})

target_link_libraries(engine_script
    PUBLIC  mts::core
    PRIVATE sol2::sol2
            ${LUA_LIBRARIES}
)

target_sources(engine_script PRIVATE
    src/script/ScriptHost.cpp
    src/script/bindings/EntityBindings.cpp
    src/script/bindings/WorldBindings.cpp
)
