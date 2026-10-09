// SPDX-License-Identifier: LGPL-2.1-or-later

#include "index_tuple.hpp"

namespace vitrio
{

namespace nb = nanobind;

index_tuple to_tuple(span<const std::size_t> values)
{
	nb::list items;
	for (const auto value : values)
	{
		items.append(value);
	}

	return index_tuple(nb::tuple(items));
}

} // namespace vitrio
