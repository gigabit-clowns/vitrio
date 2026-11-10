# Benchmarks

The rules the benchmarks and their records follow.

- **Benchmarks.** A procedure measures, and a question is a named sweep of
  its ReFrame test, not a procedure of its own. A run writes every setting
  into its record, defaults included, and fails on a parameter it does not
  have, so that a mistyped one does not measure the default. Results are
  never committed. The schema of the records, in
  `benchmarks/python/vitrio_bench/results.py`, is the contract with whoever
  analyses them, and a change to what a field means bumps it.
