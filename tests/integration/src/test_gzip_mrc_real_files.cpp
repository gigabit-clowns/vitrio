// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>

#include <array>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

std::vector<std::size_t> shape_of(const image_reader &reader)
{
	const auto extents = reader.get_descriptor().get_extents();
	return std::vector<std::size_t>(extents.begin(), extents.end());
}

// Read a box of a file: from an origin, of some extents, into an array of
// those extents.
std::vector<float> read_box(
	const image_reader &reader,
	const std::vector<std::size_t> &origin,
	const std::vector<std::size_t> &extents
)
{
	auto destination = make_host_array<float>(extents, numerical_type::float32);

	image_transfer_plan regions(
		image_transfer_shape(extents, extents.size(), extents.size())
	);
	regions.add(
		make_span(origin),
		make_span(std::vector<std::size_t>(extents.size(), 0))
	);
	read_now(reader, destination, regions);

	return get_values<float>(destination);
}

} // anonymous namespace

TEST_CASE(
	"MRC files compressed with gzip read as the files they compress",
	"[mrc][gzip][image_file_format_selector]"
)
{
	image_file_read_format_selector selector;
	selector.register_builtin_formats();
	const auto transfers = make_file_transfer_pool();

	// Each asset of the EMDB is here as it is and compressed with gzip, as
	// the EMDB holds its maps.
	const std::array<std::string, 2> names = {
		"EMD-3197.map", "EMD-3001.map"
	};
	for (const auto &name : names)
	{
		const auto plain_path = get_mrc_asset_path(name);
		const auto compressed_path = get_mrc_asset_path(name + ".gz");

		const auto format = selector.get_most_suitable_format(
			image_file_probe(compressed_path));
		REQUIRE( format != nullptr );
		REQUIRE( format->get_name() == "MRC (gzip)" );

		const auto plain = selector.open(plain_path, *transfers);
		const auto compressed = selector.open(compressed_path, *transfers);

		REQUIRE( compressed->get_descriptor() == plain->get_descriptor() );

		const auto shape = shape_of(*plain);
		const std::vector<std::size_t> origin(shape.size(), 0);

		SECTION( name + " read whole" )
		{
			REQUIRE( read_box(*compressed, origin, shape) ==
				read_box(*plain, origin, shape) );
		}

		SECTION( name + " read as a box inside it, into another type" )
		{
			std::vector<std::size_t> start(shape.size());
			std::vector<std::size_t> box(shape.size());
			for (std::size_t axis = 0; axis < shape.size(); ++axis)
			{
				start[axis] = shape[axis] / 3;
				box[axis] = shape[axis] / 2;
			}

			auto expected = make_host_array<float>(
				box, numerical_type::float32);
			auto converted = make_host_array<double>(
				box, numerical_type::float64);

			image_transfer_plan regions(
				image_transfer_shape(box, box.size(), box.size()));
			regions.add(make_span(start), make_span(origin));
			read_now(*plain, expected, regions);
			read_now(*compressed, converted, regions);

			const auto values = get_values<float>(expected);
			REQUIRE( get_values<double>(converted) ==
				std::vector<double>(values.begin(), values.end()) );
		}
	}
}
