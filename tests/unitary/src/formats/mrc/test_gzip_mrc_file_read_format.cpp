// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/mrc/gzip_mrc_file_read_format.hpp>

#include <formats/mrc/mrc_file_read_format.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include "../../compression/zlib_test_streams.hpp"
#include "fixtures/mrc_test_file.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::mrc;
using namespace vitrio::test;

namespace
{

// Compress the bytes of a file into a gzip file.
void write_gzip(const std::string &path, const std::vector<char> &raw)
{
	write_file(path, [&raw]
	{
		const auto encoded =
			deflate_stream(std::vector<byte>(raw.begin(), raw.end()), 31);
		return std::vector<char>(encoded.begin(), encoded.end());
	}());
}

std::vector<float> read_whole(const image_reader &reader)
{
	const auto extents = reader.get_descriptor().get_extents();
	const std::vector<std::size_t> shape(extents.begin(), extents.end());
	auto destination = make_array(
		make_contiguous_array_descriptor(
			make_span(shape),
			numerical_type::float32
		)
	);

	image_transfer_plan regions(
		image_transfer_shape(shape, shape.size(), shape.size()));
	const std::vector<std::size_t> origin(shape.size(), 0);
	regions.add(make_span(origin), make_span(origin));
	read_now(reader, destination, regions);

	std::size_t count = 1;
	for (const auto extent : shape)
	{
		count *= extent;
	}

	const auto *data = reinterpret_cast<const float*>(destination.get_data());
	return std::vector<float>(data, data + count);
}

} // anonymous namespace

TEST_CASE( "the gzip MRC format claims compressed MRC files",
	"[gzip_mrc_file_read_format]" )
{
	const scoped_path path("gzip_read_format_claimed.map.gz");
	const gzip_mrc_file_read_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "MRC (gzip)" );
	}

	SECTION( "a file whose inflated header carries the identifier" )
	{
		write_gzip(path.get(), make_file(4, 3, 1, 0, 2, counting(12)));

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}

	SECTION( "the identifier is claimed whatever the name" )
	{
		const std::string other = path.get() + ".unknown";
		write_gzip(other, make_file(4, 3, 1, 0, 2, counting(12)));

		REQUIRE( format.get_suitability(image_file_probe(other)) ==
			image_file_format_suitability::normal );

		std::remove(other.c_str());
	}

	SECTION( "a header without it, named as MRC, is claimed weakly" )
	{
		auto raw = make_file(4, 3, 1, 0, 2, counting(12));
		std::memcpy(raw.data() + 208, "\0\0\0\0", 4);
		write_gzip(path.get(), raw);

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::fallback );
	}

	SECTION( "a header without it, named otherwise, is not claimed" )
	{
		const scoped_path other("gzip_read_format_claimed.txt.gz");
		auto raw = make_file(4, 3, 1, 0, 2, counting(12));
		std::memcpy(raw.data() + 208, "\0\0\0\0", 4);
		write_gzip(other.get(), raw);

		REQUIRE( format.get_suitability(image_file_probe(other.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "an MRC file that is not compressed is not claimed" )
	{
		write_file(path.get(), make_file(4, 3, 1, 0, 2, counting(12)));

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "a file that is not there is not claimed" )
	{
		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "the MRC format leaves a compressed file to it" )
	{
		write_gzip(path.get(), make_file(4, 3, 1, 0, 2, counting(12)));

		REQUIRE( mrc_file_read_format().get_suitability(
			image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}
}

TEST_CASE( "the gzip MRC format opens a file as its header states it",
	"[gzip_mrc_file_read_format]" )
{
	const scoped_path path("gzip_read_format_opened.map.gz");
	const gzip_mrc_file_read_format format;

	SECTION( "a stack of images reports its shape and data type" )
	{
		write_gzip(path.get(), make_file(4, 3, 2, 0, 2, counting(24)));

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);

		const std::vector<std::size_t> extents = {2, 3, 4};
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents), 2, numerical_type::float32) );
	}

	SECTION( "the values that follow the header are read" )
	{
		const auto values = counting(24);
		write_gzip(path.get(), make_file(4, 3, 2, 0, 2, values));

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);

		REQUIRE( read_whole(*reader) == values );
	}

	SECTION( "values an extended header leaves unaligned are read" )
	{
		const auto values = counting(24);
		auto raw = make_file(4, 3, 2, 0, 2, values);
		raw.insert(raw.begin() + mrc::header_size, 2, 'x');
		put_int32(raw, 92, 2);
		write_gzip(path.get(), raw);

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);

		REQUIRE( read_whole(*reader) == values );
	}

	SECTION( "a volume" )
	{
		const auto values = counting(24);
		write_gzip(path.get(), make_file(4, 3, 2, 1, 2, values));

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);

		REQUIRE( reader->get_descriptor().get_core_rank() == 3 );
		REQUIRE( read_whole(*reader) == values );
	}
}

TEST_CASE( "the gzip MRC format refuses what it can not read",
	"[gzip_mrc_file_read_format]" )
{
	const scoped_path path("gzip_read_format_refused.map.gz");
	const gzip_mrc_file_read_format format;

	SECTION( "a header without the identifier" )
	{
		auto raw = make_file(4, 3, 1, 0, 2, counting(12));
		raw[208] = 'X';
		write_gzip(path.get(), raw);

		REQUIRE_THROWS_AS(
			format.open(
				image_file_probe(path.get()),
				*make_file_transfer_pool()
			),
			image_file_format_error
		);
	}

	SECTION( "a stream that ends within the header" )
	{
		auto raw = make_file(4, 3, 1, 0, 2, counting(12));
		raw.resize(500);
		write_gzip(path.get(), raw);

		REQUIRE_THROWS_AS(
			format.open(
				image_file_probe(path.get()),
				*make_file_transfer_pool()
			),
			image_file_format_error
		);
	}

	SECTION( "values the stream ends before, when they are read" )
	{
		auto raw = make_file(4, 3, 2, 0, 2, counting(24));
		raw.resize(raw.size() - sizeof(float));
		write_gzip(path.get(), raw);

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);

		REQUIRE_THROWS_WITH(
			read_whole(*reader),
			Catch::Matchers::StartsWith(path.get() + ": ")
		);
	}
}

TEST_CASE(
	"the gzip MRC format reads a single section as its file is named",
	"[gzip_mrc_file_read_format]"
)
{
	const gzip_mrc_file_read_format format;

	SECTION( "a stack of one image from a .mrcs.gz file" )
	{
		const scoped_path path("gzip_read_format_single.mrcs.gz");
		write_gzip(path.get(), make_file(4, 3, 1, 0, 2, counting(12)));

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);
		const auto extents = reader->get_descriptor().get_extents();

		REQUIRE( std::vector<std::size_t>(extents.begin(), extents.end()) ==
			std::vector<std::size_t>{1, 3, 4} );
	}

	SECTION( "a single image from any other" )
	{
		const scoped_path path("gzip_read_format_single.mrc.gz");
		write_gzip(path.get(), make_file(4, 3, 1, 0, 2, counting(12)));

		const auto reader = format.open(
			image_file_probe(path.get()),
			*make_file_transfer_pool()
		);
		const auto extents = reader->get_descriptor().get_extents();

		REQUIRE( std::vector<std::size_t>(extents.begin(), extents.end()) ==
			std::vector<std::size_t>{3, 4} );
	}
}
