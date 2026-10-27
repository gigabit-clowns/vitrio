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

vitrio is young and its API and ABI are subject to change.

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
- Feeds images and their metadata to PyTorch or JAX in shuffled batches,
  with the next batches read in the background and no worker processes.
- Keeps a copy of what has been read in RAM or in a memory-mapped file, so
  that going back to the same images does not hit the original files again.
- Takes new formats without changes to the library: a format is a class
  that registers itself.

## Supported formats

| Format | Read | Write | Extensions | Notes |
|---|---|---|---|---|
| MRC | yes | yes | `.mrc`, `.mrcs`, `.map`; also `.st`, `.rec` and `.ali` when reading | Images, volumes and stacks of either. 8- and 16-bit integers, 16- and 32-bit floats, complex. Either byte order. |
| TIFF | yes | yes | `.tif`, `.tiff` | Classic and BigTIFF, with one page or many. Integers and floats up to 64 bits, complex. |
| EER | yes | no | `.eer` | The events of an electron-counting detector. It can provide access to a raw electron-event timeline or seamless rendering into a movie of arbitrary fractionation. Supports compression 65000, 65001 and 65002. |
| DM (DigitalMicrograph) | yes | no | `.dm3`, `.dm4` | Versions 3 and 4, as Gatan Microscopy Suite writes gain and dark references. Images and stacks of them. Integers, floats and complex; RGB and packed complex are not read. Either byte order. The thumbnail is skipped, and only the first image of a file is read. Versions 2 and 5 are not read. |

A file is recognized by its contents where the format allows it, and by its
extension otherwise. TIFF and EER go through libtiff and can be left out of a
build. More formats will follow.

## Installing

```
pip install vitrio
```

There are wheels for Linux (x86_64 and aarch64), macOS (Apple silicon and
Intel) and Windows (x86_64), for Python 3.10 and newer. They carry everything
they need. On any other platform pip builds the package from source, which
takes a C++ compiler and access to the network: the libraries vitrio needs
are fetched and built with it.

The C++ library is built from source for now.

## Using it from Python

The reference of the package is at
[gigabit-clowns.github.io/vitrio/python](https://gigabit-clowns.github.io/vitrio/python/),
built from its docstrings, which `help()` and your editor show too.

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

Reading many images is done in batches, on a thread pool, and the `_async`
functions return right away with a completion to wait on, so that the next
batch is on its way while you work on the current one. Patches are cropped
out of an image around a list of centres the same way, and a scratch keeps
the images a program goes back to in RAM or in a file. Each of those is
shown in a small program under [examples/](https://github.com/gigabit-clowns/vitrio/blob/main/examples/README.md).

An array can be anything that hands out host memory through DLPack or the
buffer protocol: a NumPy array, a PyTorch or JAX tensor, a `memoryview`. It
is read or written where it is, views included.

A movie of an electron-counting detector, such as an EER file, holds the
events the detector counted rather than images. It is read through a
provider that cuts its frames into as many fractions as you ask for:

```python
readers = vio.detector_event_image_reader_provider(40, binning=2)
movie = vio.read("movie.eer", readers, data_type=np.float32)    # 40 fractions
```

## Feeding a model

`vitrio.data` reads a dataset for training in batches. A `Dataset` is rows
of named fields: images read from files, and whatever metadata goes with
them, such as the CTF, the pose or the class of each particle. vitrio
parses no metadata file itself. It takes what you parsed it into, such as
the columns of a DataFrame from [starfile](https://github.com/teamtomo/starfile).
A `DataLoader` goes through it in shuffled batches, as dicts of NumPy
arrays, PyTorch tensors or JAX arrays, with a few batches in flight on a
thread pool. A batch bound for a GPU is read straight into pinned memory.

```python
dataset = vio.data.Dataset({
    "image": vio.data.ImageSource(locations, data_type=np.float32),
    "label": labels,
})
for batch in vio.data.DataLoader(dataset, batch_size=128, shuffle=True):
    batch["image"], batch["label"], batch["index"]
```

A dataset only describes its rows. What reads them is an image loader,
with its threads, its open files and maybe a scratch, and it is given to
the reads: `DataLoader(dataset, ..., loader=vio.loader(workers=8))`. Reads
given none go through a loader shared by the whole process.

The pieces fit other layouts too:

- A cryoSPARC `.cs` file is a structured array that `np.load` reads.
  `vio.data.make_locations(cs["blob/path"], cs["blob/idx"])` gives its
  locations, and `vio.data.as_columns(cs)` its columns.
- A row may have several images, all of one shape: give `ImageSource` a row
  of locations for each row, such as the tilts of a particle or the halves
  of a movie.
- `PatchSource` crops a patch around a centre out of a larger image for
  each row, such as particles out of micrographs or subtomograms out of a
  tomogram.
- `dataset.subset(rows)` splits off a validation set, and
  `dataset.preload()` reads everything into memory once. To keep a copy
  that fills as the first epoch goes and may live in a file, read through
  a loader with a scratch instead:
  `DataLoader(dataset, ..., loader=vio.loader(scratch=scratch))`.
- With JAX, `output="jax"` puts each batch on a device, or across several
  with `sharding=`. JAX compiles again for every new shape, so pass
  `drop_last=True`, or `pad_last=True`, which adds a `valid` field marking
  the padded rows. Across several processes, give each one
  `vio.data.BatchSampler(..., shard=(jax.process_index(),
  jax.process_count()))` and the same seed.
- PyTorch's own DataLoader takes a `Dataset` as it is. Give it a
  `BatchSampler` as its `sampler` with `batch_size=None`, so that it reads
  a batch per call.

## Examples

[examples/](https://github.com/gigabit-clowns/vitrio/blob/main/examples/README.md) holds programs that show vitrio at work,
sorted by language and by how much of it you hold. In Python, `basic/`
lets vitrio assemble what it needs, and `advanced/` names the thread
pools, loaders and scratches. Among them, `train_2d_classifier.py` trains a
network of PyTorch on the 2D classes of a RELION job,
`extract_particles.py` crops the particles picked on a set of micrographs
into stacks, and `fractionate_eer.py` cuts EER movies into fractions.

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
what it declares. Their reference is at
[gigabit-clowns.github.io/vitrio/cpp](https://gigabit-clowns.github.io/vitrio/cpp/). That includes how to read the events of an EER file through
`detector_event_image_reader_provider`.

[examples/cpp/](https://github.com/gigabit-clowns/vitrio/blob/main/examples/cpp/read_and_convert.cpp) has this program as a
target of the build.

## Building from source

You need a C++14 compiler and CMake 3.24 or newer, and on Linux a C compiler
and make too. The libraries vitrio needs are fetched and built with it,
static, inside its own library:

- Boost (Filesystem, Interprocess and ContainerHash)
- spdlog
- libtiff, and zlib for it, unless TIFF is turned off, which leaves EER out
  too
- liburing, on Linux, unless io_uring is turned off
- Catch2 and trompeloeil, for the tests

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

To use the libraries installed on the system instead, turn
`VITRIO_BUILD_DEPENDENCIES` off. CMake then looks for each of them with
`find_package`, or pkg-config for liburing, and CMake 3.18 is enough. They
have to be Boost 1.70, libtiff 4.5, spdlog 1.4, liburing 2.0 and Catch2 3 or
newer. One of them alone is found rather than built by turning its own option
off, such as `-DVITRIO_BUILD_TIFF=OFF`. The others are `VITRIO_BUILD_BOOST`,
`VITRIO_BUILD_SPDLOG`, `VITRIO_BUILD_ZLIB`, `VITRIO_BUILD_LIBURING`,
`VITRIO_BUILD_CATCH2` and `VITRIO_BUILD_TROMPELOEIL`.

Without access to the network, give each library that is built the
directory of its sources instead, such as
`-DFETCHCONTENT_SOURCE_DIR_BOOST=<dir>`.

A few options change what gets built:

| Option | Effect |
|---|---|
| `-DVITRIO_BUILD_DEPENDENCIES=OFF` | Find the libraries vitrio needs installed, rather than fetching and building them |
| `-DVITRIO_BUILD_TESTING=OFF` | Skip the tests, and with them Catch2 and trompeloeil |
| `-DVITRIO_ENABLE_TIFF=OFF` | Leave TIFF and EER out, and libtiff with them |
| `-DVITRIO_ENABLE_IO_URING=OFF` | On Linux, leave io_uring out, and liburing with it |
| `-DVITRIO_BUILD_PYTHON=ON` | Build the Python package too. Needs Python 3.10 or newer with nanobind installed |
| `-DVITRIO_BUILD_EXAMPLES=ON` | Build the C++ examples of `examples/cpp`, which CTest runs |
| `-DVITRIO_BUILD_DOCS=ON` | Add the target `vitrio-docs`, which builds the API documentation with Doxygen, and with `VITRIO_BUILD_PYTHON` the target `vitrio-python-docs`, which builds that of the Python package with Sphinx |

With `VITRIO_BUILD_PYTHON` the package ends up under `python/` in the build
tree, so putting that directory on `PYTHONPATH` is enough to import it.
`pip install .` builds and installs it in one go. It is given the options
above as `-C cmake.define.<option>=<value>`, such as
`-C cmake.define.VITRIO_BUILD_DEPENDENCIES=OFF`.

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](https://github.com/gigabit-clowns/vitrio/blob/main/LICENSE).
