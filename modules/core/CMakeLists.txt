
engine_add_module(core)

target_sources(engine_core PRIVATE
    src/core/log/Log.cpp
    src/core/fs/Paths.cpp)

# get external libraries here
find_package(spdlog CONFIG REQUIRED)

# spdlog stays private
target_link_libraries(engine_core PRIVATE spdlog::spdlog)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()