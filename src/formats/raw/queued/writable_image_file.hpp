// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_handle.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <string>

namespace vitrio
{

/**
 * @brief A file open for writing, which a writer and the transfers it has
 * under way share, and which is closed once the last of them lets it go.
 *
 * Closing it does not wait for what was written to reach the storage: a
 * sync of it is a transfer of its own.
 */
class writable_image_file
{
public:
	/**
	 * @brief Open a file for writing.
	 *
	 * @param path Path to the file, which exists.
	 * @throws image_file_error If the file can not be opened.
	 */
	explicit writable_image_file(std::string path);

	writable_image_file(const writable_image_file &other) = delete;
	writable_image_file(writable_image_file &&other) = delete;

	~writable_image_file();

	writable_image_file& operator=(const writable_image_file &other) = delete;
	writable_image_file& operator=(writable_image_file &&other) = delete;

	/**
	 * @brief Get the path the file was opened at.
	 *
	 * @return const std::string& The path.
	 */
	const std::string& get_path() const noexcept;

	/**
	 * @brief Get the file, as the system names it.
	 *
	 * @return file_handle_t The file, open for writing.
	 */
	file_handle_t get_file() const noexcept;

	/**
	 * @brief Get the file as Boost keeps it open, which a read of it shares
	 * through a pointer that keeps this alive.
	 *
	 * @return const boost::interprocess::file_mapping& The open file.
	 */
	const boost::interprocess::file_mapping& get_open_file() const noexcept;

private:
	std::string m_path;

	// Boost opens a file for mapping on every system without mapping it,
	// which is all a writer that queues positioned writes of it needs.
	boost::interprocess::file_mapping m_file;
};

} // namespace vitrio
