# SPDX-License-Identifier: LGPL-2.1-or-later

# The sources are compiled once, into an object library. The shared library
# and the unit tests are both made of those objects, which is how the unit
# tests reach what the shared library does not export.
#
# CMake gives the export definition, position independent code and hidden
# visibility to SHARED targets only, so the object library is given them here.
add_library(vitrio-obj OBJECT)

set_target_properties(
	vitrio-obj
	PROPERTIES
		POSITION_INDEPENDENT_CODE ON
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(
	vitrio-obj
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

target_compile_features(vitrio-obj PUBLIC cxx_std_14)

target_compile_definitions(
	vitrio-obj
	PRIVATE
		vitrio_EXPORTS
		VITRIO_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		VITRIO_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		VITRIO_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(MSVC)
	target_compile_definitions(
		vitrio-obj
		PRIVATE
			NOMINMAX
	)
endif()

vitrio_enable_warnings(vitrio-obj)

target_link_libraries(
	vitrio-obj
	PUBLIC
		Threads::Threads
		Boost::filesystem
		Boost::headers
		half::half
		spdlog::spdlog_header_only
)

target_sources(
	vitrio-obj
	PRIVATE
		builtin_image_file_format_registry.cpp
		caching_image_reader_provider.cpp
		clipping_image_transfer_sanitizer.cpp
		executor_image_loader.cpp
		executor_image_saver.cpp
		file_image_reader_provider.cpp
		file_image_writer_provider.cpp
		host_image_scratch_storage.cpp
		image_descriptor.cpp
		image_file_probe.cpp
		image_file_read_format.cpp
		image_file_read_format_registry.cpp
		image_file_read_format_selector.cpp
		image_file_write_format.cpp
		image_file_write_format_registry.cpp
		image_file_write_format_selector.cpp
		image_loader.cpp
		image_location.cpp
		image_location_grouping.cpp
		image_metadata.cpp
		image_plan_builders.cpp
		image_read.cpp
		image_reader.cpp
		image_reader_provider.cpp
		image_region_grouping.cpp
		image_saver.cpp
		image_scratch.cpp
		image_scratch_entry.cpp
		image_scratch_storage.cpp
		image_transaction_plan.cpp
		image_transfer_plan.cpp
		image_transfer_sanitizer.cpp
		image_transfer_shape.cpp
		image_write.cpp
		image_writer.cpp
		image_writer_provider.cpp
		index_table.cpp
		indexed_image_scratch.cpp
		indexed_image_scratch_entry.cpp
		interned_key_list.cpp
		library_version.cpp
		logger.cpp
		mapped_file_image_scratch_storage.cpp
		scratch_image_reader.cpp
		scratch_image_reader_provider.cpp
		strict_image_transfer_sanitizer.cpp
)

if(VITRIO_ENABLE_TIFF)
	target_link_libraries(vitrio-obj PUBLIC TIFF::TIFF)
endif()

add_subdirectory(array)
add_subdirectory(concurrency)
add_subdirectory(formats)
add_subdirectory(memory)
add_subdirectory(system)

# vitrio is a shared library only. Its image formats register themselves
# through objects at namespace scope, and a static archive would drop the
# object files nothing else refers to.
add_library(vitrio SHARED)
add_library(vitrio::vitrio ALIAS vitrio)

target_link_libraries(
	vitrio
	PRIVATE
		vitrio-obj
)

# Until 1.0 a minor release may break the ABI, so it is part of the name the
# dynamic linker looks for.
if(PROJECT_VERSION_MAJOR EQUAL 0)
	set(VITRIO_SOVERSION 0.${PROJECT_VERSION_MINOR})
else()
	set(VITRIO_SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

set_target_properties(
	vitrio
	PROPERTIES
		LINKER_LANGUAGE CXX
		VERSION ${PROJECT_VERSION}
		SOVERSION ${VITRIO_SOVERSION}
)

target_include_directories(
	vitrio
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(vitrio PUBLIC cxx_std_14)

# Keeps the symbols of the static libraries vitrio is linked with out of its
# own interface.
if(UNIX AND NOT APPLE)
	target_link_options(vitrio PRIVATE LINKER:--exclude-libs,ALL)
endif()

generate_export_header(
	vitrio
	BASE_NAME VITRIO
	EXPORT_MACRO_NAME VITRIO_API
	NO_EXPORT_MACRO_NAME VITRIO_LOCAL
	DEPRECATED_MACRO_NAME VITRIO_DEPRECATED
	STATIC_DEFINE VITRIO_NO_EXPORTS
	EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/vitrio/export.hpp
)

if(NOT VITRIO_INSTALL)
	return()
endif()

install(
	TARGETS vitrio
	EXPORT vitrio-targets
	RUNTIME
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		COMPONENT vitrio_runtime
	LIBRARY
		DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT vitrio_runtime
		NAMELINK_COMPONENT vitrio_namelink
	ARCHIVE
		DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT vitrio_development
)

install(
	DIRECTORY
		${PROJECT_SOURCE_DIR}/include/
		${PROJECT_BINARY_DIR}/include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	COMPONENT vitrio_development
	FILES_MATCHING
		PATTERN "*.hpp"
		PATTERN "*.inl"
)

configure_package_config_file(
	${PROJECT_SOURCE_DIR}/cmake/config/vitrio-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/vitrio-config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vitrio
)

# The same rule as the name of the library: before 1.0 only a release of the
# same minor version is compatible.
if(PROJECT_VERSION_MAJOR EQUAL 0)
	set(VITRIO_COMPATIBILITY SameMinorVersion)
else()
	set(VITRIO_COMPATIBILITY SameMajorVersion)
endif()

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/vitrio-config-version.cmake
	COMPATIBILITY ${VITRIO_COMPATIBILITY}
)

install(
	EXPORT vitrio-targets
	FILE vitrio-targets.cmake
	NAMESPACE vitrio::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vitrio
	COMPONENT vitrio_development
)

install(
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/vitrio-config.cmake
		${CMAKE_CURRENT_BINARY_DIR}/vitrio-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vitrio
	COMPONENT vitrio_development
)
