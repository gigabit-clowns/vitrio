<p align="center">
<picture>
<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/gigabit-clowns/vitrio/main/docs/logo/github/readme-banner-dark.svg">
<img alt="vitrio, high-throughput cryo-EM image I/O for C++ and Python" src="https://raw.githubusercontent.com/gigabit-clowns/vitrio/main/docs/logo/github/readme-banner-light.svg" width="800">
</picture>
</p>

# vitrio

[![PyPI](https://img.shields.io/pypi/v/vitrio)](https://pypi.org/project/vitrio/)
[![Build and test](https://github.com/gigabit-clowns/vitrio/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/gigabit-clowns/vitrio/actions/workflows/build-and-test.yml)
[![Documentation](https://img.shields.io/badge/docs-gigabit--clowns.github.io%2Fvitrio-0b5cad)](https://gigabit-clowns.github.io/vitrio/)

vitrio is an image I/O library for Cryo-Electron Microscopy (cryo-EM and
cryo-ET). It reads and writes micrographs, movies, particle stacks, volumes
and tomograms in MRC, TIFF, EER, DigitalMicrograph and ZARR files. It is a
C++14 library with a Python package on top.

It takes care of the parts of image I/O that every program otherwise writes
again. The same calls work on every format, so your code does not need a
reader for each one or a branch to tell them apart. You name what you want,
such as one particle of a stack, a patch of a micrograph or an EER movie
counted into frames, and vitrio reads that part in the data type you ask
for. The pixels land in memory you already have, such as a NumPy array or a
PyTorch or JAX tensor. When the data sits on slow storage, vitrio can keep a
copy of it on a fast local disk. It fills the copy as images are first read
and serves later reads from it, while the client code that reads stays untouched.

It is also very fast for programs that go through many images, such as particle
extraction, processing pipelines and the training of neural networks.
Reading one image at a time uses only a fraction of what the storage can
deliver. With vitrio you ask for a whole batch in one call, the call returns
at once, and the batch is read in the background while your program keeps
working.

```python
import vitrio as vio

micrograph = vio.read("micrograph.mrc")
particle = vio.read(vio.ImageLocation.from_string("12@particles.mrcs"))
tomogram = vio.read("tomogram.zarr")
```

vitrio is young. Its API and ABI may change between minor releases until
version 1.0.

## What it offers

Work that vitrio takes off your hands:

- One interface for every format. The same calls read and write MRC, TIFF,
  ZARR and the rest, and vitrio recognizes the format from the file. A
  program written for MRC stacks reads TIFF or ZARR without a change.
- Any part of a file. You can ask for a whole file, one image of a stack, a
  patch around a picked particle or a slice of a volume. vitrio reads only
  the bytes that the request needs.
- Any array. Pixels are read into any memory shared through DLPack or the
  buffer protocol, without a copy, and converted on the way to the data type
  you ask for. A file of 16-bit integers can land in a `float32` tensor.
- Metadata under the same names. The pixel size is `pixel_size_x` whether it
  comes from an MRC header, a TIFF tag or an OME-Zarr scale.
- EER movies as ordinary movies. You say how many fractions you want and
  vitrio counts the electron events into them.
- A scratch behind the reads. vitrio can keep a local copy of images that
  are read again and again, in RAM or in a file on fast local storage. It
  fills the copy as the images are first read and serves them from there
  afterwards. You ask for it once, when the loader is created, and the reads
  stay the same.
- A data loader for training. `vitrio.data` feeds images and their metadata
  to PyTorch or JAX in shuffled batches, with no worker processes.
- Room for your own formats. In C++, a new format is a class that you
  register at run time, and every read and write can use it right away.

What makes it fast:

- A read or a write takes a whole batch, from any number of files. vitrio
  sorts the requests and merges those that lie close together.
- Reads and writes can be asynchronous, so the next batch loads while you
  work on the current one. On Linux most formats go through io_uring when
  the kernel allows it.
- A batch bound for a GPU is read straight into pinned memory.

## How fast is it

We measured vitrio on one workstation, against the usual ways of reading
cryo-EM data from Python. Reading shuffled batches of particles from a cold
NVMe drive, it delivered about 2.7 GB/s, which is what `fio` measures for
that drive. `numpy.memmap` and mrcfile delivered about 0.6 GB/s. On an array
of hard disks the same read was about six times faster than a memory map.

We also moved the image I/O of three packages onto vitrio, in forks that are
not published yet. Topaz trained its denoiser 1.7 times faster than upstream
at its fastest setting. IsoNet2 predicted a tomogram with 9 GB of memory
instead of 101 GB. cryoDRGN took 13% to 32% less time when training from
hard disks.

vitrio does not make everything faster:

- The gains come from batches and from reading ahead. A loop that calls
  `vio.read` once per particle offers no gain.
- A training run that is limited by the GPU takes the same time. That was
  the case for cryoDRGN on the NVMe drive.
- A slow network stays the limit. Over 1 Gb Ethernet vitrio filled the link,
  which a memory map nearly did too.

These are early results from a single machine. The harness that produced
them is in
[benchmarks/](https://github.com/gigabit-clowns/vitrio/blob/main/benchmarks/README.md),
so you can measure on the storage you care about.

## Installing

```
pip install vitrio
```

There are wheels for Linux (x86_64 and aarch64), macOS (Apple silicon and
Intel) and Windows (x86_64), for Python 3.10 and newer. On any other
platform pip builds the package from source, which needs a C++ compiler and
network access.

The C++ library is built from source for now. See
[Building from source](#building-from-source).

## Using it from Python

`read` returns an array that NumPy and PyTorch use without a copy. The
values are converted if you ask for another data type.

```python
import numpy as np
import torch
import vitrio as vio

micrograph = vio.read("micrograph.mrc", data_type=np.float32)
pixels = np.asarray(micrograph)           # the same memory, not a copy
tensor = torch.from_dlpack(micrograph)    # and again

vio.write_single(pixels, "copy.tif")
```

Metadata such as the pixel size is read as a dict, and can be given back
when a file is written.

```python
metadata = vio.read_metadata("map.mrc")
pixel_size = metadata.get("pixel_size_x")    # in ångströms, or None

vio.write_single(volume, "copy.mrc", metadata=metadata)
```

Many images are read as a batch. The call returns a completion at once, and
the batch arrives in an array you provide.

```python
loader = vio.loader()
locations = [vio.ImageLocation("particles.mrcs", i) for i in range(256)]
images = np.empty((256, 128, 128), np.float32)

reading = vio.read_batch_async(loader, images, locations)
...               # do something else while the batch is read
reading.get()     # wait for it. This raises if a read failed
```

To have vitrio keep a local copy of what it reads, give the loader a
scratch. The reads themselves do not change.

```python
scratch = vio.create_file_image_scratch(
    locations, "/local/nvme/particles.scratch", run_length=64
)
loader = vio.loader(scratch=scratch)

reading = vio.read_batch_async(loader, images, locations)    # as before
```

To train a model, `vitrio.data` does the batching and the reading ahead for
you. A `Dataset` is rows of named fields: images read from files, and any
metadata that goes with them. vitrio parses no metadata file itself. It
takes what you already parsed, such as the columns of a DataFrame from
[starfile](https://github.com/teamtomo/starfile).

```python
dataset = vio.data.Dataset({
    "image": vio.data.ImageSource(locations, data_type=np.float32),
    "label": labels,
})
loader = vio.data.DataLoader(
    dataset, batch_size=128, shuffle=True, output="torch", device="cuda"
)
for batch in loader:
    batch["image"], batch["label"], batch["index"]
```

## Choosing how to read and write

vitrio has three ways in. Which one you pick decides how fast your program
runs, more than any setting does.

| Your program | Use | What to expect |
|---|---|---|
| Feeds a model with particles, patches or subtomograms | `vitrio.data.Dataset` and `DataLoader` | The simplest. It reads ahead for you, and is as fast as a hand-written loop when the storage is the limit |
| Does anything else with many images, such as extraction, preprocessing or writing results | `read_batch_async`, `read_patches_async`, `write_batch_async` and `write_patches_async` | The fastest. Make one call per batch, and keep the next batch in flight while you compute |
| Opens one file in a script or a notebook | `read`, `write_single`, `write_stack` and `write` | The simplest call. Each one opens a file and waits for it, so keep them out of loops over many images |

The
[performance guide](https://github.com/gigabit-clowns/vitrio/blob/main/docs/guides/performance.md)
explains each of them, with the settings that matter for NVMe drives, hard
disks, network storage and containers.

## Supported formats

| Format | Extensions | Read | Write |
|---|---|---|---|
| MRC | `.mrc`, `.mrcs`, `.map`, `.st`, `.rec`, `.ali`, also compressed with gzip, such as `.map.gz` | Yes | `.mrc`, `.mrcs` and `.map`, uncompressed |
| TIFF | `.tif`, `.tiff` | Classic and BigTIFF | Classic and BigTIFF |
| EER | `.eer` | As electron events, or counted into a movie | No |
| DigitalMicrograph | `.dm3`, `.dm4` | Yes | No |
| ZARR | A directory | Versions 2 and 3, OME-Zarr 0.4 and 0.5 | Version 3, OME-Zarr 0.5 |

The
[formats page](https://github.com/gigabit-clowns/vitrio/blob/main/docs/guides/formats.md)
lists the data types, the compression and the metadata that each format
supports. More formats will follow.

## Using it from C++

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

```cpp
#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>

#include <memory>

int main()
{
	// The threads that read and write files.
	const auto transfers = std::make_shared<vitrio::file_transfer_pool>(
		std::make_shared<vitrio::thread_pool_executor>(2)
	);
	vitrio::file_image_reader_provider readers(
		vitrio::image_file_read_format_selector::get_shared(),
		transfers
	);

	// Read as 32-bit floats, whatever the file holds.
	const vitrio::array image = vitrio::read(
		"micrograph.mrc", readers, vitrio::numerical_type::float32
	);

	vitrio::write_single(
		image, "copy.tif",
		*vitrio::image_file_write_format_selector::get_shared(),
		*transfers
	);
}
```

The headers under `include/vitrio/` are the whole API, and each one
documents what it declares. `read_batch_async` and its siblings are in
`image_read.hpp` and `image_write.hpp`, and the advice of the table above
holds for them too.

## Documentation

- [Python reference](https://gigabit-clowns.github.io/vitrio/python/), built
  from the docstrings that `help()` and your editor show.
- [Performance guide](https://github.com/gigabit-clowns/vitrio/blob/main/docs/guides/performance.md),
  on choosing an API and setting it up for your storage.
- [Supported formats](https://github.com/gigabit-clowns/vitrio/blob/main/docs/guides/formats.md),
  in detail.
- [C++ reference](https://gigabit-clowns.github.io/vitrio/cpp/), built from
  the public headers.
- [Examples](https://github.com/gigabit-clowns/vitrio/blob/main/examples/README.md):
  small programs you can run, such as training a PyTorch network on the 2D
  classes of a RELION job, extracting picked particles into stacks and
  cutting EER movies into fractions.

## Building from source

You need a C++14 compiler and CMake 3.24 or newer. The libraries vitrio
depends on are downloaded and built with it.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix <where>
```

Add `-DVITRIO_BUILD_PYTHON=ON` to build the Python package too, or run
`pip install .` to build and install it in one go.
[docs/guides/building.md](https://github.com/gigabit-clowns/vitrio/blob/main/docs/guides/building.md)
lists the dependencies and the build options. It also explains how to use
the libraries installed on your system and how to build without network
access.

## Citing vitrio

GitHub offers the citation under **Cite this repository**, as BibTeX or APA.
It comes from
[CITATION.cff](https://github.com/gigabit-clowns/vitrio/blob/main/CITATION.cff).

## Licence

vitrio is distributed under the GNU Lesser General Public License, version
2.1 or later. See
[LICENSE](https://github.com/gigabit-clowns/vitrio/blob/main/LICENSE).
