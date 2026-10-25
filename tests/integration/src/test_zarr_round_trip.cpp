// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/image_reader.hpp>

#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <boost/filesystem/operations.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

using extents = std::vector<std::size_t>;

// A store under the build tree, removed with all it holds when a case
// leaves it.
class scoped_store
{
public:
	explicit scoped_store(const std::string &name)
		: m_path(scoped_path(name).get())
	{
		boost::filesystem::remove_all(m_path);
	}

	scoped_store(const scoped_store &other) = delete;
	scoped_store& operator=(const scoped_store &other) = delete;

	~scoped_store()
	{
		boost::system::error_code ignored;
		boost::filesystem::remove_all(m_path, ignored);
	}

	const std::string& get() const noexcept
	{
		return m_path;
	}

private:
	std::string m_path;
};

template <typename T>
std::vector<T> counting(std::size_t count)
{
	std::vector<T> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<T>(p % 1000);
	}
	return values;
}

template <typename T>
void write_store(
	const std::string &path,
	const image_descriptor &descriptor,
	const image_transfer_plan &regions,
	const extents &source_shape,
	numerical_type source_type,
	const std::vector<T> &values
)
{
	image_file_write_format_selector writers;
	writers.register_builtin_formats();

	const auto writer = writers.open(
		path, descriptor, image_metadata(), *make_file_transfer_pool());
	write_now(*writer, make_host_array<T>(source_shape, source_type, values),
		regions);
	flush_now(*writer);
}

} // anonymous namespace

TEST_CASE( "a ZARR store written through the selectors reads back",
	"[zarr][image_file_format_selector]" )
{
	const auto readers = std::make_shared<file_image_reader_provider>(
		image_file_read_format_selector::get_shared(),
		make_file_transfer_pool()
	);

	SECTION( "the write format is chosen by the extension" )
	{
		image_file_write_format_selector writers;
		writers.register_builtin_formats();
		const auto *format = writers.get_most_suitable_format(
			image_file_probe("absent.zarr/"));
		REQUIRE( format != nullptr );
		REQUIRE( format->get_name() == "ZARR" );
	}

	SECTION( "a volume, whole and in a patch across its chunks" )
	{
		const scoped_store store("round_trip_volume.zarr");
		const extents shape = {130, 20, 140};
		const image_descriptor descriptor(
			make_span(shape), 3, numerical_type::float32);
		const auto values = counting<float>(130 * 20 * 140);
		write_store(store.get(), descriptor, whole_of(shape), shape,
			numerical_type::float32, values);

		REQUIRE( boost::filesystem::exists(store.get() + "/zarr.json") );
		REQUIRE( boost::filesystem::exists(store.get() + "/0/c/1/0/1") );
		const auto reader = readers->acquire(store.get());
		REQUIRE( reader->get_descriptor() == descriptor );
		auto destination = make_host_array<float>(
			shape, numerical_type::float32);
		read_now(*reader, destination, whole_of(shape));
		REQUIRE( get_values<float>(destination) == values );

		// A patch of 4 by 4 by 4 about the corner of eight chunks.
		image_transfer_plan patch(image_transfer_shape({4, 4, 4}, 3, 3));
		patch.add(make_span(extents{126, 2, 126}), make_span(extents{0, 0, 0}));
		auto read_patch = make_host_array<float>(
			{4, 4, 4}, numerical_type::float32);
		read_now(*reader, read_patch, patch);
		const auto patch_values = get_values<float>(read_patch);
		REQUIRE( patch_values[0] == values[(126 * 20 + 2) * 140 + 126] );
		REQUIRE( patch_values[63] == values[(129 * 20 + 5) * 140 + 129] );
	}

	SECTION( "a stack, of which one image is read by its index" )
	{
		const scoped_store store("round_trip_stack.zarr");
		const extents shape = {5, 30, 40};
		const image_descriptor descriptor(
			make_span(shape), 2, numerical_type::uint16);
		const auto values = counting<std::uint16_t>(5 * 30 * 40);
		write_store(store.get(), descriptor, whole_of(shape), shape,
			numerical_type::uint16, values);

		REQUIRE( readers->acquire(store.get())->get_descriptor() ==
			descriptor );
		const auto image = vitrio::read(
			image_location(store.get(), 3), *readers);
		REQUIRE( get_values<std::uint16_t>(image) ==
			std::vector<std::uint16_t>(
				values.begin() + 3 * 1200, values.begin() + 4 * 1200) );
	}

	SECTION( "a store written again replaces what it held" )
	{
		const scoped_store store("round_trip_replaced.zarr");
		const extents shape = {4, 4};
		const image_descriptor descriptor(
			make_span(shape), 2, numerical_type::int8);
		write_store(store.get(), descriptor, whole_of(shape), shape,
			numerical_type::int8, std::vector<std::int8_t>(16, 5));

		const extents smaller = {2, 3};
		write_store(store.get(),
			image_descriptor(make_span(smaller), 2, numerical_type::int8),
			whole_of(smaller), smaller, numerical_type::int8,
			std::vector<std::int8_t>(6, -1));

		REQUIRE( readers->acquire(store.get())->get_descriptor() ==
			image_descriptor(make_span(smaller), 2, numerical_type::int8) );
		REQUIRE( get_values<std::int8_t>(vitrio::read(
			image_location(store.get()), *readers)) ==
			std::vector<std::int8_t>(6, -1) );
	}
}
