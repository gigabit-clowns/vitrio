# Working on vitrio

vitrio reads and writes the image files of electron microscopy. It is a C++
library with a Python package over it, and it depends on nothing of rexlib,
where its code comes from: the image subsystem of rexlib at `7bf00d69` is
being ported here a phase at a time.

Keep this file true. When a change makes something here wrong or missing, such
as a moved directory, a new convention, a dependency or a workflow, update it
in the same commit that causes it.

## Layout

| Path | Holds |
|---|---|
| `include/vitrio/` | The public headers. Everything here is part of the API |
| `include/vitrio/array/` | The array classes, their descriptor and the types of their elements |
| `include/vitrio/concurrency/` | Executors, the tasks they run and the completions that report them |
| `include/vitrio/exceptions/` | The exception types |
| `src/` | The implementation, plus the headers that are not public. Its directories are those of `include/vitrio/` |
| `tests/unitary/` | White-box Catch2 suite, built from the objects of the library, which sees `src/` |
| `tests/integration/` | Black-box Catch2 suite, linked with the shared library as a consumer is |
| `tests/headers/` | Compiles each public header on its own under every C++ standard the compiler has |
| `cmake/modules/` | CMake modules of the project |
| `cmake/config/` | The template of the installed CMake package config |
| `.github/actions/` | The actions the workflows share. They belong to this repository |

Every directory holding sources carries a `CMakeLists.txt` naming them. The
lists are explicit rather than globbed, so a new `.cpp` has to be named there
before it is built. The header check is the one exception: it globs
`include/`, so that a new header is checked without being listed.

Tests mirror what they test: a header or source with a function body has a
test file of its own, at the same relative path under `tests/unitary/src/`.

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

With CMake 3.21 or newer the presets do the same: `cmake --preset debug`,
`cmake --build --preset debug`, `ctest --preset debug`. There are `release` and
`sanitize` and `sanitize-thread` presets too. CMake 3.18 is the minimum, and one CI job builds with
exactly that version. Under it CTest runs each suite as a single test, because
listing the cases needs the JSON support of CMake 3.19.

vitrio is a shared library only. Its image formats will register themselves
through objects at namespace scope, and a static archive would drop the object
files nothing else refers to.

## Dependencies

Dependencies are found with `find_package` and nothing else: the build never
downloads. Where they come from is up to whoever builds, be it system
packages, conda or vcpkg.

`vcpkg.json` is how CI gets them. Its `builtin-baseline` pins every version at
once, and the CMake files never mention vcpkg. To use it locally, set
`CMAKE_TOOLCHAIN_FILE` to `scripts/buildsystems/vcpkg.cmake` of a vcpkg
checkout.

A dependency a test needs goes in the `tests` feature of the manifest. One the
library needs goes in `dependencies`.

| Dependency | Needed by |
|---|---|
| Threads | The library, privately |
| Catch2 3 | The test suites |

## Conventions

- **C++14.** The library and its public headers use nothing newer, and CMake
  requires that standard rather than preferring it. The header check compiles
  the public headers as C++14, 17, 20 and 23.
- **Licence.** Every source file starts with
  `SPDX-License-Identifier: LGPL-2.1-or-later`.
- **Public and private.** A header under `include/` is public and is installed.
  A header under `src/` is neither. A public symbol is marked `VITRIO_API`,
  from the generated `vitrio/export.hpp`; everything else is hidden.
- **One class per file.** A class is declared in a `.hpp` and defined in a
  `.cpp`, or in an `.inl` included at the end of the header when it has to be
  inline.
- **Formatting.** Tabs, 80 columns. The code is formatted by hand and there is
  no `.clang-format`: none reproduces the style, so one would rewrite the code
  it was run on.
- **Vocabulary types.** `vitrio::span` is a view of contiguous elements, of
  dynamic extent only. It is a type of its own under every C++ standard and
  never an alias of `std::span`, so that the ABI does not depend on the
  standard a consumer compiles with. `vitrio::byte` is `unsigned char`, the
  type C++14 lets the memory of any object be accessed through.
- **Arrays.** Four classes with one job each: `array` and `const_array` keep
  their memory alive, `array_ref` and `const_array_ref` do not, and the
  `const` ones cannot be written through. A reference is two pointers, as
  cheap to copy as a span, and what it refers to must outlive it. A function
  that uses an array only until it returns takes a reference; one that keeps
  it takes an owning class. None of them does anything with its elements.
  Memory from anywhere becomes an array through a `std::shared_ptr` that
  releases it.
- **Sizes that come from a file.** Extents and strides may be read from a
  file, so arithmetic on them goes through `checked_add` and
  `checked_multiply` and an overflow is reported, not computed.
- **Assertions.** `VITRIO_ASSERT`, from `src/assert.hpp`, checks what the code
  relies on. What a caller of the public API may get wrong is reported with an
  exception instead.
- **Versions.** `VERSION` holds the version of the project. Before 1.0 a minor
  release may break the ABI, so the name of the shared library carries the
  minor version (`libvitrio.so.0.1`).

## Continuous integration

`.github/workflows/build-and-test.yml` builds and tests with GCC and Clang on
Linux, Clang on macOS and MSVC on Windows, then again with the address and
undefined behaviour sanitizers and with the thread sanitizer, and once more
with CMake 3.18. It uses public
actions and the ones under `.github/actions/`, and none of the organisation's.
