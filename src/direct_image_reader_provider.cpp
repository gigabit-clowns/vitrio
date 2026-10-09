// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/direct_image_reader_provider.hpp>

#include <vitrio/image_read_format_manager.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

direct_image_reader_provider::direct_image_reader_provider(
	std::shared_ptr<const image_read_format_manager> formats
)
	: m_formats(std::move(formats))
{
	if (!m_formats)
	{
		throw std::invalid_argument(
			"direct_image_reader_provider: The format manager must not be "
			"null."
		);
	}
}

direct_image_reader_provider::~direct_image_reader_provider() = default;

std::shared_ptr<const image_reader>
direct_image_reader_provider::acquire(const std::string &path)
{
	return m_formats->open(path);
}

} // namespace vitrio
