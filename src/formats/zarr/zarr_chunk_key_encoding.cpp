// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_chunk_key_encoding.hpp"

#include "zarr_json.hpp"

#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

char parse_separator(
	const nlohmann::json &value,
	char fallback,
	const std::string &where
)
{
	if (value.is_null())
	{
		return fallback;
	}
	if (value == ".")
	{
		return '.';
	}
	if (value == "/")
	{
		return '/';
	}

	reject_metadata(where, "The separator of chunk keys is neither . nor /.");
}

const nlohmann::json& find_or_null(
	const nlohmann::json &object,
	const char *key
)
{
	static const nlohmann::json null;
	if (!object.is_object())
	{
		return null;
	}

	const auto found = object.find(key);
	return found == object.end() ? null : *found;
}

} // anonymous namespace

zarr_chunk_key_encoding::zarr_chunk_key_encoding(
	std::string prefix,
	char separator
)
	: m_prefix(std::move(prefix))
	, m_separator(separator)
{
}

std::string
zarr_chunk_key_encoding::get_key(span<const std::size_t> chunk) const
{
	if (chunk.empty())
	{
		return m_prefix.empty() ? std::string("0") : m_prefix;
	}

	auto key = m_prefix;
	for (std::size_t axis = 0; axis < chunk.size(); ++axis)
	{
		if (!key.empty())
		{
			key += m_separator;
		}
		key += std::to_string(chunk[axis]);
	}

	return key;
}

zarr_chunk_key_encoding parse_v2_chunk_key_encoding(
	const nlohmann::json &metadata,
	const std::string &where
)
{
	return zarr_chunk_key_encoding(
		std::string(),
		parse_separator(
			find_or_null(metadata, "dimension_separator"), '.', where)
	);
}

zarr_chunk_key_encoding parse_v3_chunk_key_encoding(
	const nlohmann::json &encoding,
	const std::string &where
)
{
	const auto &name = get_member(encoding, "name", where);
	const auto &separator = find_or_null(
		find_or_null(encoding, "configuration"), "separator");

	if (name == "default")
	{
		return zarr_chunk_key_encoding(
			"c", parse_separator(separator, '/', where));
	}
	if (name == "v2")
	{
		return zarr_chunk_key_encoding(
			std::string(), parse_separator(separator, '.', where));
	}

	reject_metadata(where, "The chunk key encoding is neither default nor v2.");
}

} // namespace zarr
} // namespace vitrio
