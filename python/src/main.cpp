// SPDX-License-Identifier: LGPL-2.1-or-later

#include "caching_image_reader_provider.hpp"
#include "counting_detector_event_renderer.hpp"
#include "detector_event_binning.hpp"
#include "detector_event_fractionation.hpp"
#include "detector_event_image_reader_provider.hpp"
#include "detector_event_renderer.hpp"
#include "detector_event_timeline.hpp"
#include "detector_event_timeline_descriptor.hpp"
#include "detector_event_timeline_file_read_format_selector.hpp"
#include "detector_event_timeline_reader.hpp"
#include "detector_event_timeline_reader_provider.hpp"
#include "executor_image_loader.hpp"
#include "executor_image_saver.hpp"
#include "file_detector_event_timeline_reader_provider.hpp"
#include "file_image_reader_provider.hpp"
#include "file_transfer_pool.hpp"
#include "file_image_writer_provider.hpp"
#include "image_descriptor.hpp"
#include "image_file_read_format_selector.hpp"
#include "image_file_write_format_selector.hpp"
#include "image_loader.hpp"
#include "image_location.hpp"
#include "image_location_grouping.hpp"
#include "image_read.hpp"
#include "image_reader_provider.hpp"
#include "image_saver.hpp"
#include "image_scratch.hpp"
#include "image_write.hpp"
#include "image_writer_provider.hpp"
#include "index_table.hpp"
#include "indexed_image_scratch.hpp"
#include "library_version.hpp"
#include "scratch_image_reader_provider.hpp"

#include <array/array.hpp>
#include <array/numerical_type.hpp>
#include <concurrency/completion.hpp>
#include <concurrency/executor.hpp>
#include <concurrency/synchronous_executor.hpp>
#include <concurrency/thread_pool_executor.hpp>

#include <nanobind/nanobind.h>

// A class is added after the one it derives from.
NB_MODULE(_binding, m)
{
	vitrio::bind_library_version(m);

	vitrio::bind_numerical_type(m);
	vitrio::bind_array(m);

	vitrio::bind_completion(m);
	vitrio::bind_executor(m);
	vitrio::bind_synchronous_executor(m);
	vitrio::bind_thread_pool_executor(m);

	vitrio::bind_index_table(m);
	vitrio::bind_image_descriptor(m);
	vitrio::bind_image_location(m);
	vitrio::bind_image_location_grouping(m);

	vitrio::bind_image_file_read_format_selector(m);
	vitrio::bind_image_file_write_format_selector(m);

	vitrio::bind_image_reader_provider(m);
	vitrio::bind_file_transfer_pool(m);
	vitrio::bind_file_image_reader_provider(m);
	vitrio::bind_caching_image_reader_provider(m);
	vitrio::bind_image_scratch(m);
	vitrio::bind_indexed_image_scratch(m);
	vitrio::bind_scratch_image_reader_provider(m);
	vitrio::bind_image_loader(m);
	vitrio::bind_executor_image_loader(m);

	vitrio::bind_image_writer_provider(m);
	vitrio::bind_file_image_writer_provider(m);
	vitrio::bind_image_saver(m);
	vitrio::bind_executor_image_saver(m);

	vitrio::bind_image_read(m);
	vitrio::bind_image_write(m);

	vitrio::bind_detector_event_timeline_descriptor(m);
	vitrio::bind_detector_event_timeline(m);
	vitrio::bind_detector_event_fractionation(m);
	vitrio::bind_detector_event_binning(m);

	vitrio::bind_detector_event_timeline_file_read_format_selector(m);
	vitrio::bind_detector_event_timeline_reader(m);
	vitrio::bind_detector_event_timeline_reader_provider(m);
	vitrio::bind_file_detector_event_timeline_reader_provider(m);

	vitrio::bind_detector_event_renderer(m);
	vitrio::bind_counting_detector_event_renderer(m);
	vitrio::bind_detector_event_image_reader_provider(m);
}
