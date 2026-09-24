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

One declaration, four answers:

```cpp
formula::render(ratio);                          // "V_w / V_c"
formula::render<formula::Dialect::LaTeX>(ratio); // "\frac{V_w}{V_c}"
formula::document(ratio);                        // text, citation and symbol table together

auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                              formula::Measured<CementVolume> { formula::Rational { 300 } });
formula::evaluate<WaterCementRatio>(ratio, environment);   // 0.6, and it knows it computed it
```

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
| [Dimensions and units](dimensions.md) | Compile-time dimensional analysis, exact unit conversion |
| [Quantities and measurements](quantities.md) | Declaring a quantity, `Describe`, measurements that may be absent |
| [Expressions and evaluation](expressions.md) | Operators, environments, outcomes, overrides |
| [Citations and rendering](citations.md) | `documented()`, the three dialects, generated documentation |
| [Tracing and audit trails](tracing.md) | `explain()`, `render_trace()`, sinks, and the zero-cost untraced path |
| [Gallery](gallery.md) | A documentation page the library generated about itself |

New here? Read **[Expressions and evaluation](expressions.md)** first — it is the layer the library
exists for. The [API reference](https://lastrada-software.github.io/formula-cpp/api/) documents
every public entity.

Each guide has a matching runnable program under `examples/` in the repository, and every snippet
on this site is taken from code that compiles, so nothing here can drift from what the library
actually does.

## A note on the examples

Every formula and citation on this site is invented — generic physics with fictional
`Example Standard` references. Real test-method standards are copyrighted and sold, so none of
their content appears in this repository: not their text, their tables, their threshold values,
nor their clause numbering. Real standards are implemented in downstream libraries that hold a
licence to them.

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

Apache-2.0. Source at [github.com/LASTRADA-Software/formula-cpp](https://github.com/LASTRADA-Software/formula-cpp).
