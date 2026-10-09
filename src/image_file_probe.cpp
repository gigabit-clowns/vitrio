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

std::string get_lowercase_extension(const std::string &path)
{
	std::string extension;

	try
	{
		extension = boost::filesystem::path(path).extension().string();
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
{
	m_exists = read_leading_bytes(m_path, m_leading_bytes);
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

} // namespace vitrio
