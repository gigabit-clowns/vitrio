# vitrio

vitrio reads and writes the image files of electron microscopy, such as MRC
and TIFF. It is a C++ library with a Python package over it.

It is under construction. The code is being ported from the image subsystem of
[rexlib](https://github.com/gigabit-clowns/rexlib). The C++ library is here:
MRC and TIFF files are read and written, whole or a batch at a time through
a loader or a saver, and a scratch keeps a copy of what is read in memory
or in a mapped file. The Python package is still to come.

## Building

vitrio needs a C++14 compiler and CMake 3.18 or newer. Its dependencies are
found with `find_package`, so they have to be installed first: Boost
(Filesystem, Interprocess and ContainerHash), half, spdlog and libtiff, and
for the tests Catch2 3 and trompeloeil.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

To leave the tests out, and their two dependencies with them, add
`-DVITRIO_BUILD_TESTING=OFF`. To leave TIFF out, and libtiff with it, add
`-DVITRIO_ENABLE_TIFF=OFF`.

## Using it from CMake

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](LICENSE).
