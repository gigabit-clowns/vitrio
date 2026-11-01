# Dependencies

How dependencies are fetched, found and pinned, and what each one is needed by.

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
| zlib | libtiff, for Deflate compression, the ZARR format, for its zlib and gzip codecs, and the MRC files compressed with gzip. Fetched before libtiff and c-blosc, it is the one a fetched libtiff and c-blosc find, which needs CMake 3.24. Not needed with `-DVITRIO_ENABLE_TIFF=OFF`, `-DVITRIO_ENABLE_ZARR=OFF` and `-DVITRIO_ENABLE_GZIP=OFF` together |
| nlohmann/json 3.6 or newer | The library, privately and header only, for the metadata of ZARR. Not needed with `-DVITRIO_ENABLE_ZARR=OFF` |
| zstd 1.4 or newer | The library, privately, for the zstd codec of ZARR, and c-blosc. Found through pkg-config. Not needed with `-DVITRIO_ENABLE_ZARR=OFF` |
| c-blosc 1.14 or newer | The library, privately, for the blosc codec of ZARR. Fetched, it is built over the zstd and zlib of the project rather than its own copies of them, and keeps its own lz4. Found through pkg-config. Not needed with `-DVITRIO_ENABLE_ZARR=OFF` |
| Catch2 3 | The test suites |
| trompeloeil | The unit tests, for their mocks |
| Google Benchmark 1.8 or newer | The C++ benchmarks. Not needed without `-DVITRIO_BUILD_BENCHMARKS=ON`. Unlike the others it is cloned with git, not fetched as an archive, since it reads its version from the tags of its repository |
| Python 3.10 or newer, nanobind 3 | The Python package. Not needed without `-DVITRIO_BUILD_PYTHON=ON`, and never fetched: pip provides nanobind |
| numpy | `vitrio.data`, and the tests of the Python package. Not a dependency of the package: `vitrio.data` reports it missing when imported |
| PyTorch, JAX | Neither is needed. `vitrio.data` imports each when a batch is handed to it, and its tests and the cases that exchange arrays with them are skipped without them. PyTorch is also a baseline of the benchmarks, which skip it without it |
| pytest | The tests of the Python package |
| zarr-python, numcodecs | The tests of the Python package that read what zarr-python writes, and the script that writes the stores of `tests/assets/zarr`. They are skipped without it, which needs Python 3.12 |
| psutil | The harness of the benchmarks, for the CPU time, memory and bytes read of a run |
| mrcfile, lmdb | The baselines of the benchmarks that read as other software does |
| zarr-python, tensorstore | The baselines of the benchmarks that write and read ZARR. Each is skipped without it; zarr-python needs Python 3.12 and tensorstore 3.11 |
| ReFrame | Running campaigns of the benchmarks. Not installed on Windows, where it does not run |
| Sphinx, Furo | The reference of the Python package, which imports it with `vitrio.data` and so needs numpy too. Not needed without `-DVITRIO_BUILD_DOCS=ON` and `-DVITRIO_BUILD_PYTHON=ON` |
| scikit-build-core | A build of the Python package by pip |
