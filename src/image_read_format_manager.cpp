// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_read_format_manager.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <assert.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_read_format.hpp>
#include <vitrio/image_reader.hpp>

#include <find_most_suitable_format.hpp>
#include <builtin_image_format_registry.hpp>

#include <utility>
#include <vector>

namespace vitrio
{

class image_read_format_manager::implementation
{
public:
	bool register_format(std::unique_ptr<image_read_format> format)
	{
		VITRIO_ASSERT(format);
		m_formats.push_back(std::move(format));
		return true;
	}

	const image_read_format*
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

	std::shared_ptr<image_reader> open(const image_probe &probe) const
	{
		const auto *format = get_most_suitable_format(probe);
		if (!format)
		{
			if (!probe.exists())
			{
				throw image_file_error(
					probe.get_path() + ": image_read_format_manager::open: No "
					"registered format claims the path, and no readable file "
					"exists there."
				);
			}

			throw unsupported_operation_error(
				probe.get_path() + ": image_read_format_manager::open: No "
				"registered format can read the file."
			);
		}

		return format->open(probe);
	}

private:
	std::vector<std::unique_ptr<image_read_format>> m_formats;

};

image_read_format_manager::image_read_format_manager() noexcept = default;
image_read_format_manager::~image_read_format_manager() = default;

void image_read_format_manager::register_builtin_formats()
{
	get_builtin_image_read_format_registry().register_all(*this);
}

bool image_read_format_manager::register_format(
	std::unique_ptr<image_read_format> format
)
{
	if (!format)
	{
		return false;
	}

	return create_if_null().register_format(std::move(format));
}

std::shared_ptr<image_reader>
image_read_format_manager::open(const std::string &path) const
{
	return get_implementation().open(image_probe(path));
}

const image_read_format*
image_read_format_manager::get_most_suitable_format(
	const image_probe &probe
) const
{
	return get_implementation().get_most_suitable_format(probe);
}

image_read_format_manager::implementation&
image_read_format_manager::create_if_null()
{
	if (!m_implementation)
	{
		m_implementation = std::make_unique<implementation>();
	}

	return *m_implementation;
}

const image_read_format_manager::implementation&
image_read_format_manager::get_implementation() const noexcept
{
	static const implementation empty_implementation;
	return m_implementation ? *m_implementation : empty_implementation;
}

} // namespace vitrio
