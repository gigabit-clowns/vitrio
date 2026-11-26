// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_file_read_format_selector.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format.hpp>
#include <vitrio/image_reader.hpp>

#include <assert.hpp>
#include <builtin_image_file_format_registry.hpp>
#include <find_most_suitable_format.hpp>

#include <mutex>
#include <utility>
#include <vector>

namespace vitrio
{

class image_file_read_format_selector::implementation
{
public:
	void register_format(std::unique_ptr<image_file_read_format> format)
	{
		VITRIO_ASSERT(format);

		const std::lock_guard<std::mutex> lock(m_mutex);
		m_formats.push_back(std::move(format));
	}

	// The lock is held while the formats are asked and no longer. A format
	// is never removed, so the one found stays valid after it.
	const image_file_read_format*
	get_most_suitable_format(const image_file_probe &probe) const
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

	std::shared_ptr<image_reader> open(
		const image_file_probe &probe,
		const file_transfer_pool &transfers
	) const
	{
		return require_format(probe, "open").open(probe, transfers);
	}

	image_metadata read_metadata(const image_file_probe &probe) const
	{
		return require_format(probe, "read_metadata").read_metadata(probe);
	}

private:
	// The format an operation of the selector goes through, which is
	// chosen the same way for each of them and missed the same way too.
	const image_file_read_format& require_format(
		const image_file_probe &probe,
		const char *operation
	) const
	{
		const auto *format = get_most_suitable_format(probe);
		if (!format)
		{
			const auto context =
				probe.get_path() + ": image_file_read_format_selector::" +
				operation + ": ";

			if (!probe.exists())
			{
				throw image_file_error(
					context + "No registered format claims the path, and "
					"no readable file exists there."
				);
			}

			throw unsupported_operation_error(
				context + "No registered format can read the file."
			);
		}

		return *format;
	}

	mutable std::mutex m_mutex;
	std::vector<std::unique_ptr<image_file_read_format>> m_formats;
};

image_file_read_format_selector::image_file_read_format_selector()
	: m_implementation(std::make_unique<implementation>())
{
}

image_file_read_format_selector::~image_file_read_format_selector() = default;

const std::shared_ptr<image_file_read_format_selector>&
image_file_read_format_selector::get_shared()
{
	static const auto instance = [] ()
	{
		auto result = std::make_shared<image_file_read_format_selector>();
		result->register_builtin_formats();

		return result;
	}();

	return instance;
}

void image_file_read_format_selector::register_builtin_formats()
{
	get_builtin_image_file_read_format_registry().register_all(*this);
}

bool image_file_read_format_selector::register_format(
	std::unique_ptr<image_file_read_format> format
)
{
	if (!format)
	{
		return false;
	}

	m_implementation->register_format(std::move(format));

	return true;
}

std::shared_ptr<image_reader> image_file_read_format_selector::open(
	const std::string &path,
	const file_transfer_pool &transfers
) const
{
	return m_implementation->open(image_file_probe(path), transfers);
}

image_metadata
image_file_read_format_selector::read_metadata(const std::string &path) const
{
	return m_implementation->read_metadata(image_file_probe(path));
}

const image_file_read_format*
image_file_read_format_selector::get_most_suitable_format(
	const image_file_probe &probe
) const
{
	return m_implementation->get_most_suitable_format(probe);
}

} // namespace vitrio
