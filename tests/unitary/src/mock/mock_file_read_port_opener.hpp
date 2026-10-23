// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_read_port.hpp>
#include <system/file_read_port_opener.hpp>

#include <trompeloeil.hpp>

#include <memory>

namespace vitrio
{

class mock_file_read_port_opener final
	: public file_read_port_opener
{
public:
	mock_file_read_port_opener() = default;

	MAKE_CONST_MOCK0(open, std::unique_ptr<file_read_port>(), override);
};

} // namespace vitrio
