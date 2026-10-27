# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs the harness of the benchmarks on tiny datasets, through the command a
# campaign runs, and checks that its records follow their schema and that
# every reader delivers the same batches. It measures nothing: it keeps the
# benchmarks from breaking unnoticed, as test_examples does for the
# examples. The harness needs psutil, and the baselines mrcfile; without
# them the cases that need them are skipped.

import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

pytest.importorskip('psutil')

BENCHMARKS = Path(__file__).parents[2] / 'benchmarks'
HARNESS = BENCHMARKS / 'python'
sys.path.insert(0, str(HARNESS))

from vitrio_bench import (    # noqa: E402
	datasets, manifests, readers, results, writers,
)

DATASET = ['--dataset.count=120', '--dataset.box=16']
LAYOUTS = ['single-stack', 'per-micrograph:7']

def harness(*arguments, check=True):
	paths = [str(HARNESS), os.environ.get('PYTHONPATH', '')]
	return subprocess.run(
		[sys.executable, '-m', 'vitrio_bench', *arguments],
		env={**os.environ, 'PYTHONPATH': os.pathsep.join(paths)},
		check=check, capture_output=True, text=True, timeout=120
	)

def run(datadir, results_file, *options, check=True):
	return harness(
		'run', 'read_batches', '--datadir', str(datadir),
		'--results', str(results_file), *DATASET, *options, check=check
	)

@pytest.mark.parametrize('dtype', ['float32', 'int16'])
@pytest.mark.parametrize('layout', LAYOUTS)
def test_every_reader_delivers_the_same_batches(tmp_path, layout, dtype):
	dataset = [f'--dataset.layout={layout}', f'--dataset.dtype={dtype}']
	harness('generate', '--datadir', str(tmp_path), *DATASET, *dataset)
	records_file = tmp_path / 'results.jsonl'
	scratchdir = f'--environment.scratchdir={tmp_path / "scratch"}'
	for name in readers.READERS:
		run(
			tmp_path, records_file, *dataset, f'--reader.name={name}',
			'--workload.batch=32', '--workload.epochs=2',
			'--workload.verify=true', scratchdir
		)

	records = results.load(records_file)
	for record in records:
		results.check(record)
	runnable = {
		name for name, reader in readers.READERS.items()
		if not readers.missing(reader)
	}
	succeeded = [record for record in records if record['status'] == 'ok']
	assert {record['reader']['name'] for record in succeeded} == runnable
	assert len({record['metrics']['batch_hash'] for record in succeeded}) == 1
	assert all(record['metrics']['images'] == 240 for record in succeeded)

@pytest.mark.parametrize('reader', ['vitrio-loader', 'vitrio-data'])
def test_every_scratch_delivers_the_same_batches(tmp_path, reader):
	layout = '--dataset.layout=per-micrograph:7'
	harness('generate', '--datadir', str(tmp_path), *DATASET, layout)
	records_file = tmp_path / 'results.jsonl'
	for scratch in ('none', 'host', 'file', 'directory'):
		harness(
			'run', 'epochs', '--datadir', str(tmp_path),
			'--results', str(records_file), *DATASET, layout,
			f'--reader.name={reader}', f'--reader.scratch={scratch}',
			'--reader.run_length=4', '--reader.cache=64',
			'--workload.batch=32', '--workload.verify=true',
			f'--environment.scratchdir={tmp_path / "scratch"}'
		)
	records = results.load(records_file)
	for record in records:
		results.check(record)
	assert all(record['status'] == 'ok' for record in records)
	assert len({record['metrics']['batch_hash'] for record in records}) == 1
	# A scratch is made anew for every run, and removed after.
	assert not list((tmp_path / 'scratch').iterdir())

@pytest.mark.skipif(shutil.which('fio') is None, reason="fio is missing")
def test_the_ceiling_is_measured(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	harness(
		'run', 'ceiling', '--datadir', str(tmp_path),
		'--results', str(records_file), *DATASET, '--reader.runtime_s=1',
		'--reader.depths=[1]', '--reader.ioengine=psync'
	)
	(record,) = results.load(records_file)
	results.check(record)
	assert record['status'] == 'ok', record['reason']
	assert record['metrics']['random_qd1_iops'] > 0

def memory_limits_work():
	if shutil.which('systemd-run') is None:
		return False
	completed = subprocess.run(
		['systemd-run', '--user', '--scope', '--quiet', '-p', 'MemoryMax=1G',
		'true'],
		capture_output=True, timeout=60
	)
	return completed.returncode == 0

@pytest.mark.skipif(
	not memory_limits_work(), reason="systemd cannot limit memory here"
)
def test_a_memory_limit_applies_and_what_it_kills_is_recorded(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	run(tmp_path, records_file, '--environment.memory_max=500M')
	completed = run(
		tmp_path, records_file, '--environment.memory_max=20M', check=False
	)
	assert completed.returncode == 1
	held, killed = results.load(records_file)
	for record in (held, killed):
		results.check(record)
	assert held['status'] == 'ok', held['reason']
	assert held['environment']['memory_limit_bytes'] == 500 << 20
	assert held['metrics']['cgroup_memory_peak_bytes'] <= 500 << 20
	assert killed['status'] == 'failed'
	assert '20M' in killed['reason']

@pytest.mark.skipif(
	not hasattr(os, 'posix_fadvise'), reason="this system has no fadvise"
)
def test_a_cold_run_records_what_was_left_cached(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	run(tmp_path, records_file, '--environment.cold=fadvise')
	(record,) = results.load(records_file)
	assert record['status'] == 'ok'
	residency = record['environment']['residency']
	assert residency is None or 0 <= residency <= 1

def test_a_parameter_a_reader_lacks_fails_the_run(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	completed = run(tmp_path, records_file, '--reader.cahce=4', check=False)
	assert completed.returncode == 1
	(record,) = results.load(records_file)
	results.check(record)
	assert record['status'] == 'failed'
	assert 'cahce' in record['reason']

def test_a_dataset_not_generated_fails_the_run(tmp_path):
	records_file = tmp_path / 'results.jsonl'
	completed = run(tmp_path, records_file, check=False)
	assert completed.returncode == 1
	(record,) = results.load(records_file)
	assert record['status'] == 'failed'
	assert 'generate' in record['reason']

def find_reframe():
	beside = Path(sys.executable).parent / 'reframe'
	return str(beside) if beside.exists() else shutil.which('reframe')

def list_reframe_tests(tmp_path, *options):
	reframe = find_reframe()
	if reframe is None:
		pytest.skip("ReFrame is not installed")
	completed = subprocess.run(
		[
			reframe, '-C', str(BENCHMARKS / 'reframe/config/workstation.py'),
			'-c', str(BENCHMARKS / 'reframe'), '-l', *options,
		],
		cwd=tmp_path, check=True, capture_output=True, text=True,
		timeout=300
	)
	# ReFrame skips a test file it cannot load with a warning, not an error.
	assert 'skipping test file' not in completed.stdout
	assert 'Found 0 check(s)' not in completed.stdout
	return completed.stdout

@pytest.mark.skipif(sys.platform == 'win32', reason="ReFrame needs POSIX")
def test_the_reframe_tests_load(tmp_path):
	list_reframe_tests(tmp_path)

@pytest.mark.skipif(sys.platform == 'win32', reason="ReFrame needs POSIX")
def test_every_reframe_parameter_takes_values_from_the_command_line(tmp_path):
	# -P refuses a parameter that does not state its type, which a named
	# sweep that redefines one can forget.
	listed = list_reframe_tests(tmp_path)
	parameters = {
		word.removeprefix('%')
		for line in listed.splitlines() if line.startswith('- ')
		for word in line.split() if word.startswith('%')
	}
	assert parameters
	options = [
		option for parameter in sorted(parameters)
		for option in ('-P', parameter)
	]
	list_reframe_tests(tmp_path, *options)

MICROGRAPHS = [
	'--dataset.count=3', '--dataset.box=128', '--dataset.layout=one-per-file',
]

@pytest.mark.parametrize('crowding, spread', [
	('sparse', ['--workload.batch=8']),
	('crowded', []),
])
def test_every_patch_reader_crops_the_same_patches(tmp_path, crowding, spread):
	harness('generate', '--datadir', str(tmp_path), *MICROGRAPHS)
	records_file = tmp_path / 'results.jsonl'
	for name, reader in readers.READERS.items():
		if not reader.reads_patches:
			continue
		harness(
			'run', 'patches', '--datadir', str(tmp_path),
			'--results', str(records_file), *MICROGRAPHS,
			f'--reader.name={name}', '--workload.patch=32',
			'--workload.patches=10', f'--workload.crowding={crowding}',
			*spread, '--workload.verify=true'
		)
	records = results.load(records_file)
	for record in records:
		results.check(record)
	succeeded = [record for record in records if record['status'] == 'ok']
	assert len(succeeded) >= 2
	assert len({record['metrics']['batch_hash'] for record in succeeded}) == 1
	assert all(record['metrics']['images'] == 30 for record in succeeded)

def test_crowded_patches_read_an_image_a_batch(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *MICROGRAPHS)
	records_file = tmp_path / 'results.jsonl'
	harness(
		'run', 'patches', '--datadir', str(tmp_path),
		'--results', str(records_file), *MICROGRAPHS,
		'--reader.name=vitrio-loader', '--workload.patch=32',
		'--workload.crowding=crowded'
	)
	(record,) = results.load(records_file)
	assert record['status'] == 'ok'
	workload = record['workload']
	# As many patches as cover 40% of an image, in batches of an image.
	assert workload['patches'] == round(0.4 * 128**2 / 32**2)
	assert workload['batch'] == workload['patches']
	assert workload['order'] == 'sequential'
	assert record['metrics']['images'] == 3 * workload['patches']

def test_a_crowded_workload_takes_no_order(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *MICROGRAPHS)
	records_file = tmp_path / 'results.jsonl'
	completed = harness(
		'run', 'patches', '--datadir', str(tmp_path),
		'--results', str(records_file), *MICROGRAPHS,
		'--reader.name=vitrio-loader', '--workload.crowding=crowded',
		'--workload.order=shuffled', check=False
	)
	assert completed.returncode == 1
	(record,) = results.load(records_file)
	results.check(record)
	assert record['status'] == 'failed'
	assert 'crowded' in record['reason']

VOLUME = [
	'--dataset.count=24', '--dataset.box=20', '--dataset.layout=single-stack',
]

@pytest.mark.parametrize('axis', ['z', 'y', 'x'])
def test_every_slice_reader_reads_the_same_slices(tmp_path, axis):
	harness('generate', '--datadir', str(tmp_path), *VOLUME)
	records_file = tmp_path / 'results.jsonl'
	for name, reader in readers.READERS.items():
		if not reader.reads_slices:
			continue
		harness(
			'run', 'slices', '--datadir', str(tmp_path),
			'--results', str(records_file), *VOLUME,
			f'--reader.name={name}', f'--workload.axis={axis}',
			'--workload.batch=7', '--workload.verify=true'
		)
	records = results.load(records_file)
	for record in records:
		results.check(record)
	succeeded = [record for record in records if record['status'] == 'ok']
	assert len(succeeded) >= 2
	assert len({record['metrics']['batch_hash'] for record in succeeded}) == 1
	slices = 24 if axis == 'z' else 20
	assert all(record['metrics']['images'] == slices for record in succeeded)

@pytest.mark.parametrize(
	'dataset',
	[
		['--dataset.count=40', '--dataset.layout=single-stack'],
		['--dataset.count=40', '--dataset.layout=per-micrograph:7'],
		[
			'--dataset.count=40', '--dataset.layout=single-stack',
			'--dataset.dtype=float16',
		],
		['--dataset.count=3', '--dataset.layout=one-per-file'],
	]
)
def test_every_writer_writes_a_dataset_and_leaves_nothing(tmp_path, dataset):
	records_file = tmp_path / 'results.jsonl'
	for name in writers.WRITERS:
		harness(
			'run', 'write_batches', '--datadir', str(tmp_path),
			'--results', str(records_file), '--dataset.box=16', *dataset,
			f'--reader.name={name}', '--workload.batch=8'
		)
	records = results.load(records_file)
	for record in records:
		results.check(record)
	succeeded = [record for record in records if record['status'] == 'ok']
	assert succeeded
	count = int(dataset[0].partition('=')[2])
	assert all(record['metrics']['images'] == count for record in succeeded)
	assert not any(tmp_path.glob('writes-*'))

def test_a_scratch_on_the_storage_of_its_data_is_flagged(tmp_path):
	datadir = tmp_path / 'data'
	harness('generate', '--datadir', str(datadir), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	completed = run(
		datadir, records_file, '--reader.scratch=file',
		f'--environment.scratchdir={tmp_path / "scratch"}'
	)
	[record] = results.load(records_file)
	assert record['status'] == 'ok'
	assert record['environment']['scratch_target']['shares_storage'] is True
	assert 'warning: the scratch directory' in completed.stderr

def write_star(path, names):
	starfile = pytest.importorskip('starfile')
	import pandas as pd

	starfile.write(
		{'particles': pd.DataFrame({'rlnImageName': names})}, path,
		overwrite=True
	)

def test_a_dataset_replays_the_layout_of_a_star_file(tmp_path):
	pytest.importorskip('starfile')
	star = tmp_path / 'particles.star'
	names = [f'{i:06d}@a.mrcs' for i in (1, 2, 3)]
	names += [f'{i:06d}@b.mrcs' for i in (1, 2)]
	write_star(star, names)
	like = [f'--dataset.like={star}', '--dataset.box=16']
	harness('generate', '--datadir', str(tmp_path), *like)
	spec = datasets.make_spec({'like': str(star), 'box': 16})
	assert datasets.load(tmp_path, spec).sizes == [3, 2]

def test_a_manifest_tells_a_whole_dataset_from_an_incomplete_one(tmp_path):
	pytest.importorskip('starfile')
	for name, count in (('a.mrcs', 3), ('b.mrcs', 2)):
		vio.write_stack(np.zeros((count, 16, 16), np.float32), tmp_path / name)
	names = [f'{i:06d}@a.mrcs' for i in (1, 2, 3)]
	names += [f'{i:06d}@b.mrcs' for i in (1, 2)]
	write_star(tmp_path / 'particles.star', names)
	manifest = {
		'files': [{
			'path': 'a.mrcs',
			'bytes': (tmp_path / 'a.mrcs').stat().st_size,
			'stack': {'count': 3, 'box': 16, 'dtype': 'float32'},
		}],
		'particles': {
			'star': 'particles.star', 'count': 5, 'stacks': 2, 'box': 16,
			'dtype': 'float32',
		},
	}
	assert manifests.check(manifest, tmp_path) == []
	(tmp_path / 'b.mrcs').unlink()
	assert any('b.mrcs' in problem for problem in manifests.check(
		manifest, tmp_path
	))

def test_every_manifest_says_what_to_download_and_check():
	for name in manifests.names():
		manifest = manifests.load(name)
		assert {'name', 'title', 'page', 'download'} <= set(manifest)
		assert manifest.get('files') or manifest.get('particles')
