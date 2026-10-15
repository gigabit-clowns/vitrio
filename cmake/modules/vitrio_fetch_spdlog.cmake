# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches spdlog at VERSION, checks it against SHA256 and adds it to the
# project, where vitrio uses spdlog::spdlog_header_only.
#
#   vitrio_fetch_spdlog(VERSION <version> SHA256 <hash>)
#
# Added below another project, spdlog installs nothing of its own.
function(vitrio_fetch_spdlog)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "VERSION;SHA256" "")

	set(tags "https://github.com/gabime/spdlog/archive/refs/tags")
	FetchContent_Declare(
		spdlog
		URL "${tags}/v${arg_VERSION}.tar.gz"
		URL_HASH SHA256=${arg_SHA256}
	)
	FetchContent_MakeAvailable(spdlog)
endfunction()
