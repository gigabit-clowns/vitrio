// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <memory>

namespace vitrio
{

class image_scratch_entry;

/**
 * @brief Reads a file through a scratch.
 *
 * The regions the scratch entry holds are read from it, on the thread that
 * starts the read. The others are offered to it out of the file, and once it
 * has taken them in they are read from it by a task on an executor. Whatever
 * it still does not hold is then read from the file itself. A read the
 * scratch entry serves in full does not read the backing file, and none of
 * the steps waits for another.
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
	 * @param tasks Where what the entry takes in is read from it. Must not
	 * be null.
	 */
	scratch_image_reader(
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<const image_reader> file,
		std::shared_ptr<executor> tasks
	);

	~scratch_image_reader() override;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const override;

private:
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<const image_reader> m_file;
	std::shared_ptr<executor> m_tasks;
};

} // namespace vitrio
