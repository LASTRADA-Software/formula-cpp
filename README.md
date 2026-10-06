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

Software that implements a test method (a standard's procedure for testing a material) usually
keeps the formula in one place, its units in another, where it comes from in a comment, and its
audit trail in code written afterwards. Those drift apart. In formula-cpp they are one
declaration: write the formula once, with ordinary operators, and get the number, its units, its
derivation and its documentation from it.

## Example

The compressive strength of a concrete specimen: the load that crushed it, 675 kN, over the area
that carried it, 22500 mm².

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
        std::println("cannot calculate: {}", result.error());
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), *result);
}
```

```text
f_c = 30 MPa
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
- **Missing and entered values** that never pass for computed ones. ([Quantities and measurements](https://lastrada-software.github.io/formula-cpp/quantities/), [Missing and entered values](https://lastrada-software.github.io/formula-cpp/tutorial/04-missing-and-entered/))
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

CI builds and tests every push to master and every pull request with MSVC `cl` and `clang-cl`
on Windows, Clang and GCC 14 on Linux, and AppleClang on macOS.

## Build options

| Option | Default | Effect |
|---|---|---|
| `FORMULA_BUILD_TESTS` | ON when top-level | Build the test suite (fetches Catch2) |
| `FORMULA_BUILD_EXAMPLES` | ON when top-level | Build the examples |
| `FORMULA_BUILD_DOCS` | ON when top-level | Configure the Doxygen API-reference target (never in the default build) |
| `FORMULA_TOOLS` | ON when top-level | Build the project's own tooling |
| `FORMULA_INSTALL` | ON when top-level | Generate install and export rules |
| `FORMULA_PEDANTIC` | ON | Strict warnings on the project's own targets |
| `FORMULA_WERROR` | OFF | Treat warnings as errors |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

Apache-2.0. See [`LICENSE`](LICENSE).
