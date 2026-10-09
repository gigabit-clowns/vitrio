// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <vitrio/indexed_image_scratch.hpp>
#include <vitrio/scratch_image_reader_provider.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/synchronous_executor.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/executor_image_loader.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/host_image_scratch_storage.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/index_table.hpp>
#include <vitrio/mapped_file_image_scratch_storage.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// Every stack holds six images of 8 by 8.
const std::size_t stack_count = 3;
const std::size_t image_count = 6;
const std::size_t image_extent = 8;
const std::size_t image_size = image_extent * image_extent;
const std::size_t image_bytes = image_size * sizeof(float);

// Room for every image of the three stacks.
const std::size_t dataset_bytes = stack_count * image_count * image_bytes;

const float untouched = -1.0F;

// Every case runs as a process of its own, and several may run at once, so
// the files of a case are named after it.
std::string name_file(const std::string &stem, const std::string &extension)
{
	const auto case_name = Catch::getResultCapture().getCurrentTestName();
	const auto tag = std::hash<std::string>()(case_name);

	return stem + "_" + std::to_string(tag) + extension;
}

// Three stacks on disk whose every value tells its stack, its image and its
// place in the image, and the provider that reads them as they are.
class image_scratch_fixture
{
public:
	image_scratch_fixture()
		: m_first(name_file("image_scratch_stack_0", ".mrcs"))
		, m_second(name_file("image_scratch_stack_1", ".mrcs"))
		, m_third(name_file("image_scratch_stack_2", ".mrcs"))
		, m_storage_file(name_file("image_scratch_storage", ".mapped"))
	{
		for (std::size_t stack = 0; stack < stack_count; ++stack)
		{
			write_stack_file(stack);
		}

		direct = std::make_shared<file_image_reader_provider>(
			image_file_read_format_selector::get_shared()
		);
	}

protected:
	const std::string& get_path(std::size_t stack) const noexcept
	{
		const std::array<const scoped_path*, stack_count> paths = {
			&m_first,
			&m_second,
			&m_third
		};

		return paths[stack]->get();
	}

	image_location locate(std::size_t stack, std::size_t image) const
	{
		return image_location(get_path(stack), image);
	}

	// Storage in main memory, or in a mapped file.
	std::shared_ptr<image_scratch_storage>
	make_storage(bool in_a_file, std::size_t size) const
	{
		if (in_a_file)
		{
			return create_mapped_file_image_scratch_storage(
				m_storage_file.get(),
				size
			);
		}

		return create_host_image_scratch_storage(size);
	}

	// A scratch of some locations of the stacks.
	std::shared_ptr<image_scratch> make_scratch(
		const std::vector<image_location> &held,
		std::shared_ptr<image_scratch_storage> storage,
		std::size_t run_length = image_count
	) const
	{
		return std::make_shared<indexed_image_scratch>(
			image_location_grouping(make_span(held)),
			*direct,
			std::move(storage),
			run_length
		);
	}

	// A scratch of some locations of the stacks, in memory or in a file that
	// it allocates itself.
	std::shared_ptr<image_scratch> create_scratch(
		const std::vector<image_location> &held,
		bool in_a_file,
		std::size_t max_size
	) const
	{
		const image_location_grouping grouping(make_span(held));
		if (in_a_file)
		{
			return create_mapped_file_image_scratch(
				grouping,
				*direct,
				m_storage_file.get(),
				image_count,
				max_size
			);
		}

		return create_host_image_scratch(
			grouping,
			*direct,
			image_count,
			max_size
		);
	}

	// A provider that reads the stacks through a scratch of some locations.
	std::shared_ptr<image_reader_provider> make_scratched(
		const std::vector<image_location> &held,
		std::shared_ptr<image_scratch_storage> storage,
		std::size_t run_length = image_count
	) const
	{
		return std::make_shared<scratch_image_reader_provider>(
			direct,
			make_scratch(held, std::move(storage), run_length)
		);
	}

	std::vector<float> read_batch(
		const std::shared_ptr<image_reader_provider> &readers,
		const std::vector<image_location> &locations
	) const
	{
		const executor_image_loader loader(
			readers,
			std::make_shared<synchronous_executor>()
		);
		const std::vector<std::size_t> extents = {
			locations.size(), image_extent, image_extent
		};
		auto destination = make_host_array<float>(
			extents,
			numerical_type::float32,
			untouched
		);

		const auto completion = read_batch_async(
			loader,
			destination.share(),
			make_span(locations)
		);
		REQUIRE_NOTHROW( completion->get() );

		return get_values<float>(destination);
	}

	std::shared_ptr<image_reader_provider> direct;

private:
	void write_stack_file(std::size_t stack)
	{
		const std::vector<std::size_t> extents = {
			image_count, image_extent, image_extent
		};
		auto values = make_host_array<float>(extents, numerical_type::float32);

		auto *data = reinterpret_cast<float*>(values.get_data());
		for (std::size_t i = 0; i < image_count * image_size; ++i)
		{
			data[i] = static_cast<float>(1000 * stack + i);
		}

		write_stack(
			const_array_ref(values),
			get_path(stack),
			*image_file_write_format_selector::get_shared()
		);
	}

	scoped_path m_first;
	scoped_path m_second;
	scoped_path m_third;
	scoped_path m_storage_file;
};

} // anonymous namespace

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a batch read through a scratch holds what the files hold",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	// A batch drawn at random from the three stacks.
	const std::vector<image_location> batch = {
		locate(2, 4), locate(0, 1), locate(1, 5), locate(0, 3),
		locate(2, 0), locate(1, 2), locate(0, 5), locate(2, 2)
	};
	const auto expected = read_batch(direct, batch);

	SECTION( "when the scratch holds every image of the batch" )
	{
		const auto scratched = make_scratched(
			batch,
			make_storage(in_a_file, dataset_bytes)
		);

		// The first pass loads the scratch and the second only reads it.
		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}

	SECTION( "when its buffer has room for only some of them" )
	{
		const auto scratched = make_scratched(
			batch,
			make_storage(in_a_file, 3 * image_bytes)
		);

		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}

	SECTION( "when it loads a run of two images at a time" )
	{
		const auto scratched = make_scratched(
			batch,
			make_storage(in_a_file, dataset_bytes),
			2
		);

		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}

	SECTION( "when it holds other images than those of the batch" )
	{
		const std::vector<image_location> held = {
			locate(0, 0), locate(0, 1), locate(1, 4), locate(2, 2)
		};
		const auto scratched = make_scratched(
			held,
			make_storage(in_a_file, dataset_bytes)
		);

		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"patches read through a scratch hold what the file holds",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	// One patch inside the image, and one over a corner of it.
	const std::size_t patch_extent = 4;
	const auto location = locate(1, 2);
	index_table centres(2);
	const std::array<std::size_t, 2> inside = {4, 4};
	const std::array<std::size_t, 2> corner = {0, 0};
	centres.add(make_span(inside));
	centres.add(make_span(corner));

	const auto read_patches = [&] (
		const std::shared_ptr<image_reader_provider> &readers
	)
	{
		const executor_image_loader loader(
			readers,
			std::make_shared<synchronous_executor>()
		);
		const std::vector<std::size_t> extents = {
			2, patch_extent, patch_extent
		};
		auto destination = make_host_array<float>(
			extents,
			numerical_type::float32,
			untouched
		);

		const auto completion = read_patches_async(
			loader,
			destination.share(),
			location,
			centres
		);
		REQUIRE_NOTHROW( completion->get() );

		return get_values<float>(destination);
	};

	const auto expected = read_patches(direct);
	const auto scratched = make_scratched(
		{location, locate(1, 3)},
		make_storage(in_a_file, dataset_bytes)
	);

	CHECK( read_patches(scratched) == expected );
	CHECK( read_patches(scratched) == expected );
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a whole file read through a scratch holds what the file holds",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	const image_location whole(get_path(1));
	const auto expected = get_values<float>(
		vitrio::read(whole, *direct)
	);

	SECTION( "when the scratch holds the whole file" )
	{
		const auto scratched = make_scratched(
			{whole},
			make_storage(in_a_file, dataset_bytes)
		);

		CHECK( get_values<float>(
			vitrio::read(whole, *scratched)
		) == expected );
		CHECK( get_values<float>(
			vitrio::read(whole, *scratched)
		) == expected );
	}

	SECTION( "when the scratch holds some of its images" )
	{
		const auto scratched = make_scratched(
			{locate(1, 0), locate(1, 4)},
			make_storage(in_a_file, dataset_bytes)
		);

		CHECK( get_values<float>(
			vitrio::read(whole, *scratched)
		) == expected );
	}
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"an image read through a scratch is converted as the file would be",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	const auto location = locate(2, 3);
	const auto expected = get_values<double>(
		vitrio::read(location, *direct, numerical_type::float64)
	);
	const auto scratched = make_scratched(
		{location},
		make_storage(in_a_file, dataset_bytes)
	);

	CHECK( get_values<double>(
		vitrio::read(location, *scratched, numerical_type::float64)
	) == expected );
	CHECK( get_values<double>(
		vitrio::read(location, *scratched, numerical_type::float64)
	) == expected );
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a batch read through a prefetched scratch holds what the files hold",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	const std::vector<image_location> batch = {
		locate(2, 4), locate(0, 1), locate(1, 5), locate(0, 3),
		locate(2, 0), locate(1, 2), locate(0, 5), locate(2, 2)
	};
	const auto expected = read_batch(direct, batch);

	const auto scratch = make_scratch(
		batch,
		make_storage(in_a_file, dataset_bytes),
		2
	);
	const auto scratched =
		std::make_shared<scratch_image_reader_provider>(direct, scratch);

	SECTION( "once the prefetch is done" )
	{
		synchronous_executor executor;
		const auto prefetched = prefetch_scratch_async(
			*scratch,
			direct,
			executor,
			image_location_grouping(make_span(batch))
		);
		REQUIRE_NOTHROW( prefetched->get() );

		CHECK( read_batch(scratched, batch) == expected );
	}

	SECTION( "while the prefetch is under way" )
	{
		thread_pool_executor executor(2);
		const auto prefetched = prefetch_scratch_async(
			*scratch,
			direct,
			executor,
			image_location_grouping(make_span(batch))
		);

		CHECK( read_batch(scratched, batch) == expected );

		REQUIRE_NOTHROW( prefetched->get() );
		CHECK( read_batch(scratched, batch) == expected );
	}
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a batch read through a scratch that allocates its own storage holds "
	"what the files hold",
	"[image_scratch]"
)
{
	const auto in_a_file = GENERATE(false, true);

	const std::vector<image_location> batch = {
		locate(2, 4), locate(0, 1), locate(1, 5), locate(0, 3),
		locate(2, 0), locate(1, 2), locate(0, 5), locate(2, 2)
	};
	const auto expected = read_batch(direct, batch);

	SECTION( "when it allocates what the batch needs" )
	{
		const auto scratched = std::make_shared<scratch_image_reader_provider>(
			direct,
			create_scratch(batch, in_a_file, dataset_bytes)
		);

		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}

	SECTION( "when its maximum size has room for only some of the batch" )
	{
		const auto scratched = std::make_shared<scratch_image_reader_provider>(
			direct,
			create_scratch(batch, in_a_file, 3 * image_bytes)
		);

		CHECK( read_batch(scratched, batch) == expected );
		CHECK( read_batch(scratched, batch) == expected );
	}
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a batch read through a file scratch that an earlier one left holds "
	"what the files hold",
	"[image_scratch]"
)
{
	const std::vector<image_location> batch = {
		locate(2, 4), locate(0, 1), locate(1, 5), locate(0, 3),
		locate(2, 0), locate(1, 2), locate(0, 5), locate(2, 2)
	};
	const auto expected = read_batch(direct, batch);

	// An earlier scratch loads the batch into its file and goes away.
	{
		const auto scratched = std::make_shared<scratch_image_reader_provider>(
			direct,
			create_scratch(batch, true, dataset_bytes)
		);

		REQUIRE( read_batch(scratched, batch) == expected );
	}

	const auto scratched = std::make_shared<scratch_image_reader_provider>(
		direct,
		create_scratch(batch, true, dataset_bytes)
	);

	CHECK( read_batch(scratched, batch) == expected );
	CHECK( read_batch(scratched, batch) == expected );
}

TEST_CASE_METHOD(
	image_scratch_fixture,
	"a batch read through two file scratches that share a file holds what "
	"the files hold",
	"[image_scratch]"
)
{
	const std::vector<image_location> batch = {
		locate(2, 4), locate(0, 1), locate(1, 5), locate(0, 3),
		locate(2, 0), locate(1, 2), locate(0, 5), locate(2, 2)
	};
	const auto expected = read_batch(direct, batch);

	const auto first = std::make_shared<scratch_image_reader_provider>(
		direct,
		create_scratch(batch, true, dataset_bytes)
	);
	REQUIRE( read_batch(first, batch) == expected );

	// A second scratch of the same images, while the first is in use.
	const auto second = std::make_shared<scratch_image_reader_provider>(
		direct,
		create_scratch(batch, true, dataset_bytes)
	);

	CHECK( read_batch(second, batch) == expected );
	CHECK( read_batch(first, batch) == expected );
}
