// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_write_format.hpp>

#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_writer.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_write_format final
	: public image_write_format
{
public:
	mock_image_write_format() = default;

	MAKE_CONST_MOCK0(get_name, std::string(), override);

	MAKE_CONST_MOCK1(
		get_suitability,
		image_format_suitability(const image_probe &probe),
		override
	);

	MAKE_CONST_MOCK3(
		open,
		std::shared_ptr<image_writer>(
			const image_probe &probe,
			const image_descriptor &descriptor,
			const image_metadata &metadata
		),
		override
	);
};

} // namespace vitrio
