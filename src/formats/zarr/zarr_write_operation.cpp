// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_write_operation.hpp"

#include <vitrio/concurrency/task.hpp>

#include <assert.hpp>

#include <utility>

namespace vitrio
{
namespace zarr
{

class zarr_write_operation::chunk_write final
	: public task
{
public:
	chunk_write(
		const zarr_write_operation &operation,
		std::size_t position
	) noexcept
		: m_operation(operation)
		, m_position(position)
	{
	}

	void run() override
	{
		m_operation.write(m_position);
	}

private:
	const zarr_write_operation &m_operation;
	std::size_t m_position;
};

zarr_write_operation::zarr_write_operation(
	std::shared_ptr<zarr_chunk_store> store,
	const_array source,
	image_chunk_regions chunks
)
	: m_store(std::move(store))
	, m_source(std::move(source))
	, m_chunks(std::move(chunks))
{
}

zarr_write_operation::~zarr_write_operation() = default;

void zarr_write_operation::start(file_transfer_scope &scope)
{
	for (std::size_t position = 0; position < m_chunks.get_chunk_count();
		++position)
	{
		scope.run(std::make_unique<chunk_write>(*this, position), position);
	}
}

void zarr_write_operation::on_transfer(
	const file_transfer_completion &,
	file_transfer_scope &
)
{
	// It queues no transfer: the tasks write the files of the chunks.
	VITRIO_ASSERT(false);
}

void zarr_write_operation::on_task(
	std::uint64_t,
	std::exception_ptr error,
	file_transfer_scope &
)
{
	if (error)
	{
		std::rethrow_exception(error);
	}
}

void zarr_write_operation::write(std::size_t position) const
{
	std::vector<const image_transfer_plan*> pieces;
	for (std::size_t plan = 0; plan < m_chunks.get_plan_count(position);
		++plan)
	{
		pieces.push_back(&m_chunks.get_regions(position, plan));
	}

	m_store->write(m_chunks.get_chunk(position), pieces, m_source);
}

} // namespace zarr
} // namespace vitrio
