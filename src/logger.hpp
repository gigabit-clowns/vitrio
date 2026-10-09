// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#define VITRIO_LOG_LEVEL_TRACE SPDLOG_LEVEL_TRACE
#define VITRIO_LOG_LEVEL_DEBUG SPDLOG_LEVEL_DEBUG
#define VITRIO_LOG_LEVEL_INFO SPDLOG_LEVEL_INFO
#define VITRIO_LOG_LEVEL_WARN SPDLOG_LEVEL_WARN
#define VITRIO_LOG_LEVEL_ERROR SPDLOG_LEVEL_ERROR
#define VITRIO_LOG_LEVEL_CRITICAL SPDLOG_LEVEL_CRITICAL

// Messages below this level are compiled out. It has to be settled before
// spdlog is included.
#ifdef VITRIO_LOG_LEVEL
	#define SPDLOG_ACTIVE_LEVEL VITRIO_LOG_LEVEL
#endif

#include <spdlog/spdlog.h>

namespace vitrio
{

/**
 * @brief Get the logger the library logs to.
 *
 * The logger belongs to the library: it is neither the default logger of
 * spdlog nor one registered with it. It writes to the standard error stream.
 *
 * @return spdlog::logger& The logger. It is the same one in every call.
 */
spdlog::logger& get_logger();

} // namespace vitrio

#define VITRIO_LOG_TRACE(...) \
	SPDLOG_LOGGER_TRACE(&::vitrio::get_logger(), __VA_ARGS__)
#define VITRIO_LOG_DEBUG(...) \
	SPDLOG_LOGGER_DEBUG(&::vitrio::get_logger(), __VA_ARGS__)
#define VITRIO_LOG_INFO(...) \
	SPDLOG_LOGGER_INFO(&::vitrio::get_logger(), __VA_ARGS__)
#define VITRIO_LOG_WARN(...) \
	SPDLOG_LOGGER_WARN(&::vitrio::get_logger(), __VA_ARGS__)
#define VITRIO_LOG_ERROR(...) \
	SPDLOG_LOGGER_ERROR(&::vitrio::get_logger(), __VA_ARGS__)
#define VITRIO_LOG_CRITICAL(...) \
	SPDLOG_LOGGER_CRITICAL(&::vitrio::get_logger(), __VA_ARGS__)
