# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs examples/extract_particles.py on a few small micrographs, laid out
# as RELION lays a project out, and checks the particles against the boxes
# numpy cuts out of the same micrographs.

import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

# What the example needs, besides what this file uses of it.
starfile = pytest.importorskip('starfile')
pd = pytest.importorskip('pandas')
pytest.importorskip('tqdm')

EXAMPLE = Path(__file__).parents[2] / 'examples' / 'extract_particles.py'
BOX_SIZE = 8

# The first is a box that lies inside its micrograph. The second reaches
# past the corner at the origin, and the third past the opposite one.
COORDINATES = {
	'Micrographs/first.mrc': [(20, 12), (2, 3)],
	'Micrographs/second.mrc': [(30, 16), (46, 30), (10, 10)],
}

def test_extracts_every_particle_into_one_stack(__project):
	__run_example(__project)
	descriptor = vio.query_descriptor(__project / 'particles.mrcs')
	assert descriptor.extents == (5, BOX_SIZE, BOX_SIZE)
	assert descriptor.core_rank == 2
	assert descriptor.data_type == vio.NumericalType.float32

def test_a_particle_is_the_box_around_its_coordinate(__project):
	__run_example(__project)
	particles = __read_particles(__project)
	for row in particles.itertuples():
		expected = __cut_box(
			__project / row.rlnMicrographName,
			row.rlnCoordinateX, row.rlnCoordinateY
		)
		assert np.array_equal(__read_image(__project, row), expected)

def test_a_box_past_the_edge_is_padded_with_zeros(__project):
	__run_example(__project)
	particles = __read_particles(__project)
	corner = __read_image(__project, particles.iloc[1])
	assert not corner[:1, :].any()
	assert not corner[:, :2].any()
	assert corner[1:, 2:].all()

def test_writes_a_stack_for_each_micrograph_when_asked_to(__project):
	__run_example(__project, '--stack-dir', 'Particles')
	first = vio.query_descriptor(__project / 'Particles' / 'first.mrcs')
	second = vio.query_descriptor(__project / 'Particles' / 'second.mrcs')
	assert first.extents == (2, BOX_SIZE, BOX_SIZE)
	assert second.extents == (3, BOX_SIZE, BOX_SIZE)

def test_the_stacks_hold_the_same_particles_however_they_are_split(
	__project
):
	__run_example(__project)
	together = [
		__read_image(__project, row)
		for row in __read_particles(__project).itertuples()
	]
	__run_example(__project, '--stack-dir', 'Particles')
	apart = [
		__read_image(__project, row)
		for row in __read_particles(__project).itertuples()
	]
	assert all(np.array_equal(a, b) for a, b in zip(together, apart))

def test_writes_the_stack_it_is_given(__project):
	__run_example(__project, '--stack-file', 'Stacks/all.mrcs')
	descriptor = vio.query_descriptor(__project / 'Stacks' / 'all.mrcs')
	assert descriptor.extents == (5, BOX_SIZE, BOX_SIZE)

def test_stores_the_data_type_it_is_asked_for(__project):
	__run_example(__project, '--data-type', 'int16')
	descriptor = vio.query_descriptor(__project / 'particles.mrcs')
	assert descriptor.data_type == vio.NumericalType.int16

def test_a_particle_carries_what_its_micrograph_does(__project):
	__run_example(__project)
	particles = __read_particles(__project)
	assert list(particles.rlnMicrographName) == [
		'Micrographs/first.mrc', 'Micrographs/first.mrc',
		'Micrographs/second.mrc', 'Micrographs/second.mrc',
		'Micrographs/second.mrc',
	]
	assert list(particles.rlnOpticsGroup) == [1, 1, 1, 1, 1]
	assert list(particles.rlnImageName) == [
		'1@particles.mrcs', '2@particles.mrcs', '3@particles.mrcs',
		'4@particles.mrcs', '5@particles.mrcs',
	]

def test_the_optics_describe_the_particles(__project):
	__run_example(__project)
	optics = starfile.read(
		__project / 'particles.star', always_dict=True
	)['optics']
	assert optics.rlnImageSize.iloc[0] == BOX_SIZE
	assert optics.rlnImageDimensionality.iloc[0] == 2
	assert optics.rlnImagePixelSize.iloc[0] == pytest.approx(1.5)

def test_both_ways_of_naming_the_stacks_are_not_taken_together(__project):
	result = __run_example(
		__project, '--stack-file', 'all.mrcs', '--stack-dir', 'Particles',
		check=False
	)
	assert result.returncode != 0

def __run_example(project, *arguments, check=True):
	return subprocess.run(
		[
			sys.executable, str(EXAMPLE),
			'-m', 'micrographs.star',
			'-c', 'coordinate_files.star',
			'-o', 'particles.star',
			'-b', str(BOX_SIZE),
			*arguments
		],
		cwd=project, env=os.environ, check=check, capture_output=True,
		text=True, timeout=120
	)

def __read_particles(project):
	return starfile.read(
		project / 'particles.star', always_dict=True
	)['particles']

def __read_image(project, particle):
	location = vio.ImageLocation.from_string(particle.rlnImageName)
	located = vio.ImageLocation(
		project / location.key, location.index_in_stack
	)
	return np.asarray(vio.read(located))

# The box RELION cuts: the coordinate lands at half the side of the box,
# and what lies outside the micrograph is zero here.
def __cut_box(micrograph, x, y):
	image = np.asarray(vio.read(micrograph)).reshape(32, 48)
	padded = np.pad(image, BOX_SIZE)
	top = int(y) + BOX_SIZE - BOX_SIZE // 2
	left = int(x) + BOX_SIZE - BOX_SIZE // 2
	return padded[top:top + BOX_SIZE, left:left + BOX_SIZE]

def __setup_micrograph(seed):
	generator = np.random.default_rng(seed)
	values = generator.integers(1, 100, size=(32, 48))
	return values.astype(np.float32)

# One micrograph is an image, and the other a volume of a single section,
# which is how the micrographs of a microscope often come.
@pytest.fixture
def __project(tmp_path):
	(tmp_path / 'Micrographs').mkdir()
	(tmp_path / 'Coordinates').mkdir()

	vio.write_single(
		__setup_micrograph(1), tmp_path / 'Micrographs' / 'first.mrc'
	)
	vio.write_single(
		__setup_micrograph(2).reshape(1, 32, 48),
		tmp_path / 'Micrographs' / 'second.mrc'
	)

	coordinate_files = []
	for micrograph, coordinates in COORDINATES.items():
		file = f'Coordinates/{Path(micrograph).stem}.star'
		picked = pd.DataFrame(
			coordinates, columns=['rlnCoordinateX', 'rlnCoordinateY']
		)
		starfile.write(picked, tmp_path / file)
		coordinate_files.append((micrograph, file))

	starfile.write(
		{
			'coordinate_files': pd.DataFrame(
				coordinate_files,
				columns=['rlnMicrographName', 'rlnMicrographCoordinates']
			)
		},
		tmp_path / 'coordinate_files.star'
	)
	starfile.write(
		{
			'optics': pd.DataFrame({
				'rlnOpticsGroupName': ['opticsGroup1'],
				'rlnOpticsGroup': [1],
				'rlnMicrographPixelSize': [1.5],
			}),
			'micrographs': pd.DataFrame({
				'rlnMicrographName': list(COORDINATES),
				'rlnOpticsGroup': [1, 1],
			}),
		},
		tmp_path / 'micrographs.star'
	)
	return tmp_path
