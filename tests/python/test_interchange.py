# SPDX-License-Identifier: LGPL-2.1-or-later

# Arrays cross between vitrio and the libraries it is used with in both
# directions, and their memory is never copied. Each test shows it by
# writing through one side and reading through the other.

import numpy as np
import pytest

import vitrio as vio

def test_reads_into_an_array_of_numpy(__written_stack):
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read(__written_stack, out=destination)
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def test_reads_into_a_view_of_a_larger_array(__written_stack):
	batch = np.zeros((3, 2, 4, 6), dtype=np.float32)
	vio.read(__written_stack, out=batch[:, 1])
	assert np.array_equal(batch[:, 1], __setup_array((3, 4, 6)))
	assert not batch[:, 0].any()

def test_reads_into_a_reversed_view(__written_stack):
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read(__written_stack, out=destination[::-1, :, ::-1])
	assert np.array_equal(
		destination, __setup_array((3, 4, 6))[::-1, :, ::-1]
	)

def test_reads_into_a_transposed_view(__written_stack):
	destination = np.zeros((6, 4, 3), dtype=np.float32)
	vio.read(__written_stack, out=destination.T)
	assert np.array_equal(destination.T, __setup_array((3, 4, 6)))

def test_reads_a_batch_into_a_view_of_a_larger_array(__written_stack):
	loader = vio.loader()
	batch = np.zeros((2, 3, 4, 6), dtype=np.float32)
	locations = [vio.ImageLocation(__written_stack, i) for i in (2, 0)]
	vio.read_batch_async(loader, batch[:, 1], locations).get()
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(batch[0, 1], stack[2])
	assert np.array_equal(batch[1, 1], stack[0])
	assert not batch[:, 0].any()
	assert not batch[:, 2].any()

@pytest.mark.parametrize(
	'make_view',
	[
		pytest.param(lambda a: a[:, ::2], id="Strided"),
		pytest.param(lambda a: a[::-1, ::-1], id="Reversed"),
		pytest.param(lambda a: a.T, id="Transposed"),
		pytest.param(lambda a: np.asfortranarray(a), id="Fortran"),
	]
)
def test_writes_a_view_of_numpy_as_numpy_sees_it(make_view, tmp_path):
	path = tmp_path / 'image.mrc'
	source = make_view(__setup_array((4, 6)))
	vio.write_single(source, path)
	assert np.array_equal(np.asarray(vio.read(path)), source)

def test_writes_an_array_that_cannot_be_written_to(tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	source.flags.writeable = False
	vio.write_single(source, path)
	assert np.array_equal(np.asarray(vio.read(path)), source)

def test_does_not_read_into_an_array_that_cannot_be_written_to(
	__written_stack
):
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	destination.flags.writeable = False
	with pytest.raises(TypeError):
		vio.read(__written_stack, out=destination)

def test_reads_into_a_memoryview(__written_stack):
	storage = bytearray(3 * 4 * 6 * 4)
	destination = memoryview(storage).cast('f', (3, 4, 6))
	vio.read(__written_stack, out=destination)
	result = np.frombuffer(storage, dtype=np.float32)
	assert np.array_equal(result.reshape(3, 4, 6), __setup_array((3, 4, 6)))

def test_reads_into_an_array_of_its_own(__written_stack):
	destination = vio.read(__written_stack)
	np.asarray(destination)[...] = 0
	vio.read(__written_stack, out=destination)
	assert np.array_equal(
		np.asarray(destination), __setup_array((3, 4, 6))
	)

# The bytes of such an array are not the values it holds, and nothing says
# so to whoever is handed its memory. So it is not handed over.
def test_an_array_of_another_byte_order_is_refused(tmp_path):
	source = __setup_array((4, 6))
	swapped = source.astype(source.dtype.newbyteorder())
	with pytest.raises(TypeError):
		vio.write_single(swapped, tmp_path / 'image.mrc')

def test_an_array_that_is_not_aligned_is_refused(tmp_path):
	storage = bytearray(4 * 6 * 4 + 1)
	source = np.frombuffer(storage, dtype=np.float32, offset=1)
	with pytest.raises(ValueError):
		vio.write_single(source.reshape(4, 6), tmp_path / 'image.mrc')

@pytest.mark.parametrize(
	'source',
	[
		pytest.param([[1.0, 2.0], [3.0, 4.0]], id="List"),
		pytest.param(np.zeros((2, 2), dtype=object), id="Objects"),
		pytest.param(
			np.zeros((2, 2), dtype=[('value', 'f4')]), id="Records"
		),
	]
)
def test_what_hands_out_no_numbers_is_refused(source, tmp_path):
	with pytest.raises(TypeError):
		vio.write_single(source, tmp_path / 'image.mrc')

def test_torch_shares_the_memory_of_an_array(__written_stack):
	torch = pytest.importorskip('torch')
	array = vio.read(__written_stack)
	tensor = torch.from_dlpack(array)
	assert tensor.shape == (3, 4, 6)
	assert tensor.dtype == torch.float32
	tensor[1, 2, 3] = -5
	assert np.asarray(array)[1, 2, 3] == -5

def test_the_tensor_of_torch_keeps_the_memory_alive(__written_stack):
	torch = pytest.importorskip('torch')
	tensor = torch.from_dlpack(vio.read(__written_stack))
	assert torch.equal(tensor, torch.from_numpy(__setup_array((3, 4, 6))))

def test_reads_into_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	destination = torch.zeros((3, 4, 6), dtype=torch.float32)
	assert vio.read(__written_stack, out=destination) is destination
	assert torch.equal(destination, torch.from_numpy(__setup_array((3, 4, 6))))

def test_reads_into_a_view_of_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	batch = torch.zeros((3, 2, 4, 6), dtype=torch.float32)
	vio.read(__written_stack, out=batch[:, 1])
	assert torch.equal(batch[:, 1], torch.from_numpy(__setup_array((3, 4, 6))))
	assert not batch[:, 0].any()

def test_reads_a_batch_into_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	loader = vio.loader()
	destination = torch.zeros((3, 4, 6), dtype=torch.float32)
	locations = [vio.ImageLocation(__written_stack, i) for i in range(3)]
	vio.read_batch_async(loader, destination, locations).get()
	assert torch.equal(destination, torch.from_numpy(__setup_array((3, 4, 6))))

def test_writes_a_tensor_of_torch(tmp_path):
	torch = pytest.importorskip('torch')
	path = tmp_path / 'stack.mrcs'
	source = torch.from_numpy(__setup_array((3, 4, 6)))
	vio.write_stack(source, path)
	assert np.array_equal(
		np.asarray(vio.read(path)), __setup_array((3, 4, 6))
	)

def test_jax_reads_the_values_of_an_array(__written_stack):
	jax_numpy = pytest.importorskip('jax.numpy')
	result = jax_numpy.from_dlpack(vio.read(__written_stack))
	assert result.shape == (3, 4, 6)
	assert np.array_equal(np.asarray(result), __setup_array((3, 4, 6)))

def test_writes_an_array_of_jax(tmp_path):
	jax_numpy = pytest.importorskip('jax.numpy')
	path = tmp_path / 'stack.mrcs'
	source = jax_numpy.asarray(__setup_array((3, 4, 6)))
	vio.write_stack(source, path)
	assert np.array_equal(
		np.asarray(vio.read(path)), __setup_array((3, 4, 6))
	)

# An array of JAX cannot be changed, and it says so when it is asked for
# memory to write to.
def test_does_not_read_into_an_array_of_jax(__written_stack):
	jax_numpy = pytest.importorskip('jax.numpy')
	destination = jax_numpy.zeros((3, 4, 6), dtype=jax_numpy.float32)
	with pytest.raises(TypeError):
		vio.read(__written_stack, out=destination)

def test_rexlib_shares_the_memory_of_an_array(__written_stack):
	rexlib = __import_rexlib()
	array = vio.read(__written_stack)
	with rexlib.device('cpu'):
		shared = rexlib.from_dlpack(array)
		np.asarray(array)[1, 2, 3] = -5
		assert shared.shape == (3, 4, 6)
		assert np.asarray(shared)[1, 2, 3] == -5

def test_reads_into_an_array_of_rexlib(__written_stack):
	rexlib = __import_rexlib()
	with rexlib.device('cpu'):
		destination = rexlib.from_dlpack(
			np.zeros((3, 4, 6), dtype=np.float32)
		)
		vio.read(__written_stack, out=destination)
		assert np.array_equal(
			np.asarray(destination), __setup_array((3, 4, 6))
		)

def test_writes_an_array_of_rexlib(tmp_path):
	rexlib = __import_rexlib()
	path = tmp_path / 'stack.mrcs'
	with rexlib.device('cpu'):
		source = rexlib.from_dlpack(__setup_array((3, 4, 6)))
		vio.write_stack(source, path)
	assert np.array_equal(
		np.asarray(vio.read(path)), __setup_array((3, 4, 6))
	)

# rexlib gained DLPack after its first release, and one without it has no
# way of taking the memory of an array.
def __import_rexlib():
	rexlib = pytest.importorskip('rexlib')
	if not hasattr(rexlib, 'from_dlpack'):
		pytest.skip("this rexlib does not exchange memory through DLPack")
	return rexlib

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	return path
