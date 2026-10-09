# SPDX-License-Identifier: LGPL-2.1-or-later

import pytest

import vitrio

def test_a_thread_pool_is_an_executor():
	assert isinstance(vitrio.ThreadPoolExecutor(2), vitrio.Executor)

def test_a_synchronous_executor_is_an_executor():
	assert isinstance(vitrio.SynchronousExecutor(), vitrio.Executor)

def test_a_thread_pool_has_the_workers_it_was_given():
	assert vitrio.ThreadPoolExecutor(3).worker_count == 3

def test_the_worker_count_is_accepted_by_name():
	assert vitrio.ThreadPoolExecutor(worker_count=2).worker_count == 2

def test_a_thread_pool_needs_a_worker_count():
	with pytest.raises(TypeError):
		vitrio.ThreadPoolExecutor()

def test_the_abstract_executor_is_not_constructed():
	with pytest.raises(TypeError):
		vitrio.Executor()

def test_a_loader_is_given_the_executor_it_runs_on():
	executor = vitrio.ThreadPoolExecutor(2)
	loader = vitrio.loader(executor=executor)
	assert isinstance(loader, vitrio.ExecutorImageLoader)

def test_one_executor_serves_a_loader_and_a_saver():
	executor = vitrio.ThreadPoolExecutor(2)
	loader = vitrio.loader(executor=executor)
	saver = vitrio.saver(vitrio.writer_provider(), executor=executor)
	assert isinstance(loader, vitrio.ImageLoader)
	assert isinstance(saver, vitrio.ImageSaver)
