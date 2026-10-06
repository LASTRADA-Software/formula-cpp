# Approachable README, Tutorial and Current-State Docs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make formula-cpp approachable: a short README with badges and a minimal example, a two-track tutorial on the documentation site whose code and output cannot drift, user documentation that describes only the current library, and coverage reported to Codecov.

**Architecture:** Every program a page shows lives under `examples/` and is built and run by CTest; its exact output is checked in as `<program>.expected.txt` and compared byte for byte by a new `cmake/CheckExpectedOutput.cmake`. Tutorial pages include code regions and output files with `pymdownx.snippets` (`check_paths: true`), so `mkdocs build --strict` fails on a missing file or section; the Pages workflow builds the site on pull requests too. The README, which GitHub renders directly, is pinned to `examples/readme.cpp` by the existing snippet and output checks. A new hygiene test refuses history wording in user documentation.

**Tech Stack:** C++23 header-only library, CMake 3.23+ with CTest, MkDocs Material with pymdownx, GitHub Actions, Clang 22 source-based coverage, Codecov.

**Spec:** `docs/superpowers/specs/2026-10-06-approachable-docs-design.md`

## Global Constraints

- Every new file starts with `SPDX-License-Identifier: Apache-2.0` in the file's comment syntax (C++ `//`, CMake/YAML `#`); `.md` and `.expected.txt` files carry none.
- All output in examples, tests and tools uses `std::print` / `std::println`; never `printf`, `puts` or iostream.
- Error handling is explicit: keep the `checked_` forms, check every `std::expected` before reading it, print or propagate the error. Never unwrap unchecked; never switch to a throwing twin (`evaluate`, `explain`, `convert_to`) to shorten code. A `constexpr` result may be checked with `static_assert(r.has_value())`.
- Every standard cited anywhere is an invented `Example Standard N:2020`. No real standard is named (`hygiene.no-real-standards` enforces it).
- User documentation (`README.md`, `docs/` except `docs/superpowers/`, doc comments in `include/`) describes the library as it is now. Never "no longer", "previously", "used to", "formerly", "once meant", "was renamed", "new in", "since 0.x". History goes only in `CHANGELOG.md`.
- Professional, neutral tone. Page and chapter titles are plain ("Tutorial", "Units and dimensions"). The phrase "zero to hero" appears nowhere.
- The README writes every library name with `formula::`; no `using namespace` in it.
- Committed text never refers to `STATUS.md`, session-only labels or reviewer IDs.
- Only GCC 14 is supported for GCC; no workarounds for older GCC.
- Per-task verification runs the Windows compilers only: `cl-debug` build and full `ctest`, plus any new negative test under `clangcl-debug`. Every compiler and preset runs once at the end, driven to full green.
- Commits end with `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`.

## Review Focus

1. **Windows line endings in program output.** `std::println` on Windows writes `\r\n`; `.expected.txt` is checked out LF (`.gitattributes` has `* text=auto eol=lf`). The output check must normalise both sides, and a mismatch must still be caught: Task 1's self-test pins it.
2. **A snippet marker misspelt or removed.** A page including `file.cpp:evaluate` after the region was renamed must fail the site build, not publish an empty code block. Task 2 proves `mkdocs build --strict` fails on a missing section, and Task 2 makes that build run on pull requests.
3. **A diagnostic quoted in `docs/tutorial/`.** The two existing diagnostic checks glob only `docs/*.md`; a stale quote in a subdirectory would pass. Task 2 extends both to `docs/**` (excluding `docs/superpowers/`), and Task 4's quote is covered by them.
4. **Legitimate prose that looks like history.** "the two determinations no longer agree" describes data. The current-state check must allow it by an explicit allow-list entry, not by weakening the pattern; Task 15's self-test pins both a flagged phrase and an allow-listed line.
5. **The CPM tag going stale on a release bump.** The README and tutorial chapter 1 name `@0.4.0`; Task 13 extends `hygiene.version` so a bump that misses either fails.

---

## File Structure

| Path | Responsibility |
|---|---|
| `cmake/CheckExpectedOutput.cmake` (new) | Runs a program, compares its stdout with a checked-in `.expected.txt` byte for byte after normalising CRLF |
| `test/fixtures/readme-wrong.expected.txt` (new) | Fixture proving the output check detects a difference |
| `examples/CMakeLists.txt` | `formula_add_pinned_example()`; registers `examples/readme.cpp` and every tutorial program; README snippet/output checks |
| `examples/readme.cpp`, `examples/readme.expected.txt` (new) | The README's minimal program and its output |
| `examples/tutorial/NN_<name>.cpp`, `.expected.txt` (new) | One program per tutorial chapter |
| `test/negative/tutorial_load_plus_area.cpp` (new) | The chapter 2 mistake that must not compile |
| `docs/tutorial/index.md`, `docs/tutorial/NN-<name>.md` (new) | Tutorial pages |
| `mkdocs.yml` | `pymdownx.snippets`, `navigation.indexes`, Tutorial and Guides nav sections |
| `.github/workflows/pages.yml` | Build the site on pull requests; deploy only from `master` |
| `cmake/CheckDocumentedDiagnostics.cmake`, `cmake/CheckDocumentedDiagnosticText.cmake` | Scan `docs/**` except `docs/superpowers/` |
| `README.md` | Landing page |
| `docs/index.md` | Site home |
| `cmake/CheckVersionConsistency.cmake`, `test/CMakeLists.txt` | CPM tag in README and chapter 1 equals the project version |
| `cmake/CheckDocsCurrentState.cmake`, `cmake/docs-current-state-allowlist.txt`, `cmake/TestDocsCurrentState.cmake` (new) | The current-state rule, its allow-list, its self-test |
| `docs/*.md`, `include/formula-cpp/*.hpp` | The current-state sweep |
| `CMakePresets.json`, `.github/workflows/coverage.yml`, `codecov.yml` | Coverage |
| `CONTRIBUTING.md`, `CHANGELOG.md` | Documentation rules; the changelog entry |

## Tutorial conventions (apply to Tasks 3-10)

**Programs.** `examples/tutorial/NN_<name>.cpp`, two-digit chapter number, snake_case name. Each program:

- starts with the SPDX line and a two-line comment naming the chapter;
- is complete and builds on its own; core chapters 2-9 start as a copy of the previous chapter's program and add to it;
- prints only what its page shows, and ends `return 0;` on success, `1` on any unexpected error (it prints the error first);
- marks every region its page includes with `// --8<-- [start:<region>]` and `// --8<-- [end:<region>]` on their own lines, region names in lower-kebab-case. Markers inside a function are indented like the code around them.

**Expected output.** Before running the program, write `NN_<name>.expected.txt` from the values computed by hand (listed per task). Run the program; where the library's spelling differs from your guess (a source marker, a unit symbol, a `≈`), take the program's spelling **only after** confirming the numeric value matches the hand-computed one. The reviewer re-derives every number.

**Registration.** In `examples/CMakeLists.txt`, after the README block from Task 1:

```cmake
formula_add_pinned_example(tutorial_01_first_formula tutorial/01_first_formula.cpp)
```

**Pages.** `docs/tutorial/NN-<name>.md`, two-digit number, kebab-case name, appended to the Tutorial section of `mkdocs.yml` nav in chapter order. Each page has exactly these sections:

```markdown
# <N>. <Title>

<What this chapter covers: two or three sentences.>

## <one heading per step, as many as the chapter needs>

<prose>

```cpp
--8<-- "examples/tutorial/NN_<name>.cpp:<region>"
```

<prose explaining the region>

## Output

```text
--8<-- "examples/tutorial/NN_<name>.expected.txt"
```

## Summary

- `<API introduced>` -- one line on what it does.

## Further reading

- [<Guide title>](../<guide>.md#<anchor>)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
```

The page shows code only through includes; the one exception is chapter 1's consumer CMake block (Task 3). Core chapters 2-9 open with one sentence saying what is added to the previous chapter's program.

**Verification per tutorial task:**

```bash
cmake --build --preset cl-debug
ctest --preset cl-debug -R "tutorial|docs\.|hygiene\."
mkdocs build --strict
```

All must pass; then the full `ctest --preset cl-debug` before committing.

---

### Task 1: Pinned output, and the README's program

**Files:**
- Create: `cmake/CheckExpectedOutput.cmake`
- Create: `test/fixtures/readme-wrong.expected.txt`
- Create: `examples/readme.cpp`
- Create: `examples/readme.expected.txt`
- Modify: `examples/CMakeLists.txt` (add the function after `formula_add_example`, register `readme`)

**Interfaces:**
- Produces: CMake function `formula_add_pinned_example(<name> <source>)`. Builds `<source>` as target `formula-cpp-example-<name>` via `formula_add_example`, and adds CTest `example.<name>.output`, which runs `cmake/CheckExpectedOutput.cmake` with `EXAMPLE_EXE` = that target's file and `EXPECTED` = `<source>` with its extension replaced by `.expected.txt`.
- Produces: `examples/readme.cpp` with region `program` (from its first `#include` to the closing brace of `main`), used by Tasks 13 and 14.

- [ ] **Step 1: Write the fixture and the expected output by hand**

`examples/readme.expected.txt` (675 kN / 22500 mm² = 675000 N / 0.0225 m² = 30 000 000 Pa = 30 MPa):

```text
f_c = 30 MPa
```

`test/fixtures/readme-wrong.expected.txt`:

```text
f_c = 31 MPa
```

- [ ] **Step 2: Write `cmake/CheckExpectedOutput.cmake`**

```cmake
# SPDX-License-Identifier: Apache-2.0
# A program whose output a page includes must print exactly that output. The
# page includes `<program>.expected.txt` verbatim (pymdownx.snippets), so this
# is the check that makes the published output the program's real output: it
# runs the program and compares its standard output with that file, whole,
# byte for byte.
#
# Line endings are normalised on both sides first: std::println writes CRLF on
# Windows, and .gitattributes checks text files out with LF. Nothing else is
# normalised -- a trailing space, a missing final newline or a changed line
# is a difference.
#
# On a difference it fails naming the file, and prints both texts, so the fix
# -- the program or the expected file -- can be chosen by reading them.
#
# `cmake_minimum_required` for the reason CheckGuideOutput.cmake gives: under
# `cmake -P`'s default OLD policies, CMake 3.28.3 misreads `if()` constructs
# the project's own minimum reads correctly.
cmake_minimum_required(VERSION 3.23)

foreach(variable EXAMPLE_EXE EXPECTED)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CheckExpectedOutput.cmake: ${variable} is not set")
    endif()
endforeach()

if(NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "CheckExpectedOutput.cmake: ${EXPECTED} does not exist")
endif()

execute_process(COMMAND "${EXAMPLE_EXE}"
                OUTPUT_VARIABLE actual
                ERROR_VARIABLE errors
                RESULT_VARIABLE exitCode)
if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "CheckExpectedOutput.cmake: ${EXAMPLE_EXE} exited with ${exitCode}:\n${errors}")
endif()

file(READ "${EXPECTED}" expected)
if(expected STREQUAL "")
    message(FATAL_ERROR
        "CheckExpectedOutput.cmake: ${EXPECTED} is empty. An empty expected output matches only a "
        "program that prints nothing, which no page includes.")
endif()

string(REPLACE "\r\n" "\n" actual "${actual}")
string(REPLACE "\r\n" "\n" expected "${expected}")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
        "CheckExpectedOutput.cmake: the output of ${EXAMPLE_EXE} differs from ${EXPECTED}.\n"
        "--- expected\n${expected}--- actual\n${actual}--- end")
endif()

message(STATUS "CheckExpectedOutput.cmake: output matches ${EXPECTED}")
```

- [ ] **Step 3: Add the function and its self-test to `examples/CMakeLists.txt`**

Directly after the closing `endfunction()` of `formula_add_example`:

```cmake
# An example whose whole output a page includes: built and run as every
# example is, and its output compared, byte for byte, with the
# `<name>.expected.txt` beside its source. The pass regex given to
# formula_add_example is "." -- any output -- because the comparison is the
# real check, and restating the output as a regex would be a second copy to
# keep in step.
function(formula_add_pinned_example name source)
    formula_add_example(${name} "${source}" ".")
    string(REGEX REPLACE "\\.cpp$" ".expected.txt" expected "${source}")
    add_test(NAME "example.${name}.output"
        COMMAND "${CMAKE_COMMAND}"
                -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-${name}>"
                -D "EXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/${expected}"
                -P "${PROJECT_SOURCE_DIR}/cmake/CheckExpectedOutput.cmake")
endfunction()

# The README's program: the shortest complete use of the library. README.md
# shows its source and output, pinned below; docs/index.md includes both.
formula_add_pinned_example(readme readme.cpp)

# The output check must catch a difference, not only pass on a match: run
# against an expected file that says 31 MPa, it must report one.
add_test(NAME example.readme.output-detects-a-difference
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-readme>"
            -D "EXPECTED=${PROJECT_SOURCE_DIR}/test/fixtures/readme-wrong.expected.txt"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckExpectedOutput.cmake")
set_tests_properties(example.readme.output-detects-a-difference PROPERTIES
    PASS_REGULAR_EXPRESSION "differs from")
```

- [ ] **Step 4: Run to verify it fails (no program yet)**

Run: `cmake --preset cl-debug`
Expected: configure FAILS: `Cannot find source file: readme.cpp`.

- [ ] **Step 5: Write `examples/readme.cpp`**

```cpp
// SPDX-License-Identifier: Apache-2.0
// The README's example: the compressive strength of a specimen, the maximum
// load it carried over the area that carried it. README.md shows it whole.

// --8<-- [start:program]
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

// A quantity is a type: a symbol, a description and a unit.
using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", formula::unit::Megapascal>;

// The formula, written once with ordinary operators.
constexpr auto strength = formula::yields<Strength>(formula::var<Load> / formula::var<Area>);

int main()
{
    auto const specimen = formula::environment(formula::Measured<Load> { 675 }, formula::Measured<Area> { 22500 });

    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate: {}", result.error());
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), *result);
}
// --8<-- [end:program]
```

The spec wrote `formula::describe(result.error())`; the evaluation error is printed with `{}` directly instead, the form `examples/expressions.cpp` uses, because `describe()` is defined for `ArithmeticError` and `SymbolError`, not for the evaluation error type.

- [ ] **Step 6: Build and run the new tests**

Run:
```bash
cmake --preset cl-debug
cmake --build --preset cl-debug --target formula-cpp-example-readme
ctest --preset cl-debug -R "example\.readme"
```
Expected: `example.readme`, `example.readme.output`, `example.readme.output-detects-a-difference` and `census.example.readme` PASS. If `example.readme.output` fails only because the program prints a source marker or a different unit spelling, confirm the value is still 30 MPa, then copy the program's exact line into `readme.expected.txt`.

- [ ] **Step 7: Full suite and commit**

Run: `cmake --build --preset cl-debug && ctest --preset cl-debug`. Expected: all pass.

```bash
git add cmake/CheckExpectedOutput.cmake test/fixtures/readme-wrong.expected.txt examples/readme.cpp examples/readme.expected.txt examples/CMakeLists.txt
git commit -m "test: pin an example's whole output to a checked-in file"
```

---

### Task 2: Site plumbing for included code

**Files:**
- Modify: `mkdocs.yml`
- Modify: `.github/workflows/pages.yml`
- Modify: `cmake/CheckDocumentedDiagnostics.cmake:192` and `cmake/CheckDocumentedDiagnosticText.cmake:79`
- Create: `docs/tutorial/index.md`

**Interfaces:**
- Produces: the nav section `Tutorial` whose first entry is `tutorial/index.md`; later tasks append chapter pages after it, in order.
- Produces: snippet includes resolved relative to the repository root, e.g. `--8<-- "examples/readme.cpp:program"`.

- [ ] **Step 1: Enable snippets and restructure the nav in `mkdocs.yml`**

Add `navigation.indexes` to `theme.features`. Append to `markdown_extensions`:

```yaml
  # The tutorial includes its code and output from the programs CTest builds
  # and runs (examples/tutorial/), so a published page cannot drift from what
  # compiles. Paths resolve from the repository root, where `mkdocs build`
  # runs; check_paths turns a missing file or a missing
  # `--8<-- [start:name]` section into a build error, which --strict in
  # .github/workflows/pages.yml then fails on.
  - pymdownx.snippets:
      base_path: ["."]
      check_paths: true
```

Replace `nav:` with:

```yaml
nav:
  - Home: index.md
  - Tutorial:
      - tutorial/index.md
  - Guides:
      - Numbers: numbers.md
      - Numeric headroom: numeric-headroom.md
      - Dimensions and units: dimensions.md
      - Quantities and measurements: quantities.md
      - Expressions and evaluation: expressions.md
      - Citations and rendering: citations.md
      - Tracing and audit trails: tracing.md
      - Displaying numbers: display.md
      - Calculations and worksheets: calculations.md
      - Rounding and conditionals: rounding-and-conditionals.md
      - Constraints and verdicts: constraints.md
      - Lookup tables: lookup-tables.md
      - Methods and overlays: methods-and-overlays.md
      - Series and grading curves: series.md
      - Statistics, outliers and precision: statistics.md
      - Other samples and other tests: records.md
      - Opaque operations and bounded retry: opaque-and-retry.md
  - Gallery: gallery.md
  # Doxygen output, dropped into the built site's api/ directory by
  # .github/workflows/pages.yml after `mkdocs build` runs -- not a page
  # MkDocs itself builds, so it is linked as an external-style URL rather
  # than a page in docs/.
  - API reference: https://lastrada-software.github.io/formula-cpp/api/
```

- [ ] **Step 2: Write `docs/tutorial/index.md`**

```markdown
# Tutorial

This tutorial teaches formula-cpp step by step. It assumes you know modern
C++ -- C++20 and C++23, `constexpr`, `std::expected`, class-type template
arguments -- and nothing about this library.

## Before you start

You need a C++23 compiler (MSVC, clang-cl, Clang, GCC 14 or AppleClang),
CMake 3.23 or newer, and [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake)
to fetch the library. Chapter 1 shows the CMake lines.

## Two tracks

**The core track** builds one program: the calculation of a concrete
specimen's compressive strength, from the load that crushed it and the
specimen's size. Each chapter extends the previous chapter's program, so read
the core track in order.

**The advanced track** covers the rest of the library in independent
chapters. Read the ones you need, in any order, once you have finished the
core track.

Every program in this tutorial is built and run by the library's test suite,
and every output shown is what that program prints. Every standard cited is
an invented `Example Standard`.

## Core track

## Advanced track
```

The two track headings get their chapter lists in Tasks 3-10.

- [ ] **Step 3: Extend the two diagnostic checks to `docs/**`**

In both scripts, replace the `file(GLOB documents "${SOURCE_DIR}/docs/*.md")` line with:

```cmake
# Every page of the documentation, the tutorial's included, but not the
# internal planning documents under docs/superpowers/, which the site does not
# publish (mkdocs.yml, exclude_docs).
file(GLOB_RECURSE documents "${SOURCE_DIR}/docs/*.md")
list(FILTER documents EXCLUDE REGEX "/docs/superpowers/")
```

- [ ] **Step 4: Build the site on pull requests in `pages.yml`**

Change the trigger and gate the deploy:

```yaml
on:
  push:
    branches: [ master ]
  # Built on every pull request, so a page whose included code or output went
  # missing fails before it merges. Deployed only from master (the deploy
  # job's `if`).
  pull_request:

concurrency:
  # One group per ref: deployments from master stay serialised, as before,
  # and a pull request's build never queues behind one.
  group: pages-${{ github.ref }}
  cancel-in-progress: false
```

On the `Upload the built site` step add `if: github.event_name == 'push'`, and on the `deploy` job add `if: github.event_name == 'push'`. Update the file's header comment: replace the sentence "On push to master only -- a PR preview of the published site is not part of this phase, and publishing from anywhere but the branch that just passed Build (build.yml) would let an unreviewed page reach readers." with "Built on every push to master and every pull request; deployed only from master, so an unreviewed page never reaches readers."

- [ ] **Step 5: Verify the site builds, and that a missing section fails it**

Run: `mkdocs build --strict`
Expected: PASS.

Then temporarily append to `docs/tutorial/index.md`:

````markdown
```cpp
--8<-- "examples/readme.cpp:no-such-region"
```
````

Run: `mkdocs build --strict`
Expected: FAIL with `Snippet section 'no-such-region' could not be located`.
Change it to `examples/no-such-file.cpp`; expected: FAIL with `could not be found`. Remove the temporary block; `mkdocs build --strict` PASSES again. Record both failure messages in the task report.

- [ ] **Step 6: Run the diagnostic checks**

Run: `ctest --preset cl-debug -R "hygiene\.documented-diagnostic"`
Expected: PASS (no tutorial page quotes a diagnostic yet).

- [ ] **Step 7: Commit**

```bash
git add mkdocs.yml .github/workflows/pages.yml cmake/CheckDocumentedDiagnostics.cmake cmake/CheckDocumentedDiagnosticText.cmake docs/tutorial/index.md
git commit -m "docs: include tutorial code from compiled programs, and build the site on PRs"
```

---

### Task 3: Chapter 1, First formula

**Files:**
- Create: `examples/tutorial/01_first_formula.cpp`, `examples/tutorial/01_first_formula.expected.txt`
- Create: `docs/tutorial/01-first-formula.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml` (nav), `docs/tutorial/index.md` (core track list)

**Interfaces:**
- Consumes: `formula_add_pinned_example` (Task 1); the Tutorial nav section (Task 2).
- Produces: quantities `Load` (`F`, kN), `Area` (`A_c`, mm²), `Strength` (`f_c`, MPa) and the formula `strength`, which chapter 2 copies; regions `includes`, `quantities`, `formula`, `environment`, `evaluate`.
- Produces: the consumer CMake block containing exactly `CPMAddPackage("gh:LASTRADA-Software/formula-cpp@0.4.0")`, which Task 13's version check reads.

- [ ] **Step 1: Write the expected output by hand**

`examples/tutorial/01_first_formula.expected.txt`:

```text
f_c = 30 MPa (derived)
```

- [ ] **Step 2: Write the program**

```cpp
// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 1: a first formula. A specimen's compressive strength is
// the maximum load it carried over the area that carried it.

// --8<-- [start:includes]
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>
// --8<-- [end:includes]

namespace
{
// --8<-- [start:quantities]
using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", formula::unit::Megapascal>;
// --8<-- [end:quantities]

// --8<-- [start:formula]
constexpr auto strength = formula::yields<Strength>(formula::var<Load> / formula::var<Area>);
// --8<-- [end:formula]
} // namespace

int main()
{
    // --8<-- [start:environment]
    auto const specimen = formula::environment(formula::Measured<Load> { 675 }, formula::Measured<Area> { 22500 });
    // --8<-- [end:environment]

    // --8<-- [start:evaluate]
    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<Strength>(), *result, result->source());
    // --8<-- [end:evaluate]
    return 0;
}
```

- [ ] **Step 3: Register, build, run**

Add `formula_add_pinned_example(tutorial_01_first_formula tutorial/01_first_formula.cpp)` to `examples/CMakeLists.txt`. Run the verification commands from *Tutorial conventions*. Expected: `example.tutorial_01_first_formula.output` PASSES (adjust only spelling, never the value, as the conventions say).

- [ ] **Step 4: Write `docs/tutorial/01-first-formula.md`**

Sections, in the page template's order:

1. Opening: the chapter calculates one specimen's compressive strength from its maximum load and loaded area, and introduces the four ideas everything else builds on: a quantity, a formula, an environment of measurements, a checked evaluation.
2. `## Add the library to a project` -- the one block not included from a file:
   ````markdown
   ```cmake
   include(cmake/CPM.cmake)
   CPMAddPackage("gh:LASTRADA-Software/formula-cpp@0.4.0")

   add_executable(strength main.cpp)
   target_compile_features(strength PRIVATE cxx_std_23)
   target_link_libraries(strength PRIVATE formula-cpp::formula-cpp)
   ```
   ````
   Then the `includes` region, and one paragraph: `formula.hpp` is the library; `format.hpp` lets `std::format` and `std::print` write its values; rendering, documentation and tracing headers are separate and introduced in chapters 7 and 8.
3. `## Declare the quantities` -- region `quantities`. Explain: a quantity is a type; its template arguments are a tag that makes it unique, its symbol, its description and the unit its values are stated in. Two quantities with the same unit are still different types.
4. `## Write the formula` -- region `formula`. `var<Q>` stands for the value of `Q`; ordinary operators build the formula; `yields<Strength>` names what it calculates. The formula is a compile-time object: declaring it computes nothing.
5. `## Provide the measurements` -- region `environment`. `Measured<Q>` holds a value in `Q`'s declared unit (675 is kN, 22500 is mm²); `environment()` collects them.
6. `## Evaluate, and check the result` -- region `evaluate`. `checked_evaluate` returns `std::expected`; an arithmetic failure (a division by zero) is an error value, never an exception or a silent zero. The result prints as number and unit; `source()` says it was derived.
7. `## Output`, `## Summary` (`Quantity`, `var`, `yields`, `Measured`, `environment`, `checked_evaluate`, `symbol_of`, `source()`), `## Further reading` (Quantities and measurements `../quantities.md`; Expressions and evaluation `../expressions.md`; API reference).

Note on the page, after the output, one sentence: kN over mm² came out in MPa with no conversion written; chapter 2 explains why.

- [ ] **Step 5: Nav and index**

`mkdocs.yml`, under `Tutorial:` after `tutorial/index.md`: `- 1. First formula: tutorial/01-first-formula.md`. In `docs/tutorial/index.md` under `## Core track`: `1. [First formula](01-first-formula.md) -- one formula, evaluated and checked.`

- [ ] **Step 6: Verify and commit**

Run the verification commands from *Tutorial conventions*, then full `ctest --preset cl-debug`. Expected: all pass.

```bash
git add examples/tutorial/01_first_formula.cpp examples/tutorial/01_first_formula.expected.txt docs/tutorial/01-first-formula.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapter 1, a first formula"
```

---

### Task 4: Chapters 2 and 3, Units and dimensions; Exact numbers

**Files:**
- Create: `examples/tutorial/02_units_and_dimensions.cpp` + `.expected.txt`, `examples/tutorial/03_exact_numbers.cpp` + `.expected.txt`
- Create: `test/negative/tutorial_load_plus_area.cpp`
- Create: `docs/tutorial/02-units-and-dimensions.md`, `docs/tutorial/03-exact-numbers.md`
- Modify: `examples/CMakeLists.txt`, `test/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Interfaces:**
- Consumes: chapter 1's program.
- Produces (chapter 3's program, copied by chapter 4): quantities `SideA` (`a`, mm), `SideB` (`b`, mm), `Load`, `Area`, `Strength`; formulas `loadedArea = formula::yields<Area>(var<SideA> * var<SideB>)` and `strength = formula::yields<Strength>(var<Load> / loadedArea)`; `using formula::var; namespace unit = formula::unit; using namespace formula::literals;` in the anonymous namespace (introduced in chapter 2 and explained there).

**Chapter 2.** Copy chapter 1. Replace the `Area` input with two side lengths and compute the area:

```cpp
// --8<-- [start:sides]
using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;
// --8<-- [end:sides]

// --8<-- [start:formulas]
constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(var<Load> / loadedArea);
// --8<-- [end:formulas]
```

A second result quantity shows the same formula evaluated into another unit:

```cpp
// --8<-- [start:other-unit]
using StrengthInNewtons = formula::Quantity<struct StrengthInNewtonsTag, "f_c", "compressive strength",
                                            unit::NewtonPerSquareMillimetre>;
// --8<-- [end:other-unit]
```

evaluated with `formula::checked_evaluate<StrengthInNewtons>(var<Load> / loadedArea, specimen)` (the `<Result>` form, see `examples/rounding_and_conditionals.cpp:103`). Inputs: `Measured<SideA> { 150 }`, `Measured<SideB> { 150 }`, `Measured<Load> { 675 }`. Every result is checked before it is read.

Hand-computed expected output (one line each, in this order):

```text
A_c = 22500 mm2 (derived)
f_c = 30 MPa (derived)
f_c = 30 N/mm2 (derived)
```

The page sections: "Compute the area from the sides" (`sides`, `formulas`: a formula can use another formula; its unit, mm × mm, is mm²); "Units convert themselves" (`other-unit` and its evaluation region: 30 MPa and 30 N/mm² are one value; the declared unit of the result decides how it is stated; conversion factors are exact); "A dimensional mistake does not compile" (below). Summary: composing formulas, `checked_evaluate<Result>`, declared units. Further reading: `../dimensions.md`, `../expressions.md`.

**The mistake that must not compile.** `test/negative/tutorial_load_plus_area.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 2: a load plus an area measures nothing, so it does not
// compile. docs/tutorial/02-units-and-dimensions.md includes the line below.
#include <formula-cpp/formula.hpp>

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;

// --8<-- [start:mistake]
constexpr auto broken = formula::var<Load> + formula::var<Area>;
// --8<-- [end:mistake]

int main()
{
    return 0;
}
```

Register in `test/CMakeLists.txt`, beside the other dimension negatives (near line 338):

```cmake
# docs/tutorial/02-units-and-dimensions.md includes this file's mistake.
formula_add_negative_test(tutorial_load_plus_area
    "formula: the two sides of this addition or subtraction measure different dimensions"
    EXPECT_COUNT 1)
```

Before committing, follow the negative-test protocol: first register it with a wrong expected text (`"formula: wrong text"`) and confirm the test FAILS; then restore the right text and confirm it PASSES under `cl-debug` and `clangcl-debug`; then temporarily delete the `broken` line and confirm it FAILS (the file now compiles); restore it.

The page includes it with `--8<-- "test/negative/tutorial_load_plus_area.cpp:mistake"` and quotes the diagnostic in a plain ``` block as g++ writes it, with no file or line:

```
static assertion failed: formula: the two sides of this addition or subtraction measure different dimensions
```

`hygiene.documented-diagnostic-text` (extended in Task 2) checks that quote; run it.

**Chapter 3.** Copy chapter 2 (drop `StrengthInNewtons`). Measure the specimen as it really is, with exact decimal literals:

```cpp
// --8<-- [start:measured]
auto const specimen = formula::environment(formula::Measured<SideA> { 150.2_r },
                                           formula::Measured<SideB> { 149.8_r },
                                           formula::Measured<Load> { 675.4_r });
// --8<-- [end:measured]
```

Print the area, the strength exactly, and the strength rounded for reading with the format spec `{:~.2HalfEven}` (see `docs/display.md`); and one line showing `0.1_r + 0.2_r == 0.3_r` is true. Hand-computed values:

- area = 150.2 × 149.8 = 22499.96 mm² (exact decimal);
- strength = 675 400 N / 22 499.96 mm² = 16885000/562499 MPa ≈ 30.0178311 MPa: no terminating decimal, so the exact value prints as a fraction;
- rounded for reading: ≈30.02 MPa.

Expected output (fraction spelling and `≈` as the library prints them):

```text
A_c = 22499.96 mm2 (derived)
f_c = 16885000/562499 MPa (derived)
f_c, for reading = ≈30.02 MPa
0.1 + 0.2 == 0.3: yes
```

Page sections: "Exact decimal literals" (`_r` makes an exact `Rational`, never a `double`); "An exact result that has no decimal" (a fraction is printed rather than a rounded decimal that would be a different number); "Rounding for reading" (the format spec names places and a rounding mode; `~` marks the approximation; rounding as part of the method comes in chapter 5). Summary: `_r`, `Rational`, `{:~.NMode}`. Further reading: `../numbers.md`, `../display.md`.

- [ ] **Step 1:** Write both `.expected.txt` files from the values above.
- [ ] **Step 2:** Write `02_units_and_dimensions.cpp`, register it, build, run its output test. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write and register `tutorial_load_plus_area` with the wrong-text / right-text / deletion protocol above.
- [ ] **Step 4:** Write `03_exact_numbers.cpp`, register it, run its output test. Expected: PASS after spelling-only adjustments.
- [ ] **Step 5:** Write both pages; nav entries `- 2. Units and dimensions: tutorial/02-units-and-dimensions.md` and `- 3. Exact numbers: tutorial/03-exact-numbers.md`; index lines `2. [Units and dimensions](02-units-and-dimensions.md) -- sides, areas and unit conversion; a mistake that does not compile.` and `3. [Exact numbers](03-exact-numbers.md) -- why the results are exact, and how to round them for reading.`
- [ ] **Step 6:** Verification commands from *Tutorial conventions*, plus `ctest --preset clangcl-debug -R tutorial_load_plus_area`, then full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 7: Commit**

```bash
git add examples/tutorial/02_* examples/tutorial/03_* test/negative/tutorial_load_plus_area.cpp docs/tutorial/02-units-and-dimensions.md docs/tutorial/03-exact-numbers.md examples/CMakeLists.txt test/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on units and on exact numbers"
```

---

### Task 5: Chapters 4 and 5, Missing and entered values; Rounding

**Files:**
- Create: `examples/tutorial/04_missing_and_entered.cpp` + `.expected.txt`, `examples/tutorial/05_rounding.cpp` + `.expected.txt`
- Create: `docs/tutorial/04-missing-and-entered.md`, `docs/tutorial/05-rounding.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Interfaces:**
- Consumes: chapter 3's program.
- Produces (chapter 5, copied by chapter 6): `constexpr formula::DecimalRounding tenthMpa { unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero };` and `strength = formula::yields<Strength>(formula::rounded<tenthMpa>(var<Load> / loadedArea))`.

**Chapter 4.** Copy chapter 3. Three environments, each evaluated and checked:

1. all measured: sides 150, 150, load 675 → `f_c = 30 MPa (derived)`;
2. side b never measured, `formula::Measured<SideB>::absent()` → the evaluation succeeds and its outcome holds no number; show this with `formula::number_of(*result)` (empty `std::optional`) and print the outcome as the library prints an empty one (see `examples/expressions.cpp:84-90`);
3. the strength entered by hand: `formula::entered(formula::Measured<Strength> { 31 })` added to the all-measured environment → `f_c = 31 MPa (manually entered)`, with `result->is_overridden()` true.

Hand-computed expected output (empty-outcome spelling as the library prints it):

```text
measured:        f_c = 30 MPa (derived)
b not measured:  f_c = empty
entered by hand: f_c = 31 MPa (manually entered)
```

Page sections: "A measurement nobody took" (absence propagates through every operator; empty is not zero); "A value entered by hand" (`entered()` overrides the formula; `source()` says so, so a report can show which numbers were derived and which asserted). Summary: `Measured<Q>::absent()`, `entered()`, `number_of`, `ValueSource`, `is_overridden()`. Further reading: `../quantities.md`, `../expressions.md`.

**Chapter 5.** Copy chapter 4's all-measured case only. Add `tenthMpa` and the rounded `strength` above, plus a second rounding that differs only in mode:

```cpp
// --8<-- [start:roundings]
constexpr formula::DecimalRounding tenthMpa { unit::Megapascal, formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };
constexpr formula::DecimalRounding tenthMpaHalfEven { unit::Megapascal, formula::DecimalPlaces { 1 },
                                                      formula::RoundingMode::HalfEven };
// --8<-- [end:roundings]
```

Evaluate with sides 150 × 150 and loads 675.4 kN and 676.125 kN. Hand-computed: 675 400 / 22 500 = 30.0177… → 30.0 in both modes; 676 125 / 22 500 = 30.05 exactly → 30.1 half away from zero, 30.0 half to even. Expected output (decimal spelling as printed; a rounded exact value prints its declared places):

```text
675.4 kN, half away from zero: f_c = 30.0 MPa
676.125 kN, half away from zero: f_c = 30.1 MPa
676.125 kN, half to even:        f_c = 30.0 MPa
```

Page sections: "Rounding is part of the formula" (`rounded<R>` rounds at that position; the rounding names its unit, places and mode, and there is no default mode); "The mode matters" (30.05 → 30.1 or 30.0). Summary: `DecimalRounding`, `DecimalPlaces`, `RoundingMode`, `rounded<R>`. Further reading: `../rounding-and-conditionals.md`, `../numbers.md`.

- [ ] **Step 1:** Write both `.expected.txt` files from the values above.
- [ ] **Step 2:** Write, register and run `04_missing_and_entered.cpp`. Expected: output test PASSES after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `05_rounding.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Write both pages; nav `- 4. Missing and entered values: tutorial/04-missing-and-entered.md`, `- 5. Rounding: tutorial/05-rounding.md`; index lines `4. [Missing and entered values](04-missing-and-entered.md) -- a measurement nobody took, and a value typed in.` and `5. [Rounding](05-rounding.md) -- rounding where the method says, in the mode it names.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/04_* examples/tutorial/05_* docs/tutorial/04-missing-and-entered.md docs/tutorial/05-rounding.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on missing values and on rounding"
```

---

### Task 6: Chapters 6 and 7, Constraints; Citations, rendering and documentation

**Files:**
- Create: `examples/tutorial/06_constraints.cpp` + `.expected.txt`, `examples/tutorial/07_documentation.cpp` + `.expected.txt`
- Create: `docs/tutorial/06-constraints.md`, `docs/tutorial/07-documentation.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Interfaces:**
- Consumes: chapter 5's program (`tenthMpa`, rounded `strength`).
- Produces (chapter 7, copied by chapter 8): `strength` wrapped in `formula::documented(...)` with the citation `{ .title = "Compressive strength", .reference = "Example Standard 12:2020", .section = "6.1", .equation = "(1)", .text = "The maximum load divided by the area of the loaded face." }`, i.e. `formula::yields<Strength>(formula::documented(formula::rounded<tenthMpa>(var<Load> / loadedArea), { ... }))`.

**Chapter 6.** Copy chapter 5. Add constraints on the specimen's size, an invented tolerance of 150 mm ± 1 mm per side, following `examples/constraints.cpp:40-47` and `:135` (`formula::constraint`, `formula::Verdict`, `formula::constant<unit::Millimetre>(...)`, `formula::check`, `formula::check_all(formula::constraints(...), environment)`):

```cpp
// --8<-- [start:constraints]
constexpr auto sideAWithinTolerance = formula::constraint(
    var<SideA> >= formula::constant<unit::Millimetre>(149) && var<SideA> <= formula::constant<unit::Millimetre>(151),
    formula::Verdict { "reject the specimen: side a out of tolerance" });
// --8<-- [end:constraints]
```

If the predicate language has no `&&`, write two constraints per side (`a >= 149 mm`, `a <= 151 mm`) and check all four with `check_all`; record which form was used in the task report. Do the same for side b. Three specimens:

1. 150.2 × 149.8 → every constraint satisfied;
2. 150.2 × 152.5 → side b violated, verdict "reject the specimen: side b out of tolerance";
3. side b absent → not checked.

Print each outcome as `examples/constraints.cpp` prints them. Expected output: one block per specimen naming each constraint's outcome word (`satisfied`, `violated`, `not checked`) and the verdict for the violation. Write it by hand from these cases, then adjust spelling only.

Page sections: "A rule the result must meet" (a constraint pairs a predicate with a verdict); "Four outcomes, not two" (satisfied, violated, not checked -- an unmeasured side is never reported as satisfied -- and invalid, when checking itself fails, with a link to the guide for it); "Checking several rules" (`check_all` checks every rule, without stopping at the first failure). Summary: `constraint`, `Verdict`, `constant`, `check`, `check_all`, `constraints`. Further reading: `../constraints.md`.

**Chapter 7.** Copy chapter 5's all-measured program (not chapter 6's constraints). Wrap `strength` in `documented()` as in *Interfaces* (see `examples/citations.cpp:37-43`). Include `<formula-cpp/render.hpp>` and `<formula-cpp/document.hpp>`. Print:

- `formula::render(strength)` -- the plain-text rendering;
- `formula::render<formula::Dialect::LaTeX>(strength)`;
- from `formula::Documentation const page = formula::document(strength);`, each `page.symbols` entry as `symbol  unit  description` (the loop in `docs/index.md`'s cycling section: `for (formula::SymbolEntry const& entry: page.symbols)`), and each citation's title and reference from `page.citations`;
- the strength value, 30.0 MPa for 150 × 150 at 675 kN.

Expected output: write by hand (the text rendering will read like `f_c = round(F / (a * b), 0.1 MPa)` -- take the library's exact spelling from the run; the symbol table has rows for `f_c`, `F`, `a`, `b` with units `MPa`, `kN`, `mm`, `mm`; the citation reads `Compressive strength` / `Example Standard 12:2020`). The reviewer checks the rows and the citation against the declarations.

Page sections: "Cite where the formula comes from" (`documented()` attaches a citation and changes nothing that is calculated); "Render it" (text and LaTeX from the same declaration); "Generate its documentation" (`document()` returns the rendering, the symbol table and the citations: everything a report's methods section needs). Summary: `documented`, `Citation` fields, `render`, `Dialect::LaTeX`, `document`, `Documentation`, `SymbolEntry`. Further reading: `../citations.md`, `../gallery.md`.

- [ ] **Step 1:** Write both `.expected.txt` files by hand from the cases above.
- [ ] **Step 2:** Write, register and run `06_constraints.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `07_documentation.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Pages; nav `- 6. Constraints: tutorial/06-constraints.md`, `- 7. Citations, rendering and documentation: tutorial/07-documentation.md`; index lines `6. [Constraints](06-constraints.md) -- rules a specimen must meet, and the four outcomes of checking one.` and `7. [Citations, rendering and documentation](07-documentation.md) -- where a formula comes from, written out as text, LaTeX and a symbol table.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/06_* examples/tutorial/07_* docs/tutorial/06-constraints.md docs/tutorial/07-documentation.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on constraints and on documentation"
```

---

### Task 7: Chapters 8 and 9, Tracing; Calculations and worksheets

**Files:**
- Create: `examples/tutorial/08_tracing.cpp` + `.expected.txt`, `examples/tutorial/09_worksheets.cpp` + `.expected.txt`
- Create: `docs/tutorial/08-tracing.md`, `docs/tutorial/09-worksheets.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Interfaces:**
- Consumes: chapter 7's documented `strength`.

**Chapter 8.** Copy chapter 7 (drop the rendering and documentation printing). Include `<formula-cpp/trace.hpp>` and `<formula-cpp/trace_render.hpp>`. Use `checked_explain`, never the throwing `explain`, following `examples/display.cpp:151-163`:

```cpp
// --8<-- [start:explain]
auto const explained = formula::checked_explain<Strength>(strength, specimen);
if (!explained)
{
    std::println("cannot explain the strength: {}", explained.error().error);
    std::print("{}", formula::render_trace(explained.error().trace, { .maxSteps = 10 }));
    return 1;
}
std::print("{}", formula::render_trace(explained->trace, { .maxSteps = 10 }));
// --8<-- [end:explain]
```

`checked_explain` returns `std::expected<Explained<Strength>, CheckedExplainFailure>`: on success the outcome and its trace; on an arithmetic error the error and the trace recorded up to it. Then explain with the strength entered by hand (checked the same way) and show that `explained->trace` is empty and `explained->outcome.is_overridden()` is true. Inputs: sides 150.2 and 149.8 mm, load 675.4 kN. Hand-computed steps: `a = 150.2 mm`, `b = 149.8 mm`, the product `22499.96 mm2`, `F = 675.4 kN`, the quotient `16885000/562499 MPa`, the rounding to `30.0 MPa`, and the documented wrapper citing `Example Standard 12:2020, 6.1, (1)`. Write the expected output in `render_trace`'s numbered-step form (see `docs/tracing.md`), then adjust spelling only.

Page sections: "How a number was reached" (`checked_explain` returns what `checked_evaluate` would, plus one step per node, each naming the steps it consumed; on failure, the steps up to the failure); "Reading a trace" (inputs in their declared units, the citation on its step); "A value entered by hand has no derivation" (empty trace; `is_overridden()`); one sentence that tracing costs nothing when not asked for. Summary: `checked_explain`, `Explained`, `Trace`, `render_trace`, `TraceRenderOptions::maxSteps`. Further reading: `../tracing.md`.

**Chapter 9.** Copy chapter 8 (drop tracing). Following `examples/electricity_bill.cpp:122-139, 229-260, 309-335`, put the whole test into one calculation:

```cpp
// --8<-- [start:quantities]
using Height = formula::Quantity<struct HeightTag, "h", "height of the specimen", unit::Millimetre>;
using Mass = formula::Quantity<struct MassTag, "m", "mass of the specimen", unit::Kilogram>;
using Volume = formula::Quantity<struct VolumeTag, "V", "volume of the specimen", unit::CubicMillimetre>;
using Density = formula::Quantity<struct DensityTag, "rho", "density of the specimen", unit::KilogramPerCubicMetre>;
// --8<-- [end:quantities]

// --8<-- [start:calculation]
inline constexpr auto test = formula::calculation(
    formula::define<Area>(var<SideA> * var<SideB>),
    formula::define<Volume>(var<Area> * var<Height>),
    formula::define<Density>(var<Mass> / var<Volume>),
    formula::define<Strength>(formula::documented(formula::rounded<tenthMpa>(var<Load> / var<Area>),
                                                  { .title = "Compressive strength",
                                                    .reference = "Example Standard 12:2020",
                                                    .section = "6.1",
                                                    .equation = "(1)",
                                                    .text = "The maximum load divided by the area of the loaded face." })));
// --8<-- [end:calculation]
```

Then a worksheet over sides 150 and 150 mm, height 150 mm, mass 8.1 kg, load 675 kN; `sheet.calculate<Density, Strength>()`; then `sheet.set(formula::Measured<Load> { 676.125_r })` and calculate again, printing `sheet.recomputed()` / `sheet.reused()` deltas as `examples/electricity_bill.cpp` does; then a what-if copy `sheet.with(formula::Measured<Mass> { 8.25_r })` and the original left unchanged. Check every value before reading it.

Hand-computed:
- area 22500 mm², volume 3 375 000 mm³ = 0.003375 m³, density 8.1 / 0.003375 = 2400 kg/m³, strength 30.0 MPa;
- after the load change: strength 30.1 MPa (30.05 half away from zero); only the strength is recalculated (area, volume, density reused);
- what-if mass 8.25 kg: density 2444.444… = 22000/9 kg/m³ (no terminating decimal; printed as a fraction unless rounded for reading with `{:~.1HalfEven}` → ≈2444.4); the original worksheet still reports 2400 kg/m³.

Write the expected output from these values, one line per step, then adjust spelling only. The reviewer checks the recomputed/reused counts against the dependency graph: a load change reaches only `Strength`.

Page sections: "One calculation, several steps" (definitions in any order; the calculation works out what reads what, at compile time); "A worksheet" (calculates on request, keeps results); "Change an input" (only what the change reaches is recalculated); "What if" (`with()` answers on a copy). Summary: `calculation`, `define<Q>`, `worksheet`, `calculate<...>`, `set`, `with`, `recomputed`, `reused`. Further reading: `../calculations.md`.

- [ ] **Step 1:** Write both `.expected.txt` files by hand from the values above.
- [ ] **Step 2:** Write, register and run `08_tracing.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `09_worksheets.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Pages; nav `- 8. Tracing: tutorial/08-tracing.md`, `- 9. Calculations and worksheets: tutorial/09-worksheets.md`; index lines `8. [Tracing](08-tracing.md) -- how each number was reached, step by step.` and `9. [Calculations and worksheets](09-worksheets.md) -- the whole test as one calculation, recalculated as inputs change.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/08_* examples/tutorial/09_* docs/tutorial/08-tracing.md docs/tutorial/09-worksheets.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on tracing and on worksheets"
```

---

### Task 8: Advanced chapters 10 and 11, Lookup tables; Methods and overlays

**Files:**
- Create: `examples/tutorial/10_lookup_tables.cpp` + `.expected.txt`, `examples/tutorial/11_methods_and_overlays.cpp` + `.expected.txt`
- Create: `docs/tutorial/10-lookup-tables.md`, `docs/tutorial/11-methods-and-overlays.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml` (nav after chapter 9), `docs/tutorial/index.md` (`## Advanced track`)

**Interfaces:**
- Consumes: nothing from earlier chapters' programs; each advanced program is self-contained and may redeclare `Load`, `Strength` and so on.

Advanced pages use the same template, open by saying what problem the feature solves in the strength test, and end their Further reading with the reference guide, which goes deeper.

**Chapter 10.** A size correction factor for the strength, looked up by the specimen's edge length from an invented banded table, following `examples/lookup_tables.cpp:61-70` and the lookup it evaluates there:

- bands: 0 to under 100 mm → 1.05; 100 to under 200 mm → 1.00; 200 to under 300 mm → 0.95;
- corrected strength = strength × factor;
- edge 150 mm, strength 30 MPa → factor 1.00 → 30 MPa; edge 100 mm (on a band boundary, lower edge inclusive) → 1.00 → 30 MPa; edge 250 mm → 0.95 → 28.5 MPa; edge 300 mm → outside every band → the lookup reports an error (domain), and the program prints it rather than a number.

Page sections: why a published table belongs in the formula; band boundaries (lower inclusive, upper exclusive; spelt "0 to under 100 mm"); a miss is not a number; a gap between bands does not compile (link to the guide). Further reading: `../lookup-tables.md`.

**Chapter 11.** One method with two variants, cube and cylinder, selected by the specimen's shape, and one jurisdiction overlay, following `examples/methods_and_overlays.cpp` (its variants by tag and its overlay that changes a rounding rule):

- cube: `f_c = F / (a * b)`; cylinder: `f_c = F / (pi * d^2 / 4)` with `formula::pi`;
- the base method rounds to 0.1 MPa half away from zero; the overlay "Example jurisdiction" rounds to 0.5 MPa;
- cube 150 × 150 at 675 kN → 30.0 MPa under the base method, 30.0 under the overlay; cylinder d = 150 mm at 530 kN → 530 000 / (π × 5625) with the library's exact π; compute it by hand with the same rational π the library uses (`formula::pi`, see `include/formula-cpp/constant.hpp` or the guide) before running; the overlay rounds the same value to the nearest 0.5;
- print which variant ran and whose rounding rule applied, as the trace states it.

Page sections: one method, several variants; an overlay changes a method for a jurisdiction and says so in the trace. Further reading: `../methods-and-overlays.md`.

- [ ] **Step 1:** Compute every expected value by hand and write both `.expected.txt` files.
- [ ] **Step 2:** Write, register and run `10_lookup_tables.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `11_methods_and_overlays.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Pages; nav `- 10. Lookup tables: tutorial/10-lookup-tables.md`, `- 11. Methods and overlays: tutorial/11-methods-and-overlays.md`; index lines under `## Advanced track`: `10. [Lookup tables](10-lookup-tables.md) -- values from a published table, and what happens outside it.` and `11. [Methods and overlays](11-methods-and-overlays.md) -- one method, several variants, and a jurisdiction's changes.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/10_* examples/tutorial/11_* docs/tutorial/10-lookup-tables.md docs/tutorial/11-methods-and-overlays.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on lookup tables and on methods"
```

---

### Task 9: Advanced chapters 12 and 13, Series; Statistics

**Files:**
- Create: `examples/tutorial/12_series.cpp` + `.expected.txt`, `examples/tutorial/13_statistics.cpp` + `.expected.txt`
- Create: `docs/tutorial/12-series.md`, `docs/tutorial/13-statistics.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Chapter 12.** One mix's strength at 7, 14 and 28 days as a series, following `examples/series.cpp` (a `MeasuredSeries`, elementwise arithmetic, `explain_series` or evaluation as that example does):

- strengths 21, 26 and 30 MPa;
- each as a share of the 28-day strength: 21/30 = 0.7, 26/30 = 13/15 (prints as a fraction, or ≈0.867 when rounded for reading), 30/30 = 1;
- the 14-day strength not recorded (absent): that element is absent, the others are still calculated.

Page sections: one quantity at several points; arithmetic element by element; an absent element stays absent and does not stop the others. Further reading: `../series.md`.

**Chapter 13.** The strengths of a set of specimens, following `examples/statistics.cpp` (`formula::sample_mean`, a spread, and outlier rejection with `formula::without_outliers` as that example and `examples/display.cpp:101-108` use it):

- three specimens 30.2, 29.8 and 31.0 MPa: mean 91/3 ≈ 30.33 MPa, range 1.2 MPa;
- five specimens 30.2, 29.8, 31.0, 30.4 and 36.0 MPa with an invented rule "reject a value more than 3 MPa from the mean of the others, one per pass": 36.0 is rejected (the mean of the other four is 30.35); then mean of the remaining four = 30.35 MPa, nothing further rejected;
- print the rejected value and the final mean as the library traces them.

If the library's outlier API cannot express "from the mean of the others" exactly, use the nearest rule its guide documents, state it in one sentence on the page, and recompute the expected values by hand for that rule.

Page sections: summary statistics over a sample; outliers rejected pass by pass, and the trace says which. Further reading: `../statistics.md`.

- [ ] **Step 1:** Compute every expected value by hand and write both `.expected.txt` files.
- [ ] **Step 2:** Write, register and run `12_series.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `13_statistics.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Pages; nav `- 12. Series: tutorial/12-series.md`, `- 13. Statistics: tutorial/13-statistics.md`; index lines `12. [Series](12-series.md) -- one quantity at several ages, calculated element by element.` and `13. [Statistics](13-statistics.md) -- means and spreads of several specimens, and outliers rejected.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/12_* examples/tutorial/13_* docs/tutorial/12-series.md docs/tutorial/13-statistics.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on series and on statistics"
```

---

### Task 10: Advanced chapters 14 and 15, Records; Opaque operations and bounded retry

**Files:**
- Create: `examples/tutorial/14_records.cpp` + `.expected.txt`, `examples/tutorial/15_opaque_and_retry.cpp` + `.expected.txt`
- Create: `docs/tutorial/14-records.md`, `docs/tutorial/15-opaque-and-retry.md`
- Modify: `examples/CMakeLists.txt`, `mkdocs.yml`, `docs/tutorial/index.md`

**Chapter 14.** Comparing a specimen with a reference sample's strength read by role, following `examples/records.cpp` (records, reading by role, the record each value came from in the trace):

- the specimen's strength 30.0 MPa; the reference sample's 32.0 MPa, from a record;
- the ratio 30/32 = 15/16 = 0.9375 (exact decimal);
- the trace names the record the reference value came from;
- a reference record not yet made: the ratio is absent, not zero.

Page sections: a value from another sample or an earlier test; where each value came from; a record not yet made. Further reading: `../records.md`.

**Chapter 15.** Following `examples/opaque_and_retry.cpp`:

- a least-squares line through load and displacement readings (invented, chosen to lie exactly on load = 100 kN/mm × displacement + 5 kN: (0.1, 15), (0.2, 25), (0.3, 35), (0.4, 45)) → slope 100 kN/mm, intercept 5 kN, and R² = 1 if the observation form is used; the trace shows the operation's inputs and outputs and marks its inside as not shown;
- a retest repeated at most 3 times until two consecutive results differ by at most 0.5 MPa: invented results 30.0, 31.2, 31.0 → accepted on the third attempt (|31.0 - 31.2| = 0.2 ≤ 0.5); print how the retry ended, as the library names it.

Compute the regression by hand before running (exact data, so the fit is exact). Page sections: a named operation whose inside is not traced; a step repeated a bounded number of times, ending in exactly one of a fixed set of ways. Further reading: `../opaque-and-retry.md`.

- [ ] **Step 1:** Compute every expected value by hand and write both `.expected.txt` files.
- [ ] **Step 2:** Write, register and run `14_records.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 3:** Write, register and run `15_opaque_and_retry.cpp`. Expected: PASS after spelling-only adjustments.
- [ ] **Step 4:** Pages; nav `- 14. Records: tutorial/14-records.md`, `- 15. Opaque operations and bounded retry: tutorial/15-opaque-and-retry.md`; index lines `14. [Records](14-records.md) -- values from a reference sample or an earlier test.` and `15. [Opaque operations and bounded retry](15-opaque-and-retry.md) -- a least-squares fit, and a retest repeated at most a fixed number of times.`
- [ ] **Step 5:** Verification commands; full `ctest --preset cl-debug`. Expected: all pass.
- [ ] **Step 6: Commit**

```bash
git add examples/tutorial/14_* examples/tutorial/15_* docs/tutorial/14-records.md docs/tutorial/15-opaque-and-retry.md examples/CMakeLists.txt mkdocs.yml docs/tutorial/index.md
git commit -m "docs: add tutorial chapters on records and on opaque operations"
```

---

### Task 11: The README

**Files:**
- Modify: `README.md` (rewrite)
- Modify: `examples/CMakeLists.txt` (README snippet and output checks)

**Interfaces:**
- Consumes: `examples/readme.cpp` region `program`, `examples/readme.expected.txt` (Task 1); tutorial page URLs (Tasks 3-7).

- [ ] **Step 1: Pin the README to its program**

Add after the `readme` registration in `examples/CMakeLists.txt`:

```cmake
# README.md shows examples/readme.cpp: its one ```cpp block must be a run of
# that source's lines, and its one ```text block a run of the lines it prints.
# GitHub renders the README directly, so it cannot include them as the site's
# pages do.
add_test(NAME docs.readme-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/readme.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/README.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")
add_test(NAME docs.readme-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-readme>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/README.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset cl-debug && ctest --preset cl-debug -R "docs\.readme"`
Expected: `docs.readme-snippets` FAILS (the current README's ```cpp blocks are from other examples).

- [ ] **Step 3: Rewrite `README.md`**

Exactly this structure; the code block is the `program` region of `examples/readme.cpp` copied verbatim (without the marker lines), the output block is `examples/readme.expected.txt` verbatim. No other ```cpp or ```text block may appear in the README (install snippets use ```cmake and ```bash).

````markdown
# formula-cpp

**[Documentation](https://lastrada-software.github.io/formula-cpp/)** ·
[Tutorial](https://lastrada-software.github.io/formula-cpp/tutorial/) ·
[API reference](https://lastrada-software.github.io/formula-cpp/api/)

[![Build](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/build.yml)
[![Package](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/package.yml/badge.svg?branch=master)](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/package.yml)
[![Pages](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/pages.yml/badge.svg?branch=master)](https://github.com/LASTRADA-Software/formula-cpp/actions/workflows/pages.yml)
[![codecov](https://codecov.io/gh/LASTRADA-Software/formula-cpp/branch/master/graph/badge.svg)](https://codecov.io/gh/LASTRADA-Software/formula-cpp)
[![Release](https://img.shields.io/github/v/release/LASTRADA-Software/formula-cpp)](https://github.com/LASTRADA-Software/formula-cpp/releases/latest)
[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)](#installation)
[![Compilers](https://img.shields.io/badge/compilers-MSVC%20%7C%20clang--cl%20%7C%20Clang%20%7C%20GCC%2014%20%7C%20AppleClang-informational.svg)](#requirements)
[![Docs](https://img.shields.io/badge/docs-online-blue.svg)](https://lastrada-software.github.io/formula-cpp/)

Declarative, traceable, self-documenting formulas for C++23. Header-only, no dependencies.

Software that implements a test method usually keeps the formula in one place, its units in
another, where it comes from in a comment, and its audit trail in code written afterwards. Those
drift apart. In formula-cpp they are one declaration: write the formula once, with ordinary
operators, and get the number, its units, its derivation and its documentation from it.

## Example

The compressive strength of a concrete specimen: the load that crushed it over the area that
carried it.

```cpp
<examples/readme.cpp, region program, verbatim>
```

```text
<examples/readme.expected.txt, verbatim>
```

- **Units are part of the type.** Kilonewtons over square millimetres arrive in megapascals with no
  conversion written. ([Units and dimensions](https://lastrada-software.github.io/formula-cpp/tutorial/02-units-and-dimensions/))
- **Mistakes are compile errors.** `formula::var<Load> + formula::var<Area>` does not compile.
  ([Units and dimensions](https://lastrada-software.github.io/formula-cpp/tutorial/02-units-and-dimensions/))
- **Arithmetic is exact.** The 30 is an exact rational, not a `double`.
  ([Exact numbers](https://lastrada-software.github.io/formula-cpp/tutorial/03-exact-numbers/))

## What you get

- **Dimensional analysis at compile time**, with exact unit conversion. ([Dimensions and units](https://lastrada-software.github.io/formula-cpp/dimensions/))
- **Exact arithmetic**, rounded only where you say, in the mode you name. ([Exact numbers](https://lastrada-software.github.io/formula-cpp/numbers/))
- **Missing and entered values** that never pass for computed ones. ([Quantities and measurements](https://lastrada-software.github.io/formula-cpp/quantities/))
- **The method's own rounding, constraints and tables**, as part of the formula. ([Rounding and conditionals](https://lastrada-software.github.io/formula-cpp/rounding-and-conditionals/), [Constraints](https://lastrada-software.github.io/formula-cpp/constraints/), [Lookup tables](https://lastrada-software.github.io/formula-cpp/lookup-tables/))
- **Traces** that show how every number was reached. ([Tracing](https://lastrada-software.github.io/formula-cpp/tracing/))
- **Rendering and generated documentation**: text, LaTeX, symbol tables and citations from the same declaration. ([Citations and rendering](https://lastrada-software.github.io/formula-cpp/citations/))

New to the library? Start with the [tutorial](https://lastrada-software.github.io/formula-cpp/tutorial/).

## Installation

### CPM

```cmake
CPMAddPackage("gh:LASTRADA-Software/formula-cpp@0.4.0")
target_link_libraries(your_target PRIVATE formula-cpp::formula-cpp)
```

### CMake, from an install tree

```bash
cmake -S . -B build -DFORMULA_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --install build
```

```cmake
find_package(formula-cpp CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE formula-cpp::formula-cpp)
```

### Copy the headers

`include/` is self-contained and depends on nothing outside the standard library.
`formula.hpp` is the umbrella header. `format.hpp`, `render.hpp`, `document.hpp`, `trace.hpp` and
`trace_render.hpp` are separate, because they need `<string>`, `<vector>` or `<format>`: include
them by name when you print, render, document or trace.

## Requirements

- C++23
- CMake 3.23 or newer
- GCC 14 or newer, if you build with GCC

CI builds and tests every push with MSVC `cl`, `clang-cl`, Clang and GCC 14 on Linux, and
AppleClang on macOS.

## Build options

<the existing "Build options" table, unchanged>

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

Apache-2.0. See [`LICENSE`](LICENSE).
````

Before committing, check the `format.hpp` claim against `CONTRIBUTING.md` invariant 4 and `include/formula-cpp/formula.hpp`: list exactly the headers the umbrella header leaves out.

- [ ] **Step 4: Verify**

Run: `ctest --preset cl-debug -R "docs\.readme|example\.readme|hygiene\."`
Expected: all PASS. Open the README in a Markdown preview and confirm every badge and link resolves (the Codecov badge shows "unknown" until the owner enables the repository; that is expected).

- [ ] **Step 5: Commit**

```bash
git add README.md examples/CMakeLists.txt
git commit -m "docs: rewrite the README as a short landing page"
```

---

### Task 12: The site's home page

**Files:**
- Modify: `docs/index.md` (rewrite)

**Interfaces:**
- Consumes: `examples/readme.cpp:program`, `examples/readme.expected.txt` (Task 1); `tutorial/index.md` (Task 2).

- [ ] **Step 1: Rewrite `docs/index.md`**

Sections, in order:

1. `# formula-cpp`, the one-line description, and the README's pitch paragraph.
2. `## Example` -- the README's sentence, then
   ````markdown
   ```cpp
   --8<-- "examples/readme.cpp:program"
   ```

   ```text
   --8<-- "examples/readme.expected.txt"
   ```
   ````
3. `## Start here` -- "New to the library? The [tutorial](tutorial/index.md) builds a complete test step by step. The guides below are the reference: each covers one part of the library in depth."
4. `## Guides` -- the existing guide table, unchanged, plus the line that every guide has a runnable program under `examples/`.
5. `## A larger example` -- one paragraph: `examples/cycling_speed.cpp` works out a cyclist's steady-state speed from power as a calculation on a worksheet, with a citation, LaTeX rendering and a symbol table; link to it on GitHub.
6. `## A note on the examples` -- the existing paragraph, unchanged.
7. The closing licence and source line.

Remove: the cycling walkthrough, "One calculation, four answers", "Three things worth knowing", the "Status" table.

- [ ] **Step 2: Verify**

Run: `mkdocs build --strict` and `ctest --preset cl-debug -R "docs\.|hygiene\."`. Expected: PASS. Open `site/index.html` and confirm the example and its output render.

- [ ] **Step 3: Commit**

```bash
git add docs/index.md
git commit -m "docs: make the site's home page a short entry point"
```

---

### Task 13: The CPM tag follows the version

**Files:**
- Modify: `cmake/CheckVersionConsistency.cmake`
- Modify: `test/CMakeLists.txt:2946-2950` (`hygiene.version`)

**Interfaces:**
- Consumes: `README.md` and `docs/tutorial/01-first-formula.md`, each containing `CPMAddPackage("gh:LASTRADA-Software/formula-cpp@<version>")`.

- [ ] **Step 1: Write the failing check**

Append to `cmake/CheckVersionConsistency.cmake`, before the final `message(STATUS ...)`:

```cmake
# The README and the tutorial tell a consumer which release to fetch with CPM.
# A version bump that leaves either behind sends every new user to an old
# release, so each must name exactly this version, at least once, and never
# another.
foreach(document IN ITEMS README TUTORIAL)
    if(NOT DEFINED ${document})
        message(FATAL_ERROR "CheckVersionConsistency.cmake: ${document} is not set")
    endif()
    file(READ "${${document}}" text)
    string(REGEX MATCHALL "gh:LASTRADA-Software/formula-cpp@[0-9]+\\.[0-9]+\\.[0-9]+" tags "${text}")
    if(NOT tags)
        message(FATAL_ERROR "version drift: ${${document}} names no CPM tag gh:LASTRADA-Software/formula-cpp@<version>")
    endif()
    foreach(tag IN LISTS tags)
        string(REGEX REPLACE "^.*@" "" tagVersion "${tag}")
        if(NOT tagVersion STREQUAL fromHeader)
            message(FATAL_ERROR "version drift: ${${document}} fetches ${tagVersion} with CPM, the project is ${fromHeader}")
        endif()
    endforeach()
endforeach()
```

Add to the `hygiene.version` command in `test/CMakeLists.txt`:

```cmake
            -D "README=${PROJECT_SOURCE_DIR}/README.md"
            -D "TUTORIAL=${PROJECT_SOURCE_DIR}/docs/tutorial/01-first-formula.md"
```

- [ ] **Step 2: Prove it fails on drift**

Temporarily change the README's tag to `@0.3.0`. Run: `cmake --preset cl-debug && ctest --preset cl-debug -R hygiene.version`. Expected: FAIL with `fetches 0.3.0 with CPM, the project is 0.4.0`. Restore `@0.4.0`; expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add cmake/CheckVersionConsistency.cmake test/CMakeLists.txt
git commit -m "test: fail when the README or tutorial fetches another release"
```

---

### Task 14: The current-state check

**Files:**
- Create: `cmake/CheckDocsCurrentState.cmake`
- Create: `cmake/docs-current-state-allowlist.txt`
- Create: `cmake/TestDocsCurrentState.cmake`
- Modify: `test/CMakeLists.txt` (register the self-test only; the real-tree test comes in Task 15)

**Interfaces:**
- Produces: `cmake -D SOURCE_DIR=<dir> -P cmake/CheckDocsCurrentState.cmake` -- scans `<dir>/README.md`, `<dir>/docs/**/*.md` except `docs/superpowers/`, and the comment lines (`//`, `///`, `/*`, ` *`) of `<dir>/include/**/*.hpp`; fails listing every `file:line: phrase` not allowed by `<dir>/cmake/docs-current-state-allowlist.txt`. An optional `ALLOWLIST` variable overrides the allow-list path.

- [ ] **Step 1: Write the self-test first**

`cmake/TestDocsCurrentState.cmake` builds a fixture tree in `WORK_DIR` and runs the check on it twice:

```cmake
# SPDX-License-Identifier: Apache-2.0
# The current-state check must fail on history wording, name every offence,
# and accept a line its allow-list names. This builds a small tree in WORK_DIR
# and runs the check on it, so the check is tested on text written to break it,
# not only on a tree that happens to pass.
cmake_minimum_required(VERSION 3.23)

foreach(variable CHECK_SCRIPT WORK_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "TestDocsCurrentState.cmake: ${variable} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(WRITE "${WORK_DIR}/README.md" "A Bounds written positionally no longer compiles.\n")
file(WRITE "${WORK_DIR}/docs/guide.md"
    "This was previously a warning.\n"
    "The two determinations no longer agree.\n"
    "New in 0.4.0: literals.\n"
    "The factor is used to round the strength, in 2.5 mm steps.\n"
    "It used to return a double.\n")
file(WRITE "${WORK_DIR}/docs/superpowers/plan.md" "This used to be ignored, and is.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp"
    "/// Formerly named old_name.\n"
    "int previously_named = 0; // code, not a comment line: not scanned\n")
file(WRITE "${WORK_DIR}/allow.txt"
    "docs/guide.md|The two determinations no longer agree.|describes data, not history\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -D "SOURCE_DIR=${WORK_DIR}" -D "ALLOWLIST=${WORK_DIR}/allow.txt"
                        -P "${CHECK_SCRIPT}"
                RESULT_VARIABLE exitCode OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(report "${out}${err}")

if(exitCode EQUAL 0)
    message(FATAL_ERROR "the check passed a tree full of history wording:\n${report}")
endif()
foreach(expected IN ITEMS "README.md:1: no longer" "docs/guide.md:1: previously" "docs/guide.md:3: new in"
                          "docs/guide.md:3: in 0.4.0" "docs/guide.md:5: used to" "include/formula-cpp/x.hpp:1: formerly")
    string(FIND "${report}" "${expected}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the check did not report '${expected}':\n${report}")
    endif()
endforeach()
foreach(unexpected IN ITEMS "docs/guide.md:2:" "docs/guide.md:4:" "superpowers" "x.hpp:2:")
    string(FIND "${report}" "${unexpected}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "the check reported '${unexpected}', which it must not:\n${report}")
    endif()
endforeach()

# With every offending line removed, the same tree passes.
file(WRITE "${WORK_DIR}/README.md" "A Bounds is written with designated initialisers.\n")
file(WRITE "${WORK_DIR}/docs/guide.md" "The two determinations no longer agree.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp" "/// Named new_name.\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -D "SOURCE_DIR=${WORK_DIR}" -D "ALLOWLIST=${WORK_DIR}/allow.txt"
                        -P "${CHECK_SCRIPT}"
                RESULT_VARIABLE exitCode OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "the check failed a clean tree:\n${out}${err}")
endif()
message(STATUS "TestDocsCurrentState: the check fails on history wording and passes a clean tree")
```

Register it in `test/CMakeLists.txt` next to `hygiene.no-real-standards-fallback`:

```cmake
# The current-state check, tested on a tree written to break it.
add_test(NAME hygiene.docs-current-state-self-test
    COMMAND "${CMAKE_COMMAND}"
            -D "CHECK_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/CheckDocsCurrentState.cmake"
            -D "WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/docs-current-state-self-test"
            -P "${PROJECT_SOURCE_DIR}/cmake/TestDocsCurrentState.cmake")
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset cl-debug && ctest --preset cl-debug -R docs-current-state-self-test`
Expected: FAIL (the check script does not exist).

- [ ] **Step 3: Write `cmake/CheckDocsCurrentState.cmake`**

Requirements, in this order:

- Header comment: the rule (user documentation describes the library as it is now; history belongs in `CHANGELOG.md`), what is scanned, the allow-list format, and that a scan that examined no file fails.
- `cmake_minimum_required(VERSION 3.23)`; require `SOURCE_DIR`; `ALLOWLIST` defaults to `${SOURCE_DIR}/cmake/docs-current-state-allowlist.txt` and may be absent (an absent file means an empty list).
- Files: `README.md` if it exists; `file(GLOB_RECURSE ... "${SOURCE_DIR}/docs/*.md")` filtered to exclude `/docs/superpowers/`; `file(GLOB_RECURSE ... "${SOURCE_DIR}/include/*.hpp")`. Fail if none was found.
- Read each file with `file(STRINGS ... )` is unsafe for `;` and `[`; read with `file(READ)`, normalise `\r\n`, and walk lines with `string(FIND)` / `string(SUBSTRING)` as `CheckGuideOutput.cmake` does. Never turn the text into a CMake list.
- For `.hpp` files only lines whose first non-blank characters are `//`, `/*` or `*` are scanned.
- Lower-case each scanned line, then test these regexes; each match reports `<path relative to SOURCE_DIR>:<line number>: <the phrase as matched, lower-cased>`:
  - `no longer`, `previously`, `formerly`, `once meant`, `was renamed`, `deprecated`, `new in`
  - `used to`, **except** when the word before it is `is`, `are`, `was`, `were`, `be`, `been` or `being` ("the factor is used to round" is the passive, not history); check the preceding word with `string(FIND)` on the lower-cased line, not a lookbehind, which CMake's regex lacks
  - `before [^.]* existed`
  - `(since|as of) v?[0-9]+\.[0-9]+(\.[0-9]+)?` and `(in|until|before|after) v?[0-9]+\.[0-9]+\.[0-9]+` (a release reference: "since 0.3", "in 0.4.0"; a two-part number after "in" is a measurement, "in 2.5 mm", and is not matched)
- Skip a line when the allow-list has an entry `<relative path>|<exact line text, trimmed>|<reason>` for it. Allow-list lines starting with `#` and blank lines are ignored; an entry without a reason is itself an error.
- Report every offence, sorted by file then line, then `FATAL_ERROR` with the count and one sentence: "User documentation describes the library as it is now; say what it does today, or record the change in CHANGELOG.md. A line that describes data rather than history goes in cmake/docs-current-state-allowlist.txt with its reason."

`cmake/docs-current-state-allowlist.txt`, initial content:

```text
# Lines in user documentation that match a history phrase but describe the
# library as it is now, read by cmake/CheckDocsCurrentState.cmake. One per line:
#
#   <path relative to the repository root>|<the line, exactly, trimmed>|<why it is not history>
#
# Keep it short: every entry is a place the check does not look.
```

- [ ] **Step 4: Run to verify the self-test passes**

Run: `ctest --preset cl-debug -R docs-current-state-self-test`
Expected: PASS. Then confirm the patterns are narrow enough on real prose by running the check on the real tree (`cmake -D SOURCE_DIR=. -P cmake/CheckDocsCurrentState.cmake`) and recording the number of hits it reports. It is expected to FAIL there; Task 15 clears it.

- [ ] **Step 5: Commit**

```bash
git add cmake/CheckDocsCurrentState.cmake cmake/docs-current-state-allowlist.txt cmake/TestDocsCurrentState.cmake test/CMakeLists.txt
git commit -m "test: add a check that user documentation describes only the present"
```

---

### Task 15: The current-state sweep

**Files:**
- Modify: every file `cmake/CheckDocsCurrentState.cmake` reports (expected: several `docs/*.md` guides, `README.md` if any remains, and doc comments in `include/formula-cpp/*.hpp`)
- Modify: `cmake/docs-current-state-allowlist.txt`
- Modify: `test/CMakeLists.txt` (register `hygiene.docs-current-state`)

**Interfaces:**
- Consumes: the check (Task 14).

- [ ] **Step 1: Register the real-tree test and watch it fail**

```cmake
# User documentation -- README.md, docs/ (not docs/superpowers/) and the doc
# comments that form the API reference -- describes the library as it is now.
# History belongs in CHANGELOG.md. See CONTRIBUTING.md, "Documentation".
add_test(NAME hygiene.docs-current-state
    COMMAND "${CMAKE_COMMAND}"
            -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckDocsCurrentState.cmake")
```

Run: `cmake --preset cl-debug && ctest --preset cl-debug -R "hygiene.docs-current-state$" --output-on-failure`
Expected: FAIL, listing every hit. Save the list in the task report.

- [ ] **Step 2: Rewrite or allow-list each hit**

For each reported line, decide:

- **History** (says what the library did before, what changed, or when): rewrite it to state current behaviour only. Example -- `docs/dimensions.md` around line 412-419:

  Before: "Each flag is a `BoundsEnd`, which reads as a `bool` but only a `bool` sets, so a `Bounds` written positionally before the two flags existed no longer compiles: in `{ true, 0, 1, 100, 1 }`, once 0 to 100, the 0 lands on `highPresent`. Only `{}` and `{ false }`, which declare no bounds, and `{ true }`, which once meant 0 to 0 and now declares a minimum of 0, still compile. Write `bounds()`, `at_least()`, `at_most()` or designated initialisers."

  After: "Each flag is a `BoundsEnd`, which reads as a `bool` but can be set only from a `bool`, so a positional initialiser that puts a number where a flag belongs does not compile: in `{ true, 0, 1, 100, 1 }` the 0 would land on `highPresent`. `{}` and `{ false }` declare no bounds, and `{ true }` declares a minimum of 0. Write `bounds()`, `at_least()`, `at_most()` or designated initialisers."

  A doc comment in `include/` that names a former spelling is rewritten the same way; if the history it carries is not yet in `CHANGELOG.md`, add it there under `## [Unreleased]`.
- **Not history** (the words describe data or a process: "the two determinations no longer agree", "the net draw is no longer read" in a worksheet's recalculation): add an allow-list entry with its reason.

Rewrites must stay accurate: re-read the surrounding paragraph and the code it describes. Where a guide's ```text or ```cpp block changes, run that guide's `docs.*` tests.

- [ ] **Step 3: Run to verify it passes**

Run: `ctest --preset cl-debug -R "hygiene\.|docs\."`
Expected: all PASS, `hygiene.docs-current-state` included.

- [ ] **Step 4: Full suite, Doxygen, site**

Run: `ctest --preset cl-debug`, `cmake --build out/build/cl-debug --target formula-cpp-docs-api` (if the doc comments changed; expect no new Doxygen warnings), `mkdocs build --strict`. Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add docs include cmake/docs-current-state-allowlist.txt test/CMakeLists.txt CHANGELOG.md README.md
git commit -m "docs: describe the library as it is now, and enforce it"
```

---

### Task 16: Coverage and Codecov

**Files:**
- Modify: `CMakePresets.json`
- Create: `.github/workflows/coverage.yml`
- Create: `codecov.yml`

- [ ] **Step 1: Add the preset**

In `configurePresets`, after `clang-ubsan`:

```json
        { "name": "clang-coverage", "displayName": "clang++ source-based coverage", "inherits": "posix",
          "cacheVariables": {
              "CMAKE_BUILD_TYPE": "Debug",
              "CMAKE_C_COMPILER": "clang",
              "CMAKE_CXX_COMPILER": "clang++",
              "CMAKE_CXX_FLAGS": "-fprofile-instr-generate -fcoverage-mapping",
              "CMAKE_EXE_LINKER_FLAGS": "-fprofile-instr-generate"
          } }
```

and matching entries in `buildPresets` and `testPresets` (same shape as `clang-ubsan`).

- [ ] **Step 2: Write `.github/workflows/coverage.yml`**

```yaml
# SPDX-License-Identifier: Apache-2.0
#
# Measures how much of the library the test suite runs, and reports it to
# Codecov. Only include/ is counted: the tests, examples and fetched
# dependencies are what does the running, not what is measured. The report is
# informational (codecov.yml): it never fails a pull request.
#
# Uploading needs the CODECOV_TOKEN repository secret, which the repository
# owner adds once on codecov.io and in Settings > Secrets. Without it the
# upload step fails and the job is red; the build and tests above it still
# show whether the suite passed.
name: Coverage
on:
  push:
    branches: [ master ]
  pull_request:

concurrency:
  group: coverage-${{ github.ref }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}

permissions:
  contents: read

env:
  CPM_SOURCE_CACHE: ${{ github.workspace }}/.cache/CPM
  # The same Clang as build.yml's Linux legs.
  CLANG_VERSION: "22"

jobs:
  coverage:
    name: Linux-clang-coverage
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Restore CPM cache
        uses: actions/cache/restore@v4
        with:
          path: ${{ env.CPM_SOURCE_CACHE }}
          key: cpm-${{ runner.os }}-${{ hashFiles('test/CMakeLists.txt') }}
          restore-keys: cpm-${{ runner.os }}-

      - uses: seanmiddleditch/gha-setup-ninja@v6

      # Retried as a whole, for the reason build.yml gives.
      - name: Install Clang ${{ env.CLANG_VERSION }}
        run: |
          for attempt in 1 2 3; do
            if wget -q https://apt.llvm.org/llvm.sh \
               && chmod +x llvm.sh \
               && sudo ./llvm.sh ${{ env.CLANG_VERSION }} \
               && sudo apt-get install -y clang-${{ env.CLANG_VERSION }} llvm-${{ env.CLANG_VERSION }}; then
              exit 0
            fi
            echo "::warning::Clang install attempt $attempt failed; retrying"
            sleep $((attempt * 15))
          done
          echo "::error::Clang ${{ env.CLANG_VERSION }} could not be installed after 3 attempts"
          exit 1

      - name: Configure
        run: cmake --preset clang-coverage -DCMAKE_CXX_COMPILER=clang++-${{ env.CLANG_VERSION }} -DCMAKE_C_COMPILER=clang-${{ env.CLANG_VERSION }}

      - name: Build
        run: cmake --build --preset clang-coverage

      # Every test process writes its own raw profile (%p is its process id).
      - name: Test
        env:
          LLVM_PROFILE_FILE: ${{ github.workspace }}/out/profiles/%p.profraw
        run: ctest --preset clang-coverage

      - name: Export the coverage of include/
        run: |
          set -euo pipefail
          llvm-profdata-${{ env.CLANG_VERSION }} merge -sparse out/profiles/*.profraw -o out/coverage.profdata
          # llvm-cov takes the first binary positionally and every other one
          # after -object.
          mapfile -t binaries < <(find out/build/clang-coverage -type f -perm -u+x -name 'formula-cpp-*' | sort)
          test "${#binaries[@]}" -gt 0
          others=()
          for exe in "${binaries[@]:1}"; do others+=(-object "$exe"); done
          llvm-cov-${{ env.CLANG_VERSION }} export -format=lcov -instr-profile=out/coverage.profdata \
            "${binaries[0]}" "${others[@]}" \
            -ignore-filename-regex='(^|/)(test|examples|tools|support|_deps|\.cache)/' \
            > out/coverage.lcov
          test -s out/coverage.lcov

      - name: Upload to Codecov
        uses: codecov/codecov-action@v5
        with:
          files: out/coverage.lcov
          token: ${{ secrets.CODECOV_TOKEN }}
          fail_ci_if_error: true
```

Before relying on the `find` expression, list the executables the build produces (`find out/build/clang-coverage -type f -perm -u+x`) in the first CI run and adjust the name pattern so every test and example binary is passed and nothing else (test executables may be named differently from examples' `formula-cpp-example-*`; check `test/CMakeLists.txt`'s `add_executable` names). Negative tests are never built (`EXCLUDE_FROM_ALL`, and expected not to compile).

- [ ] **Step 3: Write `codecov.yml`**

```yaml
# SPDX-License-Identifier: Apache-2.0
# Coverage is reported on every pull request but never blocks one.
coverage:
  status:
    project:
      default:
        informational: true
    patch:
      default:
        informational: true
ignore:
  - "test/**"
  - "examples/**"
  - "tools/**"
  - "support/**"
```

- [ ] **Step 4: Verify**

Run on Windows: `cmake --list-presets` (the new preset is hidden there, by its `posix` condition) and `ctest --preset cl-debug -R hygiene.spdx` (the new YAML files carry the SPDX line). The workflow itself is verified by its first CI run on the pull request; record in the task report that it has not run locally.

- [ ] **Step 5: Commit**

```bash
git add CMakePresets.json .github/workflows/coverage.yml codecov.yml
git commit -m "ci: measure the library's test coverage and report it to Codecov"
```

---

### Task 17: CONTRIBUTING.md and the changelog

**Files:**
- Modify: `CONTRIBUTING.md`
- Modify: `CHANGELOG.md` (`## [Unreleased]`)

- [ ] **Step 1: Add the "Documentation" section to `CONTRIBUTING.md`**

After the "Invariants" list, a new `## Documentation` section with:

1. **The current-state rule**: user documentation -- `README.md`, everything under `docs/` except `docs/superpowers/`, and the doc comments in `include/` that form the API reference -- describes the library as it is now. It never says what the library used to do, what changed, or in which release something appeared; that belongs in `CHANGELOG.md`. `ctest -R hygiene.docs-current-state` enforces it; a line that matches a history phrase but describes data goes in `cmake/docs-current-state-allowlist.txt` with its reason.
2. **The tutorial**: each chapter is a page in `docs/tutorial/` and a program in `examples/tutorial/`. The page includes code with `--8<-- "examples/tutorial/<file>.cpp:<region>"` and output with `--8<-- "examples/tutorial/<file>.expected.txt"`; regions are marked in the program with `// --8<-- [start:<region>]` and `// --8<-- [end:<region>]`. When a program's output changes, update its `.expected.txt` (`example.<name>.output` fails until you do) and re-read the page. `mkdocs build --strict` fails on a missing file or region; it runs on every pull request.
3. **The README** shows `examples/readme.cpp` and its output verbatim; `docs.readme-snippets` and `docs.readme-output` check them. The CPM tag in the README and tutorial chapter 1 must equal the project version (`hygiene.version`).
4. **The consumer-globals test** -- move here, verbatim apart from tense, the README paragraph beginning "`test/consumer_globals_tests.cpp` declares 258 ordinary globals".

- [ ] **Step 2: Changelog**

Under `## [Unreleased]`, `### Added`:

```markdown
- **A tutorial** on the documentation site, in two tracks: nine chapters that build one
  compressive-strength test step by step, and six independent chapters on the rest of the library.
  Its code and output are included from programs the test suite builds and runs.
- **Coverage reporting**: a workflow measures the test suite's coverage of `include/` and reports
  it to Codecov.
```

and under `### Changed` (create the heading if absent):

```markdown
- The README is a short landing page: badges, a link to the documentation, a minimal example,
  and installation with CPM, `find_package` or the headers alone. The vcpkg section is removed,
  as no port is published.
- The documentation describes the library as it is now; what changed, and when, is recorded only
  in this changelog. A test enforces it.
```

- [ ] **Step 3: Verify and commit**

Run: `ctest --preset cl-debug -R "hygiene\."` and `mkdocs build --strict`. Expected: PASS.

```bash
git add CONTRIBUTING.md CHANGELOG.md
git commit -m "docs: document the documentation rules, and record this change"
```

---

### Task 18: Licence detection (requires the owner's decision before it runs)

**Files:**
- Modify: `LICENSE`

The repository's `LICENSE` differs from the Apache License 2.0 text in its terms, not only its layout: compared word by word with the canonical text (https://www.apache.org/licenses/LICENSE-2.0.txt), it differs in dozens of places -- for example, section 2's "direct or contributory patent infringement" wording, a grant to "sell copies" that the canonical text does not contain, and the notice-exclusion clause of section 4(d). That is why GitHub reports "Other". The appendix's copyright line (`Copyright 2026 Yaraslau Tamashevich`) is the only intended difference.

This task changes the legal text of the project's licence. **Do not run it until the owner has explicitly confirmed** that the canonical Apache-2.0 text is what the project is licensed under. If confirmed:

- [ ] **Step 1:** Replace `LICENSE` with the canonical text from https://www.apache.org/licenses/LICENSE-2.0.txt, byte for byte (LF line endings), keeping the appendix exactly as the canonical file has it and appending nothing else.
- [ ] **Step 2:** Verify: a whitespace-normalised word diff against the canonical file is empty. After the branch is pushed, `gh api repos/LASTRADA-Software/formula-cpp/license --jq .license.spdx_id` on the branch's PR head reports `Apache-2.0` once merged.
- [ ] **Step 3: Commit**

```bash
git add LICENSE
git commit -m "chore: use the Apache License 2.0 text verbatim"
```

If the owner decides otherwise, this task is dropped; the README's licence badge is static and correct either way.

---

### Task 19: Finish

- [ ] **Step 1:** Whole-branch review (`sdd-reviewer`) against the spec and this plan.
- [ ] **Step 2:** Every preset green: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release` on Windows; `clang-debug`, `clang-release`, `gcc-release` (g++-14), `clang-ubsan` on Linux; every negative test on cl and clang-cl.
- [ ] **Step 3:** `mkdocs build --strict` and the Doxygen target with no new warnings.
- [ ] **Step 4:** Push the branch and open the PR as a draft; drive CI to green, including the new Coverage and Pages builds; mark it ready (`gh pr ready`). The owner reviews and merges.
- [ ] **Step 5:** Tell the owner the two settings outside the repository: enable the repository on codecov.io and add the `CODECOV_TOKEN` secret.
