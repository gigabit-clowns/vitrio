// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_port_opener.hpp"

namespace vitrio
{

/**
 * @brief Opens ports that read as they are asked, which every system has.
 */
class blocking_file_read_port_opener final
	: public file_read_port_opener
{
public:
	blocking_file_read_port_opener() noexcept;
	~blocking_file_read_port_opener() override;

	std::unique_ptr<file_read_port> open() const override;
};

} // namespace vitrio
