// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_writer_provider.hpp>
#include <vitrio/platform.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

class image_descriptor;
class image_metadata;
class image_file_write_format_selector;

/**
 * @brief A provider that writes files. Its files are explicitly declared and
 * closed by the client.
 *
 * Creating a file replaces whatever was there, so a writer dropped and
 * reopened would truncate everything already written to it, and no eviction
 * policy can decide when that is safe. Which files exist and when each is
 * finished is instead said outright, through @ref declare and @ref close.
 *
 * Each file therefore has one lifecycle, **declare, acquire as often as
 * needed, close**:
 * - A declared file is created, through the format selector, the first time
 *   it is acquired, so declaring many files costs no file descriptor until
 *   each is reached. Every later acquire returns that same writer until the
 *   file is closed, and a key never declared is refused with
 *   @c std::out_of_range.
 * - @ref close flushes and drops the writer, which is what gives its file
 *   descriptor back.
 * - @ref flush flushes every writer created so far, and creates none: a
 *   declared file never acquired stays uncreated.
 *
 * @par Thread safety
 * Every method may be called concurrently. The lock is held while a file is
 * created, since two concurrent calls creating one file would otherwise each
 * replace it, and only long enough to look it up when it is already open.
 *
 * @see file_image_reader_provider
 * @see image_file_write_format_selector
 */
class VITRIO_API file_image_writer_provider final
	: public image_writer_provider
{
public:
	/**
	 * @brief Construct a provider creating files through a format selector.
	 *
	 * @param formats The formats a file may be created with.
	 * @throws std::invalid_argument If @p formats is null.
	 */
	explicit file_image_writer_provider(
		std::shared_ptr<const image_file_write_format_selector> formats
	);

	~file_image_writer_provider() override;

	/**
	 * @brief Make a file writable.
	 *
	 * Records the descriptor and metadata the file will be created with.
	 * Nothing is created until the file is first acquired, so this costs no
	 * file descriptor and can not fail on anything the file system has to
	 * say.
	 *
	 * A key that is still declared is refused rather than replaced,
	 * because replacing it would silently strand whatever had been written
	 * to the file it names. @ref close it first, which says so.
	 *
	 * @param key Key of the file to create, which is handed to the format
	 * selector as its path.
	 * @param descriptor What the file holds.
	 * @param metadata How its samples map onto physical space.
	 * @throws std::logic_error If that key is already declared.
	 */
	void declare(
		std::string key,
		image_descriptor descriptor,
		const image_metadata &metadata
	);

	/**
	 * @brief Finish a file.
	 *
	 * Flushes the writer if the file was ever acquired, drops it, and
	 * forgets the declaration. Acquiring that key afterwards throws as an
	 * undeclared one does, and declaring it again creates the file afresh.
	 *
	 * Closing a file that was declared but never acquired creates nothing
	 * and simply forgets it.
	 *
	 * @param key Key of the file to finish.
	 * @throws std::out_of_range If that key is not declared.
	 * @throws image_file_error If the pending writes could not be
	 * completed.
	 */
	void close(const std::string &key);

	/**
	 * @brief Make everything written to the files reach the storage.
	 *
	 * Flushes the writer of every file acquired so far and creates none: a
	 * file declared and never acquired stays uncreated. Every file stays
	 * declared and writable.
	 *
	 * @throws image_file_error If the pending writes could not be
	 * completed.
	 */
	void flush();

	/**
	 * @brief Get how many files are declared.
	 *
	 * Counts what has been declared and not yet closed, whether or not it
	 * has been acquired.
	 *
	 * @return std::size_t The number of files.
	 */
	std::size_t get_file_count() const noexcept;

	std::shared_ptr<image_writer> acquire(const std::string &key) override;

private:
	class implementation;
	VITRIO_STD_MEMBER_INTERFACE
	std::unique_ptr<implementation> m_implementation;
};

} // namespace vitrio
