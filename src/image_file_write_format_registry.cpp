// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_file_write_format_registry.hpp>

#include <vitrio/image_file_write_format.hpp>
#include <vitrio/image_file_write_format_selector.hpp>

namespace vitrio
{

image_file_write_format_registry::image_file_write_format_registry() = default;
image_file_write_format_registry::~image_file_write_format_registry() = default;

void image_file_write_format_registry::add(
	image_file_write_format_factory factory
)
{
	if (factory)
	{
		m_factories.push_back(factory);
	}
}

void image_file_write_format_registry::register_all(
	image_file_write_format_selector &selector
) const
{
	for (const auto factory : m_factories)
	{
		selector.register_format(factory());
	}
}

} // namespace vitrio
