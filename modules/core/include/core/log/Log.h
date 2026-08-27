/**
 * @file Log.h
 * @author Sumin Park
 * @brief Logging macros
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <source_location>

namespace mts
{
    enum class LogLevel
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off,
    };

    struct LogConfig
    {
        std::string filePath = "logs/engine.log";
        LogLevel consoleLevel = LogLevel::Trace;
        LogLevel fileLevel = LogLevel::Trace;
        std::size_t maxFileBytes = 5 * 1024 * 1024;
        std::size_t maxFiles = 3;
    };

    void InitLog(const LogConfig &config = {});

    // Sometimes need manual flush
    void FlushLog();

    namespace detail
    {
        bool ShouldLog(LogLevel level);
        void Log(LogLevel level, std::string_view msg, std::source_location loc = std::source_location::current());
        void LogAssert(const char *expr, std::string_view msg, std::source_location loc = std::source_location::current());
    }
}

#define MTS_LOG_LEVEL_TRACE 0
#define MTS_LOG_LEVEL_DEBUG 1
#define MTS_LOG_LEVEL_INFO 2
#define MTS_LOG_LEVEL_WARN 3
#define MTS_LOG_LEVEL_ERROR 4
#define MTS_LOG_LEVEL_CRITICAL 5
#define MTS_LOG_LEVEL_OFF 6

// Compile-time floor. Anything below this is stripped entirely.
#ifndef MTS_ACTIVE_LOG_LEVEL
#ifdef NDEBUG
#define MTS_ACTIVE_LOG_LEVEL MTS_LOG_LEVEL_INFO
#else
#define MTS_ACTIVE_LOG_LEVEL MTS_LOG_LEVEL_TRACE
#endif
#endif

#define MTS_LOG_IMPL(level, ...)                                        \
    do                                                                  \
    {                                                                   \
        if (::mts::detail::ShouldLog(level))                            \
        {                                                               \
            ::mts::detail::Log(level, ::std::format(__VA_ARGS__));      \
        }                                                               \
    } while (0)

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_TRACE
#define MTS_LOG_TRACE(...) MTS_LOG_IMPL(::mts::LogLevel::Trace, __VA_ARGS__)
#else
#define MTS_LOG_TRACE(...) ((void)0)
#endif

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_DEBUG
#define MTS_LOG_DEBUG(...) MTS_LOG_IMPL(::mts::LogLevel::Debug, __VA_ARGS__)
#else
#define MTS_LOG_DEBUG(...) ((void)0)
#endif

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_INFO
#define MTS_LOG_INFO(...) MTS_LOG_IMPL(::mts::LogLevel::Info, __VA_ARGS__)
#else
#define MTS_LOG_INFO(...) ((void)0)
#endif

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_WARN
#define MTS_LOG_WARN(...) MTS_LOG_IMPL(::mts::LogLevel::Warn, __VA_ARGS__)
#else
#define MTS_LOG_WARN(...) ((void)0)
#endif

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_ERROR
#define MTS_LOG_ERROR(...) MTS_LOG_IMPL(::mts::LogLevel::Error, __VA_ARGS__)
#else
#define MTS_LOG_ERROR(...) ((void)0)
#endif

#if MTS_ACTIVE_LOG_LEVEL <= MTS_LOG_LEVEL_CRITICAL
#define MTS_LOG_CRITICAL(...) MTS_LOG_IMPL(::mts::LogLevel::Critical, __VA_ARGS__)
#else
#define MTS_LOG_CRITICAL(...) ((void)0)
#endif
