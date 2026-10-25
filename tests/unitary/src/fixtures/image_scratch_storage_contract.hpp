// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>

#include "counting_image_file.hpp"
#include "../mock/mock_image_reader.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <trompeloeil.hpp>

namespace vitrio
{

/**
 * @brief Read regions of the values a storage holds into an array, and
 * wait for them.
 *
 * @throws Whatever the read reports.
 */
inline void serve_now(
	const image_scratch_storage &storage,
	const array_descriptor &values,
	array &destination,
	const image_transfer_plan &regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	storage.serve_async(values, destination.share(), regions, done);
	done->get();
}

/**
 * @brief Take regions of a file into the values a storage holds, and wait
 * for them.
 *
 * @throws Whatever the load reports.
 */
inline void load_now(
	image_scratch_storage &storage,
	std::shared_ptr<const image_reader> file,
	const array_descriptor &values,
	const image_transfer_plan &regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	storage.load_async(std::move(file), values, regions, done);
	done->get();
}

/**
 * @brief Images of a stack of 2 by 2 images, image `indices[i]` landing in
 * slot `slots[i]`.
 */
inline image_transfer_plan images_into_slots(
	const std::vector<std::size_t> &indices,
	const std::vector<std::size_t> &slots
)
{
	image_transfer_plan plan(
		image_transfer_shape(std::vector<std::size_t>{2, 2}, 3, 3)
	);
	for (std::size_t i = 0; i < indices.size(); ++i)
	{
		const std::array<std::size_t, 3> file_offset = {indices[i], 0, 0};
		const std::array<std::size_t, 3> array_offset = {slots[i], 0, 0};
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

/**
 * @brief Check that the storages a function makes keep the contract of an
 * @ref image_scratch_storage, in a section each.
 *
 * @param name A name no other case of the suite uses, which the files of
 * the case are named after.
 * @param make_storage Makes a storage of a number of bytes, as a
 * std::shared_ptr.
 */
template <typename MakeStorage>
void check_image_scratch_storage_contract(
	const std::string &name,
	MakeStorage make_storage
)
{
	constexpr std::size_t size = 4096;
	const auto storage = make_storage(size);
	REQUIRE( storage != nullptr );

	// Three images of 2 by 2 float32, from the 64th byte on, counting up
	// from zero.
	const array_descriptor values(
		{3, 2, 2},
		{4, 2, 1},
		16,
		numerical_type::float32
	);
	const auto counted = test::count_from(0, 12);
	const auto counted_bytes = make_span(
		reinterpret_cast<const byte*>(counted.data()),
		counted.size() * sizeof(float)
	);

	SECTION( "a storage has the size it was made with" )
	{
		CHECK( storage->get_size() == size );
	}

	SECTION( "what is written is read back" )
	{
		const std::string text = "what a scratch holds";
		storage->write(
			size - text.size(),
			make_span(
				reinterpret_cast<const byte*>(text.data()),
				text.size()
			)
		);

		std::string back(text.size(), ' ');
		storage->read(
			size - text.size(),
			make_span(reinterpret_cast<byte*>(&back[0]), back.size())
		);
		CHECK( back == text );
	}

	SECTION( "bytes that reach past the storage are refused" )
	{
		std::vector<byte> bytes(8);

		CHECK_THROWS_AS(
			storage->read(size - 4, make_span(bytes)),
			std::out_of_range
		);
		CHECK_THROWS_AS(
			storage->write(
				size - 4,
				make_span(
					static_cast<const byte*>(bytes.data()),
					bytes.size()
				)
			),
			std::out_of_range
		);
	}

	SECTION( "a serve reads values into an array, converted to its type" )
	{
		storage->write(64, counted_bytes);
		auto destination = test::make_host_array<double>(
			{2, 2, 2},
			numerical_type::float64,
			-1.0
		);

		serve_now(
			*storage,
			values,
			destination,
			images_into_slots({2, 0}, {0, 1})
		);

		CHECK(
			test::get_values<double>(destination) ==
			std::vector<double>{8, 9, 10, 11, 0, 1, 2, 3}
		);
	}

	SECTION( "a serve of values that reach past the storage is reported" )
	{
		const array_descriptor past(
			{3, 2, 2},
			{4, 2, 1},
			1020,
			numerical_type::float32
		);
		auto destination =
			test::make_host_array<float>({1, 2, 2}, numerical_type::float32);

		CHECK_THROWS_AS(
			serve_now(
				*storage,
				past,
				destination,
				images_into_slots({2}, {0})
			),
			std::out_of_range
		);
	}

	SECTION( "a load takes regions of a file in, which a serve then reads" )
	{
		const scoped_path path(name + ".raw");
		const std::vector<std::size_t> extents = {8, 2, 2};
		test::write_counting_image_file(path.get(), extents);
		const auto file =
			test::open_counting_image_file(path.get(), extents, 2);

		load_now(*storage, file, values, images_into_slots({4, 1}, {0, 2}));

		auto destination = test::make_host_array<float>(
			{2, 2, 2},
			numerical_type::float32,
			-1.0F
		);
		serve_now(
			*storage,
			values,
			destination,
			images_into_slots({2, 0}, {0, 1})
		);

		const auto image_1 = test::count_from(4, 4);
		const auto image_4 = test::count_from(16, 4);
		std::vector<float> expected(image_1.begin(), image_1.end());
		expected.insert(expected.end(), image_4.begin(), image_4.end());
		CHECK( test::get_values<float>(destination) == expected );
	}

	SECTION( "a load reports what reading the file fails with" )
	{
		const auto file = std::make_shared<mock_image_reader>();
		REQUIRE_CALL(
			*file,
			read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.SIDE_EFFECT(
				_3->notify_done(
					std::make_exception_ptr(std::runtime_error("read"))
				)
			);

		CHECK_THROWS_AS(
			load_now(*storage, file, values, images_into_slots({0}, {1})),
			std::runtime_error
		);
	}

	SECTION( "a null notifier is refused" )
	{
		const auto file = std::make_shared<mock_image_reader>();
		auto destination =
			test::make_host_array<float>({1, 2, 2}, numerical_type::float32);

		CHECK_THROWS_AS(
			storage->serve_async(
				values,
				destination.share(),
				images_into_slots({0}, {0}),
				nullptr
			),
			std::invalid_argument
		);
		CHECK_THROWS_AS(
			storage->load_async(
				file,
				values,
				images_into_slots({0}, {0}),
				nullptr
			),
			std::invalid_argument
		);
	}
}

} // namespace vitrio
