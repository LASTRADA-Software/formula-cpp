# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only, no dependencies.

Write a formula once, with ordinary operators. Get back a number, a rendering, and a
documentation page — from the same declaration.

```cpp
#include <formula-cpp/formula.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>

namespace unit = formula::unit;
using formula::var;

// A quantity carries its own symbol, description and unit.
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", unit::Litre> {};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", unit::Litre> {};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", unit::One> {};

// The formula, and where it comes from, declared together.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)" });
```

That single declaration answers four different questions:

```cpp
formula::render(ratio);                          // "V_w / V_c"
formula::render<formula::Dialect::LaTeX>(ratio); // "\frac{V_w}{V_c}"

formula::document(ratio);   // the rendered formula, its citation, and a symbol table:
                            //   V_w = effective water content [l]
                            //   V_c = cement content [l]

auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                              formula::Measured<CementVolume> { formula::Rational { 300 } });
formula::evaluate<WaterCementRatio>(ratio, environment);   // 0.6, and it knows it computed it
```

The output above is what `examples/citations.cpp` actually prints.

## See it work

Every block below is real code from `examples/`, with the output those programs
actually print.

### A dimensional mistake is a compile error, not a wrong number

```cpp
constexpr auto broken = formula::var<Volume> + formula::var<Length>;
```

```
error C2338: static assertion failed: 'formula: the two sides of this addition
or subtraction measure different dimensions; the offending operands appear in
this diagnostic as the template arguments of RequireAddendsAgree'
```

The diagnostic names the two quantities and points at the line that wrote the
formula. Not at evaluation, not at a failing test, and not at a support ticket
six months later.

Asking an environment for a quantity it was never given fails the same way:

```
error C2338: static assertion failed: 'formula: this environment provides no
value for this quantity; the quantity and the environment appear in this
diagnostic as the template arguments of RequireProvided'
```

### Units convert themselves, across the whole formula

`Diameter` is declared in millimetres, `Area` in square metres. Nothing in the
formula mentions either — the conversion is part of what the declaration means.

```cpp
constexpr auto circularArea = formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 };

constexpr auto known = formula::environment(formula::Measured<Diameter> { formula::Rational { 100 } });
constexpr auto area  = formula::checked_evaluate<Area>(circularArea, known);
```

```
circular area of a 100 mm diameter = 0.007854 m2 (computed)
2500 g reported as m = 2.500000 kg
```

Note `constexpr`: that area was computed at compile time.

### A measurement nobody took stays missing

```cpp
constexpr auto unknown = formula::environment(formula::Measured<Diameter>::absent());
constexpr auto empty   = formula::checked_evaluate<Area>(circularArea, unknown);
```

```
area with no diameter measured: empty
```

Not `0.0`. An absence propagates through every operator and arrives at the
result still saying "nobody measured this" — which is a different statement
from "this is zero", and the difference matters when someone signs off on it.

### A number a person typed in never masquerades as a computed one

```cpp
auto const batch = formula::environment(
    formula::Measured<WaterVolume> { formula::Rational { 180 } },
    formula::Measured<CementVolume> { formula::Rational { 300 } },
    formula::entered(formula::Measured<WaterCementRatio> { formula::Rational { 1, 2 } }));

auto const ratio = formula::checked_evaluate<WaterCementRatio>(waterCementRatio, batch);
```

```
w/c = 0.500000 (entered)
```

The formula would have computed 0.6. A person entered 0.5, so that is the
answer — and `ratio->source()` says `ManuallyEntered`, so a report can show
which numbers were derived and which were asserted.

### Arithmetic that does not drift

```
volume = 450 ml = 9/20 l
round trip exact: yes
flow rate = 9/8 l/min
reported  = 1.13 l/min
one decimal, ceiling = 1.2
one decimal, nearest = 1.1
they differ: yes
ten tenths == one: yes
```

`9/20` is exact, not 0.450000000000000011. Ten tenths really do sum to one.
Rounding happens once, where you ask for it, in the mode you name — and the
last two lines are the reason that matters: the same number rounds to 1.2 or
1.1 depending on the rule the method specifies, and the library makes you say
which.

### The library documents itself

`document()` walks a formula for its rendered text, its citations and its
symbol table. [`docs/gallery.md`](docs/gallery.md) is a page generated that way
from several formulas at once — checked into the repository, and a CI test
fails if it ever stops matching what the generator produces.

### A derived number can show how it was reached

`explain()` evaluates a formula exactly as `evaluate()` does and also returns
a `Trace` — one step per node, each naming the earlier steps it consumed.
`render_trace()` turns that into text, bounded by a limit you choose:

```cpp
formula::Explained<WaterCementRatio> const explained = formula::explain<WaterCementRatio>(ratio, inputs);
std::string const trace = formula::render_trace(explained.trace, { .maxSteps = 10 });
```

```
1. V_w = 180 l
2. V_c = 300 l
3. #1 / #2 = 3/5
4. #3 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)]
```

Every value is shown in the unit it was declared in, not the coherent SI unit
the arithmetic actually ran on — that is `9/50` cubic metres above, and nobody
typed cubic metres. When the environment overrides the result instead of
letting the formula derive it, `explained.trace` comes back empty — nothing
ran, so nothing was recorded — and `explained.outcome.is_overridden()` says
so instead: an overridden number shows *that a person entered it*, a
different fact from how it was reached and arguably a more important one.
See [the tracing guide](docs/tracing.md) for the detail. Tracing costs
nothing when nobody asks for it: a sink is
passed by value, and the untraced path — `evaluate()`, `checked_evaluate()` —
defaults to one that does nothing, adding no instruction the evaluator would
not already emit once the call inlines, measured on all four compilers this
library targets. See [the tracing guide](docs/tracing.md).

## What this is for

Test-method standards are written as prose with formulas in them, and software
that implements them usually ends up with the formula in one place, its units
in another, its provenance in a comment, and its audit trail bolted on
afterwards. Those four drift apart.

Here they are one declaration. The formula is the documentation is the audit
trail. A reviewer reading a report can be shown the equation, the clause it
came from, the values that went in, and whether a human overrode the result —
because all of it came from the same line of code.

**No macros.** None of this is preprocessor machinery.

## Documentation

**<https://lastrada-software.github.io/formula-cpp/>** — guides and the generated API reference.

| Guide | What it covers |
|---|---|
| [Exact numbers](docs/numbers.md) | `Rational`, the rounding modes, why exactness is the default |
| [Dimensions and units](docs/dimensions.md) | Compile-time dimensional analysis, exact unit conversion |
| [Quantities](docs/quantities.md) | Declaring a quantity, `Describe`, measurements that may be absent |
| [Writing formulas](docs/expressions.md) | Operators, evaluation, environments, overrides |
| [Citations and rendering](docs/citations.md) | `documented()`, the three dialects, generated documentation |
| [Tracing and audit trails](docs/tracing.md) | `explain()`, `render_trace()`, sinks, and the zero-cost untraced path |
| [Gallery](docs/gallery.md) | A documentation page the library generated about itself |

Every example in the documentation uses generic physics with invented `Example Standard`
citations. Real standards are copyrighted, so none of their content appears in this repository.

Each guide has a matching runnable program under `examples/`.

## Status

Usable for what is listed as shipped, and still growing. The public API may change until 1.0.

| Area | State |
|---|---|
| Exact rational arithmetic, rounding modes | shipped |
| Dimensions with rational exponents, units, exact conversion | shipped |
| Quantities, metadata, absent measurements | shipped |
| Formulas, operators, environments, evaluation | shipped |
| Citations, rendering dialects, generated documentation | shipped |
| Calculation tracing and audit trails | shipped |
| Conditionals, lookup tables, constraints, series, statistics | planned |

## Requirements

- C++23
- CMake 3.23 or newer

CI builds and tests every push on MSVC `cl`, `clang-cl`, Clang and GCC 14 on Linux, and
AppleClang on macOS. Minimum compiler versions are not settled yet; earlier ones may work but
are untested.

## Installation

### vcpkg

The primary consumption path.

### CMake, from an install tree

```bash
cmake -S . -B build -DFORMULA_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --install build
```

Then, in the consuming project:

```cmake
find_package(formula-cpp CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE formula-cpp::formula-cpp)
```

### Copy the headers

`include/` is self-contained and depends on nothing outside the standard library.

`formula.hpp` is the umbrella header. `render.hpp`, `document.hpp`, `trace.hpp` and
`trace_render.hpp` are deliberately left out of it: they need `<string>` and/or `<vector>`, and a
consumer who only evaluates numbers should not compile those into every translation unit. Include
them by name when you want text, or a trace, or both — see
[the tracing guide](docs/tracing.md) for `trace.hpp` and `trace_render.hpp` specifically.

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

## Licence

Apache-2.0. See [`LICENSE`](LICENSE).
