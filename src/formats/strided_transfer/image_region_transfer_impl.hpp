// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_transfer.hpp"

#include "image_region_loop.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <array/cast.hpp>
#include <array/numerical_type_dispatch.hpp>
#include <memory/byte_order.hpp>

#include <complex>
#include <type_traits>

namespace vitrio
{

/**
 * @brief Read one element of a file into an array.
 *
 * Every kernel here names its destination first, as the layout its loop
 * walks does.
 *
 * Conversion goes through @ref cast, which is inline down to the
 * conversions of @ref float16_t: a call per element is something no
 * vectorizer gets through.
 */
struct region_read_kernel
{
	template <typename T, typename Q>
	void operator()(T *array, const Q *file) const noexcept
	{
		cast(array, file);
	}
};

/**
 * @brief Read one element of a file of the other byte order into an array.
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
};

/**
 * @brief Write one element of an array into a file.
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
};

/**
 * @brief Write one element of an array into a file of the other byte order.
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

/**
 * @brief Walk every region with one layout.
 *
 * Each region is the same space reached through a different pair of
 * pointers, so the layout is built once and only the two bases move.
 *
 * The destination comes first, as it does in the layout.
 */
template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_regions(
	const Kernel &kernel,
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	SourcePointer source_data
)
{
	for (std::size_t i = 0; i < destination_offsets.size(); ++i)
	{
		run_region_loop(
			kernel,
			layout,
			destination_data + destination_offsets[i],
			source_data + source_offsets[i]
		);
	}
}

template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_supported_regions(
	std::true_type,
	const Kernel &kernel,
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	SourcePointer source_data
)
{
	run_regions(
		kernel,
		layout,
		destination_offsets,
		source_offsets,
		destination_data,
		source_data
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
	const image_region_layout &,
	span<const std::ptrdiff_t>,
	span<const std::ptrdiff_t>,
	DestinationPointer,
	SourcePointer
)
{
	throw unsupported_operation_error(
		"image_region_transfer: The values of the file can not be converted "
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
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	SourcePointer source_data
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
			source_data
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
			source_data
		);
	}
}

template <typename Q>
void read_regions_as(
	const image_region_read_walk &walk,
	std::size_t first_region,
	std::size_t region_count,
	void *array_data,
	numerical_type array_type,
	const Q *file_data,
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
		[&walk, array_offsets, file_offsets, array_data, file_data, swapped]
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
				file_data
			);
		},
		array_type
	);
}

template <typename Q>
void write_regions_as(
	const image_region_write_walk &walk,
	const void *array_data,
	numerical_type array_type,
	Q *file_data,
	bool swapped
)
{
	const auto &offsets = walk.get_offsets();

	dispatch_numerical_type(
		[&offsets, &walk, array_data, file_data, swapped] (auto array_tag)
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
				offsets.get_file(),
				offsets.get_array(),
				file_data,
				array
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
		const image_region_read_walk&, \
		std::size_t, \
		std::size_t, \
		void*, \
		numerical_type, \
		const __VA_ARGS__*, \
		bool \
	); \
	template void write_regions_as<__VA_ARGS__>( \
		const image_region_write_walk&, \
		const void*, \
		numerical_type, \
		__VA_ARGS__*, \
		bool \
	)

} // namespace vitrio
