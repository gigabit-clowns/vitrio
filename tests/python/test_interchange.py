# SPDX-License-Identifier: LGPL-2.1-or-later

# Arrays cross between vitrio and the libraries it is used with in both
# directions, and their memory is never copied. Each test shows it by
# writing through one side and reading through the other.

import numpy
import pytest

import vitrio

def test_reads_into_an_array_of_numpy(__written_stack):
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read(__written_stack, out=destination)
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def test_reads_into_a_view_of_a_larger_array(__written_stack):
	batch = numpy.zeros((3, 2, 4, 6), dtype=numpy.float32)
	vitrio.read(__written_stack, out=batch[:, 1])
	assert numpy.array_equal(batch[:, 1], __setup_array((3, 4, 6)))
	assert not batch[:, 0].any()

def test_reads_into_a_reversed_view(__written_stack):
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read(__written_stack, out=destination[::-1, :, ::-1])
	assert numpy.array_equal(
		destination, __setup_array((3, 4, 6))[::-1, :, ::-1]
	)

def test_reads_into_a_transposed_view(__written_stack):
	destination = numpy.zeros((6, 4, 3), dtype=numpy.float32)
	vitrio.read(__written_stack, out=destination.T)
	assert numpy.array_equal(destination.T, __setup_array((3, 4, 6)))

def test_reads_a_batch_into_a_view_of_a_larger_array(__written_stack):
	loader = vitrio.loader()
	batch = numpy.zeros((2, 3, 4, 6), dtype=numpy.float32)
	locations = [vitrio.ImageLocation(__written_stack, i) for i in (2, 0)]
	vitrio.read_batch_async(loader, batch[:, 1], locations).get()
	stack = __setup_array((3, 4, 6))
	assert numpy.array_equal(batch[0, 1], stack[2])
	assert numpy.array_equal(batch[1, 1], stack[0])
	assert not batch[:, 0].any()
	assert not batch[:, 2].any()

@pytest.mark.parametrize(
	'make_view',
	[
		pytest.param(lambda a: a[:, ::2], id="Strided"),
		pytest.param(lambda a: a[::-1, ::-1], id="Reversed"),
		pytest.param(lambda a: a.T, id="Transposed"),
		pytest.param(lambda a: numpy.asfortranarray(a), id="Fortran"),
	]
)
def test_writes_a_view_of_numpy_as_numpy_sees_it(make_view, tmp_path):
	path = tmp_path / 'image.mrc'
	source = make_view(__setup_array((4, 6)))
	vitrio.write_single(source, path)
	assert numpy.array_equal(numpy.asarray(vitrio.read(path)), source)

def test_writes_an_array_that_cannot_be_written_to(tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	source.flags.writeable = False
	vitrio.write_single(source, path)
	assert numpy.array_equal(numpy.asarray(vitrio.read(path)), source)

def test_does_not_read_into_an_array_that_cannot_be_written_to(
	__written_stack
):
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	destination.flags.writeable = False
	with pytest.raises(TypeError):
		vitrio.read(__written_stack, out=destination)

def test_reads_into_a_memoryview(__written_stack):
	storage = bytearray(3 * 4 * 6 * 4)
	destination = memoryview(storage).cast('f', (3, 4, 6))
	vitrio.read(__written_stack, out=destination)
	result = numpy.frombuffer(storage, dtype=numpy.float32)
	assert numpy.array_equal(result.reshape(3, 4, 6), __setup_array((3, 4, 6)))

def test_reads_into_an_array_of_its_own(__written_stack):
	destination = vitrio.read(__written_stack)
	numpy.asarray(destination)[...] = 0
	vitrio.read(__written_stack, out=destination)
	assert numpy.array_equal(
		numpy.asarray(destination), __setup_array((3, 4, 6))
	)

# The bytes of such an array are not the values it holds, and nothing says
# so to whoever is handed its memory. So it is not handed over.
def test_an_array_of_another_byte_order_is_refused(tmp_path):
	source = __setup_array((4, 6))
	swapped = source.astype(source.dtype.newbyteorder())
	with pytest.raises(TypeError):
		vitrio.write_single(swapped, tmp_path / 'image.mrc')

def test_an_array_that_is_not_aligned_is_refused(tmp_path):
	storage = bytearray(4 * 6 * 4 + 1)
	source = numpy.frombuffer(storage, dtype=numpy.float32, offset=1)
	with pytest.raises(ValueError):
		vitrio.write_single(source.reshape(4, 6), tmp_path / 'image.mrc')

@pytest.mark.parametrize(
	'source',
	[
		pytest.param([[1.0, 2.0], [3.0, 4.0]], id="List"),
		pytest.param(numpy.zeros((2, 2), dtype=object), id="Objects"),
		pytest.param(
			numpy.zeros((2, 2), dtype=[('value', 'f4')]), id="Records"
		),
	]
)
def test_what_hands_out_no_numbers_is_refused(source, tmp_path):
	with pytest.raises(TypeError):
		vitrio.write_single(source, tmp_path / 'image.mrc')

def test_torch_shares_the_memory_of_an_array(__written_stack):
	torch = pytest.importorskip('torch')
	array = vitrio.read(__written_stack)
	tensor = torch.from_dlpack(array)
	assert tensor.shape == (3, 4, 6)
	assert tensor.dtype == torch.float32
	tensor[1, 2, 3] = -5
	assert numpy.asarray(array)[1, 2, 3] == -5

def test_the_tensor_of_torch_keeps_the_memory_alive(__written_stack):
	torch = pytest.importorskip('torch')
	tensor = torch.from_dlpack(vitrio.read(__written_stack))
	assert torch.equal(tensor, torch.from_numpy(__setup_array((3, 4, 6))))

def test_reads_into_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	destination = torch.zeros((3, 4, 6), dtype=torch.float32)
	assert vitrio.read(__written_stack, out=destination) is destination
	assert torch.equal(destination, torch.from_numpy(__setup_array((3, 4, 6))))

def test_reads_into_a_view_of_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	batch = torch.zeros((3, 2, 4, 6), dtype=torch.float32)
	vitrio.read(__written_stack, out=batch[:, 1])
	assert torch.equal(batch[:, 1], torch.from_numpy(__setup_array((3, 4, 6))))
	assert not batch[:, 0].any()

def test_reads_a_batch_into_a_tensor_of_torch(__written_stack):
	torch = pytest.importorskip('torch')
	loader = vitrio.loader()
	destination = torch.zeros((3, 4, 6), dtype=torch.float32)
	locations = [vitrio.ImageLocation(__written_stack, i) for i in range(3)]
	vitrio.read_batch_async(loader, destination, locations).get()
	assert torch.equal(destination, torch.from_numpy(__setup_array((3, 4, 6))))

def test_writes_a_tensor_of_torch(tmp_path):
	torch = pytest.importorskip('torch')
	path = tmp_path / 'stack.mrcs'
	source = torch.from_numpy(__setup_array((3, 4, 6)))
	vitrio.write_stack(source, path)
	assert numpy.array_equal(
		numpy.asarray(vitrio.read(path)), __setup_array((3, 4, 6))
	)

def test_jax_reads_the_values_of_an_array(__written_stack):
	jax_numpy = pytest.importorskip('jax.numpy')
	result = jax_numpy.from_dlpack(vitrio.read(__written_stack))
	assert result.shape == (3, 4, 6)
	assert numpy.array_equal(numpy.asarray(result), __setup_array((3, 4, 6)))

def test_writes_an_array_of_jax(tmp_path):
	jax_numpy = pytest.importorskip('jax.numpy')
	path = tmp_path / 'stack.mrcs'
	source = jax_numpy.asarray(__setup_array((3, 4, 6)))
	vitrio.write_stack(source, path)
	assert numpy.array_equal(
		numpy.asarray(vitrio.read(path)), __setup_array((3, 4, 6))
	)

# An array of JAX cannot be changed, and it says so when it is asked for
# memory to write to.
def test_does_not_read_into_an_array_of_jax(__written_stack):
	jax_numpy = pytest.importorskip('jax.numpy')
	destination = jax_numpy.zeros((3, 4, 6), dtype=jax_numpy.float32)
	with pytest.raises(TypeError):
		vitrio.read(__written_stack, out=destination)

def test_rexlib_shares_the_memory_of_an_array(__written_stack):
	rexlib = __import_rexlib()
	array = vitrio.read(__written_stack)
	with rexlib.device('cpu'):
		shared = rexlib.from_dlpack(array)
		numpy.asarray(array)[1, 2, 3] = -5
		assert shared.shape == (3, 4, 6)
		assert numpy.asarray(shared)[1, 2, 3] == -5

def test_reads_into_an_array_of_rexlib(__written_stack):
	rexlib = __import_rexlib()
	with rexlib.device('cpu'):
		destination = rexlib.from_dlpack(
			numpy.zeros((3, 4, 6), dtype=numpy.float32)
		)
		vitrio.read(__written_stack, out=destination)
		assert numpy.array_equal(
			numpy.asarray(destination), __setup_array((3, 4, 6))
		)

def test_writes_an_array_of_rexlib(tmp_path):
	rexlib = __import_rexlib()
	path = tmp_path / 'stack.mrcs'
	with rexlib.device('cpu'):
		source = rexlib.from_dlpack(__setup_array((3, 4, 6)))
		vitrio.write_stack(source, path)
	assert numpy.array_equal(
		numpy.asarray(vitrio.read(path)), __setup_array((3, 4, 6))
	)

# rexlib gained DLPack after its first release, and one without it has no
# way of taking the memory of an array.
def __import_rexlib():
	rexlib = pytest.importorskip('rexlib')
	if not hasattr(rexlib, 'from_dlpack'):
		pytest.skip("this rexlib does not exchange memory through DLPack")
	return rexlib

def __setup_array(shape):
	count = int(numpy.prod(shape))
	return numpy.arange(count, dtype=numpy.float32).reshape(shape)

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vitrio.write_stack(__setup_array((3, 4, 6)), path)
	return path
