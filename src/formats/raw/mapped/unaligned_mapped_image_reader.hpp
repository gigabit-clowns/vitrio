// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_file_mapping.hpp"

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <string>

namespace vitrio
{

/**
 * @brief Reads a file whose values are contiguous but not aligned for their
 * data type, through a mapping of it.
 *
 * A format that places its values wherever the bytes before them end, as
 * DigitalMicrograph does, can not have them addressed where they lie, which
 * is what @ref mapped_image_reader does, and why it refuses them. This
 * reader copies them first: a read moves the rows its regions reach out of
 * the mapping a page at a time, into a page of its own that is aligned, and
 * moves the regions out of that. The copy is of bytes, so the
 * values keep the byte order of the file until they are converted.
 *
 * Each read has a page of its own, so @ref read can be called concurrently,
 * as on @ref mapped_image_reader. A page is only written where the rows a
 * read reaches go, and is otherwise left as it was allocated.
 */
class unaligned_mapped_image_reader final
	: public image_reader
{
public:
	/**
	 * @brief Map a file for reading.
	 *
	 * @param path Path to the file.
	 * @param descriptor The shape and data type of the values. An image of
	 * rank two or a stack of them of rank three, whose values are laid out
	 * contiguously, the columns changing fastest.
	 * @param data_offset Where the values begin in the file, in bytes.
	 * @param order The byte order of the values.
	 * @param metadata What the file states beyond its shape and data type.
	 * @throws std::invalid_argument If @p descriptor is of neither rank two
	 * nor three.
	 * @throws image_file_error If the file can not be mapped.
	 * @throws image_file_format_error If the file is shorter than the values
	 * it is said to hold.
	 */
	unaligned_mapped_image_reader(
		const std::string &path,
		image_descriptor descriptor,
		std::size_t data_offset,
		byte_order order,
		image_metadata metadata
	);

	~unaligned_mapped_image_reader() override = default;

	/**
	 * @brief Tell whether the reader can read values laid out as a layout
	 * states: an image, or a stack of them, laid out one value after
	 * another, the columns changing fastest.
	 *
	 * @param layout The layout.
	 * @return bool Whether it can.
	 */
	static bool supports(const raw_image_layout &layout) noexcept;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

private:
	image_file_mapping m_mapping;
	image_descriptor m_descriptor;
	std::size_t m_data_offset;
	byte_order m_byte_order;
	image_metadata m_metadata;
	std::size_t m_row_size;
	std::size_t m_page_size;
};

} // namespace vitrio
