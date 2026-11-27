<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Performance guide

This guide helps you choose how to call vitrio and how to set it up for your
storage. It uses the names of the Python package. The C++ functions have the
same names in the namespace `vitrio`, apart from `vitrio.data`, which exists
only in Python.

The advice comes from measurements on one workstation, made with the
harness under
[benchmarks/](https://github.com/gigabit-clowns/vitrio/blob/main/benchmarks/README.md).
Measure on your own storage when it matters.

## TLDR

1. If your program goes through a dataset in batches, shuffled or in order,
   use `vitrio.data`. Training a model is the usual case.
2. If it does anything else with many images, use the low-level async functions:
   `read_batch_async`, `read_patches_async`, `write_batch_async` and
   `write_patches_async`. Make one call per batch, and keep several batches
   in flight while you work on the current one.
3. Keep `read`, `write_single`, `write_stack` and `write` for single files,
   outside the parts of your program where speed matters.
4. Create one loader and reuse it. Give it a `cache` of open files.
5. If the data sits on hard disks or on network storage and you read it more
   than once, put a scratch on a local SSD or RAM.

The rest of this guide explains each point.

## The three ways to read and write

### `vitrio.data`: the simplest way to go through a dataset

A `Dataset` describes rows: images, patches cropped out of larger images,
and the metadata that goes with them. A `DataLoader` goes through the rows
in batches, in order or shuffled. It reads the next batches in the
background and hands each one out as NumPy arrays, PyTorch tensors or JAX
arrays.

```python
import numpy as np
import vitrio as vio

dataset = vio.data.Dataset({
    "image": vio.data.ImageSource(locations, data_type=np.float32),
    "label": labels,
})
batches = vio.data.DataLoader(
    dataset, batch_size=128, shuffle=True, output="torch", device="cuda"
)
for epoch in range(epochs):
    for batch in batches:
        train_step(batch["image"], batch["label"])
```

Use it whenever your program iterates over a dataset in batches, whether
they are particles, patches of micrographs or subtomograms. Training a
neural network is the best-known case, with validation and inference. A
classical refinement that visits every particle in each iteration fits too.
So does an embarrassingly parallel job that applies one operation to every
image: that is a single epoch in order, with no shuffling. You get the
batching and the reading ahead of the next section without writing them.

It costs little. In our measurements it kept up with a hand-written loop
over the batch functions when the images came from storage. It fell behind
only when every image was already in the page cache, at rates that few
programs can use.

### The batch oriented asynchronous functions: the fastest way

`read_batch_async` reads one image per location into the slots of an array.
`read_patches_async` crops patches around a list of centres out of one
image. `write_batch_async` and `write_patches_async` are their mirrors. All
four return at once with a `Completion`, and the work goes on in the
background on the threads of a loader or a saver.

Use them for everything that `vitrio.data` does not cover: particle
extraction, preprocessing, motion correction, writing results while the next
ones are computed.

They are fast for two reasons. A call hands vitrio the whole batch, so it
can sort the requests by file, merge those that lie close together and keep
many of them in flight. And since the call returns at once, the storage
works while your program computes.

### `read` and the `write` functions: one file at a time

`read` opens a file, reads it and closes it before it returns.
`write_single`, `write_stack` and `write` create a file, write it and wait
until it has reached the storage. They are the right choice for a single
micrograph or volume in a script or a notebook, or for a tool that converts
one file.

Do not call them once per image in a loop. Each call pays for opening the
file again, and nothing overlaps: the storage waits while you compute, and
you wait while it reads. On our NVMe drive, one call per particle was more
than ten times slower than one call per batch. It was also slower than
indexing a NumPy memory map.

## Getting the most out of the batch functions

### Make one call per batch

Put every image you need next into one call, even when they come from many
files. A batch of a few hundred particles or a few micrographs is a good
size. Larger batches take more memory and bring little more.

For patches, group the centres by image and make one `read_patches_async`
call per image. Extraction programs do this naturally by going through the
picks micrograph by micrograph.

### Keep several batches in flight

Start reading the next batches before you work on the current one. Keep
several of them in flight, since one is often too few. Four is a good start,
and it is what `vitrio.data` does. In our measurements the first batch read
ahead removed most of the time a loop spent waiting for data, and a few more
removed a little of the rest.

The cost of a deeper queue is memory, because every batch in flight holds
its array. The Tuning section at the end says when to go deeper.

```python
from collections import deque

import numpy as np
import vitrio as vio

def read_ahead(loader, batches, shape, ahead=4):
    """Yield the images of each batch, with the next batches under way."""
    pending = deque()
    for locations in batches:
        images = np.empty((len(locations), *shape), np.float32)
        reading = vio.read_batch_async(loader, images, locations)
        pending.append((images, reading))
        if len(pending) > ahead:
            images, reading = pending.popleft()
            reading.get()    # waits, and raises if a read failed
            yield images
    for images, reading in pending:
        reading.get()
        yield images

loader = vio.loader(cache=64)
for images in read_ahead(loader, batches, shape=(128, 128)):
    process(images)
```

Each batch in flight needs an array of its own. Leave an array alone until
its completion is ready.

### Read into the array you will use

The destination array decides the data type. vitrio converts the values as
it reads them, so a file of 16-bit integers lands directly in a `float32`
array. Allocate the array with the data type and the memory you want to work
with, such as pinned memory for a GPU, and you save a copy.

### Create one loader and reuse it

`vio.loader()` starts a thread pool sized to the machine. Create it once and
pass it to every read. A loader created inside a loop starts and stops its
threads on every pass.

Give the loader a `cache`, which is the number of files it keeps open
between reads. Without it, every call opens its files again, and opening a
stack costs more than reading a particle from it. A value at least as large
as the number of files a batch touches is a good start. `vitrio.data` uses
64 by default.

If your program reads and writes, create one `ThreadPoolExecutor` and give
it to both the loader and the saver, so that they share its threads.

### Write in batches, and finish files in the background

Writes follow the same pattern. Declare each file with its final shape on a
writer provider, write to it in batches through a saver, and finish it with
`close_async`. Finishing a file waits for the storage, so let a few files
finish in the background while you write the next ones.

```python
executor = vio.ThreadPoolExecutor(8)
writers = vio.writer_provider()
saver = vio.saver(writers, executor=executor)

# A stack of 1000 images of 128 by 128, written a batch at a time.
writers.declare(
    "particles.mrcs",
    vio.ImageDescriptor((1000, 128, 128), 2, vio.NumericalType("float32")),
)
destinations = [vio.ImageLocation("particles.mrcs", i) for i in range(256)]
writing = vio.write_batch_async(saver, images, destinations)
...                  # compute the next batch meanwhile
writing.get()        # raises if a write failed

closing = writers.close_async("particles.mrcs")
...                  # write other files meanwhile
closing.get()        # the file has reached the storage
```

[extract_particles.py](https://github.com/gigabit-clowns/vitrio/blob/main/examples/python/advanced/extract_particles.py)
is a complete program built this way. It crops particles out of micrographs
and stores them in stacks, with reads and writes in flight at the same time.

## Getting the most out of `vitrio.data`

The defaults suit most programs. These are the settings worth knowing:

- `prefetch` is the number of batches under way at once. The default is 4.
  Raise it when the batches are small, as the Tuning section explains. Lower
  it only when the memory of the batches under way matters.
- `output="torch"` with `device="cuda"` reads each batch into pinned memory
  and copies it to the GPU without blocking. With `output="jax"` the batch
  goes to a JAX device.
- `transform` runs on each batch after it has reached the device. Normalize
  and augment there, on the GPU, instead of on each image on the CPU.
- `loader` takes a loader of your own, such as
  `vio.loader(workers=8, cache=256, scratch=scratch)`. Without it, all the
  datasets of a process share one loader.

`DataLoader` starts no worker processes. Its reads run on threads and
release the GIL. It does not need the worker processes of PyTorch's own
`DataLoader`. If you use them anyway, each worker starts a thread pool of
its own.

## Reading the same images many times

A training run or a refinement reads the same particles once per epoch.
After the first epoch the operating system serves them from its page cache, if they fit in
memory. Two tools help when that is not enough.

`dataset.preload()` reads everything into memory once, in the order of the
rows. It is the simplest choice when the dataset fits in RAM. It also suits
hard disks when the rows are listed in the order of the files, because a
pass in order is several times faster there than a shuffled one.

A scratch is a local copy that fills as images are first read. It can live
in RAM, in a file, or in a directory that vitrio keeps within a size budget.
A scratch in a file can be larger than RAM. It survives the program, so the
next run starts from it, and several programs can share it.

```python
scratch = vio.create_file_image_scratch(
    locations, "/local/nvme/particles.scratch", run_length=64
)
loader = vio.loader(cache=64, scratch=scratch)
batches = vio.data.DataLoader(dataset, batch_size=128, loader=loader)
```

Use a scratch when the data sits on storage slower than a local SSD and you
read it more than once. On our array of hard disks, a short training run was
several times faster through a file scratch on an NVMe drive than read
directly. The scratch also keeps later epochs fast when the dataset does not
fit in memory.

A scratch does not help in every case:

- Put it on storage faster than the source. A scratch on the same disk as
  the data only adds work.
- Data that already sits on an NVMe drive and fits in the page cache gains
  nothing. A scratch in RAM was slower than no scratch in that case.
- Over a slow network the first epoch still has to bring every image across.
  The scratch pays off in the next run, and in later epochs when the dataset
  does not fit in the page cache.

`run_length` is the number of images of a file that the scratch takes in
together, in one read. Longer runs suit hard disks. We measured with 64.

## Storage

### NVMe and other SSDs

A fast drive answers many requests at once, and vitrio keeps many in flight.
Batches and reading ahead are all it takes to use the drive fully. On our
NVMe drive, shuffled batches of particles arrived at the rate that `fio`
measures for the drive.

### Hard disks

Hard disks are slow at random reads, whoever asks. vitrio sorts and merges
the reads of a batch, which made shuffled reads several times faster than a
memory map in our measurements. A shuffled first epoch is still far below the
sequential rate of the disks. If you can, read the data in the order of the
files once, with `dataset.preload()` or through a scratch on an SSD.

### Network file systems

Over a slow link the network is the limit. On NFS over 1 Gb Ethernet vitrio
filled the link, and a memory map came close. A scratch on a local drive
makes sure the data crosses the network only once.

### Containers and clusters

On Linux, vitrio reads and writes MRC, DigitalMicrograph and ZARR files
through io_uring. Some systems do not allow it. Docker blocks it in its
default seccomp profile, and some enterprise kernels ship with it disabled,
which the sysctl `kernel.io_uring_disabled` shows. vitrio then falls back to
memory maps and ordinary writes. Nothing needs to be configured, and all of
this guide still applies. In our measurements the fallback was somewhat
slower on cold reads, and never by a large factor.

To compare the two on your machine, set the environment variable
`VITRIO_DISABLE_ASYNCHRONOUS_IO=1`, which forces the fallback.

## Files and data types

- Smaller data types help where the storage is the limit. Particles stored
  as `float16` take half the bytes of `float32`, and vitrio converts them
  to `float32` as it reads. They arrive faster in proportion.
- A single large stack and a stack per micrograph read at similar rates from
  cold storage. With many files, set `cache` so that they stay open.
- A slice of a volume costs what it touches of the file. A slice along the
  first axis is one contiguous plane. A slice across the last axis touches a
  few bytes of every row of the volume, and no reader makes that cheap. If
  you need many of those, read the volume once and slice it in memory.
- A ZARR array is read a chunk at a time, and a chunk is always decoded
  whole. Reading a whole array is fast, because its chunks are decoded in
  parallel. Many small random patches out of large chunks are not, so choose
  chunks close to the size of what you will read.
- An MRC file compressed with gzip has to be inflated from its start. Read
  such a file once, whole.

## What vitrio does not speed up

- A program limited by its computation. If the GPU is busy all the time,
  faster reads change nothing.
- A single large file read or written in one go. The storage is the limit
  either way.
- Storage that is already used fully, such as a slow network link.

To find out which one limits you, compare what your program reads per second
with what the storage can deliver, as `fio` measures it.

## Tuning

### How many batches to keep in flight

Prefer a deep queue. The storage can only work on what it has been asked
for, and a request that waits its turn costs nothing but the memory of its
array. vitrio limits how many requests it hands to the kernel at once, so a
deep queue of batches does not flood the storage. Unless a measurement on
your own storage says otherwise, keep too many batches in flight rather than
too few.

How deep to go depends on how much one batch asks of the storage:

- A batch of a few hundred particles spread over many files keeps the
  storage busy on its own. Four batches in flight are enough to cover the
  time between them. In our training loop on an NVMe drive, a deeper queue
  did no better.
- A small batch asks for little. The patches of one micrograph, or a few
  images of one file, become a handful of reads. Keep eight or more of these
  in flight, so that several files are read at once.
  [extract_particles.py](https://github.com/gigabit-clowns/vitrio/blob/main/examples/python/advanced/extract_particles.py)
  keeps eight reads and eight writes under way.
- Finishing a file waits for the storage, so let many files finish at once.
  When we wrote small stacks to an NVMe drive, the write rate kept rising
  up to 16 files finishing at a time.

Go shallower when a single batch is large. A movie or a tomogram takes
gigabytes, and one of them ahead is usually all the memory allows.

These two measurements are the only ones we have on queue depth so far.
Where it matters, try a few depths on your own storage.

### Threads

The defaults come from the benchmarks and are meant to work without tuning.
The thread counts are arguments: `workers` of `vio.loader` and `vio.saver`,
and the size of a `ThreadPoolExecutor`. They default to the number of
processors.

### Internal sizes

A few internal sizes can be overridden through environment variables, such
as the widest gap between two regions that vitrio reads through instead of
skipping.
[src/config.hpp](https://github.com/gigabit-clowns/vitrio/blob/main/src/config.hpp)
lists them with the reasoning behind each default. They are not part of the
API and may change between releases.
