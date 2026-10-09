// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_write_format_selector.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_write_format.hpp>
#include <vitrio/image_writer.hpp>

#include <assert.hpp>
#include <builtin_image_format_registry.hpp>
#include <find_most_suitable_format.hpp>

#include <mutex>
#include <utility>
#include <vector>

namespace vitrio
{

class image_write_format_selector::implementation
{
public:
	void register_format(std::unique_ptr<image_write_format> format)
	{
		VITRIO_ASSERT(format);

		const std::lock_guard<std::mutex> lock(m_mutex);
		m_formats.push_back(std::move(format));
	}

	// The lock is held while the formats are asked and no longer. A format
	// is never removed, so the one found stays valid after it.
	const image_write_format*
	get_most_suitable_format(const image_probe &probe) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

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
				probe.get_path() + ": image_write_format_selector::open: No "
				"registered format can create the file."
			);
		}

		return format->open(probe, descriptor, metadata);
	}

private:
	mutable std::mutex m_mutex;
	std::vector<std::unique_ptr<image_write_format>> m_formats;
};

image_write_format_selector::image_write_format_selector()
	: m_implementation(std::make_unique<implementation>())
{
}

image_write_format_selector::~image_write_format_selector() = default;

const std::shared_ptr<image_write_format_selector>&
image_write_format_selector::get_shared()
{
	static const auto instance = [] ()
	{
		auto result = std::make_shared<image_write_format_selector>();
		result->register_builtin_formats();

		return result;
	}();

	return instance;
}

void image_write_format_selector::register_builtin_formats()
{
	get_builtin_image_write_format_registry().register_all(*this);
}

bool image_write_format_selector::register_format(
	std::unique_ptr<image_write_format> format
)
{
	if (!format)
	{
		return false;
	}

	m_implementation->register_format(std::move(format));

	return true;
}

std::shared_ptr<image_writer> image_write_format_selector::open(
	const std::string &path,
	const image_descriptor &descriptor,
	const image_metadata &metadata
) const
{
	return m_implementation->open(image_probe(path), descriptor, metadata);
}

const image_write_format*
image_write_format_selector::get_most_suitable_format(
	const image_probe &probe
) const
{
	return m_implementation->get_most_suitable_format(probe);
}

} // namespace vitrio
