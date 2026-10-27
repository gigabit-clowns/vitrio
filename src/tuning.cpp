// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tuning.hpp"

#include <config.hpp>
#include <logger.hpp>

#include <system/environment_variable.hpp>

#include <exception>
#include <limits>

namespace vitrio
{

namespace
{

// What a variable of the environment holds, or none where it is not set or
// can not be read.
boost::optional<std::string> read_variable(const char *name) noexcept
{
	try
	{
		return get_environment_variable(name);
	}
	catch (const std::exception &)
	{
		return boost::none;
	}
}

std::size_t read_value(const char *name, std::size_t fallback) noexcept
{
	return parse_tuning_value(name, read_variable(name), fallback);
}

// A whole number above zero that fits, or zero.
std::size_t parse_whole_number(const std::string &text) noexcept
{
	if (text.empty())
	{
		return 0;
	}

	const auto largest = std::numeric_limits<std::size_t>::max();
	std::size_t value = 0;
	for (const auto character : text)
	{
		if (character < '0' || character > '9')
		{
			return 0;
		}

		const auto digit = static_cast<std::size_t>(character - '0');
		if (value > (largest - digit) / 10)
		{
			return 0;
		}
		value = value * 10 + digit;
	}

	return value;
}

} // anonymous namespace

std::size_t parse_tuning_value(
	const char *name,
	const boost::optional<std::string> &text,
	std::size_t fallback
) noexcept
{
	if (!text)
	{
		return fallback;
	}

	const auto value = parse_whole_number(*text);
	if (value == 0)
	{
		VITRIO_LOG_WARN(
			"{} holds \"{}\", which is not a whole number above zero, and {} "
			"is taken instead.",
			name,
			*text,
			fallback
		);
		return fallback;
	}

	return value;
}

bool parse_tuning_flag(const boost::optional<std::string> &text) noexcept
{
	return text && !text->empty() && *text != "0";
}

std::size_t get_prefetch_gap_cap() noexcept
{
	static const auto value = read_value(
		"VITRIO_PREFETCH_GAP_CAP",
		VITRIO_DEFAULT_PREFETCH_GAP_CAP
	);
	return value;
}

std::size_t get_prefetch_byte_budget() noexcept
{
	static const auto value = read_value(
		"VITRIO_PREFETCH_BYTE_BUDGET",
		VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET
	);
	return value;
}

std::size_t get_staging_byte_budget() noexcept
{
	static const auto value = read_value(
		"VITRIO_STAGING_BYTE_BUDGET",
		VITRIO_DEFAULT_STAGING_BYTE_BUDGET
	);
	return value;
}

std::size_t get_scratch_load_byte_budget() noexcept
{
	static const auto value = read_value(
		"VITRIO_SCRATCH_LOAD_BYTE_BUDGET",
		VITRIO_DEFAULT_SCRATCH_LOAD_BYTE_BUDGET
	);
	return value;
}

std::size_t get_file_transfer_queue_capacity() noexcept
{
	static const auto value = read_value(
		"VITRIO_FILE_TRANSFER_QUEUE_CAPACITY",
		VITRIO_DEFAULT_FILE_TRANSFER_QUEUE_CAPACITY
	);
	return value;
}

std::size_t get_file_transfer_workers() noexcept
{
	static const auto value = read_value(
		"VITRIO_FILE_TRANSFER_WORKERS",
		VITRIO_DEFAULT_FILE_TRANSFER_WORKERS
	);
	return value;
}

bool is_asynchronous_io_disabled() noexcept
{
	static const bool disabled =
		parse_tuning_flag(read_variable("VITRIO_DISABLE_ASYNCHRONOUS_IO"));
	return disabled;
}

} // namespace vitrio
