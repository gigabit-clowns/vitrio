// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/scratch_image_reader_provider.hpp>

#include "scratch_image_reader.hpp"

#include <vitrio/image_scratch.hpp>
#include <vitrio/image_scratch_entry.hpp>

#include <assert.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

scratch_image_reader_provider::scratch_image_reader_provider(
	std::shared_ptr<image_reader_provider> backing,
	std::shared_ptr<image_scratch> scratch
)
	: m_backing(std::move(backing))
	, m_scratch(std::move(scratch))
{
	if (!m_backing)
	{
		throw std::invalid_argument(
			"scratch_image_reader_provider: The backing provider must not "
			"be null."
		);
	}

	if (!m_scratch)
	{
		throw std::invalid_argument(
			"scratch_image_reader_provider: The scratch must not be null."
		);
	}
}

scratch_image_reader_provider::~scratch_image_reader_provider() = default;

std::shared_ptr<const image_reader>
scratch_image_reader_provider::acquire(const std::string &key)
{
	auto file = m_backing->acquire(key);
	VITRIO_ASSERT(file);

	auto entry = m_scratch->find(key);
	if (!entry)
	{
		return file;
	}

	return std::make_shared<scratch_image_reader>(
		std::move(entry),
		std::move(file)
	);
}

} // namespace vitrio
