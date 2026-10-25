// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <tuning.hpp>

#include "fixtures/captured_log.hpp"

#include <config.hpp>
#include <system/environment_variable.hpp>

#include <boost/optional.hpp>

#include <cstddef>
#include <string>

using namespace vitrio;

namespace
{

boost::optional<std::string> holding(const char *text)
{
	return std::string(text);
}

} // anonymous namespace

TEST_CASE(
	"parse_tuning_value takes a whole number above zero",
	"[tuning]"
)
{
	CHECK( parse_tuning_value("A", holding("128"), 64) == 128 );
	CHECK( parse_tuning_value("A", holding("1"), 64) == 1 );
}

TEST_CASE(
	"parse_tuning_value keeps the fallback where the variable is not set",
	"[tuning]"
)
{
	const captured_log log;
	CHECK( parse_tuning_value("A", boost::none, 64) == 64 );
	CHECK( log.get().empty() );
}

TEST_CASE(
	"parse_tuning_value keeps the fallback, and logs it, where the variable "
	"holds anything else",
	"[tuning]"
)
{
	const char *const refused[] = {
		"", "0", "-3", "12x", " 12", "1.5", "99999999999999999999999999",
	};
	for (const auto *text : refused)
	{
		const captured_log log;
		CHECK( parse_tuning_value("VITRIO_A", holding(text), 64) == 64 );
		CHECK( log.get().find("VITRIO_A") != std::string::npos );
	}
}

TEST_CASE(
	"parse_tuning_flag is set by anything but nothing and 0",
	"[tuning]"
)
{
	CHECK_FALSE( parse_tuning_flag(boost::none) );
	CHECK_FALSE( parse_tuning_flag(holding("")) );
	CHECK_FALSE( parse_tuning_flag(holding("0")) );
	CHECK( parse_tuning_flag(holding("1")) );
	CHECK( parse_tuning_flag(holding("yes")) );
}

TEST_CASE(
	"each tuning value is its default unless the environment overrides it",
	"[tuning]"
)
{
	const auto expected = [](const char *name, std::size_t macro)
	{
		return parse_tuning_value(name, get_environment_variable(name), macro);
	};

	CHECK( get_prefetch_gap_cap() == expected(
		"VITRIO_PREFETCH_GAP_CAP",
		VITRIO_DEFAULT_PREFETCH_GAP_CAP
	) );
	CHECK( get_prefetch_byte_budget() == expected(
		"VITRIO_PREFETCH_BYTE_BUDGET",
		VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET
	) );
	CHECK( get_staging_byte_budget() == expected(
		"VITRIO_STAGING_BYTE_BUDGET",
		VITRIO_DEFAULT_STAGING_BYTE_BUDGET
	) );
	CHECK( get_chunk_byte_budget() == expected(
		"VITRIO_CHUNK_BYTE_BUDGET",
		VITRIO_DEFAULT_CHUNK_BYTE_BUDGET
	) );
	CHECK( get_chunks_in_flight() == expected(
		"VITRIO_CHUNKS_IN_FLIGHT",
		VITRIO_DEFAULT_CHUNKS_IN_FLIGHT
	) );
	CHECK( get_zarr_image_chunk_edge() == expected(
		"VITRIO_ZARR_IMAGE_CHUNK_EDGE",
		VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE
	) );
	CHECK( get_zarr_volume_chunk_edge() == expected(
		"VITRIO_ZARR_VOLUME_CHUNK_EDGE",
		VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE
	) );
	CHECK( get_chunk_write_byte_budget() == expected(
		"VITRIO_CHUNK_WRITE_BYTE_BUDGET",
		VITRIO_DEFAULT_CHUNK_WRITE_BYTE_BUDGET
	) );
	CHECK( get_file_transfer_queue_capacity() == expected(
		"VITRIO_FILE_TRANSFER_QUEUE_CAPACITY",
		VITRIO_DEFAULT_FILE_TRANSFER_QUEUE_CAPACITY
	) );
	CHECK( get_file_transfer_workers() == expected(
		"VITRIO_FILE_TRANSFER_WORKERS",
		VITRIO_DEFAULT_FILE_TRANSFER_WORKERS
	) );
	CHECK( is_asynchronous_io_disabled() ==
		parse_tuning_flag(
			get_environment_variable("VITRIO_DISABLE_ASYNCHRONOUS_IO")
		) );
}
