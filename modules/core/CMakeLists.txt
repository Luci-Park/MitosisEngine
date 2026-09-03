
engine_add_module(core)

target_sources(engine_core PRIVATE
    src/core/log/Log.cpp
    src/core/fs/Paths.cpp
    src/core/ecs/TransformHierarchy.cpp
    src/core/ecs/HierarchyIndex.cpp)

# get external libraries here
find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# spdlog stays private
target_link_libraries(engine_core PRIVATE spdlog::spdlog)

# glm is PUBLIC: glm types are in the public component headers, so anything
# that includes core/ecs/components/Transform.h needs glm's include path too
target_link_libraries(engine_core PUBLIC glm::glm)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()