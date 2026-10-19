// SPDX-License-Identifier: LGPL-2.1-or-later

// Measures the engine that moves images between a file and an array, on its
// own: no file is opened and no thread is started, and the file is a buffer
// in memory. What it measures is the copy and the conversion of the elements,
// which bound a read once its pages are in memory. A std::memcpy of the same
// images is the ceiling of a copy that converts nothing.
//
// The images of a batch are drawn at random from a file larger than the caches
// of the processor, as a shuffled epoch draws them from the page cache.
// VITRIO_BENCH_FILE_BYTES sets how large, so that a short run, such as the one
// CTest makes, need not fill hundreds of megabytes first.

#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>
#include <formats/strided_transfer/image_region_write_walk.hpp>

#include <array/fixed_width_float.hpp>
#include <array/numerical_type_traits.hpp>
#include <memory/byte_order.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

using namespace vitrio;

namespace
{

// Bytes the file spans by default: more than the caches of a processor hold.
constexpr std::size_t default_file_bytes = std::size_t(256) << 20;

// Images per batch.
constexpr std::size_t batch_size = 64;

// Side of a micrograph, for patches cropped out of micrographs.
constexpr std::size_t micrograph_side = 4096;

// Elements a padded array adds to each row.
constexpr std::size_t row_padding = 16;

// Distinct values the file is tiled with. Filling it with a fresh value per
// element would take longer than measuring it, and what the values are does
// not change what moving them costs.
constexpr std::size_t noise_length = std::size_t(1) << 16;

const std::vector<std::int64_t> boxes = {64, 128, 256, 512};

std::size_t file_bytes()
{
	const char *value = std::getenv("VITRIO_BENCH_FILE_BYTES");
	return value ? std::strtoull(value, nullptr, 10) : default_file_bytes;
}

enum class layout
{
	// Whole images of a stack, as particles are read.
	stack,
	// Square regions cropped out of micrographs, as patches are read.
	patches,
};

// A value of T that a file of particles could hold: around zero for floating
// point types, and within a range a detector counts in for integers.
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
draw(std::mt19937 &engine)
{
	const int low = std::is_signed<T>::value ? -100 : 0;
	const int high = std::min<int>(200, std::numeric_limits<T>::max());
	return static_cast<T>(
		std::uniform_int_distribution<int>(low, high)(engine)
	);
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, T>::type
draw(std::mt19937 &engine)
{
	return T(std::normal_distribution<float>(0.0f, 1.0f)(engine));
}

template <typename T>
T reversed(T value)
{
	std::array<unsigned char, sizeof(T)> raw;
	std::memcpy(raw.data(), &value, raw.size());
	std::reverse(raw.begin(), raw.end());
	T result;
	std::memcpy(&result, raw.data(), sizeof(result));
	return result;
}

// Values of T tiled over count elements, in the given byte order.
template <typename T>
std::vector<T> noise(std::size_t count, byte_order order)
{
	std::mt19937 engine(42);
	std::vector<T> tile(std::min(count, noise_length));
	for (auto &value : tile)
	{
		value = draw<T>(engine);
		if (order != get_system_byte_order())
		{
			value = reversed(value);
		}
	}

	std::vector<T> values(count);
	for (std::size_t first = 0; first < count; first += tile.size())
	{
		const auto length = std::min(tile.size(), count - first);
		std::copy_n(tile.begin(), length, values.begin() + first);
	}
	return values;
}

std::vector<std::ptrdiff_t>
contiguous_strides(const std::vector<std::size_t> &extents)
{
	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}
	return strides;
}

// The file, the array and the regions of one batch between them.
struct batch
{
	std::vector<std::size_t> file_extents;
	std::vector<std::ptrdiff_t> file_strides;
	std::vector<std::size_t> array_extents;
	std::vector<std::ptrdiff_t> array_strides;
	image_transfer_plan regions;
};

batch make_batch(
	layout kind,
	std::size_t box,
	std::size_t file_element_bytes,
	std::size_t padding
)
{
	std::mt19937 engine(7);
	batch result{{}, {}, {}, {}, image_transfer_plan(
		image_transfer_shape({box, box}, 3, 3)
	)};

	if (kind == layout::stack)
	{
		const auto images = std::max<std::size_t>(
			1, file_bytes() / (box * box * file_element_bytes)
		);
		result.file_extents = {images, box, box};
	}
	else
	{
		const auto side = micrograph_side;
		const auto micrographs = std::max<std::size_t>(
			1, file_bytes() / (side * side * file_element_bytes)
		);
		result.file_extents = {micrographs, side, side};
	}
	result.file_strides = contiguous_strides(result.file_extents);
	result.array_extents = {batch_size, box, box + padding};
	result.array_strides = contiguous_strides(result.array_extents);

	std::uniform_int_distribution<std::size_t> image(
		0, result.file_extents[0] - 1
	);
	std::uniform_int_distribution<std::size_t> corner(
		0, result.file_extents[1] - box
	);
	result.regions.reserve(batch_size);
	for (std::size_t i = 0; i < batch_size; ++i)
	{
		const std::vector<std::size_t> file_offset = kind == layout::stack
			? std::vector<std::size_t>{image(engine), 0, 0}
			: std::vector<std::size_t>{
				image(engine), corner(engine), corner(engine)
			};
		const std::vector<std::size_t> array_offset = {i, 0, 0};
		result.regions.add(make_span(file_offset), make_span(array_offset));
	}
	return result;
}

std::size_t element_count(const std::vector<std::size_t> &extents)
{
	std::size_t count = 1;
	for (const auto extent : extents)
	{
		count *= extent;
	}
	return count;
}

void count_throughput(
	benchmark::State &state,
	std::size_t box,
	std::size_t file_element_bytes,
	std::size_t array_element_bytes
)
{
	const auto elements = static_cast<double>(batch_size * box * box);
	state.SetBytesProcessed(
		static_cast<std::int64_t>(state.iterations() * elements)
		* static_cast<std::int64_t>(array_element_bytes)
	);
	state.counters["images_per_second"] = benchmark::Counter(
		batch_size, benchmark::Counter::kIsIterationInvariantRate
	);
	state.counters["file_bytes_per_second"] = benchmark::Counter(
		elements * file_element_bytes,
		benchmark::Counter::kIsIterationInvariantRate,
		benchmark::Counter::kIs1024
	);
}

// Reads a batch out of a file of Q into an array of T.
template <typename Q, typename T>
void measure_read(
	benchmark::State &state,
	layout kind,
	byte_order order,
	std::size_t padding
)
{
	const auto box = static_cast<std::size_t>(state.range(0));
	const auto plan = make_batch(kind, box, sizeof(Q), padding);
	const auto file = noise<Q>(element_count(plan.file_extents), order);
	std::vector<T> array(element_count(plan.array_extents));
	const image_region_read_walk walk(
		plan.regions,
		make_span(plan.file_extents), make_span(plan.file_strides),
		make_span(plan.array_extents), make_span(plan.array_strides),
		0
	);

	for (auto _ : state)
	{
		read_regions(
			walk,
			array.data(), numerical_type_of<T>::value,
			reinterpret_cast<const vitrio::byte*>(file.data()),
			numerical_type_of<Q>::value,
			order
		);
		benchmark::DoNotOptimize(array.data());
		benchmark::ClobberMemory();
	}
	count_throughput(state, box, sizeof(Q), sizeof(T));
}

// Writes a batch out of an array of T into a file of Q.
template <typename T, typename Q>
void measure_write(benchmark::State &state)
{
	const auto box = static_cast<std::size_t>(state.range(0));
	const auto plan = make_batch(layout::stack, box, sizeof(Q), 0);
	const auto array = noise<T>(
		element_count(plan.array_extents), get_system_byte_order()
	);
	std::vector<Q> file(element_count(plan.file_extents));
	const image_region_write_walk walk(
		plan.regions,
		make_span(plan.file_extents), make_span(plan.file_strides),
		make_span(plan.array_extents), make_span(plan.array_strides),
		0
	);

	for (auto _ : state)
	{
		write_regions(
			walk,
			array.data(), numerical_type_of<T>::value,
			reinterpret_cast<vitrio::byte*>(file.data()),
			numerical_type_of<Q>::value,
			get_system_byte_order()
		);
		benchmark::DoNotOptimize(file.data());
		benchmark::ClobberMemory();
	}
	count_throughput(state, box, sizeof(Q), sizeof(T));
}

// The ceiling: the same images of a stack of float, one std::memcpy each.
void measure_memcpy(benchmark::State &state)
{
	const auto box = static_cast<std::size_t>(state.range(0));
	const auto image = box * box;
	const auto plan = make_batch(layout::stack, box, sizeof(float), 0);
	const auto file = noise<float>(
		element_count(plan.file_extents), get_system_byte_order()
	);
	std::vector<float> array(element_count(plan.array_extents));

	std::mt19937 engine(7);
	std::uniform_int_distribution<std::size_t> pick(
		0, plan.file_extents[0] - 1
	);
	std::vector<std::size_t> sources(batch_size);
	for (auto &source : sources)
	{
		source = pick(engine);
	}

	for (auto _ : state)
	{
		for (std::size_t i = 0; i < batch_size; ++i)
		{
			std::memcpy(
				array.data() + i * image,
				file.data() + sources[i] * image,
				image * sizeof(float)
			);
		}
		benchmark::DoNotOptimize(array.data());
		benchmark::ClobberMemory();
	}
	count_throughput(state, box, sizeof(float), sizeof(float));
}

// What working out the walk of a batch costs, which every read pays once
// before it moves anything.
void plan_walk(benchmark::State &state, layout kind)
{
	const auto box = static_cast<std::size_t>(state.range(0));
	const auto plan = make_batch(kind, box, sizeof(float), 0);
	for (auto _ : state)
	{
		image_region_read_walk walk(
			plan.regions,
			make_span(plan.file_extents), make_span(plan.file_strides),
			make_span(plan.array_extents), make_span(plan.array_strides),
			0
		);
		benchmark::DoNotOptimize(walk);
	}
	state.counters["images_per_second"] = benchmark::Counter(
		batch_size, benchmark::Counter::kIsIterationInvariantRate
	);
}

// A benchmark is named benchmark::internal::Benchmark before 1.9.5, and
// benchmark::Benchmark after, so its type is left to deduce.
template <typename Benchmark>
Benchmark* with_boxes(Benchmark *b)
{
	b->ArgName("box");
	for (const auto box : boxes)
	{
		b->Arg(box);
	}
	return b->Unit(benchmark::kMicrosecond);
}

template <typename Q, typename T>
std::string pair_name(const char *operation, const char *case_name)
{
	return std::string(operation) + "/" + case_name + "/"
		+ to_string(numerical_type_of<Q>::value) + "/"
		+ to_string(numerical_type_of<T>::value);
}

template <typename Q, typename T>
void add_read(
	const char *case_name,
	layout kind,
	byte_order order,
	std::size_t padding
)
{
	with_boxes(benchmark::RegisterBenchmark(
		pair_name<Q, T>("read", case_name).c_str(),
		[=](benchmark::State &state) {
			measure_read<Q, T>(state, kind, order, padding);
		}
	));
}

template <typename T, typename Q>
void add_write()
{
	with_boxes(benchmark::RegisterBenchmark(
		pair_name<T, Q>("write", "stack").c_str(),
		[](benchmark::State &state) { measure_write<T, Q>(state); }
	));
}

byte_order other_byte_order()
{
	return get_system_byte_order() == byte_order::little_endian
		? byte_order::big_endian
		: byte_order::little_endian;
}

void add_benchmarks()
{
	const auto native = get_system_byte_order();
	const auto swapped = other_byte_order();

	// What particles are stored as, read as float32, which models take, and
	// as themselves.
	add_read<std::int8_t, float>("stack", layout::stack, native, 0);
	add_read<std::uint8_t, float>("stack", layout::stack, native, 0);
	add_read<std::int16_t, float>("stack", layout::stack, native, 0);
	add_read<std::uint16_t, float>("stack", layout::stack, native, 0);
	add_read<float16_t, float>("stack", layout::stack, native, 0);
	add_read<float, float>("stack", layout::stack, native, 0);
	add_read<double, float>("stack", layout::stack, native, 0);
	add_read<std::int16_t, std::int16_t>("stack", layout::stack, native, 0);
	add_read<float16_t, float16_t>("stack", layout::stack, native, 0);
	add_read<float, float16_t>("stack", layout::stack, native, 0);

	// A file in the other byte order, as MRC files may be.
	add_read<float, float>("swapped", layout::stack, swapped, 0);
	add_read<std::int16_t, float>("swapped", layout::stack, swapped, 0);
	add_read<float16_t, float>("swapped", layout::stack, swapped, 0);

	// Strided on the file's side: patches cropped out of micrographs.
	add_read<float, float>("patches", layout::patches, native, 0);
	add_read<std::int16_t, float>("patches", layout::patches, native, 0);
	add_read<float16_t, float>("patches", layout::patches, native, 0);

	// Strided on the array's side: rows padded.
	add_read<float, float>("padded", layout::stack, native, row_padding);
	add_read<float16_t, float>("padded", layout::stack, native, row_padding);

	// Particles written as float32, and as the smaller types they may be
	// stored as.
	add_write<float, float>();
	add_write<float, float16_t>();
	add_write<float, std::int16_t>();

	with_boxes(benchmark::RegisterBenchmark(
		"memcpy/stack/float32", measure_memcpy
	));
	with_boxes(benchmark::RegisterBenchmark(
		"walk/stack",
		[](benchmark::State &state) { plan_walk(state, layout::stack); }
	));
	with_boxes(benchmark::RegisterBenchmark(
		"walk/patches",
		[](benchmark::State &state) { plan_walk(state, layout::patches); }
	));
}

} // anonymous namespace

int main(int argc, char **argv)
{
	benchmark::Initialize(&argc, argv);
	if (benchmark::ReportUnrecognizedArguments(argc, argv))
	{
		return 1;
	}
	add_benchmarks();
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
	return 0;
}
