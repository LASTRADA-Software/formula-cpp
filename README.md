# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only, no dependencies.

Write a formula once, with ordinary operators. Get back a number, a rendering, and a
documentation page — from the same declaration.

How fast does a cyclist ride on 250 W? `examples/cycling_speed.cpp` works out
the steady-state speed from power. Each step is a formula, named for the
quantity it yields — a type carrying its own symbol, description and unit —
and the speed's formula carries its citation:

```cpp
// ---- The steps ----
inline constexpr auto gradient = formula::yields<Gradient>(var<Rise> / var<Run>);
inline constexpr auto totalMass = formula::yields<TotalMass>(var<RiderMass> + var<BikeMass>);
inline constexpr auto resistingForce =
    formula::yields<ResistingForce>(var<TotalMass> * gravity * (var<RollingCoefficient> + var<Gradient>) );
inline constexpr auto dragFactor = formula::yields<DragFactor>(formula::Rational { 1, 2 } * var<AirDensity> * var<DragArea>);
inline constexpr auto powerTerm = formula::yields<PowerTerm>(var<Power> / (2 * var<DragFactor>) );
inline constexpr auto forceTerm = formula::yields<ForceTerm>(var<ResistingForce> / (3 * var<DragFactor>) );

// The speed, read from a and b by name, so its rendering stays one line.
inline constexpr auto discriminant = formula::pow<2>(var<PowerTerm>) + formula::pow<3>(var<ForceTerm>);
inline constexpr auto speed = formula::yields<Speed>(formula::documented(
    formula::cbrt(var<PowerTerm> + formula::sqrt(discriminant))
        + formula::cbrt(var<PowerTerm> - formula::sqrt(discriminant)),
    { .title = "Validation of a mathematical model for road cycling power",
      .reference = "J. C. Martin, D. L. Milliken, J. E. Cobb, K. L. McFadden and A. R. Coggan, "
                   "Journal of Applied Biomechanics, 1998",
      .text = "A simplified form of the model, without drivetrain or bearing losses: the power a rider holds "
              "balances rolling resistance, gravity and air drag; in steady state, with no wind and a small "
              "gradient, P = v * (m * g * (C_rr + s) + 1/2 * rho * C_dA * v^2), solved here for v." }));
```

`ride(surface)` puts them into one `formula::calculation`, after a step that
looks up the rolling coefficient by the road surface. Rendered, the lookup
spells out its whole table on one line; the steps after it read:

```
s = h / L
m = m_r + m_b
F = m * 9.80665 m/s2 * (C_rr + s)
k = 0.5 * rho * C_dA
a = P / (2 * k)
b = F / (3 * k)
v = root3(a + sqrt(a^2 + b^3)) + root3(a - sqrt(a^2 + b^3))
```

The speeds it calculates at 250 W:

```
flat road, asphalt, 250 W:     10.332 m/s (37.2 km/h)
3 % climb, asphalt, 250 W:     6.783 m/s (24.4 km/h)
flat road, cobbles, 250 W:     8.335 m/s (30.0 km/h)
```

Every step up to `a` and `b` is an exact lookup, sum, product or quotient: the
gradient (`3/100` on the climb), the force, `a` and `b` are exact rationals,
never rounded. The cube roots have no exact value, so the speed is evaluated
in `double` (`formula::checked_evaluate_si<double>`) from the exact `a` and
`b`, by Cardano's formula, which solves the cubic for the speed in closed
form. On an 8 % descent at no power, Cardano's square root has no real
answer, and the program reports a `DomainError` instead of a speed. The same
calculation also says what its symbols mean and where it comes from: see
[One calculation, four answers](#one-calculation-four-answers).

A quantity can also be declared as a struct deriving from `formula::Quantity`,
`struct RiderMass: formula::Quantity<RiderMass, "m_r", "rider's mass", unit::Kilogram> {};`.
Both spellings are supported, and can be used together in one formula; [the
quantities guide](docs/quantities.md#declaring-a-quantity) says what each
costs.

## See it work

Every block below is real code from `examples/`, with the output those programs
actually print. The one mistake that must not compile is shown from
`test/negative/`, which pins it.

### One calculation, four answers

The inputs of the cycling example are quantities, as every value it
calculates is:

```cpp
// ---- The inputs ----
using RiderMass = formula::Quantity<struct RiderMassTag, "m_r", "rider's mass", unit::Kilogram>;
using BikeMass = formula::Quantity<struct BikeMassTag, "m_b", "bike's mass", unit::Kilogram>;
using DragArea = formula::Quantity<struct DragAreaTag, "C_dA", "drag area", unit::SquareMetre>;
using AirDensity = formula::Quantity<struct AirDensityTag, "rho", "density of the air", unit::KilogramPerCubicMetre>;
using Power = formula::Quantity<struct PowerTag, "P", "power the rider holds", unit::Watt>;
using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", unit::Metre>;
```

`ride(RoadSurface::Asphalt)` is the calculation for an asphalt road. Rendering
it, as plain text and as LaTeX, and documenting it, are three calls; the
documentation, `page`, holds the symbol table the program prints, and the
citation:

```cpp
auto const decimals = formula::NumberStyle::exact_decimal();
auto const onAsphalt = ride(RoadSurface::Asphalt);

// ---- 1. The calculation, one step a line ----
std::println("the calculation, in the order it calculates:\n{}\n", formula::render(onAsphalt, { .numbers = decimals }));
std::println("in LaTeX:\n{}\n",
             formula::render<formula::Dialect::LaTeX>(onAsphalt, latexSymbols, { .numbers = decimals }));

// ---- 2. Its symbol table and its citation ----
formula::Documentation const page = formula::document(onAsphalt, { .numbers = decimals });
std::println("its symbol table:");
for (formula::SymbolEntry const& entry: page.symbols)
    std::println("  {:<5} {:<6} {}", entry.symbol, entry.unit, entry.description);
```

The plain rendering is shown above. In LaTeX, the speed's line reads:

```
v = \sqrt[3]{a + \sqrt{a^{2} + b^{3}}} + \sqrt[3]{a - \sqrt{a^{2} + b^{3}}}
```

The symbol table lists the eight calculated values first, then the seven
inputs; its calculated rows read:

```
its symbol table:
  C_rr         rolling resistance coefficient
  s            road gradient
  m     kg     mass of rider and bike
  F     N      rolling resistance and gravity together
  k     kg/m   air drag per square of speed
  a     m3/s3  power over twice the drag factor
  b     m2/s2  force over three times the drag factor
  v     m/s    steady-state speed
```

and the citation, from `page.citations`:

```
the speed, after:
  Validation of a mathematical model for road cycling power
  J. C. Martin, D. L. Milliken, J. E. Cobb, K. L. McFadden and A. R. Coggan, Journal of Applied Biomechanics, 1998
```

The value comes from the example's `ride_on(surface, inputs)`. `riding(watts,
rise)` is the environment of the inputs: a rider of 75 kg on a bike of
8.5 kg, with a drag area of 0.32 m², in air of 1.225 kg/m³, holding `watts` on
a road rising `rise` over 1000 m. `ride_on` calculates every step up to `a` and
`b` exactly on a `formula::worksheet` of `ride(surface)` and those inputs,
evaluates the speed's formula in `double` from them, rounds it to the
millimetre a second and converts it to km/h. Each result is checked before it
is read:

```cpp
auto const flat = ride_on(RoadSurface::Asphalt, riding(250, 0));
if (!flat)
{
    std::println("flat road at 250 W: {}", formula::describe(flat.error()));
    return 1;
}
std::println("\nflat road, asphalt, 250 W:     {} ({:.1HalfEven})", flat->inMetresPerSecond, flat->inKilometresPerHour);
```

`riding(250, 30)` is the 3 % climb, 30 m over 1000 m.

The text comes from `render.hpp` and `document.hpp`, and the printing from
`format.hpp`; the umbrella header `formula.hpp` holds the rest (see [Copy the
headers](#copy-the-headers)).

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
static_assert(area.has_value());
```

```
circular area of a 103 mm diameter = ≈0.008332 m2 (derived)
2500 g reported as m = 2.5 kg
```

Note `constexpr`: that area was computed at compile time, so the check that
the arithmetic did not fail is a `static_assert` — `checked_evaluate` returns
the outcome or the error, and neither is read unchecked. The area is held
exactly, with `formula::pi` an exact fraction close to pi, and has no short
decimal, so it is printed rounded to six places and marked `≈`.

To serialise a unit — a JSON annotation, a database column — write its stable
ASCII key, `formula::view_ascii(someUnit)`, not its display symbol, which may
be restyled. A unit whose symbol is not ASCII declares the key as
`.asciiText` (`unit::Micrometre`, shown `µm`, is keyed `um`); one that does
not is refused where the
library takes it as a quantity's, constant's, rounding's, table's or other
formula node's unit.

### A measurement nobody took stays missing

```cpp
constexpr auto diameterUnknown = formula::environment(formula::Measured<Diameter>::absent());
constexpr auto emptyArea = formula::checked_evaluate(circularArea, diameterUnknown);
static_assert(emptyArea.has_value());
```

```
area with no diameter measured: empty
```

Not `0.0`. An absence propagates through every operator and arrives at the
result still saying "nobody measured this" — which is a different statement
from "this is zero", and the difference matters when someone signs off on it.

### A number a person typed in never masquerades as a computed one

The `0.05_r` below needs `using namespace formula::literals;` in scope, as the
example has it:

```cpp
auto const climb = formula::environment(formula::Measured<Rise> { 90 },
                                        formula::Measured<Run> { 3000 },
                                        formula::entered(formula::Measured<Gradient> { 0.05_r }));
auto const slope = formula::checked_evaluate(gradient, climb);
if (!slope)
{
    std::println("road gradient: {}", slope.error());
    return 1;
}
std::println("{} = {} ({})", formula::symbol_of<Gradient>(), *slope, slope->source());
```

```
s = 0.05 (manually entered)
```

The evaluation is checked before its outcome is read: `slope.error()` would
say, in words, what arithmetic failed. Here `gradient` is
`formula::yields<Gradient>(var<Rise> / var<Run>)`, and `0.05_r` is the exact
decimal one twentieth, never a `double`. The formula would have computed
90 m / 3000 m = 0.03. A surveyor entered 0.05, so that is the answer — and
`slope->source()` is `ValueSource::ManuallyEntered`, printed above, so a report
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
`render_trace()` turns that into text, bounded by a limit you choose.
`examples/tracing.cpp` explains a road gradient, `s = h / L`, with a citation
attached, from a rise of 90 m over a run entered as 3 km:

```cpp
auto const explained = formula::explain<Gradient>(gradient, inputs);

// render_trace has no default for maxSteps: TraceRenderOptions::maxSteps
// is a StepLimit, which has no default constructor, so a caller who
// writes render_trace(explained.trace, {}) does not compile, rather than
// risking an unbounded dump of a derivation many times this size.
std::string const rendered = formula::render_trace(explained.trace, { .maxSteps = 10 });
std::print("{}", rendered);
```

```
1. h = 90 m
2. L = 3 km
3. #1 / #2 = 3/100
4. #3 = 3/100 [Road gradient, Example Standard 1:2020, 5.4.2, (3)]
```

Every value is shown in the unit written after it: an input in the unit it was
declared in, not the coherent unit the arithmetic actually ran on — the run is
declared in kilometres, so it reads `3 km`, though the division worked on
3000 m. A computed value borrows the unit of the values it was computed from
where that is safe, and is otherwise shown in the coherent unit, spelt from the
base units (`kg/m^3`); only a dimensionless value, like the gradient above, is
a bare number. When the environment overrides the result instead of letting the
formula derive it, `explained.trace` comes back empty — nothing ran, so nothing
was recorded — and `explained.outcome.is_overridden()` says so instead: an
overridden number shows *that a person entered it*, a different fact from how
it was reached and arguably a more important one. Tracing costs nothing when
nobody asks for it: a sink is passed by value, and the untraced path —
`evaluate()`, `checked_evaluate()` — defaults to one that does nothing, adding
no instruction the evaluator would not already emit once the call inlines,
measured on all four compilers this library targets. See [the tracing
guide](docs/tracing.md).

### A published table that a value falls outside of gives no number at all

```cpp
inline constexpr formula::BandTable<3> SizeBands {
    formula::band(0, 127),   // 0 to under 127 mm
    formula::band(127, 173), // 127 to under 173 mm
    formula::band(173, 211), // 173 to under 211 mm -- 211 mm itself is NOT in it
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

Every citation of a standard in the documentation is an invented `Example Standard`. Real
standards are copyrighted, so none of their content appears in this repository. The one real
reference, in `examples/cycling_speed.cpp`, cites a published paper by its title, authors,
journal and year only, and quotes none of its text.

Each guide has a matching runnable program under `examples/`.

## Status

0.4.0 is the latest release ([CHANGELOG](CHANGELOG.md)). Usable for what is listed as shipped,
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
| Short spellings: exact decimal literals, bound formulas, `number_of`, `trace_of`, `std::format` of results | shipped |
| 128-bit exact numbers, and a unit named on every value a trace or formula writes | shipped |

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
