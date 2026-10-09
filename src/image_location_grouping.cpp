// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_location_grouping.hpp>

#include <vitrio/image_location.hpp>

#include <assert.hpp>

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace vitrio
{

image_location_grouping::image_location_grouping(
	span<const image_location> locations
)
{
	std::unordered_map<std::string, std::size_t> files;
	std::vector<std::string> keys;
	std::vector<std::vector<std::size_t>> named;
	std::vector<bool> whole;

	for (const auto &location : locations)
	{
		const auto inserted = files.emplace(location.get_key(), keys.size());
		if (inserted.second)
		{
			keys.push_back(location.get_key());
			named.emplace_back();
			whole.push_back(false);
		}

		const auto file_index = inserted.first->second;
		if (location.has_index_in_stack())
		{
			named[file_index].push_back(location.get_index_in_stack());
		}
		else
		{
			whole[file_index] = true;
		}
	}

	std::vector<std::size_t> order(keys.size());
	std::iota(order.begin(), order.end(), std::size_t(0));
	std::sort(
		order.begin(),
		order.end(),
		[&keys] (std::size_t lhs, std::size_t rhs)
		{
			return keys[lhs] < keys[rhs];
		}
	);

	m_keys.reserve(keys.size());
	m_first_positions.reserve(keys.size() + 1);
	m_first_positions.push_back(0);
	for (const auto file_index : order)
	{
		m_keys.push_back(std::move(keys[file_index]));

		if (!whole[file_index])
		{
			auto &indices = named[file_index];
			std::sort(indices.begin(), indices.end());
			m_indices.insert(
				m_indices.end(),
				indices.begin(),
				std::unique(indices.begin(), indices.end())
			);
		}

		m_first_positions.push_back(m_indices.size());
	}
}

image_location_grouping::image_location_grouping(
	const image_location_grouping &other
) = default;
image_location_grouping::image_location_grouping(
	image_location_grouping &&other
) noexcept = default;
image_location_grouping::~image_location_grouping() = default;

image_location_grouping& image_location_grouping::operator=(
	const image_location_grouping &other
) = default;
image_location_grouping& image_location_grouping::operator=(
	image_location_grouping &&other
) noexcept = default;

std::size_t image_location_grouping::get_group_count() const noexcept
{
	return m_keys.size();
}

image_location_grouping::group
image_location_grouping::get_group(std::size_t index) const
{
	if (index >= get_group_count())
	{
		throw std::out_of_range(
			"image_location_grouping: The index is not below the number of "
			"groups."
		);
	}

	const auto first = m_first_positions[index];
	const auto last = m_first_positions[index + 1];

	return group(
		m_keys[index],
		make_span(m_indices.data() + first, last - first)
	);
}

image_location_grouping::group::group(
	const std::string &key,
	span<const std::size_t> indices
) noexcept
	: m_key(&key)
	, m_indices(indices)
{
}

const std::string& image_location_grouping::group::get_key() const noexcept
{
	VITRIO_ASSERT(m_key);
	return *m_key;
}

bool image_location_grouping::group::is_whole() const noexcept
{
	return m_indices.empty();
}

span<const std::size_t>
image_location_grouping::group::get_indices() const noexcept
{
	return m_indices;
}

} // namespace vitrio
