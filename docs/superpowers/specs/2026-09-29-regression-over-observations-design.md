# Regression over observations: a line with R², and multiple regression — design

**Status:** draft, ready for review · **Date:** 2026-09-29 · **Owner:** Christian Parpart

Issues #4 and #5. Builds on `2026-09-29-declared-precision-design.md` (wide integers, `rounded_output`,
`compute_exact`).

## 1. Problem

`linear_least_squares` fits a curve whose length is part of the formula, and evaluates only in `Rational`:

1. **The number of points is data.** A method fits "all determinations of the series that meet condition X".
   `observations<Q, Capacity>` models exactly that for statistics, but cannot be the input of an opaque operation.
2. **Realistic data outgrows 64 bits.** About 50 points at 4 decimals overflows the exact route; the only way out
   today is a `double` computation that reaches neither the trace nor the page.
3. **No coefficient of determination.** Methods report R² and use it as an acceptance criterion.
4. **No multiple regression.** `y = c0 + c1·x1 + c2·x2` is computed outside the library, untraced, and entered as if
   measured. Spreadsheets answer a singular design with arbitrary coefficients.

## 2. Observations as an opaque input

**Move** `ObservationsNodeBase`, `ObservationsNode`, `ObservationsVarNode`, `observations`, `ObservationsValue`,
`EvaluatedObservations`, `HearsObservations` and `evaluate_observations` from `binning.hpp` into a new
`include/formula-cpp/observations.hpp` (a pure refactor; `binning.hpp` includes it; the stale comment "the only thing
that reads it is `binned`" goes). Otherwise `opaque.hpp` would pull in binning, lookup and bands.

**A new shape, `InputShape::Observations`:** any `ObservationsNode`; it arrives in `compute` as one
`std::span<Rep const>` over the observations **made** — their count, not the capacity, possibly empty — pointing
into the evaluated value, with no copy; it contributes one dimension; it evaluates in any `Rep`.

- `OpaqueInput<ObservationsNode>`: known, shape `Observations`, run-time length (0), so `opaque_lengths_agree`
  skips it — counts are not a framework rule, because an operation over two independent samples wants different
  counts.
- Overloads in `evaluate_opaque_input`, `opaque_input_failure`, `opaque_input_present` (observations are never
  absent: none made is an empty span), `opaque_held` and `opaque_arguments`.
- `OpaqueCallFailure` gains `FailureSite site` as its last member (default `ResultElement`, so existing aggregate
  initialisations stay valid). A conversion failure at observation k propagates with
  `site = InputObservation`, and the trace says `[carried up from #1, at observation 3]`.
- Render, document, overlays and level walks already handle observations; tests prove each reaches the input.

**Pairing** uses one observation input per column, not a pairs type: observation i of each input belongs to row i.
The docs say so ("build every column from the same rows"). Inputs whose counts differ make the fit fail with its
own `DomainError`.

## 3. #4 — a line through observations

```cpp
auto const fit = formula::linear_least_squares(formula::observations<Time, 64>,
                                               formula::observations<Length, 64>,
                                               formula::Citation { "Rate of change", "Example Standard 12", "5.1" });
formula::opaque_output<"slope">(fit)                                   // exact, or Overflow
formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 },
                        formula::RoundingMode::HalfEven>(fit)          // correctly rounded at any size
formula::constraint(formula::rounded_output<"r squared", formula::unit::One, formula::DecimalPlaces { 4 },
                                            formula::RoundingMode::Floor>(fit)
                    >= formula::constant<formula::unit::One>(formula::Rational { 999, 1000 }))    // acceptance
```

- A new operation `LinearLeastSquaresOfObservations`, reached by an overload
  `linear_least_squares(ObservationsNode x, ObservationsNode y, Citation)`. The existing curve refusal gains a
  constraint so overload resolution is unambiguous on every compiler, and its message names both routes: "formula:
  linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a value
  series with curve(domain, values), or read both with observations<Q, Capacity>".
- Name `"linear least squares"`; outputs `intercept`, `slope`, `r squared`, `points`; dimensions
  `{y, y/x, Scalar, Scalar}` (the trace borrows `mm/s` from the inputs, as it does for the curve fit).
- **Exact pre-checks, in every `Rep`, by `==`, before any sum:** the two counts agree; n ≥ 2; the domain is not
  flat; the values are not flat (then R² would be 0/0 — the whole fit is its own `DomainError`, so a flat response
  never passes an R² acceptance check). All are the operation's own `DomainError`.
- `R² = SS_reg / SS_tot` with centred sums, dimensionless; for a line, `S_xy² / (S_xx · S_yy)`.
- `points` is the count (exact in both representations): shown on the call's line and usable downstream, for
  example for degrees of freedom `n − 2`.
- The existing curve operation keeps its outputs, lines and census figures; it gains only `compute_exact`
  (foundation §9). Moving it onto the new kernel would change its pins and make it refuse flat values.

**Evaluation**

| Route | Kernel |
|---|---|
| `compute_exact` (for `rounded_output`) | the exact kernel (§5) in wide integers |
| `compute<Rational>` (for `opaque_output`, `checked_evaluate`, `explain`) | the same exact kernel, each output narrowed to `Rational`; `Overflow` if one does not fit — all outputs of one call answer or fail together |
| `compute<double>` (for `checked_evaluate_si<double>`) | centred two-pass sums through `RepTraits`, one operation per call (never `a*b + c` in one expression, so no fused multiply-add); untraced |

**Fixture** (the guide's data: t = 1, 2, 4, 7 s; L = 10.2, 10.9, 12.1, 14.3 mm): intercept 19/2 mm, slope
19/28 mm/s, R² = 1083/1085, 4 points. The exact call line reads
`linear least squares(#1, #2) = intercept = 19/2 mm; slope = 19/28 mm/s; r squared = 1083/1085; points = 4 [inside not shown] [Rate of change, Example Standard 12, 5.1]`.

## 4. #5 — multiple linear regression

```cpp
auto const fit = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 64>, formula::observations<Content, 64>),
    formula::observations<Length, 64>, citation);
formula::rounded_output<"coefficient 2", …>(fit)
```

- `regressors(x1, …, xK)` builds a small holder (a parameter pack before the citation is not deducible);
  `multiple_least_squares(regressors(…), y, citation)` returns `OpaqueCall<MultipleLeastSquares<K>, X1, …, XK, Y>`.
  Regressors first, values last, as the curve has points then values; the trace reads
  `multiple least squares(#1, #2, #3)`.
- Outputs: `constant`, `coefficient 1` … `coefficient K`, `r squared`, `points` — generated into static storage of
  the class template and satisfying the readable-name rule (ASCII letters, digits, single spaces). Positions in
  text are one-based. A cross-translation-unit test guards the generated names.
- Dimensions `{y, y/x1, …, y/xK, Scalar, Scalar}`, so the trace borrows units such as `mm/K`.
- **K is 1 to 8.** Beyond 8, and with no regressor, a compile-time refusal; `K = 1` equals the line.
- **Compile-time refusals**, each a `formula: …` message and a negative test: uncited; a series (not observations)
  as a regressor or the values; no regressor; more than 8; **the same quantity twice** (as two regressors, or as a
  regressor and the values — the design would be singular whatever was observed).
- **Exact pre-checks** (every `Rep`): counts agree, n ≥ K + 1, values not flat, no regressor flat.
- **A singular design is an error, not a number.** In the exact kernel it is decided exactly (a zero pivot left
  after fraction-free elimination) and is the fit's own `DomainError`. In `double` (the untraced route) it is
  decided by a stated relative-pivot tolerance: singular when a pivot of the centred normal equations
  `Dₖ ≤ 10⁻⁹ · S_kk` (that is, 1 − R²ₖ of regressor k on the ones before it is below 10⁻⁹). The docs state
  honestly that exact singularity cannot be decided from rounded data, and that a design near the tolerance has
  few trustworthy digits either way.

## 5. The exact kernel (wide integers)

Inputs are exact `Rational`s. Each column is brought to a common denominator (the least common multiple of its
elements' denominators, in wide integers); decimal data has power-of-ten denominators, so this stays small. Then
every sum is an integer:

- counts and sums `ΣX_j`, `ΣY`, cross sums `ΣX_jX_k`, `ΣX_jY`, `ΣY²`;
- centred, scaled by n: `M_jk = n·ΣX_jX_k − ΣX_jΣX_k`, `v_j = n·ΣX_jY − ΣX_jΣY`, `T = n·ΣY² − (ΣY)²`;
- **K = 1:** slope `v/M` (rescaled by the columns' denominators), intercept `ȳ − slope·x̄`, R² `v²/(M·T)`;
- **K ≥ 2:** fraction-free (Bareiss) elimination with row exchanges on `[M | v]`; a column with no non-zero pivot
  is a singular design; back-substitution gives each coefficient as an exact fraction; the constant is
  `ȳ − Σ c_j x̄_j`; `R² = Σ c_j v_j / T` (rescaled).

Every output is a `detail::WideRatio`. The width is a constant of the kernel sized for K ≤ 8 and the census's
realistic data, measured and pinned; a sum or a pivot that outgrows it is `Overflow` — many distinct denominators
(the census's stress row) can do that, and the census pins where.

The `double` route runs the same algorithm shape — means, then one pass of centred sums, then square-root-free
Cholesky (LDLᵀ) with the tolerance above — never storing n-sized arrays.

## 6. Trace, page, docs

- Trace: one opaque step per output used (each output re-runs the call — documented; sharing one call among
  outputs stays out of scope). On the rounded route the call's line names its outputs without values (foundation
  §6). Failures read as today, with the new "at observation k" suffix for input conversions.
- Page: the call and its citation are listed once; observations inputs render `t(i)`, `L(i)`.
- `docs/opaque-and-retry.md`: "A line through observations" (exact route, rounded route, R², points, the
  flat-values rule, pairing by rows) and "Several regressors" (API, outputs, the singular-design table, the
  tolerance statement); the "no traced fallback in double" paragraph names `rounded_output`.
- `docs/numeric-headroom.md`: census rows for the line with R² at 3 and 4 decimals and for K = 2 — where the exact
  route stops and where the rounded route stops, if it does.
- `examples/opaque_and_retry.cpp` (its guide's output is checked by `docs.opaque-and-retry-output`): the line over
  observations with R², a 50-point fit at 4 decimals answered through `rounded_output`, a two-regressor fit, and a
  refused singular design.
- `docs/statistics.md` notes that observations also feed fits; README and docs index feature rows.
- CHANGELOG `[Unreleased]`: Added (`InputShape::Observations`, the observation fit with `r squared` and `points`,
  `multiple_least_squares`, `regressors`); Changed (`OpaqueCallFailure::site`, the "at observation" suffix,
  `RequireFitOfCurve`'s wording, the new `InputShape` enumerator).

## 7. Tests

- Opaque framework, with a test-local operation over observations: `compute` sees the count not the capacity; zero
  observations is an empty span; run-time `MeasuredObservations::from` input; both representations; a conversion
  overflow at observation 3 is `Propagated` with site `InputObservation` and renders "at observation 3"; mixed
  single-value and observations inputs; different capacities allowed; render, document, overlay and level walks
  reach the input.
- Kernel: K = 1 equals the closed form; row order does not change the result; each pre-check in both
  representations; K = 2 exact solve; singular designs `x2 = 2·x1`, `x2 = x1 + 273.15`, `x2 = 3·x1 − 7` refused
  (exactly in the wide kernel, by the tolerance in `double`); a near-collinear valid design (1 − R² ≈ 10⁻⁶) accepted
  with correct coefficients; n = K + 1 gives R² = 1; n = K is `DomainError`; `double` within 10⁻¹² relative of the
  exact result on well-conditioned data.
- #4: `STATIC_REQUIRE` on the fixture; trace, render (all dialects), page; inside a method variant with a rounding
  rule and with an R² constraint; a 50-point, 4-decimal fixture where `opaque_output` is `Overflow` and
  `rounded_output` answers, checked against reference values computed independently (source stated in the test).
- #5: the same, plus K = 1 equals the line, each output's dimension, and the generated names across translation
  units.
- Census rows as in §6.
- Negative tests (expected text wrong first, then a deletion check): the observation fit uncited, observations
  mixed with a series, the same quantity as points and values; `multiple_least_squares` uncited, a series
  regressor, no regressor, more than 8, a repeated regressor; an observations input where a shape is refused; an
  overlay constant inside the fit; a calculation definition that reads the fit's observations (the existing
  refusal, pinned).

## 8. Out of scope

Worksheets and calculations reading observations (the existing refusal stays and is pinned);
`BoundEnvironment` forwarding observations; weights, a line through the origin, polynomial terms (with #5 they
are consumer-built columns); residuals (n values, not one output); standard errors and the residual sum of squares
(adding outputs later changes every trace line of the call, so it is a "Changed" entry when it comes); fits over
`without_outliers` (it drops rows per input and breaks pairing); sharing one call among several outputs;
op-declared failure reasons ("the regressors are collinear") beyond the library's existing wording.

## 9. Risks

- cl C4459 on kernel locals that match consumer-globals names; never value-initialise an array of `Rational`.
- g++ evaluating `consteval` checks behind a `&&` whose left side is false: each new check gets its own
  `if constexpr` stage, as `opaque.hpp` already does.
- constexpr step limits: constexpr fixtures stay at 8 observations or fewer; larger ones run at run time.
- cl and string views into static storage of a class template in constant expressions (the generated output
  names): the cross-TU test guards it.
- The exact route's headroom for the new line is lower than the curve fit's (R² needs wider intermediates and all
  outputs fail together); the census pins it, and `rounded_output` is the documented answer.
