#include <core/log/Log.h>
#include <window/Window.h>
#include <renderer/VulkanRenderer.h>
int main()
{
    mts::InitLog();

    mts::WindowDesc desc{};
    desc.m_title = "MitosisEngine - Window Test";

    auto window = mts::Window::Create(desc);

    mts::VulkanRenderer renderer;
    if (!renderer.Initialize({.window = window.get(), .appName = "MitosisEngine", .enableValidation = true}))
    {
        mts::FlushLog();
        return -1;
    }
    while (!window->ShouldClose())
    {
        window->PollEvents();
        renderer.DrawFrame();
    }
    renderer.Shutdown();

    MTS_LOG_INFO("Window closed");
    mts::FlushLog();
    return 0;
}
