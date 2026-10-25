// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <indexed_image_scratch_entry.hpp>

#include "fixtures/counting_image_file.hpp"
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_scratch_storage.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/host_image_scratch_storage.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_scratch_open_mode.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <trompeloeil.hpp>
#include <utility>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// A stack of eight images of 2 by 2. Image n holds 4n, 4n + 1, 4n + 2 and
// 4n + 3.
const std::vector<std::size_t> file_extents = {8, 2, 2};
const std::vector<std::size_t> image_extents = {2, 2};
const std::size_t image_size = 4;

const float untouched = -1.0F;

// The stack on disk, and a reader over it.
class stack_file
{
public:
	explicit stack_file(const std::string &name)
		: m_path(name)
	{
		write_counting_image_file(m_path.get(), file_extents);
		m_reader = open_counting_image_file(m_path.get(), file_extents, 2);
	}

	const std::shared_ptr<const image_reader>& get_reader() const noexcept
	{
		return m_reader;
	}

private:
	scoped_path m_path;
	std::shared_ptr<const image_reader> m_reader;
};

// When the stack was last modified, as far as the entries are told.
const std::uint64_t stack_time = 1700000000;

std::size_t count_runs(std::size_t index_count, std::size_t run_length)
{
	return index_count == 0 ? 0 : (index_count - 1) / run_length + 1;
}

// Where the values of an entry begin, after a flag per run.
std::size_t locate_values(std::size_t run_count)
{
	return (run_count * sizeof(std::uint64_t) + 63) / 64 * 64;
}

// Storage laid out as a scratch lays out an entry: a flag per run, then the
// images the entry holds. Every flag starts as `flag`, and every value as
// `value`.
class entry_storage
{
public:
	entry_storage(
		std::size_t index_count,
		std::size_t run_count,
		float value = 0.0F,
		std::uint64_t flag = 0
	)
		: m_storage(
			create_host_image_scratch_storage(
				locate_values(run_count) +
				std::max<std::size_t>(index_count, 1) *
					image_size * sizeof(float)
			)
		)
		, m_flags({run_count}, {1}, 0, numerical_type::uint64)
		, m_values(
			{index_count, 2, 2},
			{4, 2, 1},
			static_cast<std::ptrdiff_t>(
				locate_values(run_count) / sizeof(float)
			),
			numerical_type::float32
		)
	{
		const std::vector<std::uint64_t> flags(run_count, flag);
		m_storage->write(0, bytes_of(flags));

		const std::vector<float> values(index_count * image_size, value);
		m_storage->write(locate_values(run_count), bytes_of(values));
	}

	std::shared_ptr<image_scratch_storage> get_storage() const noexcept
	{
		return m_storage;
	}

	const array_descriptor& get_flags() const noexcept
	{
		return m_flags;
	}

	const array_descriptor& get_values() const noexcept
	{
		return m_values;
	}

	std::vector<std::uint64_t> read_flags() const
	{
		std::vector<std::uint64_t> flags(m_flags.get_extents().front());
		m_storage->read(
			0,
			make_span(
				reinterpret_cast<vitrio::byte*>(flags.data()),
				flags.size() * sizeof(std::uint64_t)
			)
		);
		return flags;
	}

	void write_flag(std::size_t run, std::uint64_t value)
	{
		const std::vector<std::uint64_t> flag(1, value);
		m_storage->write(run * sizeof(std::uint64_t), bytes_of(flag));
	}

private:
	template <typename T>
	static span<const vitrio::byte> bytes_of(const std::vector<T> &values)
	{
		return make_span(
			reinterpret_cast<const vitrio::byte*>(values.data()),
			values.size() * sizeof(T)
		);
	}

	std::shared_ptr<image_scratch_storage> m_storage;
	array_descriptor m_flags;
	array_descriptor m_values;
};

// An entry that holds some images of the stack, with nothing loaded.
std::shared_ptr<indexed_image_scratch_entry> make_entry(
	std::vector<std::size_t> indices,
	std::size_t run_length
)
{
	const entry_storage storage(
		indices.size(),
		count_runs(indices.size(), run_length)
	);

	return std::make_shared<indexed_image_scratch_entry>(
		std::move(indices),
		storage.get_storage(),
		storage.get_values(),
		storage.get_flags(),
		stack_time,
		run_length,
		image_scratch_open_mode::empty
	);
}

// An entry that holds the first four images of the stack in runs of two,
// over storage that the caller keeps.
std::shared_ptr<indexed_image_scratch_entry> make_entry_over(
	const entry_storage &storage,
	std::uint64_t modification_time,
	image_scratch_open_mode mode
)
{
	return std::make_shared<indexed_image_scratch_entry>(
		std::vector<std::size_t>({0, 1, 2, 3}),
		storage.get_storage(),
		storage.get_values(),
		storage.get_flags(),
		modification_time,
		2,
		mode
	);
}

// An entry of some indices, over `storage` as `values` and `flags` read it.
std::shared_ptr<indexed_image_scratch_entry> make_entry_of(
	std::vector<std::size_t> indices,
	std::shared_ptr<image_scratch_storage> storage,
	array_descriptor values,
	array_descriptor flags,
	std::size_t run_length = 1
)
{
	return std::make_shared<indexed_image_scratch_entry>(
		std::move(indices),
		std::move(storage),
		std::move(values),
		std::move(flags),
		stack_time,
		run_length,
		image_scratch_open_mode::empty
	);
}

// A batch of `count` images, every element set to `untouched`.
array make_batch(std::size_t count)
{
	return make_host_array<float>(
		{count, 2, 2},
		numerical_type::float32,
		untouched
	);
}

// Image `indices[i]` of the stack, landing in slot `i` of a batch.
image_transfer_plan images(const std::vector<std::size_t> &indices)
{
	image_transfer_plan plan(image_transfer_shape(image_extents, 3, 3));
	for (std::size_t slot = 0; slot < indices.size(); ++slot)
	{
		const std::array<std::size_t, 3> file_offset = {indices[slot], 0, 0};
		const std::array<std::size_t, 3> array_offset = {slot, 0, 0};
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

// `count` consecutive images from `first`, as one region at the start of a
// batch.
image_transfer_plan image_range(std::size_t first, std::size_t count)
{
	image_transfer_plan plan(image_transfer_shape({count, 2, 2}, 3, 3));
	const std::array<std::size_t, 3> file_offset = {first, 0, 0};
	const std::array<std::size_t, 3> array_offset = {0, 0, 0};
	plan.add(make_span(file_offset), make_span(array_offset));

	return plan;
}

std::vector<std::size_t> get_file_indices(const image_transfer_plan &plan)
{
	std::vector<std::size_t> indices;
	for (std::size_t region = 0; region < plan.get_region_count(); ++region)
	{
		indices.push_back(plan.get_file_offset(region).front());
	}

	return indices;
}

std::vector<std::size_t> get_array_slots(const image_transfer_plan &plan)
{
	std::vector<std::size_t> slots;
	for (std::size_t region = 0; region < plan.get_region_count(); ++region)
	{
		slots.push_back(plan.get_array_offset(region).front());
	}

	return slots;
}

// The values of a batch that holds the given images of the stack in order.
std::vector<float> values_of_images(const std::vector<std::size_t> &indices)
{
	std::vector<float> values;
	for (const auto index : indices)
	{
		const auto image = count_from(index * image_size, image_size);
		values.insert(values.end(), image.begin(), image.end());
	}

	return values;
}

using index_list = std::vector<std::size_t>;

} // anonymous namespace

TEST_CASE(
	"an indexed_image_scratch_entry needs indices that are strictly ascending",
	"[indexed_image_scratch_entry]"
)
{
	const entry_storage storage(3, 3);

	SECTION( "indices that descend are refused" )
	{
		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({4, 1, 5}),
				storage.get_storage(),
				storage.get_values(),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}

	SECTION( "an index given twice is refused" )
	{
		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 4}),
				storage.get_storage(),
				storage.get_values(),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry needs a storage",
	"[indexed_image_scratch_entry]"
)
{
	const entry_storage storage(3, 3);

	REQUIRE_THROWS_AS(
		make_entry_of(
			index_list({1, 4, 5}),
			nullptr,
			storage.get_values(),
			storage.get_flags()
		),
		std::invalid_argument
	);
}

TEST_CASE(
	"an indexed_image_scratch_entry needs values that match its indices",
	"[indexed_image_scratch_entry]"
)
{
	SECTION( "values of another number of indices are refused" )
	{
		const entry_storage storage(2, 3);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				storage.get_values(),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}

	SECTION( "values that are not initialized are refused" )
	{
		const entry_storage storage(3, 3);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				array_descriptor(),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}

	SECTION( "values with a negative stride are refused" )
	{
		const entry_storage storage(3, 3);
		const auto &values = storage.get_values();
		const auto last_image =
			values.get_offset() + 2 * static_cast<std::ptrdiff_t>(image_size);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				array_descriptor(
					{3, 2, 2},
					{-4, 2, 1},
					last_image,
					numerical_type::float32
				),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}

	SECTION( "values outside the storage are refused" )
	{
		const entry_storage storage(3, 3);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				array_descriptor(
					{3, 2, 2},
					{4, 2, 1},
					storage.get_values().get_offset() + 1,
					numerical_type::float32
				),
				storage.get_flags()
			),
			std::out_of_range
		);
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry refuses a run of no slot",
	"[indexed_image_scratch_entry]"
)
{
	const entry_storage storage(3, 3);

	REQUIRE_THROWS_AS(
		make_entry_of(
			index_list({1, 4, 5}),
			storage.get_storage(),
			storage.get_values(),
			storage.get_flags(),
			0
		),
		std::invalid_argument
	);
}

TEST_CASE(
	"an indexed_image_scratch_entry needs one flag per run",
	"[indexed_image_scratch_entry]"
)
{
	SECTION( "flags of another number of runs are refused" )
	{
		const entry_storage storage(3, 2);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				storage.get_values(),
				storage.get_flags()
			),
			std::invalid_argument
		);
	}

	SECTION( "flags of another data type are refused" )
	{
		const entry_storage storage(3, 3);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				storage.get_values(),
				array_descriptor({3}, {1}, 0, numerical_type::float32)
			),
			std::invalid_argument
		);
	}

	SECTION( "flags that are not initialized are refused" )
	{
		const entry_storage storage(3, 3);

		REQUIRE_THROWS_AS(
			make_entry_of(
				index_list({1, 4, 5}),
				storage.get_storage(),
				storage.get_values(),
				array_descriptor()
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry flags the runs it loads with the "
	"modification time of its file",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_flags.raw");
	const entry_storage storage(4, 2);
	const auto entry = make_entry_over(
		storage,
		stack_time,
		image_scratch_open_mode::empty
	);

	store_now(*entry, stack.get_reader(), images({3}));

	// The second run holds image 3, and the first is not loaded.
	CHECK( storage.read_flags() ==
		std::vector<std::uint64_t>({0, stack_time}) );
}

TEST_CASE(
	"an empty indexed_image_scratch_entry clears its flags",
	"[indexed_image_scratch_entry]"
)
{
	const entry_storage storage(4, 2, 7.0F, stack_time);
	const auto entry = make_entry_over(
		storage,
		stack_time,
		image_scratch_open_mode::empty
	);
	auto destination = make_batch(1);

	const auto missing = read_held_now(*entry, destination, images({0}));

	CHECK( missing.get_region_count() == 1 );
	CHECK( storage.read_flags() == std::vector<std::uint64_t>({0, 0}) );
}

TEST_CASE(
	"a resumed indexed_image_scratch_entry has loaded the runs whose flag "
	"holds the modification time of its file",
	"[indexed_image_scratch_entry]"
)
{
	// What an earlier entry left: the first run loaded, the second loaded
	// from an older version of the file.
	entry_storage storage(4, 2, 7.0F);
	storage.write_flag(0, stack_time);
	storage.write_flag(1, stack_time - 1);

	SECTION( "a run flagged with that time is read without being loaded" )
	{
		const auto entry = make_entry_over(
			storage,
			stack_time,
			image_scratch_open_mode::resumed
		);
		auto destination = make_batch(1);

		const auto missing = read_held_now(*entry, destination, images({1}));

		CHECK( missing.get_region_count() == 0 );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(image_size, 7.0F) );
	}

	SECTION( "a run flagged with another time is not loaded" )
	{
		const auto entry = make_entry_over(
			storage,
			stack_time,
			image_scratch_open_mode::resumed
		);
		auto destination = make_batch(1);

		const auto missing = read_held_now(*entry, destination, images({2}));

		CHECK( missing.get_region_count() == 1 );
	}

	SECTION( "the flags are left as they were" )
	{
		const auto entry = make_entry_over(
			storage,
			stack_time,
			image_scratch_open_mode::resumed
		);

		CHECK( storage.read_flags() ==
			std::vector<std::uint64_t>({stack_time, stack_time - 1}) );
	}

	SECTION( "no run is loaded when the time of the file is not known" )
	{
		storage.write_flag(0, 0);
		storage.write_flag(1, 0);
		const auto entry = make_entry_over(
			storage,
			0,
			image_scratch_open_mode::resumed
		);
		auto destination = make_batch(2);

		const auto missing =
			read_held_now(*entry, destination, images({1, 2}));

		CHECK( missing.get_region_count() == 2 );
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry that is reset has nothing loaded",
	"[indexed_image_scratch_entry]"
)
{
	const entry_storage storage(4, 2, 7.0F, stack_time);
	const auto entry = make_entry_over(
		storage,
		stack_time,
		image_scratch_open_mode::resumed
	);
	auto destination = make_batch(1);

	entry->reset();
	const auto missing = read_held_now(*entry, destination, images({1}));

	CHECK( missing.get_region_count() == 1 );
	CHECK( storage.read_flags() == std::vector<std::uint64_t>({0, 0}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry reads nothing before it is loaded",
	"[indexed_image_scratch_entry]"
)
{
	const auto entry = make_entry({0, 1, 2, 3, 4, 5, 6, 7}, 4);
	auto destination = make_batch(2);

	const auto missing = read_held_now(*entry, destination, images({3, 5}));

	CHECK( get_file_indices(missing) == index_list({3, 5}) );
	CHECK( get_array_slots(missing) == index_list({0, 1}) );
	CHECK( get_values<float>(destination) ==
		std::vector<float>(2 * image_size, untouched) );
}

TEST_CASE(
	"an indexed_image_scratch_entry loads the run of a stored image",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_run.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3, 4, 5, 6, 7}, 4);

	// One read of the file, for the four images of the second run.
	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( get_file_indices(_2) == index_list({4, 5, 6, 7}) )
		.LR_WITH( get_array_slots(_2) == index_list({4, 5, 6, 7}) )
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );

	store_now(*entry, file, images({5}));

	SECTION( "the stored image is read from the entry" )
	{
		auto destination = make_batch(1);

		const auto missing = read_held_now(*entry, destination, images({5}));

		CHECK( missing.get_region_count() == 0 );
		CHECK( get_values<float>(destination) == values_of_images({5}) );
	}

	SECTION( "so are the other images of its run" )
	{
		auto destination = make_batch(2);

		const auto missing =
			read_held_now(*entry, destination, images({7, 4}));

		CHECK( missing.get_region_count() == 0 );
		CHECK( get_values<float>(destination) == values_of_images({7, 4}) );
	}

	SECTION( "the images of other runs are still missing" )
	{
		auto destination = make_batch(2);

		const auto missing =
			read_held_now(*entry, destination, images({1, 6}));

		CHECK( get_file_indices(missing) == index_list({1}) );
		CHECK( get_array_slots(missing) == index_list({0}) );
	}

	SECTION( "storing an image of the same run does not read the file" )
	{
		store_now(*entry, file, images({6, 4}));
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry splits its images into runs of one length",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_runs.raw");
	const auto file = std::make_shared<mock_image_reader>();

	// Eight images in runs of three: two whole runs and one of what is left.
	const auto entry = make_entry({0, 1, 2, 3, 4, 5, 6, 7}, 3);

	SECTION( "the last run has the images that are left" )
	{
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH( get_file_indices(_2) == index_list({6, 7}) )
			.LR_WITH( get_array_slots(_2) == index_list({6, 7}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);

		store_now(*entry, file, images({7}));
	}

	SECTION( "a region across two runs loads both" )
	{
		trompeloeil::sequence order;
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.IN_SEQUENCE(order)
			.LR_WITH( get_file_indices(_2) == index_list({0, 1, 2}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.IN_SEQUENCE(order)
			.LR_WITH( get_file_indices(_2) == index_list({3, 4, 5}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);

		store_now(*entry, file, image_range(2, 2));
	}

	SECTION( "a region across two runs is missing until both are loaded" )
	{
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.TIMES(2)
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		auto destination = make_batch(2);

		store_now(*entry, file, images({2}));
		CHECK( read_held_now(*entry, destination, image_range(2, 2))
			.get_region_count() == 1 );

		store_now(*entry, file, images({3}));
		CHECK( read_held_now(*entry, destination, image_range(2, 2))
			.get_region_count() == 0 );
		CHECK( get_values<float>(destination) == values_of_images({2, 3}) );
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry loads all its images at once when a run is "
	"long enough",
	"[indexed_image_scratch_entry]"
)
{
	const auto run_length = GENERATE(
		std::size_t(4),
		std::size_t(9),
		std::numeric_limits<std::size_t>::max()
	);
	const stack_file stack("indexed_scratch_entry_one_run.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, run_length);

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( get_file_indices(_2) == index_list({0, 1, 2, 3}) )
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );

	store_now(*entry, file, images({2}));
}

TEST_CASE(
	"an indexed_image_scratch_entry loads only the images it holds",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_subset.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({1, 4, 5}, 3);

	SECTION( "storing a held image loads the held images of its run" )
	{
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH( get_file_indices(_2) == index_list({1, 4, 5}) )
			.LR_WITH( get_array_slots(_2) == index_list({0, 1, 2}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		auto destination = make_batch(3);

		store_now(*entry, file, images({4}));
		const auto missing =
			read_held_now(*entry, destination, images({4, 2, 1}));

		// Image 2 is not held: it stays missing and its slot untouched.
		auto expected = values_of_images({4, 0, 1});
		std::fill_n(expected.begin() + image_size, image_size, untouched);

		CHECK( get_file_indices(missing) == index_list({2}) );
		CHECK( get_array_slots(missing) == index_list({1}) );
		CHECK( get_values<float>(destination) == expected );
	}

	SECTION( "storing a region loads the held images among the ones it spans" )
	{
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH( get_file_indices(_2) == index_list({1, 4, 5}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		auto destination = make_batch(6);

		store_now(*entry, file, image_range(0, 6));
		const auto missing =
			read_held_now(*entry, destination, image_range(0, 6));

		// Images 0, 2 and 3 are not held, so the region is not read.
		CHECK( get_file_indices(missing) == index_list({0}) );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(6 * image_size, untouched) );
	}

	SECTION( "storing images that are not held reads nothing" )
	{
		FORBID_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		);

		store_now(*entry, file, images({0, 2, 3, 7}));
	}

	SECTION( "storing a region between held images reads nothing" )
	{
		FORBID_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		);

		store_now(*entry, file, image_range(2, 2));
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry takes a region that reaches past the "
	"largest index there is",
	"[indexed_image_scratch_entry]"
)
{
	const std::size_t max_index = std::numeric_limits<std::size_t>::max();
	const stack_file stack("indexed_scratch_entry_overflow.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({1, 4, 5}, 1);

	SECTION( "storing it loads the images held from its first index on" )
	{
		trompeloeil::sequence order;
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.IN_SEQUENCE(order)
			.LR_WITH( get_file_indices(_2) == index_list({4}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.IN_SEQUENCE(order)
			.LR_WITH( get_file_indices(_2) == index_list({5}) )
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);

		store_now(*entry, file, image_range(4, max_index));
	}

	SECTION( "storing it reads nothing when none of them is held" )
	{
		FORBID_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		);

		store_now(*entry, file, image_range(max_index, max_index));
	}

	SECTION( "reading it answers it" )
	{
		auto destination = make_batch(2);

		const auto missing =
			read_held_now(*entry, destination, image_range(4, max_index));

		CHECK( get_file_indices(missing) == index_list({4}) );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(2 * image_size, untouched) );
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry reads a region that spans several images",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_range.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({2, 3, 4}, 3);

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );
	auto destination = make_batch(2);

	SECTION( "when all of them are held" )
	{
		store_now(*entry, file, image_range(3, 2));
		const auto missing =
			read_held_now(*entry, destination, image_range(3, 2));

		CHECK( missing.get_region_count() == 0 );
		CHECK( get_values<float>(destination) == values_of_images({3, 4}) );
	}

	SECTION( "and not when the last of them is not" )
	{
		// The held part is still loaded: images 4 and 5, of which 4 is held.
		store_now(*entry, file, image_range(4, 2));
		const auto missing =
			read_held_now(*entry, destination, image_range(4, 2));

		CHECK( get_file_indices(missing) == index_list({4}) );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(2 * image_size, untouched) );
	}

	SECTION( "nor when the first of them is not" )
	{
		store_now(*entry, file, image_range(1, 2));
		const auto missing =
			read_held_now(*entry, destination, image_range(1, 2));

		CHECK( get_file_indices(missing) == index_list({1}) );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(2 * image_size, untouched) );
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry loads every run a stored region spans",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_whole.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3, 4, 5, 6, 7}, 3);

	// Eight slots in runs of three: three reads.
	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.TIMES(3)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );
	auto destination = make_batch(8);

	store_now(*entry, file, image_range(0, 8));
	const auto missing =
		read_held_now(*entry, destination, image_range(0, 8));

	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<float>(destination) ==
		count_from(0, 8 * image_size) );
}

TEST_CASE(
	"an indexed_image_scratch_entry reads a patch of a held image",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_patch.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({5}, 1);

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );

	// The second row of image 5, into an array of one row.
	image_transfer_plan patch(image_transfer_shape({1, 2}, 3, 2));
	const std::array<std::size_t, 3> file_offset = {5, 1, 0};
	const std::array<std::size_t, 2> array_offset = {0, 0};
	patch.add(make_span(file_offset), make_span(array_offset));
	auto destination =
		make_host_array<float>({1, 2}, numerical_type::float32, untouched);

	store_now(*entry, file, patch);
	const auto missing = read_held_now(*entry, destination, patch);

	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<float>(destination) == std::vector<float>({22, 23}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry converts to the type of the destination",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_convert.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({5}, 1);

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );
	auto destination =
		make_host_array<double>({1, 2, 2}, numerical_type::float64, -1.0);

	store_now(*entry, file, images({5}));
	const auto missing = read_held_now(*entry, destination, images({5}));

	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<double>(destination) ==
		std::vector<double>({20, 21, 22, 23}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry passes over a plan of another rank",
	"[indexed_image_scratch_entry]"
)
{
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3, 4, 5, 6, 7}, 8);

	// A plan for a file of two axes, where the stack has three.
	image_transfer_plan plan(image_transfer_shape(image_extents, 2, 3));
	const std::array<std::size_t, 2> file_offset = {0, 0};
	const std::array<std::size_t, 3> array_offset = {0, 0, 0};
	plan.add(make_span(file_offset), make_span(array_offset));
	auto destination = make_batch(1);

	SECTION( "storing it reads nothing" )
	{
		FORBID_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		);

		store_now(*entry, file, plan);
	}

	SECTION( "reading it answers every region" )
	{
		const auto missing = read_held_now(*entry, destination, plan);

		CHECK( missing.get_region_count() == 1 );
		CHECK( get_values<float>(destination) ==
			std::vector<float>(image_size, untouched) );
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry leaves a run absent when loading it fails",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_retry.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);
	auto destination = make_batch(1);
	trompeloeil::sequence order;

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.IN_SEQUENCE(order)
		.SIDE_EFFECT( _3->notify_done(
			std::make_exception_ptr(std::runtime_error("from the file"))
		) );
	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.IN_SEQUENCE(order)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );

	REQUIRE_THROWS_AS(
		store_now(*entry, file, images({2})),
		std::runtime_error
	);
	CHECK( read_held_now(*entry, destination, images({2}))
		.get_region_count() == 1 );

	// The next store reads the file again.
	store_now(*entry, file, images({2}));
	const auto missing = read_held_now(*entry, destination, images({2}));

	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<float>(destination) == values_of_images({2}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry loads a run once for two threads",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_threads.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);

	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.TIMES(1)
		.LR_SIDE_EFFECT( stack.get_reader()->read_async(_1.share(), _2, _3) );

	std::thread first([&] { store_now(*entry, file, images({1})); });
	std::thread second([&] { store_now(*entry, file, images({3})); });
	first.join();
	second.join();

	auto destination = make_batch(2);
	const auto missing = read_held_now(*entry, destination, images({1, 3}));

	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<float>(destination) == values_of_images({1, 3}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry checks the destination of a read",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_destination.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);

	SECTION( "a destination that is not initialized is refused" )
	{
		store_now(*entry, stack.get_reader(), images({1}));
		array destination;

		REQUIRE_THROWS_AS(
			read_held_now(*entry, destination, images({1})),
			std::invalid_argument
		);
	}

	SECTION( "a held image that does not fit the destination is refused" )
	{
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_SIDE_EFFECT(
				stack.get_reader()->read_async(_1.share(), _2, _3)
			);
		auto destination = make_batch(1);

		store_now(*entry, file, images({0, 1}));

		// The second image lands in a slot the batch does not have.
		REQUIRE_THROWS_AS(
			read_held_now(*entry, destination, images({0, 1})),
			std::out_of_range
		);
	}
}

TEST_CASE(
	"an indexed_image_scratch_entry has its storage serve what it holds, "
	"from the slots it holds it in",
	"[indexed_image_scratch_entry]"
)
{
	// The entry holds images 2, 5 and 7 of the stack, a run each.
	const auto storage = std::make_shared<mock_image_scratch_storage>();
	ALLOW_CALL(*storage, get_size()).RETURN(std::size_t{1} << 20);
	ALLOW_CALL(*storage, write(trompeloeil::_, trompeloeil::_));
	const array_descriptor values(
		{3, 2, 2},
		{4, 2, 1},
		16,
		numerical_type::float32
	);
	const array_descriptor flags({3}, {1}, 0, numerical_type::uint64);
	const auto entry = std::make_shared<indexed_image_scratch_entry>(
		index_list({2, 5, 7}),
		storage,
		values,
		flags,
		stack_time,
		1,
		image_scratch_open_mode::empty
	);

	// The run of image 5, the second slot, is taken in through the storage.
	const auto file = std::make_shared<mock_image_reader>();
	{
		REQUIRE_CALL(
			*storage,
			load_async(trompeloeil::_, values, trompeloeil::_, trompeloeil::_)
		)
			.WITH(
				_3.get_region_count() == 1 &&
				_3.get_file_offset(0)[0] == 5 &&
				_3.get_array_offset(0)[0] == 1
			)
			.SIDE_EFFECT( _4->notify_done() );
		store_now(*entry, file, images({5}));
	}

	// Of images 7 and 5, only 5 is held, which the storage serves from its
	// slot into the second slot of the batch.
	REQUIRE_CALL(
		*storage,
		serve_async(values, trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.WITH(
			_3.get_region_count() == 1 &&
			_3.get_file_offset(0)[0] == 1 &&
			_3.get_array_offset(0)[0] == 1
		)
		.SIDE_EFFECT( _4->notify_done() );
	auto destination = make_batch(2);
	const auto missing = read_held_now(*entry, destination, images({7, 5}));

	REQUIRE( missing.get_region_count() == 1 );
	CHECK( missing.get_file_offset(0)[0] == 7 );
	CHECK( missing.get_array_offset(0)[0] == 0 );
}

TEST_CASE(
	"an indexed_image_scratch_entry refuses a null notifier",
	"[indexed_image_scratch_entry]"
)
{
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);
	FORBID_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	);

	CHECK_THROWS_AS(
		entry->store_async(file, images({1}), nullptr),
		std::invalid_argument
	);
}

TEST_CASE(
	"an indexed_image_scratch_entry reports at once a store of what it holds",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_held.raw");
	const auto entry = make_entry({0, 1, 2, 3}, 2);
	store_now(*entry, stack.get_reader(), images({0, 1}));

	const auto file = std::make_shared<mock_image_reader>();
	FORBID_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	);

	const auto done = std::make_shared<counting_completion>(1);
	entry->store_async(file, images({1, 0}), done);
	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
}

TEST_CASE(
	"an indexed_image_scratch_entry has a store that asks for a run being "
	"loaded wait for it, rather than read it again",
	"[indexed_image_scratch_entry]"
)
{
	const stack_file stack("indexed_scratch_entry_joined.raw");
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);

	// The read of the run is held back until the case lets it go.
	std::shared_ptr<completion_notifier> loaded;
	std::unique_ptr<array> run_values;
	std::unique_ptr<image_transfer_plan> run_regions;
	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.TIMES(1)
		.LR_SIDE_EFFECT(
			run_values = std::make_unique<array>(_1.share());
			run_regions = std::make_unique<image_transfer_plan>(_2);
			loaded = _3
		);

	const auto first = std::make_shared<counting_completion>(1);
	const auto second = std::make_shared<counting_completion>(1);
	entry->store_async(file, images({1}), first);
	entry->store_async(file, images({3}), second);
	CHECK_FALSE( first->is_ready() );
	CHECK_FALSE( second->is_ready() );

	REQUIRE( loaded != nullptr );
	stack.get_reader()->read_async(
		std::move(*run_values),
		*run_regions,
		std::move(loaded)
	);
	REQUIRE( first->is_ready() );
	REQUIRE( second->is_ready() );
	CHECK_NOTHROW( first->get() );
	CHECK_NOTHROW( second->get() );

	auto destination = make_batch(2);
	const auto missing = read_held_now(*entry, destination, images({1, 3}));
	CHECK( missing.get_region_count() == 0 );
	CHECK( get_values<float>(destination) == values_of_images({1, 3}) );
}

TEST_CASE(
	"an indexed_image_scratch_entry reports a run that fails to load to "
	"every store waiting for it",
	"[indexed_image_scratch_entry]"
)
{
	const auto file = std::make_shared<mock_image_reader>();
	const auto entry = make_entry({0, 1, 2, 3}, 4);

	std::shared_ptr<completion_notifier> loaded;
	REQUIRE_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.TIMES(1)
		.LR_SIDE_EFFECT( loaded = _3 );

	const auto first = std::make_shared<counting_completion>(1);
	const auto second = std::make_shared<counting_completion>(1);
	entry->store_async(file, images({1}), first);
	entry->store_async(file, images({2}), second);

	REQUIRE( loaded != nullptr );
	loaded->notify_done(
		std::make_exception_ptr(std::runtime_error("from the file"))
	);
	REQUIRE( first->is_ready() );
	REQUIRE( second->is_ready() );
	CHECK_THROWS_AS( first->get(), std::runtime_error );
	CHECK_THROWS_AS( second->get(), std::runtime_error );

	auto destination = make_batch(1);
	CHECK( read_held_now(*entry, destination, images({1}))
		.get_region_count() == 1 );
}

TEST_CASE(
	"an indexed_image_scratch_entry leaves a run absent when its flag can not "
	"be written, and reports why",
	"[indexed_image_scratch_entry]"
)
{
	const auto storage = std::make_shared<mock_image_scratch_storage>();
	ALLOW_CALL(*storage, get_size()).RETURN(std::size_t{1} << 20);
	ALLOW_CALL(*storage, write(trompeloeil::_, trompeloeil::_));
	const array_descriptor values(
		{2, 2, 2},
		{4, 2, 1},
		16,
		numerical_type::float32
	);
	const array_descriptor flags({2}, {1}, 0, numerical_type::uint64);
	const auto entry = std::make_shared<indexed_image_scratch_entry>(
		index_list({3, 4}),
		storage,
		values,
		flags,
		stack_time,
		1,
		image_scratch_open_mode::empty
	);

	// The flag of the second run, the one of image 4, is not written.
	REQUIRE_CALL(*storage, write(sizeof(std::uint64_t), trompeloeil::_))
		.THROW( image_file_error("flag") );
	REQUIRE_CALL(
		*storage,
		load_async(trompeloeil::_, values, trompeloeil::_, trompeloeil::_)
	)
		.SIDE_EFFECT( _4->notify_done() );
	const auto file = std::make_shared<mock_image_reader>();

	CHECK_THROWS_AS(store_now(*entry, file, images({4})), image_file_error);

	FORBID_CALL(
		*storage,
		serve_async(
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_
		)
	);
	auto destination = make_batch(1);
	const auto missing = read_held_now(*entry, destination, images({4}));
	CHECK( missing.get_region_count() == 1 );
}
