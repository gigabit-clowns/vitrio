// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_scratch.hpp>
#include <vitrio/image_scratch_open_mode.hpp>
#include <vitrio/platform.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

namespace vitrio
{

class executor;
class file_transfer_pool;
class image_location_grouping;
class image_reader_provider;
class image_scratch_storage;

/**
 * @brief A scratch that holds the images that image_location-s can name, each
 * in a slot of its storage.
 *
 * What it holds of a file are indices of the first axis of the file, which
 * is what an @ref image_location names: for example the images of a stack.
 * Each held index has a slot in the storage.
 *
 * It holds the images that a grouping of locations names, as far as they
 * fit in the storage. Each file gets one entry, and the entries are stored
 * one after another. The storage decides where the copies live: in main
 * memory, or in a file.
 *
 * Which images are held is fixed at construction. Entries are loaded from
 * their files when regions are stored into them.
 *
 * Loading works in runs. A run is a block of consecutive held images of one
 * file. Every run of a file has the same number of images, except the last
 * one, which may have fewer. Storing any image loads its whole run in one
 * read of the file.
 *
 * The storage also records what is loaded, so that a scratch constructed
 * later over the same storage can continue from it. The storage starts with
 * a fingerprint of how the scratch is laid out: the keys and the held
 * indices, the run length, and the data type and extents of each file.
 * Each run has a flag, which holds the modification time that its file had
 * when the run was loaded. The time is asked of the file system, with the
 * key as the path of the file. A file whose time can not be told is loaded
 * again by every scratch.
 */
class VITRIO_API indexed_image_scratch final
	: public image_scratch
{
public:
	/**
	 * @brief Construct a scratch for a grouping of locations.
	 *
	 * A location with a stack index names that index of the first axis of
	 * its file. A location without one names the whole file.
	 *
	 * Files are taken in the order of the grouping, which is ascending
	 * order of key. Each is opened once, to learn its shape and data type,
	 * all of them at once on @p tasks, which this waits for: it must not be
	 * called on a thread of @p tasks. If the storage runs out of room, the
	 * current file keeps the lowest indices that fit and the remaining files
	 * hold nothing.
	 *
	 * @param locations The images to hold, grouped by file.
	 * @param files Provider used to open the files.
	 * @param tasks Where the files are opened.
	 * @param storage Where the copies are stored. Its size is the capacity
	 * of the scratch.
	 * @param run_length Number of held indices per run. A file that holds
	 * no more indices than this is a single run.
	 * @param mode Whether the scratch starts empty or resumed.
	 * - An empty scratch has nothing loaded, and does not read @p storage.
	 * - A resumed scratch keeps the runs that an earlier scratch loaded
	 * into @p storage, if that scratch had the same fingerprint. A run is
	 * kept only if its file has not been modified since. A file whose
	 * modification time can not be read is never kept. If the fingerprint
	 * differs, the scratch starts empty. The contents of @p storage must be
	 * defined, which those of newly allocated memory are not.
	 * @throws std::invalid_argument If @p storage is null, if it is smaller
	 * than a fingerprint, or if @p run_length is zero.
	 * @throws std::out_of_range If a location has a stack index that its
	 * file does not have.
	 * @throws image_file_error If a file does not exist or can not be read,
	 * or if @p storage can not be read or written.
	 * @throws unsupported_operation_error If no format can read a file.
	 * @throws image_file_format_error If a file is malformed or truncated.
	 */
	indexed_image_scratch(
		const image_location_grouping &locations,
		image_reader_provider &files,
		executor &tasks,
		std::shared_ptr<image_scratch_storage> storage,
		std::size_t run_length,
		image_scratch_open_mode mode = image_scratch_open_mode::empty
	);

	~indexed_image_scratch() override;

	std::shared_ptr<image_scratch_entry>
	find(const std::string &key) override;

private:
	using entry_map_type =
		std::unordered_map<std::string, std::shared_ptr<image_scratch_entry>>;

	VITRIO_STD_MEMBER_INTERFACE
	entry_map_type m_entries;
};

/**
 * @brief Create a scratch that stores its copies in main memory.
 *
 * The memory is as large as the scratch needs for the images it holds. The
 * scratch holds every image of @p locations, unless that needs more than
 * @p max_size. It then holds the images that fit, as
 * @ref indexed_image_scratch describes.
 *
 * Each file is opened twice through @p files, all of them at once on
 * @p tasks, which this waits for: once to compute the size and once to
 * construct the scratch. It must not be called on a thread of @p tasks.
 *
 * @param locations The images to hold, grouped by file.
 * @param files Provider used to open the files.
 * @param tasks Where the files are opened.
 * @param run_length Number of held indices per run.
 * @param max_size Maximum number of bytes to allocate. By default there
 * is no maximum.
 * @return std::shared_ptr<image_scratch> The scratch. Never null.
 * @throws std::invalid_argument If @p run_length is zero, or if there is
 * nothing to hold: @p locations names no image, or @p max_size has no room
 * for one.
 * @throws std::bad_alloc If the memory can not be allocated.
 * @throws std::out_of_range If a location has a stack index that its
 * file does not have.
 * @throws image_file_error If a file does not exist or can not be read.
 * @throws unsupported_operation_error If no format can read a file.
 * @throws image_file_format_error If a file is malformed or truncated.
 */
VITRIO_API
std::shared_ptr<image_scratch> create_host_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	std::size_t run_length,
	std::size_t max_size = std::numeric_limits<std::size_t>::max()
);

/**
 * @brief Create a scratch that stores its copies in a file.
 *
 * The file is read and written by the threads of @p transfers, as
 * @ref create_file_image_scratch_storage describes, and is as large as the
 * scratch needs for the images it holds. The scratch holds every image of
 * @p locations, unless that needs more than @p max_size. It then holds the
 * images that fit, as @ref indexed_image_scratch describes.
 *
 * If the file at @p path already holds a scratch with the same
 * fingerprint, the scratch resumes from what that file has loaded.
 * Programs that hold the same images may use the file at the same time,
 * and share it.
 *
 * Otherwise the file is overwritten: it is created if it is missing, given
 * the size of the scratch, and the scratch starts empty. A warning is
 * logged if the file held something else. Programs that use the file at
 * the same time must therefore hold the same images.
 *
 * The file is not removed when the scratch is destroyed.
 *
 * Each image file is opened twice through @p files, all of them at once on
 * @p tasks, which this waits for: once to compute the size and once to
 * construct the scratch. It must not be called on a thread of @p tasks.
 *
 * @param locations The images to hold, grouped by file.
 * @param files Provider used to open the image files.
 * @param tasks Where the image files are opened.
 * @param transfers The threads that carry out the transfers of the scratch
 * file.
 * @param path Path of the scratch file.
 * @param run_length Number of held indices per run.
 * @param max_size Maximum size of the file in bytes. By default there is no
 * maximum.
 * @return std::shared_ptr<image_scratch> The scratch. Never null.
 * @throws std::invalid_argument If @p run_length is zero, or if there is
 * nothing to hold: @p locations names no image, or @p max_size has no room
 * for one. No file is created then.
 * @throws std::out_of_range If a location has a stack index that its
 * file does not have.
 * @throws image_file_error If the scratch file can not be created, sized,
 * given its room or opened, or if an image file does not exist or can not
 * be read.
 * @throws unsupported_operation_error If no format can read an image file.
 * @throws image_file_format_error If an image file is malformed or truncated.
 */
VITRIO_API
std::shared_ptr<image_scratch> create_file_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	const file_transfer_pool &transfers,
	const std::string &path,
	std::size_t run_length,
	std::size_t max_size = std::numeric_limits<std::size_t>::max()
);

/**
 * @brief Create a scratch that stores its copies in a file of a directory
 * that stays within a budget.
 *
 * The directory holds the files of scratches, each named after the
 * fingerprint of its scratch, which @ref indexed_image_scratch describes.
 * The scratch of the same images, laid out the same way, is given the file
 * an earlier one left, and resumes from it as
 * @ref create_file_image_scratch does. Several programs may use the
 * directory at the same time, and those that hold the same images share
 * their file.
 *
 * A new file is made room for: the files of other scratches are removed,
 * the one used least recently first, until it fits in @p budget beside
 * those that remain. A file is used when a scratch is created over it. A
 * file that can not be removed, such as one that is open on Windows, keeps
 * its room, and the scratch then holds what fits beside it. One that is
 * removed while a scratch elsewhere has it open, as POSIX allows, is gone
 * from the directory, but its space on the disk is freed only once nothing
 * has it open. The budget may therefore be exceeded on the disk for as long
 * as such a scratch lives.
 *
 * The scratch holds every image of @p locations, unless that needs more
 * than the budget. It then holds the images that fit in the budget, as
 * @ref indexed_image_scratch describes, and every other file that can be
 * removed is removed to make room for them.
 *
 * The budget is not weighed against the free space of the disk, which
 * must have room for it. A file is given its room on the disk when it is
 * created, on the file systems that can reserve it, which fails at once on
 * a disk that is full. Elsewhere the room is taken as the scratch takes
 * images in, and a disk that is full by then fails the loads that find it
 * so.
 *
 * Only the files of scratches count against the budget, and only they are
 * ever removed. Each is named after its fingerprint, in hexadecimal, with
 * the extension `.vitrio-scratch`. The directory also holds
 * `vitrio-scratch.lock`, which programs take turns through.
 *
 * Each image file is opened twice through @p files, all of them at once on
 * @p tasks, which this waits for: once to compute the size, however often
 * files that can not be removed leave less room than was computed, and once
 * to construct the scratch. It must not be called on a thread of @p tasks.
 *
 * @param locations The images to hold, grouped by file.
 * @param files Provider used to open the image files.
 * @param tasks Where the image files are opened.
 * @param transfers The threads that carry out the transfers of the scratch
 * file.
 * @param directory Path of the directory. It is created if it is missing.
 * @param run_length Number of held indices per run.
 * @param budget The most bytes that the files of scratches in
 * @p directory may take together.
 * @return std::shared_ptr<image_scratch> The scratch. Never null.
 * @throws std::invalid_argument If @p run_length is zero, or if there is
 * nothing to hold: @p locations names no image, or the budget has no room
 * for one beside the files that can not be removed. No file is created
 * then.
 * @throws std::out_of_range If a location has a stack index that its
 * file does not have.
 * @throws image_file_error If the directory can not be created, locked or
 * listed, if the scratch file can not be created, sized, given its room or
 * opened, or if an image file does not exist or can not be read.
 * @throws unsupported_operation_error If no format can read an image file.
 * @throws image_file_format_error If an image file is malformed or truncated.
 */
VITRIO_API
std::shared_ptr<image_scratch> create_directory_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	const file_transfer_pool &transfers,
	const std::string &directory,
	std::size_t run_length,
	std::size_t budget
);

} // namespace vitrio
