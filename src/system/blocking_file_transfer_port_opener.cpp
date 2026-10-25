// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blocking_file_transfer_port_opener.hpp"

#include "blocking_file_transfer_port.hpp"

namespace vitrio
{

blocking_file_transfer_port_opener::
blocking_file_transfer_port_opener() noexcept = default;

blocking_file_transfer_port_opener::
~blocking_file_transfer_port_opener() = default;

std::unique_ptr<file_transfer_port>
blocking_file_transfer_port_opener::open() const
{
	return std::make_unique<blocking_file_transfer_port>();
}

} // namespace vitrio
