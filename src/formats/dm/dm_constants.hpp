// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vitrio
{
namespace dm
{

/**
 * @brief Size of the header of a version 3 file, in bytes: its version, the
 * length of its root group and the byte order of its values, the second in
 * 32 bits.
 */
constexpr std::size_t dm3_header_size = 12;

/**
 * @brief Size of the header of a version 4 file, in bytes, which states the
 * length of its root group in 64 bits.
 */
constexpr std::size_t dm4_header_size = 16;

/**
 * @brief Size of the header of a group that follows its label, before its
 * count of entries: whether the group is sorted and whether it is open.
 */
constexpr std::size_t group_flags_size = 2;

/**
 * @brief The four bytes a data entry begins with, `%%%%`.
 */
constexpr std::array<char, 4> data_delimiter = {{'%', '%', '%', '%'}};

/**
 * @brief The kind of an entry of a group, which its first byte states.
 */
namespace tag_kind
{

constexpr std::uint8_t group = 20;
constexpr std::uint8_t data = 21;

} // namespace tag_kind

/**
 * @brief How the value of a data entry is encoded, which the first word of
 * its encoding states, and each further type the encoding names.
 *
 * Their names are the ones DigitalMicrograph gives them.
 */
namespace encoding
{

constexpr std::uint64_t int16 = 2;
constexpr std::uint64_t int32 = 3;
constexpr std::uint64_t uint16 = 4;
constexpr std::uint64_t uint32 = 5;
constexpr std::uint64_t float32 = 6;
constexpr std::uint64_t float64 = 7;
constexpr std::uint64_t boolean = 8;
constexpr std::uint64_t int8 = 9;
constexpr std::uint64_t uint8 = 10;
constexpr std::uint64_t int64 = 11;
constexpr std::uint64_t uint64 = 12;
constexpr std::uint64_t structure = 15;
constexpr std::uint64_t string = 18;
constexpr std::uint64_t array = 20;

} // namespace encoding

/**
 * @brief Deepest a group may be nested in a file, the root being at depth
 * one.
 *
 * The files DigitalMicrograph writes nest a handful of groups deep. The limit
 * keeps a damaged or hostile file from exhausting the stack of the parser.
 */
constexpr std::size_t max_group_depth = 64;

/**
 * @brief Deepest the types of an encoding may be nested, as an array of
 * arrays is.
 *
 * @see max_group_depth
 */
constexpr std::size_t max_encoding_depth = 8;

} // namespace dm
} // namespace vitrio
