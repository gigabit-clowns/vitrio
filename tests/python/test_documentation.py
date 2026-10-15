# SPDX-License-Identifier: LGPL-2.1-or-later

# Every name the package exports has a docstring, and so has every public
# method and property of its classes, so that the reference built from them
# has no gap and a new name cannot arrive without one.
#
# What the extension binds is read from its stubs, because nanobind puts the
# signature of a function in its `__doc__` and an undocumented one would
# still look documented. What is written in Python is read from the objects.

import ast
import inspect
from pathlib import Path

import pytest

import vitrio
import vitrio.data
from vitrio import _binding

STUBS = Path(_binding.__file__).with_name('_binding.pyi')

def __has_docstring(node):
	return ast.get_docstring(node) is not None

def __undocumented_in_stubs():
	tree = ast.parse(STUBS.read_text())
	missing = []
	for node in tree.body:
		if isinstance(node, ast.FunctionDef):
			if not __has_docstring(node):
				missing.append(node.name)
		elif isinstance(node, ast.ClassDef):
			if node.name not in vitrio.__all__:
				continue
			if not __has_docstring(node):
				missing.append(node.name)
			for member in node.body:
				if (isinstance(member, ast.FunctionDef)
						and not member.name.startswith('_')
						and not __has_docstring(member)):
					missing.append(f'{node.name}.{member.name}')
	return missing

def __undocumented_in_python(module, names):
	missing = []
	for name in names:
		member = getattr(module, name)
		if getattr(member, '__module__', '').startswith('vitrio._binding'):
			continue
		if isinstance(member, str):
			continue
		if not inspect.getdoc(member):
			missing.append(f'{module.__name__}.{name}')
		if inspect.isclass(member):
			for key, value in vars(member).items():
				if key.startswith('_') or not (
					callable(value) or isinstance(value, property)
				):
					continue
				if not inspect.getdoc(value):
					missing.append(f'{module.__name__}.{name}.{key}')
	return missing

def test_everything_exported_is_exported_by_name():
	assert 'read' in vitrio.__all__
	assert 'Array' in vitrio.__all__
	for name in vitrio.__all__:
		assert hasattr(vitrio, name), name

def test_the_binding_documents_what_the_package_exports():
	if not STUBS.exists():
		pytest.skip('this build wrote no stubs')
	assert __undocumented_in_stubs() == []

def test_what_is_written_in_python_is_documented():
	assert __undocumented_in_python(vitrio, vitrio.__all__) == []
	assert __undocumented_in_python(vitrio.data, vitrio.data.__all__) == []
