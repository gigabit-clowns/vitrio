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
| `reframe/` | The ReFrame tests: a base test per procedure, and its named sweeps |
| `reframe/config/` | Example configurations of machines |

The harness is not part of the package and is not built. CI runs none of
the benchmarks, apart from the smoke test in
`tests/python/test_benchmarks.py`, which checks that every reader delivers
the same batches and that every record follows its schema.

## Getting started

Build the Python package and install what the harness needs:

```sh
cmake --preset release -DVITRIO_BUILD_PYTHON=ON
cmake --build --preset release
pip install --group bench
```

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
- **Your machine.** Copy `reframe/config/workstation.py` and list what it
  can do in `features` (below).

## A run

`python -m vitrio_bench run <procedure>` reads one point, in a process of
its own, so that no thread pool, open file or cached page of one run is
left for the next. The point is given part by part, as
`--<part>.<name>=<value>`:

| Part | What | Examples |
|---|---|---|
| `dataset` | What is read | `count`, `box`, `dtype`, `layout`, `seed` |
| `reader` | Who reads it, with which settings | `name`, `workers`, `cache`, `in_flight`, `dtype` |
| `workload` | How it is read | `order`, `batch`, `images`, `epochs`, `verify` |
| `environment` | Where, and in what state | `cold`, `cpus` |

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
fails if the machine cannot do it, and the record says what was done.

| Capability | Asked with | Needs |
|---|---|---|
| Drop the dataset's pages from the cache, with `POSIX_FADV_DONTNEED` | `--environment.cold=fadvise` | Linux |
| Drop every clean page of the machine | `--environment.cold=drop` | root, and the ReFrame feature `root` |
| Run on some CPUs only | `--environment.cpus=0-3` | Linux |

After the cache is dropped, how much of the dataset is still cached is
measured with `mincore` and recorded as `residency`. On NFS or a parallel
file system that empties the client's cache only. The server keeps its own,
which only someone with access to it can empty.

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
| `layout` | `single-stack` | `single-stack`, or `per-micrograph:<mean>`: one stack per micrograph, as RELION extracts them, with a number of particles drawn around the mean |
| `seed` | 0 | |

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

## Readers

| Reader | Stands for | Parameters |
|---|---|---|
| `vitrio-loader` | `vio.loader` and `read_batch_async`: each batch in one call, batches read ahead | `workers`, `cache`, `in_flight` |
| `vitrio-per-item` | The ablation of `vitrio-loader`: the same loader, one particle per call, each waited for. The gap between the two is what batching and reading ahead are worth | `workers`, `cache` |
| `mrcfile-per-read` | `mrcfile.mmap` opened for every particle, as cryoSTAR and DynaMight read | |
| `mrcfile-open` | Every stack kept open with mrcfile, one particle at a time | |
| `numpy-memmap` | Every stack mapped with `np.memmap`, a batch gathered from each stack with one fancy index | |
| `preload` | Everything read into memory first: the upper bound, and where it stops fitting | |

Every reader takes `dtype`, the data type it delivers, `float32` by
default.

## Named sweeps

Each answers one question, most of them about what the code predicts and
the defaults do not account for.

| Sweep | Question |
|---|---|
| `readers` | Where vitrio stands against the baselines, on one stack and on one stack per micrograph |
| `workers` | **H1**: a batch is read with one task per file, so a batch from one stack runs on one thread whatever the pool's size, unless batches are read ahead. **H4**: the pool's size that suits the storage need not be the number of cores the loader takes by default, which is the machine's and not the process's |
| `cache` | **H3**: `vio.loader` keeps no open readers by default and opens a file for every batch that reads from it, while the loader `vio.data` shares keeps 64. What is each worth for each layout? |

Tied to the benchmark strategy of issue #26: `read_batches` covers its C1,
C2 and C4, `readers` the baselines of C1, `workers` C1 and the ablation of
F1, `cache` C2.

## Records

JSON Lines, one object per run, in the format `vitrio_bench/results.py`
describes. Each record carries a `schema` version, the four parts of the
run, a `build` part (which vitrio: its build tree, build type, compiler
flags and commit, or the installed version; and the versions of what ran
beside it), and the metrics. The machine is described once, in
`machines/<name>.json` next to the results, and records name it.

Results are never committed here, and neither is their analysis. The schema
is the contract with whoever analyses them.
