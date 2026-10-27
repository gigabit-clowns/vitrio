// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/indexed_image_scratch.hpp>

#include "image_scratch_directory.hpp"
#include "indexed_image_scratch_entry.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/file_image_scratch_storage.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/host_image_scratch_storage.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch_storage.hpp>

#include <assert.hpp>
#include <logger.hpp>
#include <memory/align.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/optional.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

std::size_t compute_slot_size(const image_descriptor &file) noexcept
{
	const auto extents = file.get_extents();
	return std::accumulate(
		extents.begin() + 1,
		extents.end(),
		get_size(file.get_data_type()),
		std::multiplies<std::size_t>()
	);
}

std::vector<std::size_t> get_named_indices(
	const image_location_grouping::group &group,
	const image_descriptor &file
)
{
	const auto index_count = file.get_extents().front();
	const auto named = group.get_indices();
	if (!named.empty() && named.back() >= index_count)
	{
		throw std::out_of_range(
			group.get_key() + ": indexed_image_scratch: A location has a "
			"stack index that the file does not have."
		);
	}

	if (!group.is_whole())
	{
		return std::vector<std::size_t>(named.begin(), named.end());
	}

	std::vector<std::size_t> indices(index_count);
	std::iota(indices.begin(), indices.end(), std::size_t(0));

	return indices;
}

array_descriptor make_values_descriptor(
	const image_descriptor &file,
	std::size_t index_count,
	std::size_t first_element
)
{
	const auto file_extents = file.get_extents();
	std::vector<std::size_t> extents(file_extents.begin(), file_extents.end());
	extents.front() = index_count;

	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return array_descriptor(
		std::move(extents),
		std::move(strides),
		static_cast<std::ptrdiff_t>(first_element),
		file.get_data_type()
	);
}

array_descriptor
make_flags_descriptor(std::size_t flag_count, std::size_t first_flag)
{
	return array_descriptor(
		std::vector<std::size_t>{flag_count},
		std::vector<std::ptrdiff_t>{1},
		static_cast<std::ptrdiff_t>(first_flag),
		numerical_type::uint64
	);
}

std::size_t
compute_run_count(std::size_t index_count, std::size_t run_length) noexcept
{
	return index_count == 0 ? 0 : (index_count - 1) / run_length + 1;
}

// When a file was last written, or zero if that can not be told.
std::uint64_t get_modification_time(const std::string &path) noexcept
{
	boost::system::error_code error;
	const auto time = boost::filesystem::last_write_time(path, error);
	if (error || time <= 0)
	{
		return 0;
	}

	return static_cast<std::uint64_t>(time);
}

// The fingerprint is an FNV-1a hash, since what the standard library and
// Boost hash to may change between their versions.
const std::uint64_t fingerprint_seed = 14695981039346656037ULL;
const std::uint64_t fingerprint_prime = 1099511628211ULL;

// To be changed whenever a scratch is laid out differently in its storage.
const std::uint64_t format_version = 1;

std::uint64_t
hash_bytes(std::uint64_t hash, const byte *bytes, std::size_t size) noexcept
{
	for (std::size_t position = 0; position < size; ++position)
	{
		hash ^= bytes[position];
		hash *= fingerprint_prime;
	}

	return hash;
}

std::uint64_t hash_value(std::uint64_t hash, std::uint64_t value) noexcept
{
	return hash_bytes(
		hash,
		reinterpret_cast<const byte*>(&value),
		sizeof(value)
	);
}

std::uint64_t read_fingerprint(const image_scratch_storage &storage)
{
	std::uint64_t fingerprint = 0;
	storage.read(
		0,
		make_span(reinterpret_cast<byte*>(&fingerprint), sizeof(fingerprint))
	);

	return fingerprint;
}

void write_fingerprint(
	image_scratch_storage &storage,
	std::uint64_t fingerprint
)
{
	storage.write(
		0,
		make_span(
			reinterpret_cast<const byte*>(&fingerprint),
			sizeof(fingerprint)
		)
	);
}

void check_storage(const image_scratch_storage *storage)
{
	if (storage == nullptr)
	{
		throw std::invalid_argument(
			"indexed_image_scratch: The storage must not be null."
		);
	}

	if (storage->get_size() < sizeof(std::uint64_t))
	{
		throw std::invalid_argument(
			"indexed_image_scratch: The storage has no room for a scratch."
		);
	}
}

void check_run_length(std::size_t run_length)
{
	if (run_length == 0)
	{
		throw std::invalid_argument(
			"indexed_image_scratch: A run must span at least one index."
		);
	}
}

// Lays out a scratch in a number of bytes: its fingerprint, then for each
// entry its flags followed by its values, each aligned for its type. It also
// computes the fingerprint of what it lays out.
class storage_cursor
{
public:
	storage_cursor(std::size_t capacity, std::size_t run_length) noexcept
		: m_capacity(capacity)
		, m_run_length(run_length)
		, m_used(sizeof(std::uint64_t))
		, m_entry_count(0)
		, m_fingerprint(
			hash_value(
				hash_value(fingerprint_seed, format_version),
				run_length
			)
		)
	{
	}

	std::size_t get_used() const noexcept
	{
		return m_used;
	}

	std::size_t get_entry_count() const noexcept
	{
		return m_entry_count;
	}

	std::uint64_t get_fingerprint() const noexcept
	{
		return m_fingerprint;
	}

	// Places an entry for as many of some indices of a file as fit, and
	// gives how many do. An entry of no index is not placed.
	std::size_t place(
		const std::string &key,
		const image_descriptor &file,
		span<const std::size_t> indices
	)
	{
		const auto index_count = count_fitting(file, indices.size());
		if (index_count == 0)
		{
			return 0;
		}

		const auto flag_count = compute_run_count(index_count, m_run_length);
		const auto first_flag = get_first_flag_byte();
		const auto first_value = get_first_value_byte(file, flag_count);
		m_flags = make_flags_descriptor(
			flag_count,
			first_flag / sizeof(std::uint64_t)
		);
		m_values = make_values_descriptor(
			file,
			index_count,
			first_value / get_size(file.get_data_type())
		);

		m_used = first_value + index_count * compute_slot_size(file);
		++m_entry_count;
		add_to_fingerprint(
			key,
			file,
			make_span(indices.data(), index_count)
		);

		return index_count;
	}

	// Where the entry placed last has its flags.
	const array_descriptor& get_flags() const noexcept
	{
		return m_flags;
	}

	// Where the entry placed last has its values.
	const array_descriptor& get_values() const noexcept
	{
		return m_values;
	}

private:
	std::size_t get_first_flag_byte() const noexcept
	{
		return align_ceil(m_used, alignof(std::uint64_t));
	}

	std::size_t get_first_value_byte(
		const image_descriptor &file,
		std::size_t flag_count
	) const noexcept
	{
		return align_ceil(
			get_first_flag_byte() + flag_count * sizeof(std::uint64_t),
			get_size(file.get_data_type())
		);
	}

	bool fits(
		const image_descriptor &file,
		std::size_t index_count
	) const noexcept
	{
		const auto flag_count = compute_run_count(index_count, m_run_length);
		const auto first_value = get_first_value_byte(file, flag_count);
		if (first_value > m_capacity)
		{
			return false;
		}

		const auto room = m_capacity - first_value;
		return index_count <= room / compute_slot_size(file);
	}

	// The flags take room too, and how many there are depends on how many
	// indices are placed, so the count is searched for.
	std::size_t count_fitting(
		const image_descriptor &file,
		std::size_t index_count
	) const noexcept
	{
		if (fits(file, index_count))
		{
			return index_count;
		}

		std::size_t fitting = 0;
		auto excluded = index_count;
		while (excluded - fitting > 1)
		{
			const auto middle = fitting + (excluded - fitting) / 2;
			if (fits(file, middle))
			{
				fitting = middle;
			}
			else
			{
				excluded = middle;
			}
		}

		return fitting;
	}

	void add_to_fingerprint(
		const std::string &key,
		const image_descriptor &file,
		span<const std::size_t> indices
	) noexcept
	{
		m_fingerprint = hash_value(m_fingerprint, key.size());
		m_fingerprint = hash_bytes(
			m_fingerprint,
			reinterpret_cast<const byte*>(key.data()),
			key.size()
		);
		m_fingerprint = hash_value(
			m_fingerprint,
			static_cast<std::uint64_t>(file.get_data_type())
		);

		const auto extents = file.get_extents();
		m_fingerprint = hash_value(m_fingerprint, extents.size());
		for (const auto extent : extents)
		{
			m_fingerprint = hash_value(m_fingerprint, extent);
		}

		m_fingerprint = hash_value(m_fingerprint, indices.size());
		for (const auto index : indices)
		{
			m_fingerprint = hash_value(m_fingerprint, index);
		}
	}

	std::size_t m_capacity;
	std::size_t m_run_length;
	std::size_t m_used;
	std::size_t m_entry_count;
	std::uint64_t m_fingerprint;
	array_descriptor m_flags;
	array_descriptor m_values;
};

void check_not_empty(const storage_cursor &layout)
{
	if (layout.get_entry_count() == 0)
	{
		throw std::invalid_argument(
			"indexed_image_scratch: There is nothing to hold. The locations "
			"name no image, or the maximum size has no room for one."
		);
	}
}

// Opens the file of one group, to learn its shape and data type, and keeps
// what that fails with rather than throw it.
class file_description final
	: public task
{
public:
	file_description(
		image_reader_provider &files,
		std::string key,
		boost::optional<image_descriptor> &descriptor,
		std::exception_ptr &error
	) noexcept
		: m_files(files)
		, m_key(std::move(key))
		, m_descriptor(descriptor)
		, m_error(error)
	{
	}

	void run() override
	{
		try
		{
			const auto file = m_files.acquire(m_key);
			VITRIO_ASSERT(file);
			m_descriptor = file->get_descriptor();
		}
		catch (...)
		{
			m_error = std::current_exception();
		}
	}

private:
	image_reader_provider &m_files;
	std::string m_key;
	boost::optional<image_descriptor> &m_descriptor;
	std::exception_ptr &m_error;
};

// What the file of each group holds, in the order of the grouping. The files
// are opened at once, a task each, and waited for. What one fails with is
// thrown, that of the first in the grouping.
std::vector<image_descriptor> describe_files(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks
)
{
	const auto group_count = locations.get_group_count();
	std::vector<boost::optional<image_descriptor>> described(group_count);
	std::vector<std::exception_ptr> errors(group_count);

	// The tasks fill these, so every one that was submitted is waited for,
	// and one that could not be is counted as failed.
	const auto done = std::make_shared<counting_completion>(group_count);
	for (std::size_t index = 0; index < group_count; ++index)
	{
		try
		{
			tasks.submit(
				std::make_unique<file_description>(
					files,
					locations.get_group(index).get_key(),
					described[index],
					errors[index]
				),
				done
			);
		}
		catch (...)
		{
			const auto error = std::current_exception();
			for (auto unsubmitted = index; unsubmitted < group_count;)
			{
				done->notify_done(error);
				++unsubmitted;
			}
			break;
		}
	}
	done->get();

	std::vector<image_descriptor> descriptors;
	descriptors.reserve(group_count);
	for (std::size_t index = 0; index < group_count; ++index)
	{
		if (errors[index])
		{
			std::rethrow_exception(errors[index]);
		}
		descriptors.push_back(std::move(*described[index]));
	}

	return descriptors;
}

// How a scratch of some locations lays out its storage, when it may use no
// more than a maximum. It is a dry run of the constructor: the files are
// placed the same way, and nothing is stored.
storage_cursor lay_out(
	const image_location_grouping &locations,
	const std::vector<image_descriptor> &descriptors,
	std::size_t run_length,
	std::size_t max_size
)
{
	const auto group_count = locations.get_group_count();

	storage_cursor cursor(max_size, run_length);
	for (std::size_t index = 0; index < group_count; ++index)
	{
		const auto group = locations.get_group(index);
		const auto &key = group.get_key();
		const auto &descriptor = descriptors[index];
		const auto indices = get_named_indices(group, descriptor);
		if (indices.empty() || compute_slot_size(descriptor) == 0)
		{
			continue;
		}

		const auto held_count =
			cursor.place(key, descriptor, make_span(indices));
		if (held_count < indices.size())
		{
			break;
		}
	}

	return cursor;
}

} // anonymous namespace

indexed_image_scratch::indexed_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	std::shared_ptr<image_scratch_storage> storage,
	std::size_t run_length,
	image_scratch_open_mode mode
)
{
	check_storage(storage.get());
	check_run_length(run_length);

	const auto group_count = locations.get_group_count();
	const auto descriptors = describe_files(locations, files, tasks);

	std::vector<std::shared_ptr<indexed_image_scratch_entry>> entries;
	storage_cursor cursor(storage->get_size(), run_length);
	for (std::size_t index = 0; index < group_count; ++index)
	{
		const auto group = locations.get_group(index);
		const auto &key = group.get_key();
		const auto &descriptor = descriptors[index];
		auto indices = get_named_indices(group, descriptor);
		if (indices.empty() || compute_slot_size(descriptor) == 0)
		{
			continue;
		}

		const auto named_count = indices.size();
		const auto held_count =
			cursor.place(key, descriptor, make_span(indices));
		if (held_count > 0)
		{
			indices.resize(held_count);
			entries.push_back(
				std::make_shared<indexed_image_scratch_entry>(
					std::move(indices),
					storage,
					cursor.get_values(),
					cursor.get_flags(),
					get_modification_time(key),
					run_length,
					mode
				)
			);
			m_entries.emplace(key, entries.back());
		}

		if (held_count < named_count)
		{
			break;
		}
	}

	// A resumed scratch has trusted its flags so far. They only mean
	// something if the storage was laid out like this one.
	const auto fingerprint = cursor.get_fingerprint();
	if (
		mode == image_scratch_open_mode::resumed &&
		read_fingerprint(*storage) != fingerprint
	)
	{
		for (const auto &entry : entries)
		{
			entry->reset();
		}
	}

	write_fingerprint(*storage, fingerprint);
}

indexed_image_scratch::~indexed_image_scratch() = default;

std::shared_ptr<image_scratch_entry>
indexed_image_scratch::find(const std::string &key)
{
	const auto ite = m_entries.find(key);
	if (ite == m_entries.end())
	{
		return nullptr;
	}

	return ite->second;
}

std::shared_ptr<image_scratch> create_host_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	std::size_t run_length,
	std::size_t max_size
)
{
	check_run_length(run_length);

	const auto layout = lay_out(
		locations,
		describe_files(locations, files, tasks),
		run_length,
		max_size
	);
	check_not_empty(layout);

	return std::make_shared<indexed_image_scratch>(
		locations,
		files,
		tasks,
		create_host_image_scratch_storage(layout.get_used()),
		run_length
	);
}

std::shared_ptr<image_scratch> create_file_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	const file_transfer_pool &transfers,
	const std::string &path,
	std::size_t run_length,
	std::size_t max_size
)
{
	check_run_length(run_length);

	const auto layout = lay_out(
		locations,
		describe_files(locations, files, tasks),
		run_length,
		max_size
	);
	check_not_empty(layout);

	// A file that is there is kept, so that programs that hold the same
	// images share it. The scratch resumes from it if it carries the
	// fingerprint, and starts empty over it otherwise.
	boost::system::error_code error;
	const auto existed = boost::filesystem::exists(path, error);
	auto storage = create_file_image_scratch_storage(
		path,
		layout.get_used(),
		transfers
	);

	// A file that was just created holds zeros where its fingerprint goes.
	const auto fingerprint = read_fingerprint(*storage);
	if (existed && fingerprint != 0 && fingerprint != layout.get_fingerprint())
	{
		VITRIO_LOG_WARN(
			"The scratch file {} holds something other than the images it "
			"is asked for, and is overwritten.",
			path
		);
	}

	return std::make_shared<indexed_image_scratch>(
		locations,
		files,
		tasks,
		std::move(storage),
		run_length,
		image_scratch_open_mode::resumed
	);
}

std::shared_ptr<image_scratch> create_directory_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	executor &tasks,
	const file_transfer_pool &transfers,
	const std::string &directory,
	std::size_t run_length,
	std::size_t budget
)
{
	check_run_length(run_length);

	image_scratch_directory scratches(directory, budget);
	const auto descriptors = describe_files(locations, files, tasks);

	// Files that can not be removed may leave less room than the layout
	// was computed for. It is then computed again, for the room there is,
	// and that is a scratch of another fingerprint.
	auto max_size = budget;
	for (;;)
	{
		const auto layout =
			lay_out(locations, descriptors, run_length, max_size);
		check_not_empty(layout);

		std::size_t room = 0;
		auto storage = scratches.acquire(
			layout.get_fingerprint(),
			layout.get_used(),
			transfers,
			room
		);

		if (storage)
		{
			return std::make_shared<indexed_image_scratch>(
				locations,
				files,
				tasks,
				std::move(storage),
				run_length,
				image_scratch_open_mode::resumed
			);
		}

		VITRIO_ASSERT(room < max_size);
		max_size = room;
	}
}

} // namespace vitrio
