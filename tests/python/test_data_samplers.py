# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_draws_the_rows_in_their_order_without_shuffling():
	batches = list(vio.data.BatchSampler(5, 2))
	assert [list(batch) for batch in batches] == [[0, 1], [2, 3], [4]]

def test_counts_its_batches():
	assert len(vio.data.BatchSampler(5, 2)) == 3
	assert len(vio.data.BatchSampler(5, 2, drop_last=True)) == 2

def test_leaves_out_a_last_batch_with_fewer_rows_when_asked_to():
	batches = list(vio.data.BatchSampler(5, 2, drop_last=True))
	assert [list(batch) for batch in batches] == [[0, 1], [2, 3]]

def test_draws_every_row_once_per_epoch_when_shuffling():
	sampler = vio.data.BatchSampler(10, 3, shuffle=True, seed=1)
	rows = np.concatenate(list(sampler))
	assert sorted(rows) == list(range(10))
	assert list(rows) != list(range(10))

def test_every_epoch_has_an_order_of_its_own():
	sampler = vio.data.BatchSampler(50, 50, shuffle=True, seed=1)
	first = next(iter(sampler))
	second = next(iter(sampler))
	assert sampler.epoch == 2
	assert not np.array_equal(first, second)

def test_one_seed_and_epoch_give_one_order():
	first = vio.data.BatchSampler(50, 7, shuffle=True, seed=3)
	second = vio.data.BatchSampler(50, 7, shuffle=True, seed=3)
	second.set_epoch(4)
	first.set_epoch(4)
	assert np.array_equal(
		np.concatenate(list(first)), np.concatenate(list(second))
	)

def test_shards_deal_out_the_rows_of_an_epoch_between_them():
	shards = [
		vio.data.BatchSampler(10, 2, shuffle=True, seed=5, shard=(index, 3))
		for index in range(3)
	]
	rows = [np.concatenate(list(shard)) for shard in shards]
	assert all(len(shard_rows) == 4 for shard_rows in rows)
	assert set(np.concatenate(rows)) == set(range(10))
	assert all(len(shard) == 2 for shard in shards)

def test_shards_draw_batches_of_one_size_when_dropping_the_last():
	shards = [
		vio.data.BatchSampler(10, 3, drop_last=True, shard=(index, 2))
		for index in range(2)
	]
	assert [[len(batch) for batch in shard] for shard in shards] == [[3], [3]]

def test_draws_nothing_of_no_rows():
	assert list(vio.data.BatchSampler(0, 2)) == []

@pytest.mark.parametrize(
	"batch_size, shard",
	[
		pytest.param(0, (0, 1), id="Empty batch"),
		pytest.param(1, (1, 1), id="Shard past its count"),
		pytest.param(1, (0, 0), id="No shard"),
	]
)
def test_what_cannot_be_drawn_is_refused(batch_size, shard):
	with pytest.raises(ValueError):
		vio.data.BatchSampler(4, batch_size, shard=shard)
