// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <formats/raw/queued/staged_image_read_operation.hpp>

#include "../../../mock/mock_file_transfer_scope.hpp"

#include <formats/raw/planning/image_transfer_policy.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_read_walk.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <memory/byte_order.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
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

// A stack of six images of two rows of three float32 values, which count up
// from zero, behind a preamble of 16 bytes.
const std::vector<std::size_t> stack_extents = {6, 2, 3};
const std::size_t image_size = 6 * sizeof(float);
const std::size_t preamble = 16;

std::vector<float> counting(std::size_t first, std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(first + i);
	}

	return values;
}

// The bytes of the file, which a case lands in the buffers of a read in
// place of the kernel.
std::vector<byte> write_file(const std::string &path)
{
	const auto values = counting(0, 36);
	std::vector<byte> bytes(preamble + values.size() * sizeof(float), 0);
	std::memcpy(
		bytes.data() + preamble,
		values.data(),
		values.size() * sizeof(float)
	);

	std::ofstream output(path, std::ios::out | std::ios::binary);
	output.write(
		reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	return bytes;
}

// The plan that reads each image of the stack into the slot of the array at
// its index, a region each, which a step may hold two of.
image_transfer_plan images_of_stack()
{
	image_transfer_plan regions(image_transfer_shape({2, 3}, 3, 3));
	for (std::size_t image = 0; image < stack_extents[0]; ++image)
	{
		regions.add(
			make_span(std::vector<std::size_t>{image, 0, 0}),
			make_span(std::vector<std::size_t>{image, 0, 0})
		);
	}

	return regions;
}

// Every case runs as a process of its own, and several may run at once, so
// the file of a case is named after it.
std::string name_file()
{
	const auto case_name = Catch::getResultCapture().getCurrentTestName();
	const auto tag = std::hash<std::string>()(case_name);

	return "staged_image_read_operation_" + std::to_string(tag) + ".raw";
}

// What was asked of one read.
struct queued_read
{
	std::uint64_t offset;
	memory_range buffer;
};

// Reads the whole stack as float64, two images a step: three steps of one
// stretch each, since the images follow one another in the file.
class staged_fixture
{
public:
	staged_fixture()
		: m_path(name_file())
		, m_bytes(write_file(m_path.get()))
		, m_destination(
			make_host_array<double>(stack_extents, numerical_type::float64)
		)
	{
		const raw_image_layout layout(
			image_descriptor(
				make_span(stack_extents),
				2,
				numerical_type::float32
			),
			std::vector<std::ptrdiff_t>{6, 3, 1},
			preamble,
			get_system_byte_order()
		);
		const auto regions = images_of_stack();
		const auto &target = m_destination.get_descriptor();
		image_region_read_walk walk(
			regions,
			layout.get_descriptor().get_extents(),
			layout.get_strides(),
			target.get_extents(),
			target.get_strides(),
			target.get_offset()
		);
		const image_transfer_schedule schedule(
			regions,
			layout.get_strides(),
			numerical_type::float32,
			walk.get_offsets().get_file(),
			preamble,
			m_bytes.size(),
			image_transfer_policy(0, 2 * image_size, sizeof(float), 0)
		);

		m_operation = std::make_unique<staged_image_read_operation>(
			m_path.get(),
			std::make_shared<const boost::interprocess::file_mapping>(
				m_path.get().c_str(),
				boost::interprocess::read_only
			),
			layout,
			m_destination.share(),
			std::move(walk),
			schedule
		);
	}

protected:
	// Lets the scope take every read, and keeps what was asked of each.
	std::unique_ptr<trompeloeil::expectation> take_reads()
	{
		return NAMED_ALLOW_CALL(
			m_scope,
			queue_read(
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_
			)
		).LR_SIDE_EFFECT( keep_read(_2, _3, _4) );
	}

	// Keeps what a read asks for. Catch's assertion is here rather than
	// inside the arguments of trompeloeil's macros, where GCC 11 refuses the
	// pragmas it expands to.
	void keep_read(
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	)
	{
		REQUIRE( buffers.size() == 1 );
		m_reads.emplace(tag, queued_read{offset, buffers[0]});
	}

	// Lands the bytes of the file in the buffer of a read, as the kernel
	// would, and hands the operation its completion.
	void land(std::uint64_t read)
	{
		const auto &queued = m_reads.at(read);
		std::memcpy(
			queued.buffer.get_address(),
			m_bytes.data() + queued.offset,
			queued.buffer.get_size()
		);
		m_operation->on_transfer(
			file_transfer_completion(read, queued.buffer.get_size()),
			m_scope
		);
	}

	scoped_path m_path;
	std::vector<byte> m_bytes;
	array m_destination;
	std::unique_ptr<staged_image_read_operation> m_operation;
	mock_file_transfer_scope m_scope;
	// By tag.
	std::map<std::uint64_t, queued_read> m_reads;
};

} // anonymous namespace

TEST_CASE_METHOD(
	staged_fixture,
	"a staged_image_read_operation reads two steps ahead as it starts",
	"[staged_image_read_operation]"
)
{
	const auto reads = take_reads();
	FORBID_CALL(m_scope, run(trompeloeil::_, trompeloeil::_));

	m_operation->start(m_scope);
	REQUIRE( m_reads.size() == 2 );
	CHECK( m_reads.at(0).offset == preamble );
	CHECK( m_reads.at(0).buffer.get_size() == 2 * image_size );
	CHECK( m_reads.at(1).offset == preamble + 2 * image_size );
}

TEST_CASE_METHOD(
	staged_fixture,
	"a staged_image_read_operation converts each stretch as it arrives, and "
	"reads the step after next once a step is converted",
	"[staged_image_read_operation]"
)
{
	const auto reads = take_reads();
	m_operation->start(m_scope);

	std::unique_ptr<task> conversion;
	{
		REQUIRE_CALL(m_scope, run(trompeloeil::_, 0u))
			.LR_SIDE_EFFECT( conversion = std::move(_1) );
		land(0);
	}

	// The conversion moves the first two images into the array.
	REQUIRE( conversion != nullptr );
	conversion->run();
	const auto values = get_values<double>(m_destination);
	const auto expected = counting(0, 12);
	CHECK(
		std::vector<double>(values.begin(), values.begin() + 12) ==
		std::vector<double>(expected.begin(), expected.end())
	);

	// The first step converted, the third is read into its half.
	CHECK( m_reads.size() == 2 );
	m_operation->on_task(0, nullptr, m_scope);
	REQUIRE( m_reads.size() == 3 );
	CHECK( m_reads.at(2).offset == preamble + 4 * image_size );
	CHECK(
		m_reads.at(2).buffer.get_address() ==
		m_reads.at(0).buffer.get_address()
	);
}

TEST_CASE_METHOD(
	staged_fixture,
	"a staged_image_read_operation fails with what a conversion throws",
	"[staged_image_read_operation]"
)
{
	const auto reads = take_reads();
	m_operation->start(m_scope);

	REQUIRE_CALL(m_scope, run(trompeloeil::_, 1u));
	land(1);

	CHECK_THROWS_AS(
		m_operation->on_task(
			1,
			std::make_exception_ptr(std::runtime_error("conversion")),
			m_scope
		),
		std::runtime_error
	);
}
