# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs the harness of the benchmarks on tiny datasets, through the command a
# campaign runs, and checks that its records follow their schema and that
# every reader delivers the same batches. It measures nothing: it keeps the
# benchmarks from breaking unnoticed, as test_examples does for the
# examples. The harness needs psutil, and the baselines mrcfile; without
# them the cases that need them are skipped.

import ast
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
	cache, datasets, environment, evict, manifests, readers, results, writers,
	zarr_libraries,
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
		'--reader.depths=[1]', '--reader.ioengine=psync',
		'--reader.write_bytes=1048576'
	)
	(record,) = results.load(records_file)
	results.check(record)
	assert record['status'] == 'ok', record['reason']
	assert record['metrics']['random_qd1_iops'] > 0
	assert record['metrics']['sequential_write_bytes_per_s'] > 0
	assert not list(tmp_path.glob('**/ceiling-write-*'))

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
	arc_bytes = record['environment']['arc_bytes']
	assert arc_bytes is None or arc_bytes >= 0

def test_the_evict_helper_stands_alone():
	# A copy owned by root runs it, so it imports nothing of the package.
	source = Path(evict.__file__)
	tree = ast.parse(source.read_text(encoding='utf-8'))
	imported = {
		name.name.split('.')[0]
		for node in ast.walk(tree) if isinstance(node, ast.Import)
		for name in node.names
	} | {
		(node.module or '').split('.')[0]
		for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
	}
	assert all(node.level == 0 for node in ast.walk(tree)
		if isinstance(node, ast.ImportFrom))
	assert 'vitrio_bench' not in imported and 'vitrio' not in imported
	assert imported <= set(sys.stdlib_module_names)
	completed = subprocess.run(
		[sys.executable, '-I', str(source), '--help'],
		env={}, check=True, capture_output=True, text=True, timeout=60
	)
	assert 'arc-max' in completed.stdout
	with pytest.raises(evict.Error, match='no command'):
		evict.run(['arc-max'])
	with pytest.raises(evict.Error, match='numbers of bytes'):
		evict.run(['arc-max', 'much'])

@pytest.mark.skipif(
	hasattr(os, 'geteuid') and os.geteuid() == 0, reason="root needs no helper"
)
def test_a_privileged_mode_without_the_helper_says_so(monkeypatch):
	monkeypatch.setattr(cache, '_find_helper', lambda: None)
	with pytest.raises(environment.Unavailable, match=cache.HELPER):
		cache.evict([], 'drop')
	with pytest.raises(environment.Unavailable, match=cache.HELPER):
		cache.evict([], 'zfs')

@pytest.mark.parametrize('platform', ['darwin', 'win32', 'sunos5'])
def test_what_a_platform_lacks_is_said_not_skipped(monkeypatch, platform):
	# A platform that cannot do something says what would, and never does
	# nothing in silence.
	monkeypatch.setattr(sys, 'platform', platform)
	with pytest.raises(evict.Error):
		evict.system().arc_limits()
	if platform != 'darwin':
		with pytest.raises(evict.Error):
			evict.system().drop_caches(3)
	else:
		with pytest.raises(environment.Unavailable, match='purge'):
			cache.evict(['anything'], 'fadvise')
	if platform == 'win32':
		monkeypatch.setattr(cache, '_has_privilege', lambda: False)
		with pytest.raises(environment.Unavailable, match='administrator'):
			cache.privileged('drop')

def test_an_arc_cap_needs_a_zfs_target(tmp_path):
	if environment.target(tmp_path).get('filesystem') == 'zfs':
		pytest.skip("the temporary directory is on ZFS")
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	completed = run(
		tmp_path, records_file, '--environment.arc_max=1G', check=False
	)
	assert completed.returncode == 1
	(record,) = results.load(records_file)
	results.check(record)
	assert record['status'] == 'failed'
	assert 'ZFS' in record['reason']
	assert record['environment']['arc_max'] == '1G'

def test_a_run_records_what_tunes_vitrio_from_the_environment(tmp_path):
	harness('generate', '--datadir', str(tmp_path), *DATASET)
	records_file = tmp_path / 'results.jsonl'
	run(tmp_path, records_file, '--workload.verify=true')
	environment_run = subprocess.run(
		[sys.executable, '-m', 'vitrio_bench', 'run', 'read_batches',
		'--datadir', str(tmp_path), '--results', str(records_file),
		*DATASET, '--workload.verify=true'],
		env={**os.environ, 'PYTHONPATH': os.pathsep.join(
			[str(HARNESS), os.environ.get('PYTHONPATH', '')]
		), 'VITRIO_DISABLE_ASYNCHRONOUS_IO': '1'},
		check=True, capture_output=True, text=True, timeout=120
	)
	assert 'status ok' in environment_run.stdout
	first, second = results.load(records_file)
	assert 'VITRIO_DISABLE_ASYNCHRONOUS_IO' not in first['environment']['tuning']
	assert second['environment']['tuning'] == {
		'VITRIO_DISABLE_ASYNCHRONOUS_IO': '1'
	}
	# Read as a process without asynchronous ports does, the batches are
	# the same.
	assert first['metrics']['batch_hash'] == second['metrics']['batch_hash']

@pytest.mark.skipif(sys.platform == 'win32', reason="ReFrame needs POSIX")
def test_every_test_runs_without_asynchronous_transfers(tmp_path):
	listed = list_reframe_tests(tmp_path, '-P', 'transfers=synchronous')
	assert '%transfers=synchronous' in listed

def test_bytes_are_read_as_systemd_states_them():
	assert environment.parse_bytes('8G') == 8 << 30
	assert environment.parse_bytes('512M') == 512 << 20
	assert environment.parse_bytes(' 4096 ') == 4096
	assert environment.parse_bytes(4096) == 4096
	for bad in ('8 GB', '-1', 'much', '1.5G'):
		with pytest.raises(ValueError):
			environment.parse_bytes(bad)

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

def reframe_lists(tmp_path, *options, config='workstation.py', env=None):
	reframe = find_reframe()
	if reframe is None:
		pytest.skip("ReFrame is not installed")
	return subprocess.run(
		[
			reframe, '-C', str(BENCHMARKS / 'reframe/config' / config),
			'-c', str(BENCHMARKS / 'reframe'), '-l', *options,
		],
		cwd=tmp_path, check=False, capture_output=True, text=True,
		timeout=300, env=env
	)

def list_reframe_tests(tmp_path, *options, **how):
	completed = reframe_lists(tmp_path, *options, **how)
	assert completed.returncode == 0, completed.stdout + completed.stderr
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

@pytest.mark.skipif(sys.platform == 'win32', reason="ReFrame needs POSIX")
def test_a_storage_is_a_partition(tmp_path):
	storages = f"a={tmp_path / 'a'},b-side={tmp_path / 'b'}"
	env = {**os.environ, 'VITRIO_BENCH_STORAGES': storages}
	listed = list_reframe_tests(
		tmp_path, '--system', 'workstation:b-side', config='storages.py',
		env=env
	)
	assert 'readers' in listed
	unset = {key: value for key, value in os.environ.items()
		if key != 'VITRIO_BENCH_STORAGES'}
	completed = reframe_lists(tmp_path, config='storages.py', env=unset)
	assert completed.returncode != 0
	assert 'VITRIO_BENCH_STORAGES' in completed.stdout + completed.stderr

VOLUME = ['--dataset.count=20', '--dataset.box=24']

@pytest.mark.parametrize('kind', ['volume', 'stack'])
def test_every_zarr_library_writes_and_reads_the_same_store(tmp_path, kind):
	harness('generate', '--datadir', str(tmp_path), *VOLUME)
	records_file = tmp_path / 'results.jsonl'
	for name in zarr_libraries.LIBRARIES:
		harness(
			'run', 'zarr', '--datadir', str(tmp_path),
			'--results', str(records_file), *VOLUME, f'--reader.name={name}',
			f'--workload.kind={kind}', '--workload.chunk_edge=8',
			'--workload.patch=5', '--workload.patches=7',
			'--workload.repeats=2', '--workload.verify=true'
		)

	records = results.load(records_file)
	for record in records:
		results.check(record)
	runnable = {
		name for name, library in zarr_libraries.LIBRARIES.items()
		if not zarr_libraries.missing(library)
	}
	succeeded = [record for record in records if record['status'] == 'ok']
	assert {record['reader']['name'] for record in succeeded} == runnable
	for metric in ('read_hash', 'patch_hash', 'files'):
		assert len({record['metrics'][metric] for record in succeeded}) == 1
	# What each wrote is removed after it.
	assert not list(tmp_path.glob('zarr-*'))

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
