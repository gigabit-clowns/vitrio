// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_flush_operation.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/host_array.hpp>

#include "../../mock/mock_file_transfer_scope.hpp"
#include "zarr_test_store.hpp"

#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <boost/filesystem/operations.hpp>

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

class flush_operation_fixture
{
public:
	flush_operation_fixture()
		// Named after the case, so that cases run at once write apart.
		: m_directory("test_zarr_flush_operation_" + std::to_string(
			std::hash<std::string>()(
				Catch::getResultCapture().getCurrentTestName())) + ".zarr")
	{
		write_byte_array_metadata(m_directory.get());
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(m_directory.get()));
		m_store = std::make_shared<zarr_chunk_store>(
			m_directory.get(), layout, 1UL << 20);
	}

	// Writes one byte of a chunk, which holds it in memory.
	void write_one(std::size_t row, std::size_t column)
	{
		const auto source = make_host_array<std::uint8_t>(
			{1, 1}, numerical_type::uint8, std::uint8_t(1));
		image_transfer_plan piece(image_transfer_shape({1, 1}, 2, 2));
		piece.add(
			make_span(extents{row % 2, column % 2}),
			make_span(extents{0, 0}));
		m_store->write(
			make_span(extents{row / 2, column / 2}), {&piece},
			source.share_const());
	}

	std::string path_of(std::size_t row, std::size_t column) const
	{
		return join_zarr_path(m_directory.get(),
			"c/" + std::to_string(row) + "/" + std::to_string(column));
	}

	// Takes every task the operation runs, by its tag.
	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		return NAMED_ALLOW_CALL(m_scope, run(_, _))
			.LR_SIDE_EFFECT( m_tasks[_2] = std::move(_1) );
	}

	void finish(zarr_flush_operation &operation, std::uint64_t tag)
	{
		auto work = std::move(m_tasks.at(tag));
		m_tasks.erase(tag);
		work->run();
		operation.on_task(tag, nullptr, m_scope);
	}

	scoped_directory m_directory;
	std::shared_ptr<zarr_chunk_store> m_store;
	mock_file_transfer_scope m_scope;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
};

} // anonymous namespace

TEST_CASE( "zarr_flush_operation stores the chunks held, then syncs",
	"[zarr_flush_operation]" )
{
	flush_operation_fixture fixture;
	fixture.write_one(0, 0);
	fixture.write_one(3, 3);
	fixture.m_store->add_unsynced(
		join_zarr_path(fixture.m_directory.get(), "zarr.json"));
	const auto tasks = fixture.take_tasks();
	zarr_flush_operation operation(fixture.m_store);

	operation.start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 2 );
	REQUIRE( fixture.m_tasks.count(0) == 1 );
	REQUIRE( fixture.m_tasks.count(2) == 1 );

	// Nothing is synced until every chunk is stored.
	fixture.finish(operation, 0);
	REQUIRE( fixture.m_tasks.size() == 1 );
	REQUIRE( boost::filesystem::exists(fixture.path_of(0, 0)) );

	fixture.finish(operation, 2);
	REQUIRE( boost::filesystem::exists(fixture.path_of(1, 1)) );
	REQUIRE( fixture.m_store->get_held_chunks().empty() );

	// The metadata and both chunks, each in a task with an odd tag.
	REQUIRE( fixture.m_tasks.size() == 3 );
	for (const std::uint64_t tag : {1, 3, 5})
	{
		fixture.finish(operation, tag);
	}
	REQUIRE( fixture.m_tasks.empty() );
	REQUIRE( fixture.m_store->take_unsynced().empty() );
}

TEST_CASE( "zarr_flush_operation syncs at once when no chunk is held",
	"[zarr_flush_operation]" )
{
	flush_operation_fixture fixture;
	const auto tasks = fixture.take_tasks();

	SECTION( "a file written" )
	{
		fixture.m_store->add_unsynced(
			join_zarr_path(fixture.m_directory.get(), "zarr.json"));
		zarr_flush_operation operation(fixture.m_store);
		operation.start(fixture.m_scope);
		REQUIRE( fixture.m_tasks.size() == 1 );
		fixture.finish(operation, 1);
	}

	SECTION( "nothing written, and nothing to do" )
	{
		zarr_flush_operation operation(fixture.m_store);
		operation.start(fixture.m_scope);
		REQUIRE( fixture.m_tasks.empty() );
	}
}

TEST_CASE( "zarr_flush_operation reports a task that failed",
	"[zarr_flush_operation]" )
{
	flush_operation_fixture fixture;
	fixture.write_one(0, 0);
	const auto tasks = fixture.take_tasks();
	zarr_flush_operation operation(fixture.m_store);
	operation.start(fixture.m_scope);

	REQUIRE_THROWS_AS(
		operation.on_task(
			0,
			std::make_exception_ptr(std::runtime_error("failed")),
			fixture.m_scope
		),
		std::runtime_error
	);
}
