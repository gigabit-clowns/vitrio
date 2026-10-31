// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_read_operation.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>


#include "../../mock/mock_file_transfer_scope.hpp"
#include "zarr_test_store.hpp"

#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;
using trompeloeil::_;

namespace
{

using extents = std::vector<std::size_t>;

// A store of 4 by 4 bytes in chunks of 2 by 2, stored as they are: chunk
// (r, c) holds 10 * (2 r + c) + its position in it, which is the value of
// each byte. The operation reads a file in a task tagged with its index.
class read_operation_fixture
{
public:
	explicit read_operation_fixture(const char *compressor = "null")
		: m_directory("test_zarr_read_operation_" + std::to_string(
			std::hash<std::string>()(
				Catch::getResultCapture().getCurrentTestName())) + ".zarr")
		, m_destination(make_host_array<std::uint8_t>(
			{4, 4}, numerical_type::uint8, 255))
	{
		write_store_file(m_directory.get(), ".zarray",
			std::string(R"({"zarr_format": 2, "shape": [4, 4],
				"chunks": [2, 2], "dtype": "|u1", "order": "C",
				"fill_value": 99, "filters": null, "compressor": )") +
				compressor + "}");
		for (std::size_t chunk = 0; chunk < 4; ++chunk)
		{
			std::string bytes;
			for (std::size_t i = 0; i < 4; ++i)
			{
				bytes.push_back(static_cast<char>(10 * chunk + i));
			}
			write_store_file(m_directory.get(), key_of(chunk), bytes);
		}
	}

	static std::string key_of(std::size_t chunk)
	{
		return std::to_string(chunk / 2) + "." + std::to_string(chunk % 2);
	}

	std::string path_of(std::size_t chunk) const
	{
		return join_zarr_path(m_directory.get(), key_of(chunk));
	}

	std::unique_ptr<zarr_read_operation> make_operation(
		std::size_t chunks_in_flight = 64
	)
	{
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(m_directory.get()));
		const auto regions = whole_of({4, 4});
		image_chunk_regions chunks(
			regions,
			layout->get_shape(),
			layout->get_chunk_layout().get_extents()
		);
		return std::make_unique<zarr_read_operation>(
			m_directory.get(), layout, m_destination.share(),
			std::move(chunks), 1UL << 20, chunks_in_flight);
	}

	// Takes every task the operation runs, by its tag, and lets it queue
	// no transfer.
	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		m_no_reads = NAMED_FORBID_CALL(m_scope, queue_read(_, _, _, _));
		return NAMED_ALLOW_CALL(m_scope, run(_, _))
			.LR_SIDE_EFFECT( m_tasks[_2] = std::move(_1) );
	}

	// Runs a task the operation ran, and hands it its outcome.
	void finish(zarr_read_operation &operation, std::uint64_t tag)
	{
		auto work = std::move(m_tasks.at(tag));
		m_tasks.erase(tag);
		work->run();
		operation.on_task(tag, nullptr, m_scope);
	}

	std::vector<std::uint8_t> values() const
	{
		return get_values<std::uint8_t>(m_destination);
	}

	// What the array holds once every chunk is read, or with one of them
	// holding the fill value instead.
	static std::vector<std::uint8_t> expected(int filled = -1)
	{
		std::vector<std::uint8_t> values(16);
		for (std::size_t row = 0; row < 4; ++row)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				const auto chunk = (row / 2) * 2 + column / 2;
				const auto inside = (row % 2) * 2 + column % 2;
				values[row * 4 + column] = static_cast<int>(chunk) == filled
					? 99
					: static_cast<std::uint8_t>(10 * chunk + inside);
			}
		}
		return values;
	}

	scoped_directory m_directory;
	array m_destination;
	mock_file_transfer_scope m_scope;
	std::unique_ptr<trompeloeil::expectation> m_no_reads;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
};

} // anonymous namespace

TEST_CASE( "zarr_read_operation reads and decodes every chunk, a task a file",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	const auto tasks = fixture.take_tasks();
	auto operation = fixture.make_operation();

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 4 );
	REQUIRE( fixture.values() == std::vector<std::uint8_t>(16, 255) );

	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, chunk);
	}
	REQUIRE( fixture.m_tasks.empty() );
	REQUIRE( fixture.values() == read_operation_fixture::expected() );
}

TEST_CASE( "zarr_read_operation fills a chunk that is not stored",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	boost::filesystem::remove(fixture.path_of(1));
	const auto tasks = fixture.take_tasks();
	auto operation = fixture.make_operation();

	operation->start(fixture.m_scope);
	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, chunk);
	}
	REQUIRE( fixture.values() == read_operation_fixture::expected(1) );
}

TEST_CASE( "zarr_read_operation reports what fails, naming the chunk",
	"[zarr_read_operation]" )
{
	SECTION( "a chunk its codec refuses" )
	{
		read_operation_fixture fixture(R"({"id": "zlib"})");
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation();
		operation->start(fixture.m_scope);

		auto reading = std::move(fixture.m_tasks.at(0));
		REQUIRE_THROWS_MATCHES(
			reading->run(),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(fixture.path_of(0) + ": "))
		);
	}

	SECTION( "a stored chunk of another size than its values" )
	{
		read_operation_fixture fixture;
		write_store_file(fixture.m_directory.get(), "0.0", "abc");
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation();
		operation->start(fixture.m_scope);

		auto reading = std::move(fixture.m_tasks.at(0));
		REQUIRE_THROWS_AS(reading->run(), image_file_format_error);
	}

	SECTION( "a file far larger than any encoding of its chunk" )
	{
		read_operation_fixture fixture;
		write_store_file(fixture.m_directory.get(), "0.0",
			std::string(1UL << 17, 'x'));
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation();
		operation->start(fixture.m_scope);

		auto reading = std::move(fixture.m_tasks.at(0));
		REQUIRE_THROWS_MATCHES(
			reading->run(),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(fixture.path_of(0) + ": "))
		);
	}

	SECTION( "a task that failed" )
	{
		read_operation_fixture fixture;
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation();
		operation->start(fixture.m_scope);

		REQUIRE_THROWS_AS(
			operation->on_task(
				0,
				std::make_exception_ptr(std::runtime_error("read")),
				fixture.m_scope),
			std::runtime_error
		);
	}
}

TEST_CASE( "zarr_read_operation holds no more chunks than it may",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	const auto tasks = fixture.take_tasks();
	auto operation = fixture.make_operation(2);

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 2 );
	REQUIRE( fixture.m_tasks.count(2) == 0 );

	// A file done makes room for the next.
	fixture.finish(*operation, 0);
	REQUIRE( fixture.m_tasks.count(2) == 1 );
	REQUIRE( fixture.m_tasks.count(3) == 0 );
}

namespace
{

// A sharded store of the assets.
class sharded_read_fixture
{
public:
	sharded_read_fixture()
		: m_destination(make_host_array<std::uint16_t>(
			{6, 10}, numerical_type::uint16, 9999))
	{
	}

	std::unique_ptr<zarr_read_operation> make_operation()
	{
		const auto directory = get_zarr_asset_path("v3_uint16_sharded.zarr");
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(directory));
		const auto regions = whole_of({6, 10});
		image_chunk_regions chunks(
			regions,
			layout->get_shape(),
			layout->get_chunk_layout().get_extents()
		);
		return std::make_unique<zarr_read_operation>(
			directory, layout, m_destination.share(), std::move(chunks),
			1UL << 20, 64);
	}

	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		m_no_reads = NAMED_FORBID_CALL(m_scope, queue_read(_, _, _, _));
		return NAMED_ALLOW_CALL(m_scope, run(_, _))
			.LR_SIDE_EFFECT( m_tasks[_2] = std::move(_1) );
	}

	// Runs every task taken so far, in order.
	void finish_all(zarr_read_operation &operation)
	{
		std::vector<std::uint64_t> tags;
		for (const auto &task : m_tasks)
		{
			tags.push_back(task.first);
		}
		for (const auto tag : tags)
		{
			auto work = std::move(m_tasks.at(tag));
			m_tasks.erase(tag);
			work->run();
			operation.on_task(tag, nullptr, m_scope);
		}
	}

	array m_destination;
	mock_file_transfer_scope m_scope;
	std::unique_ptr<trompeloeil::expectation> m_no_reads;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
};

} // anonymous namespace

TEST_CASE( "zarr_read_operation reads the index of a shard, then its chunks",
	"[zarr_read_operation]" )
{
	sharded_read_fixture fixture;
	const auto tasks = fixture.take_tasks();
	auto operation = fixture.make_operation();

	// Six shards, of which the last is not stored, a task each.
	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 6 );
	fixture.finish_all(*operation);
	REQUIRE( fixture.m_tasks.empty() );

	// The first shard does not store the chunk at the origin, and the last
	// shard is not stored.
	std::vector<std::uint16_t> expected(60);
	for (std::size_t row = 0; row < 6; ++row)
	{
		for (std::size_t column = 0; column < 10; ++column)
		{
			const bool filled =
				(row < 2 && column < 2) || (row >= 4 && column >= 8);
			expected[row * 10 + column] = filled
				? 0
				: static_cast<std::uint16_t>(3 * (row * 10 + column) + 5);
		}
	}
	REQUIRE( get_values<std::uint16_t>(fixture.m_destination) == expected );
}
