# Examples

Small programs that show vitrio at work. Each one can be read in a sitting
and run as it is. The examples that take no arguments make their own images
in a temporary directory, so they need no data. They also check their own
results, so an example that stops working fails its test.

The Python examples come in two groups:

- **Basic.** vitrio assembles what it needs. You say what to read and
  write, and the defaults suit most programs.
- **Advanced.** You create the pieces yourself: thread pools, loaders,
  savers, scratches, and the array that each read lands in. This is how a
  program reaches the best throughput.

If you are not sure which calls your program should use, read the
[performance guide](../docs/guides/performance.md) first.

## Python

Install the package (`pip install vitrio`), or put the `python/` directory of
a build on `PYTHONPATH`. All of them need NumPy.

| Example | Shows |
|---|---|
| [basic/read_stack.py](python/basic/read_stack.py) | `vio.read`: a file, one image of a stack, another data type, your own array |
| [basic/write.py](python/basic/write.py) | `write_single`, `write_stack` and `write` with a descriptor, data types, formats and metadata |
| [basic/convert.py](python/basic/convert.py) | A command line tool that converts a file to another format or data type and keeps its metadata |
| [basic/iterate_batches.py](python/basic/iterate_batches.py) | `vio.data`: a dataset of images and metadata, read in shuffled batches |
| [basic/train_2d_classifier.py](python/basic/train_2d_classifier.py) | `vio.data` on a RELION job, feeding a PyTorch network. Needs starfile, pandas and PyTorch |
| [advanced/batches_in_flight.py](python/advanced/batches_in_flight.py) | `read_batch_async`: batches read in the background while the previous one is used |
| [advanced/scratch_across_epochs.py](python/advanced/scratch_across_epochs.py) | A scratch that keeps images in RAM, and a prefetch that fills it |
| [advanced/extract_particles.py](python/advanced/extract_particles.py) | A complete program: particles cropped out of the micrographs of a RELION project and written into stacks, with reads and writes in flight. Needs starfile, pandas and tqdm |
| [advanced/fractionate_eer.py](python/advanced/fractionate_eer.py) | EER movies cut into fractions and stored as MRC or TIFF, with the next movie decoded while the current one is written |

`convert.py`, `train_2d_classifier.py`, `extract_particles.py` and
`fractionate_eer.py` take arguments and run on your own files.

## C++

Built with `-DVITRIO_BUILD_EXAMPLES=ON`, which adds one target for each
example and a CTest test that runs it.

| Example | Shows |
|---|---|
| [cpp/read_and_convert.cpp](cpp/read_and_convert.cpp) | Writing an image and reading it back as another data type |
| [cpp/read_batches.cpp](cpp/read_batches.cpp) | `read_batch_async`: a stack read in batches, with the next batch read in the background |
