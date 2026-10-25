// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_read_operation.hpp"

#include "zarr_node.hpp"

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
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

const std::uint64_t kind_bits = 2;

// What the file of a chunk may hold beyond the chunk decoded, for a codec
// that encodes some chunks into more bytes than they decode into, as every
// compressor does a chunk it can not compress. A file larger than that holds
// no chunk of the array, and is refused before memory is taken for it.
std::size_t get_encoded_limit(std::size_t decoded)
{
	return decoded + decoded / 16 + (1UL << 16);
}

std::size_t get_unit_limit(
	std::size_t chunk_bytes,
	std::size_t byte_budget,
	std::size_t chunks_in_flight
)
{
	// A chunk held takes what it was read into and what it decodes into.
	const auto per_chunk = std::max<std::size_t>(1, 2 * chunk_bytes);
	return std::max<std::size_t>(
		1, std::min(chunks_in_flight, byte_budget / per_chunk));
}

} // anonymous namespace

// A step of one chunk, which the executor of computations runs.
class zarr_read_operation::step final
	: public task
{
public:
	step(
		const zarr_read_operation &operation,
		std::size_t unit,
		step_kind kind
	) noexcept
		: m_operation(operation)
		, m_unit(unit)
		, m_kind(kind)
	{
	}

	void run() override
	{
		switch (m_kind)
		{
		case step_kind::open:
			m_operation.open(m_unit);
			break;
		case step_kind::decode:
			m_operation.decode(m_unit);
			break;
		case step_kind::whole:
			m_operation.read_whole(m_unit);
			m_operation.decode(m_unit);
			break;
		}
	}

private:
	const zarr_read_operation &m_operation;
	std::size_t m_unit;
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
	, m_limit(get_unit_limit(
		m_layout->get_chunk_layout().get_byte_size(),
		byte_budget,
		chunks_in_flight))
	, m_encoded_limit(get_encoded_limit(
		m_layout->get_chunk_layout().get_byte_size()))
	, m_units(m_chunks.get_chunk_count())
	, m_next(0)
	, m_active(0)
{
	const auto &keys = m_layout->get_key_encoding();
	for (std::size_t unit = 0; unit < m_units.size(); ++unit)
	{
		m_units[unit].path = join_zarr_path(
			m_directory, keys.get_key(m_chunks.get_chunk(unit)));
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
	const auto index = static_cast<std::size_t>(completion.get_tag());
	VITRIO_ASSERT(index < m_units.size());
	auto &unit = m_units[index];

	if (completion.get_error())
	{
		throw image_file_error(
			unit.path + ": zarr_read_operation: The chunk could not be "
			"read: " + completion.get_error().message()
		);
	}
	if (completion.get_size() < unit.encoded.size())
	{
		throw image_file_format_error(
			unit.path + ": zarr_read_operation: The chunk ends before the "
			"size of its file says."
		);
	}

	unit.file.reset();
	run(scope, index, step_kind::decode);
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

	const auto index = static_cast<std::size_t>(tag >> kind_bits);
	const auto kind = static_cast<step_kind>(
		tag & ((std::uint64_t(1) << kind_bits) - 1));
	VITRIO_ASSERT(index < m_units.size());
	auto &unit = m_units[index];

	if (kind == step_kind::open && !unit.missing && !unit.encoded.empty())
	{
		const memory_range buffer(unit.encoded.data(), unit.encoded.size());
		scope.queue_read(
			unit.file->get_mapping_handle().handle,
			0,
			make_span(&buffer, 1),
			index
		);
		return;
	}
	if (kind == step_kind::open)
	{
		// Nothing to read: the chunk is not stored, or its file is empty,
		// which its codecs refuse.
		unit.file.reset();
		run(scope, index, step_kind::decode);
		return;
	}

	std::vector<byte>().swap(unit.encoded);
	--m_active;
	admit(scope);
}

void zarr_read_operation::admit(file_transfer_scope &scope)
{
	while (m_next < m_units.size() && m_active < m_limit)
	{
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
	std::size_t unit,
	step_kind kind
)
{
	const auto tag = static_cast<std::uint64_t>(unit) << kind_bits |
		static_cast<std::uint64_t>(kind);
	scope.run(std::make_unique<step>(*this, unit, kind), tag);
}

void zarr_read_operation::open(std::size_t index) const
{
	auto &unit = m_units[index];

	boost::system::error_code error;
	const auto size = boost::filesystem::file_size(unit.path, error);
	if (error)
	{
		if (error == boost::system::errc::no_such_file_or_directory)
		{
			unit.missing = true;
			return;
		}

		throw image_file_error(
			unit.path + ": zarr_read_operation: The chunk could not be "
			"reached: " + error.message()
		);
	}
	if (size > m_encoded_limit)
	{
		throw image_file_format_error(
			unit.path + ": zarr_read_operation: The file of the chunk holds "
			"more bytes than any encoding of the chunk."
		);
	}

	try
	{
		unit.file.reset(new boost::interprocess::file_mapping(
			unit.path.c_str(), boost::interprocess::read_only));
	}
	catch (const boost::interprocess::interprocess_exception &exception)
	{
		throw image_file_error(
			unit.path + ": zarr_read_operation: The chunk could not be "
			"opened: " + exception.what()
		);
	}

	unit.encoded.resize(static_cast<std::size_t>(size));
}

void zarr_read_operation::read_whole(std::size_t index) const
{
	open(index);

	auto &unit = m_units[index];
	if (unit.missing || unit.encoded.empty())
	{
		unit.file.reset();
		return;
	}

	const memory_range buffer(unit.encoded.data(), unit.encoded.size());
	std::error_code error;
	const auto size = read_scattered(
		unit.file->get_mapping_handle().handle,
		0,
		make_span(&buffer, 1),
		error
	);
	unit.file.reset();

	if (error)
	{
		throw image_file_error(
			unit.path + ": zarr_read_operation: The chunk could not be "
			"read: " + error.message()
		);
	}
	if (size < unit.encoded.size())
	{
		throw image_file_format_error(
			unit.path + ": zarr_read_operation: The chunk ends before the "
			"size of its file says."
		);
	}
}

void zarr_read_operation::decode(std::size_t index) const
{
	const auto &unit = m_units[index];
	const auto &chunk = m_layout->get_chunk_layout();
	const auto &target = m_destination.get_descriptor();

	// A chunk that is not stored is its fill value everywhere: one element
	// that every stride of zero reads again.
	std::vector<byte> decoded;
	const byte *data;
	std::vector<std::ptrdiff_t> fill_strides;
	auto strides = chunk.get_strides();
	auto order = chunk.get_byte_order();
	if (unit.missing)
	{
		data = m_layout->get_fill_element().data();
		fill_strides.assign(chunk.get_extents().size(), 0);
		strides = make_span(fill_strides);
		order = get_system_byte_order();
	}
	else if (m_layout->get_codecs().is_empty())
	{
		if (unit.encoded.size() != chunk.get_byte_size())
		{
			throw image_file_format_error(
				unit.path + ": zarr_read_operation: The chunk does not hold "
				"as many bytes as its values."
			);
		}
		data = unit.encoded.data();
	}
	else
	{
		try
		{
			m_layout->get_codecs().decode(
				make_span(
					static_cast<const byte*>(unit.encoded.data()),
					unit.encoded.size()),
				chunk.get_byte_size(),
				decoded
			);
		}
		catch (const image_file_format_error &error)
		{
			throw image_file_format_error(unit.path + ": " + error.what());
		}
		data = decoded.data();
	}

	for (std::size_t plan = 0; plan < m_chunks.get_plan_count(index); ++plan)
	{
		const image_region_read_walk walk(
			m_chunks.get_regions(index, plan),
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
