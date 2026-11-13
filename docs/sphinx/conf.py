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
# Where the reference is published, which the theme writes into the
# canonical link of every page, so that a search engine keeps one copy of it
# however it was reached.
html_baseurl = 'https://gigabit-clowns.github.io/vitrio/python/'
# The lockup of the logo holds the name, so the sidebar does not repeat it.
# The files are those of `docs/logo/`, which the build copies.
html_static_path = ['../logo/lockup']
html_favicon = '../logo/favicon/favicon.svg'
html_theme_options = {
	'light_logo': 'vitrio-logo-horizontal.svg',
	'dark_logo': 'vitrio-logo-horizontal-on-dark.svg',
	'sidebar_hide_name': True,
}

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
