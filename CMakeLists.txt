cmake_minimum_required(VERSION 3.26)

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
include(cmake/Shaders.cmake)
include(cmake/Assets.cmake)
include(cmake/Fonts.cmake)

option(BUILD_TESTING "Build unit tests (per module, run via ctest)" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(Catch2 3 CONFIG REQUIRED)
    include(CTest)
    include(Catch)
endif()

add_subdirectory(third_party)

add_subdirectory(modules/app)
add_subdirectory(modules/assets)
add_subdirectory(modules/core)
add_subdirectory(modules/editortheme)
add_subdirectory(modules/renderer)
add_subdirectory(modules/window)

add_subdirectory(tools/AssetCooker)

add_executable(HelloWorld)
target_sources(HelloWorld PRIVATE main.cpp)

engine_add_shaders(HelloWorld
    SOURCE assets/shaders/triangle.slang
    ENTRIES vertexMain:vertex fragmentMain:fragment)

engine_add_shaders(HelloWorld
    SOURCE assets/shaders/unlit.slang
    ENTRIES vertexMain:vertex fragmentMain:fragment)

engine_cook_assets(HelloWorld
    SOURCE_ROOTS assets/
    OUT_DIR ${CMAKE_BINARY_DIR}/cooked_assets)

engine_copy_fonts(HelloWorld SOURCE_DIR fonts)

target_link_libraries(HelloWorld PRIVATE
    mts::app
    mts::assets
    mts::core
)
