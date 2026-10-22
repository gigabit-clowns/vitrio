// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/raw_image_layout.hpp>

#include <vitrio/image_metadata.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_reader;

/**
 * @brief Open a reader of a file that holds its values as they are, where and
 * how a layout states.
 *
 * Every format whose values lie in the file as they are opens its files
 * through this, and the reader suits the system and the values. Where the
 * process can have asynchronous queues of file reads, the reader queues
 * its reads of the file, whatever the layout. Elsewhere it reads through a
 * mapping of the file where the values are aligned for their data type, and
 * through pages copied out of a mapping where they are not, which takes an
 * image, or a stack of them, laid out one value after another.
 *
 * @param path Path to the file.
 * @param layout Where and how the file holds its values.
 * @param metadata What the file states beyond its shape and data type.
 * @return std::shared_ptr<image_reader> The reader. Never null.
 * @throws image_file_error If the file can not be opened or mapped.
 * @throws image_file_format_error If the file is shorter than @p layout says
 * it is, or if it is read through a mapping and its values are not aligned
 * and are not an image or a stack of them laid out one value after another.
 */
std::shared_ptr<image_reader> open_raw_image_reader(
	const std::string &path,
	raw_image_layout layout,
	image_metadata metadata
);

} // namespace vitrio
