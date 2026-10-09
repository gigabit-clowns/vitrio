// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <logger.hpp>

#include <spdlog/sinks/ostream_sink.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief Keeps what the library logs until a case leaves it, instead of
 * letting it out.
 */
class captured_log
{
public:
	captured_log()
		: m_previous(get_logger().sinks())
	{
		get_logger().sinks() = {
			std::make_shared<spdlog::sinks::ostream_sink_mt>(m_captured)
		};
	}

	captured_log(const captured_log &other) = delete;
	captured_log(captured_log &&other) = delete;

	~captured_log()
	{
		get_logger().sinks() = m_previous;
	}

	captured_log& operator=(const captured_log &other) = delete;
	captured_log& operator=(captured_log &&other) = delete;

	std::string get() const
	{
		return m_captured.str();
	}

private:
	std::ostringstream m_captured;
	std::vector<spdlog::sink_ptr> m_previous;
};

} // namespace vitrio
