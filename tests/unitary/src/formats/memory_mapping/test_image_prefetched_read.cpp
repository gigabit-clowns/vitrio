// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/memory_mapping/image_prefetched_read.hpp>

#include <vitrio/tests/host_array.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>
#include <system/page_size.hpp>

#include "../../fixtures/aligned_memory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

const float untouched = -1.0F;

// Four images of 2 by 2 whose values count up from zero, so image n holds
// 4n, 4n + 1, 4n + 2 and 4n + 3. They lie in a page of their own, as in a
// mapping, after `preamble` bytes of something else.
class stack_memory
{
public:
	explicit stack_memory(std::size_t preamble = 0)
		: m_size(get_page_size())
		, m_block(m_size, m_size)
		, m_values(
			{4, 2, 2},
			{4, 2, 1},
			static_cast<std::ptrdiff_t>(preamble / sizeof(float)),
			numerical_type::float32
		)
	{
		std::fill(m_block.get(), m_block.get() + m_size, vitrio::byte(0xFF));
		for (std::size_t element = 0; element < 16; ++element)
		{
			const auto value = static_cast<float>(element);
			std::memcpy(
				m_block.get() + preamble + element * sizeof(float),
				&value,
				sizeof(value)
			);
		}
	}

	span<const vitrio::byte> get_memory() const noexcept
	{
		return make_span(m_block.get(), m_size);
	}

	const array_descriptor& get_values() const noexcept
	{
		return m_values;
	}

	// Turns every value around, as a file stated in the other byte order
	// would hold it.
	void swap_bytes(std::size_t preamble)
	{
		for (std::size_t element = 0; element < 16; ++element)
		{
			auto *first = m_block.get() + preamble + element * sizeof(float);
			std::reverse(first, first + sizeof(float));
		}
	}

private:
	std::size_t m_size;
	aligned_memory m_block;
	array_descriptor m_values;
};

byte_order get_other_byte_order() noexcept
{
	return get_system_byte_order() == byte_order::little_endian
		? byte_order::big_endian
		: byte_order::little_endian;
}

// Image `indices[i]` of the stack, landing in slot `i` of a batch.
image_transfer_plan images(const std::vector<std::size_t> &indices)
{
	image_transfer_plan plan(image_transfer_shape({2, 2}, 3, 3));
	for (std::size_t slot = 0; slot < indices.size(); ++slot)
	{
		const std::array<std::size_t, 3> file_offset = {indices[slot], 0, 0};
		const std::array<std::size_t, 3> array_offset = {slot, 0, 0};
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

array make_batch(std::size_t count)
{
	return make_host_array<float>(
		{count, 2, 2},
		numerical_type::float32,
		untouched
	);
}

// What the reads ask for, one step to an element.
class advice_record
{
public:
	void operator()(span<const memory_range> ranges)
	{
		m_steps.emplace_back(ranges.begin(), ranges.end());
	}

	const std::vector<std::vector<memory_range>>&
	get_steps() const noexcept
	{
		return m_steps;
	}

	// Whether some stretch asked for holds every byte from `first` to
	// `first + size`.
	bool covers(const vitrio::byte *first, std::size_t size) const noexcept
	{
		for (const auto &step : m_steps)
		{
			for (const auto &range : step)
			{
				const auto *begin = begin_of(range);
				if (first >= begin && first + size <= begin + range.get_size())
				{
					return true;
				}
			}
		}

		return false;
	}

	// Whether every stretch asked for lies within `memory`.
	bool lies_within(span<const vitrio::byte> memory) const noexcept
	{
		for (const auto &step : m_steps)
		{
			for (const auto &range : step)
			{
				const auto *begin = begin_of(range);
				if (
					begin < memory.data() ||
					begin + range.get_size() > memory.data() + memory.size()
				)
				{
					return false;
				}
			}
		}

		return true;
	}

private:
	static const vitrio::byte* begin_of(const memory_range &range) noexcept
	{
		return static_cast<const vitrio::byte*>(range.get_address());
	}

	std::vector<std::vector<memory_range>> m_steps;
};

} // anonymous namespace

TEST_CASE(
	"read_prefetched_regions reads each region to where the plan places it",
	"[image_prefetched_read]"
)
{
	const stack_memory stack;
	auto destination = make_batch(3);
	advice_record advice;

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		images({3, 1}),
		advice
	);

	// The third slot is named by no region and is left as it was.
	CHECK( get_values<float>(destination) == std::vector<float>({
		12, 13, 14, 15,
		4, 5, 6, 7,
		untouched, untouched, untouched, untouched
	}) );
}

TEST_CASE(
	"read_prefetched_regions reads values that begin inside the memory",
	"[image_prefetched_read]"
)
{
	const std::size_t preamble = 40;
	const stack_memory stack(preamble);
	auto destination = make_batch(1);
	advice_record advice;

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		images({2}),
		advice
	);

	CHECK( get_values<float>(destination) ==
		std::vector<float>({8, 9, 10, 11}) );
}

TEST_CASE(
	"read_prefetched_regions converts to the data type of the destination",
	"[image_prefetched_read]"
)
{
	const stack_memory stack;
	auto destination =
		make_host_array<double>({1, 2, 2}, numerical_type::float64, -1.0);
	advice_record advice;

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		images({2}),
		advice
	);

	CHECK( get_values<double>(destination) ==
		std::vector<double>({8, 9, 10, 11}) );
}

TEST_CASE(
	"read_prefetched_regions reads values stated in the other byte order",
	"[image_prefetched_read]"
)
{
	stack_memory stack;
	stack.swap_bytes(0);
	auto destination = make_batch(1);
	advice_record advice;

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_other_byte_order(),
		array_ref(destination),
		images({1}),
		advice
	);

	CHECK( get_values<float>(destination) ==
		std::vector<float>({4, 5, 6, 7}) );
}

TEST_CASE(
	"read_prefetched_regions reads a region smaller than an image",
	"[image_prefetched_read]"
)
{
	const stack_memory stack;
	auto destination =
		make_host_array<float>({1, 2}, numerical_type::float32, untouched);
	advice_record advice;

	// The second row of image 2, into an array of one row.
	image_transfer_plan row(image_transfer_shape({1, 2}, 3, 2));
	const std::array<std::size_t, 3> file_offset = {2, 1, 0};
	const std::array<std::size_t, 2> array_offset = {0, 0};
	row.add(make_span(file_offset), make_span(array_offset));

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		row,
		advice
	);

	CHECK( get_values<float>(destination) == std::vector<float>({10, 11}) );
}

TEST_CASE(
	"read_prefetched_regions asks for what it reads before reading it",
	"[image_prefetched_read]"
)
{
	const std::size_t preamble = 40;
	const stack_memory stack(preamble);
	auto destination = make_batch(2);
	const auto *batch = reinterpret_cast<const float*>(destination.get_data());

	advice_record advice;
	bool asked_before_reading = false;
	const auto prefetch =
		[&](span<const memory_range> ranges)
		{
			if (advice.get_steps().empty())
			{
				asked_before_reading = std::all_of(
					batch,
					batch + 8,
					[](float value) { return value == untouched; }
				);
			}

			advice(ranges);
		};

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		images({3, 0}),
		prefetch
	);

	// Two small regions are one step, asked for whole before either is
	// read.
	REQUIRE( advice.get_steps().size() == 1 );
	CHECK( asked_before_reading );

	const auto image_bytes = 4 * sizeof(float);
	const auto *values = stack.get_memory().data() + preamble;
	CHECK( advice.covers(values, image_bytes) );
	CHECK( advice.covers(values + 3 * image_bytes, image_bytes) );
	CHECK( advice.lies_within(stack.get_memory()) );
}

TEST_CASE(
	"read_prefetched_regions asks for nothing outside the memory",
	"[image_prefetched_read]"
)
{
	// The memory begins 8 bytes into a page, so the page the first image
	// starts in begins before it.
	const std::size_t lead = 8;
	const stack_memory stack(lead);
	const auto block = stack.get_memory();
	const auto memory = make_span(block.data() + lead, block.size() - lead);
	const array_descriptor values(
		{4, 2, 2},
		{4, 2, 1},
		0,
		numerical_type::float32
	);
	auto destination = make_batch(1);
	advice_record advice;

	read_prefetched_regions(
		memory,
		values,
		get_system_byte_order(),
		array_ref(destination),
		images({0}),
		advice
	);

	CHECK( get_values<float>(destination) ==
		std::vector<float>({0, 1, 2, 3}) );
	REQUIRE_FALSE( advice.get_steps().empty() );
	CHECK( advice.lies_within(memory) );
	CHECK( advice.covers(memory.data(), 4 * sizeof(float)) );
}

TEST_CASE(
	"read_prefetched_regions of an empty plan reads and asks for nothing",
	"[image_prefetched_read]"
)
{
	const stack_memory stack;
	auto destination = make_batch(1);
	advice_record advice;

	read_prefetched_regions(
		stack.get_memory(),
		stack.get_values(),
		get_system_byte_order(),
		array_ref(destination),
		images({}),
		advice
	);

	CHECK( advice.get_steps().empty() );
	CHECK( get_values<float>(destination) ==
		std::vector<float>(4, untouched) );
}

TEST_CASE(
	"read_prefetched_regions refuses what it can not read, and asks for "
	"nothing then",
	"[image_prefetched_read]"
)
{
	const stack_memory stack;
	auto destination = make_batch(1);
	advice_record advice;

	const auto read = [&](array_ref target, const image_transfer_plan &plan)
	{
		read_prefetched_regions(
			stack.get_memory(),
			stack.get_values(),
			get_system_byte_order(),
			target,
			plan,
			advice
		);
	};

	SECTION( "a destination that is not initialized" )
	{
		array empty;

		REQUIRE_THROWS_AS(
			read(array_ref(empty), images({0})),
			std::invalid_argument
		);
	}

	SECTION( "a region the values do not contain" )
	{
		REQUIRE_THROWS_AS(
			read(array_ref(destination), images({4})),
			std::out_of_range
		);
	}

	SECTION( "a region that does not fit in the destination" )
	{
		REQUIRE_THROWS_AS(
			read(array_ref(destination), images({0, 1})),
			std::out_of_range
		);
	}

	CHECK( advice.get_steps().empty() );
}
