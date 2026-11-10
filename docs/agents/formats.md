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
