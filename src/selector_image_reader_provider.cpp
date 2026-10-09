// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/selector_image_reader_provider.hpp>

#include <vitrio/image_read_format_selector.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

selector_image_reader_provider::selector_image_reader_provider(
	std::shared_ptr<const image_read_format_selector> formats
)
	: m_formats(std::move(formats))
{
	if (!m_formats)
	{
		throw std::invalid_argument(
			"selector_image_reader_provider: The format selector must not "
			"be null."
		);
	}
}

selector_image_reader_provider::~selector_image_reader_provider() = default;

std::shared_ptr<const image_reader>
selector_image_reader_provider::acquire(const std::string &key)
{
	return m_formats->open(key);
}

} // namespace vitrio
