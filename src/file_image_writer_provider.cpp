// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_image_writer_provider.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_writer.hpp>

#include <assert.hpp>
#include <waiting_transfers.hpp>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vitrio
{

class file_image_writer_provider::implementation
{
public:
	implementation(
		std::shared_ptr<const image_file_write_format_selector> formats,
		std::shared_ptr<const file_transfer_pool> transfers
	)
		: m_formats(std::move(formats))
		, m_transfers(std::move(transfers))
	{
	}

	void declare(
		std::string key,
		image_descriptor descriptor,
		const image_metadata &metadata
	)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto ite = m_files.find(key);
		if (ite != m_files.end())
		{
			throw std::invalid_argument(
				key + ": file_image_writer_provider::declare: The file is "
				"already declared."
			);
		}

		m_files.emplace(
			std::move(key),
			declared_file(std::move(descriptor), metadata)
		);
	}

	void close(const std::string &key)
	{
		// Outside the lock: a flush reaches the storage, and no other file
		// needs to wait for it.
		const auto writer = forget(key, "close");
		if (writer)
		{
			flush_and_wait(*writer);
		}
	}

	void close_async(
		const std::string &key,
		std::shared_ptr<completion_notifier> done
	)
	{
		if (!done)
		{
			throw std::invalid_argument(
				"file_image_writer_provider::close_async: The notifier must "
				"not be null."
			);
		}

		std::shared_ptr<image_writer> writer;
		try
		{
			writer = forget(key, "close_async");
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		if (!writer)
		{
			done->notify_done();
			return;
		}

		// What the flush reports to keeps the writer until it has.
		auto &flushed = *writer;
		std::shared_ptr<completion_notifier> closing;
		try
		{
			closing = std::make_shared<closing_notifier>(
				std::move(writer),
				done
			);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}
		flushed.flush_async(std::move(closing));
	}

	std::size_t get_file_count() const noexcept
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_files.size();
	}

	std::shared_ptr<image_writer> acquire(const std::string &key)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto ite = m_files.find(key);
		if (ite == m_files.end())
		{
			throw std::out_of_range(
				key + ": file_image_writer_provider::acquire: The file "
				"is not declared."
			);
		}

		auto &file = ite->second;
		if (!file.get_writer())
		{
			file.set_writer(
				m_formats->open(
					ite->first,
					file.get_descriptor(),
					file.get_metadata(),
					*m_transfers
				)
			);
			VITRIO_ASSERT(file.get_writer());
		}

		return file.get_writer();
	}

	void flush()
	{
		std::vector<std::shared_ptr<image_writer>> writers;
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			writers.reserve(m_files.size());
			for (const auto &file : m_files)
			{
				if (file.second.get_writer())
				{
					writers.push_back(file.second.get_writer());
				}
			}
		}

		// Every flush is under way at once, and each one completes whatever
		// another fails.
		const auto done = std::make_shared<counting_completion>(writers.size());
		for (const auto &writer : writers)
		{
			writer->flush_async(done);
		}
		done->get();
	}

private:
	// What the flush of a file being closed reports to: the writer is
	// dropped, which gives its file back, and then the close is reported.
	class closing_notifier final
		: public completion_notifier
	{
	public:
		closing_notifier(
			std::shared_ptr<image_writer> writer,
			std::shared_ptr<completion_notifier> done
		) noexcept
			: m_writer(std::move(writer))
			, m_done(std::move(done))
		{
		}

		void notify_done(std::exception_ptr error) override
		{
			m_writer.reset();
			m_done->notify_done(std::move(error));
		}

	private:
		std::shared_ptr<image_writer> m_writer;
		std::shared_ptr<completion_notifier> m_done;
	};

	// Forgets the declaration of a file, and gives back its writer, if it
	// was ever created.
	std::shared_ptr<image_writer> forget(
		const std::string &key,
		const char *caller
	)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto ite = m_files.find(key);
		if (ite == m_files.end())
		{
			throw std::out_of_range(
				key + ": file_image_writer_provider::" + caller + ": The "
				"file is not declared."
			);
		}

		auto writer = ite->second.get_writer();
		m_files.erase(ite);
		return writer;
	}

	// What a file was declared as, and its writer once it is created. Its
	// key is what it is stored under.
	class declared_file
	{
	public:
		declared_file(image_descriptor descriptor, image_metadata metadata)
			: m_descriptor(std::move(descriptor))
			, m_metadata(std::move(metadata))
		{
		}

		const image_descriptor& get_descriptor() const noexcept
		{
			return m_descriptor;
		}

		const image_metadata& get_metadata() const noexcept
		{
			return m_metadata;
		}

		const std::shared_ptr<image_writer>& get_writer() const noexcept
		{
			return m_writer;
		}

		void set_writer(std::shared_ptr<image_writer> writer) noexcept
		{
			m_writer = std::move(writer);
		}

	private:
		image_descriptor m_descriptor;
		image_metadata m_metadata;
		std::shared_ptr<image_writer> m_writer;
	};

	mutable std::mutex m_mutex;
	std::shared_ptr<const image_file_write_format_selector> m_formats;
	std::shared_ptr<const file_transfer_pool> m_transfers;
	std::unordered_map<std::string, declared_file> m_files;
};

file_image_writer_provider::file_image_writer_provider(
	std::shared_ptr<const image_file_write_format_selector> formats,
	std::shared_ptr<const file_transfer_pool> transfers
)
{
	if (!formats)
	{
		throw std::invalid_argument(
			"file_image_writer_provider: The format selector must not be "
			"null."
		);
	}

	if (!transfers)
	{
		throw std::invalid_argument(
			"file_image_writer_provider: The pool of file transfers must not "
			"be null."
		);
	}

	m_implementation = std::make_unique<implementation>(
		std::move(formats),
		std::move(transfers)
	);
}

file_image_writer_provider::~file_image_writer_provider() = default;

void file_image_writer_provider::declare(
	std::string key,
	image_descriptor descriptor,
	const image_metadata &metadata
)
{
	m_implementation->declare(
		std::move(key),
		std::move(descriptor),
		metadata
	);
}

void file_image_writer_provider::close(const std::string &key)
{
	m_implementation->close(key);
}

void file_image_writer_provider::close_async(
	const std::string &key,
	std::shared_ptr<completion_notifier> done
)
{
	m_implementation->close_async(key, std::move(done));
}

std::size_t file_image_writer_provider::get_file_count() const noexcept
{
	return m_implementation->get_file_count();
}

std::shared_ptr<image_writer>
file_image_writer_provider::acquire(const std::string &key)
{
	return m_implementation->acquire(key);
}

void file_image_writer_provider::flush()
{
	m_implementation->flush();
}

} // namespace vitrio
