# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only, no dependencies.

Write a formula once, with ordinary operators. Get back a number, a rendering, and a
documentation page — from the same declaration.

```cpp
namespace unit = formula::unit;
using formula::var;

// A quantity carries its own symbol, description and unit, and its tag makes it a type of its own.
using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

// The formula and its citation, declared together: documented() attaches the
// citation to the division, and forwards that division's dimension unchanged.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });
```

That single declaration answers four different questions: what the formula
is, as plain text and as LaTeX; what its symbols mean and where it comes from;
and what it computes.

```cpp
std::string const plain = formula::render(ratio);
std::string const latex = formula::render<formula::Dialect::LaTeX>(ratio);
formula::Documentation const documentation = formula::document(ratio);
auto const inputs = formula::environment(formula::Measured<WaterVolume> { 180 },
                                         formula::Measured<CementVolume> { 300 });
auto const result = formula::checked_evaluate<WaterCementRatio>(ratio, inputs);
```

`examples/citations.cpp` prints the four answers:

```
plain: V_w / V_c
latex: \frac{V_w}{V_c}
symbol: V_w = effective water content [l]
symbol: V_c = cement content [l]
citation: Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)
w/c = 0.6 (derived)
```

`0.6` is exact, and `derived` says the library computed it rather than a
person typing it in. The text comes from `render.hpp` and `document.hpp`, and
the printing from `format.hpp`; the umbrella header `formula.hpp` holds the
rest (see [Copy the headers](#copy-the-headers)).

A quantity can also be declared as a struct deriving from `formula::Quantity`,
`struct WaterVolume: formula::Quantity<WaterVolume, ...> {};`. Both spellings
are supported, and mix in one formula; [the quantities
guide](docs/quantities.md#declaring-a-quantity) says what each costs.

## See it work

Every block below is real code from `examples/`, with the output those programs
actually print. The one mistake that must not compile is shown from
`test/negative/`, which pins it.

### A dimensional mistake is a compile error, not a wrong number

```cpp
inline constexpr auto broken = formula::var<Volume> + formula::var<Length>;
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
constexpr auto circularArea = formula::yields<Area>(formula::pi * formula::pow<2>(var<Diameter>) / 4);
```

`yields<Area>` names the result where the formula is written, so the call that
evaluates it names none:

```cpp
constexpr auto diameterKnown = formula::environment(formula::Measured<Diameter> { 103 });
constexpr auto area = formula::checked_evaluate(circularArea, diameterKnown);
```

```
circular area of a 103 mm diameter = ≈0.008332 m2 (derived)
2500 g reported as m = 2.5 kg
```

Note `constexpr`: that area was computed at compile time. The area is held
exactly, with `formula::pi` an exact fraction close to pi, and has no short
decimal, so it is printed rounded to six places and marked `≈`.

### A measurement nobody took stays missing

```cpp
constexpr auto diameterUnknown = formula::environment(formula::Measured<Diameter>::absent());
constexpr auto emptyArea = formula::checked_evaluate(circularArea, diameterUnknown);
```

```
area with no diameter measured: empty
```

Not `0.0`. An absence propagates through every operator and arrives at the
result still saying "nobody measured this" — which is a different statement
from "this is zero", and the difference matters when someone signs off on it.

### A number a person typed in never masquerades as a computed one

```cpp
auto const batch = formula::environment(formula::Measured<WaterVolume> { 180 },
                                        formula::Measured<CementVolume> { 300 },
                                        formula::entered(formula::Measured<WaterCementRatio> { 0.5_r }));
auto const ratio = formula::checked_evaluate(waterCementRatio, batch);
```

```
w/c = 0.5 (manually entered)
```

Here `waterCementRatio` is
`formula::yields<WaterCementRatio>(var<WaterVolume> / var<CementVolume>)`, and
`0.5_r` is the exact decimal one half, never a `double`. The formula would
have computed 0.6. A person entered 0.5, so that is the answer — and
`ratio->source()` is `ValueSource::ManuallyEntered`, printed above, so a report
can show which numbers were derived and which were asserted.

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

### A decimal only where it is exact, a rounding only where you ask

```cpp
#include <formula-cpp/format.hpp>
```

```text
std::format("{}", Rational { 3, 5 })                           0.6
std::format("{}", Rational { 1, 3 })                           1/3
std::format("{:/}", Rational { 3, 5 })                         3/5
std::format("{:.2HalfEven}", Rational { 23653, 200 })          118.26
std::format("{:.2HalfAwayFromZero}", Rational { 23653, 200 })  118.27
std::format("{:.2HalfEven}", Rational { 4 })                   4.00
std::format("{:~.3HalfEven}", Rational { 1, 3 })               ≈0.333
std::format("{:~.3HalfEven}", Rational { 3, 5 })               0.6
```

`1/3` has no decimal, so it stays `1/3`: `0.333` would be a different number.
A rounding names its mode — there is no default — and `~` marks it `≈`.
Traces and rendered formulas take the same choice; see
[Displaying numbers](docs/display.md).

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
auto const explained = formula::explain<WaterCementRatio>(ratio, inputs);

// render_trace has no default for maxSteps: TraceRenderOptions::maxSteps
// is a StepLimit, which has no default constructor, so a caller who
// writes render_trace(explained.trace, {}) does not compile, rather than
// risking an unbounded dump of a derivation many times this size.
std::print("{}", formula::render_trace(explained.trace, { .maxSteps = 10 }));
```

```
1. V_w = 180 l
2. V_c = 300 l
3. #1 / #2 = 3/5
4. #3 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)]
```

Every value is shown in the unit it was declared in, not the coherent unit
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

### A published table that a value falls outside of gives no number at all

```cpp
inline constexpr formula::BandTable<3> SizeBands {
    formula::band(0, 1, 127, 1),   // 0 to under 127 mm
    formula::band(127, 1, 173, 1), // 127 to under 173 mm
    formula::band(173, 1, 211, 1), // 173 to under 211 mm -- 211 mm itself is NOT in it
};
```

```
1. d = 211 mm
2. lookup(#1) = argument outside the domain of the operation [in no band; the bands cover 0 to under 211 mm]
```

Not zero, not the nearest band, not the last row. A method that defined no
correction at 211 mm has defined none, and inventing one would put a number in
a test report that nothing downstream could tell apart from a number the method
actually published. A table with a **gap** in it does not even compile, and the
diagnostic names the two rows that do not meet. Three table kinds — banded,
exact and interpolating — are covered in
[the lookup-tables guide](docs/lookup-tables.md).

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
| [Dimensions and units](docs/dimensions.md) | Compile-time dimensional analysis, exact unit conversion, and base dimensions the SI does not have, such as money |
| [Quantities](docs/quantities.md) | Declaring a quantity, `Describe`, measurements that may be absent |
| [Writing formulas](docs/expressions.md) | Operators, evaluation, environments, overrides, and logarithms and exponentials, exact or rounded to declared places |
| [Citations and rendering](docs/citations.md) | `documented()`, the three dialects, generated documentation |
| [Tracing and audit trails](docs/tracing.md) | `explain()`, `render_trace()`, sinks, and the zero-cost untraced path |
| [Displaying numbers](docs/display.md) | Decimals in traces and rendered formulas, exact unless an approximation is asked for, std::format for Rational and Measured, and values the exact layer cannot hold, written as the rounding their formula declares |
| [Calculations and worksheets](docs/calculations.md) | Named values defined by expressions, a dependency graph checked at compile time, a worksheet that recalculates only what a change reaches, what-if copies, overrides, and a derivation per named value |
| [Rounding and conditionals](docs/rounding-and-conditionals.md) | Rounding as a node, `when()`, and the traced `numeric_value_of` escape hatch |
| [Constraints and verdicts](docs/constraints.md) | Validating a result with `constraint()` and `check()`, the four-state outcome, and checking a set without short-circuit |
| [Lookup tables](docs/lookup-tables.md) | The three table kinds, validation that refuses a gap, and why a miss is not a number |
| [Methods and overlays](docs/methods-and-overlays.md) | Variants selected by tag, a method's own rounding rule and constraints, jurisdiction overlays and their provenance in the trace, a jurisdiction's own acceptance logic, and jurisdiction-scoped vocabularies |
| [Series and grading curves](docs/series.md) | One quantity at each point of a method's domain, the index marker, elementwise arithmetic, absence and failure per element, conformity against a limit envelope, snapping, grading curves and splicing, and binning raw observations |
| [Statistics, outliers and precision](docs/statistics.md) | Counts, means, variances and ranges of a sample -- a series or raw observations -- the spread rounded exactly, outliers rejected pass by pass with the author's verdict on an abort, critical values from the author's table, and precision limits at the level they check |
| [Other samples and other tests](docs/records.md) | Reading from a reference sample or a prior test by role, computing over another specimen, the record each value came from in the trace, lineage as a gate, and a record not yet made |
| [Opaque operations and bounded retry](docs/opaque-and-retry.md) | A named operation such as a least-squares line through a curve, or through raw observations with R², and a regression on several regressors, traced by its inputs and outputs with its inside marked as not shown, and a step repeated until it is accepted, at most a fixed number of times, ending in exactly one of six ways -- the method's verdict when it runs out |
| [Gallery](docs/gallery.md) | A documentation page the library generated about itself |

Every example in the documentation uses generic physics with invented `Example Standard`
citations. Real standards are copyrighted, so none of their content appears in this repository.

Each guide has a matching runnable program under `examples/`.

## Status

0.2.0 is the latest release ([CHANGELOG](CHANGELOG.md)). Usable for what is listed as shipped,
and still growing. The public API may change until 1.0.

| Area | State |
|---|---|
| Exact rational arithmetic, rounding modes | shipped |
| Dimensions with rational exponents, units, exact conversion | shipped |
| Quantities, metadata, absent measurements | shipped |
| Formulas, operators, environments, evaluation | shipped |
| Citations, rendering dialects, generated documentation | shipped |
| Calculation tracing and audit trails | shipped |
| Rounding nodes (decimal places, significant digits), conditionals (`when()`) | shipped |
| Constraints, verdicts, checking a set without short-circuit | shipped |
| Lookup tables: banded, exact and interpolating | shipped |
| Methods: variants, rounding rules, constraints, jurisdiction overlays, vocabularies | shipped |
| Series and grading curves, binning | shipped |
| Statistics, precision limits, outlier rejection | shipped |
| Other samples and other tests: records, context, lineage | shipped |
| Opaque operations (least squares), bounded retry | shipped |
| Values the exact layer cannot hold, reported at a declared precision (`rounded_output`) | shipped |
| Logarithms and exponentials, rounded exactly to declared places | shipped |
| Least squares over observations, with R², and several regressors | shipped |
| Power, energy and Fahrenheit units | shipped |
| Named base dimensions such as money | shipped |
| Decimals in traces, rendered formulas and `std::format` | shipped |
| Calculations: definitions, dependency graph, incremental worksheets | shipped |

## Requirements

- C++23
- CMake 3.23 or newer
- GCC 14 or newer, if you build with GCC; older GCC is not supported

CI builds and tests every push on MSVC `cl`, `clang-cl`, Clang and GCC 14 on Linux, and
AppleClang on macOS. The other compilers' minimum versions are not settled yet; earlier ones may
work but are untested.

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

`test/consumer_globals_tests.cpp` declares 258 ordinary globals such as `result`, `value`, `x` and
`index` before including every header, and builds under cl `/W4 /WX` and g++ `-Wshadow -Werror`:
no header's local or parameter hides one of them in anything that test instantiates -- evaluation
of every node kind, `render`, `document` and the trace in every dialect, constraints, methods and
every overlay operation (the test lists them). cl reports a template's local only in a template
that is instantiated, and never a function template's parameter, so a template the test does not
reach is not covered by it.

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
