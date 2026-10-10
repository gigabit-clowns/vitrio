// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/eer/eer_encoding.hpp>

#include <formats/tiff/tiff_file.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include "fixtures/eer_test_file.hpp"

#include <stdexcept>

using namespace vitrio;
using namespace vitrio::eer;
using namespace vitrio::test;

namespace
{

eer_encoding read_encoding_of(const std::string &path)
{
	vitrio::tiff::tiff_file file(
		path, vitrio::tiff::tiff_file_mode::read);
	return read_eer_encoding(file, path);
}

} // anonymous namespace

TEST_CASE( "an eer_encoding holds the widths of its fields", "[eer_encoding]" )
{
	const eer_encoding encoding(7, 2, 1);

	REQUIRE( encoding.get_rle_bits() == 7 );
	REQUIRE( encoding.get_horizontal_bits() == 2 );
	REQUIRE( encoding.get_vertical_bits() == 1 );
	REQUIRE( encoding.get_code_bits() == 10 );
	REQUIRE( encoding == eer_encoding(7, 2, 1) );
	REQUIRE( encoding != eer_encoding(7, 1, 2) );
}

TEST_CASE( "an eer_encoding refuses widths no stream has", "[eer_encoding]" )
{
	REQUIRE_THROWS_AS( eer_encoding(0, 2, 2), std::invalid_argument );
	REQUIRE_THROWS_AS( eer_encoding(17, 2, 2), std::invalid_argument );
	REQUIRE_THROWS_AS( eer_encoding(7, 9, 2), std::invalid_argument );
	REQUIRE_THROWS_AS( eer_encoding(16, 8, 1), std::invalid_argument );
	REQUIRE_NOTHROW( eer_encoding(16, 8, 0) );
}

TEST_CASE( "the compression schemes of EER are told apart", "[eer_encoding]" )
{
	REQUIRE( is_eer_compression(65000) );
	REQUIRE( is_eer_compression(65001) );
	REQUIRE( is_eer_compression(65002) );
	REQUIRE_FALSE( is_eer_compression(1) );
	REQUIRE_FALSE( is_eer_compression(65003) );
}

TEST_CASE( "the encoding of a page is read from its compression and tags",
	"[eer_encoding]" )
{
	const scoped_path path("eer_encoding_page.eer");
	eer_test_movie movie;
	movie.frames = {{}};

	SECTION( "65000 encodes runs of 8 bits" )
	{
		movie.compression = 65000;
		movie.rle_bits = 8;
		write_eer_file(path.get(), movie);

		REQUIRE( read_encoding_of(path.get()) == eer_encoding(8, 2, 2) );
	}

	SECTION( "65001 encodes runs of 7 bits" )
	{
		write_eer_file(path.get(), movie);

		REQUIRE( read_encoding_of(path.get()) == eer_encoding(7, 2, 2) );
	}

	SECTION( "65002 states its widths in tags of its own" )
	{
		movie.compression = 65002;
		movie.rle_bits = 6;
		movie.horizontal_bits = 1;
		movie.vertical_bits = 3;
		movie.states_widths = true;
		write_eer_file(path.get(), movie);

		REQUIRE( read_encoding_of(path.get()) == eer_encoding(6, 1, 3) );
	}

	SECTION( "65002 without its tags stands for the widths of 65001" )
	{
		movie.compression = 65002;
		write_eer_file(path.get(), movie);

		REQUIRE( read_encoding_of(path.get()) == eer_encoding(7, 2, 2) );
	}

	SECTION( "tags stating widths no encoding has are refused" )
	{
		movie.compression = 65002;
		movie.rle_bits = 20;
		movie.states_widths = true;
		write_eer_file(path.get(), movie);

		REQUIRE_THROWS_AS(
			read_encoding_of(path.get()),
			image_file_format_error
		);
	}

	SECTION( "a page compressed otherwise is refused" )
	{
		movie.compression = 1;
		write_eer_file(path.get(), movie);

		REQUIRE_THROWS_AS(
			read_encoding_of(path.get()),
			image_file_format_error
		);
	}
}
