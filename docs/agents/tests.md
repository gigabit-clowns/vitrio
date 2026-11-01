# Tests

How the tests name the files they write, stand in for interfaces and wait for transfers.

- **Files a test writes.** `scoped_path` names a file under the build tree
  and removes it. Every case runs as a process of its own and several may
  run at once, so no two cases may name the same file: a fixture that
  several cases share names its files after the running case.
- **Mocks.** A test that needs a stand-in for an interface uses the
  trompeloeil mock under `tests/unitary/src/mock/`, never a hand-written
  fake. A mock is added with the first test that uses it. An expectation on
  a mocked transfer reports to its notifier, as the transfer would. A mock
  that the code under test destroys, such as a task or a file transfer
  operation, holds its own expectations in its member `expectations`, which
  are checked as it goes: trompeloeil aborts when a mock is destroyed before
  the expectations on it.
- **Transfers in tests.** A test that reads or writes through an interface
  waits with `read_now`, `write_now`, `flush_now` and `store_now`, from
  `tests/assets/include/vitrio/tests/transfers.hpp`.
