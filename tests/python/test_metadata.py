# SPDX-License-Identifier: LGPL-2.1-or-later

import pathlib
import types

import numpy as np
import pytest

import vitrio as vio

ASSETS = pathlib.Path(__file__).resolve().parents[1] / 'assets' / 'mrc'

# The cell of an MRC file is of single precision, so a pixel size comes back
# as near as that holds it.
TOLERANCE = 1e-6

def test_the_pixel_sizes_a_volume_is_written_with_are_read_back(tmp_path):
	path = tmp_path / 'volume.mrc'
	metadata = {
		'pixel_size_x': 0.83, 'pixel_size_y': 0.84, 'pixel_size_z': 1.06
	}
	vio.write_single(__setup_array((5, 4, 6)), path, metadata=metadata)
	read = vio.read_metadata(path)
	assert read['pixel_size_x'] == pytest.approx(0.83, rel=TOLERANCE)
	assert read['pixel_size_y'] == pytest.approx(0.84, rel=TOLERANCE)
	assert read['pixel_size_z'] == pytest.approx(1.06, rel=TOLERANCE)

def test_a_stack_of_images_has_no_pixel_size_along_z(tmp_path):
	path = tmp_path / 'stack.mrcs'
	metadata = {
		'pixel_size_x': 0.83, 'pixel_size_y': 0.83, 'pixel_size_z': 1.06
	}
	vio.write_stack(__setup_array((5, 4, 6)), path, metadata=metadata)
	read = vio.read_metadata(path)
	assert read['pixel_size_x'] == pytest.approx(0.83, rel=TOLERANCE)
	assert read['pixel_size_y'] == pytest.approx(0.83, rel=TOLERANCE)
	assert 'pixel_size_z' not in read

def test_a_file_written_as_a_descriptor_states_takes_metadata(tmp_path):
	path = tmp_path / 'volume.mrc'
	descriptor = vio.ImageDescriptor((5, 4, 6), 3, vio.NumericalType.float32)
	vio.write(
		__setup_array((5, 4, 6)), path, descriptor,
		metadata={'pixel_size_z': 1.5}
	)
	assert vio.read_metadata(path)['pixel_size_z'] == 1.5

def test_a_file_written_with_no_metadata_states_no_pixel_size(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((4, 6)), path)
	read = vio.read_metadata(path)
	assert 'pixel_size_x' not in read
	assert 'pixel_size_y' not in read
	assert read.get('pixel_size_x') is None

def test_the_metadata_is_a_dict_of_numbers_and_texts(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': 0.5}
	)
	read = vio.read_metadata(path)
	assert type(read) is dict
	assert type(read['pixel_size_x']) is float
	assert type(read['mrc.labels']) is str
	assert all(type(label) is str for label in read)
	assert list(read) == sorted(read)

def test_the_dict_is_the_callers_to_change(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': 0.5}
	)
	read = vio.read_metadata(path)
	read['pixel_size_x'] = 2.0
	assert vio.read_metadata(path)['pixel_size_x'] == 0.5

def test_an_mrc_file_records_the_library_after_the_labels_given(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path,
		metadata={'mrc.labels': 'Created by something else'}
	)
	labels = vio.read_metadata(path)['mrc.labels'].split('\n')
	assert len(labels) == 2
	assert labels[0] == 'Created by something else'
	assert labels[1].startswith('Created by vitrio')

def test_what_is_read_of_a_file_is_what_another_is_written_with(tmp_path):
	first = tmp_path / 'first.mrc'
	second = tmp_path / 'second.mrc'
	vio.write_single(
		__setup_array((5, 4, 6)), first,
		metadata={'pixel_size_x': 0.83, 'pixel_size_y': 0.84}
	)
	vio.write_single(
		__setup_array((5, 4, 6)), second, metadata=vio.read_metadata(first)
	)
	assert vio.read_metadata(second) == vio.read_metadata(first)

def test_an_integer_given_for_a_pixel_size_is_a_float(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': 2}
	)
	read = vio.read_metadata(path)
	assert read['pixel_size_x'] == 2.0
	assert type(read['pixel_size_x']) is float

@pytest.mark.parametrize(
	'value', [np.float32(0.5), np.float64(0.5), np.int64(2), np.uint8(2)]
)
def test_a_number_of_numpy_is_a_number(tmp_path, value):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': value}
	)
	assert vio.read_metadata(path)['pixel_size_x'] == float(value)

def test_any_mapping_is_metadata(tmp_path):
	path = tmp_path / 'image.mrc'
	metadata = types.MappingProxyType({'pixel_size_x': 0.5})
	vio.write_single(__setup_array((4, 6)), path, metadata=metadata)
	assert vio.read_metadata(path)['pixel_size_x'] == 0.5

def test_what_another_format_states_is_left_out(tmp_path):
	path = tmp_path / 'image.mrc'
	metadata = {
		'pixel_size_x': 0.5,
		'tiff.software': 'something else',
		'application.frames': 40,
		'application.dose': 1.25,
	}
	vio.write_single(__setup_array((4, 6)), path, metadata=metadata)
	assert set(vio.read_metadata(path)) == {'pixel_size_x', 'mrc.labels'}

def test_a_pixel_size_a_file_can_not_hold_is_left_out(tmp_path):
	path = tmp_path / 'image.mrc'
	metadata = {'pixel_size_x': -1.0, 'pixel_size_y': float('nan')}
	vio.write_single(__setup_array((4, 6)), path, metadata=metadata)
	read = vio.read_metadata(path)
	assert 'pixel_size_x' not in read
	assert 'pixel_size_y' not in read

def test_a_misspelled_standard_field_is_refused(tmp_path):
	with pytest.raises(ValueError):
		vio.write_single(
			__setup_array((4, 6)), tmp_path / 'image.mrc',
			metadata={'pixelsize_x': 0.5}
		)

def test_a_standard_field_of_another_type_is_refused(tmp_path):
	with pytest.raises(ValueError):
		vio.write_single(
			__setup_array((4, 6)), tmp_path / 'image.mrc',
			metadata={'pixel_size_x': '0.5'}
		)

@pytest.mark.parametrize(
	'metadata',
	[
		{'pixel_size_x': True},
		{'application.flag': False},
		{'application.values': [1, 2]},
		{'application.nothing': None},
		{'application.bytes': b'text'},
		{1: 0.5},
		[('pixel_size_x', 0.5)],
		'pixel_size_x',
		0.5,
	]
)
def test_what_is_no_metadata_is_refused(tmp_path, metadata):
	with pytest.raises(TypeError):
		vio.write_single(
			__setup_array((4, 6)), tmp_path / 'image.mrc', metadata=metadata
		)

def test_an_integer_beyond_what_the_metadata_holds_is_refused(tmp_path):
	with pytest.raises(OverflowError):
		vio.write_single(
			__setup_array((4, 6)), tmp_path / 'image.mrc',
			metadata={'application.frames': 2 ** 63}
		)

def test_nothing_is_written_when_the_metadata_is_refused(tmp_path):
	path = tmp_path / 'image.mrc'
	with pytest.raises(ValueError):
		vio.write_single(
			__setup_array((4, 6)), path, metadata={'pixelsize_x': 0.5}
		)
	assert not path.exists()

def test_a_declared_file_is_created_with_its_metadata(tmp_path):
	path = tmp_path / 'stack.mrcs'
	descriptor = vio.ImageDescriptor((2, 4, 6), 2, vio.NumericalType.float32)
	writers = vio.writer_provider()
	writers.declare(
		path, descriptor, {'pixel_size_x': 0.5, 'pixel_size_y': 0.25}
	)
	saver = vio.saver(writers)
	locations = [vio.ImageLocation(path, 0), vio.ImageLocation(path, 1)]
	vio.write_batch_async(saver, __setup_array((2, 4, 6)), locations).get()
	writers.close(path)
	read = vio.read_metadata(path)
	assert read['pixel_size_x'] == 0.5
	assert read['pixel_size_y'] == 0.25

def test_a_file_is_declared_with_metadata_by_name(tmp_path):
	writers = vio.writer_provider()
	writers.declare(
		key=tmp_path / 'stack.mrcs',
		descriptor=vio.ImageDescriptor(
			(2, 4, 6), 2, vio.NumericalType.float32
		),
		metadata={'pixel_size_x': 0.5}
	)
	assert writers.file_count == 1

def test_a_file_is_not_declared_with_metadata_that_is_refused(tmp_path):
	writers = vio.writer_provider()
	descriptor = vio.ImageDescriptor((2, 4, 6), 2, vio.NumericalType.float32)
	with pytest.raises(ValueError):
		writers.declare(
			tmp_path / 'stack.mrcs', descriptor, {'pixelsize_x': 0.5}
		)
	with pytest.raises(TypeError):
		writers.declare(
			tmp_path / 'stack.mrcs', descriptor, {'pixel_size_x': True}
		)
	assert writers.file_count == 0

def test_a_path_is_the_same_as_a_string_and_as_a_path(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': 0.5}
	)
	assert vio.read_metadata(str(path)) == vio.read_metadata(path)

def test_the_formats_to_read_the_metadata_with_may_be_given(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(
		__setup_array((4, 6)), path, metadata={'pixel_size_x': 0.5}
	)
	formats = vio.ImageFileReadFormatSelector.get_shared()
	assert vio.read_metadata(path, formats) == vio.read_metadata(path)
	assert formats.read_metadata(path) == vio.read_metadata(path)

def test_the_metadata_of_a_file_that_is_not_there_is_not_read(tmp_path):
	with pytest.raises(RuntimeError):
		vio.read_metadata(tmp_path / 'absent.mrc')

def test_a_map_of_the_emdb_states_its_voxel_size():
	read = vio.read_metadata(ASSETS / 'EMD-3197.map')
	assert read == {
		'mrc.labels': '::::EMDATABANK.org::::EMD-3197::::',
		'pixel_size_x': pytest.approx(11.4),
		'pixel_size_y': pytest.approx(11.4),
		'pixel_size_z': pytest.approx(11.4),
	}

def test_a_map_whose_columns_run_along_z_states_its_voxel_size():
	read = vio.read_metadata(ASSETS / 'EMD-3001.map')
	assert read['pixel_size_x'] == pytest.approx(17.93 / 40, rel=TOLERANCE)
	assert read['pixel_size_y'] == pytest.approx(4.71 / 12, rel=TOLERANCE)
	assert read['pixel_size_z'] == pytest.approx(33.03 / 72, rel=TOLERANCE)

def test_a_tiff_file_states_what_it_is_written_with(tmp_path):
	path = tmp_path / 'image.tif'
	metadata = {
		'pixel_size_x': 0.83,
		'pixel_size_y': 0.84,
		'tiff.image_description': 'a micrograph',
		'tiff.software': 'something',
		'tiff.date_time': '2026:11:26 12:00:00',
	}
	__write_tiff(__setup_array((4, 6)), path, metadata)
	read = vio.read_metadata(path)
	assert read == {
		'pixel_size_x': pytest.approx(0.83, rel=TOLERANCE),
		'pixel_size_y': pytest.approx(0.84, rel=TOLERANCE),
		'tiff.image_description': 'a micrograph',
		'tiff.software': 'something',
		'tiff.date_time': '2026:11:26 12:00:00',
	}

def test_a_tiff_file_says_vitrio_made_it_unless_told_what_did(tmp_path):
	path = tmp_path / 'image.tif'
	__write_tiff(__setup_array((4, 6)), path, None)
	read = vio.read_metadata(path)
	assert list(read) == ['tiff.software']
	assert read['tiff.software'] == 'vitrio ' + vio.__version__
	__write_tiff(
		__setup_array((4, 6)), path, {'tiff.software': 'something'}
	)
	assert vio.read_metadata(path) == {'tiff.software': 'something'}

def test_the_pixel_size_of_an_mrc_file_goes_into_a_tiff_file(tmp_path):
	source = tmp_path / 'image.mrc'
	converted = tmp_path / 'image.tif'
	image = __setup_array((4, 6))
	vio.write_single(
		image, source, metadata={'pixel_size_x': 0.5, 'pixel_size_y': 0.25}
	)
	__write_tiff(image, converted, vio.read_metadata(source))
	assert vio.read_metadata(converted) == {
		'pixel_size_x': pytest.approx(0.5, rel=TOLERANCE),
		'pixel_size_y': pytest.approx(0.25, rel=TOLERANCE),
		'tiff.software': 'vitrio ' + vio.__version__,
	}

def test_a_text_that_is_not_utf8_is_read_with_what_is_not_replaced(tmp_path):
	path = tmp_path / 'image.tif'
	__write_tiff(
		__setup_array((4, 6)), path,
		{'tiff.image_description': 'a micrograph at 1 A'}
	)
	raw = path.read_bytes()
	assert raw.count(b'a micrograph at 1 A') == 1
	# The ångström sign of Latin-1, which is no UTF-8.
	path.write_bytes(raw.replace(b'at 1 A', b'at 1 \xc5'))
	read = vio.read_metadata(path)
	assert read['tiff.image_description'] == 'a micrograph at 1 \ufffd'

def __write_tiff(array, path, metadata):
	try:
		vio.write_single(array, path, metadata=metadata)
	except RuntimeError:
		pytest.skip('this build writes no TIFF files')

def __setup_array(shape):
	return np.arange(np.prod(shape), dtype=np.float32).reshape(shape)
