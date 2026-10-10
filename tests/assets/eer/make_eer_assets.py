# SPDX-License-Identifier: LGPL-2.1-or-later

"""Write the EER files of tests/assets/eer, as README.txt describes.

Run as: python make_eer_assets.py <directory>
"""

import sys
import numpy as np

def encode_strip(events, pixel_count, rle_bits, horz_bits, vert_bits):
	"""Encode one strip: events are (pixel, h, v), pixel ascending."""
	codes = []
	most = (1 << rle_bits) - 1
	code_bits = rle_bits + horz_bits + vert_bits
	position = 0
	for pixel, h, v in events:
		gap = pixel - position
		while gap >= most:
			codes.append((most, rle_bits))
			gap -= most
		stored_h = h ^ horz_bits
		stored_v = v ^ vert_bits
		codes.append((
			gap | (stored_h << rle_bits) | (stored_v << (rle_bits + horz_bits)),
			code_bits
		))
		position = pixel + 1
	gap = pixel_count - position
	while gap >= most:
		codes.append((most, rle_bits))
		gap -= most
	if gap > 0:
		codes.append((gap, code_bits))
	value = 0
	count = 0
	for code, bits in codes:
		value |= code << count
		count += bits
	# Whole bytes, an even number of them, and a spare pair for decoders
	# that want a whole code past the last one.
	size = (count + 7) // 8
	size += size % 2 + 2
	return value.to_bytes(size, 'little')

def frame_events(frame, width, height, horz_bits, vert_bits, empty):
	"""Events of a frame as (row, column, h, v)."""
	if frame in empty:
		return []
	events = []
	for pixel in range(width * height):
		if (7 * pixel + 3 * frame) % 5 == 0:
			h = (pixel + frame) % (1 << horz_bits)
			v = (3 * pixel + frame) % (1 << vert_bits)
			events.append((pixel // width, pixel % width, h, v))
	return events

def write_tiff(path, pages):
	"""Write a little-endian BigTIFF of pages already encoded, as EER files are.

	Each page is (tags, strips): tags maps a code to (type, values), with
	values a bytes object for UNDEFINED and a list of integers otherwise,
	and strips are the encoded strips, whose offsets and byte counts are
	filled in here.
	"""
	formats = {3: '<u2', 4: '<u4', 16: '<u8'}
	data = bytearray(b'II+\0\x08\0\0\0' + bytes(8))
	previous = 8
	for tags, strips in pages:
		offsets = []
		for strip in strips:
			while len(data) % 8:
				data += b'\0'
			offsets.append(len(data))
			data += strip
		tags = dict(tags)
		tags[273] = (16, offsets)
		tags[279] = (16, [len(strip) for strip in strips])

		while len(data) % 8:
			data += b'\0'
		directory = len(data)
		value_base = directory + 8 + 20 * len(tags) + 8
		values = bytearray()
		entries = []
		for code in sorted(tags):
			kind, value = tags[code]
			if kind == 7:
				raw = bytes(value)
				count = len(raw)
			else:
				raw = np.array(value, dtype=formats[kind]).tobytes()
				count = len(value)
			if len(raw) <= 8:
				field = raw.ljust(8, b'\0')
			else:
				while len(values) % 8:
					values += b'\0'
				field = (value_base + len(values)).to_bytes(8, 'little')
				values += raw
			entries.append(
				code.to_bytes(2, 'little') + kind.to_bytes(2, 'little') +
				count.to_bytes(8, 'little') + field
			)
		data[previous:previous + 8] = directory.to_bytes(8, 'little')
		data += len(entries).to_bytes(8, 'little') + b''.join(entries)
		previous = len(data)
		data += bytes(8)
		data += values
	with open(path, 'wb') as out:
		out.write(data)

def write(path, compression, width, height, rows_per_strip, frames,
		rle_bits, horz_bits, vert_bits, empty):
	metadata = (
		'<metadata><item name="numberOfFrames">%d</item></metadata>' % frames
	).encode()
	pages = []
	for frame in range(frames):
		events = frame_events(
			frame, width, height, horz_bits, vert_bits, empty)
		strips = []
		for first in range(0, height, rows_per_strip):
			rows = min(rows_per_strip, height - first)
			strip = [
				((row - first) * width + column, h, v)
				for row, column, h, v in events
				if first <= row < first + rows
			]
			strips.append(encode_strip(
				strip, rows * width, rle_bits, horz_bits, vert_bits))
		tags = {
			256: (3, [width]),
			257: (3, [height]),
			258: (3, [8]),
			259: (3, [compression]),
			262: (3, [1]),
			277: (3, [1]),
			278: (3, [rows_per_strip]),
			65001: (7, metadata),
		}
		if compression == 65002:
			tags[65007] = (3, [rle_bits])
			tags[65008] = (3, [horz_bits])
			tags[65009] = (3, [vert_bits])
		pages.append((tags, strips))
	write_tiff(path, pages)

if __name__ == '__main__':
	out = sys.argv[1]
	write(out + '/movie_rle7.eer', 65001, 8, 6, 3, 4, 7, 2, 2, ())
	write(out + '/movie_rle6.eer', 65002, 16, 8, 4, 3, 6, 1, 2, (1,))
