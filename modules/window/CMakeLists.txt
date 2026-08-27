engine_add_module(window)

target_link_libraries(engine_window PUBLIC mts::core)

if(ENGINE_FAMILY_DESKTOP)
    target_sources(engine_window PRIVATE src/window/glfw/GLFWWindow.cpp)

    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(engine_window PRIVATE glfw)

elseif(ENGINE_PLATFORM STREQUAL "Android")
    target_sources(engine_window PRIVATE src/window/android/AndroidWindow.cpp)
    target_link_libraries(engine_window PRIVATE android log)

elseif(ENGINE_PLATFORM STREQUAL "iOS")
    target_sources(engine_window PRIVATE src/window/ios/IOSWindow.mm)

elseif(ENGINE_FAMILY_WEB)
    target_sources(engine_window PRIVATE src/window/web/WebWindow.cpp)

endif()
