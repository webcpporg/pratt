# Working on pratt

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to pratt.

## What pratt is

A library of webcpp's own, not a port: `meta/libraries.json` has
`"port-of": null`, and pratt has no oracle, no twins and no "Differences"
appendix. Its page, `doc/pratt.adoc`, with the Doc Comments of its headers,
is what a change is checked against.

Two layers, kept apart:

- **The engine**, `include/webcpp/pratt.hpp`, `pratt/errors.hpp` and
  `pratt/parser.hpp`: the concepts `token_stream` and `grammar_for`, the
  `parser`, `parse` and the six helpers. It knows no token, no operator, no
  node and no number type. `meta/include-boundaries.json` forbids its
  headers to include `webcpp/pratt/calculator` or `boost/decimal`, and the
  lint enforces it.
- **The calculator**, `include/webcpp/pratt/calculator.hpp`, namespace
  `calc`: a grammar on the engine, written with the helpers alone, with its
  lazy lexer, its concepts `number` and `environment_for`, and its
  `environment`.

## Cautions

- **The errors are fixed.** `error`'s values 1 to 9 never change and are
  never reused; a new error takes the next value. The category's name,
  `webcpp.pratt`, and its identity, `0x7072617474000000`, never change
  either. `test/errors_test.cpp` pins the values and the name.
- **Every recursion goes through `expression()`.** The depth limit counts
  `expression` calls only. A nud, a led or a helper that recursed any other
  way would escape the limit, and could overflow the stack. The argument
  loop of the openui-lang grammar calls `expression` too.
- **`default_max_depth` is measured, not chosen.** It is the largest power
  of two at most half the deepest parse that wasm's default stack of
  64 KiB holds in a debug build, in the shape that costs the most of the
  calculator's grammar and of openui-lang's: at least 203 calls, openui-lang's
  call shape on wasip2, so half of 203 is 101, hence 64. The table holds the
  three shapes of `test/depth_test.cpp`. A change to the frames of
  `expression`, of the helpers, of the calculator's nud and led or of
  openui-lang's may move it: measure again on wasip2, wasip3 and emscripten
  in their debug build, and update together the table above
  `default_max_depth` in `parser.hpp`, the value, and the "Limits" chapter
  of the page, which repeats the table verbatim and quotes the cost of a
  level. `depth_test` fails on wasm when the default no longer fits.
  Emscripten's own default stack is the same 64 KiB, so `depth_test` passes
  there unchanged: the canary, twice the default depth, still fits.
- **The first error in reading order wins.** The lexers are lazy, and a
  character that starts no token is a token of the highest binding power,
  whose nud and led return `invalid_character`, so the engine reaches it
  wherever it stands. Writes made before an error stay made.
  `test/calculator_test.cpp` pins both, with `1 $ 2`, `x + 1 $` and
  `i++ $`.
- **Exactness.** The calculator builds a number in the number type's own
  arithmetic, never through `double`; it computes an integral power by
  repeated squaring, for exactness and determinism, and calls `pow` only
  for an exponent that is not an integer, or for a negative one whose
  positive power is out of the type's range; division and remainder by zero
  are `division_by_zero`. Its default number type is `decimal64_t`, for
  exactness.
- **Allocation.** The engine and the calculator's parse allocate nothing;
  `calc::environment` allocates through its `Allocator` alone, for a new
  name. `test/allocation_test.cpp` replaces the global `operator new` to
  prove it, and fails on a change that allocates. A test or an example
  never states how many allocations a standard library makes: MSVC's map
  allocates when it is built, and in a debug build each container adds a
  proxy. It compares what a step adds, and that every allocation is given
  back.
- **No exceptions in a header.** The headers compile without exceptions,
  which the lint checks by compiling every public header, and every test and
  example, without them: no `throw`, `try` or `catch` under `include/`.
- **`config.hpp` comes first.** Every public header includes
  `<webcpp/pratt/config.hpp>` first, which includes `<webcpp/config.hpp>` first
  and defines `WEBCPP_PRATT_NO_EXCEPTIONS` when webcpp has no exceptions
  (`WEBCPP_HAS_EXCEPTIONS` is not defined: the compiler has none, or the
  program defined `WEBCPP_NO_EXCEPTIONS`) or when a program defines it for
  pratt alone; the tests `config`, `config_developer_defined` and
  `config_all_off` check the three ways. A header raises only through
  `boost::throw_exception` (the lint's bare throw).
- **openui-lang.** The micro-subset in `example/openui/grammar.hpp` is the
  seed of a future parser of openui-lang. Name the language openui-lang,
  alone, in every file and commit. `test/openui_test.cpp` shares the grammar
  through `<include>../example` in `test/Jamfile`.

## The page

- Each `@see "<title>"` of a Doc Comment names a top-level chapter of the
  page: "Introduction", "Writing a grammar", "The calculator", "Errors",
  "Limits", "Allocation", "Testing". Renaming a chapter renames its `@see`
  lines in the same commit; doc-check fails otherwise.
- The guide's programs are `example/pratt_<topic>.cpp`, each with its
  `.expected`; the page shows their regions by tag, and some of their
  outputs by `lines=`. A change to a program's output checks the `lines=`
  ranges of `calculator.adoc` and `openui.adoc`.
- Line 30 of `example/calculator_repl.input` is ten thousand `(` and a `1`;
  the page shows the input with `lines=1..29;31..33`. Keep that line where
  it is, or move the range with it.
- The README copies `example/pratt_first_grammar.cpp[tag=program]` and its
  output; doc-check compares the copy at every doc build.
