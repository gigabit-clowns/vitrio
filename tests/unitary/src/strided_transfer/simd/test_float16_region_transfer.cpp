// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <strided_transfer/simd/float16_region_transfer.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <array/float16_t.hpp>
#include <memory/reinterpret_bits.hpp>

#include <hwy/targets.h>

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace vitrio;

namespace
{

constexpr std::uint32_t pattern_count = 0x10000U;

bool is_nan(std::uint16_t bits)
{
	return (bits & 0x7C00U) == 0x7C00U && (bits & 0x3FFU) != 0;
}

bool is_nan(float value)
{
	return std::isnan(value);
}

bool same_sign(std::uint16_t bits, float value)
{
	return ((bits >> 15) != 0) == std::signbit(value);
}

// What the scalar conversions of float16_t say, which the vector loops are
// held to: bit for bit, except that a NaN is any NaN of the same sign.
bool widens_as_scalar(std::uint16_t bits, float widened)
{
	const auto expected = static_cast<float>(float16_t::from_bits(bits));
	if (is_nan(bits))
	{
		return is_nan(widened) && same_sign(bits, widened);
	}
	return reinterpret_bits<std::uint32_t>(widened) ==
		reinterpret_bits<std::uint32_t>(expected);
}

bool narrows_as_scalar(float value, std::uint16_t narrowed)
{
	const auto expected = float16_t(value).get_bits();
	if (is_nan(value))
	{
		return is_nan(narrowed) && same_sign(narrowed, value);
	}
	return narrowed == expected;
}

std::string describe(std::uint32_t bits, int width)
{
	std::ostringstream out;
	out << "0x" << std::hex << std::uppercase << std::setfill('0')
		<< std::setw(width) << bits;
	return out.str();
}

// Runs a check once per instruction set the library was compiled for, with
// Highway told that it is the one the processor has, and tells Highway to
// look at the processor again afterwards, whatever the check did.
class forced_targets
{
public:
	forced_targets() = default;

	~forced_targets()
	{
		hwy::SetSupportedTargetsForTest(0);
	}

	forced_targets(const forced_targets&) = delete;
	forced_targets& operator=(const forced_targets&) = delete;
};

template <typename Check>
void for_each_target(Check &&check)
{
	const forced_targets guard;
	const auto targets = hwy::SupportedAndGeneratedTargets();
	REQUIRE_FALSE( targets.empty() );
	for (const auto target : targets)
	{
		hwy::SetSupportedTargetsForTest(target);
		INFO( "On " << hwy::TargetName(target) );
		check();
	}
}

image_region_layout make_layout(
	const std::vector<std::size_t> &extents,
	const std::vector<std::ptrdiff_t> &destination_strides,
	const std::vector<std::ptrdiff_t> &source_strides
)
{
	const auto rank = extents.size();
	image_transfer_plan regions(image_transfer_shape(extents, rank, rank));

	return image_region_layout(
		regions,
		make_span(destination_strides),
		make_span(source_strides)
	);
}

image_region_layout make_run_layout(std::size_t count)
{
	return make_layout({count}, {1}, {1});
}

const std::ptrdiff_t single_region[] = {0};

// A run of count elements starting at the two pointers.
void widen_run(float *destination, const float16_t *source, std::size_t count)
{
	widen_float16_regions(
		make_run_layout(count),
		make_span(single_region, 1),
		make_span(single_region, 1),
		destination,
		0,
		source,
		0
	);
}

void narrow_run(float16_t *destination, const float *source, std::size_t count)
{
	narrow_float16_regions(
		make_run_layout(count),
		make_span(single_region, 1),
		make_span(single_region, 1),
		destination,
		0,
		source,
		0
	);
}

std::vector<float16_t> every_pattern()
{
	std::vector<float16_t> patterns(pattern_count);
	for (std::uint32_t bits = 0; bits < pattern_count; ++bits)
	{
		patterns[bits] = float16_t::from_bits(static_cast<std::uint16_t>(bits));
	}
	return patterns;
}

// Floats that put the narrowing to the test: every half precision number,
// the midpoint between each pair of neighbours and the floats either side
// of it, the edge of the range, the subnormals of a float and a few NaNs,
// and then a pseudo-random sweep over the patterns of a float.
std::vector<float> narrowing_inputs()
{
	std::vector<float> inputs;

	for (std::uint32_t bits = 0; bits < pattern_count; ++bits)
	{
		if (is_nan(static_cast<std::uint16_t>(bits)))
		{
			continue;
		}
		const auto value =
			static_cast<float>(float16_t::from_bits(
				static_cast<std::uint16_t>(bits)
			));
		inputs.push_back(value);

		// The next number of the same sign, unless this is the largest.
		const auto magnitude = bits & 0x7FFFU;
		if (magnitude < 0x7BFFU)
		{
			const auto next = static_cast<float>(float16_t::from_bits(
				static_cast<std::uint16_t>(bits + 1)
			));
			const auto midpoint = value + (next - value) / 2;
			inputs.push_back(midpoint);
			inputs.push_back(std::nextafter(midpoint, value));
			inputs.push_back(std::nextafter(midpoint, next));
		}
	}

	for (const auto value : {
		65504.0F, 65519.0F, 65520.0F, 65536.0F, FLT_MAX,
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::denorm_min(),
		1e-40F, 1e-30F, 6.1e-5F, 6.2e-5F
	})
	{
		inputs.push_back(value);
		inputs.push_back(-value);
	}

	for (const auto bits : {
		0x7FC00000U, 0xFFC00000U, 0x7F800001U, 0x7FC55555U, 0xFF800001U
	})
	{
		inputs.push_back(reinterpret_bits<float>(bits));
	}

	std::uint32_t state = 0x9E3779B9U;
	for (std::size_t i = 0; i < 1000000; ++i)
	{
		state = state * 1664525U + 1013904223U;
		inputs.push_back(reinterpret_bits<float>(state));
	}

	return inputs;
}

// Every length up to the widest vector and one past it, so that each lane
// count of a tail is run on every instruction set, then a few long ones.
const std::size_t run_lengths[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 37, 63, 64, 65,
	100, 1001
};

constexpr float untouched_float = -12345.0F;
const float16_t untouched_half = float16_t::from_bits(0xBEEFU);

} // anonymous namespace

TEST_CASE(
	"widen_float16_regions widens every pattern as float16_t does",
	"[float16_region_transfer][simd]"
)
{
	const auto patterns = every_pattern();
	std::vector<float> widened(pattern_count + 1);

	for_each_target([&]
	{
		std::fill(widened.begin(), widened.end(), untouched_float);
		widen_run(widened.data(), patterns.data(), pattern_count);

		std::string failure;
		for (std::uint32_t bits = 0; bits < pattern_count; ++bits)
		{
			if (!widens_as_scalar(
				static_cast<std::uint16_t>(bits),
				widened[bits]
			))
			{
				failure = describe(bits, 4) + " widened to " + describe(
					reinterpret_bits<std::uint32_t>(widened[bits]),
					8
				);
				break;
			}
		}
		REQUIRE( failure == "" );
		REQUIRE( widened.back() == untouched_float );
	});
}

TEST_CASE(
	"narrow_float16_regions narrows every float as float16_t does",
	"[float16_region_transfer][simd]"
)
{
	const auto inputs = narrowing_inputs();
	std::vector<float16_t> narrowed(inputs.size() + 1);

	for_each_target([&]
	{
		std::fill(narrowed.begin(), narrowed.end(), untouched_half);
		narrow_run(narrowed.data(), inputs.data(), inputs.size());

		std::string failure;
		for (std::size_t i = 0; i < inputs.size(); ++i)
		{
			if (!narrows_as_scalar(inputs[i], narrowed[i].get_bits()))
			{
				failure = describe(
					reinterpret_bits<std::uint32_t>(inputs[i]),
					8
				) + " narrowed to " + describe(narrowed[i].get_bits(), 4);
				break;
			}
		}
		REQUIRE( failure == "" );
		REQUIRE( narrowed.back().get_bits() == untouched_half.get_bits() );
	});
}

TEST_CASE(
	"the float16 region loops convert a run of any length and nothing past it",
	"[float16_region_transfer][simd]"
)
{
	const auto patterns = every_pattern();
	std::vector<float> floats(patterns.size());
	for (std::size_t i = 0; i < patterns.size(); ++i)
	{
		floats[i] = is_nan(patterns[i].get_bits())
			? 1.5F
			: static_cast<float>(patterns[i]);
	}

	for_each_target([&]
	{
		for (const auto count : run_lengths)
		{
			INFO( "A run of " << count );

			// Start inside the patterns, so that the run is not the same
			// prefix every time.
			const std::size_t start = 0x3C00U;

			std::vector<float> widened(count + 1, untouched_float);
			widen_run(widened.data(), patterns.data() + start, count);
			bool all_widened = true;
			for (std::size_t i = 0; i < count; ++i)
			{
				all_widened = all_widened && widens_as_scalar(
					patterns[start + i].get_bits(),
					widened[i]
				);
			}
			REQUIRE( all_widened );
			REQUIRE( widened[count] == untouched_float );

			std::vector<float16_t> narrowed(count + 1, untouched_half);
			narrow_run(narrowed.data(), floats.data() + start, count);
			bool all_narrowed = true;
			for (std::size_t i = 0; i < count; ++i)
			{
				all_narrowed = all_narrowed && narrows_as_scalar(
					floats[start + i],
					narrowed[i].get_bits()
				);
			}
			REQUIRE( all_narrowed );
			REQUIRE( narrowed[count].get_bits() == untouched_half.get_bits() );
		}
	});
}

TEST_CASE(
	"the float16 region loops walk regions and axes as run_contiguous_regions "
	"does",
	"[float16_region_transfer][simd]"
)
{
	// Two regions of 5 rows of 37, held in a source whose rows are 37 wide
	// and placed into a destination whose rows are 64 wide, from an origin.
	const std::size_t rows = 5;
	const std::size_t width = 37;
	const std::ptrdiff_t destination_row = 64;
	const std::ptrdiff_t destination_origin = 64;
	const std::ptrdiff_t region_offsets_destination[] = {64, 64 + 6 * 64};
	const std::ptrdiff_t region_offsets_source[] = {0, 5 * 37};

	std::vector<float16_t> source(2 * rows * width);
	for (std::size_t i = 0; i < source.size(); ++i)
	{
		source[i] = float16_t(static_cast<float>(i) * 0.25F - 40.0F);
	}

	// Where each element of the destination comes from, or nowhere. The
	// pointer handed over is the element at the origin, so the offset of an
	// element is its index in the buffer.
	const auto expected_source = [&] (std::size_t destination_index)
		-> std::ptrdiff_t
	{
		const auto offset = static_cast<std::ptrdiff_t>(destination_index);
		for (std::size_t region = 0; region < 2; ++region)
		{
			const auto in_region =
				offset - region_offsets_destination[region];
			if (in_region < 0)
			{
				continue;
			}
			const auto row = in_region / destination_row;
			const auto column = in_region % destination_row;
			if (row < static_cast<std::ptrdiff_t>(rows) &&
				column < static_cast<std::ptrdiff_t>(width))
			{
				return region_offsets_source[region] +
					row * static_cast<std::ptrdiff_t>(width) + column;
			}
		}
		return -1;
	};

	for_each_target([&]
	{
		{
			INFO( "With the inner axis contiguous on both sides" );
			const auto layout = make_layout(
				{rows, width},
				{destination_row, 1},
				{static_cast<std::ptrdiff_t>(width), 1}
			);
			std::vector<float> destination(12 * 64, untouched_float);

			widen_float16_regions(
				layout,
				make_span(region_offsets_destination, 2),
				make_span(region_offsets_source, 2),
				destination.data() + destination_origin,
				destination_origin,
				source.data(),
				0
			);

			std::string mismatch;
			for (std::size_t i = 0; i < destination.size(); ++i)
			{
				const auto from = expected_source(i);
				const auto expected = from < 0
					? untouched_float
					: static_cast<float>(source[from]);
				if (destination[i] != expected)
				{
					mismatch = "element " + std::to_string(i) + " is " +
						std::to_string(destination[i]) + " rather than " +
						std::to_string(expected);
					break;
				}
			}
			REQUIRE( mismatch == "" );
		}

		{
			INFO( "Narrowing into rows spaced apart" );
			std::vector<float> floats(source.size());
			for (std::size_t i = 0; i < source.size(); ++i)
			{
				floats[i] = static_cast<float>(source[i]) + 0.125F;
			}
			const auto layout = make_layout(
				{rows, width},
				{destination_row, 1},
				{static_cast<std::ptrdiff_t>(width), 1}
			);
			std::vector<float16_t> destination(12 * 64, untouched_half);

			narrow_float16_regions(
				layout,
				make_span(region_offsets_destination, 2),
				make_span(region_offsets_source, 2),
				destination.data() + destination_origin,
				destination_origin,
				floats.data(),
				0
			);

			std::string mismatch;
			for (std::size_t i = 0; i < destination.size(); ++i)
			{
				const auto from = expected_source(i);
				const auto expected = from < 0
					? untouched_half.get_bits()
					: float16_t(floats[from]).get_bits();
				if (destination[i].get_bits() != expected)
				{
					mismatch = "element " + std::to_string(i) + " is " +
						describe(destination[i].get_bits(), 4) +
						" rather than " + describe(expected, 4);
					break;
				}
			}
			REQUIRE( mismatch == "" );
		}
	});
}
