// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_tag.hpp"

#include <assert.hpp>

#include <utility>

namespace vitrio
{
namespace dm
{

dm_tag::dm_tag(std::string label)
	: m_group(true)
	, m_label(std::move(label))
	, m_children()
	, m_encoding()
	, m_payload_offset(0)
	, m_payload_size(0)
{
}

dm_tag::dm_tag(
	std::string label,
	std::vector<std::uint64_t> encoding,
	std::size_t payload_offset,
	std::size_t payload_size
)
	: m_group(false)
	, m_label(std::move(label))
	, m_children()
	, m_encoding(std::move(encoding))
	, m_payload_offset(payload_offset)
	, m_payload_size(payload_size)
{
}

dm_tag::dm_tag(dm_tag &&other) noexcept = default;

dm_tag::~dm_tag() = default;

dm_tag& dm_tag::operator=(dm_tag &&other) noexcept = default;

bool dm_tag::is_group() const noexcept
{
	return m_group;
}

const std::string& dm_tag::get_label() const noexcept
{
	return m_label;
}

void dm_tag::add_child(dm_tag child)
{
	VITRIO_ASSERT(m_group);
	m_children.push_back(std::unique_ptr<dm_tag>(new dm_tag(std::move(child))));
}

std::size_t dm_tag::get_child_count() const noexcept
{
	return m_children.size();
}

const dm_tag& dm_tag::get_child(std::size_t index) const noexcept
{
	VITRIO_ASSERT(index < m_children.size());
	return *m_children[index];
}

const dm_tag* dm_tag::find_child(const std::string &label) const noexcept
{
	for (const auto &child : m_children)
	{
		if (child->get_label() == label)
		{
			return child.get();
		}
	}

	return nullptr;
}

span<const std::uint64_t> dm_tag::get_encoding() const noexcept
{
	return make_span(m_encoding.data(), m_encoding.size());
}

std::size_t dm_tag::get_payload_offset() const noexcept
{
	return m_payload_offset;
}

std::size_t dm_tag::get_payload_size() const noexcept
{
	return m_payload_size;
}

} // namespace dm
} // namespace vitrio
