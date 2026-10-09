// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_read_format_registry.hpp>

#include <vitrio/image_read_format.hpp>
#include <vitrio/image_read_format_selector.hpp>

namespace vitrio
{

image_read_format_registry::image_read_format_registry() = default;
image_read_format_registry::~image_read_format_registry() = default;

void image_read_format_registry::add(image_read_format_factory factory)
{
	if (factory)
	{
		m_factories.push_back(factory);
	}
}

void image_read_format_registry::register_all(
	image_read_format_selector &selector
) const
{
	for (const auto factory : m_factories)
	{
		selector.register_format(factory());
	}
}

} // namespace vitrio
