<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# vitrio

vitrio is an image I/O library for Cryo-Electron Microscopy (cryo-EM and
cryo-ET). This is the reference of its C++ library. The
[README](https://github.com/gigabit-clowns/vitrio#readme) introduces the
library, and the Python package has a reference of its own at
[python/](https://gigabit-clowns.github.io/vitrio/python/).

```cmake
find_package(vitrio REQUIRED)
target_link_libraries(my_program PRIVATE vitrio::vitrio)
```

The headers under `include/vitrio/` are the whole API, and each one
documents what it declares.

## Guides

- [Performance guide](../guides/performance.md): which functions to call,
  and how to set them up for your storage.
- [Supported formats](../guides/formats.md): the data types, the compression
  and the metadata of each format.
- [Building from source](../guides/building.md): the dependencies and the
  build options.

## Reading and writing one file

vitrio::read and vitrio::write_single are the simplest calls. Each one opens
a file and waits for it, so they suit a program that handles a single file.

\include read_and_convert.cpp

## Reading many images

A program that goes through many images reads them in batches with
vitrio::read_batch_async or vitrio::read_patches_async, and starts the next
batch before it uses the current one. vitrio::write_batch_async and
vitrio::write_patches_async do the same for writes.

\include read_batches.cpp

## More

- vitrio::detector_event_image_reader_provider reads the events of an EER
  file as a movie.
- vitrio::image_file_read_format_selector::register_format registers a
  format of your own.
