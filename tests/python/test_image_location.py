# SPDX-License-Identifier: LGPL-2.1-or-later

import pathlib
import pickle

import pytest

import vitrio

ImageLocation = vitrio.ImageLocation

def test_default_addresses_nothing():
	location = ImageLocation()
	assert location.key == ''
	assert location.index_in_stack == ImageLocation.no_stack_index

def test_key_alone_addresses_the_whole_file():
	location = ImageLocation('stack.mrc')
	assert location.key == 'stack.mrc'
	assert location.index_in_stack == ImageLocation.no_stack_index

def test_index_is_kept_as_given():
	location = ImageLocation('stack.mrc', 2)
	assert location.key == 'stack.mrc'
	assert location.index_in_stack == 2

def test_arguments_are_accepted_by_name():
	location = ImageLocation(key='stack.mrc', index_in_stack=2)
	assert location.index_in_stack == 2

def test_a_path_is_taken_as_the_key_it_names():
	path = pathlib.Path('directory') / 'stack.mrc'
	assert ImageLocation(path, 2) == ImageLocation(str(path), 2)

def test_the_key_is_a_string_whatever_it_was_given_as():
	assert isinstance(ImageLocation(pathlib.Path('stack.mrc')).key, str)

def test_says_whether_it_carries_an_index():
	assert ImageLocation('stack.mrc', 2).has_index_in_stack
	assert not ImageLocation('stack.mrc').has_index_in_stack

def test_equal_locations_compare_equal():
	first = ImageLocation('stack.mrc', 2)
	second = ImageLocation('stack.mrc', 2)
	assert first == second

@pytest.mark.parametrize(
	"other",
	[
		pytest.param(ImageLocation('other.mrc', 2), id="Different key"),
		pytest.param(ImageLocation('stack.mrc', 3), id="Different index"),
		pytest.param(ImageLocation('stack.mrc'), id="Whole file"),
	]
)
def test_different_locations_compare_not_equal(other):
	assert ImageLocation('stack.mrc', 2) != other

def test_orders_by_key_then_index():
	assert ImageLocation('a.mrc', 9) < ImageLocation('b.mrc', 0)
	assert ImageLocation('a.mrc', 0) < ImageLocation('a.mrc', 1)

def test_equal_locations_hash_equal():
	first = ImageLocation('stack.mrc', 2)
	second = ImageLocation('stack.mrc', 2)
	assert hash(first) == hash(second)

def test_usable_as_a_dictionary_key():
	table = {ImageLocation('stack.mrc', 2): 'value'}
	assert table[ImageLocation('stack.mrc', 2)] == 'value'

def test_whole_file_is_written_as_a_bare_key():
	assert str(ImageLocation('stack.mrc')) == 'stack.mrc'

def test_index_is_written_one_based():
	assert str(ImageLocation('stack.mrc', 2)) == '3@stack.mrc'

def test_repr_states_the_key_and_the_index():
	assert repr(ImageLocation('stack.mrc', 2)) == (
		"ImageLocation(key='stack.mrc', index_in_stack=2)"
	)

def test_repr_leaves_out_the_index_of_a_whole_file():
	assert repr(ImageLocation('stack.mrc')) == "ImageLocation(key='stack.mrc')"

def test_pickle():
	location = ImageLocation('stack.mrc', 2)
	assert pickle.loads(pickle.dumps(location)) == location

def test_pickle_keeps_a_whole_file_whole():
	location = ImageLocation('stack.mrc')
	assert not pickle.loads(pickle.dumps(location)).has_index_in_stack

@pytest.mark.parametrize(
	"text, expected",
	[
		pytest.param('stack.mrc', ImageLocation('stack.mrc'), id="Whole file"),
		pytest.param(
			'3@stack.mrc', ImageLocation('stack.mrc', 2), id="One based index"
		),
	]
)
def test_from_string_reads_a_location(text, expected):
	assert ImageLocation.from_string(text) == expected

@pytest.mark.parametrize(
	"text",
	[
		pytest.param('stack.mrc', id="Whole file"),
		pytest.param('3@stack.mrc', id="Index in a stack"),
	]
)
def test_from_string_is_the_inverse_of_str(text):
	assert str(ImageLocation.from_string(text)) == text

@pytest.mark.parametrize(
	"text",
	[
		pytest.param('', id="Empty string"),
		pytest.param('0@stack.mrc', id="Zero is not a valid index"),
		pytest.param('@stack.mrc', id="Empty index"),
		pytest.param('x@stack.mrc', id="Index is not a number"),
		pytest.param('3@', id="Empty key"),
	]
)
def test_from_string_raises_value_error_with_invalid_string(text):
	with pytest.raises(ValueError):
		ImageLocation.from_string(text)

def test_the_constructor_never_parses():
	assert ImageLocation('3@stack.mrc').key == '3@stack.mrc'
