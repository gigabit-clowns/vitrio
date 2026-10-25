// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <formats/raw/queued/staged_image_write_operation.hpp>

#include "../../../mock/mock_file_transfer_scope.hpp"

#include <formats/raw/create_image_file.hpp>
#include <formats/raw/planning/image_region_runs.hpp>
#include <formats/raw/planning/image_transfer_policy.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_write_walk.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

const std::size_t preamble = 16;

std::vector<double> counting(std::size_t first, std::size_t count)
{
	std::vector<double> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<double>(first + i);
	}

	return values;
}

// Every case runs as a process of its own, and several may run at once, so
// the file of a case is named after it.
std::string name_file()
{
	const auto case_name = Catch::getResultCapture().getCurrentTestName();
	const auto tag = std::hash<std::string>()(case_name);

	return "staged_image_write_operation_" + std::to_string(tag) + ".raw";
}

// The float32 values of a stretch of a file, as a buffer of a write holds
// them.
std::vector<float> floats_in(const const_memory_range &buffer)
{
	std::vector<float> values(buffer.get_size() / sizeof(float));
	std::memcpy(values.data(), buffer.get_address(), buffer.get_size());
	return values;
}

std::vector<float> as_floats(const std::vector<double> &values)
{
	return std::vector<float>(values.begin(), values.end());
}

// What was asked of one write.
struct queued_write
{
	std::uint64_t offset;
	const_memory_range buffer;
};

// Writes regions of an array of float64 into a file of float32, through a
// mocked scope.
class staged_fixture
{
public:
	explicit staged_fixture(
		std::vector<std::size_t> file_extents,
		std::size_t core_rank
	)
		: m_path(name_file())
		, m_file_extents(std::move(file_extents))
		, m_layout(
			image_descriptor(
				make_span(m_file_extents),
				core_rank,
				numerical_type::float32
			),
			contiguous_strides(m_file_extents),
			preamble,
			get_system_byte_order()
		)
	{
		create_image_file(
			m_path.get(),
			preamble + m_layout.get_data_size()
		);
		m_file = std::make_shared<writable_image_file>(m_path.get());
	}

	// Plans the writes of regions of an array, a step at a time of a
	// budget.
	void plan(
		const array &source,
		const image_transfer_plan &regions,
		std::size_t byte_budget
	)
	{
		const auto &origin = source.get_descriptor();
		image_region_write_walk walk(
			regions,
			m_layout.get_descriptor().get_extents(),
			m_layout.get_strides(),
			origin.get_extents(),
			origin.get_strides(),
			origin.get_offset()
		);
		const image_transfer_schedule schedule(
			regions,
			m_layout.get_strides(),
			numerical_type::float32,
			walk.get_offsets().get_file(),
			preamble,
			preamble + m_layout.get_data_size(),
			image_transfer_policy(0, byte_budget, sizeof(float), 0)
		);
		const image_region_runs runs(
			regions,
			m_layout.get_strides(),
			numerical_type::float32
		);

		m_operation = std::make_unique<staged_image_write_operation>(
			m_file,
			m_layout,
			source.share_const(),
			std::move(walk),
			schedule,
			runs
		);
	}

protected:
	static std::vector<std::ptrdiff_t>
	contiguous_strides(const std::vector<std::size_t> &extents)
	{
		std::vector<std::ptrdiff_t> strides(extents.size());
		std::ptrdiff_t stride = 1;
		for (auto axis = extents.size(); axis > 0; --axis)
		{
			strides[axis - 1] = stride;
			stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
		}
		return strides;
	}

	// Lets the scope take every conversion, and keeps each by its tag.
	std::unique_ptr<trompeloeil::expectation> take_conversions()
	{
		return NAMED_ALLOW_CALL(
			m_scope,
			run(trompeloeil::_, trompeloeil::_)
		).LR_SIDE_EFFECT( m_conversions[_2] = std::move(_1) );
	}

	// Lets the scope take every write, and keeps what was asked of each.
	std::unique_ptr<trompeloeil::expectation> take_writes()
	{
		return NAMED_ALLOW_CALL(
			m_scope,
			queue_write(
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_
			)
		).LR_SIDE_EFFECT(
			REQUIRE( _3.size() == 1 );
			m_writes.emplace(_4, queued_write{_2, _3[0]})
		);
	}

	// Runs a conversion and hands the operation its outcome.
	void convert(std::uint64_t range)
	{
		m_conversions.at(range)->run();
		m_operation->on_task(range, nullptr, m_scope);
	}

	void complete(std::uint64_t write)
	{
		m_operation->on_transfer(
			file_transfer_completion(
				write,
				m_writes.at(write).buffer.get_size()
			),
			m_scope
		);
	}

	scoped_path m_path;
	std::vector<std::size_t> m_file_extents;
	raw_image_layout m_layout;
	std::shared_ptr<writable_image_file> m_file;
	std::unique_ptr<staged_image_write_operation> m_operation;
	mock_file_transfer_scope m_scope;
	// By tag.
	std::map<std::uint64_t, std::unique_ptr<task>> m_conversions;
	std::map<std::uint64_t, queued_write> m_writes;
};

// A stack of six images of two rows of three values, written whole from an
// array that counts up from zero, two images a step: three steps of one
// stretch each, since the images follow one another in the file.
class stack_fixture
	: public staged_fixture
{
public:
	stack_fixture()
		: staged_fixture({6, 2, 3}, 2)
		, m_source(
			make_host_array<double>(
				{6, 2, 3},
				numerical_type::float64,
				counting(0, 36)
			)
		)
	{
		image_transfer_plan regions(image_transfer_shape({2, 3}, 3, 3));
		for (std::size_t image = 0; image < 6; ++image)
		{
			regions.add(
				make_span(std::vector<std::size_t>{image, 0, 0}),
				make_span(std::vector<std::size_t>{image, 0, 0})
			);
		}

		plan(m_source, regions, 2 * image_size);
	}

protected:
	static constexpr std::size_t image_size = 6 * sizeof(float);

	array m_source;
};

constexpr std::size_t stack_fixture::image_size;

// An image of four rows of four values, which a case writes regions of.
class patch_fixture
	: public staged_fixture
{
public:
	patch_fixture()
		: staged_fixture({4, 4}, 2)
	{
	}
};

} // anonymous namespace

TEST_CASE_METHOD(
	stack_fixture,
	"a staged_image_write_operation converts two steps ahead as it starts, "
	"and writes nothing yet",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	FORBID_CALL(
		m_scope,
		queue_write(
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_
		)
	);

	m_operation->start(m_scope);
	CHECK( m_conversions.size() == 2 );
	CHECK( m_conversions.count(0) == 1 );
	CHECK( m_conversions.count(1) == 1 );
}

TEST_CASE_METHOD(
	stack_fixture,
	"a staged_image_write_operation writes a stretch once it is converted, "
	"and converts the step after next once a step is written",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	const auto writes = take_writes();
	m_operation->start(m_scope);

	// The first step converted, its stretch is written from its half.
	convert(0);
	REQUIRE( m_writes.size() == 1 );
	CHECK( m_writes.at(0).offset == preamble );
	CHECK( m_writes.at(0).buffer.get_size() == 2 * image_size );
	CHECK( floats_in(m_writes.at(0).buffer) == as_floats(counting(0, 12)) );

	// Its write done, the third step is converted into its half.
	CHECK( m_conversions.size() == 2 );
	complete(0);
	REQUIRE( m_conversions.size() == 3 );
	convert(2);
	REQUIRE( m_writes.size() == 2 );
	CHECK( m_writes.at(2).offset == preamble + 4 * image_size );
	CHECK(
		m_writes.at(2).buffer.get_address() ==
		m_writes.at(0).buffer.get_address()
	);
	CHECK( floats_in(m_writes.at(2).buffer) == as_floats(counting(24, 12)) );
}

TEST_CASE_METHOD(
	stack_fixture,
	"a staged_image_write_operation keeps a step it has converted until its "
	"writes are done",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	const auto writes = take_writes();
	m_operation->start(m_scope);

	// Both steps converted, neither written: the third waits for a half.
	convert(1);
	convert(0);
	CHECK( m_writes.size() == 2 );
	CHECK( m_conversions.size() == 2 );

	// The second is written first, but the first still holds its half.
	complete(1);
	CHECK( m_conversions.size() == 2 );
	complete(0);
	CHECK( m_conversions.size() == 3 );
}

TEST_CASE_METHOD(
	stack_fixture,
	"a staged_image_write_operation fails with what a conversion throws",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	m_operation->start(m_scope);

	CHECK_THROWS_AS(
		m_operation->on_task(
			1,
			std::make_exception_ptr(std::runtime_error("conversion")),
			m_scope
		),
		std::runtime_error
	);
}

TEST_CASE_METHOD(
	stack_fixture,
	"a staged_image_write_operation fails with a write the system fails",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	const auto writes = take_writes();
	m_operation->start(m_scope);
	convert(0);

	CHECK_THROWS_AS(
		m_operation->on_transfer(
			file_transfer_completion(
				0,
				std::make_error_code(std::errc::no_space_on_device)
			),
			m_scope
		),
		image_file_error
	);
}

TEST_CASE_METHOD(
	patch_fixture,
	"a staged_image_write_operation writes the runs its regions cover, and "
	"nothing between them",
	"[staged_image_write_operation]"
)
{
	const auto conversions = take_conversions();
	const auto writes = take_writes();

	SECTION( "a patch is a write for each of its rows" )
	{
		// Two rows of two values, from the second row and column on.
		const auto source = make_host_array<double>(
			{2, 2},
			numerical_type::float64,
			std::vector<double>{1, 2, 3, 4}
		);
		image_transfer_plan regions(image_transfer_shape({2, 2}, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 1}),
			make_span(std::vector<std::size_t>{0, 0})
		);
		plan(source, regions, 1 << 20);
		m_operation->start(m_scope);
		convert(0);

		REQUIRE( m_writes.size() == 2 );
		CHECK( m_writes.at(0).offset == preamble + 5 * sizeof(float) );
		CHECK(
			floats_in(m_writes.at(0).buffer) == std::vector<float>{1, 2}
		);
		CHECK( m_writes.at(1).offset == preamble + 9 * sizeof(float) );
		CHECK(
			floats_in(m_writes.at(1).buffer) == std::vector<float>{3, 4}
		);
	}

	SECTION(
		"patches side by side that make up whole rows are written as the "
		"one stretch those rows are"
	)
	{
		// Two patches of two rows of two values, from the second row on, one
		// at the first column and one at the third.
		const auto source = make_host_array<double>(
			{2, 2, 2},
			numerical_type::float64,
			std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8}
		);
		image_transfer_plan regions(image_transfer_shape({2, 2}, 2, 3));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);
		regions.add(
			make_span(std::vector<std::size_t>{1, 2}),
			make_span(std::vector<std::size_t>{1, 0, 0})
		);
		plan(source, regions, 1 << 20);
		m_operation->start(m_scope);
		convert(0);

		REQUIRE( m_writes.size() == 1 );
		CHECK( m_writes.at(0).offset == preamble + 4 * sizeof(float) );
		CHECK(
			floats_in(m_writes.at(0).buffer) ==
			std::vector<float>{1, 2, 5, 6, 3, 4, 7, 8}
		);
	}

	SECTION(
		"patches side by side that do not make up whole rows are written as "
		"the pieces of rows they make up"
	)
	{
		// Two patches of two rows of one value, from the second row on, one
		// at the first column and one at the second.
		const auto source = make_host_array<double>(
			{2, 2, 1},
			numerical_type::float64,
			std::vector<double>{1, 2, 3, 4}
		);
		image_transfer_plan regions(image_transfer_shape({2, 1}, 2, 3));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);
		regions.add(
			make_span(std::vector<std::size_t>{1, 1}),
			make_span(std::vector<std::size_t>{1, 0, 0})
		);
		plan(source, regions, 1 << 20);
		m_operation->start(m_scope);
		convert(0);

		REQUIRE( m_writes.size() == 2 );
		CHECK( m_writes.at(0).offset == preamble + 4 * sizeof(float) );
		CHECK(
			floats_in(m_writes.at(0).buffer) == std::vector<float>{1, 3}
		);
		CHECK( m_writes.at(1).offset == preamble + 8 * sizeof(float) );
		CHECK(
			floats_in(m_writes.at(1).buffer) == std::vector<float>{2, 4}
		);
	}
}
