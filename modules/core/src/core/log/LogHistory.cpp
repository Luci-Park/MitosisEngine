#include "core/log/LogHistory.h"

#include <deque>
#include <mutex>

namespace mir
{
    namespace
    {
        std::mutex gMutex;
        std::deque<LogEntry> gEntries;
        std::size_t gCapacity = 2000;
    }

    void LogHistory::SetCapacity(std::size_t capacity)
    {
        std::lock_guard lock(gMutex);
        gCapacity = capacity;
        while (gEntries.size() > gCapacity)
            gEntries.pop_front();
    }

    void LogHistory::Push(LogEntry entry)
    {
        std::lock_guard lock(gMutex);
        if (gCapacity == 0)
            return;
        if (gEntries.size() >= gCapacity)
            gEntries.pop_front();
        gEntries.push_back(std::move(entry));
    }

    std::vector<LogEntry> LogHistory::Snapshot()
    {
        std::lock_guard lock(gMutex);
        return std::vector<LogEntry>(gEntries.begin(), gEntries.end());
    }

    void LogHistory::Clear()
    {
        std::lock_guard lock(gMutex);
        gEntries.clear();
    }
}
