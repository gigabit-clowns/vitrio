# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches spdlog at COMMIT, that of a release, and adds it to the project,
# where vitrio uses spdlog::spdlog_header_only.
#
#   vitrio_fetch_spdlog(COMMIT <sha>)
#
# Added below another project, spdlog installs nothing of its own.
function(vitrio_fetch_spdlog)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	FetchContent_Declare(
		spdlog
		URL "https://github.com/gabime/spdlog/archive/${arg_COMMIT}.tar.gz"
	)
	FetchContent_MakeAvailable(spdlog)
endfunction()
