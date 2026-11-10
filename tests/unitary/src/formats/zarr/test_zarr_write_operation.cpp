// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_write_operation.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

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

class write_operation_fixture
{
public:
	explicit write_operation_fixture(std::size_t sync_budget = 1UL << 20)
		// Named after the case, so that cases run at once write apart.
		: m_directory("test_zarr_write_operation_" + std::to_string(
			std::hash<std::string>()(
				Catch::getResultCapture().getCurrentTestName())) + ".zarr")
	{
		write_byte_array_metadata(m_directory.get());
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(m_directory.get()));
		m_store = std::make_shared<zarr_chunk_store>(
			m_directory.get(), layout, 1UL << 20, sync_budget);
	}

	std::unique_ptr<zarr_write_operation> make_operation(
		const image_transfer_plan &regions
	)
	{
		std::vector<std::uint8_t> values(16);
		for (std::size_t p = 0; p < values.size(); ++p)
		{
			values[p] = static_cast<std::uint8_t>(p);
		}
		const auto source = make_host_array<std::uint8_t>(
			{4, 4}, numerical_type::uint8, values);
		const extents shape = {4, 4};
		const extents chunk = {2, 2};

		return std::make_unique<zarr_write_operation>(
			m_store,
			source.share_const(),
			image_chunk_regions(regions, make_span(shape), make_span(chunk))
		);
	}

	bool is_stored(std::size_t row, std::size_t column) const
	{
		return boost::filesystem::exists(join_zarr_path(m_directory.get(),
			"c/" + std::to_string(row) + "/" + std::to_string(column)));
	}

	scoped_directory m_directory;
	std::shared_ptr<zarr_chunk_store> m_store;
	mock_file_transfer_scope m_scope;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
};

} // anonymous namespace

TEST_CASE( "zarr_write_operation writes each chunk in a task of its own",
	"[zarr_write_operation]" )
{
	write_operation_fixture fixture;
	ALLOW_CALL(fixture.m_scope, run(_, _))
		.LR_SIDE_EFFECT( fixture.m_tasks[_2] = std::move(_1) );
	auto operation = fixture.make_operation(whole_of({4, 4}));

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 4 );
	REQUIRE_FALSE( fixture.is_stored(0, 0) );

	for (auto &entry : fixture.m_tasks)
	{
		entry.second->run();
		operation->on_task(entry.first, nullptr, fixture.m_scope);
	}
	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		REQUIRE( fixture.is_stored(chunk / 2, chunk % 2) );
	}
}

TEST_CASE( "zarr_write_operation syncs the chunks stored as they pass the "
	"budget", "[zarr_write_operation]" )
{
	// Chunks of 4 bytes, synced every 6: the second chunk passes it.
	write_operation_fixture fixture(6);
	ALLOW_CALL(fixture.m_scope, run(_, _))
		.LR_SIDE_EFFECT( fixture.m_tasks[_2] = std::move(_1) );
	auto operation = fixture.make_operation(whole_of({4, 4}));
	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 4 );

	auto finish = [&](std::uint64_t tag)
	{
		auto work = std::move(fixture.m_tasks.at(tag));
		fixture.m_tasks.erase(tag);
		work->run();
		operation->on_task(tag, nullptr, fixture.m_scope);
	};
	finish(0);
	REQUIRE( fixture.m_tasks.size() == 3 );

	// Two chunks stored: a sync of each, tagged past the chunks.
	finish(1);
	REQUIRE( fixture.m_tasks.size() == 4 );
	REQUIRE( fixture.m_tasks.count(4) == 1 );
	REQUIRE( fixture.m_tasks.count(5) == 1 );
	finish(4);
	finish(5);
	REQUIRE( fixture.m_tasks.size() == 2 );

	// Counted from the batch on: the third chunk alone does not pass it.
	finish(2);
	REQUIRE( fixture.m_tasks.size() == 1 );
	finish(3);
	REQUIRE( fixture.m_tasks.size() == 2 );
	REQUIRE( fixture.m_tasks.count(6) == 1 );
	REQUIRE( fixture.m_tasks.count(7) == 1 );
}

TEST_CASE( "zarr_write_operation writes only the chunks the regions reach",
	"[zarr_write_operation]" )
{
	write_operation_fixture fixture;
	ALLOW_CALL(fixture.m_scope, run(_, _))
		.LR_SIDE_EFFECT( fixture.m_tasks[_2] = std::move(_1) );

	image_transfer_plan regions(image_transfer_shape({2, 2}, 2, 2));
	regions.add(make_span(extents{2, 0}), make_span(extents{0, 0}));
	auto operation = fixture.make_operation(regions);

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 1 );
	fixture.m_tasks.begin()->second->run();
	REQUIRE( fixture.is_stored(1, 0) );
	REQUIRE_FALSE( fixture.is_stored(0, 0) );
}

TEST_CASE( "zarr_write_operation reports a task that failed",
	"[zarr_write_operation]" )
{
	write_operation_fixture fixture;
	ALLOW_CALL(fixture.m_scope, run(_, _));
	auto operation = fixture.make_operation(whole_of({4, 4}));
	operation->start(fixture.m_scope);

	REQUIRE_THROWS_AS(
		operation->on_task(
			0,
			std::make_exception_ptr(std::runtime_error("failed")),
			fixture.m_scope
		),
		std::runtime_error
	);
}
