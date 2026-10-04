# Units on every computed trace value — design

**Status:** draft, ready for review · **Date:** 2026-10-03 · **Owner:** Christian Parpart

Resolves issue #2 ("Trace: arithmetic on single values prints bare coherent-SI numbers with no unit"). It lands
in one pull request with the `Int128` change (`2026-10-03-int128-rational-design.md`). The two designs share no
code.

## 1. Problem

A trace step holds its value in the coherent SI unit (`Step::value`, `trace.hpp:982-988`), and shows it in
`Step::unit`. Every step whose kind has no declared unit is given `coherent(N::dimension)` (`trace.hpp:3124`). That
unit has no symbol, so the renderer prints the number with nothing after it (`value_in_declared_unit`,
`trace_render.hpp:1495-1520`, appends a symbol only when there is one).

Take the outlier-rejection example in `docs/statistics.md`:

```text
2. 3/50
3. pass mean = 413/10 g
4. #2 * #3 = 1239/500000
...
6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)
```

Line 4 is 2.478 g, printed in kilograms with no unit. Two lines later the same value is `1239/500 g`. The number
is right, but a reader cannot tell what it is or check it by eye.

The same happens to every step kind that computes a value:

- the four binary operators;
- negation, `abs`, powers and roots;
- conditionals and precision limits, which re-state a value another step computed;
- sample variance;
- sums, ranges and means, whose borrowing gives up on an offset unit or a unit without a symbol.

Series steps already show a readable unit: they borrow their operand step's unit through
`detail::operand_unit_or`, under the safety rule `detail::borrowable` (`trace.hpp:2118-2156`). Single values never
got the same treatment.

## 2. The rule

> **No computed value prints without a unit.** A step shows its value in a unit read off its operand steps when
> that is unambiguous and safe. Otherwise it shows the coherent unit and writes that unit's symbol.

Two principles carry over from the series code unchanged:

- **The decision is read off the operand steps, never off C++ types.** What an operand step shows is what carries
  over, so a step never claims a unit its operands did not show.
- **`detail::borrowable` is the safety rule.** It never borrows a unit with an offset (°C, °F) or a unit without a
  symbol. A product, sum or difference of offset readings is not a point on that scale, and would be off by the
  offset. A step whose value *is* one operand's value (a pass-through) may show any unit with a symbol, offset
  included, under `detail::borrowable_for_a_point`, as a mean already does.

## 3. Recording — `RecordingSink::produced` (`trace.hpp`)

Each rule applies only when the claimed operand steps are provably the node's own:

- each side records a step of its own (`detail::RecordsOwnStep`);
- the run-time operand count is what the rule expects;
- the borrowed unit's dimension equals the step's.

Otherwise the coherent unit stands, and §4 gives it a symbol. This is the guard set the series block already uses
(`trace.hpp:3856-3872`).

| Step | Shows | Condition |
|---|---|---|
| `x * k`, `k * x`, `x / k` (scaling by a pure number) | `x`'s step unit | `k`'s dimension is dimensionless and `x`'s is not; `x`'s unit is `borrowable`. Both sides dimensionless: no unit is chosen, as for series |
| `x + y`, `x - y` | the shared unit | both operand steps show the same scale and symbol: dimension, magnitude, offset and symbol equal, and `borrowable`. The shown decimals are the larger of the two |
| `-x`, `abs(x)` | `x`'s step unit | `x`'s unit is `borrowable` |
| a conditional | the chosen branch's step unit | the branch step was claimed and its value is the conditional's value; `borrowable_for_a_point` |
| a precision limit | the re-stated step's unit | the step it re-states was claimed and holds its value; `borrowable_for_a_point` |
| everything else (products and quotients of dimensioned values, powers, roots, variance, mixed units, offset sums and differences) | the coherent unit | unchanged; §4 writes its symbol |

Notes:

- **Scaling** extends `detail::scaled_operand` (`trace.hpp:2225-2241`) from `ElementwiseBinaryNode` to `BinaryNode`.
  `BinarySides<BinaryNode>` already exists (`trace.hpp:2169-2174`).
- **"Same scale and symbol" is a new predicate**, not `Unit::operator==`. That operator also compares decimals and
  bounds (`unit.hpp:86-87`), and two gram readings declared with different decimals are still both grams. The
  borrowed unit is the left operand's, with its decimals raised to the larger of the two, so the sum is never shown
  less precisely than an operand.
- **°C − °C is not shown in °C.** `borrowable` refuses offset units, so the difference of two Celsius readings shows
  the coherent unit, `K`, which is what a temperature interval is.
- **Steps that already copy their operand's unit pick up the new units automatically:** `Documented`,
  `ReplacedVariant`, `VariantSelected`, `RecordScope`, and opaque outputs through `opaque_output_unit`.

## 4. Rendering — the coherent symbol (`trace_render.hpp`)

When a step's unit has no symbol and its dimension is not dimensionless, `value_in_declared_unit` writes the
number, then `" "`, then `detail::coherent_unit_text(dimension)` (`trace_render.hpp:2473-2526`). That helper already
exists, is already pinned (`test/opaque_tests.cpp:1044-1074`), and writes base units in a fixed order with carets:
`kg`, `kg^2`, `m/s`, `m^2`, `kg/(m s^2)`, `K`, `EUR`, `EUR/JPY`, `m^(1/2)`. A dimensionless value stays a bare number.

- **The symbol is never stored in `Step::unit`.** `Symbol` holds 16 characters, terminator included, and
  `EUR s^2/(m^2 kg)` alone is 16. `detail::is_unlabelled` must also stay true for a coherent unit, so number styling
  does not change: such a value is still never padded, and its approximation still extends to the first significant
  digit.
- **`opaque_value_text`** (`trace_render.hpp:2528-2543`) drops its own append, which this generalises, so the unit
  is not written twice.
- **Series elements and statistic lines** go through the same function, so they are labelled too.
- **Worksheet headers** (`block_value_text`) already use a declared unit, so they are unchanged.

## 5. What changes for a reader

| Today | After |
|---|---|
| `4. #2 * #3 = 1239/500000` | `4. #2 * #3 = 1239/500 g` |
| `#1 - #2 = 108000000` (kWh − kWh) | `#1 - #2 = 30 kWh` |
| `fridge_kw * fridge_h = 17280000` | `fridge_kw * fridge_h = 17280000 m^2 kg/s^2` |
| `sample_variance(#1) = 427/125000000` | `sample_variance(#1) = 427/125000000 kg^2` |
| `if #1 > #2 then #3 = 60000000` (MPa branch) | `if #1 > #2 then #3 = 60 MPa` |
| `#1 - #2 = 5` (°C − °C) | `#1 - #2 = 5 K` |
| `#3 / #6 = 134/1185` (dimensionless) | unchanged |

A value shown in a borrowed unit is styled the way that unit's declared values are: padded to its decimals under a
padded style, and rounded to them under an approximating one. The display guide's `≈0.004` (a bare product in kg)
becomes a gram value at the gram's declared decimal.

## 6. Tests

**One pinned case per acceptance criterion of issue #2:**

1. The rejection example prints `#2 * #3 = 1239/500 g`.
2. A sum of two gram values prints in grams. A difference of two °C readings prints in `K`, not °C.
3. A product of two lengths prints in `m^2`, never as a bare number.
4. No step prints a number in a unit different from the one it is shown with. A test helper walks every step of
   several traces: the rejection example, the electricity bill, a statistics trace, a conditional, a precision limit,
   and an opaque call. For each step it:
    - renders the step in the exact fraction style;
    - splits off the value and the unit text;
    - requires the unit text to be the step's own symbol, or `coherent_unit_text` when the step has none;
    - requires `checked_convert(value, shown unit, coherent unit)` to equal `Step::value`.

    The helper parses `a/b` itself: the only text-to-`Rational` parser, `rational_from_spelling`, is `consteval`.

**Rule tests, each fixture shaped so a mutation of its guard fails:**

- scaling on the left and on the right;
- `x / k`, and `k / x` (not scaling);
- both sides dimensionless;
- g + g with different declared decimals;
- g + kg (mixed: coherent);
- °C − °C;
- negation and `abs` of a gram value and of a Celsius reading;
- a conditional over an MPa branch and over a Celsius branch;
- a precision limit;
- a forwarding consumer node, whose operands are not its own sides: coherent.

**Re-pinned expectations.** About 290 lines in tests and guides show bare numbers today. Every dimensioned one is
updated to the new text; dimensionless ones are unchanged. The largest sets:

| File | Lines |
|---|---|
| `test/calculation_trace_tests.cpp` | 38 |
| `test/trace_render_tests.cpp` | 32 |
| `test/vocabulary_tests.cpp` | 27 |
| `test/retry_tests.cpp` | 21 |
| `test/rejection_tests.cpp` | 21 |
| `test/precision_tests.cpp` | 13 |

`examples/display.cpp:208` pins a bare tare difference, which now borrows grams.

## 7. Documentation

**Regenerated or checked against the examples:**

- `docs/gallery.md` is regenerated by its generator, and `gallery.is-current` holds it.
- Every ```` ```text ```` block in a checked guide follows its example's output (`docs.*-output`).
- Guides whose blocks no test checks are updated by hand: tracing, rounding-and-conditionals, lookup-tables,
  expressions, constraints, calculations, records and methods-and-overlays.

**Prose that states the old rule is rewritten:**

- `docs/display.md` "A value in a unit nobody declared" (`:161-218`, `:359-373`). Its dish and tare examples now
  borrow grams, so the section moves to an example that is still coherent, such as a product of lengths, and keeps
  teaching the unpadded, first-significant-digit styling.
- `docs/tracing.md:296-345`, `docs/dimensions.md:447-455`, `docs/statistics.md:110-112`, `docs/series.md:106-113`,
  `docs/calculations.md:597-599`, `docs/expressions.md:608-612`, `docs/lookup-tables.md:748-752`,
  `docs/rounding-and-conditionals.md:259-264`, `docs/methods-and-overlays.md:86-90`.
- The `Step::unit` doc comment (`trace.hpp:970-997`, quoted in `docs/tracing.md:308-325`), the recording comment
  (`trace.hpp:3093-3117`), and `trace_render.hpp:25-39` and `:104-123`.
- Test comments that state the old rule, e.g. `trace_render_tests.cpp:101-104`.
- CHANGELOG `[Unreleased]`, *Changed*: computed trace values show a unit, borrowed where safe and coherent otherwise.

## 8. Out of scope

- **Named derived units** (`Pa`, `N`, `J`) for coherent values. `coherent_unit_text` writes base units, and choosing
  a named unit is a separate decision.
- **Borrowing through products or powers** (g × g → g², mm × mm → mm²). A composed symbol needs a unit algebra over
  symbols that the library does not have; the coherent symbol is correct and checkable.
- **Any change to `Step::value`**, which stays the coherent SI value.
