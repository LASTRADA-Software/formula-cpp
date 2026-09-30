# Regression over Observations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fit a line, with its coefficient of determination and its number of points, through raw observations
whose count is data (issue #4), and fit several regressors at once (issue #5). Both answer exactly where the exact
layer can hold the result, correctly rounded at any size through `rounded_output`, and approximately in `double`;
a singular design is an error, never a number. Today `linear_least_squares` fits only a curve, whose length is part
of the formula (`least_squares.hpp:188-192`), reports no R², and overflows from 34 points at 3 decimals
(`docs/numeric-headroom.md:307-315`).

**Architecture:**
- The raw observations move out of `binning.hpp` into a header of their own, `observations.hpp`, so that
  `opaque.hpp` can read them without pulling in classes, lookups and bands.
- `InputShape::Observations`: an opaque operation's input may be raw observations. `compute` receives one
  `std::span<Rep const>` over the observations **made**, pointing into the evaluated value; counts are not a
  framework rule. `OpaqueCallFailure` gains `site`, and a relayed conversion failure reads `at observation k`.
- `detail/least_squares_kernel.hpp`: an exact kernel in the foundation's wide integers (common denominators,
  integer sums, centred sums times n, the line's closed form, fraction-free elimination with row exchanges for
  two or more regressors, back-substitution, R²) and its `double` counterpart (two passes, square-root-free
  Cholesky with a stated relative-pivot tolerance). Pure numerics, no nodes.
- `least_squares.hpp`: `LinearLeastSquaresOfObservations` behind a new overload of `linear_least_squares`, and
  `MultipleLeastSquares<K>` behind `multiple_least_squares(regressors(...), values, citation)`; each declares the
  foundation's `compute_exact` hook, so `rounded_output` rounds the exact kernel's wide result.

**Tech Stack:** C++23, header-only; Catch2 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness; the overflow
census (`test/overflow_census_tests.cpp`, `cmake/CheckCensusPage.cmake`).

**Spec:** `docs/superpowers/specs/2026-09-29-regression-over-observations-design.md` (issues #4 and #5), built on
`docs/superpowers/specs/2026-09-29-declared-precision-design.md` (wide integers, `rounded_output`,
`compute_exact`).

**Order:** this plan runs **after** the declared-precision (foundation) plan and the logarithms plan. Every line
anchor below was read at `c30f261`; those two plans move many of them. Find each place by the name quoted beside
its anchor, never by the number alone.

## Global Constraints

These bind every task. Several exist because the alternative failed in an earlier phase.

- **C++23, header-only**, no dependency beyond the standard library in shipped headers ("Header-only, no
  dependencies", README.md:3).
- **Per task: MSVC `cl-debug` and g++-14 only** (the owner's rule). All eight presets, Doxygen 1.9.8 and
  `mkdocs build --strict` run once, at the end, by the controller. Both commands print `ALL OK`,
  `BUILD FAILED` or `TESTS FAILED` and exit non-zero on failure:
  - **Windows (cl-debug):** `pwsh -NoProfile -File C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\6031eb20-da56-4aa2-bb6a-de8c11d53bfc\scratchpad\cl.ps1 [-Filter <ctest -R regex>] [-Target <target>] [-NoTest]`
    (VS dev shell, preset `cl-debug` in `out/build/cl-debug`, build, then `ctest -j 12`).
  - **g++-14 (WSL):** `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/6031eb20-da56-4aa2-bb6a-de8c11d53bfc/scratchpad/gcc14.sh [ctest -R regex]`
    (`--preset gcc-release -B out/build/gcc14-release -DCMAKE_CXX_COMPILER=g++-14`, the only build with
    `-Wshadow -Wconversion -Wpedantic -Werror`). **Never `wsl bash -lc '...'`** (it ate `$?`); a POSIX probe goes
    in a script file with an `(exit 42)` control that prints its code.
  - Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is a *build* error.
  - **Baseline.** At `c551c5d` both builds pass 1808/1808. The foundation and logarithms plans add tests before
    this one, so before Task 1 run both commands once, unfiltered, and write the two totals in Task 1's report.
    **Every count in this plan is a delta** ("+N cases", "+N negative tests", "+1 probe check"): the
    absolutes depend on the plans before this one and are fixed at dispatch. Each task reports its totals and
    the difference from the task before, which must equal its stated delta.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:** SPDX header on every file; no `NOLINT`;
  core public headers include no `<string>`, `<vector>`, `<format>` or `<iostream>` (`hygiene.headers`); every
  public `static_assert` message begins `formula: ` and is stable (negative tests match on it); never identify a
  type with `decltype([]{})`; every new header goes into the install `FILE_SET` in the root `CMakeLists.txt:41-100`
  (`hygiene.installed-headers`, which globs `include/**.hpp`, `detail/` included) and every new top-level public
  header into `test/consumer_globals_tests.cpp`'s includes (`hygiene.consumer-globals`, which globs
  `include/formula-cpp/*.hpp`).
- **Defect classes.** Read `D:\formula-cpp\.superpowers\sdd\2026-09-25-methods-and-overlays\defect-classes.md`
  before starting. Each task's report says, for each of its nine classes, what you checked and how.
- **No third-party standard content.** Cite only invented `Example Standard N:YYYY` references (this plan uses
  `Example Standard 12`). Never name a real standards body or standard number anywhere --
  `hygiene.no-real-standards` scans every tracked file. Fixture numbers are plainly invented (primes and odd
  decimals, none a Renard R40 value).
- **Negative tests** (`test/negative/<name>.cpp` + `formula_add_negative_test(<name> "<expected>" ...)` in
  `test/CMakeLists.txt`; ctest names them `negative.<name>`) assert that the build fails **with this library's own
  text**. Protocol for each: register it with a deliberately wrong expected string, run
  `-Filter "negative\.<name>"` and watch it fail; put the right string in and watch it pass. **Deletion check:**
  delete the guard it pins (the `static_assert` line), confirm the case then compiles (or, where the task says so,
  fails only on the text a `REJECT` names), restore the file **with a plain write** (a timestamp-preserving copy
  leaves ninja trusting stale outputs). `EXPECT_COUNT n` where a second message could plausibly fire; `REJECT`
  for the message the next check down would add.
- **No `{}` default member initialiser on any member that holds an expression or a node** (defect class 4):
  `Regressors::inputs`, like `OpaqueCall::inputs` (`opaque.hpp:698-701`).
- **Never value-initialise an array of class type** in code a consumer's build instantiates
  (`std::array<Rational, N> x {}`, `std::array<detail::WideSigned<L>, K> x {}`): cl 19.51 then instantiates a
  compiler-internal helper that declares an `i`, which hides a consumer's global (C4459; `series.hpp:720-731`).
  Default-initialise and fill in a loop. `consteval` functions over `Dimension` may keep `{}`, as
  `detail::opaque_input_dimensions` does (`opaque.hpp:442`).
- **Names.** Under g++ `-Wshadow` and cl C4459 no parameter or local may hide one of the globals in
  `test/consumer_globals_tests.cpp:125-146`. The ones this work reaches for: `row`, `rows`, `column`, `columns`,
  `i`, `j`, `k`, `n`, `m`, `x`, `y`, `v`, `sum`, `total`, `count`, `values`, `points`, `slope`, `intercept`, `fit`,
  `mean`, `factor`, `scale`, `scaled`, `upper`, `lower`, `tolerance`, `epsilon`, `copy`, `root`, `span`, `entry`,
  `element`, `previous`, `result`, `value`, `data`, `size`, `numerator`, `denominator`, `quotient`, `sign` -- read
  the whole list. Tasks 4 and 6 add `observation`, `observations`, `pivot`, `design`, `coefficient`,
  `coefficients`, `regressor` and `regressors` to it, so the kernel avoids those too. cl reports a non-template
  function's parameters and locals wherever its header is included, and a template's only where the probe
  instantiates it (`test/consumer_globals_tests.cpp:13-17`): `compute_exact` of the line (a non-template member)
  and the kernel's `line_outputs` are the first to trip it, so neither may name a parameter `points`.
- **g++ evaluates a `consteval` call behind a `&&` whose left side is false** (`opaque.hpp:588-592`): each new
  staged check asks its predicate in an `if constexpr` of its own and gates its `static_assert` with
  `std::conditional_t`, as `RequireOpaqueCallValid` does.
- **Constant evaluation.** Measured on cl 19.51.36257: a constant evaluation stops at about **1 049 000 steps**
  by default (for this plan: a loop of 300 000 trivial iterations compiled, 350 000 failed with C2131
  "evaluation exceeding step limit of 1048576"), and **one binary `divmod` over 12 limbs costs about 130 000
  steps** (measured by the controller). One exact fit makes dozens of wide divisions (a least common multiple and a
  scaling per element, a reduction and a division per output), so **no `STATIC_REQUIRE` runs the exact kernel's
  wide arithmetic**: every exact fit is checked at run time with `CHECK`. `STATIC_REQUIRE` covers what needs no
  wide operation -- widths, output names and dimensions, and refusals the pre-checks decide before any sum.
  Fixtures stay at 8 observations or fewer all the same, so that each can become a constant expression later.
- **Documentation is read by humans.** Every ```` ```text ```` block in a guide is consecutive lines of its
  example's real output and every ```` ```cpp ```` block consecutive lines of its source
  (`docs.opaque-and-retry-output` / `-snippets`, `examples/CMakeLists.txt:276-287`). A refusal quoted in `docs/`
  must be a header's message verbatim (`hygiene.documented-diagnostic-text`). No internal labels in any public
  text or commit message: no phase, task, lane, reviewer, planner, foundation or tracker names -- every sentence
  must make sense to a reader who never saw this plan. Doxygen comment on every public entity and every member
  (Doxygen fails on undocumented ones).
- **Do not run clang-format** on existing files (CONTRIBUTING.md: it rewrites 99 files and breaks `var<Q> * x`).
  Match the surrounding style by hand.
- **Catch2 splits test filters on commas**, and `-Filter` is a ctest regex over Catch2 test names. Prove a filter
  selected something (ctest prints the count) before trusting its result.
- **Commits.** Conventional subject, a body that explains why, and the last line exactly
  `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`. One commit per task. Never `--no-verify`, never
  amend or rewrite a commit another task made, never commit files outside your task. Every commit builds and passes
  on its own.
- **You are the only writer in the worktree** `D:\formula-cpp\.claude\worktrees\next-features`. Do not dispatch
  subagents. Do not touch any other worktree or `D:\formula-cpp` itself. End your turn after the task's report; do
  not start the next task.
- **CHANGELOG.md** gets its entry under `## [Unreleased]` (`### Added` at `CHANGELOG.md:9`, `### Changed` at
  `:178`) in the task that changes public behaviour.
- **Consume the foundation, never re-implement it** (`docs/superpowers/plans/2026-09-29-declared-precision.md`,
  "Contract adjustments"). From `detail/wide_int.hpp`: `WideUnsigned<L>` (`requires(L >= 2)`; `from_u64`,
  `from_limbs`, `limbs()`, `to_u64`, `is_zero`, `bit_length`, `limb` -- 0 past the top --, `==`, `<=>`),
  `add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none`, `mul_small_checked_or_none`, `add_small_checked_or_none`, `shift_left_checked_or_none`,
  `shift_right`, `divmod` (`WideDivision<L>`), `gcd`, `lcm_checked_or_none`, `pow10<L>`, `WideRatio<L>`, and
  `WideSigned<L> { bool negative; WideUnsigned<L> magnitude; }` with its own `add_checked_or_none`, `sub_checked_or_none` and
  `mul_checked_or_none` overloads (zero is never negative). From `detail/wide_rounding.hpp`: `reduced`,
  `wide_from_rational`, `scaled_to_denominator(Rational, WideUnsigned<L> const&) -> std::optional<WideSigned<L>>`,
  `round_wide_ratio`, `narrow_wide_ratio`, `rounded_in_unit` (the last three reduce first). From `opaque.hpp`:
  `rounded_output<"name", U, Places, Mode>(call)`, `OpaqueValues`, `StepKind::RoundedOpaqueOutput`,
  `detail::declares_compute_exact<Op, Inputs...>`, and the hook: `static constexpr std::size_t exact_limbs` (at
  least 4) and a `noexcept` `compute_exact` returning exactly `std::expected<std::array<detail::WideRatio<
  exact_limbs>, M>, ArithmeticError>`, called with `OpaqueArgumentTuple<Rational, Inputs...>` built by
  `opaque_held` and `opaque_arguments` -- so Task 2's observation overloads reach it. If a foundation name differs
  from this list when you start, use the foundation's and tell the controller.

---

## Findings the design depends on (read at `c30f261`)

1. **Where observations live and who reads them.** `binning.hpp` declares `ObservationsNodeBase` (`:62-68`, whose
   comment says "the only thing that reads it is `binned`", now false), `ObservationsNode` (`:70-72`),
   `ObservationsVarNode` (`:74-95`), `observations` (`:97-102`), `ObservationsValue` (`:104-121`),
   `EvaluatedObservations` (`:123-126`), `detail::RefusedObservations` (`:171-180`), `detail::HearsObservations`
   (`:254-260`) and `detail::evaluate_observations` (`:262-291`). They are read by `statistics.hpp:31,151-169`,
   `trace.hpp:14,3791`, `render.hpp:877,1557-1567,2154`, `document.hpp:518-521,985-1004`,
   `overlay.hpp:2133-2160`, `precision.hpp:603-661`, `calculation.hpp:364`, `record.hpp:1370`, each of which
   reaches them through `binning.hpp` today.
2. **`evaluate_observations` evaluates in any `Rep`** (`in_si<Rep>`, `binning.hpp:275`), writes zero past the
   count so a copy never reads an indeterminate `double` (`:280-284`), and fails at an observation with
   `SeriesFailure { error, at, FailureSite::InputObservation }` (`:277`). Observations are never absent: one not
   made is not listed (`environment.hpp:230-242`).
3. **The opaque plumbing** is a set of overloads per shape: `OpaqueInput` (`opaque.hpp:367-405`),
   `OpaqueArguments` (`:453-476`), `evaluate_opaque_input` (`:857-869`), `opaque_input_failure` (`:871-894`),
   `opaque_input_present` (`:896-920`), `opaque_held` (`:933-957`), `opaque_arguments` (`:959-978`). Lengths are
   compared only where non-zero (`opaque_lengths_agree`, `:421-435`), so a run-time length of 0 opts out.
   The input walk -- `evaluate_opaque_from` (`:1000-1022`) at `c30f261`, `detail::evaluate_opaque_inputs` once the
   declared-precision plan has landed -- passes every evaluated input on **by const reference** to the caller's
   local, so a span into it stays valid while `compute` runs.
4. **`OpaqueCallFailure` is initialised positionally with its trailing member left out** (`opaque.hpp:878, 885,
   893, 993` omit `notEvaluated`, which has `= 0`), and gcc-release builds with `-Wall -Wextra`
   (`cmake/PedanticCompiler.cmake:48`). A last member `site` with a default initialiser is therefore safe.
5. **The trace already has the field.** `Step::failureSite` exists (`trace.hpp:1324-1329`) and
   `failed_position_text` reads it (`trace_render.hpp:1680-1681`), but `opaque_produced` sets only
   `failedElement` (`trace.hpp:3994`) and `opaque_failure_suffix` always says `, at element`
   (`trace_render.hpp:2513-2516`). No new `Step` field is needed; `opaque_tests.cpp:712-722` pins 47 fields.
6. **Output units in the trace** are borrowed from the inputs' steps (`trace.hpp:2645-2695`): a unit with an
   offset (degrees Celsius) is never borrowed, so a coefficient per temperature over a Celsius regressor reads in
   the coherent unit, spelled `m/K`; a dimensionless output borrows nothing; a quotient such as `mm/s` is borrowed
   from `mm` and `s`.
7. **`detail::RefusedObservations` is not refused already.** It has no `refused` member, so
   `detail::refused_already` (`expression.hpp:157-169`) is false for it; `BinnedNode` decides its own refusal by
   `std::is_same_v` (`binning.hpp:207`). A refused fit that reads refused observations needs the flag.
8. **The calculation walk refuses each quantity read as observations once** (`calculation.hpp:189-209,
   362-372`). Measured for this plan on g++ 14.2 with a definition over a curve fit of two series (`Elapsed`,
   `Length`): two messages, one per quantity. The same walk reaches an opaque call's inputs through
   `LevelChildren<OpaqueOutputNode>` (`precision.hpp:669-673`), so a fit over two observations draws two.
9. **A census table is a fixed list.** `cmake/CheckCensusPage.cmake:69-72` collects `statistics`, `resolution`,
   `cylinder`, `least-squares`, and `:183-190` replaces them; a new table needs a `census_block` and a
   `replace_block` line and a `<!-- census:NAME -->` block in the page. The wide kernel does not report to the
   census tally (only `checked_int.hpp`, `Rational::make`, `rounding.hpp` and `rounded_root.hpp` do), so a
   regression row states where each route stops, not bits of headroom.
10. **No `MillimetrePerSecond` in `unit.hpp`** (`MillimetrePerMinute` is at `:399-403`): tests and the example
    declare the units they round in, as `GramSquared` is declared in the census (`overflow_census_tests.cpp:131`).
11. **The consumer-globals probe counts its checks** (`REQUIRE(probe.checks.size() == ...)`,
    `consumer_globals_run_tests.cpp:16`). Tasks 4 and 6 each add +1 probe check to whatever it reads then.

## The kernel width

Inputs are exact `Rational`s in coherent SI. The **realistic envelope**: at most 1024 observations; each value at
most 6 decimals and magnitude at most 10^6 in its declared unit; a coherent factor p/q with p, q at most 2^22
(`kWh` to J is 3.6 * 10^6, `h` to s 3600, `mg` to kg 10^-6, `mm/min` to m/s 1/60000; an offset such as 273.15 K
adds only a denominator 20 that 10^6 already covers).

| quantity | bound | why |
|---|---|---|
| a column's common denominator D | 10^6 q <= 2^42 | every element's denominator divides 10^6 q |
| a scaled element a = x D | 10^12 p <= 2^62 | x = k p / (10^6 q) with abs(k) <= 10^12 |
| totals S | 2^72 | n <= 2^10 |
| cross totals P | 2^134 | n a^2 |
| centred sums M, v, T | 2^144 (2^145 while formed) | n P - S S; abs(v) <= sqrt(M T) |
| K = 1: slope v D_x / (M D_y), intercept (M S_y - v S_x) / (n D_y M), R² v^2 / (M T) | 288 bits | R² dominates |
| rounding at declared places | + 82 bits | a unit factor up to 2^22, and 10^18 at 18 places |
| K >= 2: a K x K minor of M or [M, v] (Hadamard) | d(K) = 144 K + ceil((K/2) log2 K) | |
| fraction-free elimination and back-substitution | 2 d(K) + 4 bits | products of two minors, a sum of K of them |

| K | bits needed | limbs (bits) | measured on edge-of-envelope data, n = 1024 |
|---|---|---|---|
| 1 | 288 + 82 = 370 | **12** (384) | 205 |
| 2 | 2 * 289 + 4 = 582 | **19** (608) | 303 |
| 3 | 2 * 435 + 4 = 874 | **28** (896) | 508 |
| 4 | 2 * 580 + 4 = 1164 | **37** (1184) | 707 |
| 5 | 2 * 726 + 4 = 1456 | **46** (1472) | 908 |
| 6 | 2 * 872 + 4 = 1748 | **55** (1760) | 1112 |
| 7 | 2 * 1018 + 4 = 2040 | **64** (2048) | 1311 |
| 8 | 2 * 1164 + 4 = 2332 | **73** (2336) | 1519 |

For K >= 2 the elimination bound dominates the outputs' (d(K) + 150 + 82). The sums, M, v and T are formed in
**12 limbs for every K** (`regressionSumLimbs`) and widened for the solve, so the loop over the rows costs the
same whatever K is. "Measured" is the largest intermediate of a Python model of this kernel (below) on random data
at the envelope's edge (values +-10^6 at 6 decimals times factors 3.6*10^6, 1/1000, 3600, 1/60000, 10^-6): the
bound holds with room. Data outside the envelope -- many distinct denominators -- can outgrow it, and is
`Overflow`, pinned by the census.

**Where the routes stop (the model's prediction, to be measured by the census in Tasks 5 and 7):**

| data (invented), 2 to 128 points | exact route (`opaque_output`) overflows | rounded route (`rounded_output`) overflows |
|---|---|---|
| a line through readings at 3 dp near 2410 N | 99 of 127 sizes, first 29 | none |
| a line through readings at 4 dp near 2410 mm | 122 of 127, first 7 | none |
| a line through a different denominator on every point (stress) | 118 of 127, first 11 | 66 of 127, first 62 (the kernel alone: first 66) |
| two regressors: 3 dp readings and a 1 dp temperature in degrees Celsius (3 to 128) | 100 of 126, first 29 | none |

The exact route overflows early because **all outputs of one call answer or fail together** and R²'s reduced
fraction is about twice as wide as the slope's.

## Reference values

Every expected value below was computed with Python's `fractions` from the data stated beside it. The model and
the scripts are in the controller's scratch directory (`plan-regression/kernel.py`, `fixtures.py`, `fifty.py`,
`k2.py`, `celsius_line.py`, `census.py`, `stress.py`); `kernel.py` checks the model (integer sums, closed form,
fraction-free elimination with row exchanges, exact back-substitution) against a plain `Fraction` solve of the
centred normal equations on 300 random designs with K = 1 to 5. The reference, for re-running by hand:

```python
from fractions import Fraction as F
def ols(regressors, ys):   # exact least squares with a constant: [constant, c1..cK, R², n]
    K, n = len(regressors), len(ys)
    xm = [sum(c) / n for c in regressors]; ym = sum(ys) / n
    C = [[sum((a - xm[j]) * (b - xm[k]) for a, b in zip(regressors[j], regressors[k])) for k in range(K)] for j in range(K)]
    c = [sum((a - xm[j]) * (b - ym) for a, b in zip(regressors[j], ys)) for j in range(K)]
    A = [row[:] + [c[i]] for i, row in enumerate(C)]
    for s in range(K):
        r = next(r for r in range(s, K) if A[r][s] != 0)   # StopIteration: singular
        A[s], A[r] = A[r], A[s]
        for i in range(K):
            if i != s and A[i][s] != 0:
                f = A[i][s] / A[s][s]; A[i] = [a - f * b for a, b in zip(A[i], A[s])]
    beta = [A[i][K] / A[i][i] for i in range(K)]
    return [ym - sum(b * x for b, x in zip(beta, xm))] + beta + [sum(b * cc for b, cc in zip(beta, c)) / sum((y - ym) ** 2 for y in ys), F(n)]
```

| fixture (data in declared units; the fit sees coherent SI) | exact results |
|---|---|
| **line**: t = 1, 2, 4, 7 s; L = 10.2, 10.9, 12.1, 14.3 mm | intercept 19/2 mm (19/2000 m); slope 19/28 mm/s (19/28000 m/s, 285/7 mm/min); R² 1083/1085; points 4. Slope at 4 dp of mm/s, half-even: 3393/5000. R² at 4 dp: floor 9981/10000, half-even 4991/5000. In `double`: slope relative error 0, R² 1.1e-16 |
| **line, Celsius**: T = 11.3, 13.7, 17.9, 19.1 °C; L = 103.52, 104.13, 104.33, 105.1 mm | slope 33/200000 m/K (33/200 mm/K); intercept at 0 K 226571/4000000 m (226571/4000 mm); R² 107811/127460; response at 0 °C, intercept + slope * 273.15 K = 8137/80000 m (8137/80 mm), which is the intercept of the same fit on the Celsius numbers |
| **fifty**: t_k = k + 1 + (7919 k mod 997)/10^4 s, L_k = 2410 + 3.17 k + ((3217 k mod 1009) - 504)/10^4 mm, k = 0..49 (t_0 = 1, t_1 = 1047/500, L_0 = 3012437/1250 mm) | reduced exact outputs: slope 46/54 bits (fits a `Rational`), intercept 64-bit numerator and R² 92/92 bits (do not), so `opaque_output` is `Overflow` for every output; intercept at 4 dp of mm half-even 4813291/2000 (2406.6455); slope at 4 dp of mm/s half-even 31707/10000 (3.1707; floor would give 3.1706); R² at 6 dp floor 249999/250000 (0.999996; half-even would give 0.999997) |
| **two regressors**: T = 11.3, 13.7, 17.9, 19.1, 23.3, 29.7 °C; w = 2.3, 3.1, 2.9, 4.1, 3.7, 4.3 %; L = 103.52, 104.13, 104.33, 105.1, 105.21, 106 mm | constant (at 0 K) 22365154943/276592800000 m (22365154943/276592800 mm); coefficient 1 346407/4609880000 m/K; coefficient 2 3842851/69148200 m per unit content (19214255/345741 mm; in mm/%, a unit of 1/10 m, 3842851/6914820); R² 27398849648/27403085919; points 6; response at 0 °C 14021209633/138296400000 m. Rounded: coefficient 1 at 4 dp of mm/K half-even 751/10000; coefficient 2 at 4 dp of mm/% half-even 5557/10000; R² at 4 dp floor 4999/5000; response at 0 °C at 2 dp of mm half-even 10139/100. In `double`: largest relative error 5.5e-15 |
| **three regressors** (kernel level, integers): x1 = 3, 7, 2, 9, 4, 8, 5; x2 = 11, 13, 17, 19, 23, 29, 31; x3 = 2, 1, 4, 3, 6, 5, 7; y = 41, 57, 49, 71, 66, 83, 79 | constant 1222381/72160; c1 14161/4510; c2 84499/72160; c3 52383/36080; R² 362845727/364047200; points 7 |
| **K + 1 rows**: (x1, x2, y) = (1, 2, 5), (2, 1, 7), (4, 3, 2) | constant 39/4; c1 -1/4; c2 -9/4; R² 1; points 3 |
| **singular**: x1 = 1, 2, 4, 7; y = 10.2, 10.9, 12.1, 14.3 | x2 = 2 x1, x2 = x1 + 273.15, x2 = 3 x1 - 7: each singular exactly; in `double` the pivot ratio D_2 / S_22 is exactly 0 for all three |
| **near-collinear**: x1 = 1..8; e = +1, -1, -1, +1, +1, -1, -1, +1 (orthogonal to x1, sum 0); x2 = x1 + e/delta; y = 3 + 2 x1 - 5 x2 | exact: constant 3, c1 2, c2 -5, R² 1, points 8 for every delta. 1 - R² of x2 on x1 = 8/(42 delta^2 + 8): delta = 500 gives 1/1312501 (7.6e-7), `double` answers, c1 relative error 5.6e-10; delta = 10000 gives 1/525000001 (1.9e-9), `double` answers, relative error 1.8e-7; delta = 20000 gives 1/2100000001 (4.8e-10), `double` refuses by the tolerance |
| **stress**: point k at ((k+1)/(k+2), (2k+3)/(k+3)) | K = 1 kernel: 15 points fit (largest intermediate 89 bits); 128 points need 744 bits, `Overflow` at 12 limbs |

## Review Focus

Inputs and failure modes the spec implies that a task could miss, each with the test that owns it:

1. **Zero observations made.** `compute` receives an empty span, never the capacity; a fit over none, or one, is
   its own `DomainError`, never a division by a count of zero. Owned by Task 2 ("an opaque operation over
   observations sees how many were made, none included") and Task 4 ("a line through observations whose counts
   differ, or too few, or flat, is the fit's own domain error").
2. **Capacities that differ between inputs, and counts that differ at run time.** Different capacities are
   allowed (no framework length rule); different counts are the fit's own `DomainError` with origin `Own`, never a
   read past the shorter. Owned by Task 2 (capacities 8 and 4 in one call) and Task 4 ("observations counted at run
   time, of different capacities, fit as written ones do"; the count mismatch in the domain-error test).
3. **A conversion failure inside an observation.** It is `Propagated`, carries the observation's zero-based
   position with `site == FailureSite::InputObservation`, and the call's line says
   `[carried up from #k, at observation 3]`, never `at element 3`. Owned by Task 2 (test-local operation) and Task 4
   (through the fit).
4. **Near-collinear but valid designs, and the tolerance's edge.** The exact kernel answers every non-singular
   design exactly; `double` answers above 10^-9 and refuses below it, so a design between the two answers exactly
   and is refused approximately. Owned by Task 3 ("the kernel answers a nearly collinear design exactly, and in
   double down to the stated tolerance").
5. **A regressor in an offset unit (degrees Celsius).** The fit sees kelvin: each coefficient is per kelvin, which
   is per degree Celsius, but the intercept or constant is the response at 0 K; the response at 0 °C is a formula
   over two outputs. A regressor `x2 = x1 + 273.15` is singular. Owned by Task 3 (the singular designs), Task 4 ("a
   line through observations against a temperature in degrees Celsius has its intercept at 0 K") and Task 6 ("a regressor in degrees
   Celsius ...").
6. **Observations in a percent unit.** The fit sees the coherent unit (a fraction), so a coefficient over a percent
   regressor is per unit, a hundred times the per-percent figure; `rounded_output` in a unit of mm per % reports it
   per percent. Owned by Task 6 ("a regressor in percent ...").
7. **A flat response and an R² acceptance.** Flat values fail the whole fit, so an R² constraint is `Invalid` with
   `DomainError`, never satisfied. Owned by Task 4 ("a flat response fails an R^2 acceptance of a line through observations, never
   passes it").

---

## Task 1: Raw observations in a header of their own (pure refactor)

**Files:**
- Create: `include/formula-cpp/observations.hpp`, `test/observations_tests.cpp`
- Modify: `include/formula-cpp/binning.hpp` (remove `:62-126`, `:171-180`, `:252-292`; include the new header;
  file comment `:4-35`), `include/formula-cpp/formula.hpp:14-56` (umbrella), root `CMakeLists.txt:41-100`
  (`FILE_SET`), `test/consumer_globals_tests.cpp:148-199` (includes), `test/CMakeLists.txt:22-99` (sources),
  `include/formula-cpp/statistics.hpp:31,61-63` (include and comment), `include/formula-cpp/render.hpp:877`
  (comment), `include/formula-cpp/trace.hpp:272` (comment), `include/formula-cpp/environment.hpp:24-29`
  (comment), `CHANGELOG.md`

**Interfaces:**
- Consumes: nothing new.
- Produces (unchanged spellings, now declared in `observations.hpp`): `struct ObservationsNodeBase`;
  `template <typename T> concept ObservationsNode`; `template <Described Q, std::size_t Capacity> struct
  ObservationsVarNode`; `template <Described Q, std::size_t Capacity> inline constexpr ObservationsVarNode<Q,
  Capacity> observations`; `template <typename Rep, std::size_t Capacity> struct ObservationsValue`;
  `template <typename Rep, std::size_t Capacity> using EvaluatedObservations`; in `detail`:
  `struct RefusedObservations`, `concept HearsObservations<Sink, O, Rep>`,
  `template <typename Rep, Described Q, std::size_t Capacity, typename Env, typename Sink> constexpr
  EvaluatedObservations<Rep, Capacity> evaluate_observations(ObservationsVarNode<Q, Capacity> const&, Env const&,
  Sink&) noexcept`.

`detail::RefusedObservations` moves too although the spec's list omits it: the refused observation fit (Task 4)
needs it without `binning.hpp`, and it is an observations stand-in, not a binning's.

- [ ] **Step 1: A failing test that includes only the new header.** Create `test/observations_tests.cpp` and add
  it to `formula-cpp-tests` after `binning_tests.cpp` in `test/CMakeLists.txt`:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// Raw observations are read through their own header, without a binning's
// classes, lookups and bands: this translation unit includes nothing else of
// the library's.
#include <formula-cpp/observations.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace
{
struct Size: formula::Quantity<Size, "d_o", "an invented particle size", formula::unit::Millimetre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Three made in room for eight: 103, 127 and 163 mm.
constexpr auto threeMade =
    formula::environment(formula::MeasuredObservations<Size, 8>(rat(103), rat(127), rat(163)));

constexpr auto read_three()
{
    formula::NullSink quiet {};
    return formula::detail::evaluate_observations<formula::Rational>(formula::observations<Size, 8>, threeMade, quiet);
}
} // namespace

TEST_CASE("raw observations are read through their own header, as many as were made", "[observations]")
{
    // In the coherent unit: 103 mm is 103/1000 m. The count is the three made,
    // not the capacity of eight, and every place past it holds zero.
    constexpr auto made = read_three();
    STATIC_REQUIRE(made.has_value());
    STATIC_REQUIRE(made->count == 3);
    STATIC_REQUIRE(made->elements[0] == rat(103, 1000));
    STATIC_REQUIRE(made->elements[2] == rat(163, 1000));
    STATIC_REQUIRE(made->elements[3] == rat(0));
    STATIC_REQUIRE(formula::ObservationsNode<decltype(formula::observations<Size, 8>)>);
    STATIC_REQUIRE(decltype(formula::observations<Size, 8>)::capacity == 8);
}
```

- [ ] **Step 2: Run and fail.** `pwsh -NoProfile -File ...\cl.ps1 -Filter "raw observations are read"`.
  Expected: `BUILD FAILED`, `observations_tests.cpp(...): fatal error C1083: Cannot open include file:
  'formula-cpp/observations.hpp'`.
- [ ] **Step 3: Move the declarations.** Create `include/formula-cpp/observations.hpp` with the SPDX line,
  `#pragma once`, and this file comment:

```cpp
/// @file
/// Raw observations: as many values of one quantity as were observed, read
/// with `observations<Q, Capacity>` from a `MeasuredObservations<Q, Capacity>`
/// in the environment (`environment.hpp`). How many there are is data; only
/// the most there can be is part of the type.
///
/// Read by a binning (`binning.hpp`), which counts them into classes, and by
/// the sample statistics (`statistics.hpp`), which summarise them. A header of
/// their own, so that a reader of observations need not include the classes,
/// lookups and bands a binning brings.
```

  Includes: `dimension.hpp`, `environment.hpp`, `error.hpp`, `evaluate.hpp` (`in_si`, `Evaluated`),
  `measured.hpp`, `quantity.hpp`, `rational.hpp`, `series.hpp` (`SeriesFailure`, `FailureSite`,
  `detail::RefusedFlag` through `expression.hpp`), `sink.hpp`; `<array>`, `<concepts>`, `<cstddef>`, `<expected>`,
  `<type_traits>`. Move the eight declarations and `detail::RefusedObservations` **verbatim**, in their order,
  except `ObservationsNodeBase`'s comment, whose last sentence becomes: "Neither a `Node`, which promises one value,
  nor a `SeriesNode`, which promises one at each point of a domain." In `binning.hpp`, replace them with
  `#include <formula-cpp/observations.hpp>` (keep its other includes: `band.hpp`, `lookup.hpp`, ...), and add one
  sentence to its file comment after the first paragraph: "The observations themselves are declared in
  `observations.hpp`, which this header includes."
- [ ] **Step 4: Point every reader at the right header.** Add `#include <formula-cpp/observations.hpp>` to the
  umbrella `formula.hpp` (alphabetical, after `number_text.hpp`), to `test/consumer_globals_tests.cpp`'s include
  list (after `number_text.hpp`), and the path
  `"${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/observations.hpp"` to the root `FILE_SET` (after
  `number_text.hpp`). `statistics.hpp:31` includes `observations.hpp` in place of `binning.hpp` (it uses no
  binning), and its comment at `:61-63` says `(`ObservationsVarNode`, `observations.hpp`)`. The comments
  `render.hpp:877` and `trace.hpp:272` name `observations.hpp` where they now name `binning.hpp` for the
  observations. Leave every other `#include <formula-cpp/binning.hpp>` alone: those headers use the binning too.
- [ ] **Step 5: Run and pass on both.** `cl.ps1` unfiltered, then `gcc14.sh` unfiltered. Expected: `ALL OK` on
  both, +1 case over the baseline; `hygiene.installed-headers`, `hygiene.consumer-globals` and
  `hygiene.headers` among the passes.
- [ ] **Step 6: CHANGELOG** under `### Added`:

```markdown
- `observations.hpp`, holding raw observations -- `observations<Q, Capacity>`, `ObservationsVarNode`,
  `ObservationsNode`, `ObservationsValue` and `EvaluatedObservations` -- which `binning.hpp` declared before and
  still includes. Code that reads observations no longer needs a binning's classes, lookups and bands.
```

- [ ] **Step 7: Commit.**

```text
refactor(observations): raw observations in a header of their own

Observations were declared in binning.hpp, whose comment said a binning
was the only reader. The sample statistics read them too, and an opaque
operation is about to: including a binning's classes, lookups and bands
for that is weight nobody needs. The declarations move unchanged, with
the stand-in for refused observations, and binning.hpp includes the new
header, so every existing include keeps compiling.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 2: Raw observations as an opaque operation's input

**Files:**
- Modify: `include/formula-cpp/opaque.hpp` (include; file comment `:24-31`; `InputShape` `:74-86`;
  `OpaqueCallFailure` `:110-128`; `input_dimension_count` doc `:154-155`; concept doc `:209-224`;
  `RequireOpaqueOutputDimensionsDeclared` message `:282-287`; `OpaqueInput` `:398-405`; `OpaqueArguments`
  `:467-476`; `RequireOpaqueComputeCallable` message `:555-559`; `evaluate_opaque_input` `:857-869`;
  `opaque_input_failure` `:871-894`; `opaque_input_present` `:896-920`; `opaque_held` / `opaque_arguments`
  `:933-978`), `include/formula-cpp/trace.hpp` (`Step::failureSite` doc `:1324-1329`; `StepKind::OpaqueOperation`
  doc `:432-434`; `opaque_produced` `:3992-3996`), `include/formula-cpp/trace_render.hpp`
  (`opaque_failure_suffix` `:2497-2522`), `include/formula-cpp/observations.hpp` (comment),
  `test/opaque_tests.cpp`, `test/negative/opaque_observations_where_series_declared.cpp`, `test/CMakeLists.txt`
  (after `:2187`), `CHANGELOG.md`

**Interfaces:**
- Consumes: Task 1's `ObservationsNode`, `ObservationsValue<Rep, C>`, `EvaluatedObservations<Rep, C>`,
  `detail::evaluate_observations`.
- Produces:
  - `enum class InputShape : std::uint8_t { Single, Series, Curve, Observations };` (appended)
  - `struct OpaqueCallFailure { ArithmeticError error; OpaqueFailure origin; std::optional<std::size_t> element;
    std::size_t notEvaluated = 0; FailureSite site = FailureSite::ResultElement; };`
  - in `detail`: `template <ObservationsNode T> struct OpaqueInput<T>`; `template <typename Rep, ObservationsNode
    T> struct OpaqueArguments<Rep, T> { using type = std::tuple<std::span<Rep const>>; };`;
    `template <typename Rep> struct OpaqueHeldObservations { std::span<Rep const> made; };`; overloads of
    `opaque_input_failure(EvaluatedObservations<Rep, C> const&)`, `opaque_input_present(ObservationsValue<Rep, C>
    const&)`, `opaque_held(ObservationsValue<Rep, C> const&) -> OpaqueHeldObservations<Rep>`,
    `opaque_arguments<Rep>(OpaqueHeldObservations<Rep> const&) -> std::tuple<std::span<Rep const>>`.
  - The trace: an opaque call relaying an observation's failure records `Step::failureSite ==
    FailureSite::InputObservation`, and its line ends `[carried up from #k, at observation m]`.

The foundation's `rounded_output` builds `compute_exact`'s arguments with `opaque_held` and
`opaque_arguments<Rational>`, and detects the hook over `OpaqueArgumentTuple<Rational, Inputs...>`
(declared-precision plan, `declares_compute_exact` and its exact evaluation), so the overloads here reach it with
no more work; Task 4 tests it.

- [ ] **Step 1: Failing tests.** Append to `test/opaque_tests.cpp` (it includes `document.hpp`, `precision.hpp`,
  `render.hpp`, `trace.hpp`, `trace_render.hpp`; add `#include <formula-cpp/overlay.hpp>` and
  `#include <formula-cpp/method.hpp>` if they are not there by now):

```cpp
namespace
{
// A consumer's operation over two samples of raw observations and one value:
// how many observations each held, and the lowest of the first raised by the
// value. Two samples need not be the same size; the library compares no counts.
struct TwoSampleLowest
{
    static constexpr std::string_view name = "two sample lowest";
    static constexpr std::array shapes { formula::InputShape::Observations,
                                         formula::InputShape::Observations,
                                         formula::InputShape::Single };
    static constexpr std::array<std::string_view, 3> outputs { "first made", "second made", "raised lowest" };

    static consteval std::optional<std::array<formula::Dimension, 3>> output_dimensions(
        std::array<formula::Dimension, 3> declared) noexcept
    {
        if (!(declared[0] == declared[2]))
            return std::nullopt;
        return std::array { formula::dim::Scalar, formula::dim::Scalar, declared[0] };
    }

    static inline int calls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 3>, formula::ArithmeticError> compute(
        std::span<Rep const> firstSample, std::span<Rep const> secondSample, Rep raisedBy) noexcept
    {
        if !consteval
        {
            ++calls;
        }
        if (firstSample.empty())
            return std::unexpected { formula::ArithmeticError::DomainError };
        std::expected<Rep, formula::ArithmeticError> const firstMade =
            formula::RepTraits<Rep>::from(formula::Rational { static_cast<std::int64_t>(firstSample.size()) });
        std::expected<Rep, formula::ArithmeticError> const secondMade =
            formula::RepTraits<Rep>::from(formula::Rational { static_cast<std::int64_t>(secondSample.size()) });
        if (!firstMade.has_value() || !secondMade.has_value())
            return std::unexpected { formula::ArithmeticError::Overflow };
        Rep least = firstSample[0];
        for (Rep const& each: firstSample)
            if (each < least)
                least = each;
        std::expected<Rep, formula::ArithmeticError> const raised = formula::RepTraits<Rep>::add(least, raisedBy);
        if (!raised.has_value())
            return std::unexpected { raised.error() };
        return std::array { *firstMade, *secondMade, *raised };
    }
};

// One sample: its lowest observation.
struct LowestObserved
{
    static constexpr std::string_view name = "lowest observed";
    static constexpr std::array shapes { formula::InputShape::Observations };
    static constexpr std::array<std::string_view, 1> outputs { "lowest" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> sample) noexcept
    {
        if (sample.empty())
            return std::unexpected { formula::ArithmeticError::DomainError };
        Rep least = sample[0];
        for (Rep const& each: sample)
            if (each < least)
                least = each;
        return std::array { least };
    }
};

struct Tare: formula::Quantity<Tare, "r_t", "an invented tare reading", unit::Gram>
{
};
// Distances in kilometres, so that reading one into metres can overflow.
struct FarReading: formula::Quantity<FarReading, "x_k", "an invented distance, in kilometres", unit::Kilometre>
{
};

constexpr auto twoSamples = formula::opaque<TwoSampleLowest>(
    { .title = "Two samples", .reference = "Example Standard 12", .section = "4.4" },
    formula::observations<Reading, 8>,
    formula::observations<Tare, 4>,
    formula::var<Shift>);

// Three readings made in room for eight, no tare made in room for four, a
// shift of 13 g: the lowest reading, 103 g, raised to 116 g.
constexpr auto threeAndNone = formula::environment(formula::MeasuredObservations<Reading, 8>(rat(127), rat(103), rat(191)),
                                                   formula::MeasuredObservations<Tare, 4>(),
                                                   formula::Measured<Shift> { rat(13) });
} // namespace

TEST_CASE("an opaque operation over observations sees how many were made, none included", "[opaque][observations]")
{
    // compute sees 3 and 0, never the capacities 8 and 4: a span over the
    // observations made. Capacities that differ are no framework error.
    constexpr auto firstMade =
        formula::checked_evaluate_si(formula::opaque_output<"first made">(twoSamples), threeAndNone);
    STATIC_REQUIRE(**firstMade == rat(3));
    constexpr auto secondMade =
        formula::checked_evaluate_si(formula::opaque_output<"second made">(twoSamples), threeAndNone);
    STATIC_REQUIRE(**secondMade == rat(0));
    // 116 g is 29/250 kg in the coherent unit.
    constexpr auto raised =
        formula::checked_evaluate_si(formula::opaque_output<"raised lowest">(twoSamples), threeAndNone);
    STATIC_REQUIRE(**raised == rat(29, 250));
    STATIC_REQUIRE(decltype(formula::opaque_output<"raised lowest">(twoSamples))::dimension == formula::dim::Mass);
    STATIC_REQUIRE(decltype(formula::opaque_output<"first made">(twoSamples))::dimension == formula::dim::Scalar);
}

TEST_CASE("observations read at run time, and in double, reach an opaque operation", "[opaque][observations]")
{
    // Five readings known only at run time, through MeasuredObservations::from.
    std::array<formula::Rational, 5> const fiveReadings { rat(139), rat(113), rat(197), rat(163), rat(127) };
    auto const fromRunTime = formula::MeasuredObservations<Reading, 8>::from(fiveReadings);
    REQUIRE(fromRunTime.has_value());
    auto const fiveAndNone = formula::environment(*fromRunTime,
                                                  formula::MeasuredObservations<Tare, 4>(),
                                                  formula::Measured<Shift> { rat(13) });
    TwoSampleLowest::calls = 0;
    auto const firstMade = formula::checked_evaluate_si(formula::opaque_output<"first made">(twoSamples), fiveAndNone);
    REQUIRE(firstMade.has_value());
    CHECK(**firstMade == rat(5));
    CHECK(TwoSampleLowest::calls == 1);
    // In double: 113 g + 13 g = 0.126 kg.
    auto const inDouble =
        formula::checked_evaluate_si<double>(formula::opaque_output<"raised lowest">(twoSamples), fiveAndNone);
    REQUIRE(inDouble.has_value());
    REQUIRE(inDouble->has_value());
    CHECK(std::abs(**inDouble - 0.126) < 1e-12);
}

TEST_CASE("an opaque call over observations is traced, rendered and documented with them", "[opaque][observations][trace]")
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"raised lowest">(twoSamples), threeAndNone, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. r = 127 g; 103 g; 191 g\n"
             "2. r_t = (no elements)\n"
             "3. r_0 = 13 g\n"
             "4. two sample lowest(#1, #2, #3) = first made = 3; second made = 0; raised lowest = 116 g "
             "[inside not shown] [Two samples, Example Standard 12, 4.4]\n"
             "5. raised lowest of #4 = 116 g\n");

    constexpr auto raisedLowest = formula::opaque_output<"raised lowest">(twoSamples);
    CHECK(formula::render(raisedLowest) == "two sample lowest(r(i), r_t(i), r_0).raised lowest");
    // Markdown and LaTeX: the observations carry the series marker, as in the
    // plain text; how a symbol with an underscore is set is the vocabulary's
    // business, pinned elsewhere, so only the call's frame is pinned here.
    std::string const markdown = formula::render<formula::Dialect::Markdown>(raisedLowest);
    CHECK(markdown.starts_with("two sample lowest(`r(i)`, "));
    CHECK(markdown.ends_with(").raised lowest"));
    std::string const latex = formula::render<formula::Dialect::LaTeX>(raisedLowest);
    CHECK(latex.starts_with("\\text{two sample lowest}({r}_{i}, "));
    CHECK(latex.ends_with(")_{\\text{raised lowest}}"));

    formula::Documentation const page = formula::document(raisedLowest);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "two sample lowest");
    REQUIRE(page.symbols.size() == 3);
    CHECK(page.symbols[0].shape == formula::ValueShape::Observations);
    CHECK(page.symbols[0].length == 8);
    CHECK(page.symbols[1].shape == formula::ValueShape::Observations);
    CHECK(page.symbols[1].length == 4);
    CHECK(page.symbols[2].shape == formula::ValueShape::Single);
}

TEST_CASE("an overlay and a level walk reach inside a call over observations", "[opaque][observations][overlay]")
{
    // The shift, read inside the call beside two samples, fixed at 29 g:
    // 103 g + 29 g = 132 g = 33/250 kg. The observations are left as they are.
    constexpr auto raisedMethod = formula::method(
        formula::variants(formula::variant<ShiftedVariant>(formula::opaque_output<"raised lowest">(twoSamples))),
        formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(),
        formula::constraints());
    constexpr auto fixed = formula::apply(
        formula::overlay(formula::with_constant<Shift>(rat(29), { .reference = "Example Standard 12:2021 NA" })),
        raisedMethod);
    constexpr auto noShift = formula::environment(formula::MeasuredObservations<Reading, 8>(rat(127), rat(103), rat(191)),
                                                  formula::MeasuredObservations<Tare, 4>());
    STATIC_REQUIRE(formula::evaluate_method<ShiftedVariant>(fixed, noShift)->value() == rat(33, 250));
    STATIC_REQUIRE(formula::detail::ConstantRewriteOf<
                   formula::ConstantOverride<Shift>,
                   std::remove_cvref_t<decltype(formula::opaque_output<"raised lowest">(twoSamples))>>::known);
    STATIC_REQUIRE(formula::detail::LevelChildren<
                   std::remove_cvref_t<decltype(formula::opaque_output<"raised lowest">(twoSamples))>>::seen);
}

TEST_CASE("an observation that fails to convert fails the call at that observation", "[opaque][observations][trace]")
{
    // 1.03 x 10^17 km is 1.03 x 10^20 m, past Rational's range: the third
    // observation. Relayed, not the operation's own, and counted as an
    // observation -- never "at element 3".
    constexpr auto lowestFar = formula::opaque<LowestObserved>({ .reference = "Example Standard 12" },
                                                               formula::observations<FarReading, 4>);
    constexpr auto overflowing = formula::environment(
        formula::MeasuredObservations<FarReading, 4>(rat(103), rat(127), rat(103'000'000'000'000'000)));
    constexpr auto called = formula::detail::evaluate_call<formula::Rational>(lowestFar, overflowing, formula::NullSink {});
    STATIC_REQUIRE(!called.has_value());
    STATIC_REQUIRE(called.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(called.error().origin == formula::OpaqueFailure::Propagated);
    STATIC_REQUIRE(called.error().element == std::optional<std::size_t> { 2 });
    STATIC_REQUIRE(called.error().site == formula::FailureSite::InputObservation);

    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"lowest">(lowestFar), overflowing, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. x_k = overflow in exact arithmetic at observation 3\n"
             "2. lowest observed(#1) = overflow in exact arithmetic [inside not shown] "
             "[carried up from #1, at observation 3] [Example Standard 12]\n"
             "3. lowest of #2 = overflow in exact arithmetic\n");
    // A series input's failure still reads "at element" (the relayed test above).
}
```

  Also declare, in the same anonymous namespace, `struct ShiftedVariant {};` (a variant tag, as `Fitted` is in
  `least_squares_tests.cpp:273-275`). The existing aggregate initialisations of `OpaqueCallFailure` in this file
  and in `trace_tests.cpp`, if any, stay as they are: `site` defaults.

- [ ] **Step 2: Run and fail.** `cl.ps1 -Filter "observation"`. Expected: `BUILD FAILED` --
  `std::array shapes { ..., formula::InputShape::Observations, ... }` names no enumerator (C2039 "'Observations':
  is not a member of 'formula::InputShape'") and `called.error().site` names no member.
- [ ] **Step 3: Implement in `opaque.hpp`.**
  - `#include <formula-cpp/observations.hpp>` (alphabetical, after `expression.hpp`).
  - Append to `InputShape`, after `Curve`:

```cpp
    /// Raw observations: any `ObservationsNode` (`observations.hpp`). They
    /// arrive as one `std::span<Rep const>` over the observations **made** --
    /// as many as were made, not the capacity, and empty when none were --
    /// pointing into the evaluated value; nothing is copied. They contribute
    /// one dimension. How many there are is data, so the library compares no
    /// counts: an operation over two independent samples takes two counts,
    /// and one that pairs its inputs row by row checks its counts itself.
    /// Evaluated in any `Rep`.
    Observations,
```

  - Append to `OpaqueCallFailure`, after `notEvaluated`:

```cpp
    /// What `element` counts: an element of the input that failed, or --
    /// for raw observations whose conversion into the coherent unit failed --
    /// an observation (`FailureSite::InputObservation`), which the trace
    /// names `at observation k`. Last, and defaulted, so that an aggregate
    /// initialisation naming the members before it stays valid.
    FailureSite site = FailureSite::ResultElement;
```

  - The file comment's input sentence (`:26-29`) reads: "a single value arrives as `Rep`, a series as
    `std::span<Rep const>`, raw observations as one `std::span<Rep const>` over those made, and a curve as two
    spans, its points first and then its values." The concept's doc (`:221-224`) says the same. Absence (`:32-35`)
    gains: "Raw observations are never absent: none made is an empty span."
  - `input_dimension_count`'s doc (`:154-155`): "one per single value, series or observations, two per curve"
    (its body already counts only a curve twice).
  - `RequireOpaqueOutputDimensionsDeclared`'s message: `"... -- one Dimension per single value, series or
    observations input and two per curve, its points then its values -- ..."`;
    `RequireOpaqueComputeCallable`'s: `"... a single value arrives as Rep, a series or raw observations as
    std::span<Rep const>, a curve as two spans, points first, ..."`. Both keep their first sentence, which is what
    `opaque_curve_dimensions_declared` and `opaque_compute_signature` match.
  - After the curve's `OpaqueInput` (`:398-405`):

```cpp
    template <ObservationsNode T>
    struct OpaqueInput<T>
    {
        static constexpr bool known = true;
        static constexpr InputShape shape = InputShape::Observations;
        /// Known only at run time: 0, which `opaque_lengths_agree` skips.
        static constexpr std::size_t length = 0;
        static constexpr std::array<Dimension, 1> dimensions { T::dimension };
    };
```

  - After the curve's `OpaqueArguments` (`:467-471`):

```cpp
    template <typename Rep, ObservationsNode T>
    struct OpaqueArguments<Rep, T>
    {
        using type = std::tuple<std::span<Rep const>>;
    };
```

  - `evaluate_opaque_input` (the per-input dispatch; the declared-precision plan replaces the walk around it,
    `evaluate_opaque_from`, with `evaluate_opaque_inputs`, and keeps building failures with `opaque_input_failure`):
    a branch before the single value's, and its doc names it:

```cpp
        else if constexpr (ObservationsNode<Input>)
            return evaluate_observations<Rep>(input, environment, sink);
```

  - After the curve's `opaque_input_failure`:

```cpp
    /// A failed observations input's failure: its error, and the observation
    /// it arose at, counted as an observation.
    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr OpaqueCallFailure opaque_input_failure(EvaluatedObservations<Rep, Capacity> const& failed) noexcept
    {
        return OpaqueCallFailure { failed.error().error, OpaqueFailure::Propagated, failed.error().element, 0,
                                   failed.error().site };
    }
```

  - After the curve's `opaque_input_present`:

```cpp
    /// Raw observations are never absent: none made is an empty span.
    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr bool opaque_input_present(ObservationsValue<Rep, Capacity> const&) noexcept
    {
        return true;
    }
```

  - After the curve's `opaque_held`, and after the curve's `opaque_arguments`:

```cpp
    /// The observations made, as `compute` reads them: a view into the
    /// evaluated value, which the input walk (`evaluate_opaque_inputs`) holds
    /// for as long as `compute` runs. Nothing is copied.
    template <typename Rep>
    struct OpaqueHeldObservations
    {
        /// The observations made, in the order made.
        std::span<Rep const> made;
    };

    template <typename Rep, std::size_t Capacity>
    [[nodiscard]] constexpr OpaqueHeldObservations<Rep> opaque_held(
        ObservationsValue<Rep, Capacity> const& evaluatedObservations) noexcept
    {
        return OpaqueHeldObservations<Rep> {
            std::span<Rep const> { evaluatedObservations.elements.data(), evaluatedObservations.count }
        };
    }
```

```cpp
    template <typename Rep>
    [[nodiscard]] constexpr std::tuple<std::span<Rep const>> opaque_arguments(OpaqueHeldObservations<Rep> const& held) noexcept
    {
        return std::tuple<std::span<Rep const>> { held.made };
    }
```

- [ ] **Step 4: The trace.** In `trace.hpp`'s `opaque_produced`, after `callStep.failedElement =
  result.error().element;` (`:3994`), add `callStep.failureSite = result.error().site;`. `Step::failureSite`'s doc
  (`:1324-1329`) adds "and, for an opaque call relaying the failure of raw observations, the observation it arose
  at"; `StepKind::OpaqueOperation`'s (`:432-434`) "on a relayed failure the input's element in
  `Step::failedElement`, and what it counts in `Step::failureSite`". In `trace_render.hpp`'s
  `opaque_failure_suffix`, replace the element clause:

```cpp
                if (recorded.failedElement.has_value())
                    relayed += (recorded.failureSite == FailureSite::InputObservation ? ", at observation "
                                                                                      : ", at element ")
                               + std::to_string(*recorded.failedElement + 1);
```

  and its doc: "with its element -- or its observation, for raw observations -- counted from one". The
  declared-precision plan's `rounded_opaque_output_line` calls this function with the rounded output's own step,
  whose `failedElement` is never set: it keeps reading `[carried up from #k]` with no position, since the new clause
  is inside the `failedElement` test. The call's own line carries the observation (Task 4 tests both).
  `observations.hpp`'s file comment gains: "and, as an input of an opaque operation (`opaque.hpp`), by a fit."
- [ ] **Step 5: Run and pass.** `cl.ps1 -Filter "observation|opaque"` (+5 cases; prove it selected them and
  the existing opaque ones), then both builds unfiltered. Expected `ALL OK` on both. The field-count pin
  (`opaque_tests.cpp:712-722`) still reads 47.
- [ ] **Step 6: Mutation check (defect class 7).** Revert Step 4's `trace_render.hpp` change alone: the
  at-observation test must fail on its `render_trace` line and nothing else. Revert `opaque_input_failure`'s `site`
  argument alone: the `STATIC_REQUIRE` on `site` must fail. Restore both with a plain write.
- [ ] **Step 7: Negative test.** `test/negative/opaque_observations_where_series_declared.cpp`: `SeriesSpan`
  (the shape of `opaque_tests.cpp:55-94`, without `calls`) given `formula::observations<Reading, 8>`;
  `int main() { return spanOf.refused ? 0 : 1; }` over `formula::opaque_output<"span">(...)`. Header comment:
  "Raw observations passed where an operation declares a series: refused once, as any other shape is, and the
  dimensions and compute are not asked about." Register after `opaque_output_as_series` (`test/CMakeLists.txt:2187`):

```cmake
formula_add_negative_test(opaque_observations_where_series_declared
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "does not accept inputs of these dimensions" "compute cannot be called")
```

  +1 negative test. Wrong string first, then right; deletion check: delete `RequireOpaqueShapes`'s `static_assert`
  (`opaque.hpp:519-522`) and confirm the case compiles.
- [ ] **Step 8: CHANGELOG.** `### Added`:

```markdown
- `InputShape::Observations`: an opaque operation may take raw observations, `observations<Q, Capacity>`, as an
  input. `compute` receives one `std::span<Rep const>` over the observations made -- as many as were made, not
  the capacity, and empty when none were -- in any `Rep`, `double` included. The library compares no counts: an
  operation over two independent samples takes two counts, and one that pairs its inputs row by row checks its
  counts itself.
```

  `### Changed`:

```markdown
- `OpaqueCallFailure` has a last member, `site` (`FailureSite`, default `FailureSite::ResultElement`), so an
  aggregate initialisation naming the members before it is unchanged. Raw observations that fail to convert at
  observation k relay `site == FailureSite::InputObservation`, and the call's trace line says
  `[carried up from #1, at observation k]`. `InputShape` has a fourth enumerator, so a consumer's exhaustive
  `switch` over it warns.
```

- [ ] **Step 9: Commit.**

```text
feat(opaque): raw observations as an opaque operation's input

A method fits "every determination that meets the condition": how many
there are is data. Observations modelled that for statistics but could
not reach an opaque operation. They now arrive as one span over the
observations made, in any representation, and their counts are not a
framework rule, since two independent samples differ in size.

An observation that fails to convert is relayed with its position,
marked as an observation, so the trace no longer calls it an element.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 3: The regression kernel, exact in wide integers and approximate in `double`

**Files:**
- Create: `include/formula-cpp/detail/least_squares_kernel.hpp`, `test/least_squares_kernel_tests.cpp`
- Modify: root `CMakeLists.txt:43-50` (`FILE_SET`, beside the other `detail/` headers), `test/CMakeLists.txt`
  (sources, after `least_squares_tests.cpp`)

A header of its own rather than more of `least_squares.hpp`: about 600 lines of numerics with no node in them,
which `least_squares.hpp` (about 360 lines once the curve fit has `compute_exact`) then uses from two operations.
No public behaviour changes, so no CHANGELOG entry.

**Interfaces:**
- Consumes: the foundation's `WideUnsigned<L>` (`from_u64`, `from_limbs`, `limb`, `is_zero`), `WideSigned<L>`
  and its `add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none`, `lcm_checked_or_none`, `divmod`, `scaled_to_denominator`,
  `WideRatio<L>`, `narrow_wide_ratio`; `RepTraits<Rep>` (`evaluate.hpp:64-156`).
- Produces (all in `formula::detail`):
  - `inline constexpr std::size_t maxRegressors = 8;`
  - `inline constexpr std::size_t regressionSumLimbs = 12;`
  - `consteval std::size_t regression_limbs(std::size_t regressorCount) noexcept;` -- 12, 19, 28, 37, 46, 55,
    64, 73 for 1 to 8
  - `template <std::size_t L> constexpr std::optional<WideSigned<L>> exact_quotient(WideSigned<L> const&,
    WideSigned<L> const&) noexcept` and `template <std::size_t Wide, std::size_t Narrow> requires(Wide >= Narrow)
    constexpr WideSigned<Wide> widened(WideSigned<Narrow> const&) noexcept` -- the two operations the foundation's
    signed type does not have
  - `template <typename Rep, std::size_t K> constexpr bool regression_is_posed(std::array<std::span<Rep const>, K>
    const&, std::span<Rep const>) noexcept`
  - `template <std::size_t K, std::size_t L> constexpr std::expected<std::array<WideSigned<L>, K + 1>,
    ArithmeticError> fraction_free_solve(std::array<std::array<WideSigned<L>, K + 1>, K> augmented) noexcept` --
    the numerators N_0..N_{K-1}, then the determinant; `DomainError` for a singular matrix
  - `template <std::size_t K> constexpr std::expected<std::array<WideRatio<regression_limbs(K)>, K + 3>,
    ArithmeticError> exact_regression(std::array<std::span<Rational const>, K> const& regressorColumns,
    std::span<Rational const> responses) noexcept` -- `{constant, c_1..c_K, R², points}`
  - `template <typename Rep, std::size_t K> constexpr std::expected<std::array<Rep, K + 3>, ArithmeticError>
    approximate_regression(std::array<std::span<Rep const>, K> const&, std::span<Rep const>) noexcept`
  - `template <std::size_t L, std::size_t M> constexpr std::expected<std::array<Rational, M>, ArithmeticError>
    narrowed_all(std::array<WideRatio<L>, M> const&) noexcept`

- [ ] **Step 1: Failing tests.** Create `test/least_squares_kernel_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// The regression kernel behind the fits over raw observations, called
// directly: exact in wide integers, approximate in double. Every expected
// value was computed with Python's fractions from the data beside it.
#include <formula-cpp/detail/least_squares_kernel.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace
{
using formula::Rational;

constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

template <std::size_t K, std::size_t N>
constexpr std::expected<std::array<Rational, K + 3>, formula::ArithmeticError> exact_fit(
    std::array<std::array<Rational, N>, K> const& regressorValues, std::array<Rational, N> const& responseValues)
{
    std::array<std::span<Rational const>, K> regressorColumns;
    for (std::size_t at = 0; at < K; ++at)
        regressorColumns[at] = std::span<Rational const> { regressorValues[at] };
    auto const wide = formula::detail::exact_regression<K>(regressorColumns, std::span<Rational const> { responseValues });
    if (!wide.has_value())
        return std::unexpected { wide.error() };
    return formula::detail::narrowed_all(*wide);
}

template <std::size_t K, std::size_t N>
std::expected<std::array<double, K + 3>, formula::ArithmeticError> approximate_fit(
    std::array<std::array<Rational, N>, K> const& regressorValues, std::array<Rational, N> const& responseValues)
{
    std::array<std::array<double, N>, K> regressorDoubles;
    std::array<double, N> responseDoubles;
    for (std::size_t rowAt = 0; rowAt < N; ++rowAt)
    {
        for (std::size_t at = 0; at < K; ++at)
            regressorDoubles[at][rowAt] = regressorValues[at][rowAt].to_double();
        responseDoubles[rowAt] = responseValues[rowAt].to_double();
    }
    std::array<std::span<double const>, K> regressorColumns;
    for (std::size_t at = 0; at < K; ++at)
        regressorColumns[at] = std::span<double const> { regressorDoubles[at] };
    return formula::detail::approximate_regression<double, K>(regressorColumns,
                                                              std::span<double const> { responseDoubles });
}

[[nodiscard]] bool close(double approximate, Rational exact, double relative)
{
    return std::abs(approximate - exact.to_double()) <= relative * std::abs(exact.to_double());
}

// The line: t = 1, 2, 4, 7 s against L = 10.2, 10.9, 12.1, 14.3 mm, in metres.
constexpr std::array<std::array<Rational, 4>, 1> lineTimes { { { rat(1), rat(2), rat(4), rat(7) } } };
constexpr std::array<Rational, 4> lineLengths { rat(102, 10'000), rat(109, 10'000), rat(121, 10'000),
                                                rat(143, 10'000) };

// Two regressors: 11.3 ... 29.7 degC in kelvin, 2.3 ... 4.3 % as fractions,
// against 103.52 ... 106 mm in metres.
constexpr std::array<std::array<Rational, 6>, 2> twoRegressors { {
    { rat(28445, 100), rat(28685, 100), rat(29105, 100), rat(29225, 100), rat(29645, 100), rat(30285, 100) },
    { rat(23, 1000), rat(31, 1000), rat(29, 1000), rat(41, 1000), rat(37, 1000), rat(43, 1000) },
} };
constexpr std::array<Rational, 6> twoRegressorLengths { rat(2588, 25'000), rat(10413, 100'000), rat(10433, 100'000),
                                                        rat(1051, 10'000),  rat(10521, 100'000), rat(106, 1000) };
} // namespace

TEST_CASE("the kernel's widths are the estimate's", "[least-squares][kernel]")
{
    STATIC_REQUIRE(formula::detail::regression_limbs(1) == 12);
    STATIC_REQUIRE(formula::detail::regression_limbs(2) == 19);
    STATIC_REQUIRE(formula::detail::regression_limbs(3) == 28);
    STATIC_REQUIRE(formula::detail::regression_limbs(8) == 73);
    STATIC_REQUIRE(formula::detail::regression_limbs(1) == formula::detail::regressionSumLimbs);
}

TEST_CASE("the exact kernel's line is the closed form", "[least-squares][kernel]")
{
    // Intercept 19/2 mm, slope 19/28 mm/s, R^2 1083/1085, four points -- not
    // the secant 41/60 mm/s, not x-on-y.
    // At run time: one exact fit makes dozens of wide divisions (the constant-evaluation rule).
    auto const line = exact_fit(lineTimes, lineLengths);
    REQUIRE(line.has_value());
    CHECK(*line == std::array { rat(19, 2000), rat(19, 28'000), rat(1083, 1085), rat(4) });
}

TEST_CASE("the order of the rows does not change the kernel's exact fit", "[least-squares][kernel]")
{
    constexpr std::array<std::array<Rational, 4>, 1> shuffledTimes { { { rat(4), rat(1), rat(7), rat(2) } } };
    constexpr std::array<Rational, 4> shuffledLengths { rat(121, 10'000), rat(102, 10'000), rat(143, 10'000),
                                                        rat(109, 10'000) };
    CHECK(exact_fit(shuffledTimes, shuffledLengths) == exact_fit(lineTimes, lineLengths));
    // Rows 6, 3, 1, 5, 2, 4 of the two-regressor fixture.
    constexpr std::array<std::array<Rational, 6>, 2> shuffledRegressors { {
        { twoRegressors[0][5], twoRegressors[0][2], twoRegressors[0][0], twoRegressors[0][4], twoRegressors[0][1],
          twoRegressors[0][3] },
        { twoRegressors[1][5], twoRegressors[1][2], twoRegressors[1][0], twoRegressors[1][4], twoRegressors[1][1],
          twoRegressors[1][3] },
    } };
    constexpr std::array<Rational, 6> shuffledResponses { twoRegressorLengths[5], twoRegressorLengths[2],
                                                          twoRegressorLengths[0], twoRegressorLengths[4],
                                                          twoRegressorLengths[1], twoRegressorLengths[3] };
    CHECK(exact_fit(shuffledRegressors, shuffledResponses) == exact_fit(twoRegressors, twoRegressorLengths));
}

TEST_CASE("each kernel pre-check is the fit's own domain error, in both representations", "[least-squares][kernel]")
{
    using formula::ArithmeticError;
    // Counts differ: three times, four lengths -- never a read past the shorter.
    std::array<Rational, 3> const threeTimes { rat(1), rat(2), rat(4) };
    std::array<std::span<Rational const>, 1> const shortColumn { std::span<Rational const> { threeTimes } };
    CHECK(formula::detail::exact_regression<1>(shortColumn, std::span<Rational const> { lineLengths }).error()
          == ArithmeticError::DomainError);
    std::array<double, 3> const threeDoubles { 1.0, 2.0, 4.0 };
    std::array<double, 4> const fourDoubles { 0.0102, 0.0109, 0.0121, 0.0143 };
    std::array<std::span<double const>, 1> const shortDoubles { std::span<double const> { threeDoubles } };
    CHECK(formula::detail::approximate_regression<double, 1>(shortDoubles, std::span<double const> { fourDoubles })
              .error()
          == ArithmeticError::DomainError);
    // None made, and as many rows as regressors: no fit.
    std::array<std::span<Rational const>, 1> const none { std::span<Rational const> {} };
    CHECK(formula::detail::exact_regression<1>(none, std::span<Rational const> {}).error() == ArithmeticError::DomainError);
    constexpr std::array<std::array<Rational, 2>, 2> twoByTwo { { { rat(1), rat(2) }, { rat(3), rat(5) } } };
    CHECK(exact_fit(twoByTwo, std::array { rat(7), rat(11) }).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(twoByTwo, std::array { rat(7), rat(11) }).error() == ArithmeticError::DomainError);
    // Flat values: R^2 would be 0/0, so the whole fit is refused.
    constexpr std::array<Rational, 4> flat { rat(127, 10'000), rat(127, 10'000), rat(127, 10'000), rat(127, 10'000) };
    CHECK(exact_fit(lineTimes, flat).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(lineTimes, flat).error() == ArithmeticError::DomainError);
    // A flat regressor: no line along it. In double, three 0.1 s have a mean
    // that is not 0.1; decided on the values themselves, it is still refused.
    constexpr std::array<std::array<Rational, 4>, 1> sameTime { { { rat(1, 10), rat(1, 10), rat(1, 10), rat(1, 10) } } };
    CHECK(exact_fit(sameTime, lineLengths).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(sameTime, lineLengths).error() == ArithmeticError::DomainError);
}

TEST_CASE("the kernel solves two and three regressors exactly", "[least-squares][kernel]")
{
    // Two: the constant is the length at 0 K, not at 0 degC.
    auto const two = exact_fit(twoRegressors, twoRegressorLengths);
    REQUIRE(two.has_value());
    CHECK(*two
          == std::array { rat(22365154943, 276592800000), rat(346407, 4609880000), rat(3842851, 69148200),
                          rat(27398849648, 27403085919), rat(6) });
    // Three, in integers.
    constexpr std::array<std::array<Rational, 7>, 3> threeRegressors { {
        { rat(3), rat(7), rat(2), rat(9), rat(4), rat(8), rat(5) },
        { rat(11), rat(13), rat(17), rat(19), rat(23), rat(29), rat(31) },
        { rat(2), rat(1), rat(4), rat(3), rat(6), rat(5), rat(7) },
    } };
    constexpr std::array<Rational, 7> threeResponses { rat(41), rat(57), rat(49), rat(71), rat(66), rat(83), rat(79) };
    auto const three = exact_fit(threeRegressors, threeResponses);
    REQUIRE(three.has_value());
    CHECK(*three
          == std::array { rat(1222381, 72160), rat(14161, 4510), rat(84499, 72160), rat(52383, 36080),
                          rat(362845727, 364047200), rat(7) });
}

TEST_CASE("the kernel fits as many rows as regressors plus one exactly, with R^2 of 1", "[least-squares][kernel]")
{
    constexpr std::array<std::array<Rational, 3>, 2> regressorsOfThree { { { rat(1), rat(2), rat(4) },
                                                                           { rat(2), rat(1), rat(3) } } };
    CHECK(exact_fit(regressorsOfThree, std::array { rat(5), rat(7), rat(2) })
          == std::array { rat(39, 4), rat(-1, 4), rat(-9, 4), rat(1), rat(3) });
}

TEST_CASE("the kernel's fraction-free solve exchanges rows at a zero pivot", "[least-squares][kernel]")
{
    // [[0, 2 | 4], [3, 1 | 5]]: the first pivot is 0 and the row below is
    // not, so the rows are exchanged. 3 g0 + g1 = 5 and 2 g1 = 4 give g0 = 1,
    // g1 = 2: numerators 6 and 12 over the determinant 6. The centred normal
    // equations are positive semi-definite, where a zero pivot leaves its
    // column zero below it, so a fit never needs the exchange; the solve is
    // still correct for any matrix it is given, and this proves it.
    using Wide = formula::detail::WideSigned<4>;
    auto const whole = [](std::uint64_t held) { return Wide { false, formula::detail::WideUnsigned<4>::from_u64(held) }; };
    std::array<std::array<Wide, 3>, 2> const augmented { { { whole(0), whole(2), whole(4) },
                                                           { whole(3), whole(1), whole(5) } } };
    auto const solved = formula::detail::fraction_free_solve<2, 4>(augmented);
    REQUIRE(solved.has_value());
    CHECK((*solved)[0].magnitude == formula::detail::WideUnsigned<4>::from_u64(6));
    CHECK((*solved)[1].magnitude == formula::detail::WideUnsigned<4>::from_u64(12));
    CHECK((*solved)[2].magnitude == formula::detail::WideUnsigned<4>::from_u64(6));
    CHECK(!(*solved)[0].negative);
    CHECK(!(*solved)[2].negative);
    // A matrix whose second column has no non-zero pivot is singular.
    std::array<std::array<Wide, 3>, 2> const singular { { { whole(1), whole(2), whole(4) },
                                                          { whole(2), whole(4), whole(5) } } };
    CHECK(formula::detail::fraction_free_solve<2, 4>(singular).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("the kernel refuses a singular design exactly and in double", "[least-squares][kernel]")
{
    // x1 = 1, 2, 4, 7 and a second regressor that is x1 again, up to scale and
    // offset: whatever was observed, the design has no unique solution.
    constexpr std::array<Rational, 4> lengths { rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10) };
    constexpr std::array<std::array<Rational, 4>, 2> doubled { { { rat(1), rat(2), rat(4), rat(7) },
                                                                 { rat(2), rat(4), rat(8), rat(14) } } };
    constexpr std::array<std::array<Rational, 4>, 2> shifted { {
        { rat(1), rat(2), rat(4), rat(7) },
        { rat(27415, 100), rat(27515, 100), rat(27715, 100), rat(28015, 100) },
    } };
    constexpr std::array<std::array<Rational, 4>, 2> affine { { { rat(1), rat(2), rat(4), rat(7) },
                                                                { rat(-4), rat(-1), rat(5), rat(14) } } };
    for (auto const& design: { doubled, shifted, affine })
    {
        CHECK(exact_fit(design, lengths).error() == formula::ArithmeticError::DomainError);
        CHECK(approximate_fit(design, lengths).error() == formula::ArithmeticError::DomainError);
    }
}

namespace
{
// x1 = 1..8, x2 = x1 + e / delta with e = +1, -1, -1, +1, +1, -1, -1, +1,
// orthogonal to x1; y = 3 + 2 x1 - 5 x2 exactly. 1 - R^2 of x2 on x1 is
// 8 / (42 delta^2 + 8).
template <std::int64_t Delta>
constexpr std::array<std::array<Rational, 8>, 2> near_collinear()
{
    constexpr std::array<std::int64_t, 8> signs { 1, -1, -1, 1, 1, -1, -1, 1 };
    std::array<std::array<Rational, 8>, 2> made;
    for (std::size_t at = 0; at < 8; ++at)
    {
        auto const position = static_cast<std::int64_t>(at) + 1;
        made[0][at] = rat(position);
        made[1][at] = rat(position * Delta + signs[at], Delta);
    }
    return made;
}

template <std::int64_t Delta>
constexpr std::array<Rational, 8> exactly_linear()
{
    constexpr auto made = near_collinear<Delta>();
    std::array<Rational, 8> responses;
    for (std::size_t at = 0; at < 8; ++at)
        responses[at] = *formula::checked_sub(*formula::checked_add(rat(3), *formula::checked_mul(rat(2), made[0][at])),
                                              *formula::checked_mul(rat(5), made[1][at]));
    return responses;
}
} // namespace

TEST_CASE("the kernel answers a nearly collinear design exactly, and in double down to the stated tolerance",
          "[least-squares][kernel]")
{
    constexpr std::array exact { rat(3), rat(2), rat(-5), rat(1), rat(8) };
    // 1 - R^2 = 1/1312501, about 7.6e-7: both answer; double loses about six
    // digits to the condition of the design.
    CHECK(exact_fit(near_collinear<500>(), exactly_linear<500>()) == exact);
    auto const at500 = approximate_fit(near_collinear<500>(), exactly_linear<500>());
    REQUIRE(at500.has_value());
    CHECK(close((*at500)[1], rat(2), 1e-8));
    // 1/525000001, about 1.9e-9, above the tolerance of 1e-9: both answer.
    CHECK(exact_fit(near_collinear<10'000>(), exactly_linear<10'000>()) == exact);
    CHECK(approximate_fit(near_collinear<10'000>(), exactly_linear<10'000>()).has_value());
    // 1/2100000001, about 4.8e-10, below it: exact still answers, double
    // refuses -- it cannot tell this design from a singular one.
    CHECK(exact_fit(near_collinear<20'000>(), exactly_linear<20'000>()) == exact);
    CHECK(approximate_fit(near_collinear<20'000>(), exactly_linear<20'000>()).error()
          == formula::ArithmeticError::DomainError);
}

TEST_CASE("the kernel's double route agrees with its exact route on well-conditioned data", "[least-squares][kernel]")
{
    auto const line = approximate_fit(lineTimes, lineLengths);
    REQUIRE(line.has_value());
    CHECK(close((*line)[0], rat(19, 2000), 1e-12));
    CHECK(close((*line)[1], rat(19, 28'000), 1e-12));
    CHECK(close((*line)[2], rat(1083, 1085), 1e-12));
    CHECK((*line)[3] == 4.0);
    auto const two = approximate_fit(twoRegressors, twoRegressorLengths);
    REQUIRE(two.has_value());
    CHECK(close((*two)[0], rat(22365154943, 276592800000), 1e-12));
    CHECK(close((*two)[1], rat(346407, 4609880000), 1e-12));
    CHECK(close((*two)[2], rat(3842851, 69148200), 1e-12));
    CHECK(close((*two)[3], rat(27398849648, 27403085919), 1e-12));
}

TEST_CASE("an exact fit that outgrows the kernel's width is Overflow, never a line", "[least-squares][kernel]")
{
    // Point k at ((k + 1)/(k + 2), (2k + 3)/(k + 3)): a different denominator
    // on every point. 15 points fit; 128 need about 744 bits, past 384.
    auto const distinct = [](std::size_t pointCount) {
        std::array<Rational, 128> xs;
        std::array<Rational, 128> ys;
        for (std::size_t at = 0; at < pointCount; ++at)
        {
            auto const position = static_cast<std::int64_t>(at);
            xs[at] = rat(position + 1, position + 2);
            ys[at] = rat(2 * position + 3, position + 3);
        }
        std::array<std::span<Rational const>, 1> const regressorColumns { std::span<Rational const> { xs }.first(pointCount) };
        return formula::detail::exact_regression<1>(regressorColumns, std::span<Rational const> { ys }.first(pointCount));
    };
    CHECK(distinct(15).has_value());
    CHECK(distinct(128).error() == formula::ArithmeticError::Overflow);
}
```

  (`xs`/`ys` are test locals in a test file, which no consumer-globals rule covers.)
- [ ] **Step 2: Run and fail.** `cl.ps1 -Filter "kernel"`. Expected: `BUILD FAILED`, C1083 on
  `formula-cpp/detail/least_squares_kernel.hpp`.
- [ ] **Step 3: Implement.** Create the header. Its file comment states: what the kernel computes and for whom;
  the exact method (common denominators, integer sums, centred sums times n, the line's closed form, fraction-free
  elimination with row exchanges, a zero pivot as the exact singular decision, back-substitution exact by
  Cramer's rule, constant and R²); **the width estimate table from this plan, in prose and figures**; and the
  approximate route with its tolerance and what it cannot promise (below). Includes:
  `detail/wide_int.hpp`, `detail/wide_rounding.hpp`, `error.hpp`, `evaluate.hpp`,
  `rational.hpp`; `<array>`, `<cstddef>`, `<cstdint>`, `<expected>`, `<optional>`, `<span>`, `<type_traits>`,
  `<utility>`. Key code, every function `[[nodiscard]]` and documented:

```cpp
namespace formula::detail
{
/// The most regressors a fit takes: the exact solve is sized for this many.
inline constexpr std::size_t maxRegressors = 8;

/// The width, in 32-bit limbs, in which a fit forms its sums and centred sums,
/// whatever its number of regressors, and the whole of a line's solve.
inline constexpr std::size_t regressionSumLimbs = 12;

/// The width, in 32-bit limbs, of the exact solve for @p regressorCount
/// regressors, 1 to `maxRegressors`: see the file comment's estimate.
[[nodiscard]] consteval std::size_t regression_limbs(std::size_t regressorCount) noexcept
{
    constexpr std::array<std::size_t, maxRegressors> limbsFor { 12, 19, 28, 37, 46, 55, 64, 73 };
    return limbsFor[regressorCount - 1];
}

/// @p dividend over @p divisor when that divides exactly; nothing for a zero
/// divisor or a remainder. The foundation's signed type adds, subtracts and
/// multiplies; the solve also divides, and only ever exactly.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> exact_quotient(WideSigned<L> const& dividend,
                                                                    WideSigned<L> const& divisor) noexcept
{
    if (divisor.magnitude.is_zero())
        return std::nullopt;
    WideDivision<L> const divided = divmod(dividend.magnitude, divisor.magnitude);
    if (!divided.remainder.is_zero())
        return std::nullopt;
    return WideSigned<L> { dividend.negative != divisor.negative && !divided.quotient.is_zero(), divided.quotient };
}

/// @p narrow in a type of more limbs, the same value: the sums are formed in
/// `regressionSumLimbs` and the solve for K >= 2 in more.
template <std::size_t Wide, std::size_t Narrow>
    requires(Wide >= Narrow)
[[nodiscard]] constexpr WideSigned<Wide> widened(WideSigned<Narrow> const& narrow) noexcept
{
    std::array<std::uint32_t, Wide> spread {};
    for (std::size_t limbAt = 0; limbAt < Narrow; ++limbAt)
        spread[limbAt] = narrow.magnitude.limb(limbAt);
    return WideSigned<Wide> { narrow.negative, WideUnsigned<Wide>::from_limbs(spread) };
}
```

  Then, in this order (names chosen off the consumer-globals list):
  - `common_denominator<L>(std::span<Rational const> column)` -> `std::optional<WideUnsigned<L>>`: start at
    `from_u64(1)` and fold in each element's denominator with `lcm_checked_or_none`; nothing on overflow. (The parameter is
    not `column`, a consumer global: `observedColumn`.)
  - each row's scaled element is the foundation's `scaled_to_denominator(element, commonDenominator)`, the
    element times the column's common denominator as a `WideSigned` (exact, since the denominator is a multiple of
    the element's own).
  - Operand order: the foundation's `mul_checked_or_none` skips zero limbs of its **left** operand, so every product puts
    the narrower factor on the left -- the row count in `n * P`, a row's scaled element in each cross product, a
    column's denominator when an output is rescaled. Where only a 32-bit divisor divides (none in this kernel as
    specified), `divmod_small` is the cheaper call.
  - `accumulate<L>(WideSigned<L>& runningTotal, std::type_identity_t<std::optional<WideSigned<L>>> const& addend)`
    -> `bool`, false when either overflowed (the `type_identity_t` keeps `L` deduced from the first argument only,
    so a plain `WideSigned` converts).
  - `regression_is_posed<Rep, K>`: every regressor column exactly as long as the responses; at least K + 1 rows;
    the responses not all `==` the first; no regressor column all `==` its first. Compared with `==` only, on the
    values as given, before any sum, so it holds in every `Rep` (the reason `least_squares.hpp:39-47` gives for the
    curve fit).
  - `struct RegressionSums<K>` (`regressorDenominators`, `responseDenominator`, `regressorTotals`,
    `responseTotal`, `crossTotals` upper triangle, `responseCrossTotals`, `responseSquareTotal`, all in
    `regressionSumLimbs`) and `regression_sums<K>`: the denominators first (one pass per column), then **one pass
    over the rows** forming each row's scaled regressors and response, never an n-sized array, accumulating every
    total and product; `Overflow` when anything outgrows 12 limbs.
  - `struct CentredSums<K>` (`regressorSpread` M, both triangles; `responseCoSpread` v; `responseSpread` T) and
    `centred_sums<K>(RegressionSums<K> const&, WideSigned<regressionSumLimbs> const& rowCount)`:
    `M_jk = n P_jk - S_j S_k`, `v_j = n P_jy - S_j S_y`, `T = n P_yy - S_y^2`, mirroring M's lower triangle.
  - `wide_ratio_of<L>(WideSigned<L> const& dividend, WideSigned<L> const& over)` -> `WideRatio<L> { .negative =
    dividend.negative != over.negative && !dividend.magnitude.is_zero(), .numerator = dividend.magnitude,
    .denominator = over.magnitude }`; `points_ratio<L>(std::size_t rowCount)` -> `n / 1`.
  - `line_outputs(RegressionSums<1> const&, CentredSums<1> const&, WideSigned<12> const& rowCount)` -> the closed
    form: intercept `(M S_y - v S_x) / (n D_y M)`, slope `v D_x / (M D_y)`, R² `v^2 / (M T)`, points. M > 0 and
    T > 0 by the pre-checks; any `nullopt` is `Overflow`.
  - `fraction_free_solve<K, L>`:

```cpp
/// Solves M g = v, given as the augmented rows [M | v], by fraction-free
/// (Bareiss) elimination: every entry stays an integer, each division by the
/// previous pivot is exact, and the last pivot is the determinant of the
/// rows as exchanged. The answer is g_k = N_k / det, returned as N_0 ...
/// N_{K-1} and then det. A column with no non-zero pivot at or below its
/// stage is a singular matrix: `DomainError`. An inexact division cannot
/// happen (Bareiss' theorem, Cramer's rule); it is a backstop, `Overflow`,
/// so that a slip here fails rather than answers.
template <std::size_t K, std::size_t L>
[[nodiscard]] constexpr std::expected<std::array<WideSigned<L>, K + 1>, ArithmeticError> fraction_free_solve(
    std::array<std::array<WideSigned<L>, K + 1>, K> augmented) noexcept
{
    WideSigned<L> previousPivot { false, WideUnsigned<L>::from_u64(1) };
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        std::size_t pivotAt = stage;
        while (pivotAt < K && augmented[pivotAt][stage].magnitude.is_zero())
            ++pivotAt;
        if (pivotAt == K)
            return std::unexpected { ArithmeticError::DomainError };
        std::swap(augmented[stage], augmented[pivotAt]);
        for (std::size_t below = stage + 1; below < K; ++below)
        {
            for (std::size_t across = stage + 1; across <= K; ++across)
            {
                std::optional<WideSigned<L>> const kept = mul_checked_or_none(augmented[stage][stage], augmented[below][across]);
                std::optional<WideSigned<L>> const removed = mul_checked_or_none(augmented[below][stage], augmented[stage][across]);
                if (!kept.has_value() || !removed.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                std::optional<WideSigned<L>> const difference = sub_checked_or_none(*kept, *removed);
                if (!difference.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                std::optional<WideSigned<L>> const eliminated = exact_quotient(*difference, previousPivot);
                if (!eliminated.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                augmented[below][across] = *eliminated;
            }
            augmented[below][stage] = WideSigned<L> {};
        }
        previousPivot = augmented[stage][stage];
    }
    std::array<WideSigned<L>, K + 1> solved;
    WideSigned<L> const determinant = augmented[K - 1][K - 1];
    solved[K] = determinant;
    solved[K - 1] = augmented[K - 1][K];
    for (std::size_t stage = K - 1; stage-- > 0;)
    {
        std::optional<WideSigned<L>> accumulated = mul_checked_or_none(augmented[stage][K], determinant);
        for (std::size_t later = stage + 1; later < K && accumulated.has_value(); ++later)
        {
            std::optional<WideSigned<L>> const known = mul_checked_or_none(augmented[stage][later], solved[later]);
            accumulated = known.has_value() ? sub_checked_or_none(*accumulated, *known) : std::nullopt;
        }
        if (!accumulated.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        std::optional<WideSigned<L>> const numeratorAt = exact_quotient(*accumulated, augmented[stage][stage]);
        if (!numeratorAt.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        solved[stage] = *numeratorAt;
    }
    return solved;
}
```

  - `solved_outputs<K>(RegressionSums<K> const&, CentredSums<K> const&, std::size_t rowCount)` for K >= 2: widen
    M, v, T, the totals and the denominators to `regression_limbs(K)` (`widened`), solve `[M | v]`, then
    `c_k = N_k D_k / (det D_y)`, constant `(det S_y - sum N_k S_k) / (n D_y det)`, R²
    `(sum N_k v_k) / (det T)`, points. The derivation, for the comment: the centred normal equations
    `sum_k M_jk / (n D_j D_k) c_k = v_j / (n D_j D_y)` become `M g = v` with `g_k = c_k D_y / D_k`; the constant is
    `ybar - sum c_k xbar_k = (S_y - sum g_k S_k) / (n D_y)`; and `R² = sum c_k C_ky / C_yy = sum g_k v_k / T`.
  - `exact_regression<K>`: pre-check (`DomainError`), `regression_sums`, `centred_sums`, then
    `if constexpr (K == 1) return line_outputs(...); else return solved_outputs<K>(...);`. `regression_limbs(K)`
    in the return type makes K = 0 or K > 8 fail to compile, in the consteval array's words; the operations never
    instantiate those.
  - `narrowed_all<L, M>`: `std::array<Rational, M> narrowed;` default-initialised, filled with
    `narrow_wide_ratio(wideOutputs[at])`, the first error returned -- **every output fits, or the call fails**.
  - `approximate_regression<Rep, K>`, over helpers that take and return `std::expected<Rep, ArithmeticError>` and
    make exactly one `RepTraits` call each (`rep_add`, `rep_sub`, `rep_mul`, `rep_div`, always called with the
    explicit `<Rep>` so that a plain `Rep` converts):

```cpp
/// The fit in any representation, for `checked_evaluate_si<double>`: the
/// same pre-checks, the means, then one pass of centred sums -- never an
/// n-sized array -- then the centred normal equations solved by
/// square-root-free Cholesky, C = L D L^T. A pivot D_k at or below 1e-9 times
/// C_kk -- that is, 1 - R^2 of regressor k on the ones before it below 1e-9 --
/// is taken for a singular design: `DomainError`. Rounded data cannot decide
/// singularity exactly, and a design near that line has few trustworthy
/// digits either way.
///
/// Every operation is its own `RepTraits` call, never `a * b + c` in one
/// expression: ISO C++ lets a compiler fuse a multiply and an add only within
/// one expression. g++ in its GNU dialect (`-std=gnu++23`) contracts across
/// statements too, so a consumer's build there can differ in the last bits;
/// the route is approximate either way, and never traced.
template <typename Rep, std::size_t K>
[[nodiscard]] constexpr std::expected<std::array<Rep, K + 3>, ArithmeticError> approximate_regression(
    std::array<std::span<Rep const>, K> const& regressorColumns, std::span<Rep const> responses) noexcept
```

  Its body: `regression_is_posed` (`DomainError`); `zero`, `rowTally` (`Traits::from(Rational { n })`) and
  `pivotFloorScale` (`Traits::from(Rational { 1, 1'000'000'000 })`); the means (`regressorMeans[K]`,
  `responseMean`); the second pass into `centredCross[K][K]` (upper triangle, then mirrored),
  `centredWithResponse[K]`, `responseSpreadSum`; then for each `stage`: `pivotValue = C_ss - sum_{m<s} L_sm^2 D_m`,
  refuse when `!(pivotFloorScale * C_ss < pivotValue)`, then `L_ls = (C_ls - sum_{m<s} L_lm L_sm D_m) / D_s` for
  each later `l`; forward `z_k = C_ky - sum_{m<k} L_km z_m`; `w_k = z_k / D_k`; back
  `c_k = w_k - sum_{m>k} L_mk c_m`; constant `ybar - sum c_k xbar_k`; R² `sum c_k C_ky / C_yy`; points
  `Traits::from(Rational { n })`. Arrays of `std::expected<Rep, ArithmeticError>` are default-initialised and
  written before read; the first error found in any output is returned. For K = 1 this is the two-pass
  `S_xy / S_xx`.
- [ ] **Step 4: Register.** Root `FILE_SET`:
  `"${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/least_squares_kernel.hpp"` in the `detail/` group;
  `least_squares_kernel_tests.cpp` in `formula-cpp-tests`.
- [ ] **Step 5: Run and pass.** `cl.ps1 -Filter "kernel"` (+11 cases, all selected), `gcc14.sh "kernel"`, then both
  unfiltered. Expected `ALL OK`.
- [ ] **Step 6: Mutation checks (defect class 7), each restored with a plain write.** (a) Drop the row exchange
  (take `pivotAt = stage` always): the fraction-free test fails, nothing else. (b) Compare `pivotFloorScale * C_ss <=
  pivotValue` with a scale of 1e-10: the 20000 case answers and fails. (c) Skip the flat-values pre-check: the
  flat case returns something other than `DomainError` (in the exact kernel, `T = 0` makes R²'s denominator zero --
  `narrow_wide_ratio` must not be reached with it; confirm the pre-check is what stops it). (d) Compute R² as
  `v^2 / (M T)` for K >= 2 (the line's formula): the two-regressor R² fails.
- [ ] **Step 7: Commit.**

```text
feat(least_squares): an exact regression kernel in wide integers, with its double counterpart

A fit through fifty readings at four decimals is an exact fraction far
wider than 64 bits, and a fit of several regressors must decide exactly
when its design is singular. The kernel brings each column to a common
denominator so that every sum is an integer, solves the centred normal
equations by fraction-free elimination, where a zero pivot is an exact
verdict of singularity, and returns wide fractions for rounding. Its
widths come from a bound on realistic data: 12 limbs for a line, up to
73 for eight regressors.

The double route runs the same shape with square-root-free Cholesky and
refuses a pivot below 1e-9 of its diagonal, since rounded data cannot
decide singularity.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 4: A line through raw observations, with R² and the number of points

**Files:**
- Modify: `include/formula-cpp/least_squares.hpp` (file comment `:4-50`; include the kernel and
  `observations.hpp`; new operation after `LinearLeastSquares` `:178`; overloads and refusals `:194-280`),
  `include/formula-cpp/observations.hpp` (`detail::RefusedObservations` gains `refused`),
  `test/least_squares_tests.cpp`, `test/consumer_globals_tests.cpp` (globals `:125-146`, probe after the fit at
  `:796-803`), `test/consumer_globals_run_tests.cpp:16`, `test/CMakeLists.txt` (`:2189-2205` and after `:2418`),
  `test/negative/least_squares_not_a_curve.cpp:2` (its `// EXPECT:` line), five new `test/negative/*.cpp`,
  `CHANGELOG.md`

**Interfaces:**
- Consumes: Task 2's shape; Task 3's `detail::exact_regression<1>`, `detail::approximate_regression<Rep, 1>`,
  `detail::narrowed_all`, `detail::regression_limbs`; the foundation's `rounded_output` and `compute_exact` hook.
- Produces:

```cpp
struct LinearLeastSquaresOfObservations
{
    static constexpr std::string_view name = "linear least squares";
    static constexpr std::array shapes { InputShape::Observations, InputShape::Observations };
    static constexpr std::array<std::string_view, 4> outputs { "intercept", "slope", "r squared", "points" };
    static constexpr std::size_t exact_limbs = detail::regression_limbs(1);   // 12
    static consteval std::optional<std::array<Dimension, 4>> output_dimensions(std::array<Dimension, 2>) noexcept;
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 4>, ArithmeticError>
        compute_exact(std::span<Rational const> pointObservations, std::span<Rational const> valueObservations) noexcept;
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 4>, ArithmeticError>
        compute(std::span<Rep const> pointObservations, std::span<Rep const> valueObservations) noexcept;
};
template <ObservationsNode X, ObservationsNode Y>
constexpr auto linear_least_squares(X pointObservations, Y valueObservations, Citation citation) noexcept;   // the fit, or a refused one
template <ObservationsNode X, ObservationsNode Y>
constexpr auto linear_least_squares(X, Y) noexcept;                                         // uncited: refused
```

  and, in `detail`: `RequireFitOfTwoQuantities<Q>`, `ObservedQuantity<T>`, `RefusedObservationFit`,
  `refused_observation_fit(Citation)`. The existing `linear_least_squares(Domain, Values, Citation)` gains
  `requires(!(ObservationsNode<Domain> && ObservationsNode<Values>))`, and `RequireFitOfCurve`'s message changes.

- [ ] **Step 1: Failing tests.** Append to `test/least_squares_tests.cpp` (add `#include <formula-cpp/constraint.hpp>`
  if `method.hpp` does not bring it):

```cpp
namespace
{
// Declared here: the library states no unit of millimetres per second.
inline constexpr formula::Unit MillimetrePerSecond { .dimension = formula::dim::Velocity,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 1000,
                                                     .symbolText = formula::symbol("mm/s"),
                                                     .decimals = 4 };
struct Speed: formula::Quantity<Speed, "v_s", "an invented slope", MillimetrePerSecond>
{
};
struct Determination: formula::Quantity<Determination, "R2", "an invented coefficient of determination", unit::One>
{
};
struct PointCount: formula::Quantity<PointCount, "n_p", "an invented number of points", unit::One>
{
};
struct Warmth: formula::Quantity<Warmth, "T_w", "an invented temperature", unit::Celsius>
{
};
inline constexpr formula::Unit MillimetrePerKelvin { .dimension = formula::dim::Length / formula::dim::Temperature,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 1000,
                                                     .symbolText = formula::symbol("mm/K"),
                                                     .decimals = 4 };
struct Stretch: formula::Quantity<Stretch, "k_T", "an invented length per kelvin", MillimetrePerKelvin>
{
};
struct FarLength: formula::Quantity<FarLength, "L_k", "an invented length, in kilometres", unit::Kilometre>
{
};

// The fixture as raw observations: t = 1, 2, 4, 7 s and L = 10.2, 10.9, 12.1,
// 14.3 mm, made in room for eight.
constexpr auto observedPoints = formula::environment(
    formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
    formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));

constexpr auto observedFit = formula::linear_least_squares(
    formula::observations<Elapsed, 8>, formula::observations<Length, 8>,
    { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
} // namespace

TEST_CASE("a line through observations gives the exact intercept, slope, r squared and points", "[least-squares][observations]")
{
    // At run time, as every assertion that runs the kernel's wide arithmetic
    // (the constant-evaluation rule); the dimensions below are static.
    // The curve fit's numbers, and two more: R^2 = 14.25^2 / (21 * 9.6875) =
    // 1083/1085, and the four observations made, not the capacity of eight.
    auto const slope = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(observedFit), observedPoints);
    CHECK(slope->measurement().value() == rat(285, 7));
    auto const intercept =
        formula::checked_evaluate<Offset>(formula::opaque_output<"intercept">(observedFit), observedPoints);
    CHECK(intercept->measurement().value() == rat(19, 2));
    auto const fitQuality =
        formula::checked_evaluate<Determination>(formula::opaque_output<"r squared">(observedFit), observedPoints);
    CHECK(fitQuality->measurement().value() == rat(1083, 1085));
    auto const made = formula::checked_evaluate<PointCount>(formula::opaque_output<"points">(observedFit), observedPoints);
    CHECK(made->measurement().value() == rat(4));
    STATIC_REQUIRE(decltype(formula::opaque_output<"intercept">(observedFit))::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(formula::opaque_output<"slope">(observedFit))::dimension == formula::dim::Velocity);
    STATIC_REQUIRE(decltype(formula::opaque_output<"r squared">(observedFit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::opaque_output<"points">(observedFit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(formula::LinearLeastSquaresOfObservations::exact_limbs == 12);
    // The hook is found: a malformed one (not noexcept, fewer than 4 limbs, another
    // return type) would fall back to compute<Rational> silently.
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::LinearLeastSquaresOfObservations,
                                                           formula::ObservationsVarNode<Elapsed, 8>,
                                                           formula::ObservationsVarNode<Length, 8>>);
}

TEST_CASE("a line through observations is traced as one call over both, rendered and documented", "[least-squares][observations][trace]")
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"slope">(observedFit), observedPoints, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 30 })
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm\n"
             "3. linear least squares(#1, #2) = intercept = 19/2 mm; slope = 19/28 mm/s; r squared = 1083/1085; "
             "points = 4 [inside not shown] [Rate of change, Example Standard 12, 5.1]\n"
             "4. slope of #3 = 19/28 mm/s\n");
    CHECK(formula::render(formula::opaque_output<"slope">(observedFit)) == "linear least squares(t(i), L(i)).slope");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::opaque_output<"r squared">(observedFit))
          == "linear least squares(`t(i)`, `L(i)`).r squared");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::opaque_output<"slope">(observedFit))
          == "\\text{linear least squares}({t}_{i}, {L}_{i})_{\\text{slope}}");
    formula::Documentation const page =
        formula::document(formula::opaque_output<"slope">(observedFit) + formula::opaque_output<"intercept">(observedFit)
                          / formula::constant<unit::Second>(rat(1)));
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].outputs
          == std::vector<std::string_view> { "intercept", "slope", "r squared", "points" });
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].shape == formula::ValueShape::Observations);
    CHECK(page.symbols[0].length == 8);
}

TEST_CASE("each output of a line through observations rounded where used is the exact output rounded", "[least-squares][observations]")
{
    // 19/28 mm/s = 0.678571... is 0.6786 at 4 dp; R^2 = 0.998156... is 0.9981
    // floored and 0.9982 to nearest.
    constexpr auto roundedSlope = formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 },
                                                          formula::RoundingMode::HalfEven>(observedFit);
    CHECK(formula::checked_evaluate<Speed>(roundedSlope, observedPoints)->measurement().value()
          == rat(3393, 5000));
    constexpr auto flooredFit = formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 },
                                                        formula::RoundingMode::Floor>(observedFit);
    CHECK(formula::checked_evaluate<Determination>(flooredFit, observedPoints)->measurement().value()
          == rat(9981, 10'000));
    // Agreement with rounding the exact output, in two modes and for every output.
    constexpr auto nearestFit = formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 },
                                                        formula::RoundingMode::HalfEven>(observedFit);
    CHECK(formula::checked_evaluate<Determination>(nearestFit, observedPoints)->measurement().value()
          == rat(4991, 5000));
    CHECK(formula::checked_evaluate<Offset>(formula::rounded_output<"intercept", unit::Millimetre,
                                                                    formula::DecimalPlaces { 1 },
                                                                    formula::RoundingMode::Floor>(observedFit),
                                            observedPoints)
              ->measurement()
              .value()
          == rat(19, 2));

    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(roundedSlope, observedPoints, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 30 });
    CHECK(text.find("3. linear least squares(#1, #2) = intercept, slope, r squared, points: rounded where used "
                    "[inside not shown] [Rate of change, Example Standard 12, 5.1]\n")
          != std::string::npos);
    CHECK(text.find("4. round(slope of #3, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]\n")
          != std::string::npos);
}

namespace
{
using TenthOfMillimetrePerMinute =
    formula::RoundingRule<unit::MillimetrePerMinute, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

constexpr auto rSquaredAtFour = formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 },
                                                        formula::RoundingMode::Floor>(observedFit);

// Two acceptances: R^2 at least 0.9981 holds (0.9981 floored), at least
// 0.9982 does not -- rounded to nearest, R^2 would be 0.9982 and pass both.
constexpr auto observedMethod = formula::method(
    formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(observedFit))),
    TenthOfMillimetrePerMinute {},
    formula::constraints(formula::constraint(rSquaredAtFour >= formula::constant<unit::One>(rat(9981, 10'000)),
                                             formula::Verdict { "repeat the readings" }),
                         formula::constraint(rSquaredAtFour >= formula::constant<unit::One>(rat(9982, 10'000)),
                                             formula::Verdict { "repeat the readings more carefully" })));
} // namespace

TEST_CASE("a line through observations is a method variant, rounded by its rule, with an R^2 acceptance", "[least-squares][observations][method]")
{
    // 285/7 mm/min = 40.714... is 40.7 at 0.1 mm/min: 407/600000 m/s.
    CHECK(formula::evaluate_method<Fitted>(observedMethod, observedPoints)->value() == rat(407, 600'000));
    auto const verdicts = formula::check_method(observedMethod, observedPoints);
    CHECK(verdicts[0].is_satisfied());
    CHECK(verdicts[1].is_violated());
}

TEST_CASE("a flat response fails an R^2 acceptance of a line through observations, never passes it", "[least-squares][observations]")
{
    // Every length 12.7 mm: R^2 would be 0/0, so the whole fit is its own
    // DomainError and the acceptance is invalid, not satisfied.
    constexpr auto flat = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Length, 8>(rat(127, 10), rat(127, 10), rat(127, 10), rat(127, 10)));
    // Static: the pre-checks refuse before any wide arithmetic.
    constexpr auto verdicts = formula::check_method(observedMethod, flat);
    STATIC_REQUIRE(verdicts[0].is_invalid());
    STATIC_REQUIRE(verdicts[0].error() == std::optional { formula::ArithmeticError::DomainError });
}

TEST_CASE("a line through observations whose counts differ, or too few, or flat, is the fit's own domain error", "[least-squares][observations]")
{
    auto const own_failure = [](auto const& environment) {
        formula::Trace<> recorded {};
        auto const outcome = formula::detail::dispatch<formula::Rational>(
            formula::opaque_output<"slope">(observedFit), environment, formula::RecordingSink { recorded });
        formula::OpaqueStepData<formula::Rational> const* const callRow = formula::opaque_data(recorded, 2);
        return !outcome.has_value() && outcome.error() == formula::ArithmeticError::DomainError && callRow != nullptr
               && callRow->failure == formula::OpaqueFailure::Own;
    };
    // Three times, four lengths: never a read past the shorter.
    CHECK(own_failure(formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)))));
    // None made, and one made: no line.
    CHECK(own_failure(formula::environment(formula::MeasuredObservations<Elapsed, 8>(),
                                           formula::MeasuredObservations<Length, 8>())));
    CHECK(own_failure(formula::environment(formula::MeasuredObservations<Elapsed, 8>(rat(3)),
                                           formula::MeasuredObservations<Length, 8>(rat(103, 10)))));
    // Every time the same, and every length the same.
    CHECK(own_failure(formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(3), rat(3), rat(3), rat(3)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)))));
    CHECK(own_failure(formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Length, 8>(rat(127, 10), rat(127, 10), rat(127, 10), rat(127, 10)))));
}

TEST_CASE("observations counted at run time, of different capacities, fit as written ones do", "[least-squares][observations]")
{
    std::array<formula::Rational, 4> const times { rat(1), rat(2), rat(4), rat(7) };
    std::array<formula::Rational, 4> const lengths { rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10) };
    auto const inputs = formula::environment(*formula::MeasuredObservations<Elapsed, 8>::from(times),
                                             *formula::MeasuredObservations<Length, 16>::from(lengths));
    constexpr auto mixed = formula::linear_least_squares(formula::observations<Elapsed, 8>,
                                                         formula::observations<Length, 16>,
                                                         { .reference = "Example Standard 12" });
    auto const slope = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(mixed), inputs);
    REQUIRE(slope.has_value());
    CHECK(slope->measurement().value() == rat(285, 7));
}

TEST_CASE("a line through observations in double agrees with the exact line", "[least-squares][observations]")
{
    // checked_evaluate_si<double> answers in the coherent unit and records no
    // trace: 19/28000 m/s, 1083/1085, 4.
    auto const slope = formula::checked_evaluate_si<double>(formula::opaque_output<"slope">(observedFit), observedPoints);
    REQUIRE(slope.has_value());
    REQUIRE(slope->has_value());
    CHECK(std::abs(**slope - 19.0 / 28'000.0) <= 1e-12 * (19.0 / 28'000.0));
    auto const fitQuality =
        formula::checked_evaluate_si<double>(formula::opaque_output<"r squared">(observedFit), observedPoints);
    CHECK(std::abs(**fitQuality - 1083.0 / 1085.0) <= 1e-12);
    auto const made = formula::checked_evaluate_si<double>(formula::opaque_output<"points">(observedFit), observedPoints);
    CHECK(**made == 4.0);
}

namespace
{
// Fifty readings at four decimals: t_k = k + 1 + (7919 k mod 997) / 10^4 s,
// L_k = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm. Reference values
// from Python's fractions (the plan's reference function).
[[nodiscard]] auto fifty_readings()
{
    std::array<formula::Rational, 50> times;
    std::array<formula::Rational, 50> lengths;
    for (std::size_t at = 0; at < 50; ++at)
    {
        auto const position = static_cast<std::int64_t>(at);
        times[at] = rat(10'000 * (position + 1) + (7919 * position) % 997, 10'000);
        lengths[at] = rat(24'100'000 + 31'700 * position + (3217 * position) % 1009 - 504, 10'000);
    }
    return formula::environment(*formula::MeasuredObservations<Elapsed, 64>::from(times),
                                *formula::MeasuredObservations<Length, 64>::from(lengths));
}

constexpr auto fiftyFit = formula::linear_least_squares(formula::observations<Elapsed, 64>,
                                                        formula::observations<Length, 64>,
                                                        { .reference = "Example Standard 12" });
} // namespace

TEST_CASE("a line through fifty readings at four decimals overflows exactly and answers rounded", "[least-squares][observations]")
{
    auto const fifty = fifty_readings();
    // Exactly: the slope alone would fit a Rational (46 and 54 bits), but the
    // intercept's numerator needs 64 bits and R^2 92 -- a call's outputs answer
    // or fail together, the operation's own Overflow.
    auto const exact = formula::checked_evaluate<Speed>(formula::opaque_output<"slope">(fiftyFit), fifty);
    REQUIRE(!exact.has_value());
    CHECK(exact.error() == formula::ArithmeticError::Overflow);
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"slope">(fiftyFit), fifty,
                                                        formula::RecordingSink { recorded });
    REQUIRE(formula::opaque_data(recorded, 2) != nullptr);
    CHECK(formula::opaque_data(recorded, 2)->failure == formula::OpaqueFailure::Own);
    // Rounded where used: 3.170694... mm/s is 3.1707 (floor would give
    // 3.1706), 2406.645493... mm is 2406.6455, and R^2 = 0.999996568...
    // floored at 6 dp is 0.999996 (to nearest, 0.999997).
    auto const slope = formula::checked_evaluate<Speed>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(fiftyFit),
        fifty);
    REQUIRE(slope.has_value());
    CHECK(slope->measurement().value() == rat(31707, 10'000));
    auto const intercept = formula::checked_evaluate<Offset>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(fiftyFit),
        fifty);
    REQUIRE(intercept.has_value());
    CHECK(intercept->measurement().value() == rat(4813291, 2000));
    auto const fitQuality = formula::checked_evaluate<Determination>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
            fiftyFit),
        fifty);
    REQUIRE(fitQuality.has_value());
    CHECK(fitQuality->measurement().value() == rat(249999, 250'000));
}

TEST_CASE("an observation that fails to convert fails the fit at that observation", "[least-squares][observations][trace]")
{
    // 1.03 x 10^17 km is 1.03 x 10^20 m: the third length. The fit relays it.
    constexpr auto farFit = formula::linear_least_squares(formula::observations<Elapsed, 8>,
                                                          formula::observations<FarLength, 8>,
                                                          { .reference = "Example Standard 12" });
    constexpr auto far = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<FarLength, 8>(rat(103), rat(127), rat(103'000'000'000'000'000), rat(163)));
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"slope">(farFit), far,
                                                        formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 }).find(
              "3. linear least squares(#1, #2) = overflow in exact arithmetic [inside not shown] "
              "[carried up from #2, at observation 3] [Example Standard 12]\n")
          != std::string::npos);
    // The rounded route relays it too: the call's line names the observation,
    // and the rounded output's own line relays the call without a position.
    formula::Trace<> roundedRecord {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            farFit),
        far,
        formula::RecordingSink { roundedRecord });
    std::string const roundedText = formula::render_trace(roundedRecord, { .maxSteps = 20 });
    INFO(roundedText);
    CHECK(roundedText.find("[carried up from #2, at observation 3]") != std::string::npos);
    CHECK(roundedText.find("[carried up from #3]") != std::string::npos);
}

TEST_CASE("a line through observations against a temperature in degrees Celsius has its intercept at 0 K", "[least-squares][observations]")
{
    // T = 11.3, 13.7, 17.9, 19.1 degC; L = 103.52, 104.13, 104.33, 105.1 mm.
    // The fit sees kelvin: the slope is 0.165 mm per kelvin, which is per
    // degree Celsius, but the intercept is the length at 0 K, 56.64275 mm; the
    // length at 0 degC, 101.7125 mm, is the intercept plus 273.15 K of slope.
    constexpr auto warmFit = formula::linear_least_squares(formula::observations<Warmth, 8>,
                                                           formula::observations<Length, 8>,
                                                           { .reference = "Example Standard 12" });
    constexpr auto warm = formula::environment(
        formula::MeasuredObservations<Warmth, 8>(rat(113, 10), rat(137, 10), rat(179, 10), rat(191, 10)),
        formula::MeasuredObservations<Length, 8>(rat(2588, 25), rat(10413, 100), rat(10433, 100), rat(1051, 10)));
    CHECK(formula::checked_evaluate<Stretch>(formula::opaque_output<"slope">(warmFit), warm)->measurement().value()
          == rat(33, 200));
    CHECK(formula::checked_evaluate<Offset>(formula::opaque_output<"intercept">(warmFit), warm)->measurement().value()
          == rat(226571, 4000));
    constexpr auto atZeroCelsius =
        formula::opaque_output<"intercept">(warmFit)
        + formula::opaque_output<"slope">(warmFit) * formula::constant<unit::Kelvin>(rat(27315, 100));
    CHECK(formula::checked_evaluate<Offset>(atZeroCelsius, warm)->measurement().value() == rat(8137, 80));
    // Per temperature, the trace shows the coherent unit: a degree Celsius
    // has an offset and is never borrowed.
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"slope">(warmFit), warm,
                                                        formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 30 }).find("slope = 33/200000 m/K") != std::string::npos);
}
```

- [ ] **Step 2: Run and fail.** `cl.ps1 -Filter "least squares|observation"`. Expected: `BUILD FAILED` --
  `linear_least_squares(observations<...>, observations<...>, {...})` reaches the two-series refusal: "formula:
  linear_least_squares fits a curve; pair the domain and the values with curve(domain, values)", and
  `LinearLeastSquaresOfObservations` is undeclared.
- [ ] **Step 3: The refused stand-in.** In `observations.hpp`, `detail::RefusedObservations` gains, after
  `capacity`:

```cpp
        /// Refused already (`detail::refused_already`): a call that reads
        /// these in place of observations it refused asks nothing more.
        static constexpr RefusedFlag refused = true;
```

  and its comment says it stands in for a binning's or a fit's operand. `BinnedNode::refused` is unchanged
  (`binning.hpp:207` decides by type), and every walk that could meet it meets it under a node refused already.
- [ ] **Step 4: The operation.** In `least_squares.hpp`, include `detail/least_squares_kernel.hpp` and
  `observations.hpp`; after `LinearLeastSquares`:

```cpp
/// Ordinary least squares over raw observations: `y = intercept + slope x`,
/// with `x` the first input's observations and `y` the second's, paired row
/// by row -- observation i of each belongs to row i, so both are built from
/// the same rows. How many there are is data. Besides the coefficients it
/// reports `r squared`, the coefficient of determination, S_xy^2 / (S_xx
/// S_yy), and `points`, the number of observations it fitted, exact in
/// every representation -- for degrees of freedom `n - 2`, say.
///
/// Decided exactly, on the observations themselves and before any sum, in
/// every `Rep`, each the fit's own `DomainError`: both inputs hold as many
/// observations, at least two, the points are not all equal, and the values
/// are not all equal -- R^2 would be 0/0, so a flat response never passes an
/// R^2 acceptance.
///
/// Three routes. `compute_exact`, which `rounded_output` rounds, is the exact
/// kernel (`detail/least_squares_kernel.hpp`) in wide integers.
/// `compute<Rational>`, behind `opaque_output`, is the same kernel with every
/// output narrowed to a `Rational`: all four answer, or all fail with
/// `Overflow` when one does not fit. `compute<double>`, behind
/// `checked_evaluate_si<double>`, is the kernel's approximate route, untraced.
struct LinearLeastSquaresOfObservations
{
    /// What the trace calls it: the curve fit's name, since it fits the same line.
    static constexpr std::string_view name = "linear least squares";
    /// Two sets of raw observations: the points, then the values.
    static constexpr std::array shapes { InputShape::Observations, InputShape::Observations };
    /// The outputs, in this order.
    static constexpr std::array<std::string_view, 4> outputs { "intercept", "slope", "r squared", "points" };
    /// The exact kernel's width for one regressor.
    static constexpr std::size_t exact_limbs = detail::regression_limbs(1);

    /// `intercept` in the values' dimension, `slope` in the values' over the
    /// points', `r squared` and `points` bare numbers. Any two dimensions are
    /// accepted.
    static consteval std::optional<std::array<Dimension, 4>> output_dimensions(
        std::array<Dimension, 2> pointsAndValues) noexcept
    {
        return std::array { pointsAndValues[1], pointsAndValues[1] / pointsAndValues[0], dim::Scalar, dim::Scalar };
    }

    /// The fit, exactly, as wide fractions in coherent units.
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 4>, ArithmeticError> compute_exact(
        std::span<Rational const> pointObservations, std::span<Rational const> valueObservations) noexcept
    {
        return detail::exact_regression<1>(std::array { pointObservations }, valueObservations);
    }

    /// The fit in coherent units: exactly, every output a `Rational` or all
    /// `Overflow`, for `Rep = Rational`; approximately otherwise.
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 4>, ArithmeticError> compute(std::span<Rep const> pointObservations,
                                                                                std::span<Rep const> valueObservations) noexcept
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            std::expected<std::array<detail::WideRatio<exact_limbs>, 4>, ArithmeticError> const exact =
                compute_exact(pointObservations, valueObservations);
            if (!exact.has_value())
                return std::unexpected { exact.error() };
            return detail::narrowed_all(*exact);
        }
        else
            return detail::approximate_regression<Rep, 1>(std::array { pointObservations }, valueObservations);
    }
};
```

- [ ] **Step 5: The overloads and refusals.** In `detail` (after `RequireFitCitation`, `:217`):

```cpp
    /// The quantity @p T observes, or @p T itself for observations refused
    /// already, which observe none.
    template <typename T>
    struct ObservedQuantity
    {
        using type = T;
    };

    template <typename T>
        requires requires { typename T::quantity; }
    struct ObservedQuantity<T>
    {
        using type = typename T::quantity;
    };

    /// Fails to compile when a line through observations reads one quantity
    /// as its points and its values. Named so the quantity prints.
    template <typename Q>
    struct RequireFitOfTwoQuantities
    {
        static_assert(!std::is_same_v<Q, Q>,
                      "formula: linear_least_squares reads one quantity as both its points and its values, and a "
                      "line through every observation against itself says nothing; read the points and the values "
                      "as two quantities -- the quantity appears in this diagnostic as the template argument Q of "
                      "RequireFitOfTwoQuantities");

        static constexpr bool value = true;
    };

    /// What a refused line through observations returns: the operation over
    /// refused observations, so that each of its four outputs can be taken and
    /// evaluated and asks nothing again.
    using RefusedObservationFit = OpaqueCall<LinearLeastSquaresOfObservations, RefusedObservations, RefusedObservations>;

    [[nodiscard]] constexpr RefusedObservationFit refused_observation_fit(Citation citation) noexcept
    {
        return RefusedObservationFit { std::tuple<RefusedObservations, RefusedObservations> {}, citation };
    }
```

  `RequireFitOfCurve`'s message becomes, verbatim:
  `"formula: linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a value series with curve(domain, values), or read both with observations<Q, Capacity>"`.
  After the curve's `linear_least_squares` (`:188-192`):

```cpp
/// A straight line through raw observations, paired row by row, for the
/// reason @p citation gives: `linear_least_squares(observations<Elapsed, 64>,
/// observations<Length, 64>, { ... })`. Its outputs are `intercept`, `slope`,
/// `r squared` and `points`. One quantity read as both is refused in this
/// library's words.
template <ObservationsNode X, ObservationsNode Y>
[[nodiscard]] constexpr auto linear_least_squares(X pointObservations, Y valueObservations, Citation citation) noexcept
{
    using PointsQuantity = typename detail::ObservedQuantity<X>::type;
    constexpr bool oneQuantity = std::is_same_v<PointsQuantity, typename detail::ObservedQuantity<Y>::type>;
    static_assert(std::conditional_t<oneQuantity, detail::RequireFitOfTwoQuantities<PointsQuantity>, std::true_type>::value);
    if constexpr (oneQuantity)
        return detail::refused_observation_fit(citation);
    else
        return opaque<LinearLeastSquaresOfObservations>(citation, pointObservations, valueObservations);
}

/// Raw observations handed to `linear_least_squares` without a citation:
/// refused in this library's words, as a curve without one is.
template <ObservationsNode X, ObservationsNode Y>
[[nodiscard]] constexpr auto linear_least_squares(X, Y) noexcept
{
    static_assert(detail::RequireFitCitation<X>::value);
    return detail::refused_observation_fit(Citation {});
}
```

  The existing refusals: `linear_least_squares(NotCurve, Citation)` (`:264-270`) returns
  `detail::refused_observation_fit(citation)` when `ObservationsNode<NotCurve>`, and the curve stand-in otherwise;
  `linear_least_squares(Domain, Values, Citation)` (`:275-280`) gains
  `requires(!(ObservationsNode<Domain> && ObservationsNode<Values>))` and returns the observation stand-in when
  either is an `ObservationsNode`, so that `opaque_output<"r squared">` over a mixed call draws no second message.
  Rewrite the file comment (`:4-50`): the operation list gains the observation fit and its R² and points; the last
  paragraph ("Nothing else is offered: ... no residuals or coefficient of determination") becomes "Nothing else is
  offered: no line through the origin, no weights, no residuals; R² only through raw observations, whose fit is a
  separate operation with its own pinned lines."
- [ ] **Step 6: Run and pass** (+11 cases). `cl.ps1 -Filter "least squares|observation|kernel"`, `gcc14.sh "least squares|observation"`,
  then both unfiltered. Expected `ALL OK`.
- [ ] **Step 7: Negative tests.** Each with the invented `Elapsed`, `Length` and `Rate` of
  `least_squares_not_a_curve.cpp`, a two-observation environment, and a `main` that takes `slope` **and**
  `r squared` and evaluates the slope, so that a second message would show. Register after
  `least_squares_in_double` (`test/CMakeLists.txt:2203-2205`):

```cmake
formula_add_negative_test(least_squares_not_a_curve
    "formula: linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a value series with curve(domain, values), or read both with observations<Q, Capacity>"
    REJECT "no matching overloaded function" "no matching function" "RequireResultDimension")
formula_add_negative_test(least_squares_observations_uncited
    "formula: linear_least_squares needs a citation, the reason the method fits a line here; pass {} when it gives none"
    REJECT "no matching overloaded function" "no matching function" "RequireResultDimension" "has no output of that name")
formula_add_negative_test(least_squares_observations_and_series
    "formula: linear_least_squares fits a curve, or points and values read as observations"
    REJECT "no matching overloaded function" "no matching function" "RequireResultDimension" "has no output of that name")
formula_add_negative_test(least_squares_observations_same_quantity
    "formula: linear_least_squares reads one quantity as both its points and its values"
    REJECT "RequireResultDimension" "has no output of that name" "passes an input of another shape")
formula_add_negative_test(overlay_constant_inside_observation_fit
    "formula: this overlay fixes a quantity the method reads as a series; one constant cannot stand for a series"
    REJECT "cannot see inside" "overrides a quantity that no variant or constraint of the method uses")
```

  (the first replaces the existing registration; also update `least_squares_not_a_curve.cpp`'s `// EXPECT:` line;
  +4 negative tests here, +1 in Step 8).
  `least_squares_observations_uncited`: `linear_least_squares(observations<Elapsed, 4>, observations<Length, 4>)`.
  `least_squares_observations_and_series`: `(observations<Elapsed, 4>, series<Length, 4>, {...})`.
  `least_squares_observations_same_quantity`: `(observations<Length, 4>, observations<Length, 4>, {...})`.
  `overlay_constant_inside_observation_fit`: `overlay_constant_inside_opaque_series.cpp` with the fit over
  `observations<Elapsed, 4>, observations<Length, 4>`. Deletion checks: the new overload's `RequireFitCitation`
  line, `RequireFitOfCurve`'s `static_assert`, and the `conditional_t` line of `RequireFitOfTwoQuantities` -- each
  case then compiles. For the overlay case delete `RequireConstantNotSeries`'s `static_assert` (`overlay.hpp`,
  near `:2530`): the expected text must disappear; report whether the case then compiles or fails on the
  `REJECT`ed "overrides a quantity" text (the `REJECT` exists for the latter).
- [ ] **Step 8: Pin the calculation refusal.** `test/negative/calculation_definition_reads_fit_observations.cpp`,
  modelled on `calculation_definition_reads_observations.cpp`: `define<Rate>(opaque_output<"slope">(
  linear_least_squares(observations<Elapsed, 4>, observations<Length, 4>, { .reference = "Example Standard 12" })))`.
  Comment: "A definition that reads a line through raw observations: the calculation walk reaches the call's inputs
  and refuses each quantity read as observations, once each -- two quantities, two messages. This is the walk's
  existing behaviour, not this fit's: a definition over a curve fit of two series draws two messages as well. It
  is pinned as it is; a walk that stops after its first refused read would make this one message." Register after
  `calculation_definition_reads_observations`:

```cmake
formula_add_negative_test(calculation_definition_reads_fit_observations
    "formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations"
    EXPECT_COUNT 2
    REJECT "formula: this definition holds a node kind the calculation cannot see inside"
           "has no detail::LevelChildren specialisation"
           "no matching")
```

  Measured for this plan with g++ 14.2 on the curve-fit twin: two messages. If g++-14 shows a different count for
  this case, report it rather than changing `EXPECT_COUNT` to fit. Deletion check: as for
  `calculation_definition_reads_observations`.
- [ ] **Step 9: Consumer globals.** Add `observation, observations, pivot, design, coefficient, coefficients` to the
  globals (`test/consumer_globals_tests.cpp:144`, before the `;`). In the probe, after the curve fit (`:796-803`),
  and naming the entry points in the file comment's list ("a line through raw observations, exactly, rounded and in
  double"):

```cpp
    // A line through raw observations: edges of 103, 163 and 241 mm against
    // twice each plus 1 mm, 207, 327 and 483 mm -- slope 2, R^2 1, exactly,
    // rounded and in double.
    std::array<formula::Rational, 3> const agreedReadings { formula::Rational { 207 }, formula::Rational { 327 },
                                                            formula::Rational { 483 } };
    auto const lineSample =
        formula::environment(*edgeObserved, *formula::MeasuredObservations<AgreedEdge, 4>::from(agreedReadings));
    constexpr auto edgeLine = formula::linear_least_squares(formula::observations<EdgeX, 4>,
                                                            formula::observations<AgreedEdge, 4>,
                                                            { .reference = "Example Standard 3" });
    auto const lineSlope = formula::checked_evaluate<Factor>(formula::opaque_output<"slope">(edgeLine), lineSample);
    auto const lineFit = formula::checked_evaluate<Factor>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(
            edgeLine),
        lineSample);
    auto const lineInDouble = formula::checked_evaluate_si<double>(formula::opaque_output<"slope">(edgeLine), lineSample);
    probe.checks.push_back(lineSlope.has_value() && lineSlope->measurement().value() == formula::Rational { 2 }
                           && lineFit.has_value() && lineFit->measurement().value() == formula::Rational { 1 }
                           && lineInDouble.has_value() && lineInDouble->has_value() && **lineInDouble > 1.999
                           && **lineInDouble < 2.001);
```

  and raise `REQUIRE(probe.checks.size() == ...)` in `consumer_globals_run_tests.cpp` by +1 probe check. cl reports C4459 only in what the probe instantiates, so this line is what guards the kernel's names.
- [ ] **Step 10: CHANGELOG.** `### Added`:

```markdown
- `linear_least_squares(observations<X, C>, observations<Y, C2>, citation)` fits a straight line through raw
  observations, paired row by row, whose number is data. Its outputs are `intercept`, `slope`, `r squared` -- the
  coefficient of determination, dimensionless -- and `points`, the number of observations fitted. Decided before
  any sum, in every representation, and each the fit's own `DomainError`: both inputs hold as many observations, at
  least two, the points are not all equal, and the values are not all equal, so a flat response never passes an R²
  acceptance. Exact through `opaque_output`, where the four outputs answer or all fail with `Overflow` when one
  does not fit a `Rational`; correctly rounded at any size through `rounded_output`; approximately, and untraced,
  through `checked_evaluate_si<double>`. One quantity read as both points and values, or observations without a
  citation, is refused where it is written.
```

  `### Changed`:

```markdown
- `linear_least_squares` given anything but a curve or two sets of observations says "formula:
  linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a value
  series with curve(domain, values), or read both with observations<Q, Capacity>", which names both ways to call it.
```

- [ ] **Step 11: Commit.**

```text
feat(least_squares): a line through raw observations, with R² and the number of points

Methods fit every determination that meets a condition, report R² and
accept a fit by it. The fit over observations pairs two sets by row and
reports R² and its number of points; it refuses a flat response rather
than dividing 0 by 0, so an R² acceptance cannot pass one. Exact results
that outgrow Rational are Overflow as a whole, and rounded_output
answers at any size from the wide kernel.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 5: A line through observations in the example, the guide and the census

**Files:**
- Modify: `examples/opaque_and_retry.cpp` (file comment `:3-22`, declarations before `:230`, `main` before
  `:374`), `examples/CMakeLists.txt:266-274` (pinned regex and its comment), `docs/opaque-and-retry.md` (new
  section after "A least-squares line", `:168-223`, before "A citation is required", `:225`; the
  no-traced-fallback paragraph `:218-223`), `test/overflow_census_tests.cpp` (after `:764`),
  `cmake/CheckCensusPage.cmake` (`:4-12`, `:69-72`, `:183-190`), `docs/numeric-headroom.md` (regenerated; new
  section after `:324`; "The answer" `:16-31`; "Which cases decide" `:326-336`), `docs/statistics.md` (after
  `:75`), `README.md:271,299`, `docs/index.md:77,115`

**Interfaces:**
- Consumes: Task 4's fit, the foundation's `rounded_output`, `number_text` (`number_text.hpp`).
- Produces: example output lines quoted by the guide; a census table `census:regression`; no library change.

- [ ] **Step 1: The example.** Add item 7 to the file comment's list ("7. A line through raw observations, whose
  number is data: exact, with R² and the number of points, and rounded where used when the exact fractions no
  longer fit."). Declarations, after section 6's (before the anonymous namespace closes at `:230`):

```cpp
// ---- 7. A line through observations -------------------------------------------------

// A unit the method states its slope in.
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
using SlopeRate = formula::Quantity<struct SlopeRateTag, "v_s", "an invented slope", millimetrePerSecond>;
using StartLength = formula::Quantity<struct StartLengthTag, "L_0", "an invented starting length", unit::Millimetre>;
using FitQuality = formula::Quantity<struct FitQualityTag, "R2", "an invented coefficient of determination", unit::One>;

constexpr auto observedFit = formula::linear_least_squares(
    formula::observations<Elapsed, 64>,
    formula::observations<Length, 64>,
    { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto observedSlope = formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 },
                                                       formula::RoundingMode::HalfEven>(observedFit);
constexpr auto closeEnough = formula::constraint(
    formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(observedFit)
        >= formula::constant<unit::One>(formula::Rational { 998, 1000 }),
    formula::Verdict { "repeat the readings" });

constexpr auto observedPoints = formula::environment(
    formula::MeasuredObservations<Elapsed, 64>(formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 4 },
                                               formula::Rational { 7 }),
    formula::MeasuredObservations<Length, 64>(formula::Rational { 102, 10 }, formula::Rational { 109, 10 },
                                              formula::Rational { 121, 10 }, formula::Rational { 143, 10 }));

/// Fifty readings at four decimals: t = k + 1 + (7919 k mod 997) / 10^4 s and
/// L = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm, for k from 0.
auto fiftyReadings()
{
    std::array<formula::Rational, 50> times;
    std::array<formula::Rational, 50> lengths;
    for (std::size_t k = 0; k < 50; ++k)
    {
        auto const position = static_cast<std::int64_t>(k);
        times[k] = formula::Rational { 10'000 * (position + 1) + (7919 * position) % 997, 10'000 };
        lengths[k] = formula::Rational { 24'100'000 + 31'700 * position + (3217 * position) % 1009 - 504, 10'000 };
    }
    return formula::environment(*formula::MeasuredObservations<Elapsed, 64>::from(times),
                                *formula::MeasuredObservations<Length, 64>::from(lengths));
}

/// @p shown as its exact decimal, with its unit.
template <typename Q>
std::string decimalText(formula::Measured<Q> const& shown)
{
    // Held first: `view()` of a temporary is deleted, since the view would dangle.
    formula::NumberText const spelled = formula::number_text(shown, formula::NumberStyle::exact_decimal());
    return std::string { spelled.view() };
}
```

  In `main`, before `all checks passed`:

```cpp
    std::printf("== 7. A line through observations ==\n\n");

    auto const exactLine = formula::explain<Rate>(formula::opaque_output<"slope">(observedFit), observedPoints);
    std::printf("%s\n", formula::render_trace(exactLine.trace, { .maxSteps = 30 }).c_str());
    check(exactLine.outcome.measurement().value() == formula::Rational { 285, 7 }, "19/28 mm/s through observations");

    std::printf("%s\n", formula::render(observedSlope).c_str());
    auto const roundedLine = formula::explain<SlopeRate>(observedSlope, observedPoints);
    std::printf("%s\n", formula::render_trace(roundedLine.trace, { .maxSteps = 30 }).c_str());
    check(roundedLine.outcome.measurement().value() == formula::Rational { 3393, 5000 }, "0.6786 mm/s");

    bool const fitAccepted = formula::check(closeEnough, observedPoints).is_satisfied();
    std::printf("r squared at 4 dp, floored, at least 0.998: %s\n", fitAccepted ? "satisfied" : "not satisfied");
    check(fitAccepted, "0.9981 is at least 0.998");

    constexpr auto flatLengths = formula::environment(
        formula::MeasuredObservations<Elapsed, 64>(formula::Rational { 1 }, formula::Rational { 2 },
                                                   formula::Rational { 4 }, formula::Rational { 7 }),
        formula::MeasuredObservations<Length, 64>(formula::Rational { 127, 10 }, formula::Rational { 127, 10 },
                                                  formula::Rational { 127, 10 }, formula::Rational { 127, 10 }));
    auto const flatLine = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(observedFit), flatLengths);
    std::printf("flat lengths: %s\n",
                flatLine.has_value() ? "a line" : std::string { formula::describe(flatLine.error()) }.c_str());
    check(!flatLine.has_value() && flatLine.error() == formula::ArithmeticError::DomainError, "a flat response has no R²");

    auto const fifty = fiftyReadings();
    auto const exactFifty = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(observedFit), fifty);
    std::printf("fifty readings at 4 decimals, exact: %s\n",
                exactFifty.has_value() ? "a line" : std::string { formula::describe(exactFifty.error()) }.c_str());
    auto const slopeOfFifty = formula::checked_evaluate<SlopeRate>(observedSlope, fifty);
    auto const startOfFifty = formula::checked_evaluate<StartLength>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            observedFit),
        fifty);
    auto const qualityOfFifty = formula::checked_evaluate<FitQuality>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
            observedFit),
        fifty);
    check(slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value(), "fifty readings, rounded");
    if (slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value())
        std::printf("fifty readings at 4 decimals, rounded: slope %s, intercept %s, r squared %s\n\n",
                    decimalText(slopeOfFifty->measurement()).c_str(), decimalText(startOfFifty->measurement()).c_str(),
                    decimalText(qualityOfFifty->measurement()).c_str());
    check(slopeOfFifty.has_value() && slopeOfFifty->measurement().value() == formula::Rational { 31707, 10'000 },
          "3.1707 mm/s");
```

  Expected output (derived; confirm by running): the exact trace

```text
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. linear least squares(#1, #2) = intercept = 19/2 mm; slope = 19/28 mm/s; r squared = 1083/1085; points = 4 [inside not shown] [Rate of change, Example Standard 12, 5.1]
4. slope of #3 = 19/28 mm/s
```

  the rounded formula and trace in the foundation's spelling (`round(linear least squares(t(i), L(i)).slope, to 4
  dp of mm/s)`; step 3 `... = intercept, slope, r squared, points: rounded where used [inside not shown] [...]`,
  step 4 `round(slope of #3, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]`), then

```text
r squared at 4 dp, floored, at least 0.998: satisfied
flat lengths: argument outside the domain of the operation
fifty readings at 4 decimals, exact: overflow in exact arithmetic
fifty readings at 4 decimals, rounded: slope 3.1707 mm/s, intercept 2406.6455 mm, r squared 0.999996
```

- [ ] **Step 2: Pin the example.** In `examples/CMakeLists.txt:274`, insert before `.*all checks passed: yes`
  (no literal `;`, so `.` stands for one):
  `.*linear least squares\(#1, #2\) = intercept = 19/2 mm. slope = 19/28 mm/s. r squared = 1083/1085. points = 4 \[inside not shown\].*rounded where used.*r squared at 4 dp, floored, at least 0\.998: satisfied.*flat lengths: argument outside the domain of the operation.*fifty readings at 4 decimals, exact: overflow in exact arithmetic.*fifty readings at 4 decimals, rounded: slope 3\.1707 mm/s, intercept 2406\.6455 mm, r squared 0\.999996`,
  and extend the comment above it ("...; a line through observations, exact and rounded where used, accepted by its
  R², refused for a flat response, and fifty readings that overflow exactly and answer rounded").
  `cl.ps1 -Filter "example\.opaque_and_retry"` must pass; run it once with a deliberately wrong digit in the
  pattern first and watch it fail.
- [ ] **Step 3: The guide.** A new section in `docs/opaque-and-retry.md`, `## A line through observations`, after
  the least-squares section's last paragraph (`:223`). Its content, in this order, each block copied from the
  example's real source or output:
  1. Why: "A method often fits every determination that meets a condition, so how many points there are is data.
     Read them as raw observations, `observations<Q, Capacity>` ([statistics](statistics.md) introduces them), and
     the fit takes as many as were made." The `observedFit` declaration (cpp).
  2. **Pairing is by row.** "Observation i of each input belongs to row i, so build every column from the same
     rows. Inputs whose counts differ make the fit fail with its own `DomainError`; so do fewer than two
     observations, points all equal, and values all equal." One sentence on why flat values are refused: R² would
     be 0/0, so a flat response never passes an R² acceptance -- the "flat lengths" output line.
  3. **Four outputs.** `intercept`, `slope`, `r squared` (S_xy² / (S_xx S_yy), dimensionless), `points` (the count,
     exact in every representation; "degrees of freedom are `points` minus two"). The exact trace block (4 lines).
  4. **When the exact fractions do not fit.** "An exact fit through fifty readings at four decimals needs integers
     of over a hundred bits. `opaque_output` then answers `Overflow` -- for every output of the call, since its
     outputs answer or fail together. A formula that declares the precision it reports a coefficient at -- a unit,
     decimal places and a rounding mode, as `rounded<>` does -- gets the correctly rounded decimal at any size:"
     the `observedSlope` declaration (cpp), its rendering and trace (text), and the two "fifty readings" lines
     (text). Link [Displaying numbers](display.md) where it explains values the exact layer cannot hold, if the
     foundation's section is there by that name.
  5. **R² as an acceptance.** The `closeEnough` declaration (cpp) and its output line; one sentence that
     `RoundingMode::Floor` makes the rounding never lift a fit over the line.
  6. **In `double`.** "`checked_evaluate_si<double>` fits observations approximately, in coherent units, with
     nothing traced; it is the exploratory route."
  7. **A temperature in degrees Celsius.** "The fit sees coherent units, so a regressor in degrees Celsius is fitted
     in kelvin: the slope per kelvin is the slope per degree Celsius, but the intercept is the value at 0 K. The
     value at 0 °C is the intercept plus the slope times 273.15 K, written as a formula over the two outputs."
  Update the no-traced-fallback paragraph (`:218-223`, as the foundation left it) so it says that a fit over
  observations is also evaluated in `double`, untraced, and that a declared precision answers the exact route's
  overflow.
- [ ] **Step 4: The census.** In `test/overflow_census_tests.cpp`, after the least-squares case (`:764`):

```cpp
// ---- Regression over observations: where each route stops -------------------------

// Declared here: the library states neither unit.
inline constexpr formula::Unit NewtonPerSecond { .dimension = formula::dim::Force / formula::dim::Time,
                                                 .symbolText = formula::symbol("N/s"),
                                                 .decimals = 4 };
inline constexpr formula::Unit MillimetrePerSecond { .dimension = formula::dim::Velocity,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 1000,
                                                     .symbolText = formula::symbol("mm/s"),
                                                     .decimals = 4 };

/// Four decimal places, the guide's fifty readings extended: x = k + 1 + (7919k
/// mod 997) / 10^4 s, y = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm.
[[nodiscard]] FitPoint four_decimals_point(std::int64_t k)
{
    return { rat(10'000 * (k + 1) + (7919 * k) % 997, 10'000), rat(24'100'000 + 31'700 * k + (3217 * k) % 1009 - 504, 10'000) };
}

/// Whether the line through the first @p count points of @p shape overflows
/// through `opaque_output` (the slope, exactly) and through `rounded_output`
/// (the slope at 4 dp of @p SlopeUnit, R^2 floored at 6 dp), with the values
/// read as observations of @p Y.
template <typename Y, formula::Unit SlopeUnit, typename Shape>
[[nodiscard]] std::pair<bool, bool> observed_line_overflows(Shape shape, std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> readings;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        readings.push_back(point.y);
    }
    auto const inputs = formula::environment(*formula::MeasuredObservations<FitTime, 128>::from(times),
                                             *formula::MeasuredObservations<Y, 128>::from(readings));
    constexpr auto fit = formula::linear_least_squares(formula::observations<FitTime, 128>,
                                                       formula::observations<Y, 128>,
                                                       { .reference = "Example Standard 12" });
    auto const overflowed = [](auto const& evaluated) {
        return !evaluated.has_value() && evaluated.error() == formula::ArithmeticError::Overflow;
    };
    bool const exact = overflowed(formula::checked_evaluate_si<Rational>(formula::opaque_output<"slope">(fit), inputs));
    bool const rounded =
        overflowed(formula::checked_evaluate_si<Rational>(
            formula::rounded_output<"slope", SlopeUnit, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit),
            inputs))
        || overflowed(formula::checked_evaluate_si<Rational>(
            formula::rounded_output<"r squared", formula::unit::One, formula::DecimalPlaces { 6 },
                                    formula::RoundingMode::Floor>(fit),
            inputs));
    return { exact, rounded };
}

/// Over every size from @p from to 128: which overflowed on each route.
struct RouteScan
{
    std::size_t sizes = 0;
    std::vector<std::size_t> exactOverflowing;
    std::vector<std::size_t> roundedOverflowing;

    [[nodiscard]] std::string row(char const* label) const
    {
        auto const first = [](std::vector<std::size_t> const& overflowing) {
            return overflowing.empty() ? std::string { "none" } : std::to_string(overflowing.front()) + " points";
        };
        return "| " + std::string { label } + " | " + std::to_string(exactOverflowing.size()) + " of "
               + std::to_string(sizes) + " | " + first(exactOverflowing) + " | "
               + std::to_string(roundedOverflowing.size()) + " of " + std::to_string(sizes) + " | "
               + first(roundedOverflowing) + " |";
    }
};

template <typename Overflows>
[[nodiscard]] RouteScan scan_routes(std::size_t from, Overflows overflows)
{
    RouteScan found;
    for (std::size_t count = from; count <= 128; ++count)
    {
        ++found.sizes;
        auto const [exact, rounded] = overflows(count);
        if (exact)
            found.exactOverflowing.push_back(count);
        if (rounded)
            found.roundedOverflowing.push_back(count);
    }
    return found;
}
```

  (Inside the file's anonymous namespace; `FitPoint`, `three_decimals_point`, `distinct_denominators_point`,
  `FitTime`, `FitForce`, `FitLength` are at `:318-402`. Local names such as `count` and `first` are fine in a test
  file.) The case:

```cpp
TEST_CASE("census: a line through observations, exact and rounded, over 2 to 128 points", "[census]")
{
    RouteScan const threeDecimals = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitForce, NewtonPerSecond>(three_decimals_point, count);
    });
    RouteScan const fourDecimals = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitLength, MillimetrePerSecond>(four_decimals_point, count);
    });
    RouteScan const distinct = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitForce, NewtonPerSecond>(distinct_denominators_point, count);
    });
    emit("regression",
         "| data (invented) | exact route: sizes that overflow | first | rounded route: sizes that overflow | first |");
    emit("regression", "|---|---|---|---|---|");
    emit("regression", threeDecimals.row("a line through readings at 3 dp near 2410 N (realistic)"));
    emit("regression", fourDecimals.row("a line through readings at 4 dp near 2410 mm (realistic)"));
    emit("regression", distinct.row("a line through a different denominator on every point (stress control)"));

    // What the page says, pinned: the realistic rows never stop on the
    // rounded route; the exact route stops early.
    CHECK(threeDecimals.roundedOverflowing.empty());
    CHECK(fourDecimals.roundedOverflowing.empty());
    REQUIRE(!threeDecimals.exactOverflowing.empty());
    CHECK(threeDecimals.exactOverflowing.front() == 29);
    REQUIRE(!fourDecimals.exactOverflowing.empty());
    CHECK(fourDecimals.exactOverflowing.front() == 7);
    REQUIRE(!distinct.roundedOverflowing.empty());
    CHECK(distinct.roundedOverflowing.front() == 62);
}
```

  The pinned figures are the model's predictions (the plan's table). Run the census once and compare:
  - **A realistic row that overflows on the rounded route is a blocker.** It contradicts the width estimate: stop,
    do not pin it, and report the row, the size and where the kernel overflowed.
  - **A different first exact-route size** means the kernel or the model computes something else: investigate,
    report both figures, then pin the measured one.
  - **A shifted stress-row size on the rounded route is expected** (it depends on how wide the rounding's own
    intermediates are, which the model only approximates): pin the measured size and say so in the report.
  `sizes` makes the "of N" honest (127 for 2..128).
- [ ] **Step 5: The page and its script.** `cmake/CheckCensusPage.cmake`: `census_block(regression
  regressionTable)` after `:72`, `replace_block(regression "${regressionTable}")` after `:186`, and the header
  comment's list (`:6-7`) names `regression`. In `docs/numeric-headroom.md`, after the least-squares section
  (`:324`), a section `### Regression over observations (realistic, and one stress control)`: two paragraphs --
  what the rows fit (the census's own shapes, read as observations of up to 128) and that the columns count sizes,
  not bits, because the wide kernel does not report to the tally; then the block

```markdown
<!-- census:regression -->

<!-- /census:regression -->
```

  and after it: "**The exact route stops early; the rounded route does not stop on realistic data.** A call's
  outputs answer or fail together, and R²'s exact fraction is about twice as wide as the slope's. Reported at
  declared decimals, the same fits answer at every size measured. A different denominator on every point outgrows
  even the wide kernel, and is `Overflow`." Add to "The answer" a sentence after its list: "A line through
  observations reported at declared decimals answers at every size measured below; its exact route stops sooner."
  Regenerate on cl: `cl.ps1 -Target formula-cpp-census-page -NoTest`, then `cl.ps1 -Filter "docs\.numeric-headroom"`.
  The examples table's `opaque_and_retry` row may change with Step 1; the regenerated page is the truth.
- [ ] **Step 6: Statistics, README, index.** `docs/statistics.md`, after the "None made is an empty sample"
  paragraph (`:72-75`): "Observations also feed fits: a line through two sets of observations, paired by row,
  reports its number of points with its coefficients ([Opaque operations and bounded retry](opaque-and-retry.md))."
  README and `docs/index.md` feature tables (`README.md:299`, `docs/index.md:115`): a row
  `| Least squares over observations, with R², and several regressors | next release |`; the guide rows
  (`README.md:271`, `docs/index.md:77`) name "a least-squares line through a curve or raw observations, with R²".
- [ ] **Step 7: Run** (+1 census case). Both builds unfiltered: `docs.opaque-and-retry-output`, `docs.opaque-and-retry-snippets`,
  `docs.numeric-headroom`, `example.opaque_and_retry` and `census.*` among the passes. On g++-14 the census page's
  examples table is held to at least cl's headroom (`CheckCensusPage.cmake:145-176`).
- [ ] **Step 8: Commit.**

```text
docs(least_squares): a line through observations in the guide, the example and the census

The guide shows the fit over observations exactly and rounded where
used, its R² as an acceptance, the flat-response refusal, and fifty
readings that overflow exactly and answer rounded. The census measures
where each route stops over 2 to 128 points: the exact route early,
since a call's outputs fail together, the rounded route not at all on
realistic data.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 6: Multiple regression over observations, refusing a singular design

**Files:**
- Modify: `include/formula-cpp/least_squares.hpp` (after Task 4's additions), `test/CMakeLists.txt` (sources;
  negatives after Task 4's), `test/consumer_globals_tests.cpp`, `test/consumer_globals_run_tests.cpp`,
  `CHANGELOG.md`
- Create: `test/multiple_least_squares_tests.cpp`, `test/regression_cross_tu.hpp`,
  `test/regression_cross_tu_b.cpp`, eight `test/negative/multiple_least_squares_*.cpp`

**Interfaces:**
- Consumes: Task 3's `detail::exact_regression<K>`, `detail::approximate_regression<Rep, K>`,
  `detail::narrowed_all`, `detail::regression_limbs`, `detail::maxRegressors`; Task 4's `detail::ObservedQuantity`,
  `detail::RefusedObservations::refused`.
- Produces:

```cpp
template <std::size_t K> requires(K >= 1 && K <= detail::maxRegressors)
struct MultipleLeastSquares
{
    static constexpr std::string_view name = "multiple least squares";
    static constexpr std::array<InputShape, K + 1> shapes;              // every one InputShape::Observations
    static constexpr std::array<std::string_view, K + 3> outputs;      // "constant", "coefficient 1" ... "coefficient K", "r squared", "points"
    static constexpr std::size_t exact_limbs = detail::regression_limbs(K);
    static consteval std::optional<std::array<Dimension, K + 3>> output_dimensions(std::array<Dimension, K + 1>) noexcept;
    template <typename... Columns> requires(sizeof...(Columns) == K + 1 && (std::same_as<Columns, std::span<Rational const>> && ...))
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, K + 3>, ArithmeticError> compute_exact(Columns...) noexcept;
    template <typename Rep, typename... Columns> requires(sizeof...(Columns) == K + 1 && (std::same_as<Columns, std::span<Rep const>> && ...))
    static constexpr std::expected<std::array<Rep, K + 3>, ArithmeticError> compute(Columns...) noexcept;
};
template <typename... Xs> struct Regressors { std::tuple<Xs...> inputs; };
template <typename... Xs> constexpr Regressors<Xs...> regressors(Xs... regressorInputs) noexcept;
template <typename... Xs, typename Y> constexpr auto multiple_least_squares(Regressors<Xs...>, Y, Citation) noexcept;
template <typename... Xs, typename Y> constexpr auto multiple_least_squares(Regressors<Xs...>, Y) noexcept;       // uncited: refused
template <typename X, typename Y> requires(!detail::isRegressors<X>)
constexpr auto multiple_least_squares(X, Y, Citation) noexcept;                                                   // unwrapped: refused
```

  `multiple_least_squares(regressors(x1, ..., xK), y, citation)` returns
  `OpaqueCall<MultipleLeastSquares<K>, X1, ..., XK, Y>`; every refusal returns
  `detail::refused_regression(citation)`, an `OpaqueCall<MultipleLeastSquares<8>, ...>` over nine refused observations, whose outputs up to
  `coefficient 8` can all be taken and evaluated without a second message.

**A deliberate deviation from the spec's wording.** The spec generates the output names into static storage of the class template. There are at most eight, so they
are written out as string literals in one `inline constexpr` table instead, which every K shares: nothing is
generated, and no string view points into a class template's storage in a constant expression -- the risk the spec
names disappears. The cross-translation-unit test is kept; it guards the positions and texts the names have in two
units.

- [ ] **Step 1: Failing tests.** `test/multiple_least_squares_tests.cpp` (added to `formula-cpp-tests` with
  `regression_cross_tu_b.cpp`):

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// Multiple regression over raw observations. Every expected value was
// computed with Python's fractions from the data beside it.
#include <formula-cpp/document.hpp>
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "regression_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace unit = formula::unit;
using regression_cross_tu::Content;
using regression_cross_tu::Length;
using regression_cross_tu::Temperature;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", unit::Second>
{
};
struct Delay: formula::Quantity<Delay, "t_d", "an invented delay", unit::Second>
{
};
struct Offset: formula::Quantity<Offset, "L_0", "an invented starting length", unit::Millimetre>
{
};
inline constexpr formula::Unit MillimetrePerPercent { .dimension = formula::dim::Length,
                                                      .magnitudeNumerator = 1,
                                                      .magnitudeDenominator = 10,
                                                      .symbolText = formula::symbol("mm/%"),
                                                      .decimals = 4 };
struct PerContent: formula::Quantity<PerContent, "k_w", "an invented length per percent of content", MillimetrePerPercent>
{
};

using regression_cross_tu::fit;
using regression_cross_tu::sixRows;

[[nodiscard]] constexpr formula::Rational exact_output(auto const& output, auto const& environment)
{
    return **formula::checked_evaluate_si(output, environment);
}
} // namespace

TEST_CASE("two regressors are fitted exactly: constant, coefficients, r squared and points", "[least-squares][multiple]")
{
    // In coherent SI: the constant in metres, at 0 K; coefficient 1 in m/K;
    // coefficient 2 in metres per unit of content (a fraction, not a percent).
    // At run time, as every exact fit is (the constant-evaluation rule).
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(fit), sixRows) == rat(346407, 4'609'880'000));
    CHECK(exact_output(formula::opaque_output<"constant">(fit), sixRows) == rat(22'365'154'943, 276'592'800'000));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(fit), sixRows) == rat(3'842'851, 69'148'200));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), sixRows) == rat(27'398'849'648, 27'403'085'919));
    CHECK(exact_output(formula::opaque_output<"points">(fit), sixRows) == rat(6));
}

TEST_CASE("each output of a multiple regression has its own dimension", "[least-squares][multiple]")
{
    STATIC_REQUIRE(decltype(formula::opaque_output<"constant">(fit))::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(formula::opaque_output<"coefficient 1">(fit))::dimension
                   == formula::dim::Length / formula::dim::Temperature);
    STATIC_REQUIRE(decltype(formula::opaque_output<"coefficient 2">(fit))::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(formula::opaque_output<"r squared">(fit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::opaque_output<"points">(fit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(formula::MultipleLeastSquares<2>::exact_limbs == 19);
    // The hook is found for one, two and eight regressors: a malformed one
    // would fall back to compute<Rational> silently.
    using TimeRead = formula::ObservationsVarNode<Elapsed, 8>;
    using LengthRead = formula::ObservationsVarNode<Length, 8>;
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<1>, TimeRead, LengthRead>);
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<2>,
                                                           formula::ObservationsVarNode<Temperature, 8>,
                                                           formula::ObservationsVarNode<Content, 8>, LengthRead>);
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<8>, TimeRead, TimeRead,
                                                           TimeRead, TimeRead, TimeRead, TimeRead, TimeRead,
                                                           TimeRead, LengthRead>);
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs.size() == 11);
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs[8] == "coefficient 8");
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs[10] == "points");
}

TEST_CASE("one regressor is the line", "[least-squares][multiple]")
{
    constexpr auto fourRows = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));
    constexpr auto line = formula::linear_least_squares(formula::observations<Elapsed, 8>, formula::observations<Length, 8>,
                                                        { .reference = "Example Standard 12" });
    constexpr auto single = formula::multiple_least_squares(formula::regressors(formula::observations<Elapsed, 8>),
                                                            formula::observations<Length, 8>,
                                                            { .reference = "Example Standard 12" });
    CHECK(exact_output(formula::opaque_output<"constant">(single), fourRows)
          == exact_output(formula::opaque_output<"intercept">(line), fourRows));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(single), fourRows)
          == exact_output(formula::opaque_output<"slope">(line), fourRows));
    CHECK(exact_output(formula::opaque_output<"r squared">(single), fourRows) == rat(1083, 1085));
}

TEST_CASE("the order of the rows does not change a multiple regression", "[least-squares][multiple]")
{
    // Rows 6, 3, 1, 5, 2, 4.
    constexpr auto shuffled = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(297, 10), rat(179, 10), rat(113, 10), rat(233, 10), rat(137, 10),
                                                      rat(191, 10)),
        formula::MeasuredObservations<Content, 8>(rat(43, 10), rat(29, 10), rat(23, 10), rat(37, 10), rat(31, 10),
                                                  rat(41, 10)),
        formula::MeasuredObservations<Length, 8>(rat(106), rat(10433, 100), rat(2588, 25), rat(10521, 100),
                                                 rat(10413, 100), rat(1051, 10)));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(fit), shuffled)
          == exact_output(formula::opaque_output<"coefficient 2">(fit), sixRows));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), shuffled)
          == exact_output(formula::opaque_output<"r squared">(fit), sixRows));
}

TEST_CASE("a singular design is a multiple regression's own domain error, exactly and in double", "[least-squares][multiple]")
{
    // The delay is twice the elapsed time on every row: no unique fit,
    // whatever the lengths were.
    constexpr auto collinear = formula::multiple_least_squares(
        formula::regressors(formula::observations<Elapsed, 8>, formula::observations<Delay, 8>),
        formula::observations<Length, 8>,
        { .reference = "Example Standard 12" });
    constexpr auto doubledDelay = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Delay, 8>(rat(2), rat(4), rat(8), rat(14)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));
    auto const exact = formula::checked_evaluate_si(formula::opaque_output<"coefficient 1">(collinear), doubledDelay);
    REQUIRE(!exact.has_value());
    CHECK(exact.error() == formula::ArithmeticError::DomainError);
    auto const approximate =
        formula::checked_evaluate_si<double>(formula::opaque_output<"coefficient 1">(collinear), doubledDelay);
    REQUIRE(!approximate.has_value());
    CHECK(approximate.error() == formula::ArithmeticError::DomainError);
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"coefficient 1">(collinear), doubledDelay,
                                                        formula::RecordingSink { recorded });
    REQUIRE(formula::opaque_data(recorded, 3) != nullptr);
    CHECK(formula::opaque_data(recorded, 3)->failure == formula::OpaqueFailure::Own);
}

TEST_CASE("fewer rows than regressors plus one are refused, and one more fits exactly", "[least-squares][multiple]")
{
    constexpr auto twoRows = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(113, 10), rat(137, 10)),
        formula::MeasuredObservations<Content, 8>(rat(23, 10), rat(31, 10)),
        formula::MeasuredObservations<Length, 8>(rat(2588, 25), rat(10413, 100)));
    CHECK(formula::checked_evaluate_si(formula::opaque_output<"constant">(fit), twoRows).error()
          == formula::ArithmeticError::DomainError);
    constexpr auto threeRows = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(113, 10), rat(137, 10), rat(179, 10)),
        formula::MeasuredObservations<Content, 8>(rat(23, 10), rat(31, 10), rat(29, 10)),
        formula::MeasuredObservations<Length, 8>(rat(2588, 25), rat(10413, 100), rat(10433, 100)));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), threeRows) == rat(1));
}

TEST_CASE("a regressor in degrees Celsius: the constant is at 0 K, and the value at 0 degC is a formula", "[least-squares][multiple]")
{
    // The constant, 80.859... mm, is the length at 0 K and 0 % content. At
    // 0 degC it is the constant plus coefficient 1 times 273.15 K:
    // 101.385... mm, 101.39 at 2 dp.
    constexpr auto atZeroCelsius =
        formula::opaque_output<"constant">(fit)
        + formula::opaque_output<"coefficient 1">(fit) * formula::constant<unit::Kelvin>(rat(27315, 100));
    CHECK(exact_output(atZeroCelsius, sixRows) == rat(14'021'209'633, 138'296'400'000));
    auto const rounded = formula::checked_evaluate<Offset>(
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(atZeroCelsius),
        sixRows);
    REQUIRE(rounded.has_value());
    CHECK(rounded->measurement().value() == rat(10139, 100));
}

TEST_CASE("a regressor in percent: its coefficient is per unit, and a unit per percent reports it per percent", "[least-squares][multiple]")
{
    // 55.574... mm per unit of content is 0.5557 mm per percent at 4 dp.
    auto const perPercent = formula::checked_evaluate<PerContent>(
        formula::rounded_output<"coefficient 2", MillimetrePerPercent, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(fit),
        sixRows);
    REQUIRE(perPercent.has_value());
    CHECK(perPercent->measurement().value() == rat(5557, 10'000));
}

TEST_CASE("a multiple regression is traced as one call, rendered and documented", "[least-squares][multiple][trace]")
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"coefficient 2">(fit), sixRows,
                                                        formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("4. multiple least squares(#1, #2, #3) = constant = 22365154943/276592800 mm; "
                    "coefficient 1 = 346407/4609880000 m/K; coefficient 2 = 19214255/345741 mm; "
                    "r squared = 27398849648/27403085919; points = 6 [inside not shown] "
                    "[Length by temperature and content, Example Standard 12, 5.3]\n")
          != std::string::npos);
    CHECK(formula::render(formula::opaque_output<"coefficient 2">(fit))
          == "multiple least squares(T(i), w(i), L(i)).coefficient 2");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::opaque_output<"coefficient 2">(fit))
          == "\\text{multiple least squares}({T}_{i}, {w}_{i}, {L}_{i})_{\\text{coefficient 2}}");
    formula::Documentation const page =
        formula::document(formula::opaque_output<"constant">(fit) + formula::opaque_output<"coefficient 2">(fit));
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].outputs
          == std::vector<std::string_view> { "constant", "coefficient 1", "coefficient 2", "r squared", "points" });
    REQUIRE(page.symbols.size() == 3);
    CHECK(page.symbols[2].shape == formula::ValueShape::Observations);
}

TEST_CASE("a multiple regression in double agrees with the exact fit", "[least-squares][multiple]")
{
    auto const approximate = [](auto const& output) {
        return **formula::checked_evaluate_si<double>(output, sixRows);
    };
    CHECK(std::abs(approximate(formula::opaque_output<"coefficient 1">(fit)) - 346407.0 / 4'609'880'000.0)
          <= 1e-12 * (346407.0 / 4'609'880'000.0));
    CHECK(std::abs(approximate(formula::opaque_output<"coefficient 2">(fit)) - 3'842'851.0 / 69'148'200.0)
          <= 1e-12 * (3'842'851.0 / 69'148'200.0));
    CHECK(approximate(formula::opaque_output<"points">(fit)) == 6.0);
}

TEST_CASE("a multiple regression's outputs are the same in two translation units", "[least-squares][multiple]")
{
    auto const here = formula::checked_evaluate_si(regression_cross_tu::secondCoefficient, sixRows);
    auto const there = second_coefficient_in_other_tu(regression_cross_tu::secondCoefficient);
    REQUIRE(here.has_value());
    REQUIRE(there.has_value());
    CHECK(**here == **there);
    for (std::size_t at = 0; at < formula::MultipleLeastSquares<2>::outputs.size(); ++at)
        CHECK(output_name_in_other_tu(at) == formula::MultipleLeastSquares<2>::outputs[at]);
}
```

  `test/regression_cross_tu.hpp`, after `opaque_cross_tu.hpp`'s pattern (named namespace, `inline constexpr`,
  the other unit takes the node as a parameter so a different type fails to link):

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One multiple regression, its environment and one of its outputs, `inline
/// constexpr` in a header and used from `multiple_least_squares_tests.cpp` and
/// `regression_cross_tu_b.cpp`. The other unit takes the output node as a
/// parameter, so an output whose type differs between the two -- another
/// position, another operation -- names another function and fails to link;
/// and it returns the operation's output names, which must read the same.

#include <formula-cpp/least_squares.hpp>

#include <cstddef>
#include <expected>
#include <string_view>

namespace regression_cross_tu
{
struct Temperature: formula::Quantity<Temperature, "T", "an invented temperature", formula::unit::Celsius>
{
};
struct Content: formula::Quantity<Content, "w", "an invented content", formula::unit::Percent>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto fit = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 8>, formula::observations<Content, 8>),
    formula::observations<Length, 8>,
    { .title = "Length by temperature and content", .reference = "Example Standard 12", .section = "5.3" });

inline constexpr auto secondCoefficient = formula::opaque_output<"coefficient 2">(fit);

// Six rows, invented: 11.3 ... 29.7 degC, 2.3 ... 4.3 %, 103.52 ... 106 mm.
inline constexpr auto sixRows = formula::environment(
    formula::MeasuredObservations<Temperature, 8>(formula::Rational { 113, 10 }, formula::Rational { 137, 10 },
                                                  formula::Rational { 179, 10 }, formula::Rational { 191, 10 },
                                                  formula::Rational { 233, 10 }, formula::Rational { 297, 10 }),
    formula::MeasuredObservations<Content, 8>(formula::Rational { 23, 10 }, formula::Rational { 31, 10 },
                                              formula::Rational { 29, 10 }, formula::Rational { 41, 10 },
                                              formula::Rational { 37, 10 }, formula::Rational { 43, 10 }),
    formula::MeasuredObservations<Length, 8>(formula::Rational { 2588, 25 }, formula::Rational { 10413, 100 },
                                             formula::Rational { 10433, 100 }, formula::Rational { 1051, 10 },
                                             formula::Rational { 10521, 100 }, formula::Rational { 106 }));
} // namespace regression_cross_tu

/// Defined in `regression_cross_tu_b.cpp`: @p output evaluated there, in SI.
[[nodiscard]] formula::Evaluated<formula::Rational> second_coefficient_in_other_tu(
    decltype(regression_cross_tu::secondCoefficient) const& output) noexcept;

/// Defined in `regression_cross_tu_b.cpp`: the two-regressor operation's output
/// name at zero-based position @p at, as that unit sees it.
[[nodiscard]] std::string_view output_name_in_other_tu(std::size_t at) noexcept;
```

  `test/regression_cross_tu_b.cpp` defines both (`formula::checked_evaluate_si(output,
  regression_cross_tu::sixRows)` and `formula::MultipleLeastSquares<2>::outputs[at]`).
- [ ] **Step 2: Run and fail.** `cl.ps1 -Filter "multiple regression|regressor"`. Expected: `BUILD FAILED`,
  `multiple_least_squares` and `regressors` undeclared.
- [ ] **Step 3: Implement** in `least_squares.hpp`, after the observation fit's overloads:

```cpp
namespace detail
{
    /// The names of a regression's coefficients, one-based: written out, not
    /// generated, so that every regression's outputs view string literals.
    inline constexpr std::array<std::string_view, maxRegressors> coefficientNames {
        "coefficient 1", "coefficient 2", "coefficient 3", "coefficient 4",
        "coefficient 5", "coefficient 6", "coefficient 7", "coefficient 8"
    };

    /// `constant`, `coefficient 1` to `coefficient K`, `r squared`, `points`.
    template <std::size_t K>
    [[nodiscard]] consteval std::array<std::string_view, K + 3> regression_output_names() noexcept
    {
        std::array<std::string_view, K + 3> named {};
        named[0] = "constant";
        for (std::size_t at = 0; at < K; ++at)
            named[at + 1] = coefficientNames[at];
        named[K + 1] = "r squared";
        named[K + 2] = "points";
        return named;
    }

    /// K + 1 inputs, every one raw observations.
    template <std::size_t K>
    [[nodiscard]] consteval std::array<InputShape, K + 1> all_observations() noexcept
    {
        std::array<InputShape, K + 1> shaped {};
        for (InputShape& each: shaped)
            each = InputShape::Observations;
        return shaped;
    }
} // namespace detail

/// Ordinary least squares with K regressors over raw observations: `y =
/// constant + coefficient 1 x1 + ... + coefficient K xK`, the regressors
/// first and the values last, each a set of observations paired row by row.
/// Built by `multiple_least_squares`; K is 1 to 8, and K = 1 is the line.
///
/// Decided exactly, before any sum, in every `Rep`, each the fit's own
/// `DomainError`: every input holds as many observations, at least K + 1, no
/// regressor is flat and the values are not flat. **A singular design is an
/// error, not a number**: exactly, a zero pivot of the fraction-free
/// elimination (`detail/least_squares_kernel.hpp`); in `double`, a pivot of
/// the centred normal equations at or below 1e-9 of its diagonal, since
/// rounded data cannot decide singularity. Outputs in the constant's,
/// the values', dimension; each coefficient in the values' over its
/// regressor's; `r squared` and `points` bare numbers.
///
/// K outside 1 to 8 is refused by the constraint alone, in the compiler's
/// words: every member below is sized by K, and a `static_assert` here would
/// be followed by their own errors. `multiple_least_squares` refuses any other
/// number in this library's words before it names this type.
template <std::size_t K>
    requires(K >= 1 && K <= detail::maxRegressors)
struct MultipleLeastSquares
{
    /// What the trace calls it.
    static constexpr std::string_view name = "multiple least squares";
    /// The K regressors, then the values: all raw observations.
    static constexpr std::array<InputShape, K + 1> shapes = detail::all_observations<K>();
    /// The outputs, in this order.
    static constexpr std::array<std::string_view, K + 3> outputs = detail::regression_output_names<K>();
    /// The exact kernel's width for K regressors.
    static constexpr std::size_t exact_limbs = detail::regression_limbs(K);

    /// The constant in the values' dimension, each coefficient in the values'
    /// over its regressor's, R^2 and the count bare. Any dimensions are
    /// accepted.
    static consteval std::optional<std::array<Dimension, K + 3>> output_dimensions(
        std::array<Dimension, K + 1> regressorsThenValues) noexcept
    {
        std::array<Dimension, K + 3> measured {};
        measured[0] = regressorsThenValues[K];
        for (std::size_t at = 0; at < K; ++at)
            measured[at + 1] = regressorsThenValues[K] / regressorsThenValues[at];
        measured[K + 1] = dim::Scalar;
        measured[K + 2] = dim::Scalar;
        return measured;
    }

    /// The fit, exactly, as wide fractions in coherent units.
    template <typename... Columns>
        requires(sizeof...(Columns) == K + 1 && (std::same_as<Columns, std::span<Rational const>> && ...))
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, K + 3>, ArithmeticError> compute_exact(
        Columns... observedColumns) noexcept
    {
        std::array<std::span<Rational const>, K + 1> const given { observedColumns... };
        std::array<std::span<Rational const>, K> regressorColumns;
        for (std::size_t at = 0; at < K; ++at)
            regressorColumns[at] = given[at];
        return detail::exact_regression<K>(regressorColumns, given[K]);
    }

    /// The fit in coherent units: exactly for `Rep = Rational`, every output a
    /// `Rational` or all `Overflow`; approximately otherwise.
    template <typename Rep, typename... Columns>
        requires(sizeof...(Columns) == K + 1 && (std::same_as<Columns, std::span<Rep const>> && ...))
    static constexpr std::expected<std::array<Rep, K + 3>, ArithmeticError> compute(Columns... observedColumns) noexcept
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            std::expected<std::array<detail::WideRatio<exact_limbs>, K + 3>, ArithmeticError> const exact =
                compute_exact(observedColumns...);
            if (!exact.has_value())
                return std::unexpected { exact.error() };
            return detail::narrowed_all(*exact);
        }
        else
        {
            std::array<std::span<Rep const>, K + 1> const given { observedColumns... };
            std::array<std::span<Rep const>, K> regressorColumns;
            for (std::size_t at = 0; at < K; ++at)
                regressorColumns[at] = given[at];
            return detail::approximate_regression<Rep, K>(regressorColumns, given[K]);
        }
    }
};

/// The regressors of a multiple regression, in order: what `regressors(...)`
/// builds, since a parameter pack before the values and the citation could
/// not be deduced.
template <typename... Xs>
struct Regressors
{
    /// The regressors, first to last. No `{}` initialiser, deliberately
    /// (defect class 4): see `Corrections` (`lookup.hpp`).
    std::tuple<Xs...> inputs;
};

/// The regressors @p regressorInputs of a multiple regression, in order:
/// `regressors(observations<Temperature, 64>, observations<Content, 64>)`.
template <typename... Xs>
[[nodiscard]] constexpr Regressors<Xs...> regressors(Xs... regressorInputs) noexcept
{
    return Regressors<Xs...> { std::tuple<Xs...> { regressorInputs... } };
}
```

  The refusals, in `detail`, each named so its operand prints, with these messages verbatim:

| struct | condition | message |
|---|---|---|
| `RequireRegressionCitation<Y>` | `sizeof(Y) == 0` | `formula: multiple_least_squares needs a citation, the reason the method fits a regression here; pass {} when it gives none` |
| `RequireRegressorsWrapped<X>` | `sizeof(X) == 0` | `formula: multiple_least_squares takes its regressors wrapped in regressors(...), then the values, then the citation: multiple_least_squares(regressors(x1, x2), y, citation)` |
| `RequireSomeRegressor<Count>` | `Count > 0` | `formula: multiple_least_squares needs at least one regressor, and regressors() names none; values with no regressor have a mean, sample_mean(observations<Q, Capacity>), not a fit` |
| `RequireAtMostEightRegressors<Count>` | `Count <= maxRegressors` | `formula: multiple_least_squares fits at most 8 regressors, the most its exact solve is sized for; the number given appears in this diagnostic as the template argument Count of RequireAtMostEightRegressors` |
| `RequireRegressionObservations<Operand>` | `ObservationsNode<Operand>` | `formula: multiple_least_squares reads each regressor and the values as raw observations, paired by the order they were made, and this is not a set of observations; read it with observations<Q, Capacity> -- the operand appears in this diagnostic as the template argument of RequireRegressionObservations` |
| `RequireRegressionQuantitiesDistinct<Q>` | `!std::is_same_v<Q, Q>` | `formula: multiple_least_squares reads one quantity twice, as two regressors or as a regressor and the values; that design is singular whatever was observed -- the quantity appears in this diagnostic as the template argument Q of RequireRegressionQuantitiesDistinct` |

  and the staged check, one message per mistake:

```cpp
    /// Whether @p T is a `Regressors`.
    template <typename T>
    inline constexpr bool isRegressors = false;

    template <typename... Xs>
    inline constexpr bool isRegressors<Regressors<Xs...>> = true;

    /// How many of @p Inputs observe the quantity @p Q.
    template <typename Q, typename... Inputs>
    inline constexpr std::size_t observersOf =
        (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Q, typename ObservedQuantity<Inputs>::type> });

    /// The position of the first of @p Inputs whose quantity another one
    /// reads too, or `sizeof...(Inputs)` when none is.
    template <typename... Inputs>
    [[nodiscard]] consteval std::size_t first_repeated_quantity() noexcept
    {
        constexpr std::array<std::size_t, sizeof...(Inputs)> observedBy {
            observersOf<typename ObservedQuantity<Inputs>::type, Inputs...>...
        };
        for (std::size_t at = 0; at < observedBy.size(); ++at)
            if (observedBy[at] > 1)
                return at;
        return observedBy.size();
    }
```

  (The inner `Inputs...` expands inside each element; the outer `...` expands the element's own `Inputs`.) Then:

```cpp
    /// Checks a multiple regression's operands, each check gated on the ones
    /// before it and asked in an `if constexpr` of its own (g++ evaluates a
    /// `consteval` call behind a false `&&`: `RequireOpaqueCallValid`).
    template <typename Y, typename... Xs>
    struct RequireRegressionValid
    {
        static constexpr std::size_t regressorCount = sizeof...(Xs);
        static_assert(RequireSomeRegressor<regressorCount>::value);
        static_assert(std::conditional_t<(regressorCount > 0), RequireAtMostEightRegressors<regressorCount>,
                                         std::true_type>::value);
        static constexpr bool countOk = regressorCount > 0 && regressorCount <= maxRegressors;

        static_assert((std::conditional_t<countOk, RequireRegressionObservations<Xs>, std::true_type>::value && ...));
        static_assert(std::conditional_t<countOk, RequireRegressionObservations<Y>, std::true_type>::value);
        static constexpr bool observationsOk = countOk && (ObservationsNode<Xs> && ...) && ObservationsNode<Y>;

        [[nodiscard]] static consteval std::size_t repeated_at() noexcept
        {
            if constexpr (observationsOk)
                return first_repeated_quantity<Xs..., Y>();
            else
                return regressorCount + 1;
        }

        static constexpr std::size_t repeatedAt = repeated_at();
        /// The quantity read twice, or the first operand's when none is (then unused).
        using Repeated = typename ObservedQuantity<
            std::tuple_element_t<(repeatedAt < regressorCount + 1 ? repeatedAt : 0), std::tuple<Xs..., Y>>>::type;
        static_assert(std::conditional_t<(repeatedAt < regressorCount + 1), RequireRegressionQuantitiesDistinct<Repeated>,
                                         std::true_type>::value);

        /// Whether the call is sound: every check above passed.
        static constexpr bool value = observationsOk && repeatedAt == regressorCount + 1;
    };

    template <std::size_t>
    using RefusedObservationsAt = RefusedObservations;

    template <std::size_t... At>
    [[nodiscard]] constexpr auto refused_regression_of(Citation citation, std::index_sequence<At...>) noexcept
    {
        return OpaqueCall<MultipleLeastSquares<sizeof...(At) - 1>, RefusedObservationsAt<At>...> {
            std::tuple<RefusedObservationsAt<At>...> {}, citation
        };
    }

    /// What every refused multiple regression returns: eight regressors of
    /// refused observations, so that any output up to `coefficient 8` can be
    /// taken and evaluated and asks nothing again.
    [[nodiscard]] constexpr auto refused_regression(Citation citation) noexcept
    {
        return refused_regression_of(citation, std::make_index_sequence<maxRegressors + 1> {});
    }
```

  (With no regressor, `std::tuple_element_t<0, std::tuple<Y>>` is Y: well-formed. With a series among the
  operands `ObservedQuantity` of it is itself, and the stage is off anyway.) The three entry points:

```cpp
/// A multiple regression of @p valueInput on @p regressorSet, for the reason
/// @p citation gives: `multiple_least_squares(regressors(observations<T, 64>,
/// observations<W, 64>), observations<L, 64>, { ... })`. Its outputs are
/// `constant`, `coefficient 1` to `coefficient K`, `r squared` and `points`.
/// Refused in this library's words: no regressor, more than 8, anything but
/// raw observations, and one quantity read twice.
template <typename... Xs, typename Y>
[[nodiscard]] constexpr auto multiple_least_squares(Regressors<Xs...> regressorSet, Y valueInput, Citation citation) noexcept
{
    if constexpr (detail::RequireRegressionValid<Y, Xs...>::value)
        return std::apply(
            [&](auto const&... regressorInputs) {
                return opaque<MultipleLeastSquares<sizeof...(Xs)>>(citation, regressorInputs..., valueInput);
            },
            regressorSet.inputs);
    else
        return detail::refused_regression(citation);
}

/// Without a citation: refused, as a fit without one is.
template <typename... Xs, typename Y>
[[nodiscard]] constexpr auto multiple_least_squares(Regressors<Xs...>, Y) noexcept
{
    static_assert(detail::RequireRegressionCitation<Y>::value);
    return detail::refused_regression(Citation {});
}

/// Regressors not wrapped in `regressors(...)`: refused, naming the spelling.
template <typename X, typename Y>
    requires(!detail::isRegressors<X>)
[[nodiscard]] constexpr auto multiple_least_squares(X, Y, Citation citation) noexcept
{
    static_assert(detail::RequireRegressorsWrapped<X>::value);
    return detail::refused_regression(citation);
}
```

  The `requires` on the unwrapped overload keeps the call with `regressors(...)` unambiguous on every compiler;
  confirm on both builds that `multiple_least_squares(regressors(a, b), y, {})` picks the first.
  `least_squares.hpp`'s file comment gains a paragraph on the multiple regression.
- [ ] **Step 4: Run and pass** (+11 cases). `cl.ps1 -Filter "multiple regression|regressor|translation units"`,
  `gcc14.sh "multiple regression|regressor"`, then both unfiltered.
- [ ] **Step 5: Negative tests.** One file each, with the cross-TU header's quantities copied in (no shared
  header), a `main` taking `coefficient 1` and `r squared` of the result and evaluating one, so that a second
  message would show:

| name | the mistake | expected (prefix) | `REJECT` |
|---|---|---|---|
| `multiple_least_squares_uncited` | `multiple_least_squares(regressors(T, w), L)` | `formula: multiple_least_squares needs a citation` | `"no matching" "has no output of that name"` |
| `multiple_least_squares_series_regressor` | `regressors(series<T, 6>, observations<w, 8>)` | `formula: multiple_least_squares reads each regressor and the values as raw observations` | `"reads one quantity twice" "has no output of that name" "passes an input of another shape"` |
| `multiple_least_squares_series_values` | values `series<L, 6>` | same | same |
| `multiple_least_squares_no_regressor` | `regressors()` | `formula: multiple_least_squares needs at least one regressor` | `"fits at most 8" "reads each regressor" "has no output of that name"` |
| `multiple_least_squares_nine_regressors` | nine distinct observed quantities | `formula: multiple_least_squares fits at most 8 regressors` | `"reads each regressor" "has no output of that name" "constraints not satisfied" "MultipleLeastSquares<9>"` |
| `multiple_least_squares_repeated_regressor` | `regressors(observations<T, 8>, observations<T, 8>)` | `formula: multiple_least_squares reads one quantity twice` | `"has no output of that name" "RequireResultDimension"` |
| `multiple_least_squares_regressor_as_values` | `regressors(observations<L, 8>), observations<L, 8>` | same | same |
| `multiple_least_squares_unwrapped` | `multiple_least_squares(observations<T, 8>, observations<L, 8>, {...})` | `formula: multiple_least_squares takes its regressors wrapped in regressors(...)` | `"no matching" "has no output of that name"` |

  +8 negative tests. `EXPECT_COUNT 1` on `multiple_least_squares_series_regressor` (one series among two regressors is one mistake).
  Wrong string first, then right; deletion check for each: the matching `static_assert` line.
- [ ] **Step 6: Consumer globals.** Add `regressor, regressors` to the globals. Probe, after Task 4's line: a
  regression of `AgreedEdge` on `EdgeX` and a new observed dimensionless `Factor`:

```cpp
    // Two regressors over three rows: y = 1 mm + 2 x + 5 mm * k, exactly, so
    // coefficient 1 is 2 and R^2 is 1.
    std::array<formula::Rational, 3> const factorReadings { formula::Rational { 1 }, formula::Rational { 3 },
                                                            formula::Rational { 2 } };
    std::array<formula::Rational, 3> const combined { formula::Rational { 212 }, formula::Rational { 342 },
                                                      formula::Rational { 493 } };
    auto const regressionSample = formula::environment(*edgeObserved,
                                                       *formula::MeasuredObservations<Factor, 4>::from(factorReadings),
                                                       *formula::MeasuredObservations<AgreedEdge, 4>::from(combined));
    constexpr auto edgeRegression = formula::multiple_least_squares(
        formula::regressors(formula::observations<EdgeX, 4>, formula::observations<Factor, 4>),
        formula::observations<AgreedEdge, 4>,
        { .reference = "Example Standard 3" });
    auto const firstCoefficient =
        formula::checked_evaluate<Factor>(formula::opaque_output<"coefficient 1">(edgeRegression), regressionSample);
    auto const inDouble =
        formula::checked_evaluate_si<double>(formula::opaque_output<"r squared">(edgeRegression), regressionSample);
    probe.checks.push_back(firstCoefficient.has_value()
                           && firstCoefficient->measurement().value() == formula::Rational { 2 }
                           && inDouble.has_value() && inDouble->has_value() && **inDouble > 0.999);
```

  (212 = 1 + 206 + 5, 342 = 1 + 326 + 15, 493 = 1 + 482 + 10.) Raise the probe's count by +1 probe check; name the entry
  points in the file comment.
- [ ] **Step 7: CHANGELOG.** `### Added`:

```markdown
- `multiple_least_squares(regressors(x1, ..., xK), y, citation)` fits y = constant + coefficient 1 x1 + ... +
  coefficient K xK through raw observations paired by row, for K from 1 to 8; K = 1 is the line. Its outputs are
  `constant`, `coefficient 1` to `coefficient K`, `r squared` and `points`, each coefficient in the values'
  dimension over its regressor's. A singular design is the fit's own `DomainError`, never a number: decided exactly
  in `Rational` and by `rounded_output`, and in `double` when a pivot of the centred normal equations is at or below
  10⁻⁹ of its diagonal. Refused where written: no citation, no regressor, more than eight, anything but raw
  observations, and one quantity read twice. `MultipleLeastSquares<K>`, `Regressors` and `regressors` are the
  operation and its holder.
```

- [ ] **Step 8: Commit.**

```text
feat(least_squares): multiple regression over observations, refusing a singular design

A fit of a length on temperature and content was computed outside the
library and entered as if measured, and spreadsheets answer a singular
design with arbitrary coefficients. multiple_least_squares fits up to
eight regressors through raw observations, exactly or rounded where
used, and a design whose regressors are collinear is its own domain
error: exactly a zero pivot, and in double a pivot below 1e-9 of its
diagonal.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Task 7: Several regressors in the example, the guide and the census

**Files:**
- Modify: `examples/opaque_and_retry.cpp` (file comment, section 8), `examples/CMakeLists.txt` (pinned regex),
  `docs/opaque-and-retry.md` (section "Several regressors" after Task 5's), `test/overflow_census_tests.cpp`
  (a K = 2 row in Task 5's case), `docs/numeric-headroom.md` (regenerated), `README.md:271`, `docs/index.md:77`

**Interfaces:**
- Consumes: Task 6's `multiple_least_squares`, `regressors`; Task 5's `RouteScan`, `scan_routes`, `decimalText`.
- Produces: example output lines quoted by the guide; one more row of `census:regression`.

- [ ] **Step 1: The example.** File comment item 8 ("8. Several regressors at once, and a design that cannot be
  solved: two regressors that measure the same thing, twice over, are refused."). Declarations:

```cpp
// ---- 8. Several regressors ------------------------------------------------------------

using Temperature = formula::Quantity<struct TemperatureTag, "T", "an invented temperature", unit::Celsius>;
// "w" is the iterated estimate's symbol already (section 5).
using Content = formula::Quantity<struct ContentTag, "w_c", "an invented content", unit::Percent>;
using Delay = formula::Quantity<struct DelayTag, "t_d", "an invented delay", unit::Second>;

// Units the method states its coefficients in.
constexpr formula::Unit millimetrePerKelvin { .dimension = formula::dim::Length / formula::dim::Temperature,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/K"),
                                              .decimals = 4 };
constexpr formula::Unit millimetrePerPercent { .dimension = formula::dim::Length,
                                               .magnitudeNumerator = 1,
                                               .magnitudeDenominator = 10,
                                               .symbolText = formula::symbol("mm/%"),
                                               .decimals = 4 };
using Expansion = formula::Quantity<struct ExpansionTag, "k_T", "an invented length per kelvin", millimetrePerKelvin>;
using Swelling = formula::Quantity<struct SwellingTag, "k_w", "an invented length per percent", millimetrePerPercent>;

constexpr auto byTemperatureAndContent = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 64>, formula::observations<Content, 64>),
    formula::observations<Length, 64>,
    { .title = "Length by temperature and content", .reference = "Example Standard 12", .section = "5.3" });

constexpr auto sixRows = formula::environment(
    formula::MeasuredObservations<Temperature, 64>(formula::Rational { 113, 10 }, formula::Rational { 137, 10 },
                                                   formula::Rational { 179, 10 }, formula::Rational { 191, 10 },
                                                   formula::Rational { 233, 10 }, formula::Rational { 297, 10 }),
    formula::MeasuredObservations<Content, 64>(formula::Rational { 23, 10 }, formula::Rational { 31, 10 },
                                               formula::Rational { 29, 10 }, formula::Rational { 41, 10 },
                                               formula::Rational { 37, 10 }, formula::Rational { 43, 10 }),
    formula::MeasuredObservations<Length, 64>(formula::Rational { 2588, 25 }, formula::Rational { 10413, 100 },
                                              formula::Rational { 10433, 100 }, formula::Rational { 1051, 10 },
                                              formula::Rational { 10521, 100 }, formula::Rational { 106 }));

// The length at 0 degC: the constant is the length at 0 K.
constexpr auto lengthAtZeroCelsius = formula::rounded<unit::Millimetre, formula::DecimalPlaces { 2 },
                                                      formula::RoundingMode::HalfEven>(
    formula::opaque_output<"constant">(byTemperatureAndContent)
    + formula::opaque_output<"coefficient 1">(byTemperatureAndContent)
          * formula::constant<unit::Kelvin>(formula::Rational { 27315, 100 }));
```

  In `main`, before `all checks passed`:

```cpp
    std::printf("== 8. Several regressors ==\n\n");

    auto const expansion =
        formula::explain<Expansion>(formula::opaque_output<"coefficient 1">(byTemperatureAndContent), sixRows);
    std::printf("%s\n", formula::render_trace(expansion.trace, { .maxSteps = 40 }).c_str());

    auto const perKelvin = formula::checked_evaluate<Expansion>(
        formula::rounded_output<"coefficient 1", millimetrePerKelvin, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(byTemperatureAndContent),
        sixRows);
    auto const perPercent = formula::checked_evaluate<Swelling>(
        formula::rounded_output<"coefficient 2", millimetrePerPercent, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(byTemperatureAndContent),
        sixRows);
    auto const atZero = formula::checked_evaluate<StartLength>(lengthAtZeroCelsius, sixRows);
    check(perKelvin.has_value() && perPercent.has_value() && atZero.has_value(), "two regressors, rounded");
    if (perKelvin.has_value() && perPercent.has_value() && atZero.has_value())
        std::printf("coefficient 1: %s, coefficient 2: %s, length at 0 degrees Celsius: %s\n",
                    decimalText(perKelvin->measurement()).c_str(), decimalText(perPercent->measurement()).c_str(),
                    decimalText(atZero->measurement()).c_str());
    check(perPercent.has_value() && perPercent->measurement().value() == formula::Rational { 5557, 10'000 },
          "0.5557 mm per percent");

    constexpr auto collinear = formula::multiple_least_squares(
        formula::regressors(formula::observations<Elapsed, 64>, formula::observations<Delay, 64>),
        formula::observations<Length, 64>,
        { .reference = "Example Standard 12" });
    constexpr auto twiceAsLate = formula::environment(
        formula::MeasuredObservations<Elapsed, 64>(formula::Rational { 1 }, formula::Rational { 2 },
                                                   formula::Rational { 4 }, formula::Rational { 7 }),
        formula::MeasuredObservations<Delay, 64>(formula::Rational { 2 }, formula::Rational { 4 },
                                                 formula::Rational { 8 }, formula::Rational { 14 }),
        formula::MeasuredObservations<Length, 64>(formula::Rational { 102, 10 }, formula::Rational { 109, 10 },
                                                  formula::Rational { 121, 10 }, formula::Rational { 143, 10 }));
    auto const unsolvable =
        formula::checked_evaluate<Length>(formula::opaque_output<"constant">(collinear), twiceAsLate);
    std::printf("a delay twice the elapsed time on every row: %s\n\n",
                unsolvable.has_value() ? "a fit" : std::string { formula::describe(unsolvable.error()) }.c_str());
    check(!unsolvable.has_value() && unsolvable.error() == formula::ArithmeticError::DomainError,
          "a singular design is refused");
```

  Expected output (derived; confirm by running):

```text
1. T = 113/10 °C; 137/10 °C; 179/10 °C; 191/10 °C; 233/10 °C; 297/10 °C
2. w_c = 23/10 %; 31/10 %; 29/10 %; 41/10 %; 37/10 %; 43/10 %
3. L = 2588/25 mm; 10413/100 mm; 10433/100 mm; 1051/10 mm; 10521/100 mm; 106 mm
4. multiple least squares(#1, #2, #3) = constant = 22365154943/276592800 mm; coefficient 1 = 346407/4609880000 m/K; coefficient 2 = 19214255/345741 mm; r squared = 27398849648/27403085919; points = 6 [inside not shown] [Length by temperature and content, Example Standard 12, 5.3]
5. coefficient 1 of #4 = 346407/4609880000 m/K
coefficient 1: 0.0751 mm/K, coefficient 2: 0.5557 mm/%, length at 0 degrees Celsius: 101.39 mm
a delay twice the elapsed time on every row: argument outside the domain of the operation
```

- [ ] **Step 2: Pin the example.** Before `.*all checks passed: yes`:
  `.*multiple least squares\(#1, #2, #3\) = constant = 22365154943/276592800 mm.*coefficient 1: 0\.0751 mm/K, coefficient 2: 0\.5557 mm/%, length at 0 degrees Celsius: 101\.39 mm.*a delay twice the elapsed time on every row: argument outside the domain of the operation`,
  and the comment above it names them. Watch it fail once with a wrong digit.
- [ ] **Step 3: The guide.** `## Several regressors`, after "A line through observations":
  1. The API: `regressors(...)` holds the regressors because a parameter pack cannot stand before the values and
     the citation; regressors first, values last, as a curve has points then values. The declaration (cpp).
  2. The outputs: `constant`, `coefficient 1` to `coefficient K` (one-based, K from 1 to 8), `r squared`,
     `points`; each coefficient in the values' dimension over its regressor's. The trace block (5 lines) and one
     paragraph on reading it: coefficient 1 is shown in the coherent `m/K`, because degrees Celsius have an offset
     and are never borrowed; the constant is the length at 0 K and 0 % -- the length at 0 °C is a formula over two
     outputs (the `lengthAtZeroCelsius` declaration, cpp); coefficient 2 is per unit of content, a fraction, so a
     method that reports per percent declares a unit of mm per % (the output line).
  3. **A singular design is an error, not a number.** The output line, then this table:

| design | exact (`opaque_output`, `rounded_output`) | `double` (`checked_evaluate_si<double>`) |
|---|---|---|
| one regressor a multiple of another (`x2 = 2 x1`) | `DomainError` | `DomainError` |
| one regressor offset from another (`x2 = x1 + 273.15`, the same temperature in kelvin) | `DomainError` | `DomainError` |
| an affine combination (`x2 = 3 x1 - 7`) | `DomainError` | `DomainError` |
| nearly collinear: 1 - R² of x2 on x1 about 2 * 10⁻⁹ | answered, exactly | answered, with few trustworthy digits |
| nearly collinear: about 5 * 10⁻¹⁰ | answered, exactly | `DomainError`, by the tolerance |
| the same quantity read twice | refused where it is written | refused where it is written |

  4. **The tolerance, stated.** "In `double` a design is taken for singular when a pivot of the centred normal
     equations is at or below 10⁻⁹ of its diagonal -- when 1 - R² of a regressor on the ones before it is below
     10⁻⁹. Rounded data cannot decide exact singularity, and a design near that line has few trustworthy digits
     either way. The exact routes decide it exactly." The row counts and refusals (no regressor, more than 8, not
     observations, uncited) in one sentence.
  README and `docs/index.md` guide rows name "and several regressors".
- [ ] **Step 4: The census's K = 2 row.** In Task 5's census case, a regression of the load on the time and a
  temperature at 1 dp, 3 to 128 rows:

```cpp
struct FitWarmth: formula::Quantity<FitWarmth, "T_r", "temperature of a reading", unit::Celsius>
{
};

/// A temperature at 1 dp in degrees Celsius: 20.0 + (7k mod 13) / 10.
[[nodiscard]] Rational warmth_at(std::int64_t k)
{
    return rat(200 + (7 * k) % 13, 10);
}

/// Whether the regression of the first @p count three-decimal loads on their
/// times and a temperature overflows through `opaque_output` and through
/// `rounded_output` (coefficient 1 at 4 dp of N/s, R^2 floored at 6 dp).
[[nodiscard]] std::pair<bool, bool> two_regressors_overflow(std::size_t count)
```

  -- the same shape as `observed_line_overflows`, over
  `multiple_least_squares(regressors(observations<FitTime, 128>, observations<FitWarmth, 128>),
  observations<FitForce, 128>, { .reference = "Example Standard 12" })`. In the case:
  `RouteScan const twoRegressors = scan_routes(3, ...)`, emitted after the line rows as
  `"two regressors: readings at 3 dp and a temperature at 1 dp in degrees Celsius (realistic)"`, and pinned:
  `CHECK(twoRegressors.roundedOverflowing.empty())`, `CHECK(twoRegressors.exactOverflowing.front() == 29)` (the
  model's prediction; the rule of Task 5 Step 4 applies). The page's prose names the row. Regenerate on cl
  (`cl.ps1 -Target formula-cpp-census-page -NoTest`).
- [ ] **Step 5: Run** (+0 cases: a row in Task 5's census case). Both builds unfiltered; `docs.*`, `example.opaque_and_retry`, `census.*` among the passes.
- [ ] **Step 6: Commit.**

```text
docs(least_squares): several regressors in the guide, the example and the census

The guide shows a two-regressor fit, what its outputs mean when a
regressor is read in degrees Celsius or in percent, and a singular
design refused, with the tolerance the double route decides by. The
census adds a two-regressor row: its exact route stops where the line's
does, and its rounded route does not stop on realistic data.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

---

## Self-Review

**Spec coverage.**

| spec | task |
|---|---|
| §1 problems 1-4 | 4 (count as data, R²), 3 + 4 (wide exact, `rounded_output`), 6 (several regressors) |
| §2 the move to `observations.hpp`, the stale comment | 1 (plus `RefusedObservations`, stated there) |
| §2 `InputShape::Observations`, span over those made, run-time length 0, overloads, `site`, "at observation k", walks reach the input | 2 |
| §2 pairing by row, counts differing are the fit's `DomainError` | 4 (tests), 5 (guide) |
| §3 the operation, overload, constrained curve refusal and its wording, name, outputs, dimensions | 4 |
| §3 pre-checks, R², points, the curve fit unchanged | 3 (kernel), 4 (operation) |
| §3 evaluation table: `compute_exact`, `compute<Rational>` all-or-nothing, `compute<double>` one op per call | 3, 4 |
| §3 fixture and its exact line | 4 (trace test), 5 (example) |
| §4 `regressors`, `multiple_least_squares`, output names in static storage, one-based, cross-TU test | 6 (names as literals: stated there) |
| §4 dimensions, K = 1 to 8, K = 1 equals the line, compile-time refusals, pre-checks | 6 |
| §4 singular design exact and by tolerance, the honest statement | 3 (kernel), 6 (operation), 7 (guide) |
| §5 exact kernel: common denominators, integer sums, centred sums times n, K = 1 closed form, Bareiss with row exchanges, zero pivot, back-substitution, constant, R², width measured and pinned, `Overflow` past it | 3 (and the width section) |
| §5 double route: means, one pass, LDLᵀ with the tolerance, no n-sized arrays | 3 |
| §6 trace per output used, rounded route's call line, failures with the suffix | 2, 4 |
| §6 page, renderings | 2, 4, 6 |
| §6 guide sections, no-traced-fallback paragraph | 5, 7 |
| §6 census rows: line at 3 and 4 dp, K = 2, where each route stops | 5, 7 |
| §6 example: line with R², 50 points rounded, two regressors, singular refused | 5, 7 |
| §6 `statistics.md`, README, index, CHANGELOG Added and Changed | 1, 2, 4, 5, 6, 7 |
| §7 opaque framework tests | 2 |
| §7 kernel tests (closed form, row order, pre-checks both reps, K = 2, singular x3, near-collinear, n = K + 1, n = K, double within 1e-12) | 3 |
| §7 #4 tests (fixture, trace, render, page, method with rule and R² constraint, 50 points) | 4 |
| §7 #5 tests (the same, K = 1 = line, dimensions, cross-TU names) | 6 |
| §7 negative tests: #4 uncited, mixed, same quantity; #5 uncited, series regressor, none, more than 8, repeated; observations where a shape is refused; overlay constant inside the fit; calculation reading the fit's observations | 2, 4, 6 |
| §9 risks: C4459, `Rational` arrays, g++ `&&`, constexpr limits, cl string views | Global Constraints; 3; 6 (literals remove the last) |

**Placeholder scan.** No "TBD" or "similar to above": every test is written out except the three places where a
pattern is repeated by construction -- Task 4's four small negative files (each named with its call and
registration), Task 6's eight negative files (a table giving each call, expected prefix and `REJECT`s), and Task
7's `two_regressors_overflow`, whose body is `observed_line_overflows` with the call named. Expected output that
depends on the foundation's spelling (the rounded route's call line and `round(...)` line) is quoted from the
foundation spec §6 and marked so.

**Type consistency.** `exact_regression<K>` returns `std::array<WideRatio<regression_limbs(K)>, K + 3>`, and each
operation's `exact_limbs` is `regression_limbs(K)`, so `compute_exact`'s declared return type matches the hook. The
line is K = 1: `{intercept, slope, r squared, points}` is the kernel's `{constant, c_1, R², points}`.
`OpaqueHeldObservations<Rep>::made` is the span `opaque_arguments` returns, and the foundation's
`declares_compute_exact` builds its argument tuple from the same `OpaqueArguments`. The kernel's signed integers are
the foundation's `WideSigned` (declared-precision plan, contract adjustment 8); this plan adds only
`exact_quotient` and `widened`, and every width it names is at least the hook's minimum of 4 limbs. `RefusedObservations` gains `refused` in
Task 4, before Task 6's stand-in uses it. `ObservedQuantity` is declared in Task 4 and reused in Task 6.

**Review Focus placement.** 1: Task 2 ("... none included"), Task 4 (domain errors). 2: Task 2 (capacities 8 and 4),
Task 4 (run-time counts, capacities 8 and 16, counts 3 and 4). 3: Task 2 and Task 4 ("an observation that fails to
convert ..."). 4: Task 3 ("the kernel answers a nearly collinear design ..."). 5: Tasks 3, 4 and 6 (Celsius). 6:
Task 6 (percent). 7: Task 4 (flat response).
