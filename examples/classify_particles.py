#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Train a network to tell the 2D classes of particles apart.

Reads the STAR file of particles that a 2D classification of RELION writes,
whose `rlnClassNumber` says the class of each particle, and trains a small
convolutional network of PyTorch to predict it from the image. A share of
the particles is kept apart to tell how well the network does on particles
it was not trained on.

	classify_particles.py -i Class2D/job010/run_it025_data.star -e 10

It shows `vitrio.data` at work: the images are zipped with a column of the
STAR file, read in shuffled batches on a thread pool and handed to PyTorch
as tensors, on a GPU when there is one, while the next batches are read.

Paths are used as the STAR file spells them, so a relative one is relative
to the working directory, which is where RELION runs.

Needs vitrio, numpy, starfile and PyTorch.
"""

from __future__ import annotations

import argparse
import os
from collections.abc import Sequence
from pathlib import Path

import numpy as np
import starfile
import torch
import vitrio as vio

IMAGE_NAME = 'rlnImageName'
CLASS_NUMBER = 'rlnClassNumber'

def positive_int(text: str) -> int:
	value = int(text)
	if value < 1:
		raise argparse.ArgumentTypeError(f"{value} is not positive")
	return value

def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Train a network to tell the 2D classes of particles "
		"apart."
	)
	parser.add_argument(
		'-i', '--particles', type=Path, required=True,
		help="STAR file of the classified particles, as RELION writes it"
	)
	parser.add_argument(
		'-e', '--epochs', type=positive_int, default=10,
		help="how many times to go through the particles "
		"(default: %(default)s)"
	)
	parser.add_argument(
		'-b', '--batch-size', type=positive_int, default=64,
		help="particles in a batch (default: %(default)s)"
	)
	parser.add_argument(
		'--validation', type=float, default=0.1,
		help="share of the particles kept apart to validate the network "
		"on (default: %(default)s)"
	)
	parser.add_argument(
		'--preload', action='store_true',
		help="read every particle into memory before training"
	)
	parser.add_argument(
		'--device', default='cuda' if torch.cuda.is_available() else 'cpu',
		help="where PyTorch trains the network (default: %(default)s)"
	)
	parser.add_argument(
		'--seed', type=int, default=0,
		help="what decides the split, the orders and the initial weights "
		"(default: %(default)s)"
	)
	parser.add_argument(
		'-j', '--threads', type=positive_int, default=os.cpu_count() or 1,
		help="threads reading the particles (default: %(default)s)"
	)
	return parser.parse_args(argv)

def make_dataset(
	particles_file: Path,
	threads: int
) -> tuple[vio.data.Dataset, int]:
	"""Zip each particle with its class, counting the classes from zero."""
	particles = starfile.read(particles_file, always_dict=True)['particles']
	classes, labels = np.unique(
		particles[CLASS_NUMBER].to_numpy(), return_inverse=True
	)
	dataset = vio.data.Dataset(
		{
			'image': vio.data.ImageSource(
				vio.data.parse_locations(particles[IMAGE_NAME]),
				data_type=np.float32
			),
			'label': labels.astype(np.int64),
		},
		workers=threads
	)
	return dataset, len(classes)

def split(
	dataset: vio.data.Dataset,
	share: float,
	seed: int
) -> tuple[vio.data.Dataset, vio.data.Dataset]:
	"""Keep a share of the particles apart, chosen at random."""
	rows = np.random.default_rng(seed).permutation(len(dataset))
	kept = max(1, round(share * len(dataset)))
	return dataset.subset(rows[kept:]), dataset.subset(rows[:kept])

def make_network(classes: int) -> torch.nn.Module:
	return torch.nn.Sequential(
		torch.nn.Conv2d(1, 16, 3, padding=1),
		torch.nn.ReLU(),
		torch.nn.MaxPool2d(2),
		torch.nn.Conv2d(16, 32, 3, padding=1),
		torch.nn.ReLU(),
		torch.nn.AdaptiveAvgPool2d(1),
		torch.nn.Flatten(),
		torch.nn.Linear(32, classes),
	)

def normalize(batch: dict[str, torch.Tensor]) -> dict[str, torch.Tensor]:
	"""Bring each image to a mean of zero and a deviation of one, on the
	device the batch was copied to."""
	images = batch['image']
	mean = images.mean(dim=(-2, -1), keepdim=True)
	deviation = images.std(dim=(-2, -1), keepdim=True).clamp_min(1e-6)
	return {**batch, 'image': ((images - mean) / deviation).unsqueeze(1)}

def train(arguments: argparse.Namespace) -> float:
	"""Train the network, and return its accuracy on the validation set."""
	torch.manual_seed(arguments.seed)
	dataset, classes = make_dataset(arguments.particles, arguments.threads)
	if arguments.preload:
		dataset = dataset.preload()
	training, validation = split(
		dataset, arguments.validation, arguments.seed
	)

	def loader(dataset: vio.data.Dataset, shuffle: bool) -> vio.data.DataLoader:
		return vio.data.DataLoader(
			dataset, arguments.batch_size, shuffle=shuffle,
			seed=arguments.seed, output='torch', device=arguments.device,
			transform=normalize
		)

	training_batches = loader(training, shuffle=True)
	validation_batches = loader(validation, shuffle=False)
	network = make_network(classes).to(arguments.device)
	optimizer = torch.optim.Adam(network.parameters(), lr=1e-3)
	loss_of = torch.nn.CrossEntropyLoss()

	accuracy = 0.0
	for epoch in range(1, arguments.epochs + 1):
		network.train()
		total_loss = 0.0
		for batch in training_batches:
			optimizer.zero_grad()
			loss = loss_of(network(batch['image']), batch['label'])
			loss.backward()
			optimizer.step()
			total_loss += loss.item() * len(batch['label'])

		network.eval()
		correct = 0
		with torch.no_grad():
			for batch in validation_batches:
				predicted = network(batch['image']).argmax(dim=1)
				correct += (predicted == batch['label']).sum().item()
		accuracy = correct / len(validation)
		print(
			f"epoch {epoch}: loss {total_loss / len(training):.4f}, "
			f"validation accuracy {accuracy:.3f}",
			flush=True
		)
	return accuracy

def main(argv: Sequence[str] | None = None) -> None:
	train(parse_arguments(argv))

if __name__ == '__main__':
	main()
