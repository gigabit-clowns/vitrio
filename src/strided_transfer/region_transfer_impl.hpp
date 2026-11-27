// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "region_transfer.hpp"

#include <strided_transfer/region_loop/region_loop.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <array/cast.hpp>
#include <array/numerical_type_dispatch.hpp>
#include <memory/byte_order.hpp>

#if defined(VITRIO_ENABLE_SIMD)
#include <strided_transfer/simd/float16_region_transfer.hpp>
#endif

#include <complex>
#include <type_traits>

namespace vitrio
{

/**
 * @brief Apply a kernel element by element over a run that is contiguous on
 * both sides.
 *
 * It is what the run operation of a kernel does unless the kernel has a
 * better way, and the loop a compiler vectorizes: conversion goes through
 * @ref cast, which is inline down to the conversions of @ref float16_t, as
 * a call per element is something no vectorizer gets through.
 */
template <typename Kernel, typename Destination, typename Source>
inline void run_kernel_over(
	const Kernel &kernel,
	Destination *destination,
	const Source *source,
	std::size_t count
) noexcept
{
	for (std::size_t i = 0; i < count; ++i)
	{
		kernel(destination + i, source + i);
	}
}

/**
 * @brief Read the elements of a file into an array.
 *
 * Every kernel here names its destination first, as the layout its loop
 * walks does, and has two operations: one element, and a run of them that
 * is contiguous on both sides, which is where a loop written for the vector
 * units of the processor plugs in.
 */
struct region_read_kernel
{
	template <typename T, typename Q>
	void operator()(T *array, const Q *file) const noexcept
	{
		cast(array, file);
	}

	template <typename T, typename Q>
	void operator()(T *array, const Q *file, std::size_t count) const noexcept
	{
		run_kernel_over(*this, array, file, count);
	}
};

/**
 * @brief Read the elements of a file of the other byte order into an array.
 *
 * The bytes are reversed in the type the file holds them in, before the
 * conversion. Reversing a value of the array's type instead would be a
 * different number whenever the two widths differ.
 */
struct byte_swapped_region_read_kernel
{
	template <typename T, typename Q>
	void operator()(T *array, const Q *file) const noexcept
	{
		const auto value = reverse_byte_order(*file);
		cast(array, &value);
	}

	template <typename T, typename Q>
	void operator()(T *array, const Q *file, std::size_t count) const noexcept
	{
		run_kernel_over(*this, array, file, count);
	}
};

/**
 * @brief Write the elements of an array into a file.
 *
 * @see region_read_kernel
 */
struct region_write_kernel
{
	template <typename Q, typename T>
	void operator()(Q *file, const T *array) const noexcept
	{
		cast(file, array);
	}

	template <typename Q, typename T>
	void operator()(Q *file, const T *array, std::size_t count) const noexcept
	{
		run_kernel_over(*this, file, array, count);
	}
};

/**
 * @brief Write the elements of an array into a file of the other byte order.
 *
 * The conversion happens first and the bytes are reversed afterwards, so that
 * they are reversed in the type the file holds them in.
 *
 * @see byte_swapped_region_read_kernel
 */
struct byte_swapped_region_write_kernel
{
	template <typename Q, typename T>
	void operator()(Q *file, const T *array) const noexcept
	{
		Q value;
		cast(&value, array);
		*file = reverse_byte_order(value);
	}

	template <typename Q, typename T>
	void operator()(Q *file, const T *array, std::size_t count) const noexcept
	{
		run_kernel_over(*this, file, array, count);
	}
};

/**
 * @brief Whether one element type can be produced from another.
 *
 * Every type can, with one exception: a complex number has no real
 * counterpart, so nothing but another complex type is produced from one.
 *
 * It is stated here rather than asked of the types, because what C++ lets
 * convert implicitly follows other rules: a complex of doubles does not
 * convert to one of floats that way, although a double converts to a float.
 */
template <typename Destination, typename Source>
struct region_transfer_support : std::true_type
{
};

template <typename Destination, typename Q>
struct region_transfer_support<Destination, std::complex<Q>> : std::false_type
{
};

template <typename T, typename Q>
struct region_transfer_support<std::complex<T>, std::complex<Q>>
	: std::true_type
{
};

#if defined(VITRIO_ENABLE_SIMD)

/**
 * @brief Walk the regions of a transfer with a loop written for the vector
 * units of the processor where the layout hands it runs, and with the
 * generic loop otherwise.
 *
 * The loops of simd/ take the runs a contiguous innermost axis hands them. A
 * layout with a strided side converts one element at a time, which the
 * generic loop does as well as any.
 *
 * @tparam Vectorized The loop, called with the arguments of
 * @c run_contiguous_regions but the kernel.
 */
template <
	typename Vectorized,
	typename Kernel,
	typename Destination,
	typename Source
>
inline void run_regions_vectorized(
	Vectorized vectorized,
	const Kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	Destination *destination_data,
	std::ptrdiff_t destination_origin,
	const Source *source_data,
	std::ptrdiff_t source_origin
)
{
	if (layout.is_inner_axis_contiguous())
	{
		vectorized(
			layout,
			destination_offsets,
			source_offsets,
			destination_data,
			destination_origin,
			source_data,
			source_origin
		);
	}
	else
	{
		// The template, named as such so that this does not resolve back to
		// the overloads below.
		run_regions<Kernel, Destination*, const Source*>(
			kernel,
			layout,
			destination_offsets,
			source_offsets,
			destination_data,
			destination_origin,
			source_data,
			source_origin
		);
	}
}

/**
 * @brief Walk the regions of a transfer between half precision values and
 * floats in the native byte order with the loops of simd/.
 *
 * These are what a call of @c run_regions with one of the two native kernels
 * and these two element types resolves to: exact matches that are not
 * templates. Every other pair of types, and the byte-swapped kernels, keep
 * the template.
 */
inline void run_regions(
	const region_read_kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float *destination_data,
	std::ptrdiff_t destination_origin,
	const float16_t *source_data,
	std::ptrdiff_t source_origin
)
{
	run_regions_vectorized(
		&widen_float16_regions,
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		destination_origin,
		source_data,
		source_origin
	);
}

inline void run_regions(
	const region_read_kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float16_t *destination_data,
	std::ptrdiff_t destination_origin,
	const float *source_data,
	std::ptrdiff_t source_origin
)
{
	run_regions_vectorized(
		&narrow_float16_regions,
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		destination_origin,
		source_data,
		source_origin
	);
}

inline void run_regions(
	const region_write_kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float *destination_data,
	std::ptrdiff_t destination_origin,
	const float16_t *source_data,
	std::ptrdiff_t source_origin
)
{
	run_regions_vectorized(
		&widen_float16_regions,
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		destination_origin,
		source_data,
		source_origin
	);
}

inline void run_regions(
	const region_write_kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float16_t *destination_data,
	std::ptrdiff_t destination_origin,
	const float *source_data,
	std::ptrdiff_t source_origin
)
{
	run_regions_vectorized(
		&narrow_float16_regions,
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		destination_origin,
		source_data,
		source_origin
	);
}

#endif // VITRIO_ENABLE_SIMD

template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_supported_regions(
	std::true_type,
	const Kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	std::ptrdiff_t destination_origin,
	SourcePointer source_data,
	std::ptrdiff_t source_origin
)
{
	run_regions(
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		destination_origin,
		source_data,
		source_origin
	);
}

// The unsupported overload never instantiates a loop, which is what keeps one
// from being compiled for every pair of element types no conversion joins.
template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
[[noreturn]]
void run_supported_regions(
	std::false_type,
	const Kernel &,
	const region_layout &,
	span<const std::ptrdiff_t>,
	span<const std::ptrdiff_t>,
	DestinationPointer,
	std::ptrdiff_t,
	SourcePointer,
	std::ptrdiff_t
)
{
	throw unsupported_operation_error(
		"region_transfer: The values of the file can not be converted "
		"into the data type asked for."
	);
}

/**
 * @brief Walk the regions with the kernel for the byte order the file states
 * its values in.
 *
 * The destination comes first, as it does in the layout.
 */
template <
	typename Support,
	typename Kernel,
	typename SwappedKernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_regions_in_byte_order(
	bool swapped,
	Support support,
	const Kernel &kernel,
	const SwappedKernel &swapped_kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	std::ptrdiff_t destination_origin,
	SourcePointer source_data,
	std::ptrdiff_t source_origin
)
{
	if (swapped)
	{
		run_supported_regions(
			support,
			swapped_kernel,
			layout,
			destination_offsets,
			source_offsets,
			destination_data,
			destination_origin,
			source_data,
			source_origin
		);
	}
	else
	{
		run_supported_regions(
			support,
			kernel,
			layout,
			destination_offsets,
			source_offsets,
			destination_data,
			destination_origin,
			source_data,
			source_origin
		);
	}
}

template <typename Q>
void read_regions_as(
	const region_read_walk &walk,
	std::size_t first_region,
	std::size_t region_count,
	void *array_data,
	numerical_type array_type,
	const Q *file_data,
	std::ptrdiff_t file_origin,
	bool swapped
)
{
	const auto &offsets = walk.get_offsets();

	// The run is taken here rather than inside the loop, so that what walks
	// the regions is the same loop whether it is handed every region or a
	// step of them.
	const auto array_offsets =
		make_span(offsets.get_array().data() + first_region, region_count);
	const auto file_offsets =
		make_span(offsets.get_file().data() + first_region, region_count);

	dispatch_numerical_type(
		[
			&walk, array_offsets, file_offsets, array_data, file_data,
			file_origin, swapped
		]
		(auto array_tag)
		{
			using T = typename decltype(array_tag)::type;
			const auto support = region_transfer_support<T, Q>();
			auto *array = static_cast<T*>(array_data);

			// The array is the destination here, so it comes first.
			run_regions_in_byte_order(
				swapped,
				support,
				region_read_kernel(),
				byte_swapped_region_read_kernel(),
				walk.get_layout(),
				array_offsets,
				file_offsets,
				array,
				0,
				file_data,
				file_origin
			);
		},
		array_type
	);
}

template <typename Q>
void write_regions_as(
	const region_write_walk &walk,
	std::size_t first_region,
	std::size_t region_count,
	const void *array_data,
	numerical_type array_type,
	Q *file_data,
	std::ptrdiff_t file_origin,
	bool swapped
)
{
	const auto &offsets = walk.get_offsets();

	// The run is taken here, as read_regions_as takes it.
	const auto array_offsets =
		make_span(offsets.get_array().data() + first_region, region_count);
	const auto file_offsets =
		make_span(offsets.get_file().data() + first_region, region_count);

	dispatch_numerical_type(
		[
			&walk, array_offsets, file_offsets, array_data, file_data,
			file_origin, swapped
		]
		(auto array_tag)
		{
			using T = typename decltype(array_tag)::type;
			const auto support = region_transfer_support<Q, T>();
			const auto *array = static_cast<const T*>(array_data);

			// The file is the destination here, so it comes first.
			run_regions_in_byte_order(
				swapped,
				support,
				region_write_kernel(),
				byte_swapped_region_write_kernel(),
				walk.get_layout(),
				file_offsets,
				array_offsets,
				file_data,
				file_origin,
				array,
				0
			);
		},
		array_type
	);
}

/**
 * @brief Instantiate the region transfer for one element type of a file.
 *
 * Write it once in a translation unit of its own per element type. The array
 * side is a grid over every data type there is, so one element type is
 * already many loops to compile.
 */
#define VITRIO_INSTANTIATE_IMAGE_REGION_TRANSFER(...) \
	template void read_regions_as<__VA_ARGS__>( \
		const region_read_walk&, \
		std::size_t, \
		std::size_t, \
		void*, \
		numerical_type, \
		const __VA_ARGS__*, \
		std::ptrdiff_t, \
		bool \
	); \
	template void write_regions_as<__VA_ARGS__>( \
		const region_write_walk&, \
		std::size_t, \
		std::size_t, \
		const void*, \
		numerical_type, \
		__VA_ARGS__*, \
		std::ptrdiff_t, \
		bool \
	)

} // namespace vitrio
