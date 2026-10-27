// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_file_mapping.hpp"

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/synchronous_image_reader.hpp>

#include <string>

namespace vitrio
{

/**
 * @brief Reads a file whose values are laid out as a @ref raw_image_layout
 * states, through a mapping of it.
 *
 * The file is mapped when the reader is constructed, so reading a region is
 * the transfer and nothing else. Everything the reader reports is settled at
 * that point and reading does not change it, which is what lets @ref read be
 * called concurrently: the mapping and what a call resolves are the only
 * state a read touches, and the latter is not shared across calls.
 *
 * A read asks for the stretches it is about to touch before it touches them,
 * a step ahead of the one it is walking, as @ref read_prefetched_regions
 * describes.
 */
class mapped_image_reader final
	: public synchronous_image_reader
{
public:
	/**
	 * @brief Map a file for reading.
	 *
	 * @param path Path to the file.
	 * @param layout Where and how the file holds its values, which must be
	 * aligned for their data type: a mapping addresses them where they lie.
	 * @param metadata What the file states beyond its shape and data type.
	 * @throws image_file_error If the file can not be mapped.
	 * @throws image_file_format_error If the values of @p layout are not
	 * aligned, or if the file is shorter than @p layout says it is.
	 */
	mapped_image_reader(
		const std::string &path,
		raw_image_layout layout,
		image_metadata metadata
	);

	~mapped_image_reader() override = default;

	/**
	 * @brief Tell whether the reader can read values laid out as a layout
	 * states: whether they are aligned for their data type.
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
	raw_image_layout m_layout;
	image_metadata m_metadata;

	/// The values of the file, as an array over the mapping reads them.
	array_descriptor m_values;
};

} // namespace vitrio
