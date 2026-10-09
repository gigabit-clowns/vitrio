// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <memory>
#include <vector>

namespace vitrio
{

class image_write_format;
class image_write_format_selector;

/**
 * @brief Factory function that creates a fresh image_write_format instance.
 */
using image_write_format_factory =
	std::unique_ptr<image_write_format> (*)();

/**
 * @brief Collects image write format factories for bulk registration.
 *
 * Registers one fresh format per factory into a selector, as many times and
 * into as many selectors as asked.
 *
 * @note @ref add must not run concurrently with any other call, while
 * @ref register_all may run concurrently with itself into different
 * selectors.
 *
 * @see image_read_format_registry
 */
class image_write_format_registry
{
public:
	using format_type = image_write_format;

	VITRIO_API image_write_format_registry();
	image_write_format_registry(
		const image_write_format_registry &other
	) = delete;
	image_write_format_registry(image_write_format_registry &&other) = delete;
	VITRIO_API ~image_write_format_registry();

	image_write_format_registry&
	operator=(const image_write_format_registry &other) = delete;
	image_write_format_registry&
	operator=(image_write_format_registry &&other) = delete;

	/**
	 * @brief Append a format factory to the registry.
	 *
	 * @param factory Function creating a fresh format instance. Null
	 * factories are ignored.
	 */
	VITRIO_API
	void add(image_write_format_factory factory);

	/**
	 * @brief Instantiate one format per registered factory and register them
	 * into a selector.
	 *
	 * @param selector The selector where the formats are registered.
	 */
	VITRIO_API
	void register_all(image_write_format_selector &selector) const;

private:
	std::vector<image_write_format_factory> m_factories;
};

} // namespace vitrio
