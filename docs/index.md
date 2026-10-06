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
Both spellings are supported, and can be used together in one formula;
[Quantities and measurements](quantities.md#declaring-a-quantity) says what
each costs.

## One calculation, four answers

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
`format.hpp`; the umbrella header `formula.hpp` holds the rest.

## Three things worth knowing before you read further

**A dimensional mistake is a compile error.** `var<Volume> + var<Length>` does not compile, and the
diagnostic names both quantities and points at the line that wrote the formula.

**Nothing is silently zero.** An evaluation returns a value, an *absence*, or a *manual override*.
A quantity nobody measured stays absent through every operator, and the result says so — which is
a different statement from "this is zero".

**Arithmetic is exact by default.** `Rational` does not drift. Rounding happens once, where you ask
for it, in the mode the method specifies.

## Where to start

| Guide | What it covers |
|---|---|
| [Exact numbers](numbers.md) | `Rational`, the rounding modes, why exactness is the default |
| [Dimensions and units](dimensions.md) | Compile-time dimensional analysis, exact unit conversion, and base dimensions the SI does not have, such as money |
| [Quantities and measurements](quantities.md) | Declaring a quantity, `Describe`, measurements that may be absent |
| [Expressions and evaluation](expressions.md) | Operators, environments, outcomes, overrides, and logarithms and exponentials, exact or rounded to declared places |
| [Citations and rendering](citations.md) | `documented()`, the three dialects, generated documentation |
| [Tracing and audit trails](tracing.md) | `explain()`, `render_trace()`, sinks, and the zero-cost untraced path |
| [Displaying numbers](display.md) | Decimals in traces and rendered formulas, exact unless an approximation is asked for, std::format for Rational and Measured, and values the exact layer cannot hold, written as the rounding their formula declares |
| [Calculations and worksheets](calculations.md) | Named values defined by expressions, a dependency graph checked at compile time, a worksheet that recalculates only what a change reaches, what-if copies, overrides, and a derivation per named value |
| [Rounding and conditionals](rounding-and-conditionals.md) | Rounding as a node, `when()`, and the traced `numeric_value_of` escape hatch |
| [Constraints and verdicts](constraints.md) | Validating a result with `constraint()` and `check()`, the four-state outcome, and checking a set without short-circuit |
| [Lookup tables](lookup-tables.md) | The three table kinds, validation that refuses a gap, and why a miss is not a number |
| [Methods and overlays](methods-and-overlays.md) | Variants selected by tag, a method's own rounding rule and constraints, jurisdiction overlays and their provenance in the trace, a jurisdiction's own acceptance logic, and jurisdiction-scoped vocabularies |
| [Series and grading curves](series.md) | One quantity at each point of a method's domain, the index marker, elementwise arithmetic, absence and failure per element, conformity against a limit envelope, snapping, grading curves and splicing, and binning raw observations |
| [Statistics, outliers and precision](statistics.md) | Counts, means, variances and ranges of a sample -- a series or raw observations -- the spread rounded exactly, outliers rejected pass by pass with the author's verdict on an abort, critical values from the author's table, and precision limits at the level they check |
| [Other samples and other tests](records.md) | Reading from a reference sample or a prior test by role, computing over another specimen, the record each value came from in the trace, lineage as a gate, and a record not yet made |
| [Opaque operations and bounded retry](opaque-and-retry.md) | A named operation such as a least-squares line through a curve, or through raw observations with R², and a regression on several regressors, traced by its inputs and outputs with its inside marked as not shown, and a step repeated until it is accepted, at most a fixed number of times, ending in exactly one of six ways -- the method's verdict when it runs out |
| [Gallery](gallery.md) | A documentation page the library generated about itself |

New here? Read **[Expressions and evaluation](expressions.md)** first — it is the layer the library
exists for. The [API reference](https://lastrada-software.github.io/formula-cpp/api/) documents
every public entity.

Each guide has a matching runnable program under `examples/` in the repository, and every snippet
on this site is taken from code that compiles, so nothing here can drift from what the library
actually does.

## A note on the examples

Every citation of a standard on this site is invented — generic physics with fictional
`Example Standard` references. Real test-method standards are copyrighted and sold, so none of
their content appears in this repository: not their text, their tables, their threshold values,
nor their clause numbering. Real standards are implemented in downstream libraries that hold a
licence to them. The one real reference, in `examples/cycling_speed.cpp`, cites a published
paper by its title, authors, journal and year only, and quotes none of its text.

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

Apache-2.0. Source at [github.com/LASTRADA-Software/formula-cpp](https://github.com/LASTRADA-Software/formula-cpp).
