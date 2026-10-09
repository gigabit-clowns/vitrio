// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_scratch;

/**
 * @brief A provider that serves files through a scratch.
 *
 * It is a **decorator over another provider**: every file is asked of the
 * backing provider. A file the scratch holds an entry of is served by a
 * reader that reads from the entry what the entry holds and from the
 * backing reader the rest. Any other file is served by the backing reader
 * itself.
 *
 * A reader served through an entry reports what the backing reader reports,
 * and reads through the backing reader only what the entry does not hold.
 */
class VITRIO_API scratch_image_reader_provider final
	: public image_reader_provider
{
public:
	/**
	 * @brief Construct a provider serving through a scratch.
	 *
	 * @param backing The provider asked for the files themselves.
	 * @param scratch What is held of the files, which reading through this
	 * provider adds to.
	 * @throws std::invalid_argument If @p backing or @p scratch is null.
	 */
	scratch_image_reader_provider(
		std::shared_ptr<image_reader_provider> backing,
		std::shared_ptr<image_scratch> scratch
	);

	~scratch_image_reader_provider() override;

	std::shared_ptr<const image_reader>
	acquire(const std::string &key) override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<image_reader_provider> m_backing;
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<image_scratch> m_scratch;
};

} // namespace vitrio
