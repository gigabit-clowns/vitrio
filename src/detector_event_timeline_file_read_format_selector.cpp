// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>

#include <vitrio/detector_event_timeline_file_read_format.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_file_probe.hpp>

#include <assert.hpp>
#include <builtin_detector_event_timeline_file_read_format_registry.hpp>
#include <find_most_suitable_format.hpp>

#include <mutex>
#include <utility>
#include <vector>

namespace vitrio
{

class detector_event_timeline_file_read_format_selector::implementation
{
public:
	void register_format(
		std::unique_ptr<detector_event_timeline_file_read_format> format
	)
	{
		VITRIO_ASSERT(format);

		const std::lock_guard<std::mutex> lock(m_mutex);
		m_formats.push_back(std::move(format));
	}

	// The lock is held while the formats are asked and no longer. A format
	// is never removed, so the one found stays valid after it.
	const detector_event_timeline_file_read_format*
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

	std::shared_ptr<detector_event_timeline_reader>
	open(const image_file_probe &probe) const
	{
		const auto *format = get_most_suitable_format(probe);
		if (!format)
		{
			if (!probe.exists())
			{
				throw image_file_error(
					probe.get_path() +
					": detector_event_timeline_file_read_format_selector::"
					"open: No registered format claims the path, and no "
					"readable file exists there."
				);
			}
			throw unsupported_operation_error(
				probe.get_path() +
				": detector_event_timeline_file_read_format_selector::open: "
				"No registered format can read the file."
			);
		}

		return format->open(probe);
	}

private:
	mutable std::mutex m_mutex;
	std::vector<
		std::unique_ptr<detector_event_timeline_file_read_format>
	> m_formats;
};

detector_event_timeline_file_read_format_selector::
detector_event_timeline_file_read_format_selector()
	: m_implementation(std::make_unique<implementation>())
{
}

detector_event_timeline_file_read_format_selector::
~detector_event_timeline_file_read_format_selector() = default;

const std::shared_ptr<detector_event_timeline_file_read_format_selector>&
detector_event_timeline_file_read_format_selector::get_shared()
{
	static const auto instance = [] ()
	{
		auto result = std::make_shared<
			detector_event_timeline_file_read_format_selector
		>();
		result->register_builtin_formats();
		return result;
	}();

	return instance;
}

void detector_event_timeline_file_read_format_selector::
register_builtin_formats()
{
	get_builtin_detector_event_timeline_file_read_format_registry()
		.register_all(*this);
}

bool detector_event_timeline_file_read_format_selector::register_format(
	std::unique_ptr<detector_event_timeline_file_read_format> format
)
{
	if (!format)
	{
		return false;
	}

	m_implementation->register_format(std::move(format));
	return true;
}

std::shared_ptr<detector_event_timeline_reader>
detector_event_timeline_file_read_format_selector::open(
	const std::string &path
) const
{
	return m_implementation->open(image_file_probe(path));
}

const detector_event_timeline_file_read_format*
detector_event_timeline_file_read_format_selector::get_most_suitable_format(
	const image_file_probe &probe
) const
{
	return m_implementation->get_most_suitable_format(probe);
}

} // namespace vitrio
