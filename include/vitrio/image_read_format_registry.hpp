// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <memory>
#include <vector>

namespace vitrio
{

class image_read_format;
class image_read_format_manager;

/**
 * @brief Factory function that creates a fresh image_read_format instance.
 */
using image_read_format_factory =
	std::unique_ptr<image_read_format> (*)();

/**
 * @brief Collects image read format factories for bulk registration.
 *
 * Registers one fresh format per factory into a manager, as many times and
 * into as many managers as asked.
 *
 * @note @ref add must not run concurrently with any other call, while
 * @ref register_all may run concurrently with itself into different
 * managers.
 *
 * @see image_write_format_registry
 */
class image_read_format_registry
{
public:
	using format_type = image_read_format;

	VITRIO_API image_read_format_registry();
	image_read_format_registry(
		const image_read_format_registry &other
	) = delete;
	image_read_format_registry(image_read_format_registry &&other) = delete;
	VITRIO_API ~image_read_format_registry();

	image_read_format_registry&
	operator=(const image_read_format_registry &other) = delete;
	image_read_format_registry&
	operator=(image_read_format_registry &&other) = delete;

	/**
	 * @brief Append a format factory to the registry.
	 *
	 * @param factory Function creating a fresh format instance. Null
	 * factories are ignored.
	 */
	VITRIO_API
	void add(image_read_format_factory factory);

	/**
	 * @brief Instantiate one format per registered factory and register them
	 * into a manager.
	 *
	 * @param manager The manager where the formats are registered.
	 */
	VITRIO_API
	void register_all(image_read_format_manager &manager) const;

private:
	std::vector<image_read_format_factory> m_factories;
};

} // namespace vitrio
