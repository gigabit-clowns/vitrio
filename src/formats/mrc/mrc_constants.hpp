// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vitrio
{
namespace mrc
{

/**
 * @brief Size of the main header, in bytes.
 */
constexpr std::size_t header_size = 1024;

/**
 * @brief Byte offset of each field of the main header.
 *
 * The offsets of the MRC2014 layout, which a header is read from and written
 * to. Their names are the ones the specification gives the fields.
 */
namespace offset
{

constexpr std::size_t nx = 0;
constexpr std::size_t ny = 4;
constexpr std::size_t nz = 8;
constexpr std::size_t mode = 12;
constexpr std::size_t nxstart = 16;
constexpr std::size_t nystart = 20;
constexpr std::size_t nzstart = 24;
constexpr std::size_t mx = 28;
constexpr std::size_t my = 32;
constexpr std::size_t mz = 36;
constexpr std::size_t cella = 40;
constexpr std::size_t cellb = 52;
constexpr std::size_t mapc = 64;
constexpr std::size_t mapr = 68;
constexpr std::size_t maps = 72;
constexpr std::size_t dmin = 76;
constexpr std::size_t dmax = 80;
constexpr std::size_t dmean = 84;
constexpr std::size_t ispg = 88;
constexpr std::size_t nsymbt = 92;
constexpr std::size_t extra1 = 96;
constexpr std::size_t exttyp = 104;
constexpr std::size_t nversion = 108;
constexpr std::size_t extra2 = 112;
constexpr std::size_t imod_stamp = 152;
constexpr std::size_t imod_flags = 156;
constexpr std::size_t origin = 196;
constexpr std::size_t map = 208;
constexpr std::size_t machst = 212;
constexpr std::size_t rms = 216;
constexpr std::size_t nlabl = 220;
constexpr std::size_t label = 224;

} // namespace offset

/**
 * @brief Size of each fixed width byte array of the main header.
 */
namespace size
{

constexpr std::size_t extra1 = 8;
constexpr std::size_t exttyp = 4;
constexpr std::size_t extra2 = 84;
constexpr std::size_t map = 4;
constexpr std::size_t machst = 4;
constexpr std::size_t label = 80;

} // namespace size

/**
 * @brief Number of labels the main header carries.
 */
constexpr std::size_t label_count = 10;

/**
 * @brief The identifier a file carries at @ref offset::map.
 *
 * Only its first three bytes are matched when a file is recognized: that is
 * the form the MRC2014 paper states, and some writers emit nothing more.
 */
constexpr std::array<char, 4> map_id = {{'M', 'A', 'P', ' '}};

/**
 * @brief Bytes of @ref offset::map that identify a file.
 */
constexpr std::size_t map_id_match_size = 3;

/**
 * @brief First two bytes of the machine stamp of a little-endian file.
 */
constexpr std::array<std::uint8_t, 2>
little_endian_machine_stamp = {{0x44, 0x44}};

/**
 * @brief First two bytes of the machine stamp some little-endian files carry
 * in place of @ref little_endian_machine_stamp.
 */
constexpr std::array<std::uint8_t, 2>
legacy_little_endian_machine_stamp = {{0x44, 0x41}};

/**
 * @brief First two bytes of the machine stamp of a big-endian file.
 */
constexpr std::array<std::uint8_t, 2>
big_endian_machine_stamp = {{0x11, 0x11}};

/**
 * @brief Number of axes of space the columns, the rows and the sections of a
 * file run along.
 *
 * @ref offset::mapc, @ref offset::mapr and @ref offset::maps name one of them
 * each, counting from one.
 */
constexpr std::int32_t space_axis_count = 3;

/**
 * @brief Space group of a file holding one image or a stack of them.
 */
constexpr std::int32_t image_stack_space_group = 0;

/**
 * @brief Space group of a file holding one volume.
 */
constexpr std::int32_t volume_space_group = 1;

/**
 * @brief First space group of the range that denotes a stack of volumes.
 */
constexpr std::int32_t first_volume_stack_space_group =
	401;

/**
 * @brief Last space group of the range that denotes a stack of volumes.
 */
constexpr std::int32_t last_volume_stack_space_group =
	630;

/**
 * @brief Value of @ref offset::imod_stamp that makes
 * @ref offset::imod_flags meaningful.
 */
constexpr std::int32_t imod_stamp_value = 1146047817;

/**
 * @brief Bit of @ref offset::imod_flags stating that mode 0 holds signed
 * bytes.
 */
constexpr std::int32_t imod_signed_bytes_flag = 1;

/**
 * @brief Version this library writes, the year followed by the version
 * within it.
 */
constexpr std::int32_t written_version = 20141;

/**
 * @brief Check whether a space group denotes a stack of volumes.
 *
 * @param space_group The space group to check.
 * @return bool true if it denotes a stack of volumes.
 */
bool is_volume_stack_space_group(std::int32_t space_group) noexcept;

} // namespace mrc
} // namespace vitrio

#include "mrc_constants.inl"
