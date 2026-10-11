# SPDX-License-Identifier: LGPL-2.1-or-later

"""Batches handed to PyTorch and JAX.

Neither is required. Each is imported when a batch is first handed to it.
A field whose values are not numbers, such as one of strings, has no
counterpart in either and stays a numpy array.
"""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import numpy as np

Allocator = Callable[[tuple[int, ...], np.dtype], np.ndarray]

def is_numeric(dtype: np.dtype) -> bool:
	"""Whether values of a type are booleans or numbers."""
	return dtype.kind in 'biufc'

def is_cuda(device: Any) -> bool:
	"""Whether a device of PyTorch is a GPU of CUDA."""
	import torch  # noqa: PLC0415
	return device is not None and torch.device(device).type == 'cuda'

def pinned_allocator() -> Allocator:
	"""
	Get an allocator of pinned memory, from PyTorch.

	A batch in pinned memory is copied to a GPU without being staged, and
	while the next one is read. vitrio reads into it where it is.
	"""
	import torch  # noqa: PLC0415

	def allocate(shape: tuple[int, ...], dtype: np.dtype) -> np.ndarray:
		if not is_numeric(dtype):
			return np.empty(shape, dtype)
		element = torch.from_numpy(np.empty(0, dtype)).dtype
		return torch.empty(shape, dtype=element, pin_memory=True).numpy()

	return allocate

def to_torch(
	batch: dict[str, np.ndarray],
	device: Any = None,
	non_blocking: bool = False
) -> dict[str, Any]:
	"""
	Hand a batch to PyTorch.

	Each field of numbers becomes a tensor over the memory of its array,
	then a copy of it on `device` when one is given.
	"""
	import torch  # noqa: PLC0415

	result: dict[str, Any] = {}
	for name, values in batch.items():
		if is_numeric(values.dtype):
			values = torch.from_numpy(values)
			if device is not None:
				values = values.to(device, non_blocking=non_blocking)
		result[name] = values
	return result

def to_jax(
	batch: dict[str, np.ndarray],
	device: Any = None,
	sharding: Any = None
) -> dict[str, Any]:
	"""
	Hand a batch to JAX.

	Every field of numbers is put on `device`, or laid out as `sharding`
	says, in one call that returns before the copies are done. When the
	sharding spans the devices of several processes, the batch is this
	process's share of the batch the arrays hold, as each process reads a
	shard of every batch.

	JAX holds no 64-bit values unless it is told to, and turns them into
	32-bit ones.
	"""
	import jax  # noqa: PLC0415

	numeric = {
		name: values for name, values in batch.items()
		if is_numeric(values.dtype)
	}
	if sharding is not None and jax.process_count() > 1:
		moved = {
			name: jax.make_array_from_process_local_data(sharding, values)
			for name, values in numeric.items()
		}
	else:
		target = sharding if sharding is not None else device
		moved = jax.device_put(numeric, target)
	return {name: moved.get(name, values) for name, values in batch.items()}
