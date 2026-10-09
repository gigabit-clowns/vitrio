// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_reader_provider.hpp>

#include <vitrio/image_reader.hpp>

namespace vitrio
{

image_reader_provider::image_reader_provider() noexcept = default;
image_reader_provider::~image_reader_provider() = default;

image_descriptor query_descriptor(
	image_reader_provider &readers,
	const std::string &path
)
{
	return readers.acquire(path)->get_descriptor();
}

} // namespace vitrio
