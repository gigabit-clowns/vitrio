# SPDX-License-Identifier: LGPL-2.1-or-later

# A project of its own, as one that uses an installed vitrio is. It knows
# nothing of the tree it is in.
cmake_minimum_required(VERSION 3.18)

project(vitrio-install-consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(vitrio ${VITRIO_EXPECTED_VERSION} REQUIRED)

add_executable(vitrio-install-consumer main.cpp)

target_link_libraries(
	vitrio-install-consumer
	PRIVATE
		vitrio::vitrio
)

enable_testing()
add_test(NAME vitrio-install-consumer COMMAND vitrio-install-consumer)
