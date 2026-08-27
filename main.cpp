#include <stdio.h>
#include <core/log/Log.h>
int main()
{
    mts::InitLog();
    MTS_LOG_INFO("Hello World");
    mts::FlushLog();
    return 0;
}