/**
 * @file LogHistory.h
 * @author Rahul Nair
 * @brief In-memory ring buffer of recent log entries, for UI consumption.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include "core/log/Log.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace mir
{
    struct LogEntry
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;
        int line = 0;
        std::chrono::system_clock::time_point time;
    };

    class LogHistory
    {
    public:
        LogHistory() = delete;

        static void SetCapacity(std::size_t capacity);
        static void Push(LogEntry entry);
        static std::vector<LogEntry> Snapshot();
        static void Clear();
    };
}
