// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_writer;
class file_transfer_pool;

/**
 * @brief Create a file that holds its values as they are, where and how a
 * layout states, and open a writer of it.
 *
 * Every format whose values lie in the file as they are creates its files
 * through this. The writer queues its writes, and its syncs, through the
 * threads of @p transfers, whose ports carry them out asynchronously where
 * the process can have such ports and as they are asked elsewhere. Writes
 * the system carries out as they are asked still beat writes through a
 * mapping, whose flushes can not overlap.
 *
 * Any file already at that path is replaced. It is laid out in full before
 * this returns: sized, and @p preamble written where it begins.
 *
 * @param path Path to the file to create.
 * @param layout Where and how the file holds its values.
 * @param preamble The bytes the file begins with, before its values.
 * @param transfers The threads that carry out the writes of the file.
 * @return std::shared_ptr<image_writer> The writer. Never null.
 * @throws std::invalid_argument If @p preamble reaches past where @p layout
 * says the values begin.
 * @throws image_file_error If the file could not be created, sized or
 * opened.
 */
std::shared_ptr<image_writer> open_raw_image_writer(
	const std::string &path,
	raw_image_layout layout,
	span<const byte> preamble,
	const file_transfer_pool &transfers
);

} // namespace vitrio
