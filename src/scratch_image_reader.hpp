// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_reader.hpp>

#include <memory>

namespace vitrio
{

class image_scratch_entry;

/**
 * @brief Reads a files through a scratch.
 *
 * The regions the scratch entry holds are read from it. The others are offered 
 * to it out of the file and asked of it again, and whatever it still does not
 * hold is read from the file itself. A read the scratch entry serves in full
 * does not read the backing file.
 *
 * What the reader reports is what the reader over the file reports.
 */
class scratch_image_reader final
	: public image_reader
{
public:
	/**
	 * @brief Construct a reader over an entry and the file it is of.
	 *
	 * @param entry What is held of the file. Must not be null.
	 * @param file A reader over the file. Must not be null.
	 */
	scratch_image_reader(
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<const image_reader> file
	);

	~scratch_image_reader() override;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

private:
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<const image_reader> m_file;
};

} // namespace vitrio
