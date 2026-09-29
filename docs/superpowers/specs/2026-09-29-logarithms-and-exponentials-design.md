# Logarithms and exponentials — design

**Status:** draft, ready for review · **Date:** 2026-09-29 · **Owner:** Christian Parpart

Issue #3. Builds on `2026-09-29-declared-precision-design.md` (the rule, the wide integers, the rounding of an
enclosure).

## 1. Problem

`function.hpp` offers integer powers, roots and `pi`. Test methods also take the natural logarithm, the decimal
logarithm and the exponential of a dimensionless expression: to linearise a measured quantity, to state a model
curve, to report on a logarithmic axis. Today a consumer declares each as an opaque operation, so the page shows a
call and the trace marks it `[inside not shown]`, although it is an ordinary function a method writes out.

## 2. What is added

Two forms of each function, following the library's own pattern for roots (`sqrt` and `rounded_sqrt`):

| | Plain: `ln(x)`, `log10(x)`, `exp(x)` | Rounded: `rounded_ln<P, M>(x)`, `rounded_log10<P, M>(x)`, `rounded_exp<P, M>(x)` |
|---|---|---|
| The page | the function, in every dialect | `round(ln(x), to 4 dp)` |
| `checked_evaluate`, `explain`, methods, worksheets (`Rational`) | exact where the value is rational (ln 1 = 0, exp 0 = 1, log10 10^k = k); `Inexact` elsewhere | the correctly rounded decimal, proved in integers |
| `checked_evaluate_si<double>` | answers directly | refused at compile time, as `rounded_sqrt` is |

The plain form fixes the page and the `double` route and is exact at its rational points; its `Inexact` elsewhere
tells the author to declare a precision. The rounded form is what lets a method evaluated exactly drop its opaque
operation. `rounded<>(ln(x))` does **not** silently become the rounded form: the plain node fails before the
rounding node sees a value, for the reasons `rounded_root.hpp` gives for roots.

**A real power `pow(base, exponentExpr)` is out of scope.** A constant rational exponent is already
`pow<P>(root<Q>(x))`, dimension-checked and exact wherever the answer is rational. An exponent computed from data
is `exp(y · ln x)`, and exactly `rounded_exp<…>(y * rounded_ln<…>(x))`, with both roundings declared and visible. A
dimensioned base under a run-time exponent has no compile-time dimension anyway. If a real power is ever added it
is named `power`, not `pow`, with its own step kind. `docs/expressions.md` says this.

## 3. Plain nodes — `function.hpp`

```cpp
enum class Transcendental : std::uint8_t { NaturalLogarithm, DecimalLogarithm, Exponential };

template <Transcendental F, Node Operand>
struct TranscendentalNode : NodeBase
{
    static_assert(detail::RequireDimensionlessArgument<Operand>::value);
    Operand operand;                                        // named `operand`; no `{}` initialiser
    static constexpr Transcendental function = F;
    static constexpr Dimension dimension = dim::Scalar;
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

template <Node Operand> constexpr auto ln(Operand operand) noexcept;     // and log10, exp
```

- One node over an enum, as `RootNode` covers `sqrt`, `cbrt` and `root<N>`. The member is named `operand` so
  overlays' `ConstantRewriteOperand` applies. The node declares none of `exponent`, `degree`, `places`, `digits`,
  `mode`, `unit`, `keyUnit`: the trace recorder reads those by name.
- The factories take a `Node` only, never a `Rational`, so they cannot be confused with `formula::pow(Rational,
  int)`.
- `RepFunctions<Rep>` (the public extension point beside `RepTraits`) gains `natural_log`, `decimal_log` and
  `exponential`. A consumer's own `Rep` needs them only to evaluate these nodes.

| Case | `Rational` | `double` |
|---|---|---|
| log of a value ≤ 0 | `DomainError` | `DomainError`, from a guard `!(v > 0.0)` that also catches NaN, before any library call |
| ln 1, exp 0, log10 10^k (\|k\| ≤ 18) | exactly 0, 1, k | the standard library's result |
| any other value | `Inexact` | `std::log`, `std::log10`, `std::exp` — **qualified**: inside namespace `formula` an unqualified call would find `formula::exp` |
| exp too large | `Inexact` | `+inf` passes, as it does for `pow<N>` and `RepTraits<double>`; pinned by a test |
| an absent operand / a failed operand | absent / the operand's error wins | the same |

log10 10^k is detected exactly: the value is a power of ten when its reduced numerator or denominator is 1 and the
other is 10^|k|.

**Dimension.** The operand must be dimensionless. `detail::RequireDimensionlessArgument` sits in `function.hpp`
beside `RequirePositiveRootDegree` (so `expression.hpp`, whose line numbers the docs quote, is untouched), and is
gated on `refused_already`, so an operand refused for another reason gives one message:

> formula: the argument of this logarithm or exponential is not dimensionless; ln, log10 and exp take a bare
> number, and of a quantity they would change with the unit it is read in -- divide it by a reference value of its
> own dimension, or read it with numeric_value_of; the argument appears in this diagnostic as the template
> argument of RequireDimensionlessArgument

A percentage is dimensionless and read in the coherent unit, so `ln` of 5 % is ln 0.05, and `log10` of 1000 % is 1.
The node's own `refused` flag passes the operand's through: `ln(L) + m` still reports the addition as well.

## 4. Rounded nodes — new `include/formula-cpp/rounded_transcendental.hpp`

```cpp
template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
struct RoundedTranscendentalNode : NodeBase { /* operand, function, places, mode, dimension = Scalar, refused */ };

template <DecimalPlaces Places, RoundingMode Mode, Node Operand> constexpr auto rounded_ln(Operand) noexcept;
// rounded_log10, rounded_exp
```

- **No unit parameter.** Operand and result are pure numbers; the step's unit is the coherent one, and the trace
  writes `to 4 dp`. (Significant digits, which suit `exp` across wide ranges, can be added later without breaking
  anything.)
- `Rational`: the special points (the only values that can tie) go to `checked_round`; everything else goes to
  the kernel (§5). ≤ 0 under a logarithm is `DomainError`; absence and operand errors as for the plain node.
- Any other `Rep` is refused by `RepRounding<double>` at compile time, as `rounded_sqrt` is.
- Same dimension check, sharing `RequireDimensionlessArgument`.

## 5. The kernel — new `include/formula-cpp/detail/transcendental.hpp`

Integer fixed point on `detail::WideUnsigned` (from the foundation): 64 integer bits and 128 fraction bits. Every
operation rounds in a known direction, so each result is an **enclosure** `[lower, upper]` of the true value, and
`detail::decide_rounding` either returns the decimal both ends round to or `Overflow`.

- **ln(a/b)**, a, b > 0: if a < b, compute ln(b/a) and negate (so only the case a > b is needed). Choose
  B = b·2^k with B ≤ a < 2B (then a + B < 2^64). With z = (a − B)/(a + B) ∈ [0, 1/3):
  ln(a/b) = k·ln 2 + 2·Σ z^(2i+1)/(2i+1). About 39 terms reach 2^-124 (23 with an extra ln(3/2) reduction, if the
  step budget asks for it).
- **log10** = ln · log10(e), both enclosed.
- **exp(a/b):** early exits from rational bounds taken in the safe direction — certain overflow of the rounded
  result is `Overflow`; a value certainly below a quarter of the last kept unit is 0 in the nearest and toward-zero
  modes and 10^-p under `Ceiling` and `AwayFromZero` (exp is always positive). Otherwise k = round(x/ln 2),
  r = x − k·ln 2 with |r| ≤ 0.35, about 24 Taylor terms, result = exp(r)·2^k.
- **Constants** ln 2 and log10(e) are stored as enclosures and re-derived by the kernel itself in a test, and
  checked against published digits.
- **Error bound** ≤ 2^-118, absolute for ln and log10, relative for exp, derived in the header comment (as
  `rounded_root.hpp` derives its own): truncations of at most one unit in the last place each, the series tail,
  and k·2^-128 from ln 2. The sign of a logarithm is always decided: |ln(a/b)| > 2^-63 for a ≠ b.
- **Undecided** (the ends round differently) is `Overflow`: the rounding needs more bits than the kernel holds;
  the rounded decimal exists, so `Inexact` would be wrong, and a new error enumerator would break
  `describe()` and consumers' switches. At places ≤ 18 the odds are below about 10^-17 per evaluation; there is no
  escalation to more bits.
- Places outside −18…18, or a kept integer that does not fit `Rational::Int`, is `Overflow`, as in `checked_round`.
- The first task **measures** cl's constexpr step budget (default 100 000) for one kernel call and records it;
  the target is at most 25 000 steps. Heavy cases are pinned at run time.

## 6. Rendering and trace

| | Plain | Markdown | LaTeX |
|---|---|---|---|
| ln | `ln(x)` | ``ln(`x`)`` | `\ln\left(x\right)` |
| log10 | `log10(x)` | ``log10(`x`)`` | `\log_{10}\left(x\right)` |
| exp | `exp(x)` | ``exp(`x`)`` | `\exp\left(x\right)` |
| rounded | `round(ln(x), to 4 dp)` | ``round(ln(`x`), to 4 dp)`` | `\operatorname{round}_{4}(\ln\left(x\right))` |

- Always the call form with parentheses, as `sqrt(A)`. No precedence override: the node groups its own operand,
  so `pow<2>(ln(x))` renders `ln(x)^2`, `\ln\left(x\right)^{2}`.
- `\exp`, not `e^{x}`: `PowerNode` renders its base as an atom, so `pow<2>(exp(x))` would become `e^{x}^{2}`, a
  LaTeX error; and `e` is a common quantity symbol (void ratio).
- The head names live in one `constexpr std::string_view` helper used by `render.hpp` and `trace_render.hpp`.
- Six step kinds, appended: `NaturalLogarithm`, `DecimalLogarithm`, `Exponential`, `RoundedNaturalLogarithm`,
  `RoundedDecimalLogarithm`, `RoundedExponential` (none is a name in namespace `formula`; checked under g++
  `-Wshadow`). Trace lines: `ln(#1)`, `log10(#1)`, `exp(#1)`; a failure reads
  `2. ln(#1) = no exact rational result exists`; the rounded form is one fused step,
  `2. round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties away from zero]` — the irrational value never appears.

## 7. Registries

`LevelChildren` (`precision.hpp`, mandatory; also drives the calculation, rejection and retry walks),
`ConstantRewrite` and `SubstitutedIn` (`overlay.hpp`; the primary `SubstitutedIn` silently answers "none", so its
entry is needed for correctness), `document` `collect` (declaration and definition), `render_node`, `StepKindOf`,
`step_expression` (no `default`, so every kind needs a case) and the mode suffix for the three rounded kinds. The
new headers join `formula.hpp`, the install file set in `CMakeLists.txt`, and `test/consumer_globals_tests.cpp`.

## 8. Tests

- `function_tests.cpp`: dimensionless result; `STATIC_REQUIRE` on ln 1 = 0, exp 0 = 1, log10 1000 = 3,
  log10 1/100 = −2, log10 10^±18; `Inexact` elsewhere; `DomainError` at 0 and −1 in both representations; `double`
  within bounds (ln 2 ∈ (0.69314718, 0.69314719)); `exp(1000)` is `+inf` in `double`; absence; the operand's error
  wins; percent operands.
- `transcendental_tests.cpp`: stored constants re-derived and matching published digits; about 30 inputs against a
  40-digit reference table (computed once with an arbitrary-precision tool, source and digits in the test file), in
  all 7 modes at several places, including 2, 1/2 (sign cases), 2^62, (2^63−1)/(2^63−2), exp at ±1 and ±43; a
  near-boundary input whose rounding a `double` computation would get wrong; a straddling enclosure → `Overflow`;
  constexpr smoke tests.
- `rounded_transcendental_tests.cpp`: ln 2 at 4 dp = 6931/10000; log10 2 = 301/1000; exp 1 = 27183/10000;
  exp −1 at 3 dp = 46/125; ties at the special points in the three half modes; `Overflow` (exp 50 at 6 dp); tiny
  exp → 0 or 10^-p by mode; domain errors; absence.
- Negative tests (expected text written wrong first, then a deletion check): a dimensioned operand through each
  factory (`EXPECT_COUNT 1`), through a hand-built aggregate, over an already-refused operand (`REJECT` the
  dimensionless message), and the rounded forms dimensioned and under `Rep = double`.
- Every-node-kind suites: `render_tests` (dialect table, Markdown inertness, bracketing), `vocabulary_tests`
  (with exactly representable inputs such as `log10(1000)` and `exp(ln(r / r))`), `overlay_tests`,
  `measured_tests` (absence), `trace_tests`, `trace_render_tests`, `document_tests`, `calculation_tests`,
  `consumer_globals`.

## 9. Documentation

- `docs/expressions.md`: "Logarithms and exponentials" after "Powers, roots and pi": the dimensionless rule and its
  message, the `numeric_value_of` route, percent semantics, the exactness policy (special points, `Inexact`,
  `double`, the rounded forms and how to choose places), the error table, the rendering table, and the note on
  real powers.
- `docs/gallery.md`: one entry, a logarithmic reduction `round(log10(N_0 / N), to 2 dp)`, regenerated so
  `gallery.is-current` passes.
- `docs/numeric-headroom.md`: one sentence — the kernel's working words sit outside the overflow census; its
  final `Rational` is counted as usual.
- CHANGELOG `[Unreleased]`: Added (the plain and rounded forms, the new `RepFunctions` members); Changed (six new
  `StepKind` enumerators break a consumer's exhaustive `switch`; an unqualified `ln`/`exp`/`log10` call on a
  formula node now finds the library's constrained template by ADL).

## 10. Risks

- The kernel's error bound is what "never a wrong number" rests on: it gets its own review against the reference
  table, and every bound rounds in its safe direction.
- cl's constexpr step budget (measured first).
- `-Wshadow` on the new enumerators; `-Wconversion` on every narrowing; UBSan on shifts.
- Locals must avoid the consumer-globals names; no standard numbers ("IEEE", "ISO" followed by digits) in text
  (`hygiene.no-real-standards`).
