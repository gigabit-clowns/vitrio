# SPDX-License-Identifier: LGPL-2.1-or-later

# The reference of the Python package, built from the docstrings of the
# package that is imported, which is the one under `python/` of the build
# tree. CMake puts it on the path.

import vitrio

project = 'vitrio'
author = 'Gigabit Clowns'
release = vitrio.__version__
version = release

extensions = [
	'sphinx.ext.autodoc',
	'sphinx.ext.napoleon',
]

# The docstrings are written in the style of Google, with `Args:`,
# `Returns:` and `Raises:`. nanobind puts the signature of what it binds on
# the first line of its docstring, which autodoc takes out and shows as the
# signature.
napoleon_google_docstring = True
napoleon_numpy_docstring = False
autodoc_docstring_signature = True
# What the constructor takes is in the docstring of `__init__`.
autoclass_content = 'both'
autodoc_member_order = 'bysource'
autodoc_typehints = 'none'

html_theme = 'furo'
html_title = f"vitrio {release}"
html_static_path = []

# A reference that cannot be resolved is a mistake in a docstring.
nitpicky = False

# nanobind writes `self` in the signature of the constructor it takes the
# signature of a class from.
def strip_self(app, what, name, obj, options, signature, return_annotation):
	if what == 'class' and signature:
		signature = signature.replace('(self, ', '(', 1)
		signature = signature.replace('(self)', '()', 1)
	return signature, return_annotation

def setup(app):
	app.connect('autodoc-process-signature', strip_self)
