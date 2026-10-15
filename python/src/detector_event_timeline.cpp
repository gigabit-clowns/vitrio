// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_timeline.hpp"

#include "guarded_detector_event_timeline.hpp"

#include <vitrio/detector_event_position_view.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/span.hpp>

#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

using guarded = guarded_detector_event_timeline;

using timestamp_view =
	nb::ndarray<nb::array_api, const std::uint64_t, nb::ndim<1>>;

using position_view =
	nb::ndarray<nb::array_api, const std::uint32_t, nb::ndim<2>>;

using position_input = nb::ndarray<
	const std::uint32_t,
	nb::ndim<2>,
	nb::c_contig,
	nb::device::cpu
>;

// What a view is given as its owner: the timeline, kept alive and counted
// until the view is gone.
struct view_owner
{
	nb::object timeline;
};

nb::capsule make_owner(nb::handle self)
{
	nb::inst_ptr<guarded>(self)->add_view();
	return nb::capsule(
		new view_owner{nb::borrow(self)},
		[] (void *pointer) noexcept
		{
			auto *owner = static_cast<view_owner*>(pointer);
			nb::inst_ptr<guarded>(owner->timeline)->remove_view();
			delete owner;
		}
	);
}

// An empty view still needs an address to point at.
template <typename T>
const T* address_of(const T *data) noexcept
{
	static const T nothing = 0;
	return data != nullptr ? data : &nothing;
}

std::size_t get_rank(guarded &self)
{
	const guarded::read_guard read(self);
	return read.get().get_rank();
}

std::size_t get_group_count(guarded &self)
{
	const guarded::read_guard read(self);
	return read.get().get_group_count();
}

std::size_t get_event_count(guarded &self)
{
	const guarded::read_guard read(self);
	return read.get().get_event_count();
}

timestamp_view get_timestamps(nb::handle self)
{
	auto &timeline = nb::cast<guarded&>(self);
	const guarded::read_guard read(timeline);
	const auto &events = read.get();

	const auto count = events.get_group_count();
	const std::size_t shape[1] = {count};

	// The timestamps are held one by one, so they are gathered into memory
	// the view owns rather than viewed where they are.
	auto *values = new std::uint64_t[count > 0 ? count : 1];
	for (std::size_t group = 0; group < count; ++group)
	{
		values[group] = events.get_timestamp(group);
	}
	nb::capsule owner(values, [] (void *pointer) noexcept
	{
		delete[] static_cast<std::uint64_t*>(pointer);
	});

	return timestamp_view(values, 1, shape, owner);
}

position_view get_positions(
	nb::handle self,
	std::optional<std::size_t> group
)
{
	auto &timeline = nb::cast<guarded&>(self);
	const guarded::read_guard read(timeline);
	const auto &events = read.get();

	if (group && *group >= events.get_group_count())
	{
		throw nb::index_error("DetectorEventTimeline group out of range");
	}

	const auto positions = group
		? events.get_positions(*group)
		: events.get_positions();
	const std::size_t shape[2] = {
		positions.get_event_count(),
		positions.get_rank()
	};

	return position_view(
		address_of(positions.get_coordinates().data()),
		2,
		shape,
		make_owner(self)
	);
}

void add_group(
	guarded &self,
	std::uint64_t timestamp,
	position_input positions
)
{
	const guarded::write_guard write(self);
	auto &events = write.get();

	if (positions.shape(1) != events.get_rank())
	{
		throw nb::value_error(
			"DetectorEventTimeline.add_group: The positions do not have the "
			"rank of the timeline."
		);
	}

	events.add_group(
		timestamp,
		detector_event_position_view(
			make_span(positions.data(), positions.size()),
			events.get_rank()
		)
	);
}

void clear(guarded &self)
{
	const guarded::write_guard write(self);
	write.get().clear();
}

void reserve(guarded &self, std::size_t group_count, std::size_t event_count)
{
	const guarded::write_guard write(self);
	write.get().reserve(group_count, event_count);
}

std::string to_repr(guarded &self)
{
	const guarded::read_guard read(self);
	const auto &events = read.get();
	return "DetectorEventTimeline(rank=" +
		std::to_string(events.get_rank()) + ", group_count=" +
		std::to_string(events.get_group_count()) + ", event_count=" +
		std::to_string(events.get_event_count()) + ")";
}

} // anonymous namespace

void bind_detector_event_timeline(nb::module_ &m)
{
	nb::class_<guarded>(
		m, "DetectorEventTimeline",
		"The events a detector counted over a stretch of time.\n"
		"\n"
		"The events come in groups of one timestamp each, such as the frames "
		"of an EER file. `timestamps` and `get_positions` return read-only "
		"views of the memory of the timeline, which keep it alive. The "
		"timeline refuses to change with `BufferError` while one of them is "
		"alive, as a `bytearray` refuses to resize while a `memoryview` of it "
		"is."
	)
		.def(
			nb::init<std::size_t>(), nb::arg("rank"),
			"Make an empty timeline.\n"
			"\n"
			"Args:\n"
			"    rank: How many axes the positions of its events have."
		)
		.def("__repr__", &to_repr)
		.def_prop_ro(
			"rank", &get_rank,
			"How many coordinates the position of an event has."
		)
		.def_prop_ro(
			"group_count", &get_group_count,
			"How many groups of events, one for each timestamp."
		)
		.def_prop_ro(
			"event_count", &get_event_count,
			"How many events in all the groups."
		)
		.def_prop_ro(
			"view_count", &guarded::get_view_count,
			"How many views of its memory are alive."
		)
		// The view owns its memory, so it needs no owner of the getter's.
		.def_prop_ro(
			"timestamps", &get_timestamps,
			nb::rv_policy::reference,
			"A view of the timestamps, one for each group, in time quanta."
		)
		.def(
			"get_positions", &get_positions,
			nb::arg("group") = nb::none(),
			"View the positions of the events.\n"
			"\n"
			"Args:\n"
			"    group: The group to view. Defaults to every group, one after "
			"the other.\n"
			"\n"
			"Returns: A read-only view with a row of coordinates for each "
			"event, on the grid of position quanta, slowest axis first.\n"
			"\n"
			"Raises:\n"
			"    IndexError: If the group is not there."
		)
		.def(
			"add_group", &add_group,
			nb::arg("timestamp"), nb::arg("positions"),
			"Add a group of events to the end.\n"
			"\n"
			"Args:\n"
			"    timestamp: The time quantum its events happened in.\n"
			"    positions: A two-dimensional array of unsigned 32-bit "
			"integers with a row for each event.\n"
			"\n"
			"Raises:\n"
			"    ValueError: If a row does not have the rank of the timeline.\n"
			"    BufferError: If a view of the timeline is alive."
		)
		.def(
			"clear", &clear,
			"Remove every group.\n"
			"\n"
			"Raises:\n"
			"    BufferError: If a view of the timeline is alive."
		)
		.def(
			"reserve", &reserve,
			nb::arg("group_count"), nb::arg("event_count"),
			"Make room for that many groups and events, to allocate once."
		);
}

} // namespace vitrio
