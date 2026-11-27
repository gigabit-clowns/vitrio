# Documentation

Where the text a user reads lives, and how it is written.

- **One copy of each text.** A text is written once and linked from
  everywhere else.
  - `README.md` introduces the library: what it is, what it offers, how
    fast it is, a first taste of the API, and links. PyPI shows it, so it
    stands alone and its links are full URLs.
  - `docs/guides/` holds the guides. Anything longer than a paragraph that
    is not about one name of the API goes there.
  - Docstrings and header comments document the API. The references are
    built from them.
  - `docs/sphinx/index.rst`, `docs/doxygen/mainpage.md`,
    `docs/site/index.html` and `docs/site/llms.txt` are entry points. Each
    holds a short introduction and links, and no text of its own that a
    guide or the README has.
- **A guide.** A guide is plain Markdown that GitHub, MyST and Doxygen all
  render: headings, lists, tables, fenced code and links. It uses no
  directive or role of MyST and no command of Doxygen. A link to another
  guide is its file name, such as `[build guide](building.md)`, which holds
  in all three. A link to anything else is a full URL. To add a guide,
  write `docs/guides/<name>.md`, add `docs/sphinx/<name>.md`, which only
  includes it, and name it in the toctree of `docs/sphinx/index.rst`.
  Doxygen takes the whole directory.
- **Code that is shown.** A snippet in the README or a guide is run before
  it is committed. The C++ shown on the first page of the C++ reference is
  an example of `examples/cpp/`, included as it is compiled, so it cannot
  go stale. The C++ snippet of the README is a copy, kept in step by hand.
- **Which functions to call.** The advice of
  `docs/guides/performance.md` is repeated in short where someone writing
  code looks first: the table of the README, `docs/site/llms.txt`, the
  docstring of the package and those of `read` and the `write` functions.
  A change to the advice is made in all of them.
- **Numbers.** A text states a performance figure only where it cannot make
  its point without one. The README has the few that back its pitch, with
  what was measured and against what, and the guides say "several times
  faster" where that is all the reader needs. No tables of results and no
  graphs. Where vitrio is not faster, the text says so.
- **Style.** Plain sentences that a newcomer follows at first reading.
  One idea a sentence, no clause nested in commas inside another, no
  em-dashes, and no claim the measurements do not back. Say a thing once.
- **Examples.** An example shows the calls a program of its kind should
  use. One that reads or writes many images does it in batches, and one
  that waits for each batch says that a real loop would not.
