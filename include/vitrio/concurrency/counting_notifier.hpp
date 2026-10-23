// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

/**
 * @brief A notifier that reports to another once a fixed number of units of
 * work have all reported to it.
 *
 * It makes several units of work one unit of the notifier it reports to:
 * the reads of one file in a batch, for example, each reported here, become
 * one report of the file to the batch. It reports once every unit has
 * reported, whichever failed, and reports the first exception reported, if
 * any; the rest are discarded.
 *
 * @par Thread safety
 * @ref notify_done may be called concurrently, from as many threads as the
 * starting count.
 */
class VITRIO_API counting_notifier final
	: public completion_notifier
{
public:
	/**
	 * @brief Construct a notifier for a known number of units of work.
	 *
	 * A @p count of zero reports to @p parent at once, from this
	 * constructor, as a success.
	 *
	 * @param count How many times @ref notify_done is reported before this
	 * reports to @p parent.
	 * @param parent Where the outcome of them all is reported. Must not be
	 * null.
	 * @throws std::invalid_argument If @p parent is null.
	 */
	counting_notifier(
		std::size_t count,
		std::shared_ptr<completion_notifier> parent
	);
	~counting_notifier() override;

	counting_notifier(const counting_notifier &other) = delete;
	counting_notifier(counting_notifier &&other) = delete;
	counting_notifier& operator=(const counting_notifier &other) = delete;
	counting_notifier& operator=(counting_notifier &&other) = delete;

	void notify_done(std::exception_ptr error = nullptr) override;

private:
	class implementation;
	VITRIO_STD_MEMBER_INTERFACE
	std::unique_ptr<implementation> m_implementation;
};

} // namespace vitrio
