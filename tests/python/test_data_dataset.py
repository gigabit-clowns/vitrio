# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import pickle

import numpy as np
import pytest

import vitrio as vio

def test_has_a_row_for_each_value_of_its_fields(__dataset):
	assert len(__dataset) == 3

def test_a_batch_holds_each_field_for_the_rows_asked_for(__dataset):
	batch = __dataset[[2, 0, 2]]
	assert list(batch) == ['image', 'label', 'name', 'index']
	assert np.array_equal(batch['image'], __setup_array((3, 4, 6))[[2, 0, 2]])
	assert np.array_equal(batch['label'], [12, 10, 12])
	assert list(batch['name']) == ['c', 'a', 'c']
	assert np.array_equal(batch['index'], [2, 0, 2])

def test_an_integer_reads_one_row(__dataset):
	row = __dataset[1]
	assert np.array_equal(row['image'], __setup_array((3, 4, 6))[1])
	assert row['label'] == 11
	assert row['index'] == 1

def test_a_negative_integer_counts_from_the_end(__dataset):
	assert __dataset[-1]['index'] == 2

def test_a_slice_reads_a_batch(__dataset):
	assert np.array_equal(__dataset[1:]['index'], [1, 2])

def test_a_mask_reads_the_rows_it_marks(__dataset):
	mask = np.array([True, False, True])
	assert np.array_equal(__dataset[mask]['label'], [10, 12])

@pytest.mark.parametrize(
	"rows",
	[
		pytest.param(3, id="Past the end"),
		pytest.param(-4, id="Before the start"),
		pytest.param([0, 3], id="Batch past the end"),
		pytest.param(np.array([True, False]), id="Mask of another length"),
	]
)
def test_a_row_past_either_end_is_refused(rows, __dataset):
	with pytest.raises(IndexError):
		__dataset[rows]

def test_rows_named_by_what_is_not_an_integer_are_refused(__dataset):
	with pytest.raises(TypeError):
		__dataset[np.array([0.5])]

def test_an_empty_batch_has_no_rows(__dataset):
	batch = __dataset[[]]
	assert batch['image'].shape == (0, 4, 6)

def test_goes_through_its_rows_when_iterated(__dataset):
	assert [row['index'] for row in __dataset] == [0, 1, 2]

def test_a_batch_read_asynchronously_is_handed_out_once_complete(__dataset):
	pending = __dataset.read_async([0, 1])
	batch = pending.get()
	assert pending.is_ready
	assert np.array_equal(batch['label'], [10, 11])

def test_reads_into_what_the_allocator_makes(__dataset):
	made = []
	def allocate(shape, dtype):
		made.append((shape, dtype))
		return np.zeros(shape, dtype)
	__dataset.read_async([0, 1], allocate).get()
	assert ((2, 4, 6), np.dtype(np.float32)) in made
	assert ((2,), np.dtype(np.int64)) in made

def test_the_indices_go_under_the_name_asked_for(__written_stack):
	dataset = vio.data.Dataset(
		{'image': vio.data.ImageSource(__locations(__written_stack))},
		index='row'
	)
	assert list(dataset[[0]]) == ['image', 'row']

def test_the_indices_are_left_out_when_asked_to(__written_stack):
	dataset = vio.data.Dataset(
		{'image': vio.data.ImageSource(__locations(__written_stack))},
		index=None
	)
	assert list(dataset[[0]]) == ['image']

def test_a_field_named_as_the_indices_is_refused():
	with pytest.raises(ValueError):
		vio.data.Dataset({'index': [1, 2]})

def test_fields_of_different_lengths_are_refused(__written_stack):
	with pytest.raises(ValueError):
		vio.data.Dataset({
			'image': vio.data.ImageSource(__locations(__written_stack)),
			'label': [1, 2],
		})

def test_a_dataset_needs_a_field():
	with pytest.raises(ValueError):
		vio.data.Dataset({})

def test_a_field_of_a_single_value_is_refused():
	with pytest.raises(ValueError):
		vio.data.Dataset({'label': 3})

def test_a_failed_read_is_raised_by_the_batch(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	source = vio.data.ImageSource(__locations(path), check=False)
	path.unlink()
	dataset = vio.data.Dataset({'image': source})
	with pytest.raises(RuntimeError):
		dataset[[0]]

def test_a_subset_holds_the_rows_asked_for_in_order(__dataset):
	subset = __dataset.subset([2, 0])
	batch = subset[[0, 1]]
	assert len(subset) == 2
	assert np.array_equal(batch['label'], [12, 10])
	assert np.array_equal(batch['image'], __setup_array((3, 4, 6))[[2, 0]])
	assert np.array_equal(batch['index'], [0, 1])

def test_a_preloaded_dataset_holds_the_same_rows(__dataset):
	preloaded = __dataset.preload()
	assert isinstance(preloaded.fields['image'], vio.data.ArraySource)
	for name, values in __dataset[[0, 1, 2]].items():
		assert np.array_equal(preloaded[[0, 1, 2]][name], values)

def test_a_field_that_is_not_a_source_is_not_preloaded(__dataset):
	with pytest.raises(ValueError):
		__dataset.preload(['label'])

def test_survives_pickling_and_reads_afterwards(__dataset):
	__dataset[[0]]
	copy = pickle.loads(pickle.dumps(__dataset))
	assert np.array_equal(copy[[1]]['image'], __dataset[[1]]['image'])

# A loader with a scratch is told apart by reading what the scratch took in
# after the file has changed. It keeps no reader open, which Windows would
# not let the file be written under.
def test_reads_on_the_loader_it_is_given(__written_stack):
	locations = __locations(__written_stack)
	dataset = vio.data.Dataset({'image': vio.data.ImageSource(locations)})
	loader = vio.loader(scratch=vio.create_host_image_scratch(locations, 3))
	dataset.read_async([0, 1, 2], loader=loader).get()
	__overwrite(__written_stack)
	batch = dataset.read_async([1], loader=loader).get()
	assert np.array_equal(batch['image'][0], __setup_array((3, 4, 6))[1])

def test_preloads_on_the_loader_it_is_given(__written_stack):
	locations = __locations(__written_stack)
	dataset = vio.data.Dataset({'image': vio.data.ImageSource(locations)})
	loader = vio.loader(scratch=vio.create_host_image_scratch(locations, 3))
	dataset.read_async([0, 1, 2], loader=loader).get()
	__overwrite(__written_stack)
	preloaded = dataset.preload(loader=loader)
	assert np.array_equal(
		preloaded.fields['image'].array, __setup_array((3, 4, 6))
	)

def test_works_with_the_data_loader_of_torch(__dataset):
	torch = pytest.importorskip('torch')
	loader = torch.utils.data.DataLoader(
		__dataset,
		sampler=torch.utils.data.BatchSampler(
			torch.utils.data.SequentialSampler(__dataset), 2, False
		),
		batch_size=None
	)
	batches = list(loader)
	assert [len(batch['index']) for batch in batches] == [2, 1]
	assert isinstance(batches[0]['image'], torch.Tensor)
	assert torch.equal(
		batches[0]['image'], torch.from_numpy(__setup_array((3, 4, 6))[:2])
	)

def test_works_with_the_data_loader_of_torch_one_row_at_a_time(__dataset):
	torch = pytest.importorskip('torch')
	numeric = __dataset.subset([0, 1, 2])
	numeric = vio.data.Dataset({
		'image': numeric.fields['image'], 'label': numeric.fields['label']
	})
	batch = next(iter(torch.utils.data.DataLoader(numeric, batch_size=3)))
	assert batch['image'].shape == (3, 4, 6)
	assert torch.equal(batch['index'], torch.tensor([0, 1, 2]))

def test_works_with_worker_processes_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	dataset = vio.data.Dataset({
		'image': vio.data.ImageSource(__locations(__written_stack)),
	})
	loader = torch.utils.data.DataLoader(
		dataset,
		sampler=torch.utils.data.BatchSampler(
			torch.utils.data.SequentialSampler(dataset), 2, False
		),
		batch_size=None,
		num_workers=2,
		multiprocessing_context='spawn'
	)
	indices = torch.cat([batch['index'] for batch in loader])
	assert torch.equal(indices, torch.tensor([0, 1, 2]))

# Python warns of any fork of a process that runs threads, which this one
# does on purpose.
@pytest.mark.skipif(not hasattr(os, 'fork'), reason="Needs fork")
@pytest.mark.filterwarnings("ignore:This process .* is multi-threaded")
def test_works_with_forked_worker_processes_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	dataset = vio.data.Dataset({
		'image': vio.data.ImageSource(__locations(__written_stack)),
	})
	# The loader shared by the process is made before the fork, and a worker
	# makes one of its own.
	dataset[[0]]
	loader = torch.utils.data.DataLoader(
		dataset,
		sampler=torch.utils.data.BatchSampler(
			torch.utils.data.SequentialSampler(dataset), 2, False
		),
		batch_size=None,
		num_workers=2,
		multiprocessing_context='fork'
	)
	images = torch.cat([batch['image'] for batch in loader])
	assert torch.equal(images, torch.from_numpy(__setup_array((3, 4, 6))))

def __overwrite(path):
	vio.write_stack(np.zeros((3, 4, 6), dtype=np.float32), path)

def __locations(path):
	return [vio.ImageLocation(path, index) for index in range(3)]

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	return path

@pytest.fixture
def __dataset(__written_stack):
	return vio.data.Dataset({
		'image': vio.data.ImageSource(__locations(__written_stack)),
		'label': np.array([10, 11, 12]),
		'name': np.array(['a', 'b', 'c'], dtype=object),
	})
