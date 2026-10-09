# vitrio

vitrio reads and writes the image files of electron microscopy, such as MRC
and TIFF. It is a C++ library with a Python package over it.

It is under construction. The code is being ported from the image subsystem of
[rexlib](https://github.com/gigabit-clowns/rexlib), and so far only what the
image code stands on is here: nothing reads or writes an image yet.

## Building

vitrio needs a C++14 compiler and CMake 3.18 or newer. Its dependencies are
found with `find_package`, so they have to be installed first: Boost
(Filesystem and ContainerHash), and for the tests Catch2 3 and trompeloeil.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

To leave the tests out, and their two dependencies with them, add
`-DVITRIO_BUILD_TESTING=OFF`.

## Using it from CMake

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](LICENSE).
