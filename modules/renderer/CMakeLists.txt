engine_add_module(renderer)

target_sources(engine_renderer PRIVATE
    src/renderer/Renderer.cpp
    src/renderer/VulkanRenderer.cpp
)

find_package(Vulkan REQUIRED)
# volk: function pointers w/o boiler plate
find_package(volk CONFIG REQUIRED)
# VMA : vulkan memory manager
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# temp for renderer
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(engine_renderer
    PUBLIC volk::volk_headers
            mts::core
            mts::window
    PRIVATE volk::volk
            GPUOpen::VulkanMemoryAllocator
            glfw
)

# makes VMA compatible with volk
target_compile_definitions(engine_renderer PRIVATE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)
