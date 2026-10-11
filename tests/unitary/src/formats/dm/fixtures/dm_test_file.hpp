// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/dm/dm_constants.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace vitrio
{
namespace test
{

/**
 * @brief Get the bytes values take in a byte order.
 *
 * The host is taken to be little-endian, as every machine the suite runs on
 * is.
 *
 * @param values The values.
 * @param little_endian The byte order to lay them out in.
 * @return std::vector<char> Their bytes.
 */
template <typename T>
std::vector<char> payload_of(const std::vector<T> &values, bool little_endian)
{
	std::vector<char> bytes(values.size() * sizeof(T));
	if (!values.empty())
	{
		std::memcpy(bytes.data(), values.data(), bytes.size());
	}

	if (!little_endian)
	{
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			std::reverse(
				bytes.begin() + i * sizeof(T),
				bytes.begin() + (i + 1) * sizeof(T)
			);
		}
	}

	return bytes;
}

/**
 * @brief Lays out a DigitalMicrograph file entry by entry, as the format
 * describes it rather than as the parser reads it.
 *
 * A group is written with the count of entries it is said to hold, and the
 * entries that follow are its own until that many have been written: the
 * caller keeps the counts right, or wrong on purpose.
 */
class dm_test_file
{
public:
	/**
	 * @brief Begin a file with its header and the start of its root group.
	 *
	 * @param version 3 or 4.
	 * @param little_endian The byte order of the values.
	 * @param root_count The count of entries of the root group.
	 */
	dm_test_file(
		std::uint32_t version,
		bool little_endian,
		std::uint64_t root_count
	)
		: m_version(version)
		, m_little_endian(little_endian)
		, m_bytes()
	{
		put_big_endian(version, 4);
		put_big_endian(0, version == 3 ? 4 : 8);
		put_big_endian(little_endian ? 1 : 0, 4);
		m_bytes.push_back(1);
		m_bytes.push_back(0);
		put_word(root_count);
	}

	/**
	 * @brief Write the head of a group: its label and its count of
	 * entries.
	 */
	dm_test_file& group(const std::string &label, std::uint64_t count)
	{
		put_entry_head(dm::tag_kind::group, label);
		m_bytes.push_back(0);
		m_bytes.push_back(0);
		put_word(count);
		return *this;
	}

	/**
	 * @brief Write a datum of any encoding and payload.
	 */
	dm_test_file& datum(
		const std::string &label,
		const std::vector<std::uint64_t> &encoding,
		const std::vector<char> &payload
	)
	{
		put_entry_head(dm::tag_kind::data, label);
		m_bytes.insert(m_bytes.end(), {'%', '%', '%', '%'});
		put_word(encoding.size());
		for (const auto word : encoding)
		{
			put_word(word);
		}
		m_bytes.insert(m_bytes.end(), payload.cbegin(), payload.cend());
		return *this;
	}

	/**
	 * @brief Write a datum of one scalar, in the byte order of the values.
	 */
	template <typename T>
	dm_test_file& scalar(
		const std::string &label,
		std::uint64_t encoding,
		T value
	)
	{
		return datum(label, {encoding}, payload_of(std::vector<T>{value}));
	}

	/**
	 * @brief Write a datum of an array of scalars, in the byte order of the
	 * values.
	 */
	template <typename T>
	dm_test_file& array(
		const std::string &label,
		std::uint64_t encoding,
		const std::vector<T> &values
	)
	{
		return datum(
			label,
			{dm::encoding::array, encoding, values.size()},
			payload_of(values)
		);
	}

	/**
	 * @brief Write bytes as they are.
	 */
	dm_test_file& raw(const std::vector<char> &bytes)
	{
		m_bytes.insert(m_bytes.end(), bytes.cbegin(), bytes.cend());
		return *this;
	}

	/**
	 * @brief Get how many bytes have been written, which is where the
	 * next ones go.
	 */
	std::size_t get_size() const noexcept
	{
		return m_bytes.size();
	}

	/**
	 * @brief Get the bytes of the file, which end in eight zeros.
	 */
	std::vector<char> finish() const
	{
		auto bytes = m_bytes;
		bytes.insert(bytes.end(), 8, '\0');
		return bytes;
	}

	/**
	 * @brief Get the bytes values take in the byte order of the file.
	 */
	template <typename T>
	std::vector<char> payload_of(const std::vector<T> &values) const
	{
		return test::payload_of(values, m_little_endian);
	}

private:
	void put_big_endian(std::uint64_t value, std::size_t size)
	{
		for (std::size_t i = size; i > 0; --i)
		{
			m_bytes.push_back(static_cast<char>(value >> (8 * (i - 1))));
		}
	}

	void put_word(std::uint64_t value)
	{
		put_big_endian(value, m_version == 3 ? 4 : 8);
	}

	void put_entry_head(std::uint8_t kind, const std::string &label)
	{
		m_bytes.push_back(static_cast<char>(kind));
		put_big_endian(label.size(), 2);
		m_bytes.insert(m_bytes.end(), label.cbegin(), label.cend());
		if (m_version == 4)
		{
			// The size of the entry, which nothing relies on.
			put_big_endian(0, 8);
		}
	}

	std::uint32_t m_version;
	bool m_little_endian;
	std::vector<char> m_bytes;
};

/**
 * @brief What an image of a test file is.
 */
struct dm_test_image
{
	/// The code of its data type.
	std::int32_t data_type;
	/// The type of the elements of its Data datum.
	std::uint64_t element_encoding;
	/// Its extents, fastest axis first, as the file states them.
	std::vector<std::uint32_t> dimensions;
	/// The bytes of its values, in the byte order of the file.
	std::vector<char> values;
	/// The size of one of its elements, which counts how many there are.
	std::size_t element_size;
};

/**
 * @brief Write one entry of the image list.
 */
inline void put_image(dm_test_file &file, const dm_test_image &image)
{
	file.group("", 1)
		.group("ImageData", 3)
		.scalar<std::int32_t>("DataType", dm::encoding::int32, image.data_type)
		.group("Dimensions", image.dimensions.size());
	for (const auto extent : image.dimensions)
	{
		file.scalar<std::uint32_t>("", dm::encoding::uint32, extent);
	}

	file.datum(
		"Data",
		{
			dm::encoding::array,
			image.element_encoding,
			image.values.size() / image.element_size
		},
		image.values
	);
}

/**
 * @brief Lay out a file of a thumbnail, which the Thumbnails group names,
 * and an image.
 *
 * @param version 3 or 4.
 * @param little_endian The byte order of the values.
 * @param image The image.
 * @param padding How many bytes to put before the image list, which moves
 * where the values of the image begin.
 * @param data_offset Where the values of the image begin, written on return.
 * @return std::vector<char> The bytes of the file.
 */
inline std::vector<char> make_image_file(
	std::uint32_t version,
	bool little_endian,
	const dm_test_image &image,
	std::size_t padding,
	std::size_t &data_offset
)
{
	dm_test_file file(version, little_endian, 3);
	file.array("Padding", dm::encoding::uint8, std::vector<std::uint8_t>(
		padding, 0
	));

	file.group("ImageList", 2);
	const dm_test_image thumbnail = {
		23,
		dm::encoding::uint32,
		{2, 2},
		std::vector<char>(16, '\x7f'),
		4
	};
	put_image(file, thumbnail);
	put_image(file, image);
	data_offset = file.get_size() - image.values.size();

	file.group("Thumbnails", 1)
		.group("", 1)
		.scalar<std::uint32_t>("ImageIndex", dm::encoding::uint32, 0);

	return file.finish();
}

/**
 * @brief Make the float32 values 0, 1, 2 and onwards.
 *
 * @param count How many values to make.
 * @return std::vector<float> The values.
 */
inline std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i);
	}

	return values;
}

/**
 * @brief Make an image of float32 values that count up from zero.
 *
 * @param little_endian The byte order of the values.
 * @param dimensions Its extents, fastest axis first.
 * @return dm_test_image The image.
 */
inline dm_test_image make_counting_image(
	bool little_endian,
	const std::vector<std::uint32_t> &dimensions
)
{
	std::size_t count = 1;
	for (const auto extent : dimensions)
	{
		count *= extent;
	}

	return dm_test_image{
		2,
		dm::encoding::float32,
		dimensions,
		payload_of(counting(count), little_endian),
		sizeof(float)
	};
}

/**
 * @brief Write bytes to a file, replacing whatever it held.
 *
 * @param path Path to the file.
 * @param raw The bytes to write.
 */
inline void write_file(const std::string &path, const std::vector<char> &raw)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

} // namespace test
} // namespace vitrio
