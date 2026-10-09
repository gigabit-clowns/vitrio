# SPDX-License-Identifier: LGPL-2.1-or-later

import re

import vitrio as vio

def test_version_is_a_string_of_three_numbers():
	assert re.fullmatch(r'\d+\.\d+\.\d+', vio.__version__)
