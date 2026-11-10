# SPDX-License-Identifier: LGPL-2.1-or-later

# Declared here rather than beside the other toggles, because this dependency
# exists only while the benchmarks are built.
cmake_dependent_option(
	VITRIO_USE_SYSTEM_GOOGLE_BENCHMARK
	"Find an installed Google Benchmark rather than fetching it"
	OFF
	"NOT VITRIO_USE_SYSTEM_DEPENDENCIES"
	ON
)

# Running a benchmark for a minimum time given in seconds, as the test of
# each one does, arrived in 1.8.
if(VITRIO_USE_SYSTEM_GOOGLE_BENCHMARK)
	find_package(benchmark 1.8 REQUIRED CONFIG)
else()
	include(vitrio_fetch_google_benchmark)
	vitrio_fetch_google_benchmark(
		COMMIT 192ef10025eb2c4cdd392bc502f0c852196baa48
	) # v1.9.5
endif()

add_subdirectory(cpp)
