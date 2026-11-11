# Building

How to configure, build and test the library, the Python package, the examples, the benchmarks and the documentation.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

That fetches the dependencies and builds them with the project, which takes
CMake 3.24. With it the presets do the same: `cmake --preset debug`,
`cmake --build --preset debug`, `ctest --preset debug`. There are `release` and
`sanitize` and `sanitize-thread` presets too. CMake 3.18 is the minimum for
a build that finds its dependencies installed, and one CI job builds with
exactly that version. Under it CTest runs each suite as a single test, because
listing the cases needs the JSON support of CMake 3.19.

The Python package is built with `-DVITRIO_BUILD_PYTHON=ON`, which needs
Python 3.10 or newer and nanobind in it (`pip install nanobind`). CMake asks
that interpreter where nanobind is; `-DPython_EXECUTABLE=<python>` chooses
it. The package is assembled under `python/` of the build tree, the
extension and its stubs beside the Python sources, and CTest runs its suite
as one test. To run pytest by hand:

```
PYTHONPATH=build/python python -m pytest tests/python
```

The `dev` group of `pyproject.toml` pins what CI installs for it
(`pip install --group dev`). The cases that exchange arrays with PyTorch,
JAX and rexlib are skipped where those are not installed, and so is the
test of an example where what the example needs is not. A new example is listed
in `examples/README.md` and, if it is C++, in `examples/cpp/CMakeLists.txt`.

`pip install .` builds the same package through scikit-build-core, which
configures this project as it is. The dependencies are fetched and built
with it, as in any other build, so a compiler is all it takes. A plain
`cmake --install` leaves the package out. It is the component
`vitrio_python`, which goes into the directory Python looks for packages
in, with the library beside the extension.

The wheels are built by cibuildwheel from the source distribution, as pip
builds it. The dependencies are built static, inside the library of vitrio,
so a wheel holds everything it needs. A wheel is built against the stable ABI
of Python, and serves 3.12 and every later version; 3.10 and 3.11 get one
each.

`-DVITRIO_BUILD_EXAMPLES=ON` builds the C++ examples of `examples/cpp`, each a
target of its own and a test that runs it. They are off by default.

The harness of the benchmarks is not built. It runs from the source tree
beside the package of a build, `PYTHONPATH=build/python:benchmarks/python
python -m vitrio_bench`, and ReFrame runs campaigns of it, as
`benchmarks/README.md` says. What it needs is the `bench` group of
`pyproject.toml`, which `dev` includes, so that the suite of the Python
package runs its smoke test. `-DVITRIO_BUILD_BENCHMARKS=ON` builds the C++
benchmarks of `benchmarks/cpp`, with Google Benchmark, and adds a test that
runs each for a moment, labelled `benchmark`. CI builds them in its main
job.

`-DVITRIO_BUILD_DOCS=ON` adds the target `vitrio-docs`, which needs
Doxygen and writes the documentation of the C++ API under
`docs/doxygen/html` of the build tree. A warning of Doxygen fails it, so a comment that names a
parameter that is not there does not go unnoticed. Its first page is
`docs/doxygen/mainpage.md`, not the README, whose `<picture>` Doxygen fails
on. A link in that page to anything outside the reference is a full URL: a
relative one is taken for a reference to a page of the documentation, and
fails it. PyPI shows the README, so a link or an image in it to another
file of the repository is a full URL too, such as its banners in
`docs/logo/` on `main`.

With `VITRIO_BUILD_PYTHON` the same option adds `vitrio-python-docs`, which
imports the package from the build tree and has Sphinx write its reference
under `docs/sphinx/html`. It needs Sphinx, Furo and numpy (`pip install --group
docs`), and a warning of Sphinx fails it too. The reference is built from
docstrings and holds none of its own text beyond the introduction and the
list of what each page documents, so a new name is added to a page of
`docs/sphinx/`. Both references are published together, under `/cpp/` and
`/python/` of the site, which has the contents of `docs/site/` at its root.

The sanitizer presets leave the Python package out. Its stubs are written
by importing the extension, and an interpreter that is not instrumented
itself only loads an instrumented extension with the runtime of the
sanitizer preloaded.

vitrio is a shared library only. Its image formats register themselves
through objects at namespace scope, with the macros of
`src/formats/image_file_format_registration_macros.hpp`, and a static archive would
drop the object files nothing else refers to.
