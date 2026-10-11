# SPDX-License-Identifier: LGPL-2.1-or-later

"""Write the DigitalMicrograph files of tests/assets/dm, as README.txt
describes.

Run as: python make_dm_assets.py <directory>
"""

import os
import struct
import sys

# How a value of a data entry is encoded.
INT16, INT32, UINT16, UINT32, FLOAT32, FLOAT64 = 2, 3, 4, 5, 6, 7
BOOLEAN, INT8, UINT8 = 8, 9, 10
STRUCT, ARRAY = 15, 20

SCALAR_FORMATS = {
	INT16: 'h', INT32: 'i', UINT16: 'H', UINT32: 'I',
	FLOAT32: 'f', FLOAT64: 'd', BOOLEAN: 'B', INT8: 'b', UINT8: 'B',
}

# The codes of the data type of an image.
DATA_TYPES = {
	'int16': (1, INT16), 'float32': (2, FLOAT32), 'uint16': (10, UINT16),
	'rgba': (23, UINT32),
}

class Scalar:
	def __init__(self, encoding, value):
		self.encoding = encoding
		self.value = value

	def info(self):
		return [self.encoding]

	def payload(self, order):
		return struct.pack(order + SCALAR_FORMATS[self.encoding], self.value)

def String(text):
	"""Text, as GMS writes it: an array of 16-bit characters."""
	return Array(UINT16, [ord(c) for c in text])

class Struct:
	def __init__(self, fields):
		self.fields = fields # (encoding, value)

	def info(self):
		words = [STRUCT, 0, len(self.fields)]
		for encoding, _ in self.fields:
			words += [0, encoding]
		return words

	def payload(self, order):
		return b''.join(
			struct.pack(order + SCALAR_FORMATS[encoding], value)
			for encoding, value in self.fields
		)

class Array:
	def __init__(self, encoding, values):
		self.encoding = encoding
		self.values = values

	def info(self):
		return [ARRAY, self.encoding, len(self.values)]

	def payload(self, order):
		form = SCALAR_FORMATS[self.encoding]
		return struct.pack(order + str(len(self.values)) + form, *self.values)

class Group:
	def __init__(self, entries):
		self.entries = entries # (label, Group or a value)

def encode_entries(entries, version, order):
	word = '>I' if version == 3 else '>Q'
	out = bytearray(struct.pack(word, len(entries)))
	for label, value in entries:
		raw_label = label.encode('ascii')
		if isinstance(value, Group):
			body = b'\x00\x00' + encode_entries(value.entries, version, order)
			kind = 20
		else:
			info = value.info()
			body = b'%%%%' + struct.pack(word, len(info))
			body += b''.join(struct.pack(word, w) for w in info)
			body += value.payload(order)
			kind = 21
		out += struct.pack('>BH', kind, len(raw_label)) + raw_label
		if version == 4:
			out += struct.pack('>Q', len(body))
		out += body
	return bytes(out)

def encode_file(root, version, order):
	"""The bytes of a file whose root group holds the entries of root."""
	group = b'\x01\x00' + encode_entries(root.entries, version, order)
	flag = 1 if order == '<' else 0
	if version == 3:
		header = struct.pack('>IIi', 3, len(group), flag)
	else:
		header = struct.pack('>IQi', 4, len(group), flag)
	# Files end with eight zero bytes.
	return header + group + bytes(8)

def image_entry(name, data_type, extents, values):
	"""An entry of the image list. extents run slowest axis first."""
	code, encoding = DATA_TYPES[data_type]
	dimensions = [('', Scalar(UINT32, e)) for e in reversed(extents)]
	calibration = [
		('', Group([
			('Origin', Scalar(FLOAT32, 0.0)),
			('Scale', Scalar(FLOAT32, 1.0)),
			('Units', String('')),
		]))
		for _ in extents
	]
	return ('', Group([
		('ImageData', Group([
			('Calibrations', Group([
				('Brightness', Group([
					('Origin', Scalar(FLOAT32, 0.0)),
					('Scale', Scalar(FLOAT32, 1.0)),
					('Units', String('Counts')),
				])),
				('Dimension', Group(calibration)),
				('DisplayCalibratedUnits', Scalar(BOOLEAN, 1)),
			])),
			('Data', Array(encoding, values)),
			('DataType', Scalar(INT32, code)),
			('Dimensions', Group(dimensions)),
			('PixelDepth', Scalar(UINT32, struct.calcsize(
				SCALAR_FORMATS[encoding])))
		])),
		('ImageTags', Group([])),
		('Name', String(name)),
		('UniqueID', Struct([(UINT32, 7), (UINT32, 5), (UINT32, 3),
			(UINT32, 1)])),
	]))

def thumbnail_entry():
	"""A thumbnail of 4 by 2 pixels, as the file browser of GMS writes."""
	return image_entry('', 'rgba', [2, 4], [0xFF808080] * 8)

def find_data_offset(raw, values, order, encoding):
	"""Where the values of the image begin, found by their bytes."""
	needle = struct.pack(
		order + str(len(values)) + SCALAR_FORMATS[encoding], *values
	)
	offset = raw.find(needle)
	assert offset >= 0 and raw.find(needle, offset + 1) < 0
	return offset

def make_file(name, version, order, data_type, extents, values, thumbnail,
	element_size, aligned):
	"""Pad the root group before the image list until the values are
	aligned as asked."""
	_, encoding = DATA_TYPES[data_type]
	for padding in range(16):
		entries = []
		if thumbnail:
			entries.append(thumbnail_entry())
		entries.append(image_entry(name, data_type, extents, values))
		root = [
			('ApplicationBounds', Array(INT32, [0, 0, 1024, 1280])),
			('Padding', Array(UINT8, [0] * padding)),
			('ImageList', Group(entries)),
			('Thumbnails', Group([
				('', Group([
					('ImageIndex', Scalar(UINT32, 0)),
					('SourceSize_Pixels', Array(INT32, [extents[-1],
						extents[-2]])),
				])),
			] if thumbnail else [])),
		]
		raw = encode_file(Group(root), version, order)
		offset = find_data_offset(raw, values, order, encoding)
		if (offset % element_size == 0) == aligned:
			return raw, offset
	raise RuntimeError('No padding aligns the values as asked.')

def counting(count, function):
	return [function(p) for p in range(count)]

def main(directory):
	files = [
		# name, version, order, type, extents, values, thumbnail, size, aligned
		('image_float32.dm3', 3, '<', 'float32', [6, 8],
			counting(48, lambda p: p * 0.25 - 4.0), True, 4, True),
		('image_uint16_unaligned.dm4', 4, '<', 'uint16', [5, 7],
			counting(35, lambda p: 1000 + 3 * p), True, 2, False),
		('stack_int16.dm4', 4, '<', 'int16', [3, 4, 6],
			counting(72, lambda p: 100 * (p // 24) - p % 24), True, 2, True),
		('image_float32_big_endian.dm3', 3, '>', 'float32', [4, 5],
			counting(20, lambda p: p * 0.25 - 4.0), False, 4, False),
	]
	for (name, version, order, data_type, extents, values, thumbnail,
		element_size, aligned) in files:
		raw, offset = make_file(os.path.splitext(name)[0], version, order,
			data_type, extents, values, thumbnail, element_size, aligned)
		with open(os.path.join(directory, name), 'wb') as output:
			output.write(raw)
		print('{}: {:,} bytes, values at {}'.format(name, len(raw), offset))

if __name__ == '__main__':
	main(sys.argv[1])
