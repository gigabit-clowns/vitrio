// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_image_scratch_storage.hpp>

#include "file_image_scratch_storage_creation.hpp"
#include "file_transfer_pool_implementation.hpp"
#include "image_scratch_storage_bounds.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/counting_notifier.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <array/array_memory.hpp>
#include <concurrency/failure_forwarding_notifier.hpp>
#include <file_transfer/file_transfer_executor.hpp>
#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/queued/make_image_read_operation.hpp>
#include <formats/raw/queued/make_image_write_operation.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <memory/byte_order.hpp>
#include <system/file_space_reservation.hpp>
#include <system/scattered_file_read.hpp>
#include <system/scattered_file_write.hpp>
#include <system/system_memory.hpp>
#include <tuning.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/file_mapping.hpp>

#include <assert.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <exception>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

const char *const name = "file_image_scratch_storage";

// Makes sure that a file of a size is at a path, and opens it. A file that
// is already there is never emptied, since another storage may share it.
std::shared_ptr<writable_image_file> lay_out_file(
	const std::string &path,
	std::size_t size
)
{
	if (size == 0)
	{
		throw image_file_error(
			path + ": create_file_image_scratch_storage: A file of no bytes "
			"holds no image."
		);
	}

	{
		// Opening a file to append creates it if it is missing, and leaves
		// it as it is otherwise.
		std::filebuf file;
		const auto opened = file.open(
			path,
			std::ios_base::out | std::ios_base::app | std::ios_base::binary
		);
		if (opened == nullptr)
		{
			throw image_file_error(
				path + ": create_file_image_scratch_storage: The file could "
				"not be created."
			);
		}
	}

	boost::system::error_code resized;
	boost::filesystem::resize_file(path, size, resized);
	if (resized)
	{
		throw image_file_error(
			path + ": create_file_image_scratch_storage: The file could not "
			"be sized: " + resized.message()
		);
	}

	auto file = std::make_shared<writable_image_file>(path);

	std::error_code reserved;
	reserve_file_space(file->get_file(), size, reserved);
	if (reserved)
	{
		throw image_file_error(
			path + ": create_file_image_scratch_storage: The file could not "
			"be given its room on the disk: " + reserved.message()
		);
	}

	return file;
}

// The values of the block, as the raw formats lay out the values of a file:
// a stack of what follows their first axis.
raw_image_layout make_layout(const array_descriptor &values)
{
	const auto extents = values.get_extents();
	const auto strides = values.get_strides();
	const auto data_type = values.get_data_type();

	return raw_image_layout(
		image_descriptor(
			extents,
			std::max<std::size_t>(extents.size(), 2) - 1,
			data_type
		),
		std::vector<std::ptrdiff_t>(strides.begin(), strides.end()),
		static_cast<std::size_t>(values.get_offset()) * get_size(data_type),
		get_system_byte_order()
	);
}

// What a piece of a load reads the file into before it writes the block,
// once it starts: the box of values its regions land in, and its regions as
// they land in the box and as the box lands among the values.
struct staged_load
{
	array_descriptor staging;
	image_transfer_plan into_staging;
	image_transfer_plan out_of_staging;
};

staged_load stage(
	const array_descriptor &values,
	const image_transfer_plan &regions
)
{
	const auto &shape = regions.get_shape();
	const auto rank = shape.get_array_rank();
	if (rank != values.get_extents().size())
	{
		throw std::invalid_argument(
			"file_image_scratch_storage: The plan does not have the rank of "
			"the values."
		);
	}

	const auto region_count = regions.get_region_count();
	std::vector<std::size_t> low(
		rank,
		std::numeric_limits<std::size_t>::max()
	);
	std::vector<std::size_t> high(rank, 0);
	for (std::size_t region = 0; region < region_count; ++region)
	{
		const auto offset = regions.get_array_offset(region);
		for (std::size_t axis = 0; axis < rank; ++axis)
		{
			low[axis] = std::min(low[axis], offset[axis]);
			high[axis] = std::max(
				high[axis],
				offset[axis] + shape.get_extent(rank, axis)
			);
		}
	}

	std::vector<std::size_t> extents(rank);
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		extents[axis] = high[axis] - low[axis];
	}

	staged_load load{
		make_contiguous_array_descriptor(
			make_span(extents),
			values.get_data_type()
		),
		image_transfer_plan(shape),
		image_transfer_plan(
			image_transfer_shape(
				std::vector<std::size_t>(
					shape.get_extents().begin(),
					shape.get_extents().end()
				),
				rank,
				rank
			)
		),
	};
	load.into_staging.reserve(region_count);
	load.out_of_staging.reserve(region_count);

	std::vector<std::size_t> staged(rank);
	for (std::size_t region = 0; region < region_count; ++region)
	{
		const auto offset = regions.get_array_offset(region);
		for (std::size_t axis = 0; axis < rank; ++axis)
		{
			staged[axis] = offset[axis] - low[axis];
		}
		load.into_staging.add(
			regions.get_file_offset(region),
			make_span(staged)
		);
		load.out_of_staging.add(offset, make_span(staged));
	}

	return load;
}

// The regions of a load in pieces of consecutive regions, each of which
// stages at most a number of bytes, or a single region that alone stages
// more.
std::vector<image_transfer_plan> split_into_pieces(
	const array_descriptor &values,
	const image_transfer_plan &regions,
	std::size_t piece_bytes
)
{
	const auto &shape = regions.get_shape();
	const auto rank = shape.get_array_rank();
	std::size_t region_bytes = vitrio::get_size(values.get_data_type());
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		region_bytes *= shape.get_extent(rank, axis);
	}
	const auto per_piece = std::max<std::size_t>(
		piece_bytes / std::max<std::size_t>(region_bytes, 1),
		1
	);

	std::vector<image_transfer_plan> pieces;
	const auto region_count = regions.get_region_count();
	pieces.reserve((region_count + per_piece - 1) / per_piece);
	for (std::size_t first = 0; first < region_count; first += per_piece)
	{
		const auto end = std::min(region_count, first + per_piece);
		image_transfer_plan piece(shape);
		piece.reserve(end - first);
		for (auto region = first; region < end; ++region)
		{
			piece.add(
				regions.get_file_offset(region),
				regions.get_array_offset(region)
			);
		}
		pieces.push_back(std::move(piece));
	}

	return pieces;
}

// A piece of a load: regions of a file, read into memory of their own and
// written from there into the block.
struct staged_piece
{
	std::shared_ptr<writable_image_file> block;
	raw_image_layout layout;
	std::shared_ptr<file_transfer_executor> transfers;
	std::shared_ptr<const image_reader> file;
	staged_load load;
	std::size_t bytes;
	// Reports the piece, once it has been written or has failed.
	std::shared_ptr<completion_notifier> done;
};

// What reading a file into the staging of a piece reports to: the staging is
// then written to the block, which reports the piece.
class staged_write_notifier final
	: public completion_notifier
{
public:
	staged_write_notifier(
		std::unique_ptr<staged_piece> piece,
		array staging
	) noexcept
		: m_piece(std::move(piece))
		, m_staging(std::move(staging))
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		auto &piece = *m_piece;
		if (error)
		{
			piece.done->notify_done(std::move(error));
			return;
		}

		std::unique_ptr<file_transfer_operation> write;
		try
		{
			write = make_image_write_operation(
				piece.block,
				piece.layout,
				m_staging.share_const(),
				piece.load.out_of_staging,
				get_staging_byte_budget()
			);
		}
		catch (...)
		{
			piece.done->notify_done(std::current_exception());
			return;
		}

		piece.transfers->submit(std::move(write), std::move(piece.done));
	}

private:
	std::unique_ptr<staged_piece> m_piece;
	array m_staging;
};

// Starts a piece: makes its staging, and reads its regions of the file into
// it. The staging is memory straight from the system: threads that allocate
// and free a piece after another through the allocator of the process leave
// each of its arenas holding the most it ever held, which kept several times
// the budget of the loads resident.
class piece_start_task final
	: public task
{
public:
	explicit piece_start_task(std::unique_ptr<staged_piece> piece) noexcept
		: m_piece(std::move(piece))
	{
	}

	void run() override
	{
		const auto bytes = std::max<std::size_t>(m_piece->bytes, 1);
		auto memory = allocate_system_memory(bytes);
		auto *data = memory.get();
		array staging(
			make_span(data, bytes),
			std::move(memory),
			m_piece->load.staging
		);
		auto into_staging = std::move(m_piece->load.into_staging);
		const auto file = m_piece->file;
		auto staged = std::make_shared<staged_write_notifier>(
			std::move(m_piece),
			staging.share()
		);
		file->read_async(
			std::move(staging),
			std::move(into_staging),
			std::move(staged)
		);
	}

private:
	std::unique_ptr<staged_piece> m_piece;
};

// Starts a piece on the threads of computations of the transfers rather than
// on the thread that lets it in, which may carry out transfers, having
// finished an earlier piece: reading a file may block, as a synchronous
// format's reads do.
class piece_start_operation final
	: public file_transfer_operation
{
public:
	explicit piece_start_operation(std::unique_ptr<staged_piece> piece) noexcept
		: m_piece(std::move(piece))
	{
	}

	void start(file_transfer_scope &scope) override
	{
		scope.run(std::make_unique<piece_start_task>(std::move(m_piece)), 0);
	}

	void on_transfer(
		const file_transfer_completion &,
		file_transfer_scope &
	) override
	{
		VITRIO_ASSERT(false);
	}

	void on_task(
		std::uint64_t,
		std::exception_ptr error,
		file_transfer_scope &
	) override
	{
		if (error)
		{
			std::rethrow_exception(error);
		}
	}

private:
	std::unique_ptr<staged_piece> m_piece;
};

// The pieces of the loads of a storage, let in while what they stage stays
// within a budget. A piece that does not fit waits for earlier ones to
// finish, in the order they came; one that alone stages more than the budget
// is let in alone.
class staged_load_admission final
{
public:
	staged_load_admission(
		std::size_t budget,
		std::shared_ptr<file_transfer_executor> transfers
	) noexcept
		: m_budget(budget)
		, m_transfers(std::move(transfers))
	{
	}

	// Lets a piece in, now or once there is room for it.
	void admit(std::unique_ptr<staged_piece> piece)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_waiting.push_back(std::move(piece));
		let_in(lock);
	}

	// Takes back what a piece that finished staged, and lets in the pieces
	// there is room for now.
	void release(std::size_t bytes) noexcept
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		VITRIO_ASSERT(bytes <= m_staged);
		m_staged -= bytes;
		let_in(lock);
	}

private:
	// Starts the pieces that fit, one at a time and outside the lock. A
	// piece that fails to start is reported, which releases it, on this
	// thread: that call finds the pieces being let in, and leaves them to
	// this one.
	void let_in(std::unique_lock<std::mutex> &lock) noexcept
	{
		if (m_letting_in)
		{
			return;
		}

		m_letting_in = true;
		while (!m_waiting.empty() && fits(m_waiting.front()->bytes))
		{
			auto piece = std::move(m_waiting.front());
			m_waiting.pop_front();
			m_staged += piece->bytes;

			lock.unlock();
			start(std::move(piece));
			lock.lock();
		}
		m_letting_in = false;
	}

	bool fits(std::size_t bytes) const noexcept
	{
		return m_staged == 0 || m_staged + bytes <= m_budget;
	}

	void start(std::unique_ptr<staged_piece> piece) noexcept
	{
		const auto done = piece->done;
		try
		{
			m_transfers->submit(
				std::make_unique<piece_start_operation>(std::move(piece)),
				std::make_shared<failure_forwarding_notifier>(done)
			);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
		}
	}

	std::mutex m_mutex;
	std::size_t m_budget;
	std::size_t m_staged = 0;
	bool m_letting_in = false;
	std::deque<std::unique_ptr<staged_piece>> m_waiting;
	std::shared_ptr<file_transfer_executor> m_transfers;
};

// What a piece reports to: it gives back what the piece staged, then reports
// to the load.
class piece_release_notifier final
	: public completion_notifier
{
public:
	piece_release_notifier(
		std::shared_ptr<staged_load_admission> admission,
		std::size_t bytes,
		std::shared_ptr<completion_notifier> done
	) noexcept
		: m_admission(std::move(admission))
		, m_bytes(bytes)
		, m_done(std::move(done))
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		m_admission->release(m_bytes);
		m_done->notify_done(std::move(error));
	}

private:
	std::shared_ptr<staged_load_admission> m_admission;
	std::size_t m_bytes;
	std::shared_ptr<completion_notifier> m_done;
};

class file_image_scratch_storage final
	: public image_scratch_storage
{
public:
	file_image_scratch_storage(
		const std::string &path,
		std::size_t size,
		std::shared_ptr<file_transfer_executor> transfers,
		std::size_t piece_bytes,
		std::size_t load_bytes
	)
		: m_file(lay_out_file(path, size))
		, m_size(size)
		, m_transfers(std::move(transfers))
		, m_piece_bytes(piece_bytes)
		, m_admission(
			std::make_shared<staged_load_admission>(load_bytes, m_transfers)
		)
	{
		VITRIO_ASSERT(m_transfers);
		VITRIO_ASSERT(m_piece_bytes > 0);
		VITRIO_ASSERT(load_bytes > 0);
	}

	~file_image_scratch_storage() override = default;

	std::size_t get_size() const noexcept override
	{
		return m_size;
	}

	void read(std::size_t offset, span<byte> bytes) const override
	{
		check_bytes_within(offset, bytes.size(), m_size, name);

		const memory_range buffer(bytes.data(), bytes.size());
		std::error_code error;
		const auto size = read_scattered(
			m_file->get_file(),
			offset,
			make_span(&buffer, 1),
			error
		);
		if (error || size < bytes.size())
		{
			throw image_file_error(
				m_file->get_path() + ": file_image_scratch_storage: The file "
				"could not be read: " +
				(error ? error.message() : std::string("it is too short"))
			);
		}
	}

	void write(std::size_t offset, span<const byte> bytes) override
	{
		check_bytes_within(offset, bytes.size(), m_size, name);

		const const_memory_range buffer(bytes.data(), bytes.size());
		std::error_code error;
		write_scattered(
			m_file->get_file(),
			offset,
			make_span(&buffer, 1),
			error
		);
		if (error)
		{
			throw image_file_error(
				m_file->get_path() + ": file_image_scratch_storage: The file "
				"could not be written: " + error.message()
			);
		}
	}

	// The values are read out of the file, as a reader of a raw file reads
	// its values.
	void serve_async(
		const array_descriptor &values,
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const override
	{
		if (!done)
		{
			throw std::invalid_argument(
				"file_image_scratch_storage: The notifier must not be null."
			);
		}

		std::unique_ptr<file_transfer_operation> operation;
		try
		{
			check_values_within(values, m_size, name);
			operation = make_image_read_operation(
				m_file->get_path(),
				std::shared_ptr<const boost::interprocess::file_mapping>(
					m_file,
					&m_file->get_open_file()
				),
				m_size,
				make_layout(values),
				std::move(destination),
				regions,
				get_staging_byte_budget()
			);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		m_transfers->submit(std::move(operation), std::move(done));
	}

	// The regions are read out of the file into memory of their own, and
	// written from there into the block, a piece at a time. The pieces of
	// the loads of the storage stage a budget at once at most.
	void load_async(
		std::shared_ptr<const image_reader> file,
		const array_descriptor &values,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) override
	{
		if (!done)
		{
			throw std::invalid_argument(
				"file_image_scratch_storage: The notifier must not be null."
			);
		}

		if (regions.get_region_count() == 0)
		{
			done->notify_done();
			return;
		}

		std::vector<std::unique_ptr<staged_piece>> pieces;
		std::shared_ptr<completion_notifier> all;
		try
		{
			if (!file)
			{
				throw std::invalid_argument(
					"file_image_scratch_storage: The file must not be null."
				);
			}
			check_values_within(values, m_size, name);

			const auto layout = make_layout(values);
			auto plans = split_into_pieces(values, regions, m_piece_bytes);
			all = std::make_shared<counting_notifier>(plans.size(), done);
			pieces.reserve(plans.size());
			for (const auto &plan : plans)
			{
				auto load = stage(values, plan);
				const auto bytes = compute_storage_requirement(load.staging);
				pieces.push_back(
					std::make_unique<staged_piece>(
						staged_piece{
							m_file,
							layout,
							m_transfers,
							file,
							std::move(load),
							bytes,
							std::make_shared<piece_release_notifier>(
								m_admission,
								bytes,
								all
							),
						}
					)
				);
			}
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		// A piece that can not be let in has staged nothing, and is
		// reported as failed.
		for (auto &piece : pieces)
		{
			try
			{
				m_admission->admit(std::move(piece));
			}
			catch (...)
			{
				all->notify_done(std::current_exception());
			}
		}
	}

private:
	// The transfers under way share it, and the last to let it go closes it.
	std::shared_ptr<writable_image_file> m_file;
	std::size_t m_size;
	std::shared_ptr<file_transfer_executor> m_transfers;
	std::size_t m_piece_bytes;
	std::shared_ptr<staged_load_admission> m_admission;
};

} // anonymous namespace

std::shared_ptr<image_scratch_storage> create_file_image_scratch_storage(
	const std::string &path,
	std::size_t size,
	std::shared_ptr<file_transfer_executor> transfers,
	std::size_t piece_bytes,
	std::size_t load_bytes
)
{
	return std::make_shared<file_image_scratch_storage>(
		path,
		size,
		std::move(transfers),
		piece_bytes,
		load_bytes
	);
}

std::shared_ptr<image_scratch_storage> create_file_image_scratch_storage(
	const std::string &path,
	std::size_t size,
	const file_transfer_pool &transfers
)
{
	return create_file_image_scratch_storage(
		path,
		size,
		transfers.get_implementation().get_executor(),
		get_staging_byte_budget(),
		get_scratch_load_byte_budget()
	);
}

} // namespace vitrio
