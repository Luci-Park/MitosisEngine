/**
 * @file Log.cpp
 * @author Sumin Park
 * @brief Logging macros
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "core/log/Log.h"
#include "core/log/Assert.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <memory>

namespace mts
{
    namespace
    {
        bool s_initialized = false;

        spdlog::level::level_enum ToSpd(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return spdlog::level::trace;
            case LogLevel::Debug:
                return spdlog::level::debug;
            case LogLevel::Info:
                return spdlog::level::info;
            case LogLevel::Warn:
                return spdlog::level::warn;
            case LogLevel::Error:
                return spdlog::level::err;
            case LogLevel::Critical:
                return spdlog::level::critical;
            case LogLevel::Off:
                return spdlog::level::off;
            }
            return spdlog::level::info;
        }

        spdlog::source_loc ToSpdLoc(const std::source_location &loc)
        {
            return spdlog::source_loc{
                loc.file_name(), static_cast<int>(loc.line()), loc.function_name()};
        }
    }

    void InitLog(const LogConfig &config)
    {
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_level(ToSpd(config.consoleLevel));

        auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.filePath, config.maxFileBytes, config.maxFiles);
        file->set_level(ToSpd(config.fileLevel));

        auto logger = std::make_shared<spdlog::logger>(
            "engine", spdlog::sinks_init_list{console, file});

        logger->set_level(std::min(ToSpd(config.consoleLevel), ToSpd(config.fileLevel)));
        logger->set_pattern("%^[%T] [%l] %v%$ (%s:%#)");
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(std::move(logger));
        s_initialized = true;
    }

    void FlushLog()
    {
        if (s_initialized)
        {
            spdlog::default_logger_raw()->flush();
        }
    }

    namespace detail
    {
        bool ShouldLog(LogLevel level)
        {
            if (!s_initialized)
            {
                return false;
            }
            return spdlog::default_logger_raw()->should_log(ToSpd(level));
        }

        void Log(LogLevel level, std::string_view msg, std::source_location loc)
        {
            if (!s_initialized)
            {
                return;
            }
            spdlog::default_logger_raw()->log(
                ToSpdLoc(loc), ToSpd(level), "{}", msg);
        }

        void LogAssert(const char *expr, std::string_view msg, std::source_location loc)
        {
            if (!s_initialized)
            {
                return;
            }

            auto *logger = spdlog::default_logger_raw();
            if (msg.empty())
            {
                logger->log(ToSpdLoc(loc), spdlog::level::critical,
                            "Assertion failed: ({})", expr);
            }
            else
            {
                logger->log(ToSpdLoc(loc), spdlog::level::critical,
                            "Assertion failed: ({}) {}", expr, msg);
            }
            logger->flush();
        }
    }
}
