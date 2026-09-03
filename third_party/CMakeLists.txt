# Dear ImGui, vendored as a submodule pinned to a tagged release on the
# docking branch (docs/decisions/0022-imgui-vendored-submodule.md - this is a
# deliberate exception to 0001, everything else comes through vcpkg.json).
#
# Never edit files under imgui/ directly - imconfig overrides live in
# imgui_config/ImGuiConfig.h instead, wired through IMGUI_USER_CONFIG.

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR
        "third_party/imgui is empty. Run: git submodule update --init --recursive")
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui_config
)

# Repo's vector types interop (glm::vec2/vec4 <-> ImVec2/ImVec4).
target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="ImGuiConfig.h")

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(imgui PRIVATE glfw)

# The renderer uses volk (VK_NO_PROTOTYPES), not the static Vulkan loader:
# the Vulkan backend resolves its functions through volk's already-loaded
# dispatch instead of linking vulkan-1.lib directly.
find_package(volk CONFIG REQUIRED)
target_link_libraries(imgui PUBLIC volk::volk_headers)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

if(ENGINE_PLATFORM STREQUAL "Windows")
    target_compile_definitions(imgui PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(ENGINE_PLATFORM STREQUAL "Linux")
    target_compile_definitions(imgui PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
        VK_USE_PLATFORM_WAYLAND_KHR)
endif()
