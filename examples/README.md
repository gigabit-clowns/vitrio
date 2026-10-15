# Examples

Programs that show vitrio at work, each one small enough to read in a sitting
and to run as it is. They make their own images in a temporary directory, so
those that take no arguments need no data, and they check what they print: an example that stops
working fails its test.

Examples are sorted by language, and then by how much of the library you hold.

- **Basic.** vitrio assembles what it needs. You say what to read and
  write, and the defaults are the ones that suit most programs.
- **Advanced.** You name the pieces: thread pools, loaders, readers,
  scratches, and the array each read lands in. This is where the throughput
  comes from.

## Python

Install the package (`pip install vitrio`), or put the `python/` directory of
a build on `PYTHONPATH`. All of them need NumPy.

| Example | Shows |
|---|---|
| [basic/read_stack.py](python/basic/read_stack.py) | `vio.read`: a file, one image of a stack, another data type, your own array |
| [basic/write.py](python/basic/write.py) | `write_single`, `write_stack`, `write` with a descriptor, data types and formats |
| [basic/convert.py](python/basic/convert.py) | A command line tool: a file in another format or data type, as the same image, stack or volume |
| [basic/iterate_batches.py](python/basic/iterate_batches.py) | `vio.data`: a dataset of images and metadata, gone through in shuffled batches |
| [basic/train_2d_classifier.py](python/basic/train_2d_classifier.py) | `vio.data` on a RELION job, feeding a PyTorch network. Needs starfile, pandas and PyTorch |
| [advanced/batches_in_flight.py](python/advanced/batches_in_flight.py) | A thread pool, a loader, and batches read in the background while the last one is used |
| [advanced/scratch_across_epochs.py](python/advanced/scratch_across_epochs.py) | A scratch that keeps images in RAM, and a prefetch that fills it |
| [advanced/extract_particles.py](python/advanced/extract_particles.py) | A complete program: patches cropped around the particles of a RELION project and written into stacks, with reads and writes in flight. Needs starfile, pandas and tqdm |
| [advanced/fractionate_eer.py](python/advanced/fractionate_eer.py) | EER movies cut into fractions, which are decoded in parallel and stored as MRC or TIFF |

Those that take arguments are tools you run on your own files, such as
`convert.py` and `fractionate_eer.py`. The rest make their own images.

## C++

Built with `-DVITRIO_BUILD_EXAMPLES=ON`, which adds one target for each
example and a CTest test that runs it.

| Example | Shows |
|---|---|
| [cpp/read_and_convert.cpp](cpp/read_and_convert.cpp) | Writing an image, reading it back as another data type |
