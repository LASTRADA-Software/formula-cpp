# Changelog

Notable changes to formula-cpp, in the form [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
describes. Nothing has been released yet: everything below is unreleased, and the public API may
change until 1.0.

## [Unreleased]

### Added

**Phase 1: a consumable project.** Apache-2.0 licensing, an `INTERFACE` CMake target with install
and export, presets for `cl`, `clang-cl`, Clang and GCC, Catch2 through a pinned CPM bootstrap, a
must-not-compile harness that asserts both that a build fails and that it fails with the library's
own message, and CI that installs the library and builds a consumer against it.

**Phase 2: exact numbers.** An exact rational type with checked arithmetic that reports overflow
rather than wrapping, and rounding to decimal places, significant digits and multiples under named
rounding modes.

**Phase 3: dimensions and units.** Dimensions with rational exponents, units with exact conversion
between them, and each unit's decimals and bounds.

**Phase 4: quantities.** Quantities declared once with their symbol, description and unit, read
through `Describe<T>`, and measurements that may be absent without being an error.

**Phase 5: formulas.** An expression layer with the arithmetic operators, powers and roots,
environments of measured values, and evaluation into an `Outcome` that holds a value, a verdict,
an invalid result or nothing, never a bare number that hides which.

**Phase 6: citations and documentation.** `documented()` citations on any part of a formula,
rendering in plain text, Markdown and LaTeX, generated documentation with a symbol table, a
MkDocs and Doxygen site, and a formula gallery produced by running the library.

**Phase 7: tracing.** Composable sinks, a trace that records every step of an evaluation, and
bounded rendering of it as an audit trail a person can check.

**Phase 8: rounding, conditionals and an escape hatch.** Rounding as a node of a formula,
`when()` conditionals, and `numeric_value_of`, which takes a number out of its unit only with a
stated justification that the trace records.

**Phase 9: constraints.** Constraints as peers of formulas, with verdicts recorded as trace steps,
and checking a whole set without stopping at the first failure.

**Phase 10: lookup tables.** Exact, banded and interpolating lookups over tables validated at
compile time for gaps, overlaps and order, each traced with the row it used or the reason it found
none. Lookup keys are shown by their enumerator names, or by an author's own spelling through
`EnumeratorName`.

**Phase 11: methods and jurisdiction overlays.** A method holds variants chosen by tag, a rounding
rule and constraints; `evaluate_method` and `check_method` evaluate and check it, and the trace
names the variant that ran and its position in the method as published. An overlay pins or prunes
variants, fixes a quantity with `with_constant`, defines one with `add_derived`, replaces a
variant's formula, changes the rounding rule and replaces the constraints. Every operation requires
a citation, each change is recorded in the trace with what the overlay cited, a fixed, derived or
replaced part is marked as such in the generated documentation, and an overlay that would silently
do nothing is refused. Overlays stack, the later one holding. A vocabulary renders a formula and its
trace in a jurisdiction's own symbols, and `TagName` spells a variant's tag.

### Fixed

- A local in `evaluate.hpp` made GCC 13.3 report `-Wmaybe-uninitialized` at `-O2`, a false
  positive, in a consumer's own build: under `-Werror` a program including the header failed to
  build. CI's GCC 14 does not report it.
- `check_all(constraints())`, over an empty set, made GCC 13 report
  `-Wunused-but-set-parameter` on the sink, in a consumer's own build under `-Werror`.
- Locals and parameters in the public headers shared names with ordinary globals such as
  `result`, `value`, `index`, `here` and `origin`, so cl at `/W4 /WX` reported C4459 in a consumer
  that declared one, and failed to build. They are renamed, and a test declares 258 such globals
  before every header.
- cl names an enumerator that is not one as a cast, `(enum Flag)true`; it was shown as a name, and
  is refused now.
- The opt-in header include check was not portable across the compilers it claimed to run on.
