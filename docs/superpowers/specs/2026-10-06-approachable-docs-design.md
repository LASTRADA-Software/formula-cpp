# Approachable README, a tutorial, and documentation of the current state only

## Goal

A C++ developer who has never seen formula-cpp should understand what it is
for within a minute of opening the repository, and be able to learn it step by
step from there. Concretely:

1. `README.md` becomes a short landing page with a minimal example, status
   badges and a link to the user documentation at the very top.
2. A new tutorial on the documentation site teaches the library incrementally,
   from the first formula to the advanced features.
3. The user documentation describes the library as it is now, and never its
   past. History belongs only in `CHANGELOG.md`. A test enforces this.

## Readers

Experienced modern C++ developers: fluent in C++20/23, `std::expected`,
`constexpr` and class-type template arguments, but new to this library and to
test-method standards. The tutorial explains the library's concepts, not the
language.

The tone is professional and neutral throughout. Page and chapter titles are
plain and descriptive ("Tutorial", "Units and dimensions"). Informal labels for
the tutorial do not appear in any page.

## 1. README.md

The README becomes a landing page of roughly 120 lines and stops duplicating the
documentation site. In order:

1. **A link line** at the very top: **Documentation**
   (<https://lastrada-software.github.io/formula-cpp/>) · Tutorial · API
   reference.
2. **Badges.**
   - Live: the Build, Package and Pages workflows (GitHub workflow badges),
     Codecov, and the latest GitHub release.
   - Static: licence (Apache-2.0, linking to `LICENSE`), C++23, header-only, and
     the supported compilers (MSVC, clang-cl, Clang, GCC 14, AppleClang).
   - A "docs" badge linking to the documentation site.
3. **A one-paragraph pitch**: what the library is, and the problem it solves --
   a test method's formula, its units, its provenance and its audit trail
   drifting apart in ordinary code.
4. **A minimal example** (below), with the output it prints.
5. **Three one-line points** under the example, each linking to its tutorial
   chapter: kN over mm² arrives in MPa with no conversion written; `var<Load> +
   var<Area>` does not compile; the result is an exact rational, not a
   `double`.
6. **What you get**: about six one-line bullets, each linking to its guide --
   compile-time units, exact arithmetic, absent versus entered values, the
   method's rounding, traces, rendering and generated documentation.
7. **Installation**, three paths, all of which work today:
   - CPM: `CPMAddPackage("gh:LASTRADA-Software/formula-cpp@0.4.0")`;
   - `find_package(formula-cpp CONFIG REQUIRED)` from an install tree;
   - copying `include/`, with the note on the opt-in headers (`render.hpp`,
     `document.hpp`, `trace.hpp`, `trace_render.hpp`, `format.hpp`).
   The vcpkg section is removed: no port is published.
   The CPM line names the current release. `cmake/CheckVersionConsistency.cmake`
   is extended to fail when that tag differs from the project version, in the
   README and in tutorial chapter 1, so a version bump cannot leave it behind.
8. **Requirements** and **build options** (the existing tables), and a link to
   `CONTRIBUTING.md`.
9. **Licence.**

Removed from the README: the cycling walkthrough, the "See it work" sections,
the "shipped" status table, the sentence naming the latest release (the release
badge shows it), and the consumer-globals paragraph, which moves to
`CONTRIBUTING.md`.

### The minimal example

The simplest complete program the library allows, in the domain the tutorial
opens with. It is a real program under `examples/` (`examples/readme.cpp`),
built and run by CTest. Every name is written with `formula::`; there is no
`using namespace`. The result is checked before it is read.

```cpp
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
        std::println("cannot calculate: {}", formula::describe(result.error()));
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), *result);
}
```

The README shows exactly what the program prints (expected `f_c = 30 MPa`;
the implementation pins the real text, including any source marker). The
README is rendered by GitHub, so it cannot include files: its code block is
pinned by the existing `cmake/CheckGuideSnippets.cmake`, and its output block
by `cmake/CheckGuideOutput.cmake`, both run against `examples/readme.cpp`.

## 2. The tutorial

### Files

- Pages: `docs/tutorial/index.md` and one page per chapter,
  `docs/tutorial/NN-<name>.md`.
- Programs: `examples/tutorial/NN_<name>.cpp`, each with its expected output
  checked in beside it as `examples/tutorial/NN_<name>.expected.txt`.
- CTest builds and runs every program; a new `cmake/CheckExpectedOutput.cmake`
  fails the test when the program's output differs from its `.expected.txt`,
  byte for byte after normalising line endings.

### Code and output are included, never copied

The tutorial pages include their code and output from the files above with
`pymdownx.snippets`, so the published pages cannot drift from what compiles and
runs.

- A program marks each region a page shows with comment markers:
  `// --8<-- [start:evaluate]` and `// --8<-- [end:evaluate]`.
- A page includes a region with `--8<-- "examples/tutorial/01_first_formula.cpp:evaluate"`,
  and the output with `--8<-- "examples/tutorial/01_first_formula.expected.txt"`.
- `mkdocs.yml` enables `pymdownx.snippets` with `base_path: ["."]` and
  `check_paths: true`. With the existing `mkdocs build --strict`, a missing
  file fails the Pages build. The implementation verifies that a missing
  *section* fails it too, and adds a check of its own if it does not.
- On GitHub the tutorial pages show the raw `--8<--` lines; they are written to
  be read on the documentation site, which the README links to.

The existing guides keep their current checks; moving them to includes is out of
scope.

The one block in the tutorial that is not included from a compiled file is the
consumer's CMake setup in chapter 1 (`CPMAddPackage` and
`target_link_libraries`), because the repository builds the tutorial programs as
its own targets. Its version tag is covered by the version check in section 1.

### Chapter structure

Every chapter page has the same sections:

1. **What this chapter covers** -- two or three sentences.
2. **The code** -- included regions, each followed by its explanation.
3. **The output** -- included from `.expected.txt`.
4. **Summary** -- the API this chapter introduced, as a short list.
5. **Further reading** -- the matching reference guide and the API reference.

### Core track: one compressive-strength test

Each core chapter's program is complete and compiles on its own, and extends the
previous chapter's program, so a reader can diff two consecutive chapters to see
exactly what was added. Every standard cited is an invented `Example Standard`.

| # | Chapter | Introduces |
|---|---|---|
| 1 | First formula | Setup with CPM; `Quantity`, `var`, `yields`, `environment`, `checked_evaluate`; handling the `std::expected` result |
| 2 | Units and dimensions | The area from the side lengths; kN over mm² as MPa; a dimensional mistake as a compile error |
| 3 | Exact numbers | `Rational`, `_r` literals, exact versus approximate printing |
| 4 | Missing and entered values | A measurement nobody took, a value entered by hand, `ValueSource` |
| 5 | Rounding as the method specifies | A rounding node; named rounding modes |
| 6 | Constraints and verdicts | A tolerance on the specimen's size; the four-state outcome |
| 7 | Citations, rendering, documentation | `documented()`; plain-text and LaTeX rendering; `document()` and its symbol table |
| 8 | Tracing | `explain()`, `render_trace()` |
| 9 | Calculations and worksheets | Density and strength as one `formula::calculation`; recalculating what a change reaches; what-if copies |

Chapter 2's compile error is quoted by its `static_assert` message, which is
stable, tested API, not by one compiler's full output. The implementation
confirms that `hygiene.documented-diagnostic-text` scans `docs/tutorial/`, and
extends it if it does not.

### Advanced track: independent chapters

Each is one self-contained program. It uses the strength test where that fits
naturally and its own small example otherwise, and ends by pointing into its
reference guide.

| # | Chapter | Example |
|---|---|---|
| 10 | Lookup tables | A correction factor looked up by specimen shape |
| 11 | Methods and overlays | Cube and cylinder variants; one jurisdiction overlay |
| 12 | Series | Strength at 7, 14 and 28 days |
| 13 | Statistics | Mean and spread of three specimens; outlier rejection |
| 14 | Records | Reading a reference sample and a prior test |
| 15 | Opaque operations and bounded retry | A least-squares fit; a retest repeated at most a fixed number of times |

### Tutorial index page

`docs/tutorial/index.md` states the prerequisites (a C++23 compiler, CMake 3.23
or newer, CPM), describes the two tracks, and says the core track is meant to be
read in order.

## 3. Documentation site

- `mkdocs.yml` navigation becomes: Home · Tutorial (index, then the chapters) ·
  Guides (the existing reference pages) · Gallery · API reference.
- `docs/index.md` stops duplicating the README. It holds the pitch; the README's
  minimal example, included from `examples/readme.cpp` and its expected output
  via snippets; **Start here**, pointing to the tutorial; the guides table; and
  the existing note that every standard cited is invented. The cycling
  walkthrough and the "shipped" table are removed from it;
  `examples/cycling_speed.cpp` stays, linked as a larger worked example.

## 4. Documentation describes the current state only

### The rule

User documentation is `README.md`, everything under `docs/` except
`docs/superpowers/`, and the doc comments in `include/` that form the API
reference. It describes the library as it is now: what it does, what it
refuses, how to use it. It never says what the library used to do, what
changed, or in which release something appeared. That belongs only in
`CHANGELOG.md`.

`CONTRIBUTING.md` gains a "Documentation" section stating this rule.

### The check

`cmake/CheckDocsCurrentState.cmake`, run as the CTest `hygiene.docs-current-state`:

- Scans `README.md`, `docs/**/*.md` except `docs/superpowers/`, and the comment
  lines of `include/**/*.hpp`.
- Fails on history wording, case-insensitively: "no longer", "previously",
  "used to", "formerly", "once meant", "before … existed", "was renamed",
  "deprecated", "new in", and release references such as "since 0.3",
  "in 0.4.0" or "as of 0.2".
- Accepts a hit listed in `cmake/docs-current-state-allowlist.txt`, by file and
  exact line text, the same pattern as `cmake/real-standards-allowlist.txt`.
  Each entry carries a one-line reason. Legitimate uses -- "the two
  determinations no longer agree" describes data, not history -- go there.
- Reports every offending file, line and phrase, not just the first.

### The sweep

Every existing hit is rewritten to describe current behaviour only, or
allow-listed with its reason. For example, the `Bounds` passage in
`docs/dimensions.md` that explains what a positional initialiser "written before
the two flags existed" does today is rewritten to state what compiles and what
does not. `CHANGELOG.md` is out of scope for both the check and the sweep.

## 5. Coverage and Codecov

- `CMakePresets.json` gains `clang-coverage`: Linux only, Clang 22, source-based
  coverage (`-fprofile-instr-generate -fcoverage-mapping`), Debug.
- `.github/workflows/coverage.yml` runs on push to `master` and on pull
  requests: installs Clang 22 the way `build.yml` does (with its retry), builds,
  runs `ctest`, merges the raw profiles with `llvm-profdata`, exports LCOV with
  `llvm-cov export` restricted to `include/` (tests, examples and `_deps` are
  excluded), and uploads with `codecov/codecov-action@v5` using the
  `CODECOV_TOKEN` secret.
- `codecov.yml` makes the project and patch statuses informational: coverage is
  reported on pull requests but never blocks one.
- Owner setup, outside the repository: enable the repository on codecov.io and
  add the `CODECOV_TOKEN` secret.

## 6. Licence detection

GitHub reports the repository's licence as "Other" although `LICENSE` is the
Apache-2.0 text with its appendix filled in. The implementation runs GitHub's
detector (`licensee`) against `LICENSE`, finds what stops the match, and fixes
it without changing the copyright line. The README's licence badge is static,
so it is correct independently of this.

## 7. CONTRIBUTING.md

Gains:

- the "Documentation" section with the current-state rule and its check;
- how to add or change a tutorial chapter: the snippet markers, and updating
  `.expected.txt` when a program's output changes;
- the consumer-globals paragraph moved from the README.

## Testing

- Every tutorial program and `examples/readme.cpp` builds on all CI compilers
  and its output matches its `.expected.txt` (`cmake/CheckExpectedOutput.cmake`).
- The README's code and output blocks are pinned to `examples/readme.cpp`.
- `mkdocs build --strict` fails on a missing included file or section.
- `hygiene.docs-current-state` passes on the swept tree, and is shown to fail on
  a fixture containing each flagged phrase.
- The coverage workflow produces an LCOV report limited to `include/` and
  uploads it.

## Out of scope

- Moving the existing guides to snippet includes.
- A vcpkg port.
- Coverage thresholds that block a pull request.
- Rewriting `CHANGELOG.md`.
