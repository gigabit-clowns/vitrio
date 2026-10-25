// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_flush_operation.hpp"

#include <vitrio/concurrency/task.hpp>

#include <assert.hpp>

#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

// Even tags store chunks, odd ones sync files.
std::uint64_t store_tag(std::size_t chunk)
{
	return static_cast<std::uint64_t>(chunk) << 1;
}

std::uint64_t sync_tag(std::size_t file)
{
	return static_cast<std::uint64_t>(file) << 1 | 1;
}

} // anonymous namespace

class zarr_flush_operation::step final
	: public task
{
public:
	step(zarr_chunk_store &store, std::vector<std::size_t> chunk) noexcept
		: m_store(store)
		, m_chunk(std::move(chunk))
	{
	}

	step(zarr_chunk_store &store, std::string file) noexcept
		: m_store(store)
		, m_file(std::move(file))
	{
	}

	void run() override
	{
		if (m_file.empty())
		{
			m_store.store(make_span(m_chunk));
		}
		else
		{
			zarr_chunk_store::sync(m_file);
		}
	}

private:
	zarr_chunk_store &m_store;
	std::vector<std::size_t> m_chunk;
	std::string m_file;
};

zarr_flush_operation::zarr_flush_operation(
	std::shared_ptr<zarr_chunk_store> store
)
	: m_store(std::move(store))
	, m_storing(0)
{
}

zarr_flush_operation::~zarr_flush_operation() = default;

void zarr_flush_operation::start(file_transfer_scope &scope)
{
	m_chunks = m_store->get_held_chunks();
	m_storing = m_chunks.size();
	if (m_storing == 0)
	{
		sync_all(scope);
		return;
	}

	for (std::size_t chunk = 0; chunk < m_chunks.size(); ++chunk)
	{
		scope.run(
			std::make_unique<step>(*m_store, m_chunks[chunk]),
			store_tag(chunk));
	}
}

void zarr_flush_operation::on_transfer(
	const file_transfer_completion &,
	file_transfer_scope &
)
{
	// It queues no transfer: the tasks store and sync the files.
	VITRIO_ASSERT(false);
}

void zarr_flush_operation::on_task(
	std::uint64_t tag,
	std::exception_ptr error,
	file_transfer_scope &scope
)
{
	if (error)
	{
		std::rethrow_exception(error);
	}

	if ((tag & 1) == 0 && --m_storing == 0)
	{
		sync_all(scope);
	}
}

void zarr_flush_operation::sync_all(file_transfer_scope &scope)
{
	m_files = m_store->take_unsynced();
	for (std::size_t file = 0; file < m_files.size(); ++file)
	{
		scope.run(
			std::make_unique<step>(*m_store, m_files[file]), sync_tag(file));
	}
}

} // namespace zarr
} // namespace vitrio
