// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "eer_encoding.hpp"

#include <formats/tiff/tiff_file.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace vitrio
{
namespace eer
{

/**
 * @brief Reads the events of an EER file.
 *
 * An EER file is a TIFF file whose pages are the frames of a movie, each
 * page encoding the events the detector counted during its frame as
 * described by @ref eer_encoding. Time is counted in frames, so the events
 * of a frame are a group of the timestamp of its index, and a frame that
 * counted nothing gives no group.
 *
 * Positions are placed on the grid the subpixels cut the pixels of a frame
 * into. Every frame must have the size and the encoding of the first one.
 *
 * The length of a frame is not read from the metadata of the file yet, so
 * the time quantum is stated as unknown.
 *
 * Decoding goes through one handle on the file, so concurrent calls to
 * @ref read are serialised.
 */
class eer_detector_event_timeline_reader final
	: public detector_event_timeline_reader
{
public:
	/**
	 * @brief Open a file for reading.
	 *
	 * @param path Path to the file.
	 * @throws image_file_error If the file can not be reached.
	 * @throws image_file_format_error If the file is not a TIFF file, if its
	 * first page is not compressed with a scheme of EER, or if its frames
	 * place events on a grid too large to address.
	 */
	explicit eer_detector_event_timeline_reader(const std::string &path);

	~eer_detector_event_timeline_reader() override = default;

	const detector_event_timeline_descriptor&
	get_descriptor() const noexcept override;

	void read(
		std::uint64_t time_begin,
		std::uint64_t time_end,
		detector_event_timeline &destination
	) const override;

private:
	/**
	 * @brief Decode the events of one frame, appending their positions.
	 *
	 * Called with the lock held.
	 *
	 * @param frame Index of the frame.
	 */
	void decode_frame(std::size_t frame) const;

	std::string m_path;
	mutable std::mutex m_mutex;
	mutable tiff::tiff_file m_file;
	std::array<std::size_t, 2> m_frame_extents;
	eer_encoding m_encoding;
	detector_event_timeline_descriptor m_descriptor;
	mutable std::vector<byte> m_strip;
	mutable std::vector<std::uint32_t> m_positions;
};

} // namespace eer
} // namespace vitrio
