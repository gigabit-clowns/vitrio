# Benchmarks

How fast vitrio reads, against the ways other software reads, and the
questions asked of it to tune it.

A benchmark here is a **procedure**: what a run does with a reader, and what
it measures. A **question** is a sweep over a procedure's parameters, not a
procedure of its own. Running a point of a sweep is a run of the harness,
and campaigns of runs are left to ReFrame.

| Path | Holds |
|---|---|
| `python/vitrio_bench/` | The harness: one run of a procedure with a reader, measured from outside vitrio and recorded as one line of JSON |
| `python/vitrio_bench/readers/` | The readers: vitrio's, and the baselines that read as other software does |
| `python/vitrio_bench/procedures/` | The procedures |
| `cpp/` | The C++ benchmark of the engine that moves images between a file and an array, with Google Benchmark |
| `reframe/` | The ReFrame tests: a base test per procedure, and its named sweeps |
| `reframe/config/` | Example configurations of machines |
| `datasets/` | Manifests of real datasets: where to download them, and how to check them |

The harness is not part of the package and is not built. CI runs none of
the benchmarks, apart from two smoke tests: `tests/python/test_benchmarks.py`
checks that every reader and every scratch delivers the same batches and
that every record follows its schema, and the C++ benchmark runs each of its
benchmarks for a moment.

## Getting started

Build the Python package and install what the harness needs:

```sh
cmake --preset release -DVITRIO_BUILD_PYTHON=ON
cmake --build --preset release
pip install --group bench
```

PyTorch, the baseline `torch-dataloader` needs, is left out of the group for
its size; install it to run that baseline.

A run by hand. `--datadir` is where datasets are generated and read, so it
is the storage being measured, and it has no default:

```sh
export PYTHONPATH=$PWD/build/release/python:$PWD/benchmarks/python
python -m vitrio_bench generate --datadir /nvme/vitrio-bench \
	--dataset.layout=per-micrograph:100
python -m vitrio_bench run read_batches --datadir /nvme/vitrio-bench \
	--results results.jsonl --dataset.layout=per-micrograph:100 \
	--reader.name=vitrio-loader --reader.cache=64 --environment.cold=fadvise
```

A campaign, which runs a named sweep three times over, in a random order:

```sh
export PYTHONPATH=$PWD/build/release/python
export VITRIO_BENCH_DATADIR=/nvme/vitrio-bench
export VITRIO_BENCH_SCRATCHDIR=/nvme/vitrio-scratch
export VITRIO_BENCH_FEATURES=fio,cgroup-memory
export VITRIO_BENCH_BUILDSDIR=/nvme/vitrio-builds
reframe -C benchmarks/reframe/config/workstation.py -c benchmarks/reframe \
	-n '^cache\b' -S results=$PWD/results.jsonl \
	--exec-policy=serial --repeat=3 --exec-order=random -r
```

- **Run serially.** ReFrame runs tests at once by default, and runs that
  overlap compete for the storage they measure. The example configuration
  allows one job at a time as well.
- **Selecting by name.** `-n` matches the display name of a variant, which
  carries its parameters (`cache %cold=none ...`), hence `'^cache\b'`
  rather than `'^cache$'`.
- **One-off questions.** `-P` turns any parameter or variable of a test
  into a sweep from the command line, such as `-P cold=fadvise` or
  `-P cache=0,64 -P workers=4`, and `-S` sets one value.
- **Your machine.** `reframe/config/workstation.py` reads it from the
  environment: `VITRIO_BENCH_FEATURES` lists what it can do (below),
  `VITRIO_BENCH_DATADIR` and `VITRIO_BENCH_SCRATCHDIR` where datasets and
  the copies readers make of them go, and `VITRIO_BENCH_BUILDSDIR` where
  the builds of vitrio that sweeps of its tuning macros compare are kept.
- **Tuning macros.** The sweeps `gap_cap` and `prefetch_budget` build vitrio
  once per value of a macro of `src/config.hpp`, with the fixture
  `VitrioBuild`, and run each point on its build. A build is kept between
  campaigns, named after its macros, so only the first campaign pays for
  it. It needs nanobind in the environment ReFrame runs in. As vitrio runs,
  the variable of the environment named as the macro without `DEFAULT_`
  overrides it, which measures a value without building it.

## A run

`python -m vitrio_bench run <procedure>` reads one point, in a process of
its own, so that no thread pool, open file or cached page of one run is
left for the next. The point is given part by part, as
`--<part>.<name>=<value>`:

| Part | What | Examples |
|---|---|---|
| `dataset` | What is read | `count`, `box`, `dtype`, `layout`, `seed`, `like` |
| `reader` | Who reads it, with which settings | `name`, and the reader's own (below) |
| `workload` | How it is read | `order`, `batch`, `images`, `epochs`, `step_s`, `patch`, `patches`, `verify` |
| `environment` | Where, and in what state | `cold`, `cpus`, `memory_max`, `scratchdir` |

A value is read as JSON when it is valid JSON (`64`, `true`, `null`), and as
text otherwise. Whatever is not given takes its default, and the record
writes every default out. A name a part does not have fails the run, so a
mistyped one does not measure the default.

The run appends its record to the file of `--results`, and prints its
status and its metrics, one per line, which is what ReFrame reads:

```
vitrio_bench status ok
vitrio_bench metric images_per_s 127210.6
```

### Features used on request

What a run can do beyond reading is done only when it is asked for, the run
fails if the machine cannot do it, and the record says what was done. The
ReFrame feature of each is what a machine's configuration lists to say it
can; a variant that asks for one its machine lacks is skipped.

| Capability | Asked with | ReFrame feature |
|---|---|---|
| Drop the dataset's pages, and those of a reader's copies, from the cache, with `POSIX_FADV_DONTNEED` (Linux) | `--environment.cold=fadvise` | |
| Drop every clean page of the machine (root) | `--environment.cold=drop` | `root` |
| Run on some CPUs only (Linux) | `--environment.cpus=0-3` | |
| Limit the memory of the run, page cache included, with a scope of systemd | `--environment.memory_max=8G` | `cgroup-memory` |
| Measure the storage's ceiling with fio | the procedure `ceiling` | `fio` |

- **Cold runs.** After the cache is dropped, how much of the files is still
  cached is measured with `mincore` and recorded as `residency`. On NFS or
  a parallel file system that empties the client's cache only. The server
  keeps its own, which only someone with access to it can empty.
- **Memory limits.** A limited run is the same command run again in the
  scope, which records the limit it found and the most memory the scope
  held (`cgroup_memory_peak_bytes`). A run the limit kills cannot record
  itself, so the command that started it records it as failed. The scope
  starts empty, and pages another process cached are not counted against
  it, so a limited run should be a cold one.

## Datasets

The harness generates the datasets it reads. For a format that stores
values as they are, what they are does not change what reading them costs;
how many files there are, how large, and in what order they are read does.
So the particles are pseudo-random noise, never zeros or a constant, which
a file system could compress or leave sparse.

| Parameter | Default | |
|---|---|---|
| `count` | 20000 | Particles |
| `box` | 128 | Side of a particle, in pixels |
| `dtype` | `float32` | `float32`, `float16` or `int16` on disk |
| `format` | `mrcs` | |
| `layout` | `single-stack` | `single-stack`; `per-micrograph:<mean>`: one stack per micrograph, as RELION extracts them, with a number of particles drawn around the mean; or `one-per-file`: one image per file, as micrographs are kept |
| `seed` | 0 | |
| `like` | | A STAR file whose layout to replay: as many stacks as it names, with as many particles each, filled with noise. It stands for `count` and `layout`, and gives the box when its optics state one. The dataset's name follows the layout, not the path of the file |

A dataset is named by a hash of its parameters, so the same parameters make
the same files on any machine. Generating one that exists does nothing.

## Procedures

### `read_batches`

Batches read as fast as possible, with nothing done with them. With no
compute to hide behind, it measures the reader alone.

| Workload | Default | |
|---|---|---|
| `order` | `shuffled` | `sequential`, `shuffled`, or `shuffled-within`: the files in order and the particles of each shuffled |
| `batch` | 256 | Particles per batch |
| `images` | all | Particles an epoch reads |
| `epochs` | 1 | |
| `seed` | 0 | What decides the order of each epoch |
| `verify` | `false` | Hash every batch, to check readers against each other. It costs CPU time, so measurements leave it off |

### `epochs`

Several epochs, with a step of known length after each batch standing in
for a model. The step sleeps, which releases the GIL as a step on a GPU
does, so no GPU is needed to tell how much of an epoch was spent waiting
for data. The first epoch is reported apart, since that is where a scratch
fills up and a converted copy is made.

Its workload is that of `read_batches`, with 3 epochs by default, and
`step_s`, the seconds of the step (0 by default). Besides the metrics of
`read_batches` it reports `first_epoch_s`, `later_epoch_s`,
`stall_fraction` (the time spent waiting for data, of all the time) and its
first and later parts, and the bytes read from storage in each epoch.

### `patches`

Patches cropped out of the images of a dataset, micrographs as a rule, as
many per image, at corners drawn at random: particles read out of
micrographs without extracting them first. The patches of a batch are read
image by image, in the order extraction reads them. Its workload is that of
`read_batches`, and `patch`, the side of a patch (128), and `patches`, how
many each image has (100). The readers that crop patches are
`vitrio-loader`, with one `read_patches_async` per image of a batch, and
the readers of mrcfile, numpy and Python's files, one patch at a time.

### `ceiling`

What the storage behind a dataset can deliver, measured with fio on its
largest file, with direct I/O: sequential reads of 1 MiB at queue depth 16,
and random reads of a particle's size at queue depths 1, 4, 16 and 64. Its
"reader" is fio: `runtime_s` (10), `ioengine` (`libaio`), `direct`,
`depths`, `sequential_block`, `sequential_depth`. It has no workload.

## Readers

| Reader | Stands for | Parameters |
|---|---|---|
| `vitrio-loader` | `vio.loader` and `read_batch_async`: each batch in one call, batches read ahead | the loader's, and `in_flight` |
| `vitrio-per-item` | The ablation of `vitrio-loader`: the same loader, one particle per call, each waited for. The gap between the two is what batching and reading ahead are worth | the loader's |
| `vitrio-data` | `vio.data.DataLoader`, as users write it. Left at its defaults it reads through the loader `vio.data` shares | the loader's, and `prefetch` |
| `mrcfile-mmap-per-read` | `mrcfile.mmap` opened for every particle, as cryoSTAR and DynaMight read | |
| `mrcfile-mmap-kept` | Every stack kept mapped with `mrcfile.mmap`, one particle at a time | |
| `mrcfile-open-last` | `mrcfile.open`, which reads a whole file into memory without mapping it, as extraction scripts do. The last file is kept, so that the rows of one file in a row read it once | |
| `numpy-memmap-kept` | Every stack mapped with `np.memmap`, a batch gathered from each stack with one fancy index | `advice`: `normal`, or `random`, which maps with `MADV_RANDOM` so that a fault reads nothing around it |
| `python-read-kept` | Every stack kept open with Python's `open`, and only what a row needs read with `seek` and `readinto`: a particle in one read, a patch one row at a time | |
| `torch-dataloader` | PyTorch's DataLoader over a dataset of one particle per item, read with mrcfile | `num_workers`, `persistent_workers`, `prefetch_factor` |
| `preload` | Everything read into memory first: the upper bound, and where it stops fitting | |
| `lmdb` | **Convert first**: every particle written once into an LMDB database in the scratch directory, then read from it, as cryodata does for Cryo-IEF, without its resizing. Conversion time and size are reported | `reuse`: read an earlier run's conversion, as a rerun does; `readahead`: let the kernel read ahead, which LSUN's loader turns off |

The loader's parameters, shared by vitrio's readers:

| Parameter | Default | |
|---|---|---|
| `workers` | the machine's CPUs | Threads of the pool |
| `cache` | 0 (`vitrio-data`: the shared loader's 64) | Open readers kept |
| `scratch` | `none` | Copies of what has been read: `host` memory, a `file`, or a file of a `directory` kept within a budget; the last two in the scratch directory. A scratch is made anew for every run, and removed after |
| `run_length` | 64 | Particles of a file a scratch takes in together |
| `budget` | what the dataset takes, and a margin | Bytes a directory scratch may take |

The readers that read files themselves are named
`<library>-<call>-<how long a file stays open>`: `per-read` opens it for
each read, `kept` keeps every file open, and `last` the last one.

Every reader takes `dtype`, the data type it delivers, `float32` by
default.

## Named sweeps

Each answers one question, most of them about what the code predicts and
the defaults do not account for.

| Sweep | Procedure | Question |
|---|---|---|
| `readers` | `read_batches` | Where vitrio stands against the baselines, on one stack and on one stack per micrograph |
| `workers` | `read_batches` | **H1**: a batch is read with one task per file, so a batch from one stack runs on one thread whatever the pool's size, unless batches are read ahead. **H4**: the pool's size that suits the storage need not be the number of cores the loader takes by default, which is the machine's and not the process's |
| `cache` | `read_batches` | **H3**: `vio.loader` keeps no open readers by default and opens a file for every batch that reads from it, while the loader `vio.data` shares keeps 64. What is each worth for each layout? |
| `dtype` | `read_batches` | Reading fewer bytes: float16 or int16 on disk, delivered as float32 |
| `scratch` | `epochs` | **H2**: a scratch is opened through the file it copies, so without a cache every batch still opens its files. **H5**: a scratch takes in the runs of a file one after another, so filling it from one stack is serial |
| `run_length` | `epochs` | How many particles of a file a scratch should take in together |
| `larger_than_ram` | `epochs` | **H6**: past the memory, a file scratch is read as vitrio reads a stack. Does it read no more than reading the stack does? |
| `overlap` | `epochs` | How many batches to read ahead, against the length of a step |
| `loaders` | `epochs` | vitrio against PyTorch's DataLoader, preloading and converting to LMDB, each at its best |
| `torch_workers` | `epochs` | How many workers PyTorch's DataLoader needs to be at its best |
| `convert_first` | `epochs` | A converted dataset's first run, which converts it, against a rerun |
| `patch_readers` | `patches` | Every reader that crops patches, at three sides |
| `gap_cap` | `patches` | **H7**: the rows of a patch lie a micrograph's row apart, and a read bridges the gaps between them up to `VITRIO_DEFAULT_PREFETCH_GAP_CAP`. A build per value |
| `prefetch_budget` | `read_batches` | How many bytes a read asks the kernel for at once, ahead of copying them: `VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET`. A build per value |
| `ceiling` | `ceiling` | What the storage delivers |

Tied to the benchmark strategy of issue #26: `read_batches` covers its C1,
C2 and C4; `epochs` C3, C7 and the loader side of A6; `ceiling` its storage
ceilings; `patches` the read side of B1 (F8); the engine's benchmark in C++
the first half of C0. Among the sweeps, `readers` covers the baselines of
C1, `workers` C1 and the ablation of F1, `cache` C2 (F2), `dtype` C4 (F5),
`scratch`, `run_length` and `larger_than_ram` C3 (F7), `overlap` C7 (F3),
and `gap_cap` and `prefetch_budget` the ablation of F6.

## Real datasets

What generated data cannot stand for, such as the end-to-end runs and the
formats whose content changes what reading costs, is read from real
datasets. Each has a manifest in `datasets/`, which states where to
download it, its files and their sizes, the STAR file that names its
particles, and how to make the variants the benchmarks read. The data is
never committed.

```sh
python -m vitrio_bench dataset list
python -m vitrio_bench dataset check empiar-10180 /data/empiar-10180
```

`check` takes the directory the manifest's `download` URL lists, and checks
every file it names, and every stack its STAR file names: there, of the box
and the data type stated, and holding the particles asked of it.

## The engine, in C++

`cpp/strided_transfer.cpp` measures the engine of
`src/strided_transfer/` on its own: no file is opened and no thread
started, so what it measures is the copy and the conversion of elements,
which bound a read once its pages are in memory. The images of a batch of 64
are drawn at random from a buffer larger than the processor's caches (256
MiB, or `VITRIO_BENCH_FILE_BYTES`), at sides of 64, 128, 256 and 512.

| Benchmarks | What |
|---|---|
| `read/stack/<file>/<array>` | Whole images of a stack, from what particles are stored as to float32, and to themselves |
| `read/swapped/...` | A file in the other byte order |
| `read/patches/...` | Strided on the file's side: patches cropped out of micrographs of 4096² |
| `read/padded/...` | Strided on the array's side: rows padded |
| `write/stack/<array>/<file>` | float32 written as float32, float16 and int16 |
| `memcpy/stack/float32` | The ceiling: a std::memcpy of the same images |
| `walk/stack`, `walk/patches` | What working out the walk of a batch costs, which every read pays first |

```sh
cmake --preset release -DVITRIO_BUILD_BENCHMARKS=ON
cmake --build --preset release --target vitrio-benchmark-strided-transfer
build/release/benchmarks/cpp/vitrio-benchmark-strided-transfer \
	--benchmark_filter='read/stack/float16' --benchmark_repetitions=3
```

The ReFrame test `strided_transfer` runs it from the build tree that
`-S build_tree=...` names, keeps Google Benchmark's JSON next to the results
file, and makes the median throughput of each benchmark a performance
variable. Two runs, such as two builds, compare with Google Benchmark's
`tools/compare.py`.

## Records

JSON Lines, one object per run, in the format `vitrio_bench/results.py`
describes. Each record carries a `schema` version, the four parts of the
run, a `build` part (which vitrio: its build tree, build type, compiler
flags and commit, or the installed version; and the versions of what ran
beside it), and the metrics. The machine is described once, in
`machines/<name>.json` next to the results, and records name it.

Results are never committed here, and neither is their analysis. The schema
is the contract with whoever analyses them. Schema 2 renamed the readers
that read files themselves: `mrcfile-per-read` became
`mrcfile-mmap-per-read`, `mrcfile-open` `mrcfile-mmap-kept`, and
`numpy-memmap` `numpy-memmap-kept`.
