// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_scratch_directory.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/mapped_file_image_scratch_storage.hpp>

// The whole of Boost.Filesystem, for directory_iterator: its own header is
// only there from Boost 1.72, and operations.hpp has stopped including it.
#include <boost/filesystem.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

const char *const file_extension = ".vitrio-scratch";
const char *const lock_name = "vitrio-scratch.lock";

// A lock file excludes other processes, but on POSIX not the other threads
// of this one, and closing any handle of it in this process drops the lock
// of every other. So the threads of this process take turns first.
std::mutex &get_process_mutex()
{
	static std::mutex mutex;
	return mutex;
}

class directory_lock
{
public:
	explicit directory_lock(const boost::filesystem::path &directory)
		: m_process_lock(get_process_mutex())
	{
		const auto path = (directory / lock_name).string();

		{
			// Opening a file to append creates it if it is missing, and
			// leaves it as it is otherwise.
			std::filebuf file;
			file.open(path, std::ios_base::out | std::ios_base::app);
		}

		try
		{
			m_file = boost::interprocess::file_lock(path.c_str());
			m_file_lock = file_lock_type(m_file);
		}
		catch (const boost::interprocess::interprocess_exception &error)
		{
			throw image_file_error(
				path + ": image_scratch_directory: The directory could not "
				"be locked: " + error.what()
			);
		}
	}

private:
	using file_lock_type =
		boost::interprocess::scoped_lock<boost::interprocess::file_lock>;

	std::lock_guard<std::mutex> m_process_lock;
	boost::interprocess::file_lock m_file;
	file_lock_type m_file_lock;
};

struct scratch_file
{
	boost::filesystem::path path;
	std::time_t last_used;
	std::size_t size;
};

// The files of the directory that hold a scratch, other than one.
std::vector<scratch_file> list_files(
	const boost::filesystem::path &directory,
	const boost::filesystem::path &excluded
)
{
	boost::system::error_code error;
	boost::filesystem::directory_iterator ite(directory, error);
	if (error)
	{
		throw image_file_error(
			directory.string() + ": image_scratch_directory: The directory "
			"could not be listed: " + error.message()
		);
	}

	std::vector<scratch_file> files;
	for (; ite != boost::filesystem::directory_iterator(); ite.increment(error))
	{
		if (error)
		{
			throw image_file_error(
				directory.string() + ": image_scratch_directory: The "
				"directory could not be listed: " + error.message()
			);
		}

		const auto &path = ite->path();
		if (path.extension() != file_extension || path == excluded)
		{
			continue;
		}

		// A file that can not be told about is passed over, since there is
		// nothing to weigh it by.
		const auto size = boost::filesystem::file_size(path, error);
		if (error)
		{
			continue;
		}

		const auto last_used = boost::filesystem::last_write_time(path, error);
		if (error)
		{
			continue;
		}

		files.push_back(
			scratch_file{path, last_used, static_cast<std::size_t>(size)}
		);
	}

	std::sort(
		files.begin(),
		files.end(),
		[] (const scratch_file &lhs, const scratch_file &rhs)
		{
			return std::tie(lhs.last_used, lhs.path) <
				std::tie(rhs.last_used, rhs.path);
		}
	);

	return files;
}

std::size_t get_total_size(const std::vector<scratch_file> &files) noexcept
{
	std::size_t total = 0;
	for (const auto &file : files)
	{
		total += file.size;
	}

	return total;
}

// Removes files, least recently used first, until a size fits beside the
// rest, and gives how much room the rest leaves.
std::size_t make_room(
	std::vector<scratch_file> files,
	std::size_t size,
	std::size_t budget
)
{
	auto kept = get_total_size(files);
	for (const auto &file : files)
	{
		if (kept <= budget && size <= budget - kept)
		{
			break;
		}

		boost::system::error_code error;
		if (boost::filesystem::remove(file.path, error) && !error)
		{
			kept -= file.size;
		}
	}

	return kept < budget ? budget - kept : 0;
}

} // anonymous namespace

image_scratch_directory::image_scratch_directory(
	std::string path,
	std::size_t budget
)
	: m_path(std::move(path))
	, m_budget(budget)
{
	boost::system::error_code error;
	boost::filesystem::create_directories(m_path, error);
	if (error || !boost::filesystem::is_directory(m_path, error))
	{
		throw image_file_error(
			m_path + ": image_scratch_directory: The directory could not be "
			"created."
		);
	}
}

const std::string& image_scratch_directory::get_path() const noexcept
{
	return m_path;
}

std::size_t image_scratch_directory::get_budget() const noexcept
{
	return m_budget;
}

std::string
image_scratch_directory::get_file_path(std::uint64_t fingerprint) const
{
	char name[17];
	std::snprintf(
		name,
		sizeof(name),
		"%016llx",
		static_cast<unsigned long long>(fingerprint)
	);

	const auto file_name = name + std::string(file_extension);
	return (boost::filesystem::path(m_path) / file_name).string();
}

std::shared_ptr<image_scratch_storage> image_scratch_directory::acquire(
	std::uint64_t fingerprint,
	std::size_t size,
	std::size_t &room
)
{
	const boost::filesystem::path directory(m_path);
	const auto path = get_file_path(fingerprint);

	const directory_lock lock(directory);

	room = make_room(list_files(directory, path), size, m_budget);
	if (size > room)
	{
		return nullptr;
	}

	auto storage = create_mapped_file_image_scratch_storage(path, size);

	boost::system::error_code error;
	boost::filesystem::last_write_time(path, std::time(nullptr), error);

	return storage;
}

} // namespace vitrio
