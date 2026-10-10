// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>

#include "../mock/mock_detector_event_timeline_file_read_format.hpp"
#include "../mock/mock_image_file_read_format.hpp"
#include "../mock/mock_image_file_write_format.hpp"

#include <memory>
#include <trompeloeil.hpp>
#include <utility>
#include <vector>

namespace vitrio
{

/**
 * A format selector whose formats are mocks, each reporting a fixed
 * suitability for every file.
 *
 * The selector owns the formats it is given, so the expectations on them have
 * to be destroyed first. This holds both, in that order, and shares the
 * selector with whatever else a test hands it to.
 */
template <typename Selector, typename Format>
class format_selector_fixture
{
public:
	format_selector_fixture()
		: m_selector(std::make_shared<Selector>())
	{
	}

	/**
	 * Register a mock format reporting @p suitability for every file.
	 *
	 * Further expectations may be set on the format returned, provided they
	 * are destroyed before this fixture.
	 */
	Format& add_format(image_file_format_suitability suitability)
	{
		auto format = std::make_unique<Format>();
		auto &result = *format;
		m_expectations.push_back(
			NAMED_ALLOW_CALL(
				result,
				get_suitability(ANY(const image_file_probe&))
			)
				.RETURN(suitability)
		);
		m_selector->register_format(std::move(format));
		return result;
	}

	const std::shared_ptr<Selector>& get_selector() const noexcept
	{
		return m_selector;
	}

private:
	std::shared_ptr<Selector> m_selector;
	std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

using read_format_selector_fixture = format_selector_fixture<
	image_file_read_format_selector,
	mock_image_file_read_format
>;

using write_format_selector_fixture = format_selector_fixture<
	image_file_write_format_selector,
	mock_image_file_write_format
>;

using detector_event_timeline_file_read_format_selector_fixture =
	format_selector_fixture<
		detector_event_timeline_file_read_format_selector,
		mock_detector_event_timeline_file_read_format
	>;

} // namespace vitrio
