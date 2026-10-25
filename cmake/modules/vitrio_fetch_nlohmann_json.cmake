# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches nlohmann/json at COMMIT, that of a release, and adds it to the
# project as nlohmann_json::nlohmann_json, which is header only.
#
#   vitrio_fetch_nlohmann_json(COMMIT <sha>)
#
# Its tests are left out, and it installs nothing of its own. Its options
# predate those that take the value of a variable of the same name, so that
# is asked for here.
function(vitrio_fetch_nlohmann_json)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(JSON_BuildTests OFF)
	set(JSON_Install OFF)

	set(archives "https://github.com/nlohmann/json/archive")
	FetchContent_Declare(
		nlohmann_json
		URL "${archives}/${arg_COMMIT}.tar.gz"
	)
	FetchContent_MakeAvailable(nlohmann_json)
endfunction()
