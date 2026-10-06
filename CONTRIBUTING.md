# Contributing

## Building

Presets are host-gated (see the `condition` blocks in `CMakePresets.json`):
`cl-*` and `clangcl-*` configure only on Windows, `clang-*` and `gcc-release`
only elsewhere. Picking a preset for the wrong host fails at configure time.

On Windows:

    cmake --preset cl-debug
    cmake --build --preset cl-debug
    ctest --preset cl-debug

(substitute `clangcl-debug` to build with `clang-cl` instead of `cl`)

On Linux or macOS:

    cmake --preset clang-debug
    cmake --build --preset clang-debug
    ctest --preset clang-debug

`cl`, `clang-cl` and `clang++` must all stay green. `g++` (`gcc-release`) has
no debug preset and is exercised by CI, not by this local list.

## Invariants

These are not style preferences. Each one is load-bearing, and most exist
because the alternative was measured and failed.

1. **Every file starts with `SPDX-License-Identifier: Apache-2.0`.**
   `ctest -R hygiene.spdx` enforces it.

2. **No `NOLINT` anywhere.** Suppressions belong in `.clang-tidy`, where they
   are reviewable in one place. `ctest -R hygiene.nolint` enforces it.

3. **The exported target stays minimal.** The installed package config must
   never name Catch2, CPM, a test target, or an absolute path. The `Package`
   CI workflow asserts this.

4. **Public headers pull in no `<string>`, `<vector>`, `<format>` or
   `<iostream>`.** A consumer that only evaluates numbers must not compile them
   in every translation unit. Those belong in opt-in headers.
   `ctest -R hygiene.headers` enforces it.

5. **Every public `static_assert` message begins `formula: `** and is stable,
   unique and greppable. The must-not-compile tests match on that text, so
   these strings are tested API; renaming one is a deliberate test change.

6. **Never identify a type with `decltype([]{})`.** A lambda in a default
   template argument gives the closure internal linkage, so the type differs in
   every translation unit. Verified to fail at link time on `cl`, `clang-cl`
   and `clang++`. Use a named tag type.

7. **The version is a committed literal.** Never derive it from `git describe`:
   `vcpkg_from_github` extracts a tarball with no `.git`.

## Documentation

1. **User documentation describes the library as it is now.** That is
   `README.md`, everything under `docs/` except `docs/superpowers/`, and the
   doc comments in `include/` that form the API reference. It never says what
   the library used to do, what changed, or in which release something
   appeared; that belongs in `CHANGELOG.md`.
   `ctest -R hygiene.docs-current-state` enforces it. A line that matches a
   history phrase but describes data goes in
   `cmake/docs-current-state-allowlist.txt`, one entry per line in the form
   `path|exact trimmed line|reason`; an entry that allows no reported line
   fails the check, so remove it when its line is reworded.

2. **The tutorial** has a page in `docs/tutorial/` and a program in
   `examples/tutorial/` for each chapter. The page includes code with
   `--8<-- "examples/tutorial/<file>.cpp:<region>"` and output with
   `--8<-- "examples/tutorial/<file>.expected.txt"`; regions are marked in the
   program with `// --8<-- [start:<region>]` and `// --8<-- [end:<region>]`.
   Each program is registered in `examples/CMakeLists.txt` with
   `formula_add_pinned_example`, whose test `example.<name>.output` compares
   the program's output with its `.expected.txt`. When a program's output
   changes, update its `.expected.txt` (`example.<name>.output` fails until you
   do) and re-read the page. A new program also needs `docs/numeric-headroom.md`
   regenerated: build the target `formula-cpp-census-page` with cl (the page's
   examples table is cl's) and commit the result. The target exists when
   `FORMULA_BUILD_TESTS`, `FORMULA_BUILD_EXAMPLES` and `FORMULA_TOOLS` are all
   on, as they are by default in a top-level build. `mkdocs build --strict` fails on a missing file or region; the
   `Pages` workflow runs it on every pull request.

3. **The README** shows `examples/readme.cpp` and its output verbatim;
   `docs.readme-snippets` and `docs.readme-output` check them. The CPM tag in
   the README and in tutorial chapter 1 (`docs/tutorial/01-first-formula.md`)
   must equal the project version; `hygiene.version` checks both.

4. **The consumer-globals test.** `test/consumer_globals_tests.cpp` declares
   309 ordinary globals (308 under compilers other than cl and clang-cl, as
   glibc declares `index`) such as `result`, `value`, `x` and `index` before
   including every header, and builds under cl `/W4 /WX` and g++
   `-Wshadow -Werror`: no header's local or parameter hides one of them in
   anything that test instantiates -- evaluation of every node kind,
   `render`, `document` and the trace in every dialect, constraints, methods
   and every overlay operation (the test lists them). cl reports a template's
   local only in a template that is instantiated, and never a function
   template's parameter, so a template the test does not reach is not covered
   by it.

## Linting

`.clang-tidy` is present but not yet enforced: no CI job runs clang-tidy or
clang-format. This is a known, tracked gap, not an oversight -- it stays open
because `.clang-tidy`'s naming section still needs reconciling with names this
library spells deliberately in standard-library style.
`index_in_tuple_v` (`include/formula-cpp/detail/type_list.hpp`) deliberately
mirrors `std::is_same_v` and so deliberately violates the configured
`readability-identifier-naming.GlobalConstantCase: CamelCase`. If you run
clang-tidy locally against `include/` and it flags names like that, that is
expected, not a bug in your setup; do not rename them to satisfy the linter,
and do not add a clang-tidy CI job without first resolving this conflict
deliberately.

`.clang-format` has the same status, and one more hazard: **the checked-in
tree does not match it, and you should not reformat existing files to make it
match.** Running clang-format over the tree today rewrites 99 files, most of
the drift predating any single change, and it silently turns this library's
central idiom `var<Q> * x` into `var<Q>* x` -- clang-format parses `var<Q>`
as a type and the `*` as a pointer declarator, and there is no setting that
rescues the DSL without turning every genuine pointer into `Trace<Rep> *p`.
So if your editor formats on save, exclude this repository or expect to
discard the result; reformatting the tree is separate work nobody has asked
for yet.

## Tests

Three kinds, and new behaviour usually needs more than one:

- **Runtime** -- ordinary Catch2 assertions.
- **Compile-time** -- `STATIC_REQUIRE`, for anything that is a compile-time
  property. A regression should be a build failure, not a runtime report.
- **Must-not-compile** -- `formula_add_negative_test(<name> <expected-text>)`
  in `test/CMakeLists.txt`, with the case in `test/negative/`. The harness
  asserts both that the build failed and that it failed with the expected
  message, so add the case with a deliberately wrong expected string first and
  watch it fail before committing the right one.
