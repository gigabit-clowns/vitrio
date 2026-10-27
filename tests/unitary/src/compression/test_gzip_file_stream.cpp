// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <compression/gzip_file_stream.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>

#include <vitrio/tests/scoped_path.hpp>
#include "compression_test_bytes.hpp"
#include "zlib_test_streams.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

using namespace vitrio;

namespace
{

void write_bytes(const std::string &path, const std::vector<byte> &bytes)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(
		reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

std::vector<byte> read_all(gzip_file_stream &stream, std::size_t size)
{
	std::vector<byte> bytes(size);
	stream.read(make_span(bytes.data(), bytes.size()));
	return bytes;
}

} // anonymous namespace

TEST_CASE( "gzip_file_stream reads the inflated bytes of a file in order",
	"[gzip_file_stream]" )
{
	const scoped_path path("gzip_file_stream_read.gz");
	const auto bytes = make_codec_test_bytes(300000);
	write_bytes(path.get(), deflate_stream(bytes, 31));

	SECTION( "whole" )
	{
		gzip_file_stream stream(path.get());
		REQUIRE( read_all(stream, bytes.size()) == bytes );
		REQUIRE( stream.get_position() == bytes.size() );
	}

	SECTION( "reading the file a few bytes at a time" )
	{
		gzip_file_stream stream(path.get(), 7);
		REQUIRE( read_all(stream, bytes.size()) == bytes );
	}

	SECTION( "skipping what comes before what is read" )
	{
		gzip_file_stream stream(path.get(), 4096);
		stream.skip(200000);
		REQUIRE( stream.get_position() == 200000 );

		const auto rest = read_all(stream, 1000);
		REQUIRE( std::equal(rest.begin(), rest.end(), bytes.begin() + 200000) );
		REQUIRE( stream.get_position() == 201000 );

		stream.skip(0);
		REQUIRE( stream.get_position() == 201000 );
	}

	SECTION( "reading nothing" )
	{
		gzip_file_stream stream(path.get());
		stream.read(span<byte>());
		REQUIRE( stream.get_position() == 0 );
	}
}

TEST_CASE( "gzip_file_stream reads a file of several members as one",
	"[gzip_file_stream]" )
{
	const scoped_path path("gzip_file_stream_members.gz");
	const auto first = make_codec_test_bytes(70000);
	const auto second = make_codec_test_bytes(50001);
	auto encoded = deflate_stream(first, 31);
	const auto more = deflate_stream(second, 31);
	encoded.insert(encoded.end(), more.begin(), more.end());
	write_bytes(path.get(), encoded);

	gzip_file_stream stream(path.get(), 1000);
	stream.skip(first.size() - 10);
	const auto across = read_all(stream, 20);

	REQUIRE( std::equal(across.begin(), across.begin() + 10, first.end() - 10) );
	REQUIRE( std::equal(across.begin() + 10, across.end(), second.begin()) );
}

TEST_CASE( "gzip_file_stream refuses what it can not read",
	"[gzip_file_stream]" )
{
	const scoped_path path("gzip_file_stream_refused.gz");
	const auto bytes = make_codec_test_bytes(10000);

	SECTION( "a file that is not there" )
	{
		REQUIRE_THROWS_AS(
			gzip_file_stream(path.get()),
			image_file_error
		);
	}

	SECTION( "more bytes than the stream holds" )
	{
		write_bytes(path.get(), deflate_stream(bytes, 31));
		gzip_file_stream stream(path.get(), 100);
		stream.skip(bytes.size());

		REQUIRE_THROWS_WITH(
			read_all(stream, 1),
			Catch::Matchers::StartsWith(path.get()) &&
			Catch::Matchers::ContainsSubstring("stream ends before")
		);
	}

	SECTION( "a file cut short" )
	{
		auto encoded = deflate_stream(bytes, 31);
		encoded.resize(encoded.size() / 2);
		write_bytes(path.get(), encoded);
		gzip_file_stream stream(path.get());

		REQUIRE_THROWS_WITH(
			stream.skip(bytes.size()),
			Catch::Matchers::ContainsSubstring("file ends before")
		);
	}

	SECTION( "a file that is not gzip" )
	{
		write_bytes(path.get(), bytes);
		gzip_file_stream stream(path.get());

		REQUIRE_THROWS_AS(
			read_all(stream, 10),
			image_file_format_error
		);
	}

	SECTION( "an empty file" )
	{
		write_bytes(path.get(), std::vector<byte>());
		gzip_file_stream stream(path.get());

		REQUIRE_THROWS_AS(
			read_all(stream, 1),
			image_file_format_error
		);
	}
}
