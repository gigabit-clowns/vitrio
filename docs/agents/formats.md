# Formats

Keys, providers and the roles of the file formats, detector events, ZARR and files compressed with gzip.

- **Keys and files.** A provider (`image_reader_provider`,
  `image_writer_provider`) is asked with a key, and what a key means is the
  provider's business. An `image_location` is a key and an index in a stack,
  and the plans, the loader and the saver carry keys too. What is about
  files says so in its name: `image_file_probe`, `image_file_error`,
  `image_file_format_error`, and three roles that are kept apart. A file
  may be a directory, as a ZARR array is: its probe has no leading bytes and
  says it is one, and a format of directories tells them by the entries
  they hold, without reading them. The roles are:
  - A file format (`image_file_read_format`, `image_file_write_format`, and
    for one kind of file `mrc_file_read_format`, `tiff_file_write_format`)
    knows one format, and registers itself at namespace scope with the
    macros of `src/formats/image_file_format_registration_macros.hpp`.
  - A selector (`image_file_read_format_selector`,
    `image_file_write_format_selector`) holds formats and opens a file with
    the most suitable of them. Its `get_shared()` holds the bundled ones.
  - The two providers whose keys are paths, `file_image_reader_provider`
    and `file_image_writer_provider`, are each given a selector and hold no
    formats themselves.
- **Metadata.** What a file states beyond its shape and data type is an
  `image_metadata`, and it is the business of the file formats alone: no
  reader and no writer has anything of it. A read format reads it when
  asked, with `read_metadata`, which opens no reader, and the read selector
  asks the format it would open the file with. A write format is given it
  with the descriptor when the file is created, which is when every format
  writes its header, and `file_image_writer_provider` only keeps what a
  file was declared with until then. Both are best effort: a format reads and
  writes what it can carry and leaves out the rest, so metadata never
  makes a creation fail. A format derives the axes its metadata is about
  with the code its `open` derives the descriptor with, so the two never
  disagree on whether a file has a `z`. A header field is under one label
  only, the standard field where there is one and otherwise a label
  prefixed with the format, such as `mrc.labels`.
- **Metadata of MRC.** The distance between the samples along an axis of
  space is the size of the cell along it over the sampling of the cell
  along it, and `MX`, `MY` and `MZ` are that sampling along X, Y and Z,
  whichever of them the columns run along, though `mrc_header` names them
  after the columns, the rows and the sections. `derive_core_space_axes`
  says which axis of space each axis of the descriptor runs along, for the
  layout and the metadata alike. A cell with no size along an axis states
  no distance there, which is how a file created with none is written: a
  cell of the extents would state an ångström. A file of images states
  nothing along the axis it stacks them on. The labels are one text,
  `mrc.labels`, a label a line, and a file is created with those it is
  given followed by that of the library, unless it is the last already.
- **Metadata of TIFF.** A page states how many of its pixels there are to
  a length, in a unit of its own, and `tiff_page_tags` has it to a
  centimetre whichever that is: a resolution with no unit stated is one to
  an inch, and one in no unit of length states nothing. The distance
  between the samples is a centimetre over it. It is written to a
  centimetre, since a resolution is a quotient of two integers of 32
  bits: one to a centimetre holds a pixel down to 0.024 Å, and one to an
  inch only down to 0.06 Å. `tiff.image_description`, `tiff.software` and
  `tiff.date_time` are the texts of the tags they are named after. A file
  is created with the name and the version of the library as its
  software, unless it is given one, as an MRC file is with a label. A file
  states what its first page does: every page is written with the
  resolutions and the first alone with the texts, so that a stack does
  not hold a long description once a page.
- **Metadata of ZARR.** Only an image of OME-Zarr states a distance
  between samples: the scale of a level along an axis of space, times the
  scale the image states for every level, in the unit of the axis. An axis
  with no unit states a scale and no distance, and so does one in a unit
  that is no length `zarr_metadata` knows. An array that is no level of an
  image states nothing. `locate_array` resolves a path into the array it
  names, its descriptor and the level of the image it is, for `open` and
  for `read_metadata` alike, so that the pixel sizes are those of the
  array that is opened, along the axes it is opened with: a tilt series,
  whose first axis is a stack, has no `z`. An image is written with a
  scale in ångströms along the axes a pixel size is given for, and at a
  scale of one and of no unit along the others. The core rank a store is
  written with is in its attributes, so no scale changes what it reads
  back as.
- **Metadata of DigitalMicrograph.** An image calibrates each of its axes
  in `ImageData`, `Calibrations`, `Dimension`, that of its columns first:
  a `Scale`, a real, and its `Units`, a text of 16-bit characters. The
  distance between the samples along x and y is the scale of the first
  and of the second, in units `dm_metadata` knows as a length. An axis
  with no units is not calibrated, and one in the units of a diffraction
  pattern, of a spectrum or of time states no distance. The micro and the
  ångström each have two characters that look alike, and both are known.
  A file is an image or a stack of images, so it has no `z`. The metadata
  is of the image `find_image` finds, which is the one that is opened. What
  an image lacks or holds as anything else is left out, and only tags that
  can not be parsed are refused. The tags an acquisition writes beside the
  image, under `ImageTags`, are not read.
- **Detector events.** A detector that counts electrons records events, not
  images. A `detector_event_timeline` holds the events of a stretch of time,
  in groups of one timestamp each, and `detector_event_position_view` views
  their positions as a matrix of a row per event. What is fixed for an
  acquisition, its grid and its time, is a
  `detector_event_timeline_descriptor`. Their files have the same three
  roles as those of images, `detector_event_timeline_file_read_format`, its
  selector and `file_detector_event_timeline_reader_provider`, and nothing
  of them is about one format: EER is one implementation, under
  `vitrio::eer`. An acquisition becomes images through
  `detector_event_image_reader_provider`, which is given the
  `detector_event_fractionation` and the `detector_event_renderer` to read
  it with; the counting one bins pixels of the detector by a
  `detector_event_binning`, stated in pixels rather than in quanta. That is chosen by whoever reads, not by the file, so it is
  stated where readers are made and never registered as a format of images.
- **ZARR.** A ZARR array is a directory, and every chunk a file of its own,
  or every shard of chunks, with an index of where each lies. The format
  opens no file until it reads: a read resolves its regions into the chunks
  they reach with `image_chunk_regions`, and its operation opens and reads
  each file, its index first for a shard, and decodes its chunks, a few
  files at a time within `VITRIO_DEFAULT_CHUNK_BYTE_BUDGET`. What a chunk
  is read into and decoded into are buffers of `zarr_chunk_buffers`, which
  the format keeps from one read to the next, as many bytes of them as that
  budget, since fresh memory for every chunk costs page faults and a fill
  of zeros that are most of what a cached chunk costs. Only the codecs that turn bytes into
  bytes are classes, `zarr_bytes_codec`: an order of C or F, a transpose and
  the order of the bytes are the strides and the byte order the strided
  transfer reads a chunk with. A chunk that is not stored is read as its fill
  value through strides of zero. Metadata that is not what ZARR states is an
  `image_file_format_error`, and what ZARR allows but the format does not
  read an `unsupported_operation_error`. A write holds the chunks it reaches
  in memory, in the order of the system, in `zarr_chunk_store`, and stores
  each once all of it in the array is written, or when what is held passes
  `VITRIO_DEFAULT_CHUNK_WRITE_BYTE_BUDGET`, or on a flush. A chunk is
  written beside its file and renamed over it, and the chunks stored are
  synced in batches of `VITRIO_DEFAULT_CHUNK_SYNC_BYTE_BUDGET`, each file
  in a task of its own, as the write that passes the budget finds them,
  and the rest by the flush: synced one by one, each paid the commit of
  the storage's log, which on ZFS is a transaction group, and synced at
  the flush alone, the storage sat idle until then and the flush waited
  for all of it at once. A chunk of the fill value
  alone is not stored, and one written again after it was stored is read
  back first. The store records the core rank under `vitrio` in its
  attributes, so that it reads back as it was written.
- **Files compressed with gzip.** gzip can only be inflated from its start,
  and such a file, as the EMDB holds its maps, is usually read once and
  dropped. So `gzip_mrc_file_read_format` opens a file by inflating its
  header alone, and each read of `gzip_image_reader` inflates the file again
  from its start through `gzip_file_stream`, up to the last value it needs,
  and keeps nothing. A file of several gzip members reads as one. What lies
  past the last value read is never inflated, so a broken end or a checksum
  that does not match goes unnoticed by a read that stops short of it.
