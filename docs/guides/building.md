<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Building from source

You need a C++14 compiler and CMake 3.24 or newer. On Linux you also need a
C compiler and make.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

## The Python package

`pip install .` builds and installs the package in one go. It takes the
options of this page as `-C cmake.define.<option>=<value>`, such as
`-C cmake.define.VITRIO_USE_SYSTEM_DEPENDENCIES=ON`.

To build it with CMake instead, add `-DVITRIO_BUILD_PYTHON=ON`. This needs
Python 3.10 or newer with nanobind installed. The package ends up under
`python/` in the build tree, so putting that directory on `PYTHONPATH` is
enough to import it.

## Dependencies

By default CMake downloads the libraries that vitrio depends on and builds
them into the vitrio library, so nothing has to be installed first.

| Library | Needed for | Left out with |
|---|---|---|
| Boost (Filesystem, Interprocess and ContainerHash) | Everything | |
| spdlog | Everything | |
| libtiff, and zlib for it | TIFF and EER | `-DVITRIO_ENABLE_TIFF=OFF` |
| nlohmann/json, c-blosc, zstd and zlib | ZARR | `-DVITRIO_ENABLE_ZARR=OFF` |
| zlib | MRC files compressed with gzip | `-DVITRIO_ENABLE_GZIP=OFF` |
| liburing | io_uring, on Linux | `-DVITRIO_ENABLE_IO_URING=OFF` |
| Google Highway | Vectorized conversion of half precision values | `-DVITRIO_ENABLE_SIMD=OFF` |
| Catch2 and trompeloeil | The tests | `-DVITRIO_BUILD_TESTING=OFF` |

### Using the libraries of the system

Turn `VITRIO_USE_SYSTEM_DEPENDENCIES` on to use the libraries installed on
the system instead. CMake then looks for each of them with `find_package`,
or with pkg-config for liburing, c-blosc and zstd. CMake 3.18 is enough in
this case.

The minimum versions are Boost 1.70, libtiff 4.5, spdlog 1.4, liburing 2.0,
nlohmann/json 3.6, c-blosc 1.14, zstd 1.4, Highway 1.2.0 and Catch2 3.

To take a single library from the system and build the others, turn its own
option on, such as `-DVITRIO_USE_SYSTEM_TIFF=ON`. The options are
`VITRIO_USE_SYSTEM_BOOST`, `VITRIO_USE_SYSTEM_SPDLOG`,
`VITRIO_USE_SYSTEM_TIFF`, `VITRIO_USE_SYSTEM_ZLIB`,
`VITRIO_USE_SYSTEM_LIBURING`, `VITRIO_USE_SYSTEM_NLOHMANN_JSON`,
`VITRIO_USE_SYSTEM_BLOSC`, `VITRIO_USE_SYSTEM_ZSTD`,
`VITRIO_USE_SYSTEM_HIGHWAY`, `VITRIO_USE_SYSTEM_CATCH2` and
`VITRIO_USE_SYSTEM_TROMPELOEIL`.

### Building without network access

Give CMake the source directory of each library that it would download, such
as `-DFETCHCONTENT_SOURCE_DIR_BOOST=<dir>`.

## Options

| Option | Effect |
|---|---|
| `-DVITRIO_USE_SYSTEM_DEPENDENCIES=ON` | Use the installed libraries instead of downloading and building them |
| `-DVITRIO_BUILD_TESTING=OFF` | Skip the tests, and Catch2 and trompeloeil with them |
| `-DVITRIO_ENABLE_TIFF=OFF` | Leave TIFF and EER out, and libtiff with them |
| `-DVITRIO_ENABLE_ZARR=OFF` | Leave ZARR out, and nlohmann/json, c-blosc and zstd with it |
| `-DVITRIO_ENABLE_GZIP=OFF` | Leave out reading MRC files compressed with gzip. zlib is left out too when TIFF and ZARR are off |
| `-DVITRIO_ENABLE_IO_URING=OFF` | On Linux, leave io_uring out, and liburing with it |
| `-DVITRIO_ENABLE_SIMD=OFF` | Convert half precision values with ordinary loops, and leave Highway out |
| `-DVITRIO_SIMD_DISPATCH=STATIC` | Compile the vector loops once, for the instruction set that the compiler flags allow. By default they are compiled once per instruction set, and the best one is chosen when the library runs |
| `-DVITRIO_SIMD_TARGETS=AVX2` | The instruction sets to compile the vector loops for beyond the baseline, by their Highway names. The default is `AVX2;AVX3` on x86, and none on aarch64, where NEON already covers half precision |
| `-DVITRIO_BUILD_PYTHON=ON` | Build the Python package too |
| `-DVITRIO_BUILD_EXAMPLES=ON` | Build the C++ examples of `examples/cpp`, which CTest then runs |
| `-DVITRIO_BUILD_DOCS=ON` | Add the target `vitrio-docs`, which builds the C++ reference with Doxygen. With `VITRIO_BUILD_PYTHON` it also adds `vitrio-python-docs`, which builds the Python reference with Sphinx |
