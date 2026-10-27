// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_chunk_store.hpp"

#include "zarr_node.hpp"

#include <formats/raw/create_image_file.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <strided_transfer/image_region_read_walk.hpp>
#include <strided_transfer/image_region_transfer.hpp>
#include <strided_transfer/image_region_write_walk.hpp>
#include <system/file_sync.hpp>
#include <system/scattered_file_write.hpp>

#include <vitrio/const_memory_range.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <assert.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

// Reverses the bytes of every value of a chunk, as a stretch of elements of
// one type, between the order of the system and the other.
void swap_order(
	const std::vector<byte> &values,
	std::vector<byte> &swapped,
	numerical_type type
)
{
	const auto count = values.size() / get_size(type);
	const std::vector<std::size_t> extents = {count};
	const std::vector<std::ptrdiff_t> strides = {1};
	const std::vector<std::size_t> origin = {0};

	image_transfer_plan whole(image_transfer_shape(extents, 1, 1));
	whole.add(make_span(origin), make_span(origin));
	const image_region_read_walk walk(
		whole, make_span(extents), make_span(strides),
		make_span(extents), make_span(strides), 0);

	const auto other = get_system_byte_order() == byte_order::little_endian
		? byte_order::big_endian
		: byte_order::little_endian;
	swapped.resize(values.size());
	read_regions(walk, swapped.data(), type, values.data(), type, other);
}

std::vector<byte> read_file(const std::string &path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		throw image_file_error(
			path + ": zarr_chunk_store: The chunk could not be opened again."
		);
	}

	std::vector<byte> bytes(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
	if (input.bad())
	{
		throw image_file_error(
			path + ": zarr_chunk_store: The chunk could not be read again."
		);
	}

	return bytes;
}

} // anonymous namespace

zarr_chunk_store::zarr_chunk_store(
	std::string directory,
	std::shared_ptr<const zarr_array_layout> layout,
	std::size_t byte_budget
)
	: m_directory(std::move(directory))
	, m_layout(std::move(layout))
	, m_byte_budget(byte_budget)
	, m_chunk_bytes(m_layout->get_chunk_layout().get_byte_size())
	, m_held_bytes(0)
	, m_clock(0)
	, m_partials(0)
{
	VITRIO_ASSERT(m_layout->get_shard_layout() == nullptr);

	// A chunk not written yet holds the fill value, in the order of the
	// system, as every chunk held in memory does.
	const auto fill = m_layout->get_fill_element();
	m_fill_chunk.resize(m_chunk_bytes);
	for (std::size_t at = 0; at + fill.size() <= m_fill_chunk.size();
		at += fill.size())
	{
		std::copy(fill.begin(), fill.end(), m_fill_chunk.begin() + at);
	}
}

zarr_chunk_store::~zarr_chunk_store() = default;

const std::string& zarr_chunk_store::get_directory() const noexcept
{
	return m_directory;
}

const zarr_array_layout& zarr_chunk_store::get_layout() const noexcept
{
	return *m_layout;
}

void zarr_chunk_store::write(
	span<const std::size_t> chunk,
	const std::vector<const image_transfer_plan*> &pieces,
	const const_array &source
)
{
	const chunk_key key(chunk.begin(), chunk.end());
	const auto &layout = m_layout->get_chunk_layout();
	const auto &array = source.get_descriptor();

	for (;;)
	{
		const auto held = acquire(key);
		std::unique_lock<std::mutex> lock(held->mutex);

		// Stored between when it was found and now: found again, as it is
		// stored now.
		if (held->retired)
		{
			continue;
		}
		if (!held->ready)
		{
			fill(key, *held);
			held->ready = true;
		}

		for (const auto *plan : pieces)
		{
			const image_region_write_walk walk(
				*plan,
				layout.get_extents(),
				layout.get_strides(),
				array.get_extents(),
				array.get_strides(),
				array.get_offset()
			);
			write_regions(
				walk,
				source.get_data(),
				array.get_data_type(),
				held->values.data(),
				layout.get_data_type(),
				get_system_byte_order()
			);

			std::size_t elements = 1;
			for (const auto extent : plan->get_shape().get_extents())
			{
				elements *= extent;
			}
			held->written += elements * plan->get_region_count();
		}

		if (held->written >= held->valid)
		{
			store_locked(key, *held);
		}
		break;
	}

	make_room();
}

std::vector<std::vector<std::size_t>>
zarr_chunk_store::get_held_chunks() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::vector<chunk_key> chunks;
	for (const auto &held : m_held)
	{
		chunks.push_back(held.first);
	}

	return chunks;
}

void zarr_chunk_store::store(span<const std::size_t> chunk)
{
	const chunk_key key(chunk.begin(), chunk.end());
	std::shared_ptr<held_chunk> held;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto found = m_held.find(key);
		if (found == m_held.end())
		{
			return;
		}
		held = found->second;
	}

	std::lock_guard<std::mutex> lock(held->mutex);
	if (!held->retired && held->ready)
	{
		store_locked(key, *held);
	}
}

std::vector<std::string> zarr_chunk_store::take_unsynced()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::vector<std::string> paths(m_unsynced.begin(), m_unsynced.end());
	m_unsynced.clear();
	return paths;
}

void zarr_chunk_store::add_unsynced(const std::string &path)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_unsynced.insert(path);
}

void zarr_chunk_store::sync(const std::string &path)
{
	boost::system::error_code missing;
	if (!boost::filesystem::exists(path, missing))
	{
		return;
	}

	const writable_image_file file(path);
	std::error_code error;
	sync_file(file.get_file(), error);
	if (error)
	{
		throw image_file_error(
			path + ": zarr_chunk_store: The file could not be synced: " +
			error.message()
		);
	}
}

std::shared_ptr<zarr_chunk_store::held_chunk>
zarr_chunk_store::acquire(const chunk_key &chunk)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto &held = m_held[chunk];
	if (!held)
	{
		held = std::make_shared<held_chunk>();
		held->valid = count_valid(chunk);
		m_held_bytes += m_chunk_bytes;
	}
	held->touched = ++m_clock;

	return held;
}

void zarr_chunk_store::fill(const chunk_key &chunk, held_chunk &held)
{
	bool stored;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		stored = m_stored.count(chunk) != 0;
	}

	if (!stored)
	{
		held.values = m_fill_chunk;
		return;
	}

	// Written before: what of it is not written again keeps what it was.
	const auto path = get_path(chunk);
	const auto encoded = read_file(path);
	const auto &layout = m_layout->get_chunk_layout();
	try
	{
		m_layout->get_codecs().decode(
			make_span(encoded.data(), encoded.size()),
			layout.get_byte_size(),
			held.values
		);
	}
	catch (const image_file_format_error &error)
	{
		throw image_file_format_error(path + ": " + error.what());
	}

	if (layout.get_byte_order() != get_system_byte_order())
	{
		std::vector<byte> swapped;
		swap_order(held.values, swapped, layout.get_data_type());
		held.values.swap(swapped);
	}
}

void zarr_chunk_store::store_locked(const chunk_key &chunk, held_chunk &held)
{
	const auto path = get_path(chunk);
	const auto &layout = m_layout->get_chunk_layout();

	if (held.values == m_fill_chunk)
	{
		boost::system::error_code error;
		boost::filesystem::remove(path, error);
		if (error)
		{
			throw image_file_error(
				path + ": zarr_chunk_store: The file of a chunk of the fill "
				"value could not be removed: " + error.message()
			);
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		m_stored.erase(chunk);
		m_unsynced.erase(path);
	}
	else
	{
		std::vector<byte> swapped;
		const auto *values = &held.values;
		if (layout.get_byte_order() != get_system_byte_order())
		{
			swap_order(held.values, swapped, layout.get_data_type());
			values = &swapped;
		}

		std::vector<byte> encoded;
		m_layout->get_codecs().encode(
			make_span(values->data(), values->size()), encoded);

		// Written beside the chunk and renamed over it, under a name no
		// other write of this store takes.
		std::uint64_t partial;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			partial = ++m_partials;
		}
		const auto written = path + ".partial." + std::to_string(partial);

		boost::system::error_code error;
		boost::filesystem::create_directories(
			boost::filesystem::path(path).parent_path(), error);
		if (error)
		{
			throw image_file_error(
				path + ": zarr_chunk_store: The directory of the chunk could "
				"not be created: " + error.message()
			);
		}

		create_image_file(written, encoded.size());
		{
			const writable_image_file file(written);
			const const_memory_range range(encoded.data(), encoded.size());
			std::error_code failure;
			const auto size = write_scattered(
				file.get_file(), 0, make_span(&range, 1), failure);
			if (failure || size != encoded.size())
			{
				boost::filesystem::remove(written, error);
				throw image_file_error(
					path + ": zarr_chunk_store: The chunk could not be "
					"written: " + failure.message()
				);
			}
		}

		boost::filesystem::rename(written, path, error);
		if (error)
		{
			boost::system::error_code ignored;
			boost::filesystem::remove(written, ignored);
			throw image_file_error(
				path + ": zarr_chunk_store: The chunk could not be put in "
				"place: " + error.message()
			);
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		m_stored.insert(chunk);
		m_unsynced.insert(path);
	}

	// Stored: whoever finds it from now on reads it back.
	held.retired = true;
	std::vector<byte>().swap(held.values);

	std::lock_guard<std::mutex> lock(m_mutex);
	const auto found = m_held.find(chunk);
	if (found != m_held.end() && found->second.get() == &held)
	{
		m_held.erase(found);
		m_held_bytes -= m_chunk_bytes;
	}
}

void zarr_chunk_store::make_room()
{
	// The chunks written least recently are stored until what is held fits
	// the budget again.
	std::vector<std::pair<chunk_key, std::shared_ptr<held_chunk>>> victims;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_held_bytes <= m_byte_budget)
		{
			return;
		}

		std::vector<std::pair<std::uint64_t, chunk_key>> order;
		for (const auto &held : m_held)
		{
			order.emplace_back(held.second->touched, held.first);
		}
		std::sort(order.begin(), order.end());

		auto bytes = m_held_bytes;
		for (const auto &entry : order)
		{
			if (bytes <= m_byte_budget)
			{
				break;
			}
			victims.emplace_back(entry.second, m_held[entry.second]);
			bytes -= m_chunk_bytes;
		}
	}

	for (const auto &victim : victims)
	{
		std::lock_guard<std::mutex> lock(victim.second->mutex);
		if (!victim.second->retired && victim.second->ready)
		{
			store_locked(victim.first, *victim.second);
		}
	}
}

std::string zarr_chunk_store::get_path(const chunk_key &chunk) const
{
	return join_zarr_path(
		m_directory, m_layout->get_key_encoding().get_key(make_span(chunk)));
}

std::size_t zarr_chunk_store::count_valid(const chunk_key &chunk) const
{
	const auto shape = m_layout->get_shape();
	const auto extents = m_layout->get_chunk_layout().get_extents();

	std::size_t valid = 1;
	for (std::size_t axis = 0; axis < chunk.size(); ++axis)
	{
		const auto start = chunk[axis] * extents[axis];
		valid *= std::min(extents[axis], shape[axis] - start);
	}

	return valid;
}

} // namespace zarr
} // namespace vitrio
