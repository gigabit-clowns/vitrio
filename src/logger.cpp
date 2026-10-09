// SPDX-License-Identifier: LGPL-2.1-or-later

#include "logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

namespace vitrio
{

spdlog::logger& get_logger()
{
	static spdlog::logger logger(
		"vitrio",
		std::make_shared<spdlog::sinks::stderr_color_sink_mt>()
	);

	return logger;
}

} // namespace vitrio
