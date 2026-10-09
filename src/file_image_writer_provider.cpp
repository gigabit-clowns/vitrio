// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_image_writer_provider.hpp>

#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_writer.hpp>

#include <assert.hpp>

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
	explicit implementation(
		std::shared_ptr<const image_file_write_format_selector> formats
	)
		: m_formats(std::move(formats))
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
		std::shared_ptr<image_writer> writer;
		{
			const std::lock_guard<std::mutex> lock(m_mutex);

			const auto ite = m_files.find(key);
			if (ite == m_files.end())
			{
				throw std::out_of_range(
					key + ": file_image_writer_provider::close: The file "
					"is not declared."
				);
			}

			writer = ite->second.get_writer();
			m_files.erase(ite);
		}

		// Outside the lock: a flush reaches the storage, and no other file
		// needs to wait for it.
		if (writer)
		{
			writer->flush();
		}
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
					file.get_metadata()
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

		for (const auto &writer : writers)
		{
			writer->flush();
		}
	}

private:
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
	std::unordered_map<std::string, declared_file> m_files;
};

file_image_writer_provider::file_image_writer_provider(
	std::shared_ptr<const image_file_write_format_selector> formats
)
{
	if (!formats)
	{
		throw std::invalid_argument(
			"file_image_writer_provider: The format selector must not be "
			"null."
		);
	}

	m_implementation = std::make_unique<implementation>(std::move(formats));
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
