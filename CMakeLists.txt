cmake_minimum_required(VERSION 3.25)

project(Mitosis
    VERSION 0.1.0
    DESCRIPTION "A modular 3D engine"
    LANGUAGES CXX)

# C++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

include(cmake/EnginePlatform.cmake)
include(cmake/EngineModule.cmake)

add_subdirectory(modules/core)
add_subdirectory(modules/window)

add_executable(HelloWorld)
target_sources(HelloWorld PRIVATE main.cpp)

target_link_libraries(HelloWorld PRIVATE
    mts::core
    mts::window
)
