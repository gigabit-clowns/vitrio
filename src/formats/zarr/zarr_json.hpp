// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Read a JSON document, as ZARR stores its metadata.
 *
 * @param path The file.
 * @return nlohmann::json The document.
 * @throws image_file_error If the file can not be read.
 * @throws image_file_format_error If it is not JSON.
 */
nlohmann::json read_json_document(const std::string &path);

/**
 * @brief Write a JSON document, as ZARR stores its metadata, replacing the
 * one at its path whole or not at all.
 *
 * It is written beside its path and renamed over it, so a reader never sees
 * half of it.
 *
 * @param path The file.
 * @param document The document.
 * @throws image_file_error If the file can not be written.
 */
void write_json_document(
	const std::string &path,
	const nlohmann::json &document
);

/**
 * @brief Get a member a JSON object must have.
 *
 * @param object The object.
 * @param key The name of the member.
 * @param where What a message starts with: the path, then what reads it.
 * @return const nlohmann::json& The member.
 * @throws image_file_format_error If @p object is not an object, or lacks
 * the member.
 */
const nlohmann::json& get_member(
	const nlohmann::json &object,
	const char *key,
	const std::string &where
);

/**
 * @brief Read the extents a JSON array lists, as a shape or the shape of a
 * chunk does.
 *
 * @param value The array.
 * @param name What the extents are, as a message names them.
 * @param where What a message starts with.
 * @return std::vector<std::size_t> The extents.
 * @throws image_file_format_error If @p value is not an array of whole
 * numbers none of which is negative.
 */
std::vector<std::size_t> get_extents(
	const nlohmann::json &value,
	const char *name,
	const std::string &where
);

/**
 * @brief Report metadata that is not what ZARR states.
 *
 * @param where What the message starts with.
 * @param reason The sentence that follows.
 * @throws image_file_format_error Always.
 */
[[noreturn]]
void reject_metadata(const std::string &where, const std::string &reason);

/**
 * @brief Report metadata that ZARR allows but vitrio does not read.
 *
 * @param where What the message starts with.
 * @param reason The sentence that follows.
 * @throws unsupported_operation_error Always.
 */
[[noreturn]]
void refuse_metadata(const std::string &where, const std::string &reason);

} // namespace zarr
} // namespace vitrio
