# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only.

A formula is written once with ordinary operators, carries its own documentation
and provenance, and from that single declaration you can get a number, an audit
trail, a rendering, and a documentation page.

## Status

Early. The foundation, the numeric and dimensional layers, and the expression
layer described in `docs/superpowers/specs/2026-09-23-formula-cpp-design.md`
are in place and are being extended phase by phase; constraints and rendering
are still to come.

`examples/simple.cpp` and `examples/expressions.cpp` demonstrate the
declarative expression layer the library exists for: formulas written with
ordinary operators, carrying their own provenance, dimensionally checked at
compile time, and evaluated into an outcome rather than a bare number. The
prototype evaluator that originally stood in for it -- a type-keyed lazy
dependency graph built from lambda functors -- has been retired. See
[`docs/expressions.md`](docs/expressions.md) and the design specification
above for the full shape.

Exact rational arithmetic (`formula::Rational`) and norm-style rounding (`formula::round` and the
seven `formula::RoundingMode` values) are available now and verified by the test suite. See
[`docs/numbers.md`](docs/numbers.md) and the worked example in `examples/exact_numbers.cpp`.

Compile-time dimensional analysis (`formula::Dimension`, checked by `RequireSameDimension`) and
units (`formula::Unit`, with exact conversion, declared display precision and optional bounds) are
also available now. See [`docs/dimensions.md`](docs/dimensions.md) and the worked example in
`examples/dimensions_and_units.cpp`.

A quantity type (`formula::Quantity`, carrying its own symbol, description and unit), a single
metadata access point for it and for foreign types alike (`formula::Describe`), and a measurement
that may honestly be absent (`formula::Measured`) are also available now. See
[`docs/quantities.md`](docs/quantities.md) and the worked example in `examples/quantities.cpp`.

Formulas written with ordinary operators (`formula::var`, `+`, `-`, `*`, `/`, `formula::pow`,
`formula::sqrt`, `formula::cbrt`, `formula::root`, `formula::pi`), a type-keyed environment of
inputs (`formula::Environment`), and evaluation into a traceable `formula::Outcome` -- a value, an
absence, or a manual override, never a silent zero -- are also available now. See
[`docs/expressions.md`](docs/expressions.md) and the worked example in `examples/expressions.cpp`.

## Requirements

- C++23
- CMake 3.23 or newer

Verified locally on MSVC 19.51 (`cl` and `clang-cl`) and Clang 22.1.3. CI
(`.github/workflows/build.yml`) additionally gates GCC 14 and AppleClang, but
this repository has no remote yet, so those legs have never actually run.
The minimum supported compiler versions are not yet settled -- see the open
questions in the design specification. Earlier versions may work but are
untested.

## Installation

### vcpkg

The primary consumption path.

### CMake, from an install tree

    cmake -S . -B build -DFORMULA_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/your/prefix
    cmake --install build

Then, in the consuming project:

    find_package(formula-cpp CONFIG REQUIRED)
    target_link_libraries(your_target PRIVATE formula-cpp::formula-cpp)

### Copy the headers

`include/` is self-contained and depends on nothing outside the standard library.

## Build options

| Option | Default | Effect |
|---|---|---|
| `FORMULA_BUILD_TESTS` | ON when top-level | Build the test suite (fetches Catch2) |
| `FORMULA_BUILD_EXAMPLES` | ON when top-level | Build the examples |
| `FORMULA_INSTALL` | ON when top-level | Generate install and export rules |
| `FORMULA_PEDANTIC` | ON | Strict warnings on the project's own targets |
| `FORMULA_WERROR` | OFF | Treat warnings as errors |

## Licence

Apache-2.0. See `LICENSE`.
