engine_add_module(window)

target_link_libraries(engine_window PUBLIC mts::core)

if(ENGINE_FAMILY_DESKTOP)
    target_sources(engine_window PRIVATE src/window/glfw/GLFWWindow.cpp)

    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(engine_window PRIVATE glfw)
    if(ENGINE_PLATFORM STREQUAL "Windows")
        target_sources(engine_window PRIVATE src/window/glfw/GLFWWin32.cpp)
    elseif(ENGINE_PLATFORM STREQUAL "macOS")
    else()
        target_sources(engine_window PRIVATE src/window/glfw/GLFWLinux.cpp)
    endif()
endif()