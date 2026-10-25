# Working on vitrio

vitrio is a high-throughput image I/O library tailored to the access patterns
and file formats common in Cryo-Electron Microscopy (CryoEM). It is a C++
library with a Python package over it, and it depends on nothing of rexlib,
where its code comes from: it was ported from the image subsystem of rexlib
at `7bf00d69`.

Keep this file true. When a change makes something here wrong or missing, such
as a moved directory, a new convention, a dependency or a workflow, update it
in the same commit that causes it.

## Layout

| Path | Holds |
|---|---|
| `include/vitrio/` | The public headers. Everything here is part of the API |
| `include/vitrio/array/` | The array classes, their descriptor and the types of their elements |
| `include/vitrio/concurrency/` | Executors, the tasks they run and the completions that report them |
| `include/vitrio/exceptions/` | The exception types |
| `src/` | The implementation, plus the headers that are not public. Its directories are those of `include/vitrio/` |
| `src/formats/` | The image formats, each in a directory and a namespace of its own, such as `vitrio::mrc`, and in `raw/` what the formats whose values lie in the file as they are share |
| `src/formats/raw/` | What the formats whose values lie in the file as they are share. Such a format resolves its header into a `raw_image_layout`, opens its reader with `open_raw_image_reader`, which picks the reader that suits the system and the values: one that queues its reads through the `file_transfer_pool` it is given where the process can have asynchronous ports, and one over a mapping otherwise. It creates its files with `open_raw_image_writer`, whose writer queues its writes through the pool wherever it runs, since writes the system carries out as they are asked still beat writes through a mapping. A file being written is laid out in full first, by `create_image_file` |
| `src/formats/raw/planning/` | Gathers the regions of a transfer into the stretches of the file they reach, merged across small gaps, or only where they touch for a write, and grouped into steps of a bounded number of bytes, on a grid of pages or of elements. A write covers only the runs of bytes its regions take, which planning works out too |
| `src/formats/raw/queued/` | Reads and writes a file whose values lie as they are through an executor of file transfers. A read or a write is planned on the thread that starts it into an operation: one that moves straight between the file and the array where nothing is converted, and otherwise one that stages its values in memory of its own a step at a time, as the planning groups the regions, and converts each stretch on the executor of computations: as it arrives for a read, and before its runs are written for a write. A flush is an operation that syncs the file, and a writer that is destroyed hands the executor one, which waits for nothing |
| `src/formats/raw/mapped/` | Reads a file through a mapping of it, for the formats whose values lie in the file as they are. A read of values out of memory advises the pages it is about to touch a step ahead, and a scratch reads its copies the same way. Values that are not aligned for their data type are read through a page copied out of the mapping instead |
| `src/formats/dm/` | The DigitalMicrograph format, versions 3 and 4, read only: its header, the tree of tags it is parsed into, how the size of a value follows from its encoding, how the image is found among the tags, and the format that registers itself |
| `src/formats/mrc/` | The MRC format: its header, how a header resolves into the layout of a file, and the two formats that register themselves |
| `src/formats/eer/` | The EER format, a TIFF file whose pages hold the events of a detector: how a frame encodes them, the decoder of one strip, the reader of the events and the format that registers itself. Built only with `VITRIO_ENABLE_TIFF` |
| `src/formats/tiff/` | The TIFF format, over libtiff: the file, the layout of a page, the decoder that holds one page at a time, and the reader and writer. Built only with `VITRIO_ENABLE_TIFF` |
| `src/strided_transfer/` | Moves regions between a file and an array: checks them against both, splits them by page for a file moved a page at a time, resolves where each one starts, works out the order to walk it in, and converts its elements. The formats move their values with it, and so do the rendering of detector events and the scratch |
| `src/array/`, `src/memory/` | Beside the array classes, the private helpers on element types: the C++ type of each, among them the half precision number C++14 lacks, the cast between two of them, their byte order, and the bits of one read as another |
| `src/system/` | What is asked of the operating system. Each has one source file per family of systems, and the build takes the one that applies. Among them the queues of file transfers, which read, write and sync: one through io_uring, built only with liburing, and one that transfers as it is asked. Over them the ports, queues whose wait another thread can end: one through io_uring, woken by a read of an eventfd in the same ring, and one that transfers as it is asked and waits on a condition variable. The openers of ports make one kind each, and the asynchronous one says whether the process can have its kind |
| `src/file_transfer/` | Carries out transfers between files and memory without blocking: a few threads, each waiting on a port of its own for the transfers of many operations at once and for the outcomes of the tasks they run on the executor of computations. An operation is called as what it started completes, and is reported once nothing it started is outstanding. The public `file_transfer_pool` holds such an executor, which its implementation, defined under `src/`, hands to the library |
| `tests/unitary/` | White-box Catch2 suite, built from the objects of the library, which sees `src/` |
| `tests/unitary/src/mock/`, `fixtures/` | The trompeloeil mocks of the interfaces, and what several test files set up alike |
| `tests/assets/` | The files the tests are given, read where they are, and the directory under the build tree the tests write to |
| `tests/assets/include/vitrio/tests/` | What both suites set up alike, written against the public headers only: paths, arrays that hold given values, the plan of a whole region, transfers waited for, and a pool of file transfers for the files a case opens |
| `tests/integration/` | Black-box Catch2 suite, linked with the shared library as a consumer is |
| `tests/headers/` | Compiles each public header on its own under every C++ standard the compiler has |
| `tests/install/` | Installs the library into the build tree, then builds and runs `consumer/`, a project of its own, against what was installed |
| `python/src/` | The extension of the Python package, `vitrio._binding`, written with nanobind. Each public header it binds has a source of the same name here, with one function that adds it to the module |
| `python/src/array/`, `concurrency/` | Beside the bindings of those directories, how an array given from Python becomes one of vitrio: the loan of its memory and the completion that holds the loan |
| `python/vitrio/` | The Python package: what Python expects on top of the binding, such as arguments with defaults |
| `python/vitrio/data/` | `vitrio.data`, which feeds images and their metadata to a model in batches: tables as columns, the sources a field reads from, the dataset, the sampler, the loader, the image loader reads go through when given none, and what hands a batch to PyTorch or JAX |
| `tests/python/` | pytest suite of the Python package, imported from the build tree |
| `examples/python/` | Programs that show the Python package at work, in `basic/` (vitrio assembles the pieces) and `advanced/` (the example names them). An example that needs no arguments makes its own images and asserts what it shows, and `tests/python/test_examples.py` runs it. One that is a tool takes files, and has a test file of its own that runs it on the files of `tests/assets/` or on ones it makes. Add a new one to the list of `TESTED_APART` in that test if it is a tool |
| `examples/cpp/` | The C++ examples, a target each. Built with `-DVITRIO_BUILD_EXAMPLES=ON`, and run by CTest |
| `benchmarks/` | How fast vitrio reads, against the ways other software reads, and the sweeps that tune it. Run by hand, as its README says. CI runs only their smoke tests |
| `benchmarks/cpp/` | The C++ benchmarks, built with `-DVITRIO_BUILD_BENCHMARKS=ON`: that of the engine of `src/strided_transfer/`, white-box, linked with the objects of the library as the unit tests are |
| `benchmarks/python/vitrio_bench/` | The harness: one run of a procedure with a reader, measured from outside vitrio and recorded as one line of JSON. Its command line, `python -m vitrio_bench run`, is what ReFrame and the smoke test launch |
| `benchmarks/reframe/` | The ReFrame tests that run campaigns of the harness: a base test per procedure, whose parameters are what a sweep may vary, its named sweeps, the fixture that builds vitrio for the sweeps of its tuning macros, and example configurations of machines |
| `benchmarks/datasets/` | The manifests of real datasets, in JSON: where to download each, what its files are, how to check them (`python -m vitrio_bench dataset check`) and how to make its variants. The data is never committed |
| `docs/` | Builds the documentation of the API, a directory for each of its parts |
| `docs/site/` | The root of the published site, whose page links both references. It is copied as it is |
| `docs/doxygen/` | The reference of the C++ headers, with Doxygen, and the README as its first page |
| `docs/sphinx/` | The reference of the Python package, with Sphinx, from its docstrings. It holds the `.rst` pages and `conf.py` |
| `cmake/modules/` | CMake modules of the project, among them a `vitrio_fetch_<dep>.cmake` for each dependency, which fetches it and builds it with the project |
| `cmake/config/` | The template of the installed CMake package config |
| `cmake/scripts/` | CMake scripts the build runs |
| `pyproject.toml` | How pip builds the Python package, and the versions of what its builds and tests install |

Every directory holding sources carries a `CMakeLists.txt` naming them. The
lists are explicit rather than globbed, so a new `.cpp` has to be named there
before it is built. That holds for the Python sources of `python/vitrio/`
too. There are two exceptions. The header check globs `include/`, so that
a new header is checked without being listed, and pytest finds the files of
`tests/python/` itself.

Tests mirror what they test: a header or source with a function body has a
test file of its own, at the same relative path under `tests/unitary/src/`.

## Building

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
parameter that is not there does not go unnoticed. The README is its first
page, so a link in it to another file of the repository is a full URL: a
relative one is taken for a reference to a page of the documentation, and
fails it.

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

## Dependencies

Every dependency is fetched and built with vitrio by default, static, and
ends up inside its library. Turning `VITRIO_BUILD_DEPENDENCIES` off finds
them all installed instead, with `find_package`, as a distribution or conda
builds vitrio. Turning off `VITRIO_BUILD_<DEP>` does that for one of them.
`cmake_dependent_option` declares those toggles, so that the global one
forces the others off.

A dependency is a branch where it is used, in `CMakeLists.txt` for the
library and in `tests/CMakeLists.txt` for the tests. Built, it is a call to
`vitrio_fetch_<dep>()` with what pins the release it fetches. Found, it is
the `find_package` call, with the oldest version that works. Each `vitrio_fetch_<dep>()` has a
module of its own in `cmake/modules/`, reads nothing but its arguments, and
sets the options of the project it fetches in its own scope. A new
dependency gets a toggle, a branch, a module and a manager in
`renovate.json`.

Those calls hold the only pins, in forms Renovate keeps up to date. What is
released on GitHub or GitLab is pinned by the commit of its release tag,
`COMMIT <sha>) # <tag>`, as the actions of the workflows are. Boost is
pinned by its tag and the SHA256 of the archive it attaches to the release,
since the archive of a commit lacks its libraries, which are submodules.

| Dependency | Needed by |
|---|---|
| Threads | The library, privately |
| Boost 1.70 or newer: Filesystem, Interprocess, ContainerHash, Optional | The library, privately. Interprocess maps the files |
| spdlog 1.4 or newer | The library, privately and header only, for its log |
| libtiff 4.5 or newer | The library, privately, for the TIFF and EER formats. Not needed with `-DVITRIO_ENABLE_TIFF=OFF` |
| liburing 2.0 or newer | The library, privately and on Linux only, for the queue of file reads through io_uring. Not needed with `-DVITRIO_ENABLE_IO_URING=OFF`. It installs no CMake package, so it is found with pkg-config. It has no CMake build either: built, its own configure script is run into the build tree, which needs a POSIX shell and make |
| zlib | libtiff, for Deflate compression. Fetched before libtiff, it is the one a fetched libtiff finds, which needs CMake 3.24. Not needed with `-DVITRIO_ENABLE_TIFF=OFF` |
| Catch2 3 | The test suites |
| trompeloeil | The unit tests, for their mocks |
| Google Benchmark 1.8 or newer | The C++ benchmarks. Not needed without `-DVITRIO_BUILD_BENCHMARKS=ON`. Unlike the others it is cloned with git, not fetched as an archive, since it reads its version from the tags of its repository |
| Python 3.10 or newer, nanobind 3 | The Python package. Not needed without `-DVITRIO_BUILD_PYTHON=ON`, and never fetched: pip provides nanobind |
| numpy | `vitrio.data`, and the tests of the Python package. Not a dependency of the package: `vitrio.data` reports it missing when imported |
| PyTorch, JAX | Neither is needed. `vitrio.data` imports each when a batch is handed to it, and its tests and the cases that exchange arrays with them are skipped without them. PyTorch is also a baseline of the benchmarks, which skip it without it |
| pytest | The tests of the Python package |
| psutil | The harness of the benchmarks, for the CPU time, memory and bytes read of a run |
| mrcfile, lmdb | The baselines of the benchmarks that read as other software does |
| ReFrame | Running campaigns of the benchmarks. Not installed on Windows, where it does not run |
| Sphinx, Furo | The reference of the Python package, which imports it with `vitrio.data` and so needs numpy too. Not needed without `-DVITRIO_BUILD_DOCS=ON` and `-DVITRIO_BUILD_PYTHON=ON` |
| scikit-build-core | A build of the Python package by pip |

## Conventions

- **C++14.** The library and its public headers use nothing newer, and CMake
  requires that standard rather than preferring it. The header check compiles
  the public headers as C++14, 17, 20 and 23.
- **Licence.** Every source file starts with
  `SPDX-License-Identifier: LGPL-2.1-or-later`.
- **Public and private.** A header under `include/` is public and is installed.
  A header under `src/` is neither. A public symbol is marked `VITRIO_API`,
  from the generated `vitrio/export.hpp`; everything else is hidden. A public
  class whose implementation the library reaches into, such as
  `file_transfer_pool`, declares `class implementation;` and a
  `get_implementation()` that is not marked, and its members that are
  exported are marked one by one. The implementation is a class of its own
  under `src/`.
- **One class per file.** A class is declared in a `.hpp` and defined in a
  `.cpp`, or in an `.inl` included at the end of the header when it has to be
  inline.
- **Implementations of an interface.** A class that implements an
  interface is named after it, with what sets it apart in front:
  `file_image_reader_provider` and `caching_image_reader_provider` are
  `image_reader_provider`s, `tiff_image_reader` is an `image_reader` and
  `mock_executor` is the mock of `executor`. Its files are named as it is.
- **Formatting.** Tabs, 80 columns. The code is formatted by hand and there is
  no `.clang-format`: none reproduces the style, so one would rewrite the code
  it was run on.
- **Vocabulary types.** `vitrio::span` is a view of contiguous elements, of
  dynamic extent only. It is a type of its own under every C++ standard and
  never an alias of `std::span`, so that the ABI does not depend on the
  standard a consumer compiles with. `vitrio::byte` is `unsigned char`, the
  type C++14 lets the memory of any object be accessed through.
  `vitrio::memory_range` is a stretch of the address space, such as one to
  be advised, and not elements to be accessed, even though a span of bytes
  is built the same way.
- **Arrays.** Four classes with one job each: `array` and `const_array` keep
  their memory alive, `array_ref` and `const_array_ref` do not, and the
  `const` ones cannot be written through. A reference is two pointers, as
  cheap to copy as a span, and what it refers to must outlive it. A function
  that uses an array only until it returns takes a reference; one that keeps
  it takes an owning class. None of them does anything with its elements.
  Memory from anywhere becomes an array through a `std::shared_ptr` that
  releases it.
- **Transfers.** `image_reader::read_async`, `image_writer::write_async` and
  `image_writer::flush_async` start a transfer and report it to a
  `completion_notifier` exactly once: a failure is reported, never thrown,
  and only a null notifier throws. A transfer keeps the owning `array` or
  `const_array` it is given until it reports.
  - A reader or writer that works on the calling thread derives from
    `synchronous_image_reader` or `synchronous_image_writer` and implements
    the plain `read`, or `write` and `flush`, which throw.
  - Code whose own contract is to be done before it returns waits through
    `src/waiting_transfers.hpp`. Holding only a reference, it lends it to the
    transfer with `borrow_array`, from `src/array/borrowed_array.hpp`.
  - Several transfers report as one through a `counting_notifier`. A task
    that starts a transfer is submitted with a `failure_forwarding_notifier`
    (`src/concurrency/`), so that the transfer reports its outcome and the
    task only its failure.
  - Files are read and written through a `file_transfer_pool`, which a file
    format is given when it opens or creates a file, and whose executor the
    reader or writer keeps. Only what is about files takes one:
    `file_image_reader_provider` and `file_image_writer_provider` hold it,
    so that `acquire` stays a key alone.
  - A write covers the runs of bytes its regions take in the file and
    nothing between them, which a concurrent write of other regions may be
    writing: it never bridges a gap, nor reads what it writes around.
  - A scratch takes in what it is offered as a transfer:
    `image_scratch_entry::store_async` reports the same way. A run is read
    from the file once, and a store that needs a run already being taken in
    waits for it. The lock of an entry guards only which runs are absent,
    being taken in or held, never a read. What a reader copies out of an
    entry once it has taken that in is a task on the executor the reader
    was given.
  - Work that waits for the tasks of an executor says so, and must not run
    on a thread of it. Creating an `indexed_image_scratch` opens its files
    that way, all at once.
- **Sizes that come from a file.** Extents and strides may be read from a
  file, so arithmetic on them goes through `checked_add` and
  `checked_multiply` and an overflow is reported, not computed.
- **Constants of a class.** A `static constexpr` member of a public class is
  marked `VITRIO_API` and defined in its `.cpp`. C++14 needs that definition
  whenever the constant is bound to a reference, and a consumer gets it from
  the shared library.
- **Files a test writes.** `scoped_path` names a file under the build tree
  and removes it. Every case runs as a process of its own and several may
  run at once, so no two cases may name the same file: a fixture that
  several cases share names its files after the running case.
- **Mocks.** A test that needs a stand-in for an interface uses the
  trompeloeil mock under `tests/unitary/src/mock/`, never a hand-written
  fake. A mock is added with the first test that uses it. An expectation on
  a mocked transfer reports to its notifier, as the transfer would. A mock
  that the code under test destroys, such as a task or a file transfer
  operation, holds its own expectations in its member `expectations`, which
  are checked as it goes: trompeloeil aborts when a mock is destroyed before
  the expectations on it.
- **Transfers in tests.** A test that reads or writes through an interface
  waits with `read_now`, `write_now`, `flush_now` and `store_now`, from
  `tests/assets/include/vitrio/tests/transfers.hpp`.
- **Private headers use their dependencies openly.** A header under `src/`
  includes what it needs and names its types as they are: a mapping of a
  file is a `boost::interprocess::mapped_region`, not a class wrapped around
  it. Hiding a dependency is for public headers only.
- **Element conversions.** `cast`, in `src/array/cast.hpp`, converts one
  element as `static_cast` does. Any type is produced from any other, except
  that a complex number is only produced into another complex type.
- **Half precision.** `float16_t`, in `src/array/float16_t.hpp`, is a class
  of vitrio's own, which converts and does no arithmetic. Its conversions
  are bit arithmetic without a branch, and no intrinsic, so that a compiler
  vectorizes the loops of the strided transfer that call them for every
  element. A choice among cases is made with a mask, since GCC keeps a
  branch around a floating point operation, and a loop with a branch is not
  vectorized. It is converted to a float exactly, and from a float and from
  a double each rounded once. Every other type goes through a float, which
  holds every half precision number and every integer half precision does
  not round to an infinity.
- **Keys and files.** A provider (`image_reader_provider`,
  `image_writer_provider`) is asked with a key, and what a key means is the
  provider's business. An `image_location` is a key and an index in a stack,
  and the plans, the loader and the saver carry keys too. What is about
  files says so in its name: `image_file_probe`, `image_file_error`,
  `image_file_format_error`, and three roles that are kept apart:
  - A file format (`image_file_read_format`, `image_file_write_format`, and
    for one kind of file `mrc_file_read_format`, `tiff_file_write_format`)
    knows one format, and registers itself at namespace scope with the
    macros of `src/formats/image_file_format_registration_macros.hpp`.
  - A selector (`image_file_read_format_selector`,
    `image_file_write_format_selector`) holds formats and opens a file with
    the most suitable of them. Its `get_shared()` holds the bundled ones.
  - The two providers whose keys are paths, `file_image_reader_provider`
    and `file_image_writer_provider`, are each given a selector and hold no
    formats themselves.
- **Detector events.** A detector that counts electrons records events, not
  images. A `detector_event_timeline` holds the events of a stretch of time,
  in groups of one timestamp each, and `detector_event_position_view` views
  their positions as a matrix of a row per event. What is fixed for an
  acquisition, its grid and its time, is a
  `detector_event_timeline_descriptor`. Their files have the same three
  roles as those of images, `detector_event_timeline_file_read_format`, its
  selector and `file_detector_event_timeline_reader_provider`, and nothing
  of them is about one format: EER is one implementation, under
  `vitrio::eer`. An acquisition becomes images through
  `detector_event_image_reader_provider`, which is given the
  `detector_event_fractionation` and the `detector_event_renderer` to read
  it with; the counting one bins pixels of the detector by a
  `detector_event_binning`, stated in pixels rather than in quanta. That is chosen by whoever reads, not by the file, so it is
  stated where readers are made and never registered as a format of images.
- **Assertions.** `VITRIO_ASSERT`, from `src/assert.hpp`, checks what the code
  relies on. What a caller of the public API may get wrong is reported with an
  exception instead.
- **Logging.** What can be reported in no other way is logged with the
  `VITRIO_LOG_*` macros of `src/logger.hpp`, which wrap spdlog and take its
  format strings. They write to a logger of the library's own, on the
  standard error stream, and never to the default logger of spdlog. spdlog
  stays out of the public headers.
- **Tuning.** A number that tunes the library has its default in a macro of
  `src/config.hpp`, `VITRIO_DEFAULT_<NAME>`, which the build may override,
  and is read through `src/tuning.hpp`, where the variable `VITRIO_<NAME>`
  of the environment overrides it once the library runs. `VITRIO_DISABLE_ASYNCHRONOUS_IO`, set
  to anything but 0, has the process read its files as one without
  asynchronous ports does. None of them is an argument of the API.
- **The Python binding.** `vitrio._binding` mirrors the C++ API, with the
  same names and the same required arguments. What Python expects on top,
  such as defaults and the ways a data type is stated, is Python code in
  `python/vitrio/`, and `__init__.py` is the whole public surface. The
  extension is the one target built as C++17, which nanobind needs. Python
  sources are indented with tabs, as the C++ ones are.
- **`vitrio.data`.** A subpackage of plain Python over the public surface of
  `vitrio`, whose own `__init__.py` is its whole public surface. It needs
  numpy, which `vitrio` does not, so `vitrio` imports it only when
  `vio.data` is first asked for. It parses no file of metadata: it takes the
  DataFrames, mappings and structured arrays the metadata was parsed into.
  A batch is a dict of numpy arrays read into memory of its own, and only
  `_frameworks.py` knows of PyTorch and JAX. A dataset and its sources
  describe rows and hold nothing that reads them: each read is given the
  image loader it goes through, a scratch included, and a read given none
  goes through the one `_default_loader.py` shares across the process.
- **File transfers in Python.** A provider assembled without a
  `FileTransferPool`, and a write given none, take the one
  `_default_transfers.py` shares across the process, whose conversions run on a thread pool of its own, apart from the
  threads of the loaders, which may wait for reads. The work of a scratch
  given no executor, opening its files, prefetching and copying out what it
  took in, runs on that thread pool too. A child of a fork makes its own, as
  `_default_loader.py` does.
- **Docstrings.** Everything `vitrio.__all__` exports has a docstring, and so
  has each public method and property of a class, which
  `tests/python/test_documentation.py` checks. They are written in the style
  of Google (`Args:`, `Returns:`, `Raises:`) and written for Python: getters
  are properties, exceptions are Python's, an array is anything that speaks
  DLPack, and a call that blocks says that it releases the GIL. In the
  binding the text is passed to `nb::class_`, `.def` and `def_prop_ro` as
  adjacent string literals, with `\n` only where a line ends, so that a
  paragraph is one line to Sphinx. A name that Python wraps, such as `read`
  or `loader`, is documented in the wrapper and only briefly in the
  binding, which says to prefer the wrapper.
- **`import vitrio as vio`.** That is how the package is imported wherever
  it is shown in use: the README, the examples and the tests. numpy is
  `np` and pandas is `pd`, as their own documentation has them. Prose names
  what it refers to in full, as in `vitrio.Array`.
- **Arrays across the binding.** A function takes an array as an
  `nb::ndarray`, which nanobind fills from DLPack or the buffer protocol
  without a copy, and `Array` hands its memory out through the object
  nanobind exports arrays with. vitrio has no DLPack code of its own.
  Memory given from Python is lent (`memory_loan`): the arrays over it are
  given an owner that owns nothing, and the loan returns the memory once
  they are gone. So no thread but one that holds the GIL ever releases
  anything of Python, which a worker thread could not do safely while the
  interpreter shuts down. An asynchronous function hands the loan to the
  `Completion` it returns, and destroying that waits for the work.
- **Views of the memory of a binding.** What a bound object hands out as a
  view of its own memory, such as the positions of a
  `DetectorEventTimeline`, is read-only and keeps the object alive. The
  object counts the views it has handed out and refuses to change while
  one is alive, with `BufferError`, as a `bytearray` refuses to resize
  while a `memoryview` of it is. Nor is it changed while it is used
  without the GIL.
- **The GIL.** A bound function that reads, writes or waits releases the
  GIL meanwhile. So does destroying a thread pool, which waits for its
  workers.
- **Benchmarks.** A procedure measures, and a question is a named sweep of
  its ReFrame test, not a procedure of its own. A run writes every setting
  into its record, defaults included, and fails on a parameter it does not
  have, so that a mistyped one does not measure the default. Results are
  never committed. The schema of the records, in
  `benchmarks/python/vitrio_bench/results.py`, is the contract with whoever
  analyses them, and a change to what a field means bumps it.
- **Versions.** `VERSION` holds the version of the project. Before 1.0 a minor
  release may break the ABI, so the name of the shared library carries the
  minor version (`libvitrio.so.0.1`).

## Pull requests

Whoever opens a pull request assigns it to themselves and gives it the label,
among the repository's own, that matches what it changes: `bug` for a fix,
`enhancement` for something new, `documentation` when only documentation
changes, and so on.

## Continuous integration

`.github/workflows/build-and-test.yml` builds and tests with GCC and Clang on
Linux, Clang on macOS and MSVC on Windows, then again with the address and
undefined behaviour sanitizers and with the thread sanitizer, and once more
with CMake 3.18. The jobs that are not under a sanitizer build the Python
package too and run its suite, one of them with the oldest Python the
package supports.

Those jobs fetch the dependencies, except the one with CMake 3.18. That one
finds them all installed from the packages of Ubuntu, as a distribution
builds vitrio.

One more job builds the last release beside the tree and has libabigail
say what changed in the ABI between the two. A change does not fail it:
before 1.0 a release may break the ABI, and the report tells which kind of
release the next one has to be. Both are built against the packages of
Ubuntu.

`.github/workflows/documentation.yml` builds the documentation of the C++ API
and of the Python package, puts them side by side and publishes them to
GitHub Pages when a version is tagged.

`.github/workflows/release.yml` builds the source distribution and, from it,
the wheels of each platform, and tests each wheel. How cibuildwheel builds
them is stated there, on its step. A tag `v*` publishes
them to PyPI, which trusts the workflow, so no token is kept. It runs too
when what decides how they are built changes, and by hand.

All of them use public actions, and none of the organisation's.
