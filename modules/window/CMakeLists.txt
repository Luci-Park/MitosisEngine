engine_add_module(window)

target_link_libraries(engine_window PUBLIC mir::core)

target_sources(engine_window PRIVATE src/window/InputCodes.cpp)

if(ENGINE_FAMILY_DESKTOP)
    target_sources(engine_window PRIVATE src/window/glfw/GLFWWindow.cpp)

    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(engine_window PRIVATE glfw)
    if(ENGINE_PLATFORM STREQUAL "Windows")
        target_sources(engine_window PRIVATE src/window/glfw/GLFWWin32.cpp src/window/splash/SplashWin32.cpp)
        target_link_libraries(engine_window PRIVATE windowscodecs ole32)
    elseif(ENGINE_PLATFORM STREQUAL "Linux")
        target_sources(engine_window PRIVATE src/window/glfw/GLFWLinux.cpp src/window/splash/SplashLinux.cpp)
    else()
        message(FATAL_ERROR "No native window backend for ${ENGINE_PLATFORM}")
    endif()
endif()