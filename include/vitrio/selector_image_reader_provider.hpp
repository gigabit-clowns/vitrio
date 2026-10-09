// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_read_format_selector;

/**
 * @brief A provider that opens each file through a format selector, every
 * time it is asked for one.
 *
 * It keeps nothing: every reader it returns is newly opened through the
 * format selector it was constructed with, so a path asked for twice is
 * opened twice and the two readers are unrelated. With no capacity to size
 * and no eviction, a file is opened exactly as often as it is asked for.
 *
 * @see image_read_format_selector
 */
class VITRIO_API selector_image_reader_provider final
	: public image_reader_provider
{
public:
	/**
	 * @brief Construct a provider opening through a format selector.
	 *
	 * @param formats The formats a file may be opened with.
	 * @throws std::invalid_argument If @p formats is null.
	 */
	explicit selector_image_reader_provider(
		std::shared_ptr<const image_read_format_selector> formats
	);

	~selector_image_reader_provider() override;

	std::shared_ptr<const image_reader>
	acquire(const std::string &path) override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<const image_read_format_selector> m_formats;
};

} // namespace vitrio
