# SPDX-License-Identifier: LGPL-2.1-or-later

# The benchmark of the engine is white-box, as the unit tests are: it is
# linked with the objects of the library rather than with the library, and
# sees its private headers.
add_executable(vitrio-benchmark-strided-transfer strided_transfer.cpp)

# The objects are in this executable, so nothing is imported from a shared
# library.
target_compile_definitions(
	vitrio-benchmark-strided-transfer
	PRIVATE
		VITRIO_NO_EXPORTS
)

target_include_directories(
	vitrio-benchmark-strided-transfer
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
	vitrio-benchmark-strided-transfer
	PRIVATE
		vitrio-obj
		benchmark::benchmark
)

vitrio_enable_warnings(vitrio-benchmark-strided-transfer)

# A short run, which checks that every benchmark still runs and measures
# nothing: its file is small, and each benchmark runs for a moment.
if(VITRIO_BUILD_TESTING)
	add_test(
		NAME vitrio-benchmark-strided-transfer
		COMMAND
			vitrio-benchmark-strided-transfer --benchmark_min_time=0.001s
	)
	set_tests_properties(
		vitrio-benchmark-strided-transfer
		PROPERTIES
			ENVIRONMENT "VITRIO_BENCH_FILE_BYTES=16777216"
			LABELS benchmark
	)
endif()
