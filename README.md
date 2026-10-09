# vitrio

vitrio reads and writes the image files of electron microscopy, such as MRC
and TIFF. It is a C++ library with a Python package over it.

It is under construction. The code is being ported from the image subsystem of
[rexlib](https://github.com/gigabit-clowns/rexlib). The C++ library is here:
MRC and TIFF files are read and written, whole or a batch at a time through
a loader or a saver, and a scratch keeps a copy of what is read in memory
or in a mapped file. The Python package is over it.

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

## The Python package

```
pip install vitrio
```

The wheels hold everything they need. Where there is none for a platform,
pip builds the package from its source, as `pip install .` does from this
tree. The dependencies of the library then have to be installed, as for any
other build, and pip brings the build tools.

To work on it, build it with `-DVITRIO_BUILD_PYTHON=ON`, which needs Python
3.10 or newer with nanobind installed in it. The package is assembled
under `python/` of the build tree, so that directory goes on `PYTHONPATH`.

```python
import numpy as np
import torch
import vitrio as vio

image = vio.read("map.mrc")
values = np.asarray(image)           # the same memory, not a copy
tensor = torch.from_dlpack(image)    # and again

stack = np.empty((64, 256, 256), dtype=np.float32)
locations = [vio.ImageLocation("stack.mrcs", i) for i in range(64)]
vio.read_batch_async(vio.loader(), stack, locations).get()

vio.write_stack(stack, "copy.mrcs")
```

An array is given as whatever hands out host memory through DLPack or the
buffer protocol: a numpy array, a tensor of PyTorch or JAX, a `memoryview`.
It is read or written where it is. The reads that allocate return a
`vitrio.Array`, which hands its memory out in both ways too.

An asynchronous read or write returns a completion that keeps its array
alive. Dropping a completion that is not ready waits for it.

[examples/extract_particles.py](examples/extract_particles.py) is a whole
program: it crops the particles picked on a set of micrographs into stacks,
reading and writing the STAR files of RELION.

## Using it from CMake

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](LICENSE).
