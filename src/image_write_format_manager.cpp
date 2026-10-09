// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_write_format_manager.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <assert.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_write_format.hpp>
#include <vitrio/image_writer.hpp>

#include <find_most_suitable_format.hpp>
#include <builtin_image_format_registry.hpp>

#include <utility>
#include <vector>

namespace vitrio
{

class image_write_format_manager::implementation
{
public:
	bool register_format(std::unique_ptr<image_write_format> format)
	{
		VITRIO_ASSERT(format);
		m_formats.push_back(std::move(format));
		return true;
	}

	const image_write_format*
	get_most_suitable_format(const image_probe &probe) const
	{
		const auto ite = find_most_suitable_format(
			m_formats.begin(),
			m_formats.end(),
			[&probe] (const auto &item)
			{
				return item->get_suitability(probe);
			}
		);

		if (ite == m_formats.cend())
		{
			return nullptr;
		}

		return ite->get();
	}

	std::shared_ptr<image_writer> open(
		const image_probe &probe,
		const image_descriptor &descriptor,
		const image_metadata &metadata
	) const
	{
		const auto *format = get_most_suitable_format(probe);
		if (!format)
		{
			throw unsupported_operation_error(
				probe.get_path() + ": image_write_format_manager::open: No "
				"registered format can create the file."
			);
		}

		return format->open(probe, descriptor, metadata);
	}

private:
	std::vector<std::unique_ptr<image_write_format>> m_formats;

};

image_write_format_manager::image_write_format_manager() noexcept = default;
image_write_format_manager::~image_write_format_manager() = default;

void image_write_format_manager::register_builtin_formats()
{
	get_builtin_image_write_format_registry().register_all(*this);
}

bool image_write_format_manager::register_format(
	std::unique_ptr<image_write_format> format
)
{
	if (!format)
	{
		return false;
	}

	return create_if_null().register_format(std::move(format));
}

std::shared_ptr<image_writer> image_write_format_manager::open(
	const std::string &path,
	const image_descriptor &descriptor,
	const image_metadata &metadata
) const
{
	return get_implementation().open(image_probe(path), descriptor, metadata);
}

const image_write_format*
image_write_format_manager::get_most_suitable_format(
	const image_probe &probe
) const
{
	return get_implementation().get_most_suitable_format(probe);
}

image_write_format_manager::implementation&
image_write_format_manager::create_if_null()
{
	if (!m_implementation)
	{
		m_implementation = std::make_unique<implementation>();
	}

	return *m_implementation;
}

const image_write_format_manager::implementation&
image_write_format_manager::get_implementation() const noexcept
{
	static const implementation empty_implementation;
	return m_implementation ? *m_implementation : empty_implementation;
}

} // namespace vitrio
