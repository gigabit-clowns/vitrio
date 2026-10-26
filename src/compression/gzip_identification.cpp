// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gzip_identification.hpp"
#include "zlib_inflater.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <algorithm>
#include <cctype>

namespace vitrio
{

namespace
{

bool is_separator(char character) noexcept
{
#if defined(_WIN32)
	return character == '/' || character == '\\';
#else
	return character == '/';
#endif
}

std::string to_lowercase(std::string text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[] (char character)
		{
			return static_cast<char>(
				std::tolower(static_cast<unsigned char>(character))
			);
		}
	);

	return text;
}

} // anonymous namespace

bool has_gzip_signature(span<const byte> bytes) noexcept
{
	return
		bytes.size() >= 3 &&
		bytes.data()[0] == 0x1f &&
		bytes.data()[1] == 0x8b &&
		bytes.data()[2] == 0x08;
}

std::string get_gzip_inner_extension(const std::string &path)
{
	const auto separator = std::find_if(
		path.rbegin(),
		path.rend(),
		is_separator
	);
	const auto name = to_lowercase(std::string(separator.base(), path.end()));

	const std::string suffix = ".gz";
	if (
		name.size() <= suffix.size() ||
		name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0
	)
	{
		return std::string();
	}

	// A name whose only other dot starts it, such as ".map.gz", is that of
	// a hidden file, and the dot starts no extension.
	const auto stem = name.substr(0, name.size() - suffix.size());
	const auto dot = stem.rfind('.');
	if (dot == std::string::npos || dot == 0)
	{
		return std::string();
	}

	return stem.substr(dot);
}

std::vector<byte>
inflate_leading_bytes(span<const byte> bytes, std::size_t limit)
{
	if (!has_gzip_signature(bytes))
	{
		return std::vector<byte>();
	}

	std::vector<byte> inflated(limit);
	auto input = bytes;
	auto output = make_span(inflated.data(), inflated.size());
	try
	{
		zlib_inflater inflater(31, true, "inflate_leading_bytes");
		inflater.inflate(input, output);
	}
	catch (const image_file_format_error &)
	{
		// What inflated before the stream broke off is kept.
	}

	inflated.resize(limit - output.size());
	return inflated;
}

} // namespace vitrio
