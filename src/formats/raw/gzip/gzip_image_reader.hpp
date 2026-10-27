// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/image_metadata.hpp>
#include <vitrio/synchronous_image_reader.hpp>

#include <string>

namespace vitrio
{

/**
 * @brief Reads a file compressed with gzip whose inflated bytes lay out
 * their values as a @ref raw_image_layout states.
 *
 * gzip can only be inflated from its start, and nothing inflated is kept
 * between reads: each read inflates the file from its start, passes over
 * the bytes before the first value it needs, and stops after the last one.
 * A file is usually read once, whole, so keeping its values would hold
 * memory for nothing. A read of what lies far into a large file is slow,
 * since everything before it is inflated again.
 *
 * Regions that lie in the file as they land in the array are inflated
 * straight into it. Any others are inflated a stretch at a time into
 * staging, and converted from there, as the staged reads of raw files are.
 *
 * Each read inflates through a stream of its own, and nothing else changes,
 * so reads may be made concurrently.
 */
class gzip_image_reader final
	: public synchronous_image_reader
{
public:
	/**
	 * @brief Construct a reader of a compressed file.
	 *
	 * Nothing of the file is read until a read is made.
	 *
	 * @param path Path to the file.
	 * @param layout Where and how its inflated bytes hold the values.
	 * @param metadata What the file states beyond its shape and data type.
	 */
	gzip_image_reader(
		std::string path,
		raw_image_layout layout,
		image_metadata metadata
	);

	~gzip_image_reader() override = default;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	/**
	 * @copydoc synchronous_image_reader::read
	 *
	 * @throws image_file_error If the file can not be opened or read.
	 * @throws image_file_format_error If the file is not a gzip stream, or if
	 * its stream ends before the values the read needs.
	 */
	void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

private:
	std::string m_path;
	raw_image_layout m_layout;
	image_metadata m_metadata;
};

} // namespace vitrio
