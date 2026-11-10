# Python

The rules of the binding, the Python package and `vitrio.data`.

- **The Python binding.** `vitrio._binding` mirrors the C++ API, with the
  same names and the same required arguments. What Python expects on top,
  such as defaults and the ways a data type is stated, is Python code in
  `python/vitrio/`, and `__init__.py` is the whole public surface. The
  extension is the one target built as C++17, which nanobind needs. Python
  sources are indented with tabs, as the C++ ones are.
- **`vitrio.data`.** A subpackage of plain Python over the public surface of
  `vitrio`, whose own `__init__.py` is its whole public surface. It needs
  numpy, which `vitrio` does not, so `vitrio` imports it only when
  `vio.data` is first asked for. It parses no file of metadata: it takes the
  DataFrames, mappings and structured arrays the metadata was parsed into.
  A batch is a dict of numpy arrays read into memory of its own, and only
  `_frameworks.py` knows of PyTorch and JAX. A dataset and its sources
  describe rows and hold nothing that reads them: each read is given the
  image loader it goes through, a scratch included, and a read given none
  goes through the one `_default_loader.py` shares across the process.
- **File transfers in Python.** A provider assembled without a
  `FileTransferPool`, and a write given none, take the one
  `_default_transfers.py` shares across the process, whose conversions run on a thread pool of its own, apart from the
  threads of the loaders, which may wait for reads. The work of a scratch
  given no executor, opening its files, prefetching and copying out what it
  took in, runs on that thread pool too. A child of a fork makes its own, as
  `_default_loader.py` does.
- **Docstrings.** Everything `vitrio.__all__` exports has a docstring, and so
  has each public method and property of a class, which
  `tests/python/test_documentation.py` checks. They are written in the style
  of Google (`Args:`, `Returns:`, `Raises:`) and written for Python: getters
  are properties, exceptions are Python's, an array is anything that speaks
  DLPack, and a call that blocks says that it releases the GIL. In the
  binding the text is passed to `nb::class_`, `.def` and `def_prop_ro` as
  adjacent string literals, with `\n` only where a line ends, so that a
  paragraph is one line to Sphinx. A name that Python wraps, such as `read`
  or `loader`, is documented in the wrapper and only briefly in the
  binding, which says to prefer the wrapper.
- **`import vitrio as vio`.** That is how the package is imported wherever
  it is shown in use: the README, the examples and the tests. numpy is
  `np` and pandas is `pd`, as their own documentation has them. Prose names
  what it refers to in full, as in `vitrio.Array`.
- **Arrays across the binding.** A function takes an array as an
  `nb::ndarray`, which nanobind fills from DLPack or the buffer protocol
  without a copy, and `Array` hands its memory out through the object
  nanobind exports arrays with. vitrio has no DLPack code of its own.
  Memory given from Python is lent (`memory_loan`): the arrays over it are
  given an owner that owns nothing, and the loan returns the memory once
  they are gone. So no thread but one that holds the GIL ever releases
  anything of Python, which a worker thread could not do safely while the
  interpreter shuts down. An asynchronous function hands the loan to the
  `Completion` it returns, and destroying that waits for the work.
- **Views of the memory of a binding.** What a bound object hands out as a
  view of its own memory, such as the positions of a
  `DetectorEventTimeline`, is read-only and keeps the object alive. The
  object counts the views it has handed out and refuses to change while
  one is alive, with `BufferError`, as a `bytearray` refuses to resize
  while a `memoryview` of it is. Nor is it changed while it is used
  without the GIL.
- **The GIL.** A bound function that reads, writes or waits releases the
  GIL meanwhile. So does destroying a thread pool, which waits for its
  workers.
