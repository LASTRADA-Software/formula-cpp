# Displaying numbers

formula-cpp computes exactly: every value is a `Rational`, and by default every
surface that writes one -- a trace, a rendered formula, a documentation page --
writes it as a fraction, `863/1000`. That text is always exact and always
reproducible, but a reader expects `0.863`. This page shows how to get
decimals on each surface, and the one rule that governs all of them: **a
decimal is written only where it is the exact value, and a rounded one only
where you ask for it.**

The four ways a number reaches text, each covered below:

- a **trace**, through `TraceRenderOptions::numbers`;
- a **rendered formula** and its **documentation**, through `RenderOptions`;
- **`number_text()`** and **`decimal_text()`**, which need neither `<format>`
  nor an allocation;
- **`std::format`**, for a `Rational` or a `Measured<Q>`, once
  `<formula-cpp/format.hpp>` is included.

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
    (var<WetMass> - var<DryMass>) / (var<DryMass> - formula::constant<unit::Gram>(rat(51, 2)));

inline constexpr auto specimen =
    formula::environment(formula::Measured<WetMass> { rat(787, 5) }, formula::Measured<DryMass> { rat(144) });
```

`render_trace` takes the style in `TraceRenderOptions::numbers`. One trace,
four ways:

```cpp
NumberStyle const exactStyle = NumberStyle::exact_decimal();
NumberStyle const roundedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
NumberStyle const paddedStyle = NumberStyle::approximate_decimal(RoundingMode::HalfEven, DecimalPadding::Padded);
std::string const fractions = formula::render_trace(trace, { .maxSteps = 20 });
std::string const exactDecimals = formula::render_trace(trace, { .maxSteps = 20, .numbers = exactStyle });
std::string const rounded = formula::render_trace(trace, { .maxSteps = 20, .numbers = roundedStyle });
std::string const padded = formula::render_trace(trace, { .maxSteps = 20, .numbers = paddedStyle });
```

```text
-- fractions, the default --
1. m_w = 787/5 g
2. m_d = 144 g
3. #1 - #2 = 67/5000
4. m_d = 144 g
5. 51/2 g
6. #4 - #5 = 237/2000
7. #3 / #6 = 134/1185
```

```text
-- exact decimals --
1. m_w = 157.4 g
2. m_d = 144 g
3. #1 - #2 = 0.0134
4. m_d = 144 g
5. 25.5 g
6. #4 - #5 = 0.1185
7. #3 / #6 = 134/1185
```

```text
-- rounded where no decimal ends --
1. m_w = 157.4 g
2. m_d = 144 g
3. #1 - #2 = 0.0134
4. m_d = 144 g
5. 25.5 g
6. #4 - #5 = 0.1185
7. #3 / #6 = ≈0.113
```

```text
-- rounded and padded --
1. m_w = 157.4 g
2. m_d = 144.0 g
3. #1 - #2 = 0.0134
4. m_d = 144.0 g
5. 25.5 g
6. #4 - #5 = 0.1185
7. #3 / #6 = ≈0.113
```

The three styles, each a `NumberStyle`:

| Style | Writes | Line 7 above |
|---|---|---|
| `NumberStyle::fraction()` | every value as a fraction in lowest terms -- the default | `134/1185` |
| `NumberStyle::exact_decimal()` | the exact decimal where the value has one, otherwise the fraction | `134/1185` |
| `NumberStyle::approximate_decimal(mode)` | the exact decimal where there is one, otherwise the value rounded in `mode` at its unit's declared decimals, marked `≈` | `≈0.113` |

Both decimal styles take a second argument, `DecimalPadding`. `Trimmed`, the
default, writes a decimal as short as it is; `Padded` pads it with zeros to its
unit's declared decimals, so a column of grams reads `144.0 g` beside
`157.4 g` -- the gram declares one decimal. Padding never cuts a decimal short:
a value with more decimals than its unit declares keeps every one of them.

### A value in a unit nobody declared

Line 3 reads `0.0134`, with no unit. A computed value -- a difference, a
product, a ratio -- is stated in the coherent SI unit of its dimension, here
the kilogram: `0.0134` is the 13.4 g the specimen lost. Nobody declared that
unit for this formula, so its decimals are `Unit`'s default of 3, which is no
one's statement of precision. Such a value is **never padded** -- the padded
trace in the next section writes a computed 0.12 kg as `0.12`, not `0.120` --
and when it is rounded it keeps those **3 places**: line 7's ratio reads
`≈0.113`.

Three places of a kilogram can hide almost everything. The dish's mass,
averaged from three weighings in grams, is computed in kilograms:

```cpp
inline constexpr auto dishMass = formula::sum(formula::series<DishWeighing, 3>) / formula::number(rat(3));
```

```text
1. t = 4.21 g; 4.23 g; 4.26 g
2. sum(#1) = 12.7 g
3. 3
4. #2 / #3 = ≈0.004
```

Line 4 is 0.004233... kg, rounded to 3 places of a kilogram: `≈0.004`. The
`≈` says it was rounded; the result itself, read in the unit its quantity
declares, keeps what matters:

```text
the dish's mass in its declared grams: ≈4.2 g
```

### What no style rounds

**A number the author typed is written exactly, whatever the style**: a
constant (line 5's `25.5 g`), a table's bound or row, a permitted value, a
limit. Rounding it would print a number nobody wrote.

**Nor is either side of a comparison a line states beside its verdict.** Two
specimens' moisture contents, checked against a limit of at most 12 %:

```cpp
inline constexpr auto moistureLimit = formula::conformity<unit::Percent>(
    formula::series<MoistureContent, 2>, atMostTwelve, formula::Verdict { "dry the specimen again" });
```

```text
1. w = ≈11.2 %; 12.5 %
2. conform(#1) [1 satisfied, 67/6 % (at most 12 %); 2 violated, 12.5 % (at most 12 %): dry the specimen again]
```

Line 1, the input, is rounded: `≈11.2 %`. Line 2 states the same 67/6 % that
it compared, exactly. A rounded comparison could contradict its own verdict --
12.04 % against at most 12 % would read `≈12.0 % (at most 12 %)`, and be
violated -- so a compared value is never shown rounded.

## Decimals in a rendered formula and its documentation

`render()` and `document()` take the style in `RenderOptions`, beside the
vocabulary (`DefaultVocabulary {}` renames nothing):

```cpp
formula::RenderOptions const decimals { .numbers = NumberStyle::exact_decimal() };
std::string const defaultText = formula::render(moistureContent);
std::string const decimalText = formula::render(moistureContent, formula::DefaultVocabulary {}, decimals);
std::string const latexText =
    formula::render<formula::Dialect::LaTeX>(moistureContent, formula::DefaultVocabulary {}, decimals);
formula::Documentation const page = formula::document(moistureContent, formula::DefaultVocabulary {}, decimals);
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
`NumberStyle::exact_decimal()`: `1/3` stays `1/3`, and no `≈` appears in a
formula. `document()` writes its formula, a derived quantity's derivation and
a rejection's limit the same way.

**A trace pads where a formula does not, on purpose.** A trace's lines state
values in a column, where a uniform number of decimals is what padding is for.
A tare typed as a whole 24 g, under a padding style:

```text
formula, padded style: m_d - 24 g
trace, padded style:
1. m_d = 144.0 g
2. 24.0 g
3. #1 - #2 = 0.12
```

The formula states the 24 its author typed; the trace pads it to the gram's one
decimal, as it pads every value in grams. Line 3, 0.12 kg in a unit nobody
declared, is not padded to that unit's default 3 decimals.

The style reaches every node through the vocabulary, the one argument every
`render_node` already receives -- your own included. A node of yours that
writes a number its author typed should write it as the library's own nodes
do: with `number_style_of(vocabulary).exact_only()`, and without padding.

## `number_text` and `decimal_text`

`number_text()` spells a `Rational` in a unit, or a `Measured<Q>` in its
quantity's unit, in a `NumberStyle` -- the same spelling a trace line gives.
`decimal_text()` rounds to a number of places you name, in a mode you name,
and writes exactly what `checked_round` rounds to. Both live in
`number_text.hpp`, which `formula.hpp` includes, and both:

- need **no `<format>`** and no `<string>`: the result is a `NumberText`, a
  fixed 64-byte buffer, so they **allocate nothing**;
- are **`constexpr`**, so a spelling can be checked at compile time:

```cpp
static_assert(formula::number_text(Rational { 3, 5 }, NumberStyle::exact_decimal(), unit::One) == "0.6");
static_assert(formula::number_text(Rational { 1, 3 }, NumberStyle::exact_decimal(), unit::One) == "1/3");
```

A `NumberText`'s characters are read through `view()`, on a named object --
`view()` on a temporary does not compile, since the view would outlive the
buffer:

```cpp
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

A `Measured` value that is absent reads `(not measured)`, in every style.

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
`Measured`, or asks whether it can** (`std::formattable`). The header declares
explicit specialisations of `std::formatter`, and an explicit specialisation
must be seen before any use that would otherwise instantiate the primary
template; translation units that disagree about it make the program
ill-formed, with no diagnostic required. **The library owns these two
specialisations** -- `std::formatter<formula::Rational, char>` and
`std::formatter<formula::Measured<Q>, char>` -- so a consumer must not
specialise them too. Only `char` is supported: a unit's symbol is UTF-8.

### The spec

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
| a `Measured` | the number in its quantity's unit, then the unit's symbol | `std::format("{}", dryMass)` | `157.4 g` |
| an absent `Measured` | `(not measured)`, whatever the body | `std::format("{}", notMeasured)` | `(not measured)` |

The example prints every row, and checks each against the text in its
source; `w` is the specimen's moisture content, 2680/237 %, and `dryMass` a
measured 157.4 g:

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
std::format("{}", dryMass)                                     157.4 g
std::format("{}", w)                                           2680/237 %
std::format("{:~HalfEven}", w)                                 ≈11.3 %
std::format("{:.2HalfEven}", w)                                11.31 %
std::format("{}", notMeasured)                                 (not measured)
std::format("{:>8~.3HalfEven}", Rational { 1, 3 })             "  ≈0.333"
std::format("{:>8}", oven)                                     "  105 °C"
std::format("{:>8}", grain)                                    "   63 µm"
```

The whole grammar:

```
spec  ::= [[fill] align] [width] [body]         fill: one UTF-8 code point; align: < > ^ (default >)
body  ::= ''                   exact decimal, else fraction      0.6   1/3    5.2 kW
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
lines up with one that does not, on every toolchain: `"  ≈0.333"`, `"  105 °C"`
and `"   63 µm"` above are each eight characters, although `≈` is three bytes
and `°` and `µ` two. The fill is one Unicode scalar value, any but `{` and
`}`; the odd fill character of a centred value goes after it. The width is a
whole number of at most nine digits written in the spec: a width taken from an
argument, `{:{}}`, is refused, and so is `0`-padding, which a fraction cannot
take.

### What goes wrong, and how it shows

A spec the grammar does not allow is refused by a function named for the
mistake: `formula_number_format_needs_a_rounding_mode`,
`formula_number_format_places_out_of_range` or
`formula_number_format_spec_not_understood`.

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
include\formula-cpp/format.hpp(333): note: failure was caused by call of undefined function or one not declared 'constexpr'
include\formula-cpp/format.hpp(333): note: see usage of 'formula::detail::formula_number_format_needs_a_rounding_mode'
```

clang and g++ name the same function, in their own words.

**A spec built at run time is checked when it is used**, and the same function
throws `std::format_error`, whose message starts `formula: ` and says what to
write instead:

```cpp
std::string_view const noMode = "{:.2}";
try
{
    (void) std::vformat(noMode, std::make_format_args(w));
```

```text
std::vformat("{:.2}", ...) throws std::format_error:
formula: this number format rounds but names no rounding mode -- write one after the places, as in {:.2HalfEven}; there is no default, because one number rounds differently under different methods
```

One case is refused only when a value is written, because no spec check can
see it: `{:~Mode}` on a `Measured` whose unit declares negative decimals --
rounding to tens or thousands -- rounds through exact arithmetic, which
overflows for a value with a large denominator, such as
`Rational::from_double_exact(0.1)` at -3 decimals. `std::format` then throws
`std::format_error` too, starting `formula: this number cannot be spelled as
the format asks`; it never writes a text that is neither the value nor the
rounding the spec asked for.
