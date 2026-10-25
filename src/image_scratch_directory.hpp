// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace vitrio
{

class file_transfer_pool;
class image_scratch_storage;

/**
 * @brief A directory of scratch files that stays within a budget.
 *
 * Each file holds the scratch of one fingerprint and is named after it, so
 * a scratch laid out as an earlier one finds the file that one left. When a
 * file has to be made room for, the files of other scratches are removed,
 * the one used least recently first. A file is used when it is acquired,
 * which sets its modification time to the present.
 *
 * Only the files it names are counted and removed. Anything else in the
 * directory is left alone and is not part of the budget.
 *
 * A file that can not be removed, such as one that is open on Windows,
 * keeps its room. One that is removed while open elsewhere, as POSIX
 * allows, is gone from the directory at once, but the space it takes on
 * the disk is freed only once nothing has it open.
 *
 * @par Thread safety
 * Every method may be called concurrently, also by several processes over
 * the same directory: they take turns through a lock file in it.
 */
class image_scratch_directory final
{
public:
	/**
	 * @brief Construct a directory of scratch files.
	 *
	 * @param path Path of the directory. It is created if it is missing.
	 * @param budget The most bytes that the files of the directory may take
	 * together.
	 * @throws image_file_error If the directory can not be created, or if
	 * it is a file.
	 */
	image_scratch_directory(std::string path, std::size_t budget);

	/**
	 * @brief Get the path of the directory.
	 *
	 * @return const std::string& The path.
	 */
	const std::string& get_path() const noexcept;

	/**
	 * @brief Get the budget of the directory.
	 *
	 * @return std::size_t The most bytes its files may take together.
	 */
	std::size_t get_budget() const noexcept;

	/**
	 * @brief Get the path of the file of a scratch.
	 *
	 * @param fingerprint The fingerprint of the scratch.
	 * @return std::string The path. The file need not be there.
	 */
	std::string get_file_path(std::uint64_t fingerprint) const;

	/**
	 * @brief Open the file of a scratch as its storage, after making room
	 * for it.
	 *
	 * The files of other scratches are removed, the one used least recently
	 * first, until the file of this one fits in the budget beside those
	 * that remain. That file is then created if it is missing and given
	 * @p size, as @ref create_file_image_scratch_storage does. It keeps what
	 * it holds, so a scratch can resume from it.
	 *
	 * @param fingerprint The fingerprint of the scratch.
	 * @param size The size of its file, in bytes.
	 * @param transfers The threads that carry out the transfers of the file.
	 * @param room Set to the most bytes a file may take beside the files
	 * that remain. It is at least @p size when the file is opened.
	 * @return std::shared_ptr<image_scratch_storage> The storage, or null
	 * if the file does not fit even beside the files that could not be
	 * removed, every other one having been. Nothing is created then.
	 * @throws image_file_error If the directory can not be locked or
	 * listed, or if the file can not be created, sized, given its room or
	 * opened.
	 */
	std::shared_ptr<image_scratch_storage> acquire(
		std::uint64_t fingerprint,
		std::size_t size,
		const file_transfer_pool &transfers,
		std::size_t &room
	);

private:
	std::string m_path;
	std::size_t m_budget;
};

} // namespace vitrio
