# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches trompeloeil at VERSION, checks it against SHA256 and adds it to the
# project as trompeloeil::trompeloeil.
#
#   vitrio_fetch_trompeloeil(VERSION <version> SHA256 <hash>)
#
# Added below another project, trompeloeil installs nothing of its own.
function(vitrio_fetch_trompeloeil)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "VERSION;SHA256" "")

	set(tags "https://github.com/rollbear/trompeloeil/archive/refs/tags")
	FetchContent_Declare(
		trompeloeil
		URL "${tags}/v${arg_VERSION}.tar.gz"
		URL_HASH SHA256=${arg_SHA256}
	)
	FetchContent_MakeAvailable(trompeloeil)
endfunction()
