# Pull requests and continuous integration

What a pull request carries, and what the workflows build, test and publish.

## Pull requests

Whoever opens a pull request assigns it to themselves and gives it the label,
among the repository's own, that matches what it changes: `bug` for a fix,
`enhancement` for something new, `documentation` when only documentation
changes, and so on.

## Continuous integration

`.github/workflows/build-and-test.yml` builds and tests with GCC and Clang on
Linux, Clang on macOS and MSVC on Windows, then again with the address and
undefined behaviour sanitizers and with the thread sanitizer, and once more
with CMake 3.18. The jobs that are not under a sanitizer build the Python
package too and run its suite, one of them with the oldest Python the
package supports.

Those jobs fetch the dependencies, except the one with CMake 3.18. That one
finds them all installed from the packages of Ubuntu, as a distribution
builds vitrio.

One more job builds the last release beside the tree and has libabigail
say what changed in the ABI between the two. A change does not fail it:
before 1.0 a release may break the ABI, and the report tells which kind of
release the next one has to be. Both are built against the packages of
Ubuntu.

`.github/workflows/documentation.yml` builds the documentation of the C++ API
and of the Python package, puts them side by side and publishes them to
GitHub Pages when a version is tagged.

`.github/workflows/release.yml` builds the source distribution and, from it,
the wheels of each platform, and tests each wheel. How cibuildwheel builds
them is stated there, on its step. A tag `v*` publishes
them to PyPI, which trusts the workflow, so no token is kept. It runs too
when what decides how they are built changes, and by hand.

All of them use public actions, and none of the organisation's.
