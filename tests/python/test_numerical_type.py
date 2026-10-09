# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

NumericalType = vio.NumericalType

NUMPY_TYPES = [
	(NumericalType.boolean, np.bool_),
	(NumericalType.int8, np.int8),
	(NumericalType.uint8, np.uint8),
	(NumericalType.int16, np.int16),
	(NumericalType.uint16, np.uint16),
	(NumericalType.int32, np.int32),
	(NumericalType.uint32, np.uint32),
	(NumericalType.int64, np.int64),
	(NumericalType.uint64, np.uint64),
	(NumericalType.float16, np.float16),
	(NumericalType.float32, np.float32),
	(NumericalType.float64, np.float64),
	(NumericalType.complex_float32, np.complex64),
	(NumericalType.complex_float64, np.complex128),
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
	assert NumericalType(np.dtype(dtype)) is member

@pytest.mark.parametrize(('member', 'dtype'), NUMPY_TYPES)
def test_is_given_as_the_name_numpy_gives_it(member, dtype):
	assert NumericalType(np.dtype(dtype).name) is member

def test_is_given_as_any_string_numpy_understands():
	assert NumericalType('f4') is NumericalType.float32

def test_is_given_as_the_dtype_of_an_array():
	array = np.zeros((2, 3), dtype=np.int16)
	assert NumericalType(array.dtype) is NumericalType.int16

@pytest.mark.parametrize(
	'value',
	[
		pytest.param('nonsense', id="Unknown name"),
		pytest.param(None, id="None"),
		pytest.param(object(), id="Not a data type"),
		pytest.param(np.dtype('U4'), id="A type no file holds"),
	]
)
def test_what_names_no_member_is_refused(value):
	with pytest.raises(ValueError):
		NumericalType(value)
