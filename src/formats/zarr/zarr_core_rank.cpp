// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_core_rank.hpp"

#include <algorithm>

namespace vitrio
{
namespace zarr
{

namespace
{

// The most axes one image or volume has.
const std::size_t largest_core = 3;

std::size_t get_written_core_rank(
	std::size_t rank,
	const nlohmann::json &attributes
)
{
	if (!attributes.is_object())
	{
		return 0;
	}

	const auto vitrio = attributes.find("vitrio");
	if (vitrio == attributes.end() || !vitrio->is_object())
	{
		return 0;
	}

	const auto core = vitrio->find("core_rank");
	if (
		core == vitrio->end() || !core->is_number_unsigned() ||
		core->get<std::uint64_t>() == 0 || core->get<std::uint64_t>() > rank
	)
	{
		return 0;
	}

	return static_cast<std::size_t>(core->get<std::uint64_t>());
}

// Whether every level states a scale for every axis.
bool has_scales(const zarr_multiscale &multiscale, std::size_t rank)
{
	return multiscale.datasets.size() > 1 && std::all_of(
		multiscale.datasets.cbegin(),
		multiscale.datasets.cend(),
		[rank] (const zarr_dataset &dataset)
		{
			return dataset.scale.size() == rank;
		}
	);
}

bool is_scaled(const zarr_multiscale &multiscale, std::size_t axis)
{
	const auto finest = multiscale.datasets.front().scale[axis];
	return std::any_of(
		multiscale.datasets.cbegin(),
		multiscale.datasets.cend(),
		[finest, axis] (const zarr_dataset &dataset)
		{
			return dataset.scale[axis] != finest;
		}
	);
}

std::size_t get_axes_core_rank(
	std::size_t rank,
	const zarr_multiscale *multiscale
)
{
	if (multiscale == nullptr || multiscale->axes.size() != rank)
	{
		return 0;
	}

	std::size_t core = 0;
	while (
		core < std::min(rank, largest_core) &&
		multiscale->axes[rank - 1 - core].type == "space"
	)
	{
		++core;
	}

	// The first axis of the core is a stack when the levels scale the others
	// but never it.
	if (core > 1 && has_scales(*multiscale, rank))
	{
		const auto first = rank - core;
		bool others_scaled = false;
		for (auto axis = first + 1; axis < rank; ++axis)
		{
			others_scaled = others_scaled || is_scaled(*multiscale, axis);
		}
		if (others_scaled && !is_scaled(*multiscale, first))
		{
			--core;
		}
	}

	return core;
}

std::size_t get_named_core_rank(
	std::size_t rank,
	const std::vector<std::string> &names
)
{
	if (names.size() != rank)
	{
		return 0;
	}

	std::size_t core = 0;
	while (core < std::min(rank, largest_core))
	{
		const auto &name = names[rank - 1 - core];
		if (name != "z" && name != "y" && name != "x")
		{
			break;
		}
		++core;
	}

	return core;
}

} // anonymous namespace

std::size_t decide_core_rank(
	std::size_t rank,
	const nlohmann::json &attributes,
	const std::vector<std::string> &dimension_names,
	const zarr_multiscale *multiscale
)
{
	for (const auto core : {
		get_written_core_rank(rank, attributes),
		get_axes_core_rank(rank, multiscale),
		get_named_core_rank(rank, dimension_names),
	})
	{
		if (core != 0)
		{
			return core;
		}
	}

	return std::min(rank, largest_core);
}

} // namespace zarr
} // namespace vitrio
