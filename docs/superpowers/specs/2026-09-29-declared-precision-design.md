# Declared precision: values the exact layer cannot hold — design

**Status:** draft, ready for review · **Date:** 2026-09-29 · **Owner:** Christian Parpart

The foundation for issues #3 (logarithms and exponentials), #4 (least squares over observations, with R²) and
#5 (multiple linear regression). Those two features are specified separately
(`2026-09-29-logarithms-and-exponentials-design.md`, `2026-09-29-regression-over-observations-design.md`); this
document specifies only what they share.

## 1. Problem

The library writes every number exactly. `checked_evaluate`, `explain`, `render_trace`, `render_derivation`,
`Outcome` and worksheets work in `Rational` only, and each refuses `double` at compile time. Two kinds of value
cannot be held there:

1. **Irrational values.** `ln 2`, `exp 1`, `log10 2`. Today the library has one precedent, `rounded_sqrt`
   (`rounded_root.hpp`): the formula states a unit, decimal places and a rounding mode, and the library computes
   the correctly rounded decimal in integers. The unrounded root is `Inexact`.
2. **Rational values too wide for 64 bits.** A least-squares slope through 50 readings at 4 decimals is an exact
   fraction whose numerator and denominator need well over 64 bits (`docs/numeric-headroom.md` measures the line
   overflowing from 34 points at 3 decimals). The exact route reports `Overflow`; nothing answers instead.

Printing a `double` trace was considered and rejected: about 45 helpers in `trace_render.hpp` are typed on
`Step<Rational>`; typed constants would lose their exact spelling; the same formula prints different last digits
on different platforms (libm differences, fused multiply-add on arm64), while traces are text two readers must
reproduce; and a `double` value can never reach a method's result, because `RepRounding<double>` refuses. Rounding
a `double` result to declared decimals was rejected too: its last digit is not guaranteed, which the library's
promise ("never a wrong number") does not allow.

## 2. The rule

> **A value the exact layer cannot hold is written only as the rounding its formula declares.** A node that
> produces one declares a unit, decimal places and a rounding mode — the triple `rounded<>` and `rounded_sqrt`
> already take — and its value is the correctly rounded decimal, proved with integer arithmetic.

Consequences:

- The rounded value is an ordinary `Rational` (denominator a power of ten). Everything downstream — `Outcome`,
  `explain`, methods, rounding rules, constraints, records, worksheets, the page — works unchanged.
- **Integer arithmetic only.** No `<cmath>` in the exact path, no intrinsics, no `__int128`. The same inputs give
  the same digits on cl, clang-cl, clang, g++, Apple clang and under UBSan, at compile time and at run time.
- **The unrounded forms stay exact-or-refuse.** A plain node answers where the exact value is rational and fits,
  and is `Inexact` or `Overflow` elsewhere — never an approximation.
- **An undecidable rounding fails.** If the working precision cannot separate the value from a rounding boundary,
  or an exact fraction outgrows the working width, the answer is `Overflow`, never a guess.
- **`checked_evaluate_si<double>` stays** the exploratory, untraced route. A fused rounding node refuses `double`
  at compile time, as `rounded_sqrt` does.

## 3. Wide unsigned integers — `include/formula-cpp/detail/wide_int.hpp`

`detail::WideUnsigned<std::size_t Limbs>`: a fixed-width unsigned integer over `std::array<std::uint32_t, Limbs>`,
least significant limb first. Products are formed in `std::uint64_t` (cl has no 128-bit integer; the reasoning of
`rounded_root.hpp` applies). Everything is `constexpr` and `noexcept`.

Operations, each checked where it can overflow (overflow is reported as an empty `std::optional`, never wrapped):

| Operation | Notes |
|---|---|
| `from_u64`, `to_u64` (optional when it does not fit) | |
| `==`, `<=>`, `is_zero`, `bit_length` | |
| `checked_add`, `checked_sub` (optional when the result is negative), `checked_mul` | schoolbook multiply |
| `mul_small(std::uint32_t)`, `add_small` | |
| `shift_left` (checked), `shift_right` | shift counts guarded for UBSan |
| `divmod(WideUnsigned const& divisor)` → `{quotient, remainder}` | binary shift-subtract: simple, obviously correct; `divisor != 0` is a precondition the callers establish |
| `gcd` | binary |
| `pow10(unsigned)` (optional) | |

Signed values are carried as a sign flag beside a `WideUnsigned` (`detail::WideRatio<Limbs>` =
`{bool negative; WideUnsigned<Limbs> numerator; WideUnsigned<Limbs> denominator;}`, denominator non-zero).

Naming: locals avoid every name in the consumer-globals probe (`test/consumer_globals_tests.cpp`), which rules
out single letters and names like `sum`, `digits`, `low`, `high`, `bound`; cl's C4459 turns a clash into an error.

## 4. Exact rounding of a wide fraction

- `detail::round_wide_ratio(WideRatio<L> value, DecimalPlaces places, RoundingMode mode)`
  → `std::expected<Rational, ArithmeticError>`: the decimal `value` rounds to at `places`, as
  `Rational::from_decimal(kept, -places)`. Places outside −18…18, or a kept integer that does not fit
  `Rational::Int`, is `Overflow`, exactly as `checked_round` reports it.
- `detail::rounded_in_unit(WideRatio<L> coherentValue, Unit, DecimalPlaces, RoundingMode)`: converts the coherent
  value into the declared unit exactly (the unit's factor is a `Rational`), rounds there, and converts the rounded
  value back into the coherent unit, as `rounded_square_root_in` does (`rounded_root.hpp:263-290`). Units with an
  offset are refused at compile time, for `rounded_sqrt`'s reason.
- **Agreement with `checked_round`** is the acceptance criterion: for every `Rational` that `checked_round`
  accepts, `round_wide_ratio` of the same value gives the same result, in all seven modes, at places −18…18.
- **Enclosures.** For an irrational value, a kernel produces a pair of bounds `[lower, upper]`. Rounding is
  monotone in every mode, so if both bounds round to the same decimal, the value does too; if they do not, the
  answer is `Overflow`. `detail::decide_rounding(lower, upper, places, mode)` implements exactly this and nothing
  else.

## 5. `rounded_output` — a declared precision for an opaque operation's output

```cpp
auto const fit = formula::linear_least_squares(formula::observations<Time, 64>,
                                               formula::observations<Length, 64>, citation);
// millimetrePerSecond: a Unit the method declares, as examples/opaque_and_retry.cpp declares its rate unit.
auto const slope = formula::rounded_output<"slope", millimetrePerSecond,
                                           formula::DecimalPlaces { 4 },
                                           formula::RoundingMode::HalfEven>(fit);
```

- `rounded_output<Name, U, Places, Mode>(call)` builds
  `RoundedOpaqueOutputNode<I, Call, U, Places, Mode>`, the fused counterpart of `opaque_output<Name>(call)`, as
  `rounded_sqrt` is the fused counterpart of `rounded<>(sqrt(…))`. `rounded<>(opaque_output<>(…))` keeps meaning
  what it means today; it does not silently become the fused route.
- Its dimension is the output's; its unit is `U`.
- **Refusals**, each a `formula: …` message and a negative test:
  - an unknown output name — the existing `RequireOpaqueOutputNamed`;
  - `U` does not measure the output's dimension — "formula: this rounded_output names a unit that does not measure
    the dimension of the output it rounds; the output and the unit appear in this diagnostic as the template
    arguments of RequireRoundedOutputUnitMatches";
  - `U` has an offset — "formula: this rounded_output names a unit with an offset, such as degrees Celsius; a
    fitted coefficient is a difference or a ratio, which an offset unit would misstate; name the unit without its
    offset";
  - a refused call — silent, as every node over a refused operand is (one message per mistake);
  - `Rep = double` — the existing `RepRounding<double>` refusal.
- **Evaluation.** The call's inputs are evaluated in `Rational`, exactly as for `opaque_output`, including
  failures (`Propagated`, with the input's site) and absence (an absent input makes the output absent). Then:
  - if the operation declares `compute_exact` (detected with a `requires` expression; an internal hook for now,
    not yet a documented customisation point), it is called with the same arguments `compute<Rational>` takes and
    returns `std::expected<std::array<detail::WideRatio<L>, M>, ArithmeticError>`; the named output is rounded with
    `rounded_in_unit`;
  - otherwise `compute<Rational>` runs and its output is rounded exactly with `checked_round` in `U`.
  An error from either is the operation's own (`OpaqueFailure::Own`), as today.
- The plain `opaque_output` route is unchanged: `compute<Rational>`, exact or `Overflow`.

## 6. Trace

- A new `StepKind::RoundedOpaqueOutput`, appended to the enum; the recorder reads `unit`, `places` and `mode` from
  the node by name, as it does for `rounded_sqrt`.
- `OpaqueStepData` (`trace.hpp:1394-1412`) and `OpaqueCallInfo` (`opaque.hpp:135-145`) gain
  `OpaqueValues values`, `enum class OpaqueValues : std::uint8_t { Exact, RoundedWhereUsed }`.
- On the rounded route no output exists as a `Rational` until it is rounded, so **the call's line names its
  outputs without values**, and each output's own line states its rounding:

```
3. t = 1 s; 2 s; 4 s; 7 s
4. L = 10.2 mm; 10.9 mm; 12.1 mm; 14.3 mm
5. linear least squares(#3, #4) = intercept, slope, r squared, points: rounded where used [inside not shown] [Rate of change, Example Standard 12, 5.1]
6. round(slope of #5, to 4 dp of mm/s) = 0.6786 mm/s [nearest, ties to even]
```

  The exact route keeps its line: `… = intercept = 19/2 mm; slope = 19/28 mm/s; … [inside not shown]`.
- The rounded value is the step's value: **no `≈` in any number style**. In fractions it reads `3393/5000 mm/s`.
- A failure reads like any opaque failure: `6. round(slope of #5, to 4 dp of mm/s) = overflow in exact arithmetic
  [the operation itself failed, not any input]`.
- `render_trace` and `render_derivation` stay `Rational`-only.

## 7. Page, rendering, overlays, walks

- `render_node`: Plain/Markdown `round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)`; LaTeX
  `\operatorname{round}_{4\,\mathrm{mm/s}}(\text{linear least squares}(t_{i}, L_{i})_{\text{slope}})`, modelled on
  `rounded_sqrt`'s spelling (`render.hpp:1293-1315`). The mode is not part of the formula text, as for `rounded<>`.
- `document`: the call is listed once among the page's opaque operations, as for `opaque_output`; the formula text
  already carries the precision.
- `LevelChildren` (`precision.hpp`), `ConstantRewrite` and `SubstitutedIn` (`overlay.hpp`) gain entries beside
  `OpaqueOutputNode`'s; `LevelChildren` also drives the calculation, rejection and retry walks.

## 8. Display rules — additions to `docs/display.md`

The rule at the top of the page gains a clause: "…and a value the exact layer cannot hold is written only as the
rounding its formula declares." A new section, after "What no style rounds":

> ### Values the exact layer cannot hold
>
> A logarithm, an exponential or a real power is irrational almost everywhere, and a line fitted through fifty
> readings at four decimals is a fraction whose numerator and denominator need hundreds of bits. The library does
> not approximate them. A formula that needs one **declares the precision it is reported at** — a unit, decimal
> places and a rounding mode, as `rounded<>` does — and the library computes the decimal the true value rounds to.
>
> - **The rounded decimal is the step's value, and it is exact.** It is the correct rounding of the true value,
>   proved by integer arithmetic. The same inputs give the same digits on every compiler and platform.
> - **Its line says it was rounded, and carries no `≈`.** `≈` marks a style's rounding of a value a step holds;
>   this rounding is the formula's, stated in the expression, its mode in the brackets.
> - **The true value is written nowhere.** An opaque call evaluated for a rounded output names its outputs without
>   values; each output's own line states its rounding.
> - **Without a declared precision, these values are refused, never approximated.** `ln(x)` answers only where the
>   logarithm is rational and is `Inexact` elsewhere, as `sqrt(2)` is; `opaque_output<"slope">(fit)` is the exact
>   slope or `Overflow`.
> - **Only a rational value can tie**, and its tie is broken by the mode as `checked_round` breaks it.
> - **A rounding that cannot be decided fails** with `Overflow` — never a guess.
> - **Floating point never reaches a trace.** `checked_evaluate_si<double>` answers approximately, on purpose, and
>   its trace has no printable form.

Also updated: `docs/opaque-and-retry.md` (the "no traced fallback in double" paragraph names `rounded_output`),
`docs/expressions.md` (irrational results: the rounded forms beside the `double` route), and the comments at
`trace_render.hpp` (number-style policy; the `Rational`-only `static_assert`), `trace.hpp` (`explain`),
`least_squares.hpp` (the `double` route) and `evaluate.hpp` (the two entry points).

## 9. Acceptance on the existing fit

`LinearLeastSquares` (the curve fit, `least_squares.hpp`) gains `compute_exact`: the same closed form as
`compute`, over integer sums after bringing each series to a common denominator. Its outputs, names, pinned
exact-route lines and census figures are unchanged.

- For every data set on which the exact route answers, `rounded_output<"slope", …>(fit)` equals
  `rounded<…>(opaque_output<"slope">(fit))`, in every mode.
- On the census's load-cell data at 3 decimals, where the exact route overflows from 34 points, `rounded_output`
  answers up to 128 points; a new census row in `docs/numeric-headroom.md` pins where it stops, if it does.
- Many distinct denominators (the census's stress row) can outgrow the working width; the honest answer is
  `Overflow`, pinned by the census.

## 10. Tests

- `test/wide_int_tests.cpp`: agreement with 64-bit arithmetic on random operands; carry chains at 2^32, 2^64 and
  the top limb; `q·d + r == n` with `r < d`; overflow reported, never wrapped; `STATIC_REQUIRE` at 4 limbs.
- Rounding: `round_wide_ratio` equals `checked_round` on a grid of numerators and denominators, all 7 modes,
  places −18…18; `IntMin`; ties; `Overflow` past `Rational::Int`. `decide_rounding`: bounds that agree, bounds that
  straddle a boundary (→ `Overflow`), both in every mode.
- `rounded_output`: value and failure cases, absence, a propagated input failure with its site, every refusal
  as a negative test (expected text written wrong first, then a deletion check), Plain/Markdown/LaTeX rendering,
  the page listing the call once, pinned trace lines in the fraction, exact-decimal, approximate and padded styles
  (no `≈` on the rounded line), a worksheet whose value is a `rounded_output` through `render_derivation`, overlay
  and level walks, `double` refused.

## 11. Out of scope

- Significant-digit declarations (`rounded_to_digits` analogues); a later addition that breaks nothing.
- A documented `compute_exact` customisation point for consumers' operations; internal until a second consumer.
- An interval representation for whole formulas; printable `double` traces; floating point then rounding.

## 12. Risks

- cl's constexpr step budget (default 100 000) at large widths: heavy cases are pinned at run time; small widths
  keep `STATIC_REQUIRE` coverage.
- Appending `StepKind` enumerators makes a consumer's exhaustive `switch` warn; recorded under "Changed" in the
  CHANGELOG.
- Double rounding when an author rounds an intermediate: visible in the formula text and the trace, and what
  methods state.
