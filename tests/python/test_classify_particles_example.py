# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs examples/classify_particles.py on a few small particles of two
# classes as easy to tell apart as can be, laid out as RELION lays a project
# out, and checks that it trains a network that tells them apart.

import os
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

# What the example needs, besides what this file uses of it.
starfile = pytest.importorskip('starfile')
pd = pytest.importorskip('pandas')
pytest.importorskip('torch')

EXAMPLE = Path(__file__).parents[2] / 'examples' / 'classify_particles.py'
BOX_SIZE = 16
PARTICLES = 48
EPOCH = re.compile(
	r'^epoch (\d+): loss ([0-9.]+), validation accuracy ([0-9.]+)$'
)

def test_reports_every_epoch(__project):
	epochs = __run_example(__project, '-e', '3')
	assert [epoch for epoch, _, _ in epochs] == [1, 2, 3]

def test_learns_to_tell_the_classes_apart(__project):
	epochs = __run_example(__project, '-e', '25')
	assert epochs[-1][2] >= 0.75
	assert epochs[-1][1] < epochs[0][1]

def test_trains_alike_on_particles_read_into_memory(__project):
	read = __run_example(__project, '-e', '2')
	preloaded = __run_example(__project, '-e', '2', '--preload')
	assert preloaded == read

def test_a_number_of_epochs_that_is_not_positive_is_refused(__project):
	result = subprocess.run(
		[sys.executable, str(EXAMPLE), '-i', 'particles.star', '-e', '0'],
		cwd=__project, env=os.environ, capture_output=True, text=True,
		timeout=120
	)
	assert result.returncode != 0

def __run_example(project, *arguments):
	result = subprocess.run(
		[
			sys.executable, str(EXAMPLE),
			'-i', 'Class2D/particles.star',
			'-b', '8',
			'--validation', '0.25',
			'--device', 'cpu',
			'-j', '2',
			*arguments
		],
		cwd=project, env=os.environ, check=True, capture_output=True,
		text=True, timeout=600
	)
	epochs = []
	for line in result.stdout.splitlines():
		match = EPOCH.match(line)
		assert match, line
		epochs.append(
			(int(match[1]), float(match[2]), float(match[3]))
		)
	return epochs

# A bright disc, or a bright ring around a dark centre.
def __particle(kind, generator):
	y, x = np.mgrid[:BOX_SIZE, :BOX_SIZE] - (BOX_SIZE - 1) / 2
	radius = np.hypot(y, x)
	image = radius < 4 if kind == 0 else (radius > 4) & (radius < 7)
	noise = generator.normal(0, 0.1, image.shape)
	return (image + noise).astype(np.float32)

@pytest.fixture
def __project(tmp_path):
	generator = np.random.default_rng(0)
	kinds = np.arange(PARTICLES) % 2
	stack = np.stack([__particle(kind, generator) for kind in kinds])
	(tmp_path / 'Extract').mkdir()
	vio.write_stack(stack, tmp_path / 'Extract' / 'particles.mrcs')

	# RELION counts classes from one, and a class may be left empty.
	particles = pd.DataFrame({
		'rlnImageName': [
			f'{index + 1:06d}@Extract/particles.mrcs'
			for index in range(PARTICLES)
		],
		'rlnClassNumber': np.where(kinds == 0, 3, 7),
		'rlnOpticsGroup': 1,
	})
	optics = pd.DataFrame({
		'rlnOpticsGroup': [1],
		'rlnImagePixelSize': [1.5],
		'rlnImageSize': [BOX_SIZE],
		'rlnImageDimensionality': [2],
	})
	(tmp_path / 'Class2D').mkdir()
	starfile.write(
		{'optics': optics, 'particles': particles},
		tmp_path / 'Class2D' / 'particles.star'
	)
	return tmp_path
