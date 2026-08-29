cmake_minimum_required(VERSION 3.25)

# Must come before project(): CMAKE_TOOLCHAIN_FILE is only honoured there.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/VcpkgToolchain.cmake)

project(Mitosis
    VERSION 0.1.0
    DESCRIPTION "A modular 3D engine"
    # C is needed for volk.c
    LANGUAGES CXX C)

# C++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

# Editing the manifest must re-run configure, otherwise vcpkg never installs
# the new dependency and IntelliSense keeps the stale include paths.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")

include(cmake/EnginePlatform.cmake)
include(cmake/EngineModule.cmake)

add_subdirectory(modules/core)
add_subdirectory(modules/renderer)
add_subdirectory(modules/window)

add_executable(HelloWorld)
target_sources(HelloWorld PRIVATE main.cpp)

target_link_libraries(HelloWorld PRIVATE
    mts::core
    mts::renderer
    mts::window
)
