// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_scratch_entry;

/**
 * @brief Copies of image files, kept where they are faster to reach than
 * the files themselves.
 *
 * A scratch holds one @ref image_scratch_entry for each file it keeps
 * something of, and finds it by the key of that file. What an entry keeps of
 * its file is the entry's implementation's own business.
 *
 * @par Thread safety
 * Every method may be called concurrently.
 */
class VITRIO_API image_scratch
{
public:
	image_scratch() noexcept;
	image_scratch(const image_scratch &other) = delete;
	image_scratch(image_scratch &&other) = delete;
	virtual ~image_scratch();

	image_scratch& operator=(const image_scratch &other) = delete;
	image_scratch& operator=(image_scratch &&other) = delete;

	/**
	 * @brief Get what is held of a file, to read it and to add to it.
	 *
	 * Shared ownership rather than a reference, so that an entry stays
	 * alive for as long as it is still read through.
	 *
	 * @param key Key of the file.
	 * @return std::shared_ptr<image_scratch_entry> The entry of the file, or
	 * null when nothing of it is held.
	 */
	virtual std::shared_ptr<image_scratch_entry>
	find(const std::string &key) = 0;
};

} // namespace vitrio
