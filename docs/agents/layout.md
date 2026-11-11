# Layout

Where everything lives in the repository, and how sources are listed and tested.

| Path | Holds |
|---|---|
| `include/vitrio/` | The public headers. Everything here is part of the API |
| `include/vitrio/array/` | The array classes, their descriptor and the types of their elements |
| `include/vitrio/concurrency/` | Executors, the tasks they run and the completions that report them |
| `include/vitrio/exceptions/` | The exception types |
| `src/` | The implementation, plus the headers that are not public. Its directories are those of `include/vitrio/` |
| `src/formats/` | The image formats, each in a directory and a namespace of its own, such as `vitrio::mrc`, and in `raw/` what the formats whose values lie in the file as they are share |
| `src/formats/raw/` | What the formats whose values lie in the file as they are share. Such a format resolves its header into a `raw_image_layout`, opens its reader with `open_raw_image_reader`, which picks the reader that suits the system and the values: one that queues its reads through the `file_transfer_pool` it is given where the process can have asynchronous ports, and one over a mapping otherwise. It creates its files with `open_raw_image_writer`, whose writer queues its writes through the pool wherever it runs, since writes the system carries out as they are asked still beat writes through a mapping. A file being written is laid out in full first, by `create_image_file` |
| `src/formats/raw/planning/` | Gathers the regions of a transfer into the stretches of the file they reach, merged across small gaps, or only where they touch for a write, and grouped into steps of a bounded number of bytes, on a grid of pages or of elements. A write covers only the runs of bytes its regions take, which planning works out too. A read first splits a region whose parts lie farther apart than a read bridges, or whose parts and gaps together would outgrow a step, such as a slice across the planes of a volume, into parts read one at a time, so that a stretch never takes in everything between them. The rows of a patch of a micrograph stay one stretch: read one by one they cost a request each, which hard disks, and file systems that hand every read to a kernel thread, serve far slower than copying the gaps |
| `src/formats/raw/queued/` | Reads and writes a file whose values lie as they are through an executor of file transfers. A read or a write is planned on the thread that starts it into an operation: one that moves straight between the file and the array where nothing is converted, and otherwise one that stages its values in memory of its own a step at a time, as the planning groups the regions, and converts each stretch on the executor of computations: as it arrives for a read, and before its runs are written for a write. The staging is not filled with zeros, which took most of the time a read of a patch spent in the program. A flush is an operation that syncs the file, and a writer that is destroyed hands the executor one, which waits for nothing |
| `src/formats/raw/mapped/` | Reads a file through a mapping of it, for the formats whose values lie in the file as they are. A read of values out of memory advises the pages it is about to touch a step ahead. Values that are not aligned for their data type are read through a page copied out of the mapping instead |
| `src/formats/raw/gzip/` | Reads a file compressed with gzip whose inflated bytes lay out the values as they are. gzip has no random access, and nothing inflated is kept: each read inflates the file from its start up to the last value it needs, straight into the array where nothing is converted, and otherwise into staging a stretch at a time, after splitting the regions whose parts lie far apart and gathering their stretches as the planning does for a staged read. Built only with `VITRIO_ENABLE_GZIP` |
| `src/formats/dm/` | The DigitalMicrograph format, versions 3 and 4, read only: its header, the tree of tags it is parsed into, how the size of a value follows from its encoding, how the image is found among the tags, and the format that registers itself |
| `src/formats/mrc/` | The MRC format: its header, how a header resolves into the layout of a file, and the formats that register themselves: one that reads and one that writes files as they are, and one that reads files compressed with gzip, such as the `.map.gz` of the EMDB, built only with `VITRIO_ENABLE_GZIP` |
| `src/formats/eer/` | The EER format, a TIFF file whose pages hold the events of a detector: how a frame encodes them, the decoder of one strip, the reader of the events and the format that registers itself. Built only with `VITRIO_ENABLE_TIFF` |
| `src/formats/tiff/` | The TIFF format, over libtiff: the file, the layout of a page, the decoder that holds one page at a time, and the reader and writer. Built only with `VITRIO_ENABLE_TIFF` |
| `src/formats/zarr/` | The ZARR format, versions 2 and 3, read, and version 3 with OME-Zarr 0.5 written: the metadata documents of a node, the layout of an array and of its chunks, their data types, fill values and keys, the codecs that turn bytes into bytes and their pipeline, the images of OME-Zarr and the core rank they decide, the layout and the index of shards, the read operation that opens and reads a file at a time and decodes its chunks, the reader and the read format that registers itself; and for writing, the metadata of a store, the chunks held in memory until they are stored, the operations that write and flush them, the writer and the write format. Built only with `VITRIO_ENABLE_ZARR` |
| `src/strided_transfer/` | Moves regions between a file and an array: checks them against both, splits them by page for a file moved a page at a time and by chunk for one moved a chunk at a time, resolves where each one starts, works out the order to walk it in, and converts its elements. The formats move their values with it, and so do the rendering of detector events and the scratch |
| `src/compression/` | What compresses with zlib, shared by the formats that do: a stream inflated a stretch at a time, of one gzip member or several, a stream inflated or deflated whole, the inflated bytes of a gzip file read in order, and how a gzip file is told apart and the extension of what it holds named. Built only with `VITRIO_ENABLE_ZARR` or `VITRIO_ENABLE_GZIP` |
| `src/array/`, `src/memory/` | Beside the array classes, the private helpers on element types: the C++ type of each, among them the half precision number C++14 lacks and the boolean of a file, the cast between two of them, their byte order, and the bits of one read as another |
| `src/system/` | What is asked of the operating system. Each has one source file per family of systems, and the build takes the one that applies. Among them the queues of file transfers, which read, write and sync: one through io_uring, built only with liburing, and one that transfers as it is asked. Over them the ports, queues whose wait another thread can end: one through io_uring, woken by a read of an eventfd in the same ring, and one that transfers as it is asked and waits on a condition variable. The openers of ports make one kind each, and the asynchronous one says whether the process can have its kind |
| `src/file_transfer/` | Carries out transfers between files and memory without blocking: a few threads, each waiting on a port of its own for the transfers of many operations at once and for the outcomes of the tasks they run on the executor of computations. An operation is called as what it started completes, and is reported once nothing it started is outstanding. The public `file_transfer_pool` holds such an executor, which its implementation, defined under `src/`, hands to the library |
| `tests/unitary/` | White-box Catch2 suite, built from the objects of the library, which sees `src/` |
| `tests/unitary/src/mock/`, `fixtures/` | The trompeloeil mocks of the interfaces, and what several test files set up alike |
| `tests/assets/` | The files the tests are given, read where they are, and the directory under the build tree the tests write to. A format has a directory of its own, with a README.txt that says where each file comes from and what it holds, and the script that wrote the synthetic ones. `.gitattributes` keeps git from taking the files of the ZARR stores, and the files compressed with gzip, for text |
| `tests/assets/include/vitrio/tests/` | What both suites set up alike, written against the public headers only: paths, arrays that hold given values, the plan of a whole region, transfers waited for, and a pool of file transfers for the files a case opens |
| `tests/integration/` | Black-box Catch2 suite, linked with the shared library as a consumer is |
| `tests/headers/` | Compiles each public header on its own under every C++ standard the compiler has |
| `tests/install/` | Installs the library into the build tree, then builds and runs `consumer/`, a project of its own, against what was installed |
| `python/src/` | The extension of the Python package, `vitrio._binding`, written with nanobind. Each public header it binds has a source of the same name here, with one function that adds it to the module |
| `python/src/array/`, `concurrency/` | Beside the bindings of those directories, how an array given from Python becomes one of vitrio: the loan of its memory and the completion that holds the loan |
| `python/vitrio/` | The Python package: what Python expects on top of the binding, such as arguments with defaults |
| `python/vitrio/data/` | `vitrio.data`, which feeds images and their metadata to a model in batches: tables as columns, the sources a field reads from, the dataset, the sampler, the loader, the image loader reads go through when given none, and what hands a batch to PyTorch or JAX |
| `tests/python/` | pytest suite of the Python package, imported from the build tree |
| `examples/python/` | Programs that show the Python package at work, in `basic/` (vitrio assembles the pieces) and `advanced/` (the example names them). An example that needs no arguments makes its own images and asserts what it shows, and `tests/python/test_examples.py` runs it. One that is a tool takes files, and has a test file of its own that runs it on the files of `tests/assets/` or on ones it makes. Add a new one to the list of `TESTED_APART` in that test if it is a tool |
| `examples/cpp/` | The C++ examples, a target each. Built with `-DVITRIO_BUILD_EXAMPLES=ON`, and run by CTest |
| `benchmarks/` | How fast vitrio reads, against the ways other software reads, how fast it writes and reads ZARR, against the libraries Python does it with, and the sweeps that tune it. Run by hand, as its README says. CI runs only their smoke tests |
| `benchmarks/cpp/` | The C++ benchmarks, built with `-DVITRIO_BUILD_BENCHMARKS=ON`: that of the engine of `src/strided_transfer/`, white-box, linked with the objects of the library as the unit tests are |
| `benchmarks/python/vitrio_bench/` | The harness: one run of a procedure with a reader, or a writer for the procedure that writes, measured from outside vitrio and recorded as one line of JSON. Its command line, `python -m vitrio_bench run`, is what ReFrame and the smoke test launch. The "reader" of the procedure `zarr` is a library of `zarr_libraries/`, which writes a store as well as reading it. What needs root, dropping the caches and limiting the ARC of ZFS, is `evict.py`, a program of its own with one implementation per platform family that imports nothing of the package, so that a copy owned by root does it through `sudo -n` |
| `benchmarks/reframe/` | The ReFrame tests that run campaigns of the harness: a base test per procedure, whose parameters are what a sweep may vary, its named sweeps, the fixture that builds vitrio for the sweeps of its tuning macros, and example configurations of machines: one partition, or a partition per storage |
| `benchmarks/datasets/` | The manifests of real datasets, in JSON: where to download each, what its files are, how to check them (`python -m vitrio_bench dataset check`) and how to make its variants. The data is never committed |
| `docs/` | Builds the documentation of the API, a directory for each of its parts |
| `docs/agents/` | The documents for agents that `AGENTS.md` indexes. No build reads them |
| `docs/site/` | The root of the published site, whose page links both references. It is copied as it is |
| `docs/doxygen/` | The reference of the C++ headers, with Doxygen, and `mainpage.md`, its first page |
| `docs/sphinx/` | The reference of the Python package, with Sphinx, from its docstrings. It holds the `.rst` pages and `conf.py` |
| `docs/logo/` | The logo kit: the mark, the wordmark, the lockups, the favicons, the banners of the README and their PNG renders, with `source/generate.py`, which writes them all again. Its README says which file is for what. The site, Doxygen and Sphinx take their logos from here |
| `cmake/modules/` | CMake modules of the project, among them a `vitrio_fetch_<dep>.cmake` for each dependency, which fetches it and builds it with the project |
| `cmake/config/` | The template of the installed CMake package config |
| `cmake/scripts/` | CMake scripts the build runs |
| `pyproject.toml` | How pip builds the Python package, and the versions of what its builds and tests install |

## Source lists

Every directory holding sources carries a `CMakeLists.txt` naming them. The
lists are explicit rather than globbed, so a new `.cpp` has to be named there
before it is built. That holds for the Python sources of `python/vitrio/`
too. There are two exceptions. The header check globs `include/`, so that
a new header is checked without being listed, and pytest finds the files of
`tests/python/` itself.

## Test files

Tests mirror what they test: a header or source with a function body has a
test file of its own, at the same relative path under `tests/unitary/src/`.
