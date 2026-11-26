// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_metadata.hpp>

#include "standard_image_metadata_field.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

/**
 * @brief Check whether an entry comes before a label in the order the
 * entries are kept in.
 */
bool precedes(
	const image_metadata::entry_type &entry,
	const std::string &label
) noexcept
{
	return entry.first < label;
}

/**
 * @brief Make a value what the label it goes under holds.
 *
 * @param label The label.
 * @param value The value given for it.
 * @return image_metadata_value The value to hold.
 * @throws std::invalid_argument If the label may not be stated, or not with
 * that value.
 */
image_metadata_value
normalize(const std::string &label, image_metadata_value value)
{
	if (label.empty())
	{
		throw std::invalid_argument(
			"image_metadata::set: The label must not be empty."
		);
	}

	const auto *field = find_standard_image_metadata_field(label);
	if (!field)
	{
		if (label.find('.') == std::string::npos)
		{
			throw std::invalid_argument(
				"image_metadata::set: " + label + " is no standard field, "
				"and any other label has a prefix that ends at a dot."
			);
		}

		return value;
	}

	if (value.get_type() == field->get_type())
	{
		return value;
	}

	if (field->get_type() == image_metadata_value_type::real &&
		value.get_type() == image_metadata_value_type::integer)
	{
		return image_metadata_value(static_cast<double>(value.get_integer()));
	}

	throw std::invalid_argument(
		"image_metadata::set: " + label + " is a standard field of another "
		"type than the value given for it."
	);
}

} // anonymous namespace

image_metadata::image_metadata() noexcept = default;

image_metadata::image_metadata(const image_metadata &other) = default;
image_metadata::image_metadata(image_metadata &&other) noexcept = default;
image_metadata::~image_metadata() = default;

image_metadata&
image_metadata::operator=(const image_metadata &other) = default;
image_metadata&
image_metadata::operator=(image_metadata &&other) noexcept = default;

void image_metadata::set(std::string label, image_metadata_value value)
{
	auto normalized = normalize(label, std::move(value));

	const auto position = std::lower_bound(
		m_entries.begin(),
		m_entries.end(),
		label,
		precedes
	);

	if (position != m_entries.end() && position->first == label)
	{
		position->second = std::move(normalized);
	}
	else
	{
		m_entries.emplace(
			position,
			std::move(label),
			std::move(normalized)
		);
	}
}

const image_metadata_value*
image_metadata::find(const std::string &label) const noexcept
{
	const auto position = std::lower_bound(
		m_entries.cbegin(),
		m_entries.cend(),
		label,
		precedes
	);

	if (position == m_entries.cend() || position->first != label)
	{
		return nullptr;
	}

	return &position->second;
}

const image_metadata_value&
image_metadata::get(const std::string &label) const
{
	const auto *value = find(label);
	if (!value)
	{
		throw std::out_of_range(
			"image_metadata::get: The metadata states nothing under " +
			label + "."
		);
	}

	return *value;
}

bool image_metadata::erase(const std::string &label) noexcept
{
	const auto position = std::lower_bound(
		m_entries.begin(),
		m_entries.end(),
		label,
		precedes
	);

	if (position == m_entries.end() || position->first != label)
	{
		return false;
	}

	m_entries.erase(position);
	return true;
}

span<const image_metadata::entry_type>
image_metadata::get_entries() const noexcept
{
	return make_span(m_entries.data(), m_entries.size());
}

} // namespace vitrio
