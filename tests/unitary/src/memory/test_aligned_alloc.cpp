// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <memory/aligned_alloc.hpp>

#include <cstddef>
#include <cstdint>

using namespace vitrio;

TEST_CASE(
	"aligned_alloc gives memory of the alignment asked for",
	"[aligned_alloc]"
)
{
	const std::size_t repetitions = 1024;
	const std::size_t alignment = 512;
	const std::size_t size = 10 * alignment;

	for (std::size_t i = 0; i < repetitions; ++i)
	{
		auto *data = static_cast<unsigned char*>(
			vitrio::aligned_alloc(size, alignment)
		);

		REQUIRE( data != nullptr );
		REQUIRE( reinterpret_cast<std::uintptr_t>(data) % alignment == 0 );

		// All of it can be written.
		for (std::size_t j = 0; j < size; ++j)
		{
			data[j] = 0xAA;
		}

		aligned_free(data);
	}
}

TEST_CASE(
	"aligned_alloc accepts an alignment below that of a pointer",
	"[aligned_alloc]"
)
{
	auto *data = vitrio::aligned_alloc(16, 2);

	REQUIRE( data != nullptr );
	REQUIRE( reinterpret_cast<std::uintptr_t>(data) % 2 == 0 );

	aligned_free(data);
}

TEST_CASE(
	"aligned_free ignores a null pointer",
	"[aligned_alloc]"
)
{
	aligned_free(nullptr);

	SUCCEED();
}
