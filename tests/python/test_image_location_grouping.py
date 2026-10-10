# SPDX-License-Identifier: LGPL-2.1-or-later

import gc

import pytest

import vitrio as vio

def test_groups_the_locations_by_file_in_ascending_order_of_key():
	grouping = vio.ImageLocationGrouping([
		vio.ImageLocation('b.mrcs', 0),
		vio.ImageLocation('a.mrcs', 1),
		vio.ImageLocation('b.mrcs', 2),
	])
	assert len(grouping) == 2
	assert [group.key for group in grouping] == ['a.mrcs', 'b.mrcs']

def test_a_group_has_its_indices_sorted_and_once_each():
	grouping = vio.ImageLocationGrouping([
		vio.ImageLocation('stack.mrcs', 4),
		vio.ImageLocation('stack.mrcs', 1),
		vio.ImageLocation('stack.mrcs', 4),
	])
	assert grouping[0].indices == (1, 4)
	assert not grouping[0].is_whole

def test_a_location_without_an_index_makes_its_file_whole():
	grouping = vio.ImageLocationGrouping([
		vio.ImageLocation('stack.mrcs', 1),
		vio.ImageLocation('stack.mrcs'),
	])
	assert grouping[0].is_whole
	assert grouping[0].indices == ()

def test_no_locations_make_no_groups():
	assert len(vio.ImageLocationGrouping([])) == 0

def test_a_group_past_the_last_is_refused():
	grouping = vio.ImageLocationGrouping([vio.ImageLocation('image.mrc')])
	with pytest.raises(IndexError):
		grouping[1]

def test_a_group_keeps_its_grouping_alive():
	group = vio.ImageLocationGrouping([
		vio.ImageLocation('stack.mrcs', 2),
	])[0]
	gc.collect()
	assert group.key == 'stack.mrcs'
	assert group.indices == (2,)

def test_takes_the_locations_by_name():
	grouping = vio.ImageLocationGrouping(
		locations=[vio.ImageLocation('image.mrc')]
	)
	assert len(grouping) == 1
