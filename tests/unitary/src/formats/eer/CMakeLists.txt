# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_eer_detector_event_timeline_file_read_format.cpp
		test_eer_detector_event_timeline_reader.cpp
		test_eer_encoding.cpp
		test_eer_extensions.cpp
		test_eer_strip_decoder.cpp
)
