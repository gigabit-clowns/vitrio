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
they need. On any other platform pip builds the package from source, and
then the dependencies listed under "Building from source" below have to be
installed first.

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

Images a program goes back to, epoch after epoch, are read faster from a
scratch: a copy of them in RAM. A loader made with one reads what the scratch
holds from it and takes in what it does not hold yet, and a prefetch fills it
ahead of the reads:

```python
scratch = vio.create_host_image_scratch(locations, run_length=64)
loader = vio.loader(cache=len(STACKS), scratch=scratch)

prefetcher = vio.ThreadPoolExecutor(2)    # keep it while it works
filling = vio.prefetch_scratch_async(scratch, locations, prefetcher)
```

`run_length` is how many images of a file are taken in together, in one read
of it. When the images do not fit in RAM, or the next run should not read
them again, the copy goes in a file mapped into memory instead:

```python
scratch = vio.create_mapped_file_image_scratch(
    locations, "/local/disk/particles.scratch", run_length=64
)
```

A later scratch of the same images over the same file resumes from what is
already in it, except for the images whose files have been modified since,
and programs that hold the same images can share the file. vitrio never
removes it, so remove it yourself once nothing uses it.

To leave the files to vitrio instead, keep them in a directory with a
budget. Each scratch gets a file of its own there, found again by the next
scratch of the same images, and the files used least recently are removed
to make room for new ones:

```python
scratch = vio.create_directory_image_scratch(
    locations, "/local/disk/vitrio", run_length=64, budget=100 * 2**30
)
```

Only the files vitrio puts there count against the budget and are ever
removed. The disk needs room for the whole budget, since the files take
their space as images are taken in.

A movie of an electron-counting detector, such as an EER file, holds the
events the detector counted rather than images. It is read through a
provider that cuts its frames into as many fractions as you ask for, as
evenly as they allow, and counts the events of each fraction per pixel:

```python
readers = vio.detector_event_image_reader_provider(40, binning=2)
movie = vio.read("movie.eer", readers, data_type=np.float32)    # 40 fractions

# Or the events themselves, a group per frame, on the grid of subpixels
events = vio.read_events("movie.eer", 0, 10)
frames = np.asarray(events.timestamps)
positions = np.asarray(events.get_positions(0))    # (events, 2): y, x
```

## Feeding a model

`vitrio.data` reads a dataset for training in batches. A `Dataset` is rows
of named fields: images read from files, and whatever metadata goes with
them, such as the CTF, the pose or the class of each particle. vitrio
parses no metadata file itself. It takes what you parsed it into, here with
[starfile](https://github.com/teamtomo/starfile):

```python
import numpy as np
import starfile
import vitrio as vio

particles = starfile.read("run_data.star", always_dict=True)["particles"]
dataset = vio.data.Dataset({
    "image": vio.data.ImageSource(
        vio.data.parse_locations(particles["rlnImageName"]),
        data_type=np.float32,
    ),
    "ctf": particles[["rlnDefocusU", "rlnDefocusV", "rlnDefocusAngle"]]
        .to_numpy(np.float32),
    "label": particles["rlnClassNumber"].to_numpy() - 1,
})

loader = vio.data.DataLoader(
    dataset, batch_size=128, shuffle=True, output="torch", device="cuda"
)
for epoch in range(10):
    for batch in loader:
        batch["image"], batch["ctf"], batch["label"], batch["index"]
```

A batch is a dict of arrays, one for each field, plus the index of each
row. The loader keeps a few batches in flight on a thread pool and reads
them straight into pinned memory when the batch is going to a GPU. Use
`output="numpy"` for plain arrays.

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

[examples/classify_particles.py](https://github.com/gigabit-clowns/vitrio/blob/main/examples/classify_particles.py)
trains a small network of PyTorch on the 2D classes of a RELION job.

## Examples

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

An EER file holds the events a detector counted rather than images, so it is
read through a provider of its own. It is given how many fractions to cut the
movie into, as evenly as the frames allow, and how to render each fraction,
such as how many pixels of the detector to bin into one:

```cpp
#include <vitrio/counting_detector_event_renderer.hpp>
#include <vitrio/detector_event_binning.hpp>
#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/detector_event_image_reader_provider.hpp>
#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/file_detector_event_timeline_reader_provider.hpp>
#include <vitrio/image_read.hpp>

#include <memory>

int main()
{
	vitrio::detector_event_image_reader_provider readers(
		std::make_shared<vitrio::file_detector_event_timeline_reader_provider>(
			vitrio::detector_event_timeline_file_read_format_selector::
				get_shared()
		),
		vitrio::detector_event_fractionation(40),
		// Blocks of 2 by 2 pixels of the detector make one of the image.
		std::make_shared<vitrio::counting_detector_event_renderer>(
			vitrio::detector_event_binning(2)
		)
	);

	// 40 fractions of 2048 by 2048 counts.
	vitrio::array movie = vitrio::read("movie.eer", readers);
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
- libtiff 4.5 or newer, unless TIFF is turned off, which leaves EER out too
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
| `-DVITRIO_ENABLE_TIFF=OFF` | Leave TIFF and EER out, and libtiff with them |
| `-DVITRIO_BUILD_PYTHON=ON` | Build the Python package too. Needs Python 3.10 or newer with nanobind installed |
| `-DVITRIO_BUILD_DOCS=ON` | Add the target `vitrio-docs`, which builds the API documentation with Doxygen |

With `VITRIO_BUILD_PYTHON` the package ends up under `python/` in the build
tree, so putting that directory on `PYTHONPATH` is enough to import it.
`pip install .` builds and installs it in one go.

## Licence

vitrio is distributed under the GNU Lesser General Public License, version 2.1
or later. See [LICENSE](https://github.com/gigabit-clowns/vitrio/blob/main/LICENSE).
