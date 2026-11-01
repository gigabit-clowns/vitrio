# Transfers

How reads, writes, flushes and the loads of a scratch are started, reported and carried out.

- **Transfers.** `image_reader::read_async`, `image_writer::write_async` and
  `image_writer::flush_async` start a transfer and report it to a
  `completion_notifier` exactly once: a failure is reported, never thrown,
  and only a null notifier throws. A transfer keeps the owning `array` or
  `const_array` it is given until it reports.
  - A reader or writer that works on the calling thread derives from
    `synchronous_image_reader` or `synchronous_image_writer` and implements
    the plain `read`, or `write` and `flush`, which throw.
  - Code whose own contract is to be done before it returns waits through
    `src/waiting_transfers.hpp`. Holding only a reference, it lends it to the
    transfer with `borrow_array`, from `src/array/borrowed_array.hpp`.
  - Several transfers report as one through a `counting_notifier`. A task
    that starts a transfer is submitted with a `failure_forwarding_notifier`
    (`src/concurrency/`), so that the transfer reports its outcome and the
    task only its failure.
  - Files are read and written through a `file_transfer_pool`, which a file
    format is given when it opens or creates a file, and whose executor the
    reader or writer keeps. Only what is about files takes one:
    `file_image_reader_provider` and `file_image_writer_provider` hold it,
    so that `acquire` stays a key alone.
  - A write covers the runs of bytes its regions take in the file and
    nothing between them, which a concurrent write of other regions may be
    writing: it never bridges a gap, nor reads what it writes around.
  - Closing a file waits for its flush, which reaches the storage, so a
    program that closes its files one after another waits for each in turn.
    `file_image_writer_provider::close_async` finishes a file without
    waiting, and reports once it is finished; a few under way at once, while
    the next files are written, keep the storage busy, as
    `examples/python/advanced/extract_particles.py` does.
  - A scratch takes in what it is offered as a transfer:
    `image_scratch_entry::store_async` reports the same way. A run is read
    from the file once, and a store that needs a run already being taken in
    waits for it. The lock of an entry guards only which runs are absent,
    being taken in or held, never a read. What a reader copies out of an
    entry once it has taken that in is a task on the executor the reader
    was given.
  - A scratch keeps its copies through the operations of its storage,
    never through a pointer: it serves values and loads them as transfers,
    and reads and writes its flags and fingerprint on the calling thread
    when it is created. The flag of a run it has taken in is written as a
    transfer too, since the load reports on a thread that carries out
    transfers, which a write that blocks would hold up.
    Storage in memory serves with one strided copy and loads straight into
    memory. Storage in a file serves through the read operation of the
    queued reader, and loads through staging and the write operation of the
    queued writer, so a full disk is a reported failure. Its loads go in
    pieces of a step of staging, and pieces wait, in the order they came,
    while `VITRIO_DEFAULT_SCRATCH_LOAD_BYTE_BUDGET` is staged: a shuffled
    batch starts a load for nearly every run, and staging them all at once
    held most of a dataset in memory, while hard disks under ZFS need about
    64 MiB in flight. The staging is memory straight from the system,
    `allocate_system_memory`, since the arenas of the allocator kept
    several times the budget resident. A load may start after it is asked
    for, so it keeps the reader it was given, as `store_async` does.
  - The executor of a `file_transfer_pool`, and a `thread_pool_executor`,
    may be let go last on one of their own threads, when a transfer or a
    task held what held them, as a load holds a file storage. They are then
    destroyed on a thread of their own, since a thread can not wait for
    itself to end.
  - Work that waits for the tasks of an executor says so, and must not run
    on a thread of it. Creating an `indexed_image_scratch` opens its files
    that way, all at once.
