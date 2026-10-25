// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_chunk_store.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_image_reader.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <strided_transfer/image_chunk_regions.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <file_transfer_pool_implementation.hpp>

#include "zarr_test_store.hpp"

#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

// An array of version 3 of uint16, of 4 by 4 in chunks of 2 by 2, or of 3 by
// 3 for the edge chunks, stored as bytes of the given order, with no codec
// so that its files can be read as they are, and the fill value 7.
class chunk_store_fixture
{
public:
	explicit chunk_store_fixture(
		std::size_t budget = 1UL << 20,
		const char *endian = "little",
		extents shape = {4, 4}
	)
		: m_directory("test_zarr_chunk_store_" + std::to_string(
			std::hash<std::string>()(
				Catch::getResultCapture().getCurrentTestName())) + ".zarr")
		, m_shape(std::move(shape))
	{
		write_store_file(m_directory.get(), "zarr.json",
			std::string(R"({"zarr_format": 3, "node_type": "array",
				"shape": [)") + std::to_string(m_shape[0]) + ", " +
				std::to_string(m_shape[1]) + R"(], "data_type": "uint16",
				"chunk_grid": {"name": "regular",
					"configuration": {"chunk_shape": [2, 2]}},
				"chunk_key_encoding": {"name": "default"},
				"fill_value": 7,
				"codecs": [{"name": "bytes",
					"configuration": {"endian": ")" + endian + R"("}}]})");
		m_layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(m_directory.get()));
		m_store = std::make_shared<zarr_chunk_store>(
			m_directory.get(), m_layout, budget);
	}

	// Writes regions of an array of the shape of the store, as a writer
	// does, a chunk at a time.
	void write(const image_transfer_plan &regions, const array &source)
	{
		const image_chunk_regions chunks(
			regions,
			make_span(m_shape),
			m_layout->get_chunk_layout().get_extents()
		);
		const auto shared = source.share_const();
		for (std::size_t position = 0; position < chunks.get_chunk_count();
			++position)
		{
			std::vector<const image_transfer_plan*> pieces;
			for (std::size_t plan = 0;
				plan < chunks.get_plan_count(position); ++plan)
			{
				pieces.push_back(&chunks.get_regions(position, plan));
			}
			m_store->write(chunks.get_chunk(position), pieces, shared);
		}
	}

	// Writes a value to one element.
	void write_one(std::size_t row, std::size_t column, std::uint16_t value)
	{
		image_transfer_plan regions(image_transfer_shape({1, 1}, 2, 2));
		regions.add(
			make_span(extents{row, column}), make_span(extents{0, 0}));
		write(regions, make_host_array<std::uint16_t>(
			{1, 1}, numerical_type::uint16, value));
	}

	std::string path_of(std::size_t row, std::size_t column) const
	{
		return join_zarr_path(m_directory.get(),
			"c/" + std::to_string(row) + "/" + std::to_string(column));
	}

	bool is_stored(std::size_t row, std::size_t column) const
	{
		return boost::filesystem::exists(path_of(row, column));
	}

	std::vector<std::uint8_t> file_of(std::size_t row, std::size_t column)
		const
	{
		std::ifstream input(
			path_of(row, column).c_str(), std::ios::in | std::ios::binary);
		return std::vector<std::uint8_t>(
			(std::istreambuf_iterator<char>(input)),
			std::istreambuf_iterator<char>());
	}

	// What a reader of the store reads of it.
	std::vector<std::uint16_t> read_back() const
	{
		const zarr_image_reader reader(
			m_directory.get(),
			m_layout,
			image_descriptor(make_span(m_shape), 2, numerical_type::uint16),
			image_metadata(),
			make_file_transfer_pool()->get_implementation().get_executor(),
			false
		);
		auto destination = make_host_array<std::uint16_t>(
			m_shape, numerical_type::uint16);
		read_now(reader, destination, whole_of(m_shape));
		return get_values<std::uint16_t>(destination);
	}

	scoped_directory m_directory;
	extents m_shape;
	std::shared_ptr<const zarr_array_layout> m_layout;
	std::shared_ptr<zarr_chunk_store> m_store;
};

std::vector<std::uint16_t> counting(std::size_t count, std::uint16_t from)
{
	std::vector<std::uint16_t> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<std::uint16_t>(from + p);
	}
	return values;
}

} // anonymous namespace

TEST_CASE( "zarr_chunk_store stores a chunk once all of it is written",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture;
	REQUIRE( fixture.m_store->get_directory() == fixture.m_directory.get() );

	fixture.write_one(0, 0, 100);
	fixture.write_one(0, 1, 101);
	fixture.write_one(1, 0, 102);
	REQUIRE_FALSE( fixture.is_stored(0, 0) );
	REQUIRE( fixture.m_store->get_held_chunks() ==
		std::vector<extents>({{0, 0}}) );

	fixture.write_one(1, 1, 103);
	REQUIRE( fixture.is_stored(0, 0) );
	REQUIRE( fixture.m_store->get_held_chunks().empty() );
	REQUIRE( fixture.file_of(0, 0) ==
		std::vector<std::uint8_t>({100, 0, 101, 0, 102, 0, 103, 0}) );

	const auto unsynced = fixture.m_store->take_unsynced();
	REQUIRE( unsynced == std::vector<std::string>({fixture.path_of(0, 0)}) );
	REQUIRE( fixture.m_store->take_unsynced().empty() );
}

TEST_CASE( "zarr_chunk_store counts only what of an edge chunk is in the array",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture(1UL << 20, "little", {3, 3});

	fixture.write_one(2, 2, 500);
	REQUIRE( fixture.is_stored(1, 1) );
	REQUIRE( fixture.file_of(1, 1) ==
		std::vector<std::uint8_t>({0xF4, 1, 7, 0, 7, 0, 7, 0}) );
}

TEST_CASE( "zarr_chunk_store stores a chunk written in part when told to",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture;
	fixture.write_one(0, 1, 9);

	// A chunk not held is passed over.
	fixture.m_store->store(make_span(extents{1, 1}));
	REQUIRE_FALSE( fixture.is_stored(1, 1) );

	fixture.m_store->store(make_span(extents{0, 0}));
	REQUIRE( fixture.is_stored(0, 0) );
	REQUIRE( fixture.m_store->get_held_chunks().empty() );
	REQUIRE( fixture.file_of(0, 0) ==
		std::vector<std::uint8_t>({7, 0, 9, 0, 7, 0, 7, 0}) );

	SECTION( "and reads it back when it is written again" )
	{
		fixture.write_one(1, 0, 11);
		fixture.m_store->store(make_span(extents{0, 0}));
		REQUIRE( fixture.file_of(0, 0) ==
			std::vector<std::uint8_t>({7, 0, 9, 0, 11, 0, 7, 0}) );
	}
}

TEST_CASE( "zarr_chunk_store stores no chunk of the fill value alone",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture;
	auto fill = make_host_array<std::uint16_t>(
		{2, 2}, numerical_type::uint16, std::uint16_t(7));
	image_transfer_plan chunk(image_transfer_shape({2, 2}, 2, 2));
	chunk.add(make_span(extents{2, 2}), make_span(extents{0, 0}));

	fixture.write(chunk, fill);
	REQUIRE_FALSE( fixture.is_stored(1, 1) );
	REQUIRE( fixture.m_store->get_held_chunks().empty() );

	SECTION( "and removes the file of one that holds it again" )
	{
		fixture.write(chunk, make_host_array<std::uint16_t>(
			{2, 2}, numerical_type::uint16, std::uint16_t(3)));
		REQUIRE( fixture.is_stored(1, 1) );

		fixture.write(chunk, fill);
		REQUIRE_FALSE( fixture.is_stored(1, 1) );
		REQUIRE( fixture.m_store->take_unsynced().empty() );
	}
}

TEST_CASE( "zarr_chunk_store stores what was written least recently past "
	"its budget", "[zarr_chunk_store]" )
{
	// Room for one chunk of 8 bytes.
	chunk_store_fixture fixture(8);

	fixture.write_one(0, 0, 1);
	REQUIRE_FALSE( fixture.is_stored(0, 0) );

	fixture.write_one(0, 2, 2);
	REQUIRE( fixture.is_stored(0, 0) );
	REQUIRE_FALSE( fixture.is_stored(0, 1) );
	REQUIRE( fixture.m_store->get_held_chunks() ==
		std::vector<extents>({{0, 1}}) );

	fixture.write_one(0, 1, 3);
	fixture.m_store->store(make_span(extents{0, 0}));
	fixture.m_store->store(make_span(extents{0, 1}));

	std::vector<std::uint16_t> expected(16, 7);
	expected[0] = 1;
	expected[1] = 3;
	expected[2] = 2;
	REQUIRE( fixture.read_back() == expected );
}

TEST_CASE( "zarr_chunk_store stores values big endian when the array does",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture(1UL << 20, "big");
	fixture.write_one(0, 0, 0x0102);
	fixture.m_store->store(make_span(extents{0, 0}));
	REQUIRE( fixture.file_of(0, 0) ==
		std::vector<std::uint8_t>({1, 2, 0, 7, 0, 7, 0, 7}) );

	// Read back in the order of the system before it is written again.
	fixture.write_one(1, 1, 0x0304);
	fixture.m_store->store(make_span(extents{0, 0}));
	REQUIRE( fixture.file_of(0, 0) ==
		std::vector<std::uint8_t>({1, 2, 0, 7, 0, 7, 3, 4}) );
}

TEST_CASE( "writes of zarr_chunk_store may be under way at once",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture(16, "little", {4, 4});
	const auto values = counting(16, 1000);
	const auto source = make_host_array<std::uint16_t>(
		{4, 4}, numerical_type::uint16, values);

	std::vector<std::thread> threads;
	for (std::size_t row = 0; row < 4; ++row)
	{
		threads.emplace_back([&fixture, &source, row]()
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				image_transfer_plan regions(
					image_transfer_shape({1, 1}, 2, 2));
				regions.add(
					make_span(extents{row, column}),
					make_span(extents{row, column}));
				fixture.write(regions, source);
			}
		});
	}
	for (auto &thread : threads)
	{
		thread.join();
	}
	for (const auto &chunk : fixture.m_store->get_held_chunks())
	{
		fixture.m_store->store(make_span(chunk));
	}

	REQUIRE( fixture.read_back() == values );
}

TEST_CASE( "zarr_chunk_store syncs what was written",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture;
	const auto metadata = join_zarr_path(
		fixture.m_directory.get(), "zarr.json");
	fixture.m_store->add_unsynced(metadata);
	REQUIRE( fixture.m_store->take_unsynced() ==
		std::vector<std::string>({metadata}) );

	REQUIRE_NOTHROW( zarr_chunk_store::sync(metadata) );
	REQUIRE_NOTHROW( zarr_chunk_store::sync(
		join_zarr_path(fixture.m_directory.get(), "gone")) );
}

TEST_CASE( "zarr_chunk_store reports a chunk it can not read back",
	"[zarr_chunk_store]" )
{
	chunk_store_fixture fixture;
	fixture.write_one(0, 0, 1);
	fixture.m_store->store(make_span(extents{0, 0}));
	write_store_file(fixture.m_directory.get(), "c/0/0", "short");

	REQUIRE_THROWS_WITH(
		fixture.write_one(0, 1, 2),
		Catch::Matchers::StartsWith(fixture.path_of(0, 0))
	);
}
