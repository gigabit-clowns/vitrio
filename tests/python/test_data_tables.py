# SPDX-License-Identifier: LGPL-2.1-or-later

import os

import numpy as np
import pytest

import vitrio as vio

def test_a_mapping_becomes_an_array_for_each_column():
	columns = vio.data.as_columns({'a': [1, 2], 'b': [[1, 2], [3, 4]]})
	assert list(columns) == ['a', 'b']
	assert isinstance(columns['a'], np.ndarray)
	assert columns['b'].shape == (2, 2)

def test_a_structured_array_becomes_its_fields():
	table = np.zeros(3, dtype=[('blob/idx', np.uint32), ('blob/path', 'S8')])
	table['blob/idx'] = [0, 1, 2]
	columns = vio.data.as_columns(table)
	assert list(columns) == ['blob/idx', 'blob/path']
	assert np.array_equal(columns['blob/idx'], [0, 1, 2])

def test_a_data_frame_becomes_its_columns():
	pd = pytest.importorskip('pandas')
	frame = pd.DataFrame({'rlnDefocusU': [1.0, 2.0], 'rlnClassNumber': [1, 2]})
	columns = vio.data.as_columns(frame)
	assert list(columns) == ['rlnDefocusU', 'rlnClassNumber']
	assert np.array_equal(columns['rlnClassNumber'], [1, 2])

def test_columns_of_different_lengths_are_refused():
	with pytest.raises(ValueError):
		vio.data.as_columns({'a': [1, 2], 'b': [1, 2, 3]})

@pytest.mark.parametrize(
	"table",
	[
		pytest.param(np.zeros(3), id="Array that is not structured"),
		pytest.param([1, 2, 3], id="List"),
	]
)
def test_what_is_not_a_table_is_refused(table):
	with pytest.raises(TypeError):
		vio.data.as_columns(table)

def test_parses_locations_as_relion_writes_them():
	locations = vio.data.parse_locations(['000003@stack.mrcs', 'whole.mrc'])
	assert locations == [
		vio.ImageLocation('stack.mrcs', 2),
		vio.ImageLocation('whole.mrc'),
	]

def test_parses_locations_given_as_bytes():
	locations = vio.data.parse_locations([b'1@stack.mrcs'])
	assert locations == [vio.ImageLocation('stack.mrcs', 0)]

def test_parsed_locations_are_relative_to_the_root(tmp_path):
	locations = vio.data.parse_locations(
		['1@Extract/stack.mrcs', '1@/absolute/stack.mrcs'], root=tmp_path
	)
	assert locations[0].key == os.path.join(tmp_path, 'Extract/stack.mrcs')
	assert locations[1].key == '/absolute/stack.mrcs'

def test_a_text_that_is_not_a_location_is_refused():
	with pytest.raises(ValueError):
		vio.data.parse_locations(['0@stack.mrcs'])

def test_makes_locations_as_cryosparc_states_them():
	locations = vio.data.make_locations(
		np.array([b'>J1/stack.mrc', b'J2/stack.mrc']),
		np.array([0, 5], dtype=np.uint32)
	)
	assert locations == [
		vio.ImageLocation('J1/stack.mrc', 0),
		vio.ImageLocation('J2/stack.mrc', 5),
	]

def test_makes_locations_of_whole_files_without_indices(tmp_path):
	locations = vio.data.make_locations(['a.mrc'], root=tmp_path)
	assert locations == [vio.ImageLocation(os.path.join(tmp_path, 'a.mrc'))]

def test_as_many_indices_as_keys_are_asked_for():
	with pytest.raises(ValueError):
		vio.data.make_locations(['a.mrcs', 'b.mrcs'], [0])
