<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Supported formats

The same calls read and write every format. When vitrio reads a file it
recognizes the format from the contents of the file where the format allows
it, and from the extension otherwise. When it writes a file it takes the
format from the extension.

| Format | Extensions | Read | Write |
|---|---|---|---|
| MRC | `.mrc`, `.mrcs`, `.map`, `.st`, `.rec`, `.ali`, also compressed with gzip, such as `.map.gz` | Yes | `.mrc`, `.mrcs` and `.map`, uncompressed |
| TIFF | `.tif`, `.tiff` | Classic and BigTIFF | Classic and BigTIFF |
| EER | `.eer` | As electron events, or counted into a movie | No |
| DigitalMicrograph | `.dm3`, `.dm4` | Yes | No |
| ZARR | A directory | Versions 2 and 3, OME-Zarr 0.4 and 0.5 | Version 3, OME-Zarr 0.5 |

A build can leave TIFF, ZARR or gzip out. The
[build guide](building.md)
lists the options. The wheels on PyPI include every format.

## MRC

An MRC file holds an image, a volume, or a stack of either.

- Data types: 8- and 16-bit integers, 16- and 32-bit floats, and 64-bit
  complex numbers.
- Files in either byte order are read.
- `.mrc`, `.mrcs` and `.map` files are written. The other extensions are
  only read.
- A file compressed with gzip, such as the `.map.gz` of a map in the EMDB,
  is read but never written. Reading any part of it inflates it from its
  start, so read such a file once, whole.
- Metadata: the pixel size along each axis and the labels of the file. Both
  are read, and written when a file is created.

## TIFF

A TIFF file holds an image, or a stack with one image per page.

- Data types: 8- to 64-bit integers, 16- to 64-bit floats, and 64- and
  128-bit complex numbers.
- Classic TIFF and BigTIFF files are read. vitrio writes a classic TIFF file
  and switches to BigTIFF when the images would not fit in one.
- Metadata: the pixel size that the resolution of the file states, its
  description, its software and its date. All of them are read and written.

## EER

An EER file holds the electron events that a detector counted, not images.

- The compression schemes 65000, 65001 and 65002 are read.
- The events can be read as they are, one stretch of time after another.
  [`read_events`](https://gigabit-clowns.github.io/vitrio/python/detector_events.html)
  does this in Python.
- The events can also be counted into a movie. You choose the number of
  fractions, and optionally a binning. The file is then read like any other
  stack of images, through a reader provider made by
  `detector_event_image_reader_provider`.
- EER files are not written.

```python
import numpy as np
import vitrio as vio

readers = vio.detector_event_image_reader_provider(40, binning=2)
movie = vio.read("movie.eer", readers, data_type=np.float32)    # 40 fractions
```

## DigitalMicrograph

A DigitalMicrograph file holds an image or a stack.

- Versions 3 and 4 are read, in either byte order. Only the first image of
  a file is read.
- Data types: 8- to 32-bit integers, 32- and 64-bit floats, and 64- and
  128-bit complex numbers. RGB and packed complex data are not read.
- Metadata: the pixel size that the calibration of the file states.
- DigitalMicrograph files are not written.

## ZARR

A ZARR array is a directory. It holds an image, a volume, or a stack of
either. vitrio recognizes it by the metadata in the directory, so the
directory can have any name when it is read. A directory that vitrio writes
is named `.zarr`.

- Data types: booleans, 8- to 64-bit integers, 16- to 64-bit floats, and
  64- and 128-bit complex numbers, in either byte order.
- Versions 2 and 3 are read, and so are images of OME-Zarr 0.4 and 0.5.
  Chunks compressed with blosc, zstd, zlib or gzip are read, alone or in
  shards.
- Version 3 is written, with the metadata of OME-Zarr 0.5. Chunks are
  compressed with blosc and zstd, and never written in shards.
- Metadata: the pixel size that the scale of an OME-Zarr image states, read
  and written.

## Metadata

What a file states beyond its pixels, its shape and its data type is its
metadata. In Python it is a dict, and in C++ an `image_metadata`. Reading it
does not read the pixels.

```python
metadata = vio.read_metadata("map.mrc")
pixel_size = metadata.get("pixel_size_x")    # in ångströms, or None

vio.write_single(volume, "copy.mrc", metadata=metadata)
```

A label without a dot is a standard field, which means the same in every
format:

- `pixel_size_x`, `pixel_size_y` and `pixel_size_z`: the distance between
  two samples along that axis, in ångströms.

Any other label starts with the name of the format that defines it:

- `mrc.labels`: the labels of an MRC file, one per line.
- `tiff.image_description`, `tiff.software` and `tiff.date_time`: the
  matching tags of a TIFF file.

A field that the file does not state is absent from the dict. When a file is
written, its format keeps the fields it can carry and leaves out the rest. A
file created without a pixel size states none.
