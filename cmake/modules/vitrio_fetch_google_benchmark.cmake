# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches Google Benchmark at COMMIT, that of a release, and builds it with the
# project, as benchmark::benchmark.
#
#   vitrio_fetch_google_benchmark(COMMIT <sha>)
#
# It is cloned with git rather than fetched as an archive, unlike the other
# dependencies: Google Benchmark reads its version from the tags of its own
# repository, and the sources of an archive have none, so git would read the
# version of whatever repository holds the build tree instead.
#
# Only its library is wanted, so its tests, its install rules and its warnings
# as errors are turned off.
function(vitrio_fetch_google_benchmark)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_SHARED_LIBS OFF)
	set(BENCHMARK_ENABLE_TESTING OFF)
	set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
	set(BENCHMARK_ENABLE_INSTALL OFF)
	set(BENCHMARK_INSTALL_DOCS OFF)
	set(BENCHMARK_ENABLE_WERROR OFF)

	FetchContent_Declare(
		benchmark
		GIT_REPOSITORY "https://github.com/google/benchmark.git"
		GIT_TAG "${arg_COMMIT}"
	)
	FetchContent_MakeAvailable(benchmark)
endfunction()
