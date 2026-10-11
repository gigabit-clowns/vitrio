# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_goes_through_the_dataset_in_batches(__dataset):
	loader = vio.data.DataLoader(__dataset, 2)
	batches = list(loader)
	assert len(loader) == 3
	assert [list(batch['index']) for batch in batches] == [[0, 1], [2, 3], [4]]
	stack = __setup_array((5, 4, 6))
	assert np.array_equal(batches[1]['image'], stack[2:4])

def test_the_metadata_stays_with_its_images_when_shuffling(__dataset):
	loader = vio.data.DataLoader(__dataset, 2, shuffle=True, seed=7)
	stack = __setup_array((5, 4, 6))
	seen = []
	for batch in loader:
		assert np.array_equal(batch['image'], stack[batch['index']])
		assert np.array_equal(batch['label'], batch['index'] * 10)
		seen.extend(batch['index'])
	assert sorted(seen) == [0, 1, 2, 3, 4]

def test_each_iteration_is_an_epoch_of_its_own(__dataset):
	loader = vio.data.DataLoader(__dataset, 5, shuffle=True, seed=2)
	orders = [list(next(iter(loader))['index']) for _ in range(4)]
	assert len({tuple(order) for order in orders}) > 1

def test_leaves_out_a_last_batch_with_fewer_rows_when_asked_to(__dataset):
	loader = vio.data.DataLoader(__dataset, 2, drop_last=True)
	assert [len(batch['index']) for batch in loader] == [2, 2]

def test_pads_the_last_batch_and_says_which_rows_are_real(__dataset):
	batches = list(vio.data.DataLoader(__dataset, 3, pad_last=True))
	assert [len(batch['image']) for batch in batches] == [3, 3]
	assert list(batches[0]['valid']) == [True, True, True]
	assert list(batches[1]['valid']) == [True, True, False]
	assert list(batches[1]['index']) == [3, 4, 3]

def test_names_the_field_of_padding_as_asked(__dataset):
	loader = vio.data.DataLoader(__dataset, 3, pad_last=True, valid='real')
	batch = next(iter(loader))
	assert 'real' in batch

def test_draws_batches_from_the_sampler_it_is_given(__dataset):
	loader = vio.data.DataLoader(__dataset, batch_sampler=[[4, 0], [2]])
	assert [list(batch['index']) for batch in loader] == [[4, 0], [2]]

@pytest.mark.parametrize("prefetch", [1, 2, 10])
def test_hands_out_every_batch_whatever_is_under_way(prefetch, __dataset):
	loader = vio.data.DataLoader(__dataset, 1, prefetch=prefetch)
	assert [int(batch['index'][0]) for batch in loader] == [0, 1, 2, 3, 4]

def test_a_batch_is_not_written_once_handed_out(__dataset):
	batches = list(vio.data.DataLoader(__dataset, 1, prefetch=2))
	stack = __setup_array((5, 4, 6))
	for index, batch in enumerate(batches):
		assert np.array_equal(batch['image'][0], stack[index])

def test_stopping_early_leaves_nothing_under_way(__dataset):
	for batch in vio.data.DataLoader(__dataset, 1, prefetch=4):
		break
	assert batch['index'][0] == 0

def test_hands_the_batch_to_a_callable_output(__dataset):
	loader = vio.data.DataLoader(__dataset, 5, output=lambda batch: len(batch))
	assert list(loader) == [4]

def test_transforms_each_batch_once_handed_out(__dataset):
	loader = vio.data.DataLoader(
		__dataset, 5, transform=lambda batch: batch['image'].mean()
	)
	assert list(loader) == [pytest.approx(__setup_array((5, 4, 6)).mean())]

def test_reads_into_what_the_allocator_makes(__dataset):
	shapes = []
	def allocate(shape, dtype):
		shapes.append(shape)
		return np.empty(shape, dtype)
	list(vio.data.DataLoader(__dataset, 5, allocator=allocate))
	assert (5, 4, 6) in shapes

@pytest.mark.parametrize(
	"arguments",
	[
		pytest.param(
			{'batch_sampler': [[0]], 'shuffle': True},
			id="Sampler and shuffle"
		),
		pytest.param(
			{'batch_sampler': [[0]], 'drop_last': True},
			id="Sampler and drop_last"
		),
		pytest.param(
			{'batch_sampler': [[0]], 'seed': 1}, id="Sampler and seed"
		),
		pytest.param(
			{'drop_last': True, 'pad_last': True}, id="Dropped and padded"
		),
		pytest.param(
			{'pad_last': True, 'valid': 'label'}, id="Padding hides a field"
		),
		pytest.param({'prefetch': 0}, id="Nothing under way"),
		pytest.param({'output': 'tensorflow'}, id="Unknown output"),
		pytest.param({'sharding': object()}, id="Sharding without JAX"),
		pytest.param({'pin_memory': True}, id="Pinned memory without PyTorch"),
	]
)
def test_arguments_that_do_not_apply_are_refused(arguments, __dataset):
	with pytest.raises(ValueError):
		vio.data.DataLoader(__dataset, **arguments)

def test_hands_out_tensors_of_torch(__dataset):
	torch = pytest.importorskip('torch')
	batch = next(iter(vio.data.DataLoader(__dataset, 2, output='torch')))
	assert isinstance(batch['image'], torch.Tensor)
	assert batch['image'].dtype == torch.float32
	assert torch.equal(batch['index'], torch.tensor([0, 1]))
	assert isinstance(batch['name'], np.ndarray)

def test_copies_tensors_to_the_device_asked_for(__dataset):
	pytest.importorskip('torch')
	loader = vio.data.DataLoader(__dataset, 2, output='torch', device='meta')
	batch = next(iter(loader))
	assert batch['image'].device.type == 'meta'

def test_reads_into_pinned_memory_for_a_gpu(__dataset):
	torch = pytest.importorskip('torch')
	if not torch.cuda.is_available():
		pytest.skip("Needs a GPU of CUDA")
	loader = vio.data.DataLoader(__dataset, 2, output='torch', device='cuda')
	batch = next(iter(loader))
	assert batch['image'].device.type == 'cuda'
	assert torch.equal(
		batch['image'].cpu(), torch.from_numpy(__setup_array((5, 4, 6))[:2])
	)

def test_hands_out_arrays_of_jax(__dataset):
	jax = pytest.importorskip('jax')
	batch = next(iter(vio.data.DataLoader(__dataset, 2, output='jax')))
	assert isinstance(batch['image'], jax.Array)
	expected = __setup_array((5, 4, 6))[:2]
	assert np.array_equal(np.asarray(batch['image']), expected)
	assert isinstance(batch['name'], np.ndarray)

def test_puts_arrays_of_jax_on_the_device_asked_for(__dataset):
	jax = pytest.importorskip('jax')
	device = jax.devices()[0]
	loader = vio.data.DataLoader(__dataset, 2, output='jax', device=device)
	batch = next(iter(loader))
	assert batch['image'].devices() == {device}

def test_lays_arrays_of_jax_out_as_a_sharding_says(__dataset):
	jax = pytest.importorskip('jax')
	mesh = jax.sharding.Mesh(np.array(jax.devices()), ('batch',))
	sharding = jax.sharding.NamedSharding(
		mesh, jax.sharding.PartitionSpec('batch')
	)
	loader = vio.data.DataLoader(
		__dataset, len(jax.devices()), output='jax', sharding=sharding,
		drop_last=True
	)
	batch = next(iter(loader))
	assert batch['image'].sharding == sharding

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

@pytest.fixture
def __dataset(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((5, 4, 6)), path)
	return vio.data.Dataset({
		'image': vio.data.ImageSource(
			[vio.ImageLocation(path, index) for index in range(5)]
		),
		'label': np.arange(5) * 10,
		'name': np.array(list('abcde'), dtype=object),
	})
