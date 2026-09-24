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
