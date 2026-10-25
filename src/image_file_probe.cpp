// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_file_probe.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>

#include <boost/filesystem.hpp>

namespace vitrio
{

// A constant of a class needs a definition in C++14 to be bound to a
// reference. From C++17 on its declaration is one.
constexpr std::size_t image_file_probe::max_leading_bytes;

namespace
{

bool is_separator(char character)
{
#if defined(_WIN32)
	return character == '/' || character == '\\';
#else
	return character == '/';
#endif
}

// The path without the separators that end it, which name no entry of their
// own: the name of "volume.zarr/" is "volume.zarr". A path of separators
// alone, the root, is kept as it is.
std::string strip_trailing_separators(const std::string &path)
{
	auto end = path.size();
	while (end > 1 && is_separator(path[end - 1]))
	{
		--end;
	}

	return path.substr(0, end);
}

std::string get_lowercase_extension(const std::string &path)
{
	std::string extension;

	try
	{
		extension = boost::filesystem::path(
			strip_trailing_separators(path)
		).extension().string();
	}
	catch (const boost::filesystem::filesystem_error &)
	{
		return std::string();
	}

	std::transform(
		extension.begin(),
		extension.end(),
		extension.begin(),
		[] (char character)
		{
			return static_cast<char>(
				std::tolower(static_cast<unsigned char>(character))
			);
		}
	);

	return extension;
}

bool names_directory(const std::string &path)
{
	boost::system::error_code error;
	return boost::filesystem::is_directory(path, error);
}

bool read_leading_bytes(
	const std::string &path,
	std::vector<byte> &bytes
)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		return false;
	}

	bytes.resize(image_file_probe::max_leading_bytes);
	input.read(
		reinterpret_cast<char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	bytes.resize(static_cast<std::size_t>(input.gcount()));

	return true;
}

} // anonymous namespace

image_file_probe::image_file_probe(std::string path)
	: m_path(std::move(path))
	, m_extension(get_lowercase_extension(m_path))
	, m_exists(false)
	, m_is_directory(names_directory(m_path))
{
	m_exists = m_is_directory || read_leading_bytes(m_path, m_leading_bytes);
}

image_file_probe::image_file_probe(const image_file_probe &other) = default;
image_file_probe::image_file_probe(image_file_probe &&other) noexcept = default;
image_file_probe::~image_file_probe() = default;

image_file_probe&
image_file_probe::operator=(const image_file_probe &other) = default;
image_file_probe&
image_file_probe::operator=(image_file_probe &&other) noexcept = default;

const std::string& image_file_probe::get_path() const noexcept
{
	return m_path;
}

const std::string& image_file_probe::get_extension() const noexcept
{
	return m_extension;
}

span<const byte> image_file_probe::get_leading_bytes() const noexcept
{
	return span<const byte>(m_leading_bytes.data(), m_leading_bytes.size());
}

bool image_file_probe::exists() const noexcept
{
	return m_exists;
}

bool image_file_probe::is_directory() const noexcept
{
	return m_is_directory;
}

} // namespace vitrio
