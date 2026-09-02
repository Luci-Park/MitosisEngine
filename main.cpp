#include <app/App.h>
#include <core/log/Log.h>

int main()
{
    // Logging lives outside App so early construction failures are still visible.
    mts::InitLog();

    mts::App app;

    mts::AppDesc desc{};
    desc.mTitle = "MitosisEngine - Window Test";

    if (!app.Initialize(desc))
    {
        mts::FlushLog();
        return -1;
    }

    app.Run();
    app.Shutdown();

    mts::FlushLog();
    return 0;
}
