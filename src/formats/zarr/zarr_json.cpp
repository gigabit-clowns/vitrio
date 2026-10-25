// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_json.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <boost/filesystem/operations.hpp>

#include <fstream>
#include <limits>

namespace vitrio
{
namespace zarr
{

nlohmann::json read_json_document(const std::string &path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		throw image_file_error(
			path + ": read_json_document: The file could not be opened."
		);
	}

	auto document = nlohmann::json::parse(input, nullptr, false);
	if (input.bad())
	{
		throw image_file_error(
			path + ": read_json_document: The file could not be read."
		);
	}
	if (document.is_discarded())
	{
		throw image_file_format_error(
			path + ": read_json_document: The file is not JSON."
		);
	}

	return document;
}

void write_json_document(
	const std::string &path,
	const nlohmann::json &document
)
{
	const auto text = document.dump(2) + "\n";
	const auto partial = path + ".partial";
	{
		std::ofstream output(
			partial.c_str(),
			std::ios::out | std::ios::binary | std::ios::trunc
		);
		output << text;
		output.flush();
		if (!output)
		{
			throw image_file_error(
				path + ": write_json_document: The file could not be "
				"written."
			);
		}
	}

	boost::system::error_code error;
	boost::filesystem::rename(partial, path, error);
	if (error)
	{
		boost::filesystem::remove(partial, error);
		throw image_file_error(
			path + ": write_json_document: The file could not be put in "
			"place: " + error.message()
		);
	}
}

const nlohmann::json& get_member(
	const nlohmann::json &object,
	const char *key,
	const std::string &where
)
{
	if (!object.is_object())
	{
		reject_metadata(where, "The metadata is not a JSON object.");
	}

	const auto found = object.find(key);
	if (found == object.end())
	{
		reject_metadata(
			where, std::string("The metadata has no \"") + key + "\".");
	}

	return *found;
}

std::vector<std::size_t> get_extents(
	const nlohmann::json &value,
	const char *name,
	const std::string &where
)
{
	if (!value.is_array())
	{
		reject_metadata(where, std::string("The ") + name + " is no list.");
	}

	std::vector<std::size_t> extents;
	extents.reserve(value.size());
	for (const auto &extent : value)
	{
		if (
			!extent.is_number_unsigned() ||
			extent.get<std::uint64_t>() >
				std::numeric_limits<std::size_t>::max()
		)
		{
			reject_metadata(
				where,
				std::string("The ") + name +
					" holds what is not a whole number of zero or more."
			);
		}
		extents.push_back(
			static_cast<std::size_t>(extent.get<std::uint64_t>()));
	}

	return extents;
}

void reject_metadata(const std::string &where, const std::string &reason)
{
	throw image_file_format_error(where + ": " + reason);
}

void refuse_metadata(const std::string &where, const std::string &reason)
{
	throw unsupported_operation_error(where + ": " + reason);
}

} // namespace zarr
} // namespace vitrio
