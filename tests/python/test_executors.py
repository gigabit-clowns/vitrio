# SPDX-License-Identifier: LGPL-2.1-or-later

import pytest

import vitrio as vio

def test_a_thread_pool_is_an_executor():
	assert isinstance(vio.ThreadPoolExecutor(2), vio.Executor)

def test_a_synchronous_executor_is_an_executor():
	assert isinstance(vio.SynchronousExecutor(), vio.Executor)

def test_a_thread_pool_has_the_workers_it_was_given():
	assert vio.ThreadPoolExecutor(3).worker_count == 3

def test_the_worker_count_is_accepted_by_name():
	assert vio.ThreadPoolExecutor(worker_count=2).worker_count == 2

def test_a_thread_pool_needs_a_worker_count():
	with pytest.raises(TypeError):
		vio.ThreadPoolExecutor()

def test_the_abstract_executor_is_not_constructed():
	with pytest.raises(TypeError):
		vio.Executor()

def test_a_loader_is_given_the_executor_it_runs_on():
	executor = vio.ThreadPoolExecutor(2)
	loader = vio.loader(executor=executor)
	assert isinstance(loader, vio.ExecutorImageLoader)

def test_one_executor_serves_a_loader_and_a_saver():
	executor = vio.ThreadPoolExecutor(2)
	loader = vio.loader(executor=executor)
	saver = vio.saver(vio.writer_provider(), executor=executor)
	assert isinstance(loader, vio.ImageLoader)
	assert isinstance(saver, vio.ImageSaver)
