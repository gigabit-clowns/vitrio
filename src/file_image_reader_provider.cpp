// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_image_reader_provider.hpp>

#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/file_transfer_pool.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

file_image_reader_provider::file_image_reader_provider(
	std::shared_ptr<const image_file_read_format_selector> formats,
	std::shared_ptr<const file_transfer_pool> transfers
)
	: m_formats(std::move(formats))
	, m_transfers(std::move(transfers))
{
	if (!m_formats)
	{
		throw std::invalid_argument(
			"file_image_reader_provider: The format selector must not "
			"be null."
		);
	}

	if (!m_transfers)
	{
		throw std::invalid_argument(
			"file_image_reader_provider: The pool of file transfers must "
			"not be null."
		);
	}
}

file_image_reader_provider::~file_image_reader_provider() = default;

std::shared_ptr<const image_reader>
file_image_reader_provider::acquire(const std::string &key)
{
	return m_formats->open(key, *m_transfers);
}

} // namespace vitrio
