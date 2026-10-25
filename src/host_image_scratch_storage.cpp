// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/host_image_scratch_storage.hpp>

#include "image_scratch_storage_bounds.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_scratch_storage.hpp>

#include <array/array_data.hpp>
#include <memory/align.hpp>
#include <memory/aligned_alloc.hpp>
#include <memory/byte_order.hpp>
#include <strided_transfer/image_region_read_walk.hpp>
#include <strided_transfer/image_region_transfer.hpp>
#include <system/page_size.hpp>

#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

const char *const name = "host_image_scratch_storage";

// The memory is owned apart from the storage, so that the arrays a load
// reads into keep it alive until the load is done.
std::shared_ptr<byte> allocate(std::size_t size)
{
	if (size == 0)
	{
		throw std::invalid_argument(
			"create_host_image_scratch_storage: The size must not be zero."
		);
	}

	const auto alignment = get_page_size();
	auto *data = aligned_alloc(align_ceil(size, alignment), alignment);
	if (data == nullptr)
	{
		throw std::bad_alloc();
	}

	return std::shared_ptr<byte>(
		static_cast<byte*>(data),
		[](byte *released) noexcept
		{
			aligned_free(released);
		}
	);
}

class host_image_scratch_storage final
	: public image_scratch_storage
{
public:
	explicit host_image_scratch_storage(std::size_t size)
		: m_memory(allocate(size))
		, m_size(size)
	{
	}

	~host_image_scratch_storage() override = default;

	std::size_t get_size() const noexcept override
	{
		return m_size;
	}

	void read(std::size_t offset, span<byte> bytes) const override
	{
		check_bytes_within(offset, bytes.size(), m_size, name);
		std::memcpy(bytes.data(), m_memory.get() + offset, bytes.size());
	}

	void write(std::size_t offset, span<const byte> bytes) override
	{
		check_bytes_within(offset, bytes.size(), m_size, name);
		std::memcpy(m_memory.get() + offset, bytes.data(), bytes.size());
	}

	// The bytes are copied before this returns, which reports.
	void write_async(
		std::size_t offset,
		span<const byte> bytes,
		std::shared_ptr<completion_notifier> done
	) override
	{
		if (!done)
		{
			throw std::invalid_argument(
				"host_image_scratch_storage: The notifier must not be null."
			);
		}

		try
		{
			write(offset, bytes);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		done->notify_done();
	}

	// The values are in memory, so they are copied out before this
	// returns, which is when it reports.
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
				"host_image_scratch_storage: The notifier must not be null."
			);
		}

		try
		{
			check_values_within(values, m_size, name);
			const auto element_size = static_cast<std::ptrdiff_t>(
				vitrio::get_size(values.get_data_type())
			);
			auto *destination_data = get_array_data(array_ref(destination));
			const auto &target = destination.get_descriptor();
			const image_region_read_walk walk(
				regions,
				values.get_extents(),
				values.get_strides(),
				target.get_extents(),
				target.get_strides(),
				target.get_offset()
			);
			read_regions(
				walk,
				destination_data,
				target.get_data_type(),
				m_memory.get() + values.get_offset() * element_size,
				values.get_data_type(),
				get_system_byte_order()
			);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		done->notify_done();
	}

	// The file is read straight into the memory.
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
				"host_image_scratch_storage: The notifier must not be null."
			);
		}

		// The array refuses values that lie outside the memory.
		array target;
		try
		{
			if (!file)
			{
				throw std::invalid_argument(
					"host_image_scratch_storage: The file must not be null."
				);
			}
			target = array(
				make_span(m_memory.get(), m_size),
				m_memory,
				values
			);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		file->read_async(
			std::move(target),
			std::move(regions),
			std::move(done)
		);
	}

private:
	std::shared_ptr<byte> m_memory;
	std::size_t m_size;
};

} // anonymous namespace

std::shared_ptr<image_scratch_storage>
create_host_image_scratch_storage(std::size_t size)
{
	return std::make_shared<host_image_scratch_storage>(size);
}

} // namespace vitrio
