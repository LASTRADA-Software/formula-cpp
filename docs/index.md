# formula-cpp

Declarative, traceable, self-documenting formulas for C++23. Header-only, no dependencies.

Software that implements a test method usually keeps the formula in one place, its units in
another, where it comes from in a comment, and its audit trail in code written afterwards. Those
drift apart. In formula-cpp they are one declaration: write the formula once, with ordinary
operators, and get the number, its units, its derivation and its documentation from it.

## Example

The compressive strength of a concrete specimen: the load that crushed it over the area that
carried it.

```cpp
--8<-- "examples/readme.cpp:program"
```

```text
--8<-- "examples/readme.expected.txt"
```

## Start here

New to the library? The [tutorial](tutorial/index.md) builds a complete test step by step. The
guides below are the reference: each covers one part of the library in depth.

## Guides

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

Each guide has a matching runnable program under `examples/` in the repository, and every snippet
on this site is taken from code that compiles, so nothing here can drift from what the library
actually does. The [API reference](https://lastrada-software.github.io/formula-cpp/api/) documents
every public entity.

## A larger example

[`examples/cycling_speed.cpp`](https://github.com/LASTRADA-Software/formula-cpp/blob/master/examples/cycling_speed.cpp)
works out a cyclist's steady-state speed from the power the rider holds. Each step is a formula in
one `formula::calculation`. The exact steps are evaluated on a worksheet; the speed, whose roots
have no exact value, is evaluated in `double` from them. The speed's formula carries a citation,
and the program renders the calculation as plain text and as LaTeX and prints its symbol table.

## A note on the examples

Every citation of a standard on this site is invented — generic physics with fictional
`Example Standard` references. Real test-method standards are copyrighted and sold, so none of
their content appears in this repository: not their text, their tables, their threshold values,
nor their clause numbering. Real standards are implemented in downstream libraries that hold a
licence to them. The one real reference, in `examples/cycling_speed.cpp`, cites a published
paper by its title, authors, journal and year only, and quotes none of its text.

Apache-2.0. Source at [github.com/LASTRADA-Software/formula-cpp](https://github.com/LASTRADA-Software/formula-cpp).
