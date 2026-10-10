// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/tests/assets.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <ctime>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief A directory under the build tree that is removed with everything
 * in it when a case leaves it, whether it succeeded or not.
 *
 * It is not created, and it is removed first if an earlier run left it. Its
 * name must not repeat across the suite, as that of a @ref scoped_path.
 */
class scoped_directory
{
public:
	explicit scoped_directory(const std::string &name)
		: m_path(get_scratch_path(name))
	{
		boost::system::error_code error;
		boost::filesystem::remove_all(m_path, error);
	}

	scoped_directory(const scoped_directory &other) = delete;
	scoped_directory(scoped_directory &&other) = delete;

	~scoped_directory()
	{
		boost::system::error_code error;
		boost::filesystem::remove_all(m_path, error);
	}

	scoped_directory& operator=(const scoped_directory &other) = delete;
	scoped_directory& operator=(scoped_directory &&other) = delete;

	const std::string& get() const noexcept
	{
		return m_path;
	}

	// The path of a file in the directory.
	std::string get(const std::string &name) const
	{
		return (boost::filesystem::path(m_path) / name).string();
	}

	// The paths of the files in the directory that have an extension.
	std::vector<std::string> list_files(const std::string &extension) const
	{
		std::vector<std::string> files;
		boost::filesystem::directory_iterator ite(m_path);
		for (; ite != boost::filesystem::directory_iterator(); ++ite)
		{
			if (ite->path().extension() == extension)
			{
				files.push_back(ite->path().string());
			}
		}

		return files;
	}

private:
	std::string m_path;
};

/**
 * @brief Make a file look as if it was last written some seconds ago.
 *
 * @param path The file.
 * @param seconds How long ago.
 */
inline void age_file(const std::string &path, std::time_t seconds)
{
	boost::filesystem::last_write_time(path, std::time(nullptr) - seconds);
}

} // namespace vitrio
