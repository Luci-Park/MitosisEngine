#include <core/log/Log.h>
#include <window/Window.h>

int main()
{
    mts::InitLog();

    mts::WindowDesc desc{};
    desc.m_title = "MitosisEngine - Window Test";

    auto window = mts::Window::Create(desc);

    uint32_t lastWidth = window->Width();
    uint32_t lastHeight = window->Height();
    MTS_LOG_INFO("Native handle: {}", window->NativeWindow().backend);

    while (!window->ShouldClose())
    {
        window->PollEvents();

        // Only log on change; the loop is uncapped and would spam otherwise.
        if (window->Width() != lastWidth || window->Height() != lastHeight)
        {
            lastWidth = window->Width();
            lastHeight = window->Height();
            MTS_LOG_INFO("Resized: {}x{}", lastWidth, lastHeight);
        }
    }

    MTS_LOG_INFO("Window closed");
    mts::FlushLog();
    return 0;
}
