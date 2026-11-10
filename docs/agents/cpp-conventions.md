# C++ conventions

The rules every C++ source of the library follows.

- **C++14.** The library and its public headers use nothing newer, and CMake
  requires that standard rather than preferring it. The header check compiles
  the public headers as C++14, 17, 20 and 23.
- **Licence.** Every source file starts with
  `SPDX-License-Identifier: LGPL-2.1-or-later`.
- **Public and private.** A header under `include/` is public and is installed.
  A header under `src/` is neither. A public symbol is marked `VITRIO_API`,
  from the generated `vitrio/export.hpp`; everything else is hidden. A public
  class whose implementation the library reaches into, such as
  `file_transfer_pool`, declares `class implementation;` and a
  `get_implementation()` that is not marked, and its members that are
  exported are marked one by one. The implementation is a class of its own
  under `src/`.
- **One class per file.** A class is declared in a `.hpp` and defined in a
  `.cpp`, or in an `.inl` included at the end of the header when it has to be
  inline.
- **Implementations of an interface.** A class that implements an
  interface is named after it, with what sets it apart in front:
  `file_image_reader_provider` and `caching_image_reader_provider` are
  `image_reader_provider`s, `tiff_image_reader` is an `image_reader` and
  `mock_executor` is the mock of `executor`. Its files are named as it is.
- **Formatting.** Tabs, 80 columns. The code is formatted by hand and there is
  no `.clang-format`: none reproduces the style, so one would rewrite the code
  it was run on.
- **Vocabulary types.** `vitrio::span` is a view of contiguous elements, of
  dynamic extent only. It is a type of its own under every C++ standard and
  never an alias of `std::span`, so that the ABI does not depend on the
  standard a consumer compiles with. `vitrio::byte` is `unsigned char`, the
  type C++14 lets the memory of any object be accessed through.
  `vitrio::memory_range` is a stretch of the address space, such as one to
  be advised, and not elements to be accessed, even though a span of bytes
  is built the same way.
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
- **Constants of a class.** A `static constexpr` member of a public class is
  marked `VITRIO_API` and defined in its `.cpp`. C++14 needs that definition
  whenever the constant is bound to a reference, and a consumer gets it from
  the shared library.
- **Private headers use their dependencies openly.** A header under `src/`
  includes what it needs and names its types as they are: a mapping of a
  file is a `boost::interprocess::mapped_region`, not a class wrapped around
  it. Hiding a dependency is for public headers only.
- **Element conversions.** `cast`, in `src/array/cast.hpp`, converts one
  element as `static_cast` does. Any type is produced from any other, except
  that a complex number is only produced into another complex type. A
  boolean of a file is a `stored_boolean`, in `src/array/stored_boolean.hpp`,
  a byte that is true unless it is zero, and never a `bool`: a file may hold
  any byte, and C++ leaves a `bool` that is neither zero nor one undefined.
- **Half precision.** `float16_t`, in `src/array/float16_t.hpp`, is a class
  of vitrio's own, which converts and does no arithmetic. Its conversions
  are bit arithmetic without a branch, and no intrinsic, so that a compiler
  vectorizes the loops of the strided transfer that call them for every
  element. A choice among cases is made with a mask, since GCC keeps a
  branch around a floating point operation, and a loop with a branch is not
  vectorized. It is converted to a float exactly, and from a float and from
  a double each rounded once. Every other type goes through a float, which
  holds every half precision number and every integer half precision does
  not round to an infinity.
- **Assertions.** `VITRIO_ASSERT`, from `src/assert.hpp`, checks what the code
  relies on. What a caller of the public API may get wrong is reported with an
  exception instead.
- **Logging.** What can be reported in no other way is logged with the
  `VITRIO_LOG_*` macros of `src/logger.hpp`, which wrap spdlog and take its
  format strings. They write to a logger of the library's own, on the
  standard error stream, and never to the default logger of spdlog. spdlog
  stays out of the public headers.
- **Tuning.** A number that tunes the library has its default in a macro of
  `src/config.hpp`, `VITRIO_DEFAULT_<NAME>`, which the build may override,
  and is read through `src/tuning.hpp`, where the variable `VITRIO_<NAME>`
  of the environment overrides it once the library runs. `VITRIO_DISABLE_ASYNCHRONOUS_IO`, set
  to anything but 0, has the process read its files as one without
  asynchronous ports does. None of them is an argument of the API.
- **Versions.** `VERSION` holds the version of the project. Before 1.0 a minor
  release may break the ABI, so the name of the shared library carries the
  minor version (`libvitrio.so.0.1`).
