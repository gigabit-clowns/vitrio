# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy
import pytest

import vitrio

NumericalType = vitrio.NumericalType

NUMPY_TYPES = [
	(NumericalType.boolean, numpy.bool_),
	(NumericalType.int8, numpy.int8),
	(NumericalType.uint8, numpy.uint8),
	(NumericalType.int16, numpy.int16),
	(NumericalType.uint16, numpy.uint16),
	(NumericalType.int32, numpy.int32),
	(NumericalType.uint32, numpy.uint32),
	(NumericalType.int64, numpy.int64),
	(NumericalType.uint64, numpy.uint64),
	(NumericalType.float16, numpy.float16),
	(NumericalType.float32, numpy.float32),
	(NumericalType.float64, numpy.float64),
	(NumericalType.complex_float32, numpy.complex64),
	(NumericalType.complex_float64, numpy.complex128),
]

def test_a_member_is_itself():
	assert NumericalType(NumericalType.float32) is NumericalType.float32

@pytest.mark.parametrize('member', list(NumericalType))
def test_is_given_as_the_name_of_a_member(member):
	assert NumericalType(member.name) is member

@pytest.mark.parametrize(('member', 'dtype'), NUMPY_TYPES)
def test_is_given_as_a_type_of_numpy(member, dtype):
	assert NumericalType(dtype) is member

@pytest.mark.parametrize(('member', 'dtype'), NUMPY_TYPES)
def test_is_given_as_a_dtype_of_numpy(member, dtype):
	assert NumericalType(numpy.dtype(dtype)) is member

@pytest.mark.parametrize(('member', 'dtype'), NUMPY_TYPES)
def test_is_given_as_the_name_numpy_gives_it(member, dtype):
	assert NumericalType(numpy.dtype(dtype).name) is member

def test_is_given_as_any_string_numpy_understands():
	assert NumericalType('f4') is NumericalType.float32

def test_is_given_as_the_dtype_of_an_array():
	array = numpy.zeros((2, 3), dtype=numpy.int16)
	assert NumericalType(array.dtype) is NumericalType.int16

@pytest.mark.parametrize(
	'value',
	[
		pytest.param('nonsense', id="Unknown name"),
		pytest.param(None, id="None"),
		pytest.param(object(), id="Not a data type"),
		pytest.param(numpy.dtype('U4'), id="A type no file holds"),
	]
)
def test_what_names_no_member_is_refused(value):
	with pytest.raises(ValueError):
		NumericalType(value)
