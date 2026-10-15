# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs every example under examples/python that needs no input. They make
# their own images in a temporary directory and assert what they show, so
# that running one to the end is the test. The two that read a RELION
# project, and those that take arguments, have a file of their own.

import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

EXAMPLES = Path(__file__).parents[2] / 'examples' / 'python'
TESTED_APART = {
	'train_2d_classifier.py', 'extract_particles.py', 'convert.py',
	'fractionate_eer.py',
}
# The examples that write a TIFF file, which a build may leave out.
NEEDS_TIFF = {'write.py'}

SELF_CONTAINED = sorted(
	path for path in EXAMPLES.rglob('*.py') if path.name not in TESTED_APART
)

def test_there_are_examples_to_run():
	assert SELF_CONTAINED

@pytest.mark.parametrize(
	'example', SELF_CONTAINED, ids=lambda path: path.name
)
def test_runs_to_the_end(example, tmp_path):
	if example.name in NEEDS_TIFF:
		try:
			vio.write_single(np.zeros((2, 2), np.uint8), tmp_path / 't.tif')
		except Exception:
			pytest.skip('this build writes no TIFF files')
	subprocess.run(
		[sys.executable, str(example)],
		cwd=tmp_path, env=os.environ, check=True, capture_output=True,
		text=True, timeout=120
	)
