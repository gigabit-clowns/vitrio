// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_read_operation.hpp"

#include "zarr_node.hpp"
#include "zarr_shard_index.hpp"

#include <strided_transfer/image_region_read_walk.hpp>
#include <strided_transfer/image_region_transfer.hpp>
#include <system/scattered_file_read.hpp>

#include <vitrio/concurrency/task.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/memory_range.hpp>

#include <assert.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/exceptions.hpp>

#include <algorithm>
#include <map>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

const std::uint64_t kind_bits = 3;

// What the file of a chunk may hold beyond the chunk decoded, for a codec
// that encodes some chunks into more bytes than they decode into, as every
// compressor does a chunk it can not compress. A chunk larger than that is
// no chunk of the array, and is refused before memory is taken for it.
std::size_t get_encoded_limit(std::size_t decoded)
{
	return decoded + decoded / 16 + (1UL << 16);
}

} // anonymous namespace

// A step of one file or one chunk, which the executor of computations runs.
class zarr_read_operation::step final
	: public task
{
public:
	step(
		const zarr_read_operation &operation,
		std::size_t id,
		step_kind kind
	) noexcept
		: m_operation(operation)
		, m_id(id)
		, m_kind(kind)
	{
	}

	void run() override
	{
		switch (m_kind)
		{
		case step_kind::open:
			m_operation.open(m_id);
			break;
		case step_kind::decode:
			m_operation.decode(m_id);
			break;
		case step_kind::whole:
			m_operation.read_whole(m_id);
			break;
		default:
			VITRIO_ASSERT(false);
		}
	}

private:
	const zarr_read_operation &m_operation;
	std::size_t m_id;
	step_kind m_kind;
};

zarr_read_operation::zarr_read_operation(
	std::string directory,
	std::shared_ptr<const zarr_array_layout> layout,
	array destination,
	image_chunk_regions chunks,
	bool asynchronous,
	std::size_t byte_budget,
	std::size_t chunks_in_flight
)
	: m_directory(std::move(directory))
	, m_layout(std::move(layout))
	, m_destination(std::move(destination))
	, m_array_data(m_destination.get_data())
	, m_chunks(std::move(chunks))
	, m_asynchronous(asynchronous)
	, m_byte_budget(byte_budget)
	, m_files_in_flight(std::max<std::size_t>(1, chunks_in_flight))
	, m_encoded_limit(get_encoded_limit(
		m_layout->get_chunk_layout().get_byte_size()))
	, m_next(0)
	, m_active(0)
	, m_reserved(0)
{
	// Each chunk reached is a part of the file that holds it: its own, or
	// that of its shard, which the parts in it share.
	const auto &keys = m_layout->get_key_encoding();
	const auto *const shards = m_layout->get_shard_layout();
	std::map<std::vector<std::size_t>, std::size_t> files;
	std::vector<std::size_t> file;
	m_parts.resize(m_chunks.get_chunk_count());
	for (std::size_t position = 0; position < m_parts.size(); ++position)
	{
		const auto chunk = m_chunks.get_chunk(position);
		auto &current = m_parts[position];
		current.position = position;
		current.entry = 0;
		if (shards != nullptr)
		{
			shards->get_shard(chunk, file);
			current.entry = shards->get_index_entry(chunk);
		}
		else
		{
			file.assign(chunk.begin(), chunk.end());
		}

		const auto found = files.emplace(file, m_units.size());
		if (found.second)
		{
			m_units.emplace_back();
			m_units.back().path = join_zarr_path(
				m_directory, keys.get_key(make_span(file)));
		}

		current.unit = found.first->second;
		m_units[current.unit].parts.push_back(position);
		++m_units[current.unit].undecoded;
	}
}

zarr_read_operation::~zarr_read_operation() = default;

void zarr_read_operation::start(file_transfer_scope &scope)
{
	admit(scope);
}

void zarr_read_operation::on_transfer(
	const file_transfer_completion &completion,
	file_transfer_scope &scope
)
{
	const auto tag = completion.get_tag();
	const auto id = static_cast<std::size_t>(tag >> kind_bits);
	const auto kind = static_cast<step_kind>(
		tag & ((std::uint64_t(1) << kind_bits) - 1));

	if (kind == step_kind::read_index)
	{
		VITRIO_ASSERT(id < m_units.size());
		auto &file = m_units[id];
		check_read(completion, file, file.index.size());
		--file.reads;
		place_chunks(scope, id);
		return;
	}

	VITRIO_ASSERT(kind == step_kind::read_chunk);
	VITRIO_ASSERT(id < m_parts.size());
	auto &file = m_units[m_parts[id].unit];
	check_read(completion, file, m_parts[id].encoded.size());
	close_after_read(file);
	run(scope, id, step_kind::decode);
}

void zarr_read_operation::on_task(
	std::uint64_t tag,
	std::exception_ptr error,
	file_transfer_scope &scope
)
{
	if (error)
	{
		std::rethrow_exception(error);
	}

	const auto id = static_cast<std::size_t>(tag >> kind_bits);
	const auto kind = static_cast<step_kind>(
		tag & ((std::uint64_t(1) << kind_bits) - 1));

	if (kind == step_kind::decode)
	{
		finish_part(scope, id);
		return;
	}

	VITRIO_ASSERT(id < m_units.size());
	auto &file = m_units[id];
	if (kind == step_kind::whole)
	{
		for (const auto part : file.parts)
		{
			std::vector<byte>().swap(m_parts[part].encoded);
		}
		file.undecoded = 0;
		m_reserved -= get_unit_bytes(id);
		--m_active;
		admit(scope);
		return;
	}

	VITRIO_ASSERT(kind == step_kind::open);
	if (file.missing)
	{
		for (const auto part : file.parts)
		{
			m_parts[part].missing = true;
			run(scope, part, step_kind::decode);
		}
		return;
	}

	if (m_layout->get_shard_layout() != nullptr)
	{
		queue_read(
			scope, file, get_index_offset(file), file.index, id,
			step_kind::read_index);
		return;
	}

	const auto part = file.parts.front();
	if (m_parts[part].encoded.empty())
	{
		// Nothing to read, which its codecs refuse.
		file.file.reset();
		run(scope, part, step_kind::decode);
		return;
	}
	queue_read(
		scope, file, 0, m_parts[part].encoded, part, step_kind::read_chunk);
}

void zarr_read_operation::admit(file_transfer_scope &scope)
{
	while (m_next < m_units.size() && m_active < m_files_in_flight)
	{
		const auto bytes = get_unit_bytes(m_next);
		if (m_active > 0 && bytes > m_byte_budget - std::min(
			m_byte_budget, m_reserved))
		{
			break;
		}

		m_reserved += bytes;
		++m_active;
		run(
			scope,
			m_next++,
			m_asynchronous ? step_kind::open : step_kind::whole
		);
	}
}

void zarr_read_operation::run(
	file_transfer_scope &scope,
	std::size_t id,
	step_kind kind
)
{
	const auto tag = static_cast<std::uint64_t>(id) << kind_bits |
		static_cast<std::uint64_t>(kind);
	scope.run(std::make_unique<step>(*this, id, kind), tag);
}

void zarr_read_operation::queue_read(
	file_transfer_scope &scope,
	unit &file,
	std::uint64_t offset,
	std::vector<byte> &buffer,
	std::size_t id,
	step_kind kind
)
{
	const memory_range range(buffer.data(), buffer.size());
	scope.queue_read(
		file.file->get_mapping_handle().handle,
		offset,
		make_span(&range, 1),
		static_cast<std::uint64_t>(id) << kind_bits |
			static_cast<std::uint64_t>(kind)
	);
	++file.reads;
}

void zarr_read_operation::check_read(
	const file_transfer_completion &completion,
	const unit &file,
	std::size_t expected
) const
{
	if (completion.get_error())
	{
		throw image_file_error(
			file.path + ": zarr_read_operation: The file could not be "
			"read: " + completion.get_error().message()
		);
	}
	if (completion.get_size() < expected)
	{
		throw image_file_format_error(
			file.path + ": zarr_read_operation: The file ends before the "
			"bytes it was to be read for."
		);
	}
}

void zarr_read_operation::close_after_read(unit &file)
{
	VITRIO_ASSERT(file.reads > 0);
	if (--file.reads == 0)
	{
		file.file.reset();
	}
}

void zarr_read_operation::finish_part(
	file_transfer_scope &scope,
	std::size_t id
)
{
	VITRIO_ASSERT(id < m_parts.size());
	std::vector<byte>().swap(m_parts[id].encoded);

	const auto index = m_parts[id].unit;
	auto &file = m_units[index];
	if (--file.undecoded == 0)
	{
		file.file.reset();
		m_reserved -= get_unit_bytes(index);
		--m_active;
		admit(scope);
	}
}

void zarr_read_operation::place_chunks(
	file_transfer_scope &scope,
	std::size_t index
)
{
	locate_chunks(index);

	auto &file = m_units[index];
	for (const auto id : file.parts)
	{
		auto &current = m_parts[id];
		if (current.missing || current.encoded.empty())
		{
			run(scope, id, step_kind::decode);
		}
		else
		{
			queue_read(
				scope, file, current.offset, current.encoded, id,
				step_kind::read_chunk);
		}
	}

	if (file.reads == 0)
	{
		file.file.reset();
	}
}

std::uint64_t zarr_read_operation::get_index_offset(const unit &file) const
{
	const auto *const shards = m_layout->get_shard_layout();
	VITRIO_ASSERT(shards != nullptr);
	return shards->get_index_location() == zarr_index_location::start
		? 0
		: file.size - shards->get_index_byte_size();
}

void zarr_read_operation::locate_chunks(std::size_t index) const
{
	auto &file = m_units[index];
	const zarr_shard_index shard(
		make_span(
			static_cast<const byte*>(file.index.data()), file.index.size()),
		*m_layout->get_shard_layout(),
		file.size,
		file.path
	);

	for (const auto id : file.parts)
	{
		auto &current = m_parts[id];
		const auto entry = shard.get_entry(current.entry);
		if (!entry.stored)
		{
			current.missing = true;
			continue;
		}
		if (entry.size > m_encoded_limit)
		{
			throw image_file_format_error(
				file.path + ": zarr_read_operation: A chunk of the shard "
				"holds more bytes than any encoding of it."
			);
		}

		current.offset = entry.offset;
		current.encoded.resize(static_cast<std::size_t>(entry.size));
	}
}

std::size_t zarr_read_operation::get_unit_bytes(std::size_t index) const
{
	// A chunk held takes what it was read into and what it decodes into.
	return m_units[index].parts.size() * 2 *
		std::max<std::size_t>(1, m_layout->get_chunk_layout().get_byte_size());
}

void zarr_read_operation::open(std::size_t index) const
{
	auto &file = m_units[index];

	boost::system::error_code error;
	const auto size = boost::filesystem::file_size(file.path, error);
	if (error)
	{
		if (error == boost::system::errc::no_such_file_or_directory)
		{
			file.missing = true;
			return;
		}

		throw image_file_error(
			file.path + ": zarr_read_operation: The file could not be "
			"reached: " + error.message()
		);
	}
	file.size = size;

	const auto *const shards = m_layout->get_shard_layout();
	if (shards != nullptr)
	{
		if (size < shards->get_index_byte_size())
		{
			throw image_file_format_error(
				file.path + ": zarr_read_operation: The shard is shorter "
				"than its index."
			);
		}
		file.index.resize(shards->get_index_byte_size());
	}
	else
	{
		if (size > m_encoded_limit)
		{
			throw image_file_format_error(
				file.path + ": zarr_read_operation: The file of the chunk "
				"holds more bytes than any encoding of the chunk."
			);
		}
		m_parts[file.parts.front()].encoded.resize(
			static_cast<std::size_t>(size));
	}

	try
	{
		file.file.reset(new boost::interprocess::file_mapping(
			file.path.c_str(), boost::interprocess::read_only));
	}
	catch (const boost::interprocess::interprocess_exception &exception)
	{
		throw image_file_error(
			file.path + ": zarr_read_operation: The file could not be "
			"opened: " + exception.what()
		);
	}
}

void zarr_read_operation::read_whole(std::size_t index) const
{
	open(index);

	auto &file = m_units[index];
	const auto read = [&file] (std::uint64_t offset, std::vector<byte> &buffer)
	{
		if (buffer.empty())
		{
			return;
		}

		const memory_range range(buffer.data(), buffer.size());
		std::error_code error;
		const auto size = read_scattered(
			file.file->get_mapping_handle().handle,
			offset,
			make_span(&range, 1),
			error
		);
		if (error)
		{
			throw image_file_error(
				file.path + ": zarr_read_operation: The file could not be "
				"read: " + error.message()
			);
		}
		if (size < buffer.size())
		{
			throw image_file_format_error(
				file.path + ": zarr_read_operation: The file ends before "
				"the bytes it was to be read for."
			);
		}
	};

	if (file.missing)
	{
		for (const auto id : file.parts)
		{
			m_parts[id].missing = true;
		}
	}
	else if (m_layout->get_shard_layout() != nullptr)
	{
		read(get_index_offset(file), file.index);
		locate_chunks(index);
	}

	// A chunk at a time, so that a shard holds no more than one of its
	// chunks in memory.
	for (const auto id : file.parts)
	{
		auto &current = m_parts[id];
		if (!current.missing)
		{
			read(current.offset, current.encoded);
		}
		decode(id);
		std::vector<byte>().swap(current.encoded);
	}

	file.file.reset();
}

void zarr_read_operation::decode(std::size_t id) const
{
	const auto &current = m_parts[id];
	const auto &file = m_units[current.unit];
	const auto &chunk = m_layout->get_chunk_layout();
	const auto &target = m_destination.get_descriptor();

	// A chunk that is not stored is its fill value everywhere: one element
	// that every stride of zero reads again.
	std::vector<byte> decoded;
	const byte *data;
	std::vector<std::ptrdiff_t> fill_strides;
	auto strides = chunk.get_strides();
	auto order = chunk.get_byte_order();
	if (current.missing)
	{
		data = m_layout->get_fill_element().data();
		fill_strides.assign(chunk.get_extents().size(), 0);
		strides = make_span(fill_strides);
		order = get_system_byte_order();
	}
	else if (m_layout->get_codecs().is_empty())
	{
		if (current.encoded.size() != chunk.get_byte_size())
		{
			throw image_file_format_error(
				file.path + ": zarr_read_operation: A chunk does not hold as "
				"many bytes as its values."
			);
		}
		data = current.encoded.data();
	}
	else
	{
		try
		{
			m_layout->get_codecs().decode(
				make_span(
					static_cast<const byte*>(current.encoded.data()),
					current.encoded.size()),
				chunk.get_byte_size(),
				decoded
			);
		}
		catch (const image_file_format_error &error)
		{
			throw image_file_format_error(file.path + ": " + error.what());
		}
		data = decoded.data();
	}

	const auto position = current.position;
	for (std::size_t plan = 0; plan < m_chunks.get_plan_count(position); ++plan)
	{
		const image_region_read_walk walk(
			m_chunks.get_regions(position, plan),
			chunk.get_extents(),
			strides,
			target.get_extents(),
			target.get_strides(),
			target.get_offset()
		);
		read_regions(
			walk,
			m_array_data,
			target.get_data_type(),
			data,
			chunk.get_data_type(),
			order
		);
	}
}

} // namespace zarr
} // namespace vitrio
