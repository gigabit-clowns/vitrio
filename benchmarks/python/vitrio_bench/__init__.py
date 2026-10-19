# SPDX-License-Identifier: LGPL-2.1-or-later

"""The harness of vitrio's benchmarks.

It measures what happens inside one run, and leaves the rest to whoever
launches runs: ReFrame for campaigns, pytest for the smoke test. Its
command line is the contract between the two:

	python -m vitrio_bench run read_batches --datadir DIR \\
		--results results.jsonl --reader.name=vitrio-loader --reader.cache=64

A run reads a dataset the harness generated (`vitrio_bench generate`), with
one of the readers of `vitrio_bench.readers`, the way one of the procedures
of `vitrio_bench.procedures` says. It appends one record to the results
file, in the format `vitrio_bench.results` describes.
"""
