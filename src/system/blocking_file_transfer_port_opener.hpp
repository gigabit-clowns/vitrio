// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_port_opener.hpp"

namespace vitrio
{

/**
 * @brief Opens ports that transfer as they are asked, which every system
 * has.
 */
class blocking_file_transfer_port_opener final
	: public file_transfer_port_opener
{
public:
	blocking_file_transfer_port_opener() noexcept;
	~blocking_file_transfer_port_opener() override;

	std::unique_ptr<file_transfer_port> open() const override;
};

} // namespace vitrio
