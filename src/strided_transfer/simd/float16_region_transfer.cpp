// SPDX-License-Identifier: LGPL-2.1-or-later

// Highway compiles this file once per instruction set the build asks for: it
// includes the file again from foreach_target.h, each time with HWY_TARGET
// and HWY_NAMESPACE set for one of them, and once more with HWY_ONCE set for
// what is compiled a single time. Everything between HWY_BEFORE_NAMESPACE and
// HWY_AFTER_NAMESPACE is compiled under the attributes of that instruction
// set, and the body of the loop is included there so that it is too, with
// the kernel inlined into it.

#include "float16_region_transfer.hpp"

#include <strided_transfer/region_loop/image_region_loop.hpp>

#include <array/cast.hpp>

#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "strided_transfer/simd/float16_region_transfer.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace vitrio
{
namespace HWY_NAMESPACE
{

#include <strided_transfer/region_loop/image_region_loop_impl.hpp>

namespace hn = hwy::HWY_NAMESPACE;

// A half precision value is handed to Highway as its bits: that is what
// Highway promises to load, store and convert on every instruction set,
// whatever it holds a half precision lane as.
using float_tag = hn::ScalableTag<float>;
using half_tag = hn::Rebind<hwy::float16_t, float_tag>;
using bits_tag = hn::Rebind<std::uint16_t, float_tag>;

static_assert(
	sizeof(float16_t) == sizeof(std::uint16_t),
	"A float16_t is loaded as its bits"
);

// Widens a vector of the bits of half precision values into floats.
//
// Where the instruction set has no conversion of its own, the releases of
// Highway up to 1.4.0 widen an infinity or a NaN into a finite float: the
// exponent of all ones becomes 31 + 112 rather than 255. Every such result
// is 65536 or more in magnitude, which no half precision number is, so those
// lanes are told apart and their exponent set to all ones, which keeps the
// mantissa, and with it the sign, the payload and the quietness of a NaN. On
// an instruction set that converts, the lanes it changes are the infinities,
// which it leaves as they are.
HWY_INLINE hn::VFromD<float_tag> widen_bits(hn::VFromD<bits_tag> bits)
{
	const float_tag d32;
	const half_tag d16;
	const auto widened = hn::PromoteTo(d32, hn::BitCast(d16, bits));
#if HWY_MAJOR == 1 && HWY_MINOR <= 4
	const hn::RebindToUnsigned<float_tag> du32;
	const auto overflowed =
		hn::Ge(hn::Abs(widened), hn::Set(d32, 65536.0F));
	const auto special = hn::BitCast(
		d32,
		hn::Or(hn::BitCast(du32, widened), hn::Set(du32, 0x7F800000U))
	);
	return hn::IfThenElse(overflowed, special, widened);
#else
	return widened;
#endif
}

// Widens half precision values into floats: one at a time where a side is
// strided, and a vector at a time over a run that is contiguous on both.
struct float16_widening_kernel
{
	void operator()(float *destination, const float16_t *source)
		const noexcept
	{
		cast(destination, source);
	}

	void operator()(
		float *destination,
		const float16_t *source,
		std::size_t count
	) const noexcept
	{
		const float_tag d32;
		const bits_tag du16;
		const auto *bits = reinterpret_cast<const std::uint16_t*>(source);
		const auto lanes = hn::Lanes(d32);

		std::size_t i = 0;
		for (; i + lanes <= count; i += lanes)
		{
			hn::StoreU(
				widen_bits(hn::LoadU(du16, bits + i)),
				d32,
				destination + i
			);
		}
		if (i < count)
		{
			const auto rest = count - i;
			hn::StoreN(
				widen_bits(hn::LoadN(du16, bits + i, rest)),
				d32,
				destination + i,
				rest
			);
		}
	}
};

// Narrows floats into half precision values, as the widening kernel widens.
struct float16_narrowing_kernel
{
	void operator()(float16_t *destination, const float *source)
		const noexcept
	{
		cast(destination, source);
	}

	void operator()(
		float16_t *destination,
		const float *source,
		std::size_t count
	) const noexcept
	{
		const float_tag d32;
		const half_tag d16;
		const bits_tag du16;
		auto *bits = reinterpret_cast<std::uint16_t*>(destination);
		const auto lanes = hn::Lanes(d32);

		std::size_t i = 0;
		for (; i + lanes <= count; i += lanes)
		{
			const auto half = hn::DemoteTo(d16, hn::LoadU(d32, source + i));
			hn::StoreU(hn::BitCast(du16, half), du16, bits + i);
		}
		if (i < count)
		{
			const auto rest = count - i;
			const auto half =
				hn::DemoteTo(d16, hn::LoadN(d32, source + i, rest));
			hn::StoreN(hn::BitCast(du16, half), du16, bits + i, rest);
		}
	}
};

// The whole walk, for this instruction set, over the layouts whose innermost
// axis the kernels take as runs. The loop is named through this namespace:
// the arguments would otherwise also find the one of vitrio.
void widen_regions(
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float *destination,
	std::ptrdiff_t destination_origin,
	const float16_t *source,
	std::ptrdiff_t source_origin
)
{
	HWY_NAMESPACE::run_contiguous_regions(
		float16_widening_kernel(),
		layout,
		destination_offsets,
		source_offsets,
		destination,
		destination_origin,
		source,
		source_origin
	);
}

void narrow_regions(
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float16_t *destination,
	std::ptrdiff_t destination_origin,
	const float *source,
	std::ptrdiff_t source_origin
)
{
	HWY_NAMESPACE::run_contiguous_regions(
		float16_narrowing_kernel(),
		layout,
		destination_offsets,
		source_offsets,
		destination,
		destination_origin,
		source,
		source_origin
	);
}

} // namespace HWY_NAMESPACE
} // namespace vitrio
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace vitrio
{

HWY_EXPORT(widen_regions);
HWY_EXPORT(narrow_regions);

void widen_float16_regions(
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float *destination,
	std::ptrdiff_t destination_origin,
	const float16_t *source,
	std::ptrdiff_t source_origin
)
{
	HWY_DYNAMIC_DISPATCH(widen_regions)(
		layout,
		destination_offsets,
		source_offsets,
		destination,
		destination_origin,
		source,
		source_origin
	);
}

void narrow_float16_regions(
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float16_t *destination,
	std::ptrdiff_t destination_origin,
	const float *source,
	std::ptrdiff_t source_origin
)
{
	HWY_DYNAMIC_DISPATCH(narrow_regions)(
		layout,
		destination_offsets,
		source_offsets,
		destination,
		destination_origin,
		source,
		source_origin
	);
}

} // namespace vitrio

#endif // HWY_ONCE
