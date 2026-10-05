# Displaying numbers

formula-cpp computes exactly: every value is a `Rational`, and by default a
trace, a rendered formula and a documentation page write one as a fraction,
`863/1000`. That text is always exact and always reproducible, but a reader
expects `0.863`. This page shows how to get
decimals on each surface, and the one rule that governs all of them: **a
decimal is written only where it is the exact value, and a rounded one only
where you ask for it -- and a value the exact layer cannot hold is written
only as the rounding its formula declares.**

The four ways a number reaches text, each covered below:

- a **trace**, and a [worksheet's derivation](calculations.md#how-a-value-was-reached),
  through `TraceRenderOptions::numbers`;
- a **rendered formula** and its **documentation**, through `RenderOptions`;
- **`number_text()`** and **`decimal_text()`**, which need neither `<format>`
  nor an allocation;
- **`std::format`**, for a `Rational` or a `Measured<Q>`, once
  `<formula-cpp/format.hpp>` is included -- which also formats an
  `Outcome<Q>`, a `Unit`, a `Dimension` and an enumeration's words.

The worked example is `examples/display.cpp`: a soil specimen's moisture
content, from its wet and dried masses in a dish. **Program output** on this
page is copied verbatim from that program's output, and `docs.display-output`
fails unless each output block is a run of consecutive lines the program
prints, exactly as quoted (`cmake/CheckGuideOutput.cmake`). **Code** is copied
from the example's source, and `docs.display-snippets` fails unless each code
block appears there as a run of consecutive lines, compared without their
indentation (`cmake/CheckGuideSnippets.cmake`). A code block deliberately not
from the example carries a `<!-- snippet: not from the example -->` comment
directly above it; one on this page does, a spec that must not compile.

Every number on this page is invented.

## Why decimals are opt-in, and never approximate

`3/5` is `0.6`, exactly: a decimal that ends. `1/3` has no such decimal, and
`0.333` is not `1/3` -- it is a different number, one nobody can reproduce
from the page it is printed on. So under an exact-decimal style the library
writes `0.6` beside `1/3`, both exact, rather than `0.6` beside `0.333`, one of
them a claim the arithmetic never made.

A rounded decimal is still available, because a reader often wants one. It is
**opt-in**: you ask for it by naming a rounding mode, since the same number
rounds differently under different methods ([Exact numbers](numbers.md#rounding)),
and the library never picks one for you. Where a style rounds only the values
that need it -- a trace's, `number_text`'s, `std::format`'s `~` -- a rounded
value is **always marked**: `≈0.113`, never `0.113`, so it cannot pass for the
exact one. Only a rounding you ask for outright, to a number of places
(`decimal_text`, `std::format`'s `.N`), is written unmarked, as you asked.

Traces, rendered formulas and documentation keep fractions unless asked. Text
a program wrote before these options existed -- an archived audit trail, a
pinned test -- reads exactly as it did, unless the program asks for decimals.

## Decimals in a trace

The moisture content is the water the specimen lost over its dry mass, the
dish's 25.5 g taken off:

```cpp
inline constexpr auto moistureContent =
    (var<WetMass> - var<DryMass>) / (var<DryMass> - formula::constant<unit::Gram>(25.5_r));

inline constexpr auto specimen =
    formula::environment(formula::Measured<WetMass> { 157.4_r }, formula::Measured<DryMass> { 144 });
```

`25.5_r` is the exact number its digits spell, 51/2
(`using namespace formula::literals;`,
[Writing an exact decimal](numbers.md#writing-an-exact-decimal)).
`checked_explain` evaluates the formula and returns a `std::expected`: the
outcome together with a `Trace` of every step ([Tracing](tracing.md)), or the
arithmetic error with the steps recorded up to it. The example checks it
before reading either, as it checks every `std::expected` on this page:

```cpp
auto const moisture = formula::checked_explain<MoistureContent>(moistureContent, specimen);
if (!moisture)
{
    std::println("moisture content: {}", moisture.error().error);
    return 1;
}
```

`render_trace` takes the style in `TraceRenderOptions::numbers`. One trace,
four ways:

```cpp
auto const exactStyle = NumberStyle::exact_decimal();
auto const roundedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
auto const paddedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven, DecimalPadding::Padded);
std::string const fractions = formula::render_trace(moisture->trace, { .maxSteps = 20 });
std::string const exactDecimals = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = exactStyle });
std::string const rounded = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = roundedStyle });
std::string const padded = formula::render_trace(moisture->trace, { .maxSteps = 20, .numbers = paddedStyle });
```

```text
-- fractions, the default --
1. m_w = 787/5 g
2. m_d = 144 g
3. #1 - #2 = 67/5 g
4. m_d = 144 g
5. 51/2 g
6. #4 - #5 = 237/2 g
7. #3 / #6 = 134/1185
```

```text
-- exact decimals --
1. m_w = 157.4 g
2. m_d = 144 g
3. #1 - #2 = 13.4 g
4. m_d = 144 g
5. 25.5 g
6. #4 - #5 = 118.5 g
7. #3 / #6 = 134/1185
```

```text
-- rounded where no decimal ends --
1. m_w = 157.4 g
2. m_d = 144 g
3. #1 - #2 = 13.4 g
4. m_d = 144 g
5. 25.5 g
6. #4 - #5 = 118.5 g
7. #3 / #6 = ≈0.113
```

```text
-- rounded and padded --
1. m_w = 157.4 g
2. m_d = 144.0 g
3. #1 - #2 = 13.4 g
4. m_d = 144.0 g
5. 25.5 g
6. #4 - #5 = 118.5 g
7. #3 / #6 = ≈0.113
```

The three styles, each a `NumberStyle`:

| Style | Writes | Line 7 above |
|---|---|---|
| `NumberStyle::fraction()` | every value as a fraction in lowest terms -- the default | `134/1185` |
| `NumberStyle::exact_decimal()` | the exact decimal where the value has one, otherwise the fraction | `134/1185` |
| `NumberStyle::approximate_decimal(mode)` | the exact decimal where there is one, otherwise the value rounded in `mode` at its unit's declared decimals, marked `≈` | `≈0.113` |

A unit's **declared decimals** are the precision its definition states
([Declared precision and bounds](dimensions.md#declared-precision-and-bounds)):
the gram declares one, the percent one. Both decimal styles take a second
argument, `DecimalPadding`. `Trimmed`, the default, writes a decimal as short
as it is; `Padded` pads it with zeros to its unit's declared decimals, so a
column of grams reads `144.0 g` beside `157.4 g`. Padding never cuts a decimal
short: a value with more decimals than its unit declares keeps every one of
them, as the padded trace of the dish's weighings below shows.

A worksheet's derivation, `render_derivation`
([How a value was reached](calculations.md#how-a-value-was-reached)), takes the same
`TraceRenderOptions`. Its steps and inputs read as a trace's lines do in the
style. Each block's header states its value in the unit its quantity
declares, as a line of another block that reads the value states it --
rounded, padded or exact alike. The block's own last step agrees with the
header on whether the value is typed, and a typed value is exact on every one
of those lines. That is all they agree on: where the last step computed the
value, it states it in the unit that step is shown in, which may be the
coherent unit (below), so it may differ from the header in its unit, its
padding and its decimals, and one may read `≈` where the other does not. The
header's definition is written as a rendered formula is (see below), its typed
numbers exact.

### A value in a unit nobody declared

Most computed values read in a unit someone declared. Lines 3 and 6 above,
differences of two masses in grams, read in grams, as a value scaled by a pure
number does: a computed step borrows the unit of the steps it read where that
is safe ([Reading a derivation](tracing.md#reading-a-derivation)). A product
or a quotient of two dimensioned values borrows nothing, even of two values in
one unit: a length times a length is no length, and a length over a time
neither. It is stated in the **coherent unit** of its dimension, followed by
that unit's spelling from the base units. A bearing plate's area, from its two
edges in millimetres:

```cpp
// A bearing plate's area: a product of two lengths, which borrows neither one's unit.
inline constexpr auto plateArea = var<PlateLength> * var<PlateWidth>;
```

Its trace, rendered in the rounded and padded style:

```text
1. l_p = 100.0 mm
2. b_p = 200.0 mm
3. #1 * #2 = 0.02 m^2
```

Line 3 is in square metres, written `m^2` after it, though nobody declared that
unit for this formula. Its decimals are `Unit`'s default of 3, which is no
one's statement of precision, so such a value is **never padded**: the edges
are padded to the millimetre's one decimal, and the area reads `0.02`, not
`0.020`. Read in the unit its quantity declares, the result keeps what
matters:

```text
the plate's area in its declared square millimetres: 20000 mm2
```

When a value in the coherent unit is rounded it keeps those 3 places -- unless
they round a value other than zero to `≈0`, which says nothing of it. They are
then extended to its first significant digit, up to 18 places, and the `≈`
stays. A creep rate, an elongation in millimetres over a time in hours:

```cpp
// A creep rate: a length over a time, which borrows neither one's unit.
inline constexpr auto creepRate = var<Elongation> / var<HoldTime>;
```

```text
1. dl = 2.4 mm
2. t_h = 0.75 h
3. #1 / #2 = ≈0.0000009 m/s
```

The rate, 0.00000088... m/s, reads `≈0.0000009`, not `≈0`. A price worked out
in euros per kilowatt-hour is stated in euros per joule: 3401/33480000000 reads
`≈0.0000001`. A value in a unit someone declared keeps that unit's places,
whatever they round to:

```text
the creep rate in its declared millimetres per minute: ≈0.05 mm/min
```

The dish's mass, the mean of three weighings in grams, is computed in grams:

```cpp
// The mean of three weighings: their sum times a typed 1/3, which has no exact decimal.
inline constexpr auto dishMass = formula::sum(formula::series<DishWeighing, 3>) * formula::number(Rational { 1, 3 });
```

Its trace, rendered in the rounded and padded style:

```text
1. t = 4.21 g; 4.23 g; 4.26 g
2. sum(#1) = 12.7 g
3. 1/3
4. #2 * #3 = ≈4.2 g
```

A series' sum keeps its quantity's unit: line 2 is in grams. The product on
line 4 is the sum scaled by a pure number, so it is in grams too, rounded at the
gram's one decimal: `≈4.2`. Line 1's weighings keep their second decimal,
though the gram declares one: padding never cuts a decimal short. The `≈` says
line 4 was rounded, as the result does, read in the unit its quantity declares:

```text
the dish's mass in its declared grams: ≈4.2 g
```

### What no style rounds

**A number the author typed is written exactly, whatever the style**: a
constant (the moisture trace's line 5, `25.5 g`), a table's bound or row, a
permitted value, a limit. Rounding it would print a number nobody wrote. The
dish's typed 1/3 has no exact decimal, and even the rounding style writes it
`1/3` (line 3 above). Every number a formula declares in a dimensioned
unit with no symbol is written in the coherent unit, exact, as its trace
writes it: a constant `3/1000 kg`, a per-element constant
`values(3/1000 kg, 1/200 kg)`, a table's band `1/4 to under 1/2 kg`, a limit
`at least 3/4 kg`.

**Nor is either side of a comparison that a trace line states beside its
verdict.** Two specimens' moisture contents, checked against a limit of at
most 12 %:

```cpp
inline constexpr auto moistureLimit = formula::conformity<unit::Percent>(
    formula::series<MoistureContent, 2>, atMostTwelve, formula::Verdict { "dry the specimen again" });
```

```text
1. w = ≈11.2 %; ≈12 %
2. conform(#1) [1 satisfied, 67/6 % (at most 12 %); 2 violated, 289/24 % (at most 12 %): dry the specimen again]
```

Line 1, the input, is rounded at the percent's one declared decimal: the
second specimen's 289/24 %, 12.0416... %, rounds to 12.0 and reads `≈12 %`,
its zero trimmed, since this style does not pad. Line 2 states both
values it compared exactly, `67/6 %` and `289/24 %`. Rounded, the second would
read `≈12 % (at most 12 %)` beside the verdict *violated* -- a comparison that
contradicts itself -- so a compared value is never shown rounded.

### Values the exact layer cannot hold

A square root, a logarithm or an exponential is irrational almost everywhere,
and the exact sums behind a line fitted through 28 points, each on a different
denominator, can already leave the 128-bit integers of `Rational`
([numeric headroom](numeric-headroom.md#least-squares-realistic-and-one-stress-control)).
The library does not approximate such values. A formula that needs one
**declares the precision it is reported at** -- a unit, decimal places and a
rounding mode, as `rounded<>` does -- and the library computes the decimal the
true value rounds to: `rounded_sqrt` for a root, `rounded_ln`, `rounded_log10`
and `rounded_exp` for a
[logarithm or an exponential](expressions.md#declaring-a-precision) -- fused
nodes computed by an integer kernel, not opaque operations -- and
`rounded_output` for an output of an
[opaque operation](opaque-and-retry.md#rounded-where-it-is-used) that computes
in wider integers, as `linear_least_squares` does, over a curve or over raw
observations, and `multiple_least_squares`. Any other operation's output is
computed in `Rational`, and fails with `Overflow` where the exact output
would.

- **The rounded decimal is the step's value, and it is exact.** It is the
  correct rounding of the true value, found with integer arithmetic alone --
  no floating point -- so its digits depend on the inputs alone.
- **Its line says it was rounded, and carries no `≈`.** `≈` marks a style's
  rounding of a value a step holds; this rounding is the formula's, stated in
  the expression, its mode in the brackets:
  `round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]`
  -- 0.6786 mm/s exactly -- and the natural logarithm of a ratio of 0.05 reads
  `round(ln(#1), to 4 dp) = -2.9957 [nearest, ties to even]`.
- **The true value is written nowhere.** An opaque call evaluated for a
  rounded output names its outputs without values --
  `linear least squares(#3) = intercept, slope: rounded where used` -- and each
  output's own line states its rounding.
- **Without a declared precision, these values are refused, never
  approximated.** `sqrt(2)` is `Inexact`, and so is `ln(2)`: a logarithm
  answers only where it is rational -- ln 1, and `log10` of a power of ten --
  and is `Inexact` elsewhere, as `exp(x)` is everywhere but 0.
  `opaque_output<"slope">(fit)` is the exact slope or `Overflow`.
- **Only a rational value can tie**, and its tie is broken by the mode as
  `checked_round` breaks it.
- **A rounding that cannot be decided fails** with `Overflow` -- never a guess.
  So does a fraction that outgrows even the wide integers it is computed in.
- **Floating point never reaches a rendered trace.**
  `checked_evaluate_si<double>` answers approximately, on purpose, and its
  trace has no printable form.

## Decimals in a rendered formula and its documentation

`render()` and `document()` take the style in `RenderOptions`, after the
[vocabulary](citations.md#whose-symbols-a-jurisdictions-vocabulary) that says
how each quantity's symbol is written, or in its place: `render(x, options)`
and `document(x, options)` write every symbol as its quantity declares it, as
`DefaultVocabulary {}` does. They take it for a formula and for a
[calculation](calculations.md#defining-named-values) alike:
`render(calculation, vocabulary, options)` writes each definition's numbers in
it, and `document(calculation, vocabulary, options)` its formula and each
calculated quantity's `calculatedAs`.

For the moisture content:

```cpp
formula::RenderOptions const decimals { .numbers = NumberStyle::exact_decimal() };
std::string const defaultText = formula::render(moistureContent);
std::string const decimalText = formula::render(moistureContent, decimals);
std::string const latexText = formula::render<formula::Dialect::LaTeX>(moistureContent, decimals);
formula::Documentation const page = formula::document(moistureContent, decimals);
```

```text
default:        (m_w - m_d) / (m_d - 51/2 g)
exact decimals: (m_w - m_d) / (m_d - 25.5 g)
LaTeX:          \frac{m_w - m_d}{m_d - 25.5\,\mathrm{g}}
document():     (m_w - m_d) / (m_d - 25.5 g)
```

**Every number in a formula's text was typed by its author** -- a constant, a
table's bound or row, a permitted value, a limit -- so every one is written as
typed: exactly, and never padded. `NumberStyle::exact_decimal(DecimalPadding::Padded)`
and `NumberStyle::approximate_decimal(mode)` are accepted and act here as
`NumberStyle::exact_decimal()`: no `≈` appears in a formula. `document()`
writes every number on its page the same way: its formula, a derived
quantity's derivation (`derivedAs`, [Methods and overlays](methods-and-overlays.md))
and a rejection's limit. The dish's mass states a typed 1/3, and a rejection of
its weighings a typed limit of 1/30 of the pass's mean
([Statistics](statistics.md)):

```cpp
inline constexpr auto dishMean = formula::sample_mean(
    formula::without_outliers<formula::PerPass::MostExtreme,
                              formula::OnLimit::Keep,
                              formula::AtMost<1>,
                              formula::KeepAtLeast<2>>(
        formula::series<DishWeighing, 3>,
        formula::deviation_from_mean(Rational { 1, 30 } * formula::pass_mean<DishWeighing>),
        formula::Verdict { "weigh the dish again" }));
```

Under the rounding style, both stay fractions:

```cpp
formula::RenderOptions const rounding { .numbers = roundedStyle };
std::string const dishFormula = formula::render(dishMass, rounding);
formula::Documentation const dishPage = formula::document(dishMean, rounding);
```

```text
formula, rounded style:           sum(t(i)) * 1/3
rejection's limit, rounded style: 1/30 * pass mean
```

**A trace pads where a formula does not, on purpose.** A trace's lines state
values in a column, where a uniform number of decimals is what padding is for.
A tare typed as a whole 24 g, under a padding style:

```text
formula, padded style: m_d - 24 g
trace, padded style:
1. m_d = 144.0 g
2. 24.0 g
3. #1 - #2 = 120.0 g
```

The formula states the 24 its author typed; the trace pads it to the gram's one
decimal, as it pads every value in grams -- line 3 too, a difference of two
values in grams, and so in grams itself.

The style reaches every node through the vocabulary, the one argument every
`render_node` already receives -- your own included
([a consumer's own node](citations.md#whose-symbols-a-jurisdictions-vocabulary)).
`number_style_of(vocabulary)` reads the style it carries. A node of yours that
writes a number its author typed should write it as every node of the
library writes a formula's numbers, in `typed_number_style(vocabulary)`: that
style, exact only and never padded -- `NumberStyle::fraction()` stays itself,
and every decimal style becomes `NumberStyle::exact_decimal()`.

## `number_text` and `decimal_text`

`number_text()` spells a `Rational` in a unit, or a `Measured<Q>` in its
quantity's unit, in a `NumberStyle` -- the same spelling a trace line gives.
`decimal_text()` rounds to a number of places you name, in a mode you name,
and writes exactly what [`checked_round`](numbers.md#rounding) rounds to. Both live in
`number_text.hpp`, which `formula.hpp` includes, and both:

- need **no `<format>`** and no `<string>`: the result is a `NumberText`, a
  fixed 128-byte buffer, so they **allocate nothing**;
- are **`constexpr`**, so a spelling can be checked at compile time:

```cpp
static_assert(formula::number_text(Rational { 3, 5 }, NumberStyle::exact_decimal(), unit::One) == "0.6");
static_assert(formula::number_text(Rational { 1, 3 }, NumberStyle::exact_decimal(), unit::One) == "1/3");
```

The example spells the specimen's moisture content, `w`, and a moisture
content nobody measured:

```cpp
formula::Measured<MoistureContent> const w = moisture->outcome.measurement();
formula::Measured<MoistureContent> const notMeasured {};
formula::NumberText const measuredText = formula::number_text(w, roundedStyle);
formula::NumberText const absentText = formula::number_text(notMeasured, roundedStyle);
formula::NumberText const twoPlaces =
    formula::decimal_text(w.value(), formula::DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded);
```

```text
measured:     ≈11.3 %
not measured: (not measured)
two places:   11.31
```

A `Measured` value that is absent -- here one constructed with nothing, `{}`
-- reads `(not measured)`, in every style.

A `Measured` value in a dimensioned unit with no symbol is written as a
trace writes it: moved exactly into the coherent unit and followed by that
unit's spelling, since a unit with no symbol cannot say what scale its
number is on. 3 of a unit of 1/1000 kg reads `3/1000 kg` as a fraction and
`0.003 kg` as a decimal, never a bare `3`. Its places are read as a trace
reads them: the coherent unit's 3 are a default nobody chose, so they are
never padded to, and never round a value that is not zero to `≈0` -- 1/3 of
that unit reads `≈0.0003 kg`. `std::format` writes it the same way. A value
in a dimensionless unit with no symbol is a bare number.

A `NumberText`'s characters are read through `view()`, a `std::string_view`,
on a named object -- `view()` on a temporary does not compile, since the view
would outlive the buffer. The example prints each one so:

```cpp
std::println("measured:     {}", measuredText.view());
std::println("not measured: {}", absentText.view());
std::println("two places:   {}\n", twoPlaces.view());
```

**When a number cannot be spelled.** `decimal_text` throws
`ArithmeticException` for more than 18 places, and where rounding to whole
tens or thousands overflows. `number_text` throws it where its rounding
overflows so, for a padded or approximating style in a unit whose
declared decimals lie outside -18 to 18, where a value in a unit with no
symbol cannot move into the coherent unit, and where that unit's spelling
does not fit the buffer. Each has a `checked_` form,
`checked_decimal_text` and `checked_number_text`, that returns the
`ArithmeticError` in a `std::expected` instead of throwing. A trace never
throws for a number: a line whose value its style cannot spell reads
`(not shown: ...)`, the reason in place of the dots, and a bound or a limit
the author typed falls back to its exact fraction. A formula's text writes
every number exact and unpadded, which cannot fail.

**Prefer them** in code that must not pull in `<format>` -- a header of your
own that consumers include everywhere -- in a spelling checked at compile
time, and where you already hold the `NumberStyle` a trace was rendered in,
so that a report and its trace spell each value alike. Reach for
`std::format` when the number is one part of a longer text.

## Formatting with `std::format`

### Opting in

```cpp
#include <formula-cpp/format.hpp>
```

`formula.hpp` does not include it: it includes `<format>`, which a consumer
who only evaluates numbers should not compile in every translation unit.
**Include it in every translation unit that formats a `Rational` or a
`Measured`, or asks whether it can** (`std::formattable`) -- and the same holds
for the outcomes, units, dimensions and enumerations it also formats
([below](#formatting-outcomes-units-dimensions-and-enumerations)). The header
declares specialisations of `std::formatter`, and a specialisation must be
seen before any use that would otherwise instantiate the primary template;
translation units that disagree about it make the program ill-formed, with no
diagnostic required. **The library owns these specialisations** --
`std::formatter<formula::Rational, char>` and
`std::formatter<formula::Measured<Q>, char>` among them -- so a consumer must
not specialise them too. Only `char` is supported: a unit's symbol is UTF-8.

### The spec

In the table and the reference below, `w` is the specimen's moisture content,
2680/237 %, and `notMeasured` one nobody measured, both from the previous
section; `wetMass`, `oven` and `grain` are measured here:

```cpp
formula::Measured<WetMass> const wetMass { 157.4_r };
formula::Measured<OvenTemperature> const oven { 58.3_r };
formula::Measured<GrainSize> const grain { 217 };
```

| Spec | Meaning | Example | Output |
|---|---|---|---|
| `{}` | the exact decimal, otherwise the fraction | `std::format("{}", Rational { 3, 5 })` | `0.6` |
| `{}` | a value with no exact decimal stays its fraction | `std::format("{}", Rational { 1, 3 })` | `1/3` |
| `{:/}` | always the fraction | `std::format("{:/}", Rational { 3, 5 })` | `3/5` |
| `{:.2HalfEven}` | rounded to 2 places in the mode named, padded, not marked | `std::format("{:.2HalfEven}", Rational { 23653, 200 })` | `118.26` |
| `{:~.3HalfEven}` | the exact decimal where there is one, otherwise rounded to 3 places and marked | `std::format("{:~.3HalfEven}", Rational { 1, 3 })` | `≈0.333` |
| `{:~HalfEven}` | for a `Measured` only: the same, at its unit's declared decimals | `std::format("{:~HalfEven}", w)` | `≈11.3 %` |
| `{:>8}` | right-aligned in 8 code points; `<` left, `^` centred | `std::format("{:>8}", Rational { 3, 5 })` | `"     0.6"` |
| `{:*^7}` | centred, filled with `*` | `std::format("{:*^7}", Rational { 3, 5 })` | `**0.6**` |
| a `Measured` | the number in its quantity's unit, then the unit's symbol | `std::format("{}", wetMass)` | `157.4 g` |
| an absent `Measured` | `(not measured)`, whatever the body | `std::format("{}", notMeasured)` | `(not measured)` |

The example prints every row, and checks each against the text in its
source:

```text
std::format("{}", Rational { 3, 5 })                           0.6
std::format("{}", Rational { 1, 3 })                           1/3
std::format("{:/}", Rational { 3, 5 })                         3/5
std::format("{:.2HalfEven}", Rational { 23653, 200 })          118.26
std::format("{:.2HalfAwayFromZero}", Rational { 23653, 200 })  118.27
std::format("{:.2HalfEven}", Rational { 4 })                   4.00
std::format("{:~.3HalfEven}", Rational { 1, 3 })               ≈0.333
std::format("{:~.3HalfEven}", Rational { 3, 5 })               0.6
std::format("{:>8}", Rational { 3, 5 })                        "     0.6"
std::format("{:*^7}", Rational { 3, 5 })                       **0.6**
std::format("{}", wetMass)                                     157.4 g
std::format("{}", w)                                           2680/237 %
std::format("{:~HalfEven}", w)                                 ≈11.3 %
std::format("{:.2HalfEven}", w)                                11.31 %
std::format("{}", notMeasured)                                 (not measured)
std::format("{:>8~.3HalfEven}", Rational { 1, 3 })             "  ≈0.333"
std::format("{:>8}", oven)                                     " 58.3 °C"
std::format("{:>8}", grain)                                    "  217 µm"
```

The whole grammar:

```
spec  ::= [[fill] align] [width] [body]         fill: one Unicode scalar value; align: < > ^ (default >)
body  ::= ''                   exact decimal, else fraction      0.6   1/3    157.4 g
        | '/'                  fraction                           3/5   1/3
        | '.' N Mode           rounded to N (0..18), padded       {:.2HalfEven} -> 118.26
        | '~' ['.' N] Mode     exact where exact, else ≈ rounded  {:~.3HalfEven} -> ≈0.333
Mode  ::= HalfAwayFromZero | HalfTowardZero | HalfEven | Ceiling | Floor | TowardZero | AwayFromZero
```

A `Rational` has no unit to take places from, so `~` on a `Rational` needs
`.N`: `{:~.3HalfEven}`, never `{:~HalfEven}`.

### Rounding modes, and why none is assumed

The mode is one of `RoundingMode`'s seven enumerators, spelled exactly as it
is there: `HalfAwayFromZero`, `HalfTowardZero`, `HalfEven`, `Ceiling`,
`Floor`, `TowardZero`, `AwayFromZero`. **There is no default.** The same
number rounds differently under different methods -- 118.265 is `118.27` under
`HalfAwayFromZero` and `118.26` under `HalfEven`, as the reference shows, and
2.5 is 3 or 2 ([Exact numbers](numbers.md#rounding)) -- and which method
applies is the method's author's decision, not a format's. So `{:.2}` does not
compile.

### Exact, fraction, or `≈`

- `{}` writes the exact decimal where the value has one, and the fraction
  otherwise. It never rounds.
- `{:/}` always writes the fraction.
- `{:.N Mode}` asks for a rounding outright: it rounds to N places, pads to
  them, and writes no marker, as `decimal_text` does.
- `{:~.N Mode}` and `{:~Mode}` write the exact decimal where there is one,
  unpadded; only a value with none is rounded, and it is marked `≈`.

`≈` never appears unless the spec asks for a rounding with `~`.

### Fill, alignment and width count code points

The width counts code points, not bytes, so a value holding `≈`, `°C` or `µm`
lines up with one that does not, on every toolchain: `"  ≈0.333"`, `" 58.3 °C"`
and `"  217 µm"` above are each eight characters, although `≈` is three bytes
and `°` and `µ` two. The fill is one Unicode scalar value, any but `{` and
`}`; the odd fill character of a centred value goes after it. The width is a
whole number of at most nine digits written in the spec: a width taken from an
argument, `{:{}}`, is refused, and so is `0`-padding, which a fraction cannot
take.

### What goes wrong, and how it shows

A spec the grammar does not allow is refused by a function named for the
mistake:

| Refused by | Which specs | Write instead |
|---|---|---|
| `formula_number_format_needs_a_rounding_mode` | a rounding that names no mode: `{:.2}`, `{:~.3}`, `{:~}` | a mode after the places, or after `~`: `{:.2HalfEven}`, `{:~.3HalfEven}`, `{:~HalfEven}` |
| `formula_number_format_places_out_of_range` | more than 18 places, `{:.19HalfEven}`; and `{:~HalfEven}` on a `Measured` whose unit declares more than 18 decimals, or fewer than -18 -- a spec that names no places at all, so the unit's are out of range | `.0` to `.18`: `{:~.3HalfEven}` rounds at 3 places, whatever the unit declares |
| `formula_number_format_spec_not_understood` | anything else the grammar does not allow: a mode misspelled or mis-cased (`{:.2Half}`, `{:.2halfeven}`), a type such as `{:x}`, `0`-padding (`{:08}`), a width from an argument (`{:{}}`) or of more than nine digits, a fill that is not one Unicode scalar value or is `{`, and `{:~HalfEven}` on a `Rational` | a spec the grammar above allows; for a `Rational`, `~` with places: `{:~.3HalfEven}` |

**In a literal format string it is a compile error**, because `std::format`
checks a literal spec while compiling and the refusing function is not
`constexpr`. The repository's own negative case holds this line:

<!-- snippet: not from the example -->
```cpp
std::string const written = std::format("{:.2}", formula::Rational { 1, 3 });
```

MSVC's `cl.exe` (19.51, the `cl-debug` preset) reports it -- the first three
lines, verbatim but for the paths, which are shown relative to the repository,
before the call stack of the evaluation:

```
test\negative\format_places_without_mode.cpp(17): error C7595: 'std::basic_format_string<char,formula::Rational>::basic_format_string': call to immediate function is not a constant expression
include\formula-cpp/format.hpp(347): note: failure was caused by call of undefined function or one not declared 'constexpr'
include\formula-cpp/format.hpp(347): note: see usage of 'formula::detail::formula_number_format_needs_a_rounding_mode'
```

clang and g++ name the same function, in their own words.

**A spec built at run time is checked when it is used**, and the same function
throws `std::format_error`, whose message starts `formula: ` and says what to
write instead (`check` is the example's own assertion, which counts a failure):

```cpp
std::string_view const noMode = "{:.2}";
try
{
    (void) std::vformat(noMode, std::make_format_args(w));
    check(false, "a rounding with no mode is refused");
}
catch (std::format_error const& refusal)
{
    std::println("\nstd::vformat(\"{{:.2}}\", ...) throws std::format_error:\n{}\n", refusal.what());
    check(std::string_view { refusal.what() }.starts_with("formula: "), "the refusal starts formula: ");
}
```

```text
std::vformat("{:.2}", ...) throws std::format_error:
formula: this number format rounds but names no rounding mode -- write one after the places, as in {:.2HalfEven}; there is no default, because one number rounds differently under different methods
```

One case is refused only when a value is written, because no spec check can
see it: `{:~Mode}` on a `Measured` whose unit declares negative decimals --
rounding to tens or thousands. A value with an exact decimal of at most 18
places is written as it is and never rounded: 1/10^18 at -3 decimals is
`0.000000000000000001`. Any other value is rounded through exact arithmetic,
which overflows for one with a large denominator, such as 2^-120,
`Rational { 1, Rational::Int { 1 } << 120 }`, at -3 decimals. `std::format`
then throws `std::format_error` too, starting `formula: this number cannot be
spelled as the format asks`; it never writes a text that is neither the
value nor the rounding the spec asked for. No spec rounds such a value to
tens or thousands. Write `{:~.0HalfEven}` instead to round it to whole units
-- a rounding to 0 to 18 places is spelled by long division, which cannot
overflow, so 2^-120 reads `≈0` -- or `{:/}` for its exact fraction, or catch
the `std::format_error`.

## Formatting outcomes, units, dimensions and enumerations

The same header formats four more kinds of value, so that a report prints the
library's own values rather than taking them apart: an `Outcome<Q>`, a `Unit`,
a `Dimension`, and every enumeration of the library that has a `describe()`.
**Include `<formula-cpp/format.hpp>` in every translation unit that formats
one of them, or asks whether it can**, for the reason
[Opting in](#opting-in) gives. That holds for an enumeration too:
`std::formattable<formula::RoundingMode, char>` is true only where the header
is included. The library owns these specialisations as well: a consumer's own
`std::formatter` for `Outcome<Q>`, `Unit`, `Dimension` or one of these
enumerations defines it twice, and a generic one for every enumeration is
ambiguous for them.

`moisture->outcome` is the moisture content's outcome from the first section,
read after its check. The example adds a verdict as a rejection yields it,
built directly here with `Outcome<DishMass>::verdict`, and an empty outcome:

```cpp
// A verdict as a rejection of the dish's weighings yields it when it cannot
// settle, built directly here, and a dish nobody weighed.
auto const reweigh = formula::Outcome<DishMass>::verdict({ "weigh the dish again" });
auto const unweighed = formula::Outcome<DishMass>::empty();
```

An output is quoted, in the table and in the program's output below, where a
leading or trailing space would be missed.

| Value | Written as | Example | Output |
|---|---|---|---|
| an `Outcome` holding a value | its `Measured`, in the same spec | `std::format("{:~HalfEven}", moisture->outcome)` | `≈11.3 %` |
| an empty `Outcome` | `(not measured)`, whatever the body | `std::format("{}", unweighed)` | `(not measured)` |
| a verdict or an invalid `Outcome` | its label, right-aligned by default | `std::format("{:22}", reweigh)` | `"  weigh the dish again"` |
| a `Unit` | its symbol, left-aligned by default | `std::format("{:4}", unit::Gram)` | `"g   "` |
| a `Dimension` | its exponents, `(dimensionless)` for a pure number | `std::format("{}", unit::Gram.dimension)` | `M^1` |
| an enumeration | its `describe()` words | `std::format("{}", RoundingMode::HalfEven)` | `nearest, ties to even` |

The example prints every row, and checks each against the text in its
source:

```text
std::format("{}", moisture->outcome)                           2680/237 %
std::format("{:~HalfEven}", moisture->outcome)                 ≈11.3 %
std::format("{}", unweighed)                                   (not measured)
std::format("{:22}", reweigh)                                  "  weigh the dish again"
std::format("{:4}", unit::Gram)                                "g   "
std::format("{}", unit::Gram.dimension)                        M^1
std::format("{}", unit::Percent.dimension)                     (dimensionless)
std::format("{}", moisture->outcome.source())                  derived
std::format("{}", RoundingMode::HalfEven)                      nearest, ties to even
```

- **An `Outcome<Q>`** takes the spec of a `Measured<Q>`. A value is written as
  its `Measured` is, rounding included, and an empty outcome reads
  `(not measured)`. A verdict writes its label, and an invalid outcome the
  reason's; a rounding in the spec does not apply to words, but the fill, the
  alignment and the width do, and the label is **right-aligned by default**,
  as a number is.
- **A `Unit`** writes its symbol, **a `Dimension`** its exponents as
  [Dimensions and units](dimensions.md) prints them, and **an enumeration** its
  `describe()` words -- `ValueSource`, `OutcomeKind`, `RoundingMode`,
  `ArithmeticError`, `BoundsCheck` and the others that have one. These take a
  string's spec, and are **left-aligned by default**, as a string is.
