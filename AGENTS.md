# Working on vitrio

vitrio is a high-throughput image I/O library tailored to the access patterns
and file formats common in Cryo-Electron Microscopy (CryoEM). It is a C++
library with a Python package over it, and it depends on nothing of rexlib,
where its code comes from: it was ported from the image subsystem of rexlib
at `7bf00d69`.

This file is a map. The rules themselves are in `docs/agents/`, one theme a
file. Keep them true. When a change makes something in this file or in
`docs/agents/` wrong or missing, such as a moved directory, a new convention,
a dependency or a workflow, update it in the same commit that causes it.

## Build and test

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

Add `-DVITRIO_BUILD_PYTHON=ON` for the Python package, and run its suite with
`PYTHONPATH=build/python python -m pytest tests/python`.

## Always

- C++14 in the library and its public headers ([C++](docs/agents/cpp-conventions.md)).
- Every source file starts with `SPDX-License-Identifier: LGPL-2.1-or-later`.
- Tabs, 80 columns, formatted by hand; Python is indented with tabs too.
- One class per file: declared in a `.hpp`, defined in a `.cpp` or an `.inl`.
- A header or source with a function body has a test file at the same
  relative path under `tests/unitary/src/` ([layout](docs/agents/layout.md)).
- A new source is named in its directory's `CMakeLists.txt`; nothing is
  globbed but the header check and pytest ([layout](docs/agents/layout.md)).
- Test doubles are the trompeloeil mocks of `tests/unitary/src/mock/`, never
  hand-written fakes ([tests](docs/agents/tests.md)).
- A public symbol is marked `VITRIO_API`; a header under `src/` is private.
- Whoever opens a pull request assigns it to themselves and labels it
  ([CI](docs/agents/ci.md)).

## Index

| Doc | Read it before |
|---|---|
| [layout.md](docs/agents/layout.md) | Looking for where something lives, or adding a directory or a source |
| [building.md](docs/agents/building.md) | Building anything beyond the commands above: presets, the Python package, wheels, examples, benchmarks, the documentation |
| [dependencies.md](docs/agents/dependencies.md) | Adding, bumping, finding or fetching a dependency |
| [cpp-conventions.md](docs/agents/cpp-conventions.md) | Writing or reviewing any C++ of the library |
| [transfers.md](docs/agents/transfers.md) | Touching readers, writers, flushes, the scratch, executors or `file_transfer_pool` |
| [formats.md](docs/agents/formats.md) | Touching `src/formats/`, providers, selectors, detector events, ZARR or gzip |
| [tests.md](docs/agents/tests.md) | Writing a test that writes files, needs a mock or waits for a transfer |
| [python.md](docs/agents/python.md) | Touching `python/`, the binding, `vitrio.data`, docstrings or the Python tests |
| [benchmarks.md](docs/agents/benchmarks.md) | Touching `benchmarks/` |
| [ci.md](docs/agents/ci.md) | Opening a pull request, or touching `.github/workflows/` or a release |
