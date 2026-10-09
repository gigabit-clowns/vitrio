# vitrio

[![PyPI](https://img.shields.io/pypi/v/vitrio)](https://pypi.org/project/vitrio/)
[![Build and test](https://github.com/gigabit-clowns/vitrio/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/gigabit-clowns/vitrio/actions/workflows/build-and-test.yml)

vitrio is a high-throughput image I/O library tailored to the access patterns
and file formats common in Cryo-Electron Microscopy (CryoEM). It is made for
programs that go through a lot of images: it loads and stores them in batches,
on several threads, and puts the pixels straight into memory you already have,
be it a NumPy array, a tensor from PyTorch or another ML framework, or your own
buffer in C++.

It is a C++14 library with a Python package on top.

```python
import vitrio as vio

micrograph = vio.read("/path/to/micrograph.mrc")
movie = vio.read("/path/to/movie.tif")
```

vitrio is young. It grew out of the image I/O of
[rexlib](https://github.com/gigabit-clowns/rexlib), and its API may still
change before 1.0.

## What it does

- Reads and writes whole files, single images of a stack, and batches of
  images that may be spread over many files.
- Crops patches around a list of coordinates, for instance the particles
  picked on a micrograph.
- Runs batches on a thread pool and gives you something to wait on, so
  several can be in flight at once.
- Converts between data types on the way, so a file of 16-bit integers can
  land in a `float32` array.
- Shares memory with anything that speaks DLPack or the buffer protocol.
  Nothing is copied on the way in or on the way out.
- From C++, keeps a copy of what has been read in RAM or in a memory-mapped
  file, so that going back to the same images does not hit the original
  files again.
- Takes new formats without changes to the library: a format is a class
  that registers itself.

## Supported formats

| Format | Read | Write | Extensions | Notes |
|---|---|---|---|---|
| MRC | yes | yes | `.mrc`, `.mrcs`, `.map`; also `.st`, `.rec` and `.ali` when reading | Images, volumes and stacks of either. 8- and 16-bit integers, 16- and 32-bit floats, complex. Either byte order. |
| TIFF | yes | yes | `.tif`, `.tiff` | Classic and BigTIFF, with one page or many. Integers and floats up to 64 bits, complex. |

A file is recognized by its contents where the format allows it, and by its
extension otherwise. TIFF goes through libtiff and can be left out of a
build. More formats will follow.

## Installing

```
pip install vitrio
```

There are wheels for Linux (x86_64 and aarch64), macOS (Apple silicon and
Intel) and Windows (x86_64), for Python 3.10 and newer. They carry everything
they need. On any other platform pip builds the package from source, and
then the dependencies listed under [Building from source](#building-from-source)
have to be installed first.

The C++ library is built from source for now.

## Using it from Python

`read` works the format out from the file and returns an array that NumPy,
PyTorch and the rest use without a copy.

```python
import numpy as np
import torch
import vitrio as vio

micrograph = vio.read("micrograph.mrc", data_type=np.float32)
pixels = np.asarray(micrograph)           # the same memory, not a copy
tensor = torch.from_dlpack(micrograph)    # and again

vio.write_single(pixels, "copy.tif")
```

Reading many images is done in batches. A batch goes into one array, on a
thread pool, and its images may come from any number of files. The call
returns before the batch is in, so the next one can be on its way while you
work on the current one:

```python
import random

BATCH_SIZE = 64
STACKS = ["first.mrcs", "second.mrcs", "third.mrcs"]

# Every particle of every stack, in the order they will be read
locations = [
    vio.ImageLocation(stack, index)
    for stack in STACKS
    for index in range(vio.query_descriptor(stack).extents[0])
]
random.shuffle(locations)

size = vio.get_core_extents(vio.query_descriptor(STACKS[0]))
loader = vio.loader(cache=len(STACKS))    # keeps the stacks open

def start_reading(first):
    batch = locations[first:first + BATCH_SIZE]
    particles = np.empty((len(batch), *size), dtype=np.float32)
    return particles, vio.read_batch_async(loader, particles, batch)

particles, reading = start_reading(0)
for first in range(BATCH_SIZE, len(locations), BATCH_SIZE):
    upcoming = start_reading(first)    # runs in the background
    reading.get()                      # wait for the current batch
    process(particles)                 # your code
    particles, reading = upcoming
reading.get()
process(particles)
```

Patches are cropped out of an image the same way, around a list of centres:

```python
patches = np.zeros((2, 128, 128), dtype=np.float32)
centres = [(1024, 512), (300, 2100)]    # (y, x)
location = vio.ImageLocation("micrograph.mrc")
vio.read_patches_async(loader, patches, location, centres).get()

vio.write_stack(patches, "patches.mrcs", data_type="float16")
```

An array can be anything that hands out host memory through DLPack or the
buffer protocol: a NumPy array, a PyTorch or JAX tensor, a `memoryview`. It
is read or written where it is, views included.

The `_async` functions return right away with a completion. Call `get()` on
it to wait and to find out whether anything failed. The completion keeps
its array alive, and dropping one that is not ready waits for it.

[examples/extract_particles.py](https://github.com/gigabit-clowns/vitrio/blob/main/examples/extract_particles.py)
is a complete program. It crops the particles picked on a set of micrographs
into stacks, reading and writing the STAR files of RELION, with several
batches in flight at once.

## Using it from C++

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

```cpp
#include <vitrio/array/array.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>

int main()
{
	vitrio::file_image_reader_provider readers(
		vitrio::image_file_read_format_selector::get_shared()
	);

	// Read as 32-bit floats, whatever the file holds.
	vitrio::array image = vitrio::read(
		"micrograph.mrc", readers, vitrio::numerical_type::float32
	);

	vitrio::write_single(
		image, "copy.tif",
		*vitrio::image_file_write_format_selector::get_shared()
	);
}
```

The headers under `include/vitrio/` are the whole API, and each one documents
what it declares.

## Building from source

You need a C++14 compiler, CMake 3.18 or newer and these libraries, which
CMake looks for with `find_package`:

- Boost 1.70 or newer (Filesystem, Interprocess and ContainerHash)
- half
- spdlog
- libtiff 4.5 or newer, unless TIFF is turned off
- Catch2 3 and trompeloeil, for the tests

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

The repository has a vcpkg manifest, so one way of getting all of them is
to pass vcpkg's toolchain file to the first command.

A few options change what gets built:

| Option | Effect |
|---|---|
| `-DVITRIO_BUILD_TESTING=OFF` | Skip the tests, and with them Catch2 and trompeloeil |
| `-DVITRIO_ENABLE_TIFF=OFF` | Leave TIFF out, and libtiff with it |
| `-DVITRIO_BUILD_PYTHON=ON` | Build the Python package too. Needs Python 3.10 or newer with nanobind installed |

With the last one the package ends up under `python/` in the build tree, so
putting that directory on `PYTHONPATH` is enough to import it. `pip install .`
builds and installs it in one go.

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](https://github.com/gigabit-clowns/vitrio/blob/main/LICENSE).
