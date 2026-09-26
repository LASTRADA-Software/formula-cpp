# Statistics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the statistics a whole test series needs expressible, traceable and documented the way scalar formulas already are. That means sample statistics (count, mean, variance, range, and an exactly rounded standard deviation), outlier rejection that removes elements from a sample and re-runs the aggregate until nothing more is rejected or a declared bound aborts it, and repeatability/reproducibility limits in both forms: a constant per level read from a table, and a function of the level of the very results being checked, evaluated in two declared passes.

**Architecture:** A **sample** is any source of repeated determinations of one quantity. In this phase that is a phase 12 series (`series<Q, N>`, static length, strict absence) and, once phase 12 task 10 lands, a phase 12 observation set (`observations<Q, Capacity>`, runtime count). Sample statistics are ordinary `Node`s, and they are the bridge back to one value, exactly as `sum` is in phase 12. Outlier rejection, `without_outliers<…>(sample, criterion, verdict)`, is a *sample transformer*. It evaluates to the surviving sample and records one trace step per pass and one per rejected element, naming the element, its value, the statistic and the limit it exceeded. A declared bound turns "too many outliers" into the author's own verdict, never into a value. Precision limits are a `Node`, `precision_limit<Kind>(level, limit)`, whose limit expression reads a bound placeholder, `precision_level<Q>`. Pass 1 evaluates the level and pass 2 evaluates the limit at that level, and both passes are trace steps. The limit is an ordinary `Node`, so a repeatability check is an ordinary `Constraint`, and it joins a method's `ConstraintSet` with no new top-level concept.

**Tech Stack:** C++23, header-only. Catch2 via CPM. `STATIC_REQUIRE` for compile-time behaviour, plus the `test/negative/` must-not-compile harness, whose asserted message strings are tested API.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md`: §16.4 Tier B #6 (statistics with sample-mutating outlier rejection) and #7 (repeatability/reproducibility, both forms, two-pass), §16.3 #4 (conditional aggregation: which observations enter the mean depends on the observations), §16.6 (two-pass evaluation is a supported mode; the evaluation result is a sum type; every result carries its origin), §16.7 (constants overridable per jurisdiction), §16.8 (the boundary: no plant lifecycles, no set partitioning), §17 row 13, §11 (the two trace scars) and §19.

**Builds on (planned, not yet merged):** the phase 12 plan, `docs/superpowers/plans/2026-09-26-series.md`, with its rulings S1–S17. This plan uses phase 12's names and types (`SeriesNode`, `series<Q, N>`, `MeasuredSeries`, `EvaluatedSeries`, `SeriesFailure`, `detail::dispatch_series`, `Step::elements`, `detail::series_marker`, `ValueShape`, `MeasuredObservations`, `observations<Q, Capacity>`) and does not define competing ones. Where a phase 12 name changes during implementation, the phase 13 task that consumes it follows the name that shipped.

**Read before any task:** `.superpowers/sdd/2026-09-25-methods-and-overlays/defect-classes.md`. Every task below names the classes its steps are built to prevent. Every implementer's report says, for each of the eight classes, what they checked and how.

---

## Dependencies

Phase 12's task numbers below are those in its plan (`2026-09-26-series.md`). "P12 Tn" means that phase 12 task has **merged to the branch phase 13 is built on** (see "Branching", below). The phase 13 tasks have been ordered so that the first four need no phase 12 code, and can run while phase 12 is still in its early tasks.

| Phase 13 task | Needs from phase 12 | Needs from phase 13 | Earliest start |
|---|---|---|---|
| **1. Spike** | Nothing (stand-ins only) | Nothing | **Now** |
| **2. Exactly rounded square root** (`rounded_sqrt`) | Nothing | Task 1's rulings | After task 1 |
| **3. Critical values keyed by sample size** (`critical_value`) | Nothing | Task 1's rulings | After task 1 (in parallel with 2 if two implementers) |
| **4. Precision limits, two-pass** (`precision_limit`, `precision_level`, `abs`, the shared bound environment) | Nothing | Task 1's rulings | After task 1 (in parallel with 2 and 3) |
| **5. Samples: count and mean over a series** | **T2** (`SeriesNode`, `SeriesVarNode`/`series<Q, N>`, `MeasuredSeries`, `get_series`, `dispatch_series`, `EvaluatedSeries`, `SeriesFailure`); **T3** (`Step::elements`, `series_entered`/`series_produced`, `series_marker`, `render_operand` widened to `SeriesNode`, `ValueShape`, `SymbolEntry::shape`/`length`, the S5 element budget); **T5** (`ConstantRewrite<…, SeriesVarNode>`, the series-quantity overlay refusal, a series in the "every node kind" vocabulary method, `sum` as the reduction precedent) | Nothing | After **P12 T5** |
| **6. Dispersion, and the joins with tasks 2–4** | T2, T3, T5 (through task 5) | 2, 3, 4, 5 | After task 5 |
| **7. Outlier rejection: the bounded fixed point** | T2, T3 (the S5 budget for the per-pass trace), T5 | 4 (the bound environment), 5, 6 | After task 6 |
| **8. Table-driven rejection criteria** | Nothing beyond task 7's | 3, 7 | After task 7 |
| **9. Observations as a sample** | **T10** (`MeasuredObservations`, `ObservationsVarNode`/`observations<Q, Capacity>`, `get_observations`, `ValueShape::Observations`, the observations trace step) | 5, 7 | After **P12 T10** and task 7 |
| **10. Guide, example and gallery** | **T11** (the series guide this links to, `write_worked_formula` for series, the `docs.<guide>-output` guard pattern for a series guide) | 1–9 | After **P12 T11** and task 9 |

**Critical path.** P12 T5 → task 5 → 6 → 7 → 8, then P12 T10/T11 → 9 → 10. Tasks 1–4 fill the time before P12 T5. If phase 12 slips at T10, tasks 1–8 are still a complete, shippable statistics layer over series. Task 9 then waits alone, and nothing else waits on it except the guide's observations section.

**Branching.** Phase 13 works in worktree `D:/formula-cpp-statistics`, branch `phase-13-statistics`, off **`phase-12-series`** (R-branch), not off `master`, because task 5 onwards needs phase 12 code. Tasks 1–4 may start from the series branch's current tip, which already carries phase 12's plan and spike and no phase 12 code. Before each phase 13 task that needs a newer phase 12 task, bring the series branch in:
- **rebase** when phase 13's branch is not yet published;
- **merge** when it is. Check whether the commits have been published before rewriting any of them: a branch having no upstream does not mean its commits are unpublished.

Expect two-sided append conflicts, each resolved by keeping both sides:
- the tail of `enum class StepKind`;
- the tail of `struct Step`;
- the `trace_render.hpp` kind switch;
- the `StepKindOf` specialisations;
- the `ConstantRewrite` specialisations;
- `formula.hpp` and the `FILE_SET` lists.

Phase 14 (`phase-14-prep/2026-09-26-context-and-lineage.md`) appends in the same places.

**Measured by task 1 (a simulation, since neither phase 12 nor phase 14 had code commits yet):**

| Pair | `trace.hpp` | `trace_render.hpp` | `overlay.hpp` | `formula.hpp` | `CMakeLists.txt` |
|---|---|---|---|---|---|
| phase 12 vs phase 13 | 2 hunks | 1 | 1 | 1 (`series.hpp` and `statistics.hpp` on adjacent lines) | 0 |
| phase 13 vs phase 14 | 2 | 1 | 1 | 0 | 0 |

Every hunk is a two-sided append, resolved by keeping both sides. Expect one more hunk per extra append site: phase 13 appends at each site for its five headers. **Ruling (lead, 2026-09-26): accepted.**

**Coordination with phase 15 (its ruling R7, adopted here).** Phase 13 keeps its **own** outlier-rejection shape and is **not** expressed as phase 15's bounded retry. Rejection changes *which data* enters an aggregate, and its result carries the rejected positions. A retry recomputes an *expression*. Phase 15 does not depend on phase 13, and phase 13 does not depend on phase 15.
- Phase 15 reserves these `StepKind`s: `RetryAttempt`, `RetryConcluded`, `OpaqueOperation`, `OpaqueOutput`, `AttemptNumber`, `PreviousAttempt`, `ThisAttempt`, `AttemptInput`.
- It reserves these namespace-scope names: `retry`, `opaque`, `opaque_output`, `linear_least_squares`, `attempt_number`, `previous_attempt`, `this_attempt`, `attempt_input`, `starting_from`.
- This plan's names collide with none of them. The full list is in "Names added" (after T15).
- `sizeof(Step<Rational>)`: phase 12 grows it from 776 to 896 bytes. Phase 13's growth on top of that is measured by task 1 step 5 and re-measured by task 7, and stated in both reports.

---

## Design decisions, and the lead's rulings

The spec settles *what* (§16.4 #6/#7) and says nothing about *how*. Each decision gives a recommendation and the reason for it. Items marked **SPIKE** depend on a measurement, which task 1 makes. The lead ruled on R1–R11 on 2026-09-26 ("Ruling:" lines below and the list at the end of this section). Task 2 does not start until task 1's findings have confirmed or amended the SPIKE items.

### T1. A sample is a phase 12 series or observation set, not a new input type

**Recommendation:**
- `concept SampleSource` is satisfied by every phase 12 `SeriesNode` and, from task 9, by `ObservationsVarNode`, plus the rejection node (T4), which is itself a sample.
- No `MeasuredSample` type is added. The input side is phase 12's `MeasuredSeries<Q, N>` (static length) and `MeasuredObservations<Q, Capacity>` (runtime count ≤ capacity).
- A sample evaluates, internally, to `detail::SampleValue<Rep, Capacity> { std::array<Rep, Capacity> values; std::array<std::size_t, Capacity> positions; std::size_t count; }`. `positions` keeps each surviving value's position in the *original* sample, so that a rejection three passes deep still names the element the lab entered.
- This is carried in `EvaluatedSample<Rep, Capacity> = std::expected<std::optional<SampleValue<Rep, Capacity>>, SeriesFailure>`, which mirrors `Evaluated<Rep>`'s `expected<optional<…>>` shape. `SeriesFailure` is phase 12's, **with `element` a `std::optional<std::size_t>`** (lead's ruling, passed to phase 12): a failure that belongs to no element, such as a rejection's limit expression missing its table row, or phase 12's own `sum` overflow, has no position. A made-up position would be a false claim.

**Ruling (R-sample, R-failure): accepted.** Phase 13 reuses `SeriesFailure` with the optional `element` phase 12 now ships, and adds no `SampleFailure` unless a further field is needed.

**Why:** The instruction for this phase is to build on phase 12's API and not invent a competing one. A sample of N specimens *is* a series of length N (a method prescribes three cubes), and a sample of however many determinations were made *is* an observation set. `noexcept` evaluation rules out `std::vector` for the reason S1/S9 gave: `bad_alloc` inside `noexcept` is `std::terminate`.

### T2. Absence is strict: one absent element makes every statistic of that sample absent

**Recommendation:** This is phase 12's S7, applied unchanged to statistics:
- A series sample with any absent element yields an absent `sample_mean`, `sample_variance`, `sample_range` and `sample_count`.
- A rejection over it yields an empty outcome with **no passes run**.
- The trace says `(not measured)`, never `0`, and never "mean of the 5 that were entered".

**Why:** "A formula with one missing input has no answer, and producing one from the inputs that happen to be present is precisely the wrong number" (`measured.hpp`, `combine`). A lab whose method takes as many determinations as were made uses an observation set (task 9), whose count is the count made, and has no absent elements by construction (S9). The two shapes are exactly the two meanings, and the author picks the one the method states.

**`sample_count` is not an exception.** The count of a series with an absent element is not N, and it is not N − 1 either. It is absent.


**Ruling (R-sample): accepted.**

### T3. Sample statistics are `Node`s; which ones, and their definitions

**Recommendation:** Add five `Node`s, each recording **one** trace step over the whole sample (S5's "one step per operation"):

| Factory | Definition | Dimension | Minimum count |
|---|---|---|---|
| `sample_count(s)` | n | `dim::Scalar` (unit `One`) | 0 |
| `sample_mean(s)` | Σx / n | Q | 1 (n = 0 is `DivisionByZero`) |
| `sample_variance(s)` | Σ(x − mean)² / (n − 1) | Q² | 2 (n < 2 is `DomainError`) |
| `sample_range(s)` | max − min | Q | 1 |
| `rounded_sqrt<U, Places, Mode>(x)` | T5 | √dim(x) | — |

- These are generic textbook definitions. The variance is the **sample** variance (n − 1), not the population variance, and the name says "sample".
- **The variance's headroom (measured in task 6 and its review):** it is computed in two passes, which holds to 2³¹ times fixture A's masses where the one-pass form fails at 2²⁵. That advantage is at large magnitudes only: at fine resolution (6 dp in g near 40 g, n = 6) the two-pass form overflows on 460 of 1,000 random samples, always as `Overflow`, never a wrong value. Task 9b carries the case; G6 is the remedy if it is judged realistic.
- There is deliberately **no `sample_stddev` node** (T5 says why). The guide shows `rounded_sqrt<…>(sample_variance(s))` for an exact report, and `sqrt(sample_variance(s))` for a `double` evaluation.
- Mean, variance and count work for any `Rep`, through `RepTraits`. `sample_range` works for any `Rep` too, because it takes a value and makes no decision: a min/max a few ULPs off moves the result by those ULPs, whereas a comparison that picks a branch moves it by a whole row. Everything that *decides* (rejection, `critical_value`) is `Rational`-only, following S15.

**Names.** `sample_mean` and not `mean`: a bare `mean`, `range` or `count` would be shadowed by, or shadow, locals in consumer code (C4459 on cl, `-Wshadow` on GCC), and `range` also meets `std::ranges` in every reader's head.

### T4. Outlier rejection is a sample transformer with a declared bound; abort is the author's verdict

**Recommendation:**

```cpp
formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep,
                          formula::AtMost<2>, formula::KeepAtLeast<4>>(
    formula::series<Mass, 6>,
    formula::deviation_from_mean(formula::rat(6, 100) * formula::pass_mean<Mass>),
    formula::Verdict { "discard the determinations and repeat the test" },
    formula::Citation { .title = "Example Standard", .section = "7.4" })
```

- **It is a sample** (`SampleSource`), whose value is the survivors. `sample_mean(without_outliers<…>(…))` is an ordinary `Node`, usable in a method variant, overlaid and vocabulary-scoped, as S13 requires of a method result.
- **The fixed point:**
  1. Each pass evaluates the criterion over the current sample.
  2. It collects the candidates that exceed.
  3. It rejects either the most extreme candidate or every exceeding one (`PerPass`, required).
  4. It repeats until a pass rejects nothing.
- **Every parameter that shapes the result is required, with no default:**
  - `PerPass`: `MostExtreme` or `EveryExceeding`. Textbook practice and real methods both use both.
  - `OnLimit`: `Keep` or `Reject`. "Deviates by more than" and "deviates by at least" are both written by real methods, and an element exactly on the limit is ordinary data (the S10 precedent).
  - `AtMost<k>`: the most elements that may be rejected in total, k ≥ 1.
  - `KeepAtLeast<m>`: the fewest that may remain, m ≥ 1.
  - Each count is a distinct *type*, so swapping the two is a compile error in this library's words, and no designated-initialiser field can be forgotten into a zero.
- **Termination is guaranteed by the type.** Every pass except the last removes at least one element, and at most k are ever removed, so at most k + 1 passes run. The loop is bounded by `AtMost` and needs no iteration cap of its own.
- **Abort.** When the next rejection would exceed k, or leave fewer than m, the rejection **aborts**:
  - it rejects nothing more;
  - it records a `RejectionAborted` step carrying the author's `Verdict` and naming the element(s) that would have been rejected;
  - in the scalar channel, anything reduced from it fails with `ArithmeticError::DomainError`. This is S8's precedent: the reason survives in the trace, and the scalar channel carries the enum. The rejection's domain is "samples on which the method defines a result", as a banded lookup's is its bands (`lookup.hpp`'s file comment).
- **The full result, `{ value, rejected positions, verdict }`** (§16.4 #6), comes from a top-level evaluator, as `check_conformity` does for conformity:

```cpp
template <Described Result, SampleSource S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<RejectionOutcome<Result, S::capacity>, SeriesFailure>
checked_evaluate_rejection(S const& rejection, Env const& environment, Sink sink = {}) noexcept;
```

  `RejectionOutcome` has these accessors:
  - `outcome()` returns an `Outcome<Result>`. On a settled run it is `value` (`Derived`), the **mean of the survivors**, because the mean is the aggregate every criterion is defined around, so it is exactly the thing the fixed point re-ran. On an abort it is `verdict(declared)`.
  - `rejected()` returns the rejected positions in rejection order, each with the pass that rejected it.
  - `survivors()` and `passes()`.
- **Ties are rejected together.** When two candidates are equally extreme, `MostExtreme` rejects **both** in that pass, and each counts against k. Otherwise which one went would depend on the order the lab typed them in (defect class 6), and every test of a tie runs both input orders.

**Why a transformer and not an aggregate wrapper:** it composes. The same rejected sample feeds `sample_mean`, `sample_variance` and `sample_range`, and a precision check (task 6) can read the survivors. The one cost is that each reduction over the same rejection re-evaluates it and records its steps again, because the trace arena does not memoise (§11 describes a back-reference, and nothing has built it yet). The guide says so, and `checked_evaluate_rejection` exists so that the common report needs one evaluation.

**Why not a `Node` with a verdict channel:** `Evaluated<Rep>` has no verdict alternative, and widening it for every node kind is the change phase 10 refused (`lookup.hpp:74-110`). `Outcome` already has the verdict alternative, so the top-level evaluator gets it for free.


**Ruling (R-transformer, R-bound): accepted**, including that ties are rejected together. No defaults for `PerPass`, `OnLimit`, `AtMost` or `KeepAtLeast`.

### T5. An exactly rounded square root, `rounded_sqrt<U, Places, Mode>(x)`, rather than a `sample_stddev` node. **SPIKE (overflow headroom)**

**The problem:** under the default `Rational`, `sqrt(sample_variance(s))` is `ArithmeticError::Inexact` for almost every real sample (s = √(4057/600) g for fixture B). So a library whose headline is exact arithmetic could not report a standard deviation. Rounding afterwards cannot help, because the root fails before the rounding node sees it.

**Recommendation:** add one fused node, `rounded_sqrt<U, Places, Mode>(radicand)`, which returns the **correctly rounded** decimal of √radicand in `U`:
1. Convert the radicand into U² exactly: divide the SI value by (U's factor)². U must be of dimension √dim(radicand), and a mismatch is refused.
2. If the exact root is rational (`checked_exact_nth_root` succeeds), delegate to `checked_round`, the only path on which a tie can occur.
3. Otherwise the root is irrational, so it cannot be a tie. Let v = a/b be the radicand in U², and S = 10^(2p). **Split the fraction first**, so that a · S never has to fit:
   - q = ⌊a/b⌋ · S + ⌊(a mod b) · S / b⌋;
   - r = ((a mod b) · S) mod b, so that v · S = q + r/b;
   - f = isqrt(q), by a constexpr binary search on `std::uint64_t`.

   Then f / 10^p ≤ √v < (f + 1) / 10^p, strictly on the right.
   - `Floor` and `TowardZero` give f.
   - `Ceiling` and `AwayFromZero` give f + 1.
   - The three `Half*` modes ask whether √(vS) > f + ½, which is vS > f² + f + ¼, which reduces exactly to `q > f² + f || (q == f² + f && 4r > b)`. Equality is impossible, because the root is irrational.

   Every intermediate fits 64 bits whenever ⌊v⌋ · S < 2⁶⁴ and b · S < 2⁶⁴. So the headroom is v up to about 1.8 · 10¹¹ in U² at p = 4, and an integer radicand of 10⁶ up to p = 6. Beyond that the result is `Overflow`. No `__int128` is used, since cl has none.
4. A negative radicand is `DomainError`. Overflow anywhere is `Overflow`, never a wrapped or clamped value.
5. `Rep = double` evaluates `std::sqrt`, then `RepRounding<double>`, and says so in its comment.

The trace records **one** step, `RoundedRoot`: `round(sqrt(#1), to 2 dp of g) [nearest, ties away from zero] = 1.85 g`. The operand is the radicand's step, whose value is exact. No step ever shows an irrational number as if it were exact.

**Why not `sample_stddev`:** a stddev node would either fail `Inexact` or have to carry its own rounding, so it would become `rounded_sqrt` with a narrower name. `rounded_sqrt` is general. It also serves a root-mean-square, or a combined standard uncertainty, and it keeps "which granularity" in the author's declaration, where §16.3 #2 says rounding lives.

**Why not fuse inside `RoundNode`:** a `RoundNode<…, RootNode<2, X>>` overload would make `rounded<…>(sqrt(x))` silently switch algorithms, and the root's own step would then have to display a value it never had. `RoundingRuleNode` (`method.hpp:1546`) derives from `RoundNode`, so a method's rule would switch too, which is action at a distance. A method whose result is a standard deviation declares `rounded_sqrt` as its variant, and the method's rule then re-rounds an already-rounded rational. At the same granularity that is the identity, and the guide says that different granularities double-round.

**Spike result (task 1): confirmed, with the split above.**
- The fixture `static_assert`s pass on cl 19.51, clang-cl and clang++ 22.1.3, and g++ 13.3 and 14.2.
- 200,000 random radicands against a 128-bit reference gave 0 mismatches (g++ only).
- Fixtures A, B and F do **not** kill the mutation `q >= f² + f`. Radicands 21/10 and 23/10 do, and task 2 carries both.

**Ruling (lead, 2026-09-26): accepted**, both the two extra fixtures and splitting the fraction first.


**Ruling (R-root): accepted.**

### T6. Rejection criteria: three generic statistics, each compared with an author-written limit expression

**Recommendation:** a criterion is a closed choice of **statistic** plus an ordinary **limit expression** that the author writes:

| Criterion factory | Statistic for a candidate x in the current pass | Limit's dimension | Decided exactly by |
|---|---|---|---|
| `deviation_from_mean(limit)` | \|x − mean\| | Q | direct comparison |
| `deviation_in_stddevs(limit)` | \|x − mean\| / s | 1 | (x − mean)² vs limit² · s², so no square root, all exact |
| `gap_to_range(limit)` | for the lowest and the highest value only: gap to its neighbour / (max − min) | 1 | direct comparison |

The limit expression is an ordinary `Node`, evaluated **once per pass**, and it may read two placeholders bound by the rejection (T7):
- `pass_mean<Q>`: the current pass's mean. `rat(6, 100) * pass_mean<Mass>` is a relative tolerance.
- `pass_count`: the current pass's n. `critical_value<Sizes, unit::One>(pass_count, {…})` is a critical-value table keyed by the current sample size (task 3).

So a "fixed multiple of s" and a "critical value from a table indexed by n" are **one** criterion, `deviation_in_stddevs`, with two limits. The limit is overlayable (`with_constant<Tolerance>` reaches a `var<Tolerance>` inside it, §16.7) and renderable, and each pass's evaluation of it is in the trace.

- `gap_to_range` examines only the two extremes by definition, so pairing it with `PerPass::EveryExceeding` is refused at compile time. At most two candidates can exist, and "every exceeding" would read as though more could.
- A pass whose range is zero (all values equal) has no outlier. Every criterion then reports no candidates rather than dividing by zero, and a test pins this.
- `deviation_in_stddevs` with n < 3 in a pass: two values are always equidistant from their mean, so every n = 2 pass would be a tie. The pass is refused as `DomainError`, not silently settled, and `KeepAtLeast<m>` with m ≥ 3 is how an author keeps this from being reached. The guide says so.

**Names:** descriptive, not eponymous. The statistics are generic textbook forms. Naming them after a person, or after a test as a standard names it, would invite a reader to expect that standard's tables and conventions, which this library must never ship. See R-names, accepted.

**Not offered:** median-based or robust statistics, trimmed means, and a significance level as a parameter. The significance level is part of the author's table and citation, not the library's arithmetic.


**Ruling (R-criteria, R-names): accepted.** Descriptive names only, everywhere: code, docs, gallery.

### T7. Placeholders bound by an enclosing construct share one mechanism. **SPIKE**

**Recommendation:** `precision_level<Q>` (task 4), `pass_mean<Q>` and `pass_count` (task 7) are empty `Node`s whose `checked_evaluate_si` reads from `detail::BoundEnvironment<Env, Binding...>`, a wrapper around the caller's environment:
- It forwards `get<Q>()`, `is_entered<Q>()`, `provides<Q>` and anything else the existing nodes call on an environment.
- It answers the placeholder itself.
- Task 4 builds it, and task 7 reuses it. It is never written twice.

**Refusals, each gated so that one mistake draws one message (class 2):**
- A placeholder evaluated **outside** its binder, with a plain `Environment`: `"formula: precision_level is meaningful only inside the limit expression of precision_limit"`, and the matching sentence for `pass_mean`/`pass_count` and `without_outliers`.
- A placeholder in the **level** expression of the very `precision_limit` that binds it (the level cannot depend on itself). The same holds for a placeholder inside the **sample** a rejection reads.
- A `precision_level<Q>` whose `Q` has a different dimension from the level expression.
- Nesting one `precision_limit` inside another's limit expression is allowed. The inner binding shadows the outer, and the trace says which level each limit was evaluated at. A test pins that the inner limit reads the inner level.

**The spike measures:**
- that a wrapper environment passes through every shipped node kind: arithmetic, lookups, `when`, rounding, `numeric_value_of` and a method's `RoundingRuleNode`;
- that `if constexpr` detection of "am I inside a binder?" draws exactly one message on cl, clang-cl, clang++ and g++;
- that none of this disturbs `RequireDistinctQuantities` or the gated `RequireProvided` message.

**Spike result (task 1): confirmed.**
- The wrapper needs to forward exactly `provides<Q>`, `is_entered<Q>`, `get<Q>()` and `source_of<Q>()`.
- A placeholder evaluates correctly through arithmetic, both lookups keyed by it, `when`, `rounded`, `numeric_value_of`, `evaluate_method` with its `RoundingRuleNode`, and `check(constraint(…))`.
- A placeholder evaluated against a plain `Environment` draws exactly one library message on all five compilers.
- Not measured: a `RecordingSink` over the placeholder, because the trace registry is closed to probe nodes. Task 4 measures it.

### T8. Two-pass precision limits: `precision_limit<PrecisionKind>(level, limit)` is a `Node`

**Recommendation:**

```cpp
enum class PrecisionKind : std::uint8_t { Repeatability, Reproducibility };

// r as a function of the level (§16.4 #7, the second form):
constexpr auto r = formula::precision_limit<formula::PrecisionKind::Repeatability>(
    (formula::var<ResultA> + formula::var<ResultB>) / formula::rat(2),                        // pass 1: the level
    formula::constant<unit::Gram>(formula::rat(1, 10)) + formula::rat(1, 50) * formula::precision_level<ResultA>); // pass 2

// the check is an ordinary constraint, so it can join a method's ConstraintSet:
constexpr auto agree = formula::constraint(formula::abs(formula::var<ResultA> - formula::var<ResultB>) <= r,
                                           formula::Verdict { "repeat the determinations" });
```

- **Both forms of §16.4 #7 are this one node:**
  - The **constant per level, read from a table**, is `banded_lookup<unit::Gram, LevelBands, unit::Gram>(precision_level<Q>, {…})` as the limit. The lookup's band is selected by the pass-1 level.
  - A constant per **declared** level (a category, not the mean) needs no two-pass at all. It is an `exact_lookup` keyed by the level category, a phase 10 `Node` today, and the guide says so.
  - The **function of the result level** is any expression over `precision_level<Q>`.
- **Why a `Node`, not a new constraint kind:** it gets `Constraint`, `ConstraintSet`, `check_method`, `AcceptanceChecked`, overlays (`with_constraints`, `with_constant`) and the vocabulary for free. The spec's only demand beyond an expression is that the two passes be *declared*, not a coincidence of evaluation order (§16.6), and the node and its two trace steps are that declaration.
- **Why a placeholder at all,** when `rat(a) + rat(b) * ((x1 + x2) / 2)` would compute the same number: the level would then be an anonymous subexpression. It could not be named in the trace ("r at level 40.4525 g"), could not be rounded once and read many times, and a reader could not see that the limit depends on the results it checks. That dependence is exactly what §16.4 #7 says makes this hard.
- **Rounding the level** before it enters the limit is the author's, and it is visible: `precision_limit<…>(rounded<unit::Gram, DecimalPlaces { 0 }, …>(mean), …)`. Fixture P shows that the verdict changes with it.
- **Reproducibility across laboratories** reads results from other laboratories. Reading another test's *record* is phase 14's context (§16.6), and a lineage predicate ("same method, same batch") is §16.8, which is phase 14 and not this phase. In phase 13 the other laboratory's result is an ordinary input quantity the author supplies. `PrecisionKind::Reproducibility` changes the symbol (`R`) and the trace wording, never the arithmetic. The guide says plainly what is not modelled.
- **n > 2 results.** The spread of n results is `sample_range(s)`, and its limit is the author's `critical_value<Sizes, unit::One>(sample_count(s), {…}) * precision_limit<…>(sample_mean(s), …)`. The factor table is author-supplied (task 3). Task 6 carries this join.

**Also added here: `abs(x)`**, a scalar `Node`. A repeatability check of two determinations needs \|x₁ − x₂\|, and the library has no absolute value today (`function.hpp`, `expression.hpp`). It renders `abs(…)` in plain text, `` `abs(…)` `` in Markdown and `\left|…\right|` in LaTeX. The spike checks the name against ADL and `using namespace std;` (T15).


**Ruling (R-precision): accepted.**

### T9. Critical-value tables are keyed by sample size, with author-supplied values and no nearest row

**Recommendation:** `critical_value<Sizes, ResultUnit>(countNode, Corrections<K>)` is a `Node`:
- `Sizes` is a `SampleSizeTable<K> = std::array<std::size_t, K>`, strictly ascending, each size ≥ 1, validated by `RequireValidSampleSizeTable` and by a runtime twin, `sample_size_table_is_well_formed`. This is one predicate used twice, never written twice (phase 10's rule).
- The values are `Corrections<K>`: runtime, author-supplied, with the arity checked, and **never shipped by the library**.
- A count that is not a declared size is a **miss** (`DomainError`): never the nearest row, never interpolated, never extrapolated. That is the lookup precedent, "the loader must say so rather than silently mis-bucket" (§16.3 #3).
- A count that is not a non-negative integer is `DomainError`, and an absent count gives an absent result.
- `Rational`-only (S15).

**Why a new node and not `exact_lookup`:** exact lookups are keyed by compile-time scoped-enum categories (`lookup.hpp:1192`, `RequireScopedEnumKey`). A sample size is a runtime integer that the rejection's own passes change.

**§3 binds here:** no test, example, guide or gallery page may carry a critical value from any published table. The fixtures' tables (below) are invented to make the fixture's decisions differ, and a comment beside each says so, so that nobody "corrects" them toward a real table.

### T10. The trace: every mutation is a step naming what was removed and why

**Recommendation:** a rejection records, in order:
1. The sample's own step. This is phase 12's series step, which already carries its elements within the S5 budget.
2. Per pass, `RejectionPass`: `pass 2: 5 values, mean 39.54 g`. Its operands are the pass's limit-expression steps.
3. Per rejected element, `OutlierRejected`: `rejected element 4 of 6 (45.2 g) in pass 1: abs(x - mean) = 4.716… g > 2.429 g (deviation from mean)`. It carries:
   - the position in the original sample;
   - the value in the unit entered;
   - the statistic and the limit, both exact;
   - the criterion's name.
   For `deviation_in_stddevs` the step shows the exact comparison the decision used, `(x - mean)² = … g² > limit² · s² = … g²`. It shows no rounded G: **no number appears in the trace that the decision did not use.** See R-squared, accepted.
4. Exactly one of these to finish:
   - `RejectionSettled`: `no element exceeds; 2 rejected, 4 remain`;
   - `RejectionAborted`: `element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test`, with the verdict and citation.

**Budget.** The steps are bounded by construction: at most k + 1 passes, k rejections and one terminal step, each counting one unit of `maxSteps` as today. The limit expression's steps repeat per pass, which is O(k × limit size) and bounded. No new render option is added, following S5.

**Record shape: side tables.** A rejection step's data is a `detail::RejectionRecord<Rep>` held in `Trace::rejectionRecords` (a `std::vector`). The step carries only `std::optional<std::size_t> rejectionRecord`, an index into it. A precision step does the same, with `Trace::precisionRecords` and `Step::precisionRecord`.

**Spike result (task 1), `sizeof(Step<Rational>)` on top of phase 12's 896 bytes, identical on cl 19.51, clang++ 22.1.3 and g++ 13.3/14.2:**

| Layout | Size |
|---|---|
| two optional records | 1,072 (+176) |
| loose fields | 1,048 (g++) / 1,056 (MSVC STL) |
| **side tables** | **928 (+32)** |

The first draft's argument that records limit growth was wrong: records cost more than loose fields.

**Ruling (lead, 2026-09-26): side tables**, at +32 bytes per step. Only `RecordingSink` writes a record or an index (class 3). The renderer reads the record through the index, and refuses to print, rather than guess, when an index is out of range, because `Step` and `Trace` are public aggregates. Follow-up **G5** records the same move for phases 12 and 14.

**Positions** are zero-based in every API (`SeriesFailure::element`, `RejectedElement::position`) and **one-based in every text the library writes**: trace, `render()`, `document()` and messages. This is consistent with phase 11's "2nd of 3". Rendering goes through the same helper phase 12 uses for `Step::failedElement`, so the two cannot disagree.

**Ruling (R-squared): accepted.** **Ruling (R-positions): one-based rendering everywhere, zero-based APIs.** Passed to phase 12, whose task 3 onward renders one-based.

### T11. Rendering and `document()`

**Recommendation:**
- `sample_mean(m(i))`, `sample_variance(m(i))`, `sample_range(m(i))` and `sample_count(m(i))` in plain text, where the operand carries phase 12's marker (S14). The results are scalars and unmarked.
- LaTeX spellings, typeset before they are pinned (task 5 step 4):

| Statistic | LaTeX |
|---|---|
| mean | `\overline{{m}_{i}}` |
| variance | `s^{2}({m}_{i})` |
| range | `\operatorname{range}({m}_{i})` |
| count | `n({m}_{i})` |

- `rounded_sqrt`: `round(sqrt(#), to 2 dp of g)` in plain text, as `RoundNode` renders with `sqrt` inside. The mode appears in the trace, not in `render()` (`render.hpp:790`'s precedent).
- `without_outliers`: `without outliers(m(i); abs(x - pass mean) > 6/100 · pass mean; most extreme per pass; keep on limit; at most 2; keep at least 4)`. Every required parameter is stated, because a rendering that omits one states half the rule.
- The placeholders render as **words, not symbols**:

| Placeholder | Plain | LaTeX |
|---|---|---|
| `precision_level` | `level` | `\text{level}` |
| `pass_mean` | `pass mean` | `\bar{x}_{\text{pass}}` |
| `pass_count` | `pass n` | `n_{\text{pass}}` |

  A symbol such as `L` or `n` could collide with an author's own quantity symbol, and phase 11 showed that symbols are jurisdiction-scoped.
- `precision_limit`: `r(0.1 g + 1/50 · level; level = (x_A + x_B) / 2)` in plain text, `R(…)` for reproducibility.
- `document()` lists each statistic's sample in the symbol table (shape and length from phase 12). The page carries the rejection's citation and verdict and a precision limit's kind. A critical-value table's sizes are printed in full, as a formula's declared rows always are.
- No `[` appears in any Markdown spelling, and every new kind runs through the existing Markdown guard (`render_tests.cpp:1313`).
- **No bare `|` in any plain-text or Markdown spelling, the trace included.** An absolute value is `abs(…)` there, and `\left|…\right|` only in LaTeX. Task 1 found that a `|` inside a Markdown table cell **silently truncates the cell**: `| without outliers(`m(i)`; |x − pass mean| > … |` renders as `without outliers(m(i));` and nothing more, with no error (python-markdown 3.10.3, pymdown-extensions 12.1). The Markdown guard gains a "no `|`" rule. Task 4 adds that rule and runs `abs` through it, and task 7 runs the rejection through it.

**Ruling (lead, 2026-09-26): accepted.** `abs(…)` in plain text and Markdown, `\left|…\right|` in LaTeX. Task 1 typeset every LaTeX spelling above clean under MathJax 3.2.2 (site configuration and strict) and under tectonic 0.17.0 with OT1 and `amsmath`.

### T12. Overlays and vocabulary reach every new node

**Recommendation:** each task adds `detail::ConstantRewrite<Sub, N>` for its nodes. Placeholders are leaves with `known = true` via `ConstantRewriteLeaf`, and `critical_value` is a leaf. Each task also adds its nodes to the "every node kind" vocabulary method (`vocabulary_tests.cpp:511`) as soon as a `Node` exists that a method variant can hold:
- tasks 2 and 4 immediately;
- task 3's `critical_value` immediately;
- task 5 onwards through `sample_mean`.

`with_constant<Tolerance>` must reach a `var<Tolerance>` inside a rejection's limit expression. That is the §16.7 case of a jurisdiction changing the tolerance.

A `with_constant<Q>` on the **sample's** quantity is refused, with phase 12 task 5's message (one constant cannot stand for a sample). Phase 13 reuses that refusal and does not write a second one.

### T13. A precision check or a rejection inside a method

**Recommendation:** no change to `Method`:
- A precision check is a `Constraint` (T8), so it joins `ConstraintSet` today.
- A rejection's mean is a `Node` (T4), so it can be a variant. The method's rounding rule then rounds the mean of the survivors.
- An aborted rejection inside `evaluate_method` fails with `DomainError`, and the trace names the verdict.

A method-level `Outcome::verdict` from an aborted rejection, rather than an error, would need `evaluate_method` to return an `Outcome`, which is a phase 11 redesign. It is recorded as follow-up **G1**.

### T14. What this phase does not build (the §16.8 boundary)

- **Bounded retry.** "If the two results differ by more than r, make a third determination" is phase 15's `retry` (its R7 ruling). The guide says how rejection differs: rejection changes *which data* enters an aggregate, while a retry recomputes *an expression*.
- **Best-of-two strategies, set partitioning into balanced subgroups, rolling-window conformity levels:** §16.8 places them downstream.
- **Other laboratories' records and lineage predicates:** phase 14.
- **A general "statistic over a map"** (for example, the mean of per-element ratios) is written with phase 12's elementwise arithmetic first: `sample_mean(series<A, N> / series<B, N>)`. That works because an elementwise node is a `SeriesNode`.

### T15. Names. **SPIKE (for `abs`)**

The new namespace-scope names are:
- factories: `sample_count`, `sample_mean`, `sample_variance`, `sample_range`, `rounded_sqrt`, `critical_value`, `abs`, `precision_limit`, `precision_level`, `without_outliers`, `deviation_from_mean`, `deviation_in_stddevs`, `gap_to_range`, `pass_mean`, `pass_count`, `checked_evaluate_rejection`;
- types: `SampleSizeTable`, `PrecisionKind`, `PerPass`, `OnLimit`, `AtMost`, `KeepAtLeast`, `RejectionOutcome`, `SampleSource`.

The new `StepKind`s are `SampleCount`, `SampleMean`, `SampleVariance`, `SampleRange`, `RoundedRoot`, `SampleSizeLookup`, `AbsoluteValue`, `PrecisionLevel`, `PrecisionLimit`, `PassMean`, `PassCount`, `RejectionPass`, `OutlierRejected`, `RejectionSettled` and `RejectionAborted`. None collides with phase 12's, phase 14's or phase 15's (see Dependencies).

**`abs` specifically.** The spike compiles:
- `formula::abs(var<A> - var<B>)` beside `<cstdlib>`/`<cmath>`;
- unqualified `abs(x)` inside a consumer TU with `using namespace std;` and `using namespace formula;`;
- `abs` as a local variable name, under GCC `-Wshadow` and cl C4459.

If any of these is ambiguous or warns, the name becomes `absolute`, and the spike's report says which compiler forced it.

**Spike result (task 1): keep `abs`.** None of the three cases drew an ambiguity or a warning on cl 19.51, clang-cl or clang++ 22.1.3, or g++ 13.3 or 14.2. The control, a local that shadows a global *variable*, does fire C4459 on cl and `-Wshadow` on clang++ and g++. clang-cl's `/W4` enables no `-Wshadow`.

### Names added (for cross-checking against phases 12, 14 and 15)

Every name below is new in namespace `formula` (or `formula::detail`, where marked) and was checked against the names phase 15 reserves:
- `StepKind`s: `RetryAttempt`, `RetryConcluded`, `OpaqueOperation`, `OpaqueOutput`, `AttemptNumber`, `PreviousAttempt`, `ThisAttempt`, `AttemptInput`;
- namespace-scope names: `retry`, `opaque`, `opaque_output`, `linear_least_squares`, `attempt_number`, `previous_attempt`, `this_attempt`, `attempt_input`, `starting_from`.

It was also checked against phase 12's planned names. **There are no collisions.**

No namespace-scope name is a common local name. There is deliberately no `mean`, `count`, `range`, `variance`, `level`, `limit`, `sample`, `pass`, `outliers`, `critical` or `precision`. Since task 11b, cl C4459 and g++ `-Wshadow` are guarded against consumer globals (`test/consumer_globals_tests.cpp`), so a bare `formula::mean` would collide with every consumer local of that name. The one short name is `abs`, which task 1 step 6 must clear or replace with `absolute` (T15).

**Namespace-scope functions and variable templates:**

| Name | Kind | Task |
|---|---|---|
| `rounded_sqrt` | factory | 2 |
| `critical_value` | factory | 3 |
| `sample_size_table_is_well_formed` | predicate | 3 |
| `abs` (kept, per task 1) | factory | 4 |
| `precision_limit` | factory | 4 |
| `precision_level` | variable template | 4 |
| `sample_count`, `sample_mean` | factories | 5 |
| `sample_variance`, `sample_range` | factories | 6 |
| `without_outliers` | factory | 7 |
| `deviation_from_mean`, `deviation_in_stddevs` | criterion factories | 7 |
| `pass_mean` | variable template | 7 |
| `pass_count` | variable | 7 |
| `checked_evaluate_rejection` | evaluator | 7 |
| `gap_to_range` | criterion factory | 8 |

**Namespace-scope types, concepts and aliases:**

| Name | Kind | Task |
|---|---|---|
| `RoundedRootNode` | node | 2 |
| `SampleSizeTable` | alias | 3 |
| `RequireValidSampleSizeTable` | validation template | 3 |
| `SampleSizeLookupNode` | node | 3 |
| `AbsoluteValueNode`, `PrecisionLevelNode`, `PrecisionLimitNode` | nodes | 4 |
| `PrecisionKind` (`Repeatability`, `Reproducibility`) | enum | 4 |
| `SampleSource` | concept | 5 |
| `EvaluatedSample` | alias | 5 |
| `SampleCountNode`, `SampleMeanNode` | nodes | 5 |
| `SampleVarianceNode`, `SampleRangeNode` | nodes | 6 |
| `PerPass` (`MostExtreme`, `EveryExceeding`) | enum | 7 |
| `OnLimit` (`Keep`, `Reject`) | enum | 7 |
| `AtMost<K>`, `KeepAtLeast<M>` | tag types | 7 |
| `PassMeanNode`, `PassCountNode` | nodes | 7 |
| `DeviationFromMean`, `DeviationInStddevs` | criteria | 7 |
| `CriterionKind` | enum | 7 |
| `RejectionNode` | sample transformer | 7 |
| `RejectionOutcome` | result | 7 |
| `RejectedElement` | result part | 7 |
| `GapToRange` | criterion | 8 |

**In `formula::detail`:**

| Name | Task |
|---|---|
| `rounded_square_root` | 2 |
| `RequireRootUnitMatches` | 2 |
| `BoundEnvironment` | 4 (shared with 7) |
| `PrecisionRecord` | 4 |
| `SampleValue` | 5 |
| `dispatch_sample` | 5 |
| `RejectionRecord` | 7 |

**Optional sink hooks** (member names on `RecordingSink`, not namespace-scope):
- `rounded_root_*`: none; `RoundedRootNode` uses `entered`/`produced`, as every `Node` does;
- `precision_level_entered`, `precision_level_produced`, `precision_limit_entered`, `precision_limit_produced`;
- `rejection_entered`, `rejection_pass`, `outlier_rejected`, `rejection_produced`.

**`StepKind` enumerators (15):** `SampleCount`, `SampleMean`, `SampleVariance`, `SampleRange`, `RoundedRoot`, `SampleSizeLookup`, `AbsoluteValue`, `PrecisionLevel`, `PrecisionLimit`, `PassMean`, `PassCount`, `RejectionPass`, `OutlierRejected`, `RejectionSettled`, `RejectionAborted`.
- Each differs in spelling or case from the factory beside it (`SampleMean` / `sample_mean`).
- Each is still checked on GCC under `-Wshadow`, as `PiConstant` had to be.
- None equals a phase 15 enumerator.

**`Step` members (2):** `std::optional<std::size_t> precisionRecord` (task 4) and `std::optional<std::size_t> rejectionRecord` (task 7), indices into **`Trace` members (2)** `std::vector<detail::PrecisionRecord> precisionRecords` and `std::vector<detail::RejectionRecord<Rep>> rejectionRecords`. Phase 12 takes `sizeof(Step<Rational>)` from 776 to 896 bytes. Task 1 measured phase 13's side-table layout at **928** bytes on cl, clang++ and g++. Task 7 re-measures once the real record exists, and both numbers go into the task reports and into the lead's cross-plan tally.

### T16. Fixtures are invented and tell every alternative apart

See "The shared fixtures" below. Every tolerance, every table value and every coefficient is invented, irregular, and chosen so that each plausible wrong implementation gives a different number. This is class 5, the class that cost phase 11 the most.

### Rulings, in one list (lead, 2026-09-26)

1. **R-sample:** accepted. Series and observation sets are the only sample sources, and absence is strict.
2. **R-transformer:** accepted, including that ties are rejected together.
3. **R-bound:** accepted. No defaults.
4. **R-root:** accepted.
5. **R-criteria:** accepted.
6. **R-names:** accepted. Descriptive names only, everywhere.
7. **R-precision:** accepted.
8. **R-squared:** accepted.
9. **R-positions:** positions are rendered **one-based** in every text the library writes (trace, render, document, messages), consistent with phase 11's "2nd of 3". APIs stay zero-based. Phase 12 applies the same rule from its task 3.
10. **R-branch:** accepted. Branch `phase-13-statistics` off `phase-12-series`, in worktree `D:/formula-cpp-statistics`.
11. **R-failure:** phase 12 makes `SeriesFailure::element` a `std::optional<std::size_t>` now. Phase 13 reuses it and adds no `SampleFailure` unless it needs another field.

---

## Global Constraints

- **C++23**, header-only, no dependency beyond the standard library in shipped headers (§3).
- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. Verify every task on all eight presets: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`, `clang-release`, `gcc-release`, `clang-ubsan`. Iterate on `cl-debug` + `gcc-release`, and hand in after the controller's `verify_all.ps1 -Root <tree>`.
- **Doxygen 1.9.8 (in WSL), zero warnings, and `mkdocs build --strict` (Windows Python) on every task**, not only before the merge.
- **No norm content** (§3):
  - no identifiers, clause numbers, equations, thresholds, **critical values**, precision coefficients or tolerances from any real standard;
  - no eponymous test names (R-names);
  - cite invented `Example Standard` references only.
  - Fixture numbers are those in "The shared fixtures", and **must not be "corrected" toward any published table.**
- **No abort, terminate or assert pop-ups.** Refusals are compile-time `static_assert`s in this library's words, or runtime results. Nothing new may `throw` from a `noexcept` path. Examples link `support/fail_without_dialogs.cpp` (already done by `formula_add_example`).
- **Never hardcode `/std:c++23`.** Never redirect a build to `/dev/null`. `FORMULA_WERROR=ON` everywhere.
- Every negative test asserts **both** that the build fails **and** that the output holds this library's own `static_assert` text. Every negative test gets a **deletion check**: delete the guard, confirm the case compiles, restore with a plain write or `touch`.
- **No `{}` default member initialiser on any member that holds an expression or a node** (defect class 4). That covers every statistic's sample operand, a rejection's sample and criterion, a criterion's limit, `precision_limit`'s level and limit, `abs`'s operand, `rounded_sqrt`'s radicand and `critical_value`'s count operand. `critical_value`'s `Corrections<K>` has no `{}` either.
- **Every sample mutation is a trace step** naming what was removed, its value, the statistic, the limit and the criterion (T10). A rejection with a `NullSink` pays nothing and records nothing, and a `RecordingSink` **always** records every rejection. There is no "quiet" mode.
- **Names.** Under GCC `-Wshadow`, no parameter or local in namespace `formula` code may be named after any name in T15. Under cl C4459, follow afcbf88's convention, and avoid `result`, `value`, `index`, `text`, `step`, `first`, `mark`, `position`, `numerator`, `denominator`, `lhs`, `rhs`, `count`, `mean`, `sample`, `pass`, `level` and `limit` as locals or parameters. Check every new `StepKind` enumerator on GCC under `-Wshadow`, as `PiConstant` had to be. **Every task adds its new entry points to `test/consumer_globals_tests.cpp`**, the task 11b guard that uses the library under globals named like common locals, and adds the matching checks to `consumer_globals_run_tests.cpp`. A new entry point that is not exercised there has not been checked against C4459 in a consumer's build.
- Every new header goes into the install `FILE_SET` (`hygiene.installed-headers` fails otherwise) and into `formula.hpp`. `trace.hpp` and `document.hpp` stay out of the umbrella.
- **Coherent SI and the method's rounding decide every fixture** (defect class 5). Grams become kilograms and variance is in kg². Every assertion names the plausible wrong implementation its value tells apart.
- `unit::Percent` emits a bare `%` in LaTeX. That is a known, tracked follow-up and is **not fixed here**. No test may pin a LaTeX string containing it as correct.
- A quoted compiler diagnostic in `docs/` must come from a real compile (`hygiene.documented-diagnostics`). Every ```` ```text ```` block in the guide must be whole consecutive lines of the example's real output.
- **Constant-evaluation budget.** Phase 12's rule: keep evaluated nodes under about 1,000 per `STATIC_REQUIRE`. For the rejection loop, **keep N × (K + 1) ≤ about 600 per `STATIC_REQUIRE`**. Task 1 measured a bare stand-in of the loop:
  - clang-cl and clang++ 22.1.3 pass 1,280 element-passes and fail at 1,536;
  - cl 19.51 passes 1,728 and fails at 2,304;
  - g++ 13.3 and 14.2 hit no limit at 9,216.

  The library's loop does more per element than the stand-in, hence the margin. Above the limit, use a runtime `REQUIRE`, and say so in the test. **Ruling (lead, 2026-09-26): accepted.**
- **Catch2 splits test filters on commas.** Prove a filter's selection count on the unmutated build before trusting a mutation run.
- **Baseline:** record the test count at the branch point in task 1's findings, and re-record it after each rebase or merge from the series branch. A task reporting fewer tests than the last recorded baseline has lost tests.

## Review Focus

Five input classes that the spec implies and no feature test naturally exercises, most likely first. Each has its pinning test in the owning task.

1. **Every value identical** (a perfectly repeatable sample). Expected: variance 0, range 0, `rounded_sqrt` of 0 is 0 exactly, no criterion finds a candidate (never a division by zero from `gap_to_range` or `deviation_in_stddevs`), the rejection settles in one pass, and a precision check is satisfied. *Pinned in tasks 6 (step 5) and 7 (step 4).*
2. **Two equally extreme values on opposite sides** (a symmetric sample). Expected: `MostExtreme` rejects both in the same pass or aborts on both, and the result is identical in both input orders. *Pinned in task 7 (step 3).*
3. **The smallest samples.** n = 1: mean equals the value, variance is `DomainError`, range is 0. n = 2 under `deviation_in_stddevs`: `DomainError`, not a tie that empties the sample. A rejection that would go below `KeepAtLeast` aborts. A `critical_value` count that is not in the table misses. *Pinned in tasks 3, 6 and 7.*
4. **Exact boundaries.**
   - A value exactly on a rejection limit, under `OnLimit::Keep` and under `OnLimit::Reject`.
   - A spread exactly equal to r.
   - A level exactly on a band boundary of a constant-per-level table.
   - A radicand whose root is exactly representable, which is the only case where `rounded_sqrt` meets a tie.
   *Pinned in tasks 7, 4 and 2.*
5. **An absent determination.** Expected: every statistic is absent, a rejection runs no pass and yields an empty outcome, and nothing reads `0` in the trace. *Pinned in tasks 5 and 7.*

---

## Interfaces that already exist: read, not guessed

In the current tree (`48d03ac`), line numbers as read:

| Fact | Where |
|---|---|
| `Outcome<Q>` with `value`/`empty`/`verdict`/`invalid`, `ValueSource`, `Verdict { std::string_view label }` | `outcome.hpp:32,57,91` |
| `ConstraintOutcome` (four states), `Constraint<P>` (no `{}` on `predicate`), `constraint()`, `check()`, `ConstraintSet`, `check_all()` | `constraint.hpp:67,185,203,233,279,338` |
| `Corrections<N>`: the two-constructor arity idiom, and no `{}` | `lookup.hpp:713-760` |
| `KeyTable` and `ExactLookupNode`: keys are compile-time **scoped enums** (`RequireScopedEnumKey`) | `lookup.hpp:917,1192` |
| `BandedLookupNode`: its operand may be any `Node`, including a placeholder | `lookup.hpp:765` |
| `BreakpointTable`, `breakpoint_table_is_well_formed`: the "one predicate used twice" shape | `lookup.hpp:1360,1441` |
| `ArithmeticError` (`DivisionByZero`, `Overflow`, `NotFinite`, `DomainError`, `Inexact`) | `error.hpp:24-43` |
| `RootNode`, `sqrt()`, `RepFunctions<Rational>::root`, which calls `checked_exact_nth_root` | `function.hpp:58,93,150`, `rational.hpp:568` |
| `RoundNode`, `rounded<>()`, `RepRounding<Rep>`, `checked_round` | `rounding_node.hpp:49,98,121`, `rounding.hpp:255` |
| `RoundingMode` (seven modes) and `DecimalPlaces` | `rounding.hpp:29,75` |
| `evaluate_method`, `check_method` (the `AcceptanceChecked` step), `RoundingRuleNode` derived from `RoundNode` | `method.hpp:1656,1730,1546` |
| `StepKind`, `Step<Rep>` (fields listed at 296–360), `Trace` | `trace.hpp:40,297` |
| `step_value_text`: a step's value in its recorded unit; `coherent()` units carry no symbol | `trace_render.hpp:737`, `evaluate.hpp:41` |
| `StepLimit`: no default, one budget | `trace_render.hpp:54` |
| `Environment::get<Q>()` and the gated `RequireProvided` | `environment.hpp:126,181` |
| `PredicateNode`, the comparison operators over two `Node`s | `predicate.hpp:71,118-160` |

From phase 12's **plan** (names as planned, confirmed at use):

| Fact | Phase 12 task |
|---|---|
| `SeriesNodeBase`, `SeriesNode`, `SeriesVarNode<Q, N>`/`series<Q, N>`, `MeasuredSeries`, `measured_series`, `get_series`, `SeriesValue`, `SeriesFailure { error, element }` (`element` now `std::optional<std::size_t>`, R-failure), `EvaluatedSeries`, `dispatch_series`, `checked_evaluate_series` | T2 |
| `Step::elements`, `Step::failedElement`, `series_entered`/`series_produced`, `SeriesStepKindOf`, `series_marker<D>`, `render_operand` widened to `Node \|\| SeriesNode`, `ValueShape { Single, Series }`, `SymbolEntry::shape`/`length`, the S5 rule that elements share `maxSteps` | T3 |
| Elementwise nodes and operators | T4 |
| `SumNode`/`sum()` (the reduction precedent), `ConstantRewrite` for series nodes, the series-quantity overlay refusal | T5 |
| `MeasuredObservations<Q, Capacity>`, `from(std::span)`, `ObservationsVarNode`/`observations<Q, Capacity>`, `get_observations`, `ValueShape::Observations` | T10 |
| `docs/series.md`, `write_worked_formula` over series, the `docs.series-output` guard | T11 |

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/rounded_root.hpp` *(new, task 2)* | `RoundedRootNode`, `rounded_sqrt<U, Places, Mode>()`, `detail::rounded_square_root` (the exact algorithm) |
| `include/formula-cpp/critical_value.hpp` *(new, task 3)* | `SampleSizeTable`, `sample_size_table_is_well_formed`, `RequireValidSampleSizeTable`, `SampleSizeLookupNode`, `critical_value<…>()` |
| `include/formula-cpp/precision.hpp` *(new, task 4)* | `AbsoluteValueNode`/`abs()`, `detail::BoundEnvironment`, `PrecisionKind`, `PrecisionLevelNode`/`precision_level<Q>`, `PrecisionLimitNode`/`precision_limit<K>()` |
| `include/formula-cpp/statistics.hpp` *(new, tasks 5–6)* | `SampleSource`, `detail::SampleValue`, `EvaluatedSample`, `detail::dispatch_sample`, `SampleCountNode`, `SampleMeanNode`, `SampleVarianceNode`, `SampleRangeNode` and their factories |
| `include/formula-cpp/rejection.hpp` *(new, tasks 7–8)* | `PerPass`, `OnLimit`, `AtMost`, `KeepAtLeast`, the criteria, `PassMeanNode`/`pass_mean<Q>`, `PassCountNode`/`pass_count`, `RejectionNode`/`without_outliers<…>()`, `RejectionOutcome`, `checked_evaluate_rejection` |
| `include/formula-cpp/trace.hpp`, `trace_render.hpp` | new `StepKind`s, `Step::rejection`/`Step::precision`, the hooks, and rendering the new steps |
| `include/formula-cpp/render.hpp`, `document.hpp`, `overlay.hpp` | render, walk and rewrite every new kind |
| `include/formula-cpp/formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt` | umbrella, `FILE_SET`, registration |
| `test/rounded_root_tests.cpp`, `critical_value_tests.cpp`, `precision_tests.cpp`, `statistics_tests.cpp`, `rejection_tests.cpp`, `statistics_cross_tu.hpp`, `statistics_cross_tu_b.cpp` *(new)* | per-surface tests |
| `test/trace_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp`, `overlay_tests.cpp`, `method_tests.cpp` | extended per task |
| `examples/statistics.cpp`, `docs/statistics.md` *(new, task 10)* | the example and the "Statistics, outliers and precision" guide |

The headers are split so that a reviewer can reject one and approve its neighbour:
- `rounded_root.hpp` is scalar numerics, useful without any sample.
- `critical_value.hpp` is a lookup.
- `precision.hpp` is scalar two-pass, with no series dependency, which is why it can land before phase 12's T5.
- `statistics.hpp` is the reductions.
- `rejection.hpp` is the only file that mutates a sample.

## The shared fixtures

Every number below is invented. The arithmetic was checked with Python's `fractions.Fraction`: the passes, means, variances, squared statistics and isqrt values. `Mass` (`"m"`, grams) is an invented quantity, and the determinations are in grams. Positions below are zero-based, as in the API; every rendered text shows them one-based (R-positions).

**Fixture A, "re-running matters"**: 40.2, 39.8, 40.5, 44.0, 40.0, 43.3 g.
- Pass-1 mean 41.3 g, variance 427/125 g², range 4.2 g.
- Under `deviation_from_mean(rat(6, 100) * pass_mean<Mass>)`:
  - pass 1: only element 3 exceeds (2.7 > 2.478). Element 5 is **inside** (2.0 < 2.478).
  - pass 2: mean 1019/25 = 40.76. Element 5 now **exceeds** (2.54 > 2.4456).
  - pass 3: mean 321/8 = 40.125, and nothing exceeds.
- `AtMost<2>` settles at **321/8 g**, rejecting 3 then 5, in 3 passes. `AtMost<1>` **aborts in pass 2**, naming element 5.
- **Kills:** a single-pass rejection, which gives 40.76 and settles; and a limit evaluated once at the pass-1 mean, which never rejects element 5.
- √variance = 1.84824…: to 2 dp, `HalfAwayFromZero` gives 1.85, `Floor` 1.84 and `Ceiling` 1.85.

**Fixture B, "policy matters"**: 40.2, 39.8, 40.5, 45.2, 40.0, 37.2 g.
- Pass-1 mean 2429/60 g, variance 4057/600 g², range 8.0 g.
- Under `deviation_from_mean(rat(6, 100) * pass_mean<Mass>)`, both 3 and 5 exceed in pass 1:
  - `MostExtreme`, `AtMost<1>`: rejects 3. In pass 2 (mean 1977/50 = 39.54) element 5 is **inside** (2.34 < 2.3724), so it settles at **1977/50 g**.
  - `EveryExceeding`, `AtMost<1>`: **aborts in pass 1**, naming elements 3 and 5, and rejects nothing.
  - `EveryExceeding`, `AtMost<2>`: rejects 3 and 5 together, settles at **321/8 g**.
- Under `deviation_in_stddevs(rat(7, 4))`:
  - pass 1: z² for element 3 = 80089/24342 ≈ 3.290 > 49/16 = 3.0625, and element 5 is inside (1.594).
  - pass 2 (mean 1977/50, variance 889/500): element 5's z² = 13689/4445 ≈ **3.0796 > 3.0625**. That is a near-boundary on purpose: a limit taken as 7/4 unsquared, or a population variance, flips it.
  - pass 3 (n = 4, variance 107/1200): max z² = 675/428 ≈ 1.577, so it settles.
  - `AtMost<2>` gives 321/8 g with {3, 5}. `AtMost<1>` aborts in pass 2 on element 5.
- √variance = 2.60032…: to 2 dp, `HalfAwayFromZero` gives 2.60, `Ceiling` 2.61 and `Floor` 2.60. To 3 dp, `Ceiling` gives 2.601. Rounding in SI (kg) instead of g gives 0.00, which fixture B kills.

**Fixture C, "tie"**: 40.0, 40.0, 44.0, 40.0, 36.0 g, and its reverse order 36.0, 40.0, 44.0, 40.0, 40.0 g.
- Mean 40. Elements 2 and 4 (positions in the first order) are both 4.0 away.
- Under a 6 % relative limit, `MostExtreme` + `AtMost<2>` rejects **both** in pass 1 and settles at 40.
- `AtMost<1>` aborts naming both.
- The reversed input gives the same outcome and the same rejected *values*, at mirrored positions.

**Fixture D, "on the limit"**: 40.0, 40.0, 40.0, 40.0, 42.5 g.
- Mean 40.5, deviation of element 4 = 2.0 g exactly.
- `deviation_from_mean(constant<unit::Gram>(rat(2)))` with `OnLimit::Keep` settles at 40.5 with nothing rejected. With `OnLimit::Reject` it rejects element 4 and settles at 40.0.

**Fixture E, "all equal"**: 40.0 × 5 g. Variance 0, range 0, `rounded_sqrt` 0, no candidates under any criterion, and it settles in one pass.

**Fixture F, "exact root"**: 1.0, 2.5, 4.0 g. Variance 9/4 g², √ = 3/2 exactly. At 0 dp: `HalfEven` 2, `HalfTowardZero` 1, `HalfAwayFromZero` 2. This is the only tie path, and it delegates to `checked_round`.

**Critical-value tables**. Invented, and **deliberately unrealistic**: no published table holds values like these, none dips and jumps as they do, and nobody could mistake one for a real table or "correct" it toward one. **Do not adjust them.** (The first draft's values tracked a published outlier table's size and growth too closely; the task 3 review caught it, and the lead ruled them replaced, 2026-09-26.) Sizes {3, 4, 5, 6, 8}. No 7, so a 7-element pass misses.
- **Task 3's own table** (lookup tests only): 10, 30, 20, 50, 40. Every neighbour differs from the next, and the mean of the two rows around the hole (45) is in no row.
- **Task 6's join table** (`statistics_tests.cpp`, the range against a critical value times a precision limit): 10, 11/50, 20, 50, 40. Its n = 4 factor, 11/50, places the four determinations 40.35, 40.45, 40.50 and 40.55 g (range 0.2 g) in the window between the limit at the first result and the limit at the mean: 9977/50000 g = 0.19954 g < 0.2 g ≤ 40007/200000 g = 0.200035 g. Changing the factor closes the window.
- **Deviation table** (used with `deviation_in_stddevs`): 90, 10, 20, 15, 60, read by the author's limit expression **scaled by 1/10**, `critical_value<Sizes, unit::One>(pass_count, {…}) * rat(1, 10)`, so the limits in force are 9, 1, 2, 3/2 and 6. The scale is the author's, visible in the formula; the table's own numbers stay out of every published range.
  - Fixture B, reading the **current** pass's n (correct): pass 1 (n = 6, limit² 9/4) has z² 80089/24342 ≈ 3.290 > 2.25, so reject 3. Pass 2 (n = 5, limit² 4) has z² 13689/4445 ≈ 3.0796 < 4, so it **settles at 1977/50 g**, rejecting {3}. The same holds for `AtMost<1>` and `AtMost<2>`.
  - Reading the **original** n = 6 in every pass (the mutation): pass 2 uses limit² 9/4, where 3.0796 > 2.25 rejects 5. Pass 3 (n = 4) has 675/428 ≈ 1.577 < 2.25 and settles at **321/8 g**, rejecting {3, 5}. With `AtMost<1>` it **aborts** in pass 2.
  - So this table is the kill for "original n". The arithmetic was checked with `fractions.Fraction`.
- **Gap table** (used with `gap_to_range`): 900, 700, 30, 45, 5, read **scaled by 1/100**, so the limits in force are 9, 7, 3/10, 9/20 and 1/20. A gap ratio never exceeds 1, so the limits 9 and 7 are impossible, and every limit is plainly invented, including the ones the trace shows.
  - Fixture B: pass 1 (n = 6): high gap 47/80 = 0.5875 > 9/20, so reject 3.
  - Pass 2 (n = 5): low gap 26/33 ≈ 0.788 > 3/10, so reject 5.
  - Pass 3 (n = 4): max 3/7 ≈ 0.429 < 7, so it settles at 321/8 g, after three passes.
  - Reading the original n here gives the same outcome (9/20 at every pass still rejects 5, and 3/7 < 9/20 settles at pass 3). This table therefore pins the three-pass `gap_to_range` trajectory and the exact gap ratios, and does **not** kill "original n". The deviation table does that.
- **Fixture G, "no row"**: any seven-element sample. n = 7 is not a declared size, so the first pass misses (`DomainError`), and the trace names n = 7.

**Fixture P, "the level decides"**: `ResultA` = 40.0 g, `ResultB` = 40.905 g, so d = 0.905 g.
- r(level) = 0.1 g + level/50.
- With level = mean = 40.4525: r = 0.90905, so **satisfied**.
- With level = `ResultA` (a first-result mutation): r = 0.9, so **violated**.
- With level rounded to 0 dp (40): r = 0.9, so **violated**. That shows the author's intermediate rounding is visible and decisive.
- With the placeholder unbound (read as 0): r = 0.1, so violated.

**Fixture Q, "constant per level from a table"**:
- Level bands [0, 20) g → r = 0.5 g, [20, 60) g → r = 0.9 g, [60, 100) g → r = 1.4 g.
- `ResultA` = 19.6, `ResultB` = 20.5: level 20.05 falls in band 2, r = 0.9, d = 0.9, so **satisfied on the closed limit**. Level taken as `ResultA` falls in band 1, r = 0.5, so violated.
- Fixture P's pair gives band 2, r = 0.9, d = 0.905, so violated.

---

## Task 1: Spike: measure what T5, T7, T10, T15 and the rejection budget depend on

**Status: done, 2026-09-26.** Findings are in `D:/formula-cpp/.superpowers/sdd/2026-09-26-statistics/task-1-spike.md`, and the probes are in this worktree's `.superpowers/spike13/`. The design decisions above carry each result, and the lead accepted the spike. The baseline is 766 tests on `cl-debug`. Neither is committed, and neither goes in the tracked tree.

**Files:**
- Create (scratch only): `.superpowers/sdd/phase-13-prep/spike/`, `.superpowers/sdd/phase-13-prep/task-1-spike.md`

**Interfaces:**
- Produces: `task-1-spike.md`, one section per probe. Each gives the exact command, the compiler and version, the observed output and a verdict. It ends with a section on what was not established. The lead amends T5, T7, T10 and T15 from it before task 2 starts.

**Depends on:** nothing. Stand-ins only, because no phase 12 code has merged.

- [ ] **Step 1: Record the baseline.** Record the test count and commit on all eight presets at the branch point.
- [ ] **Step 2: Probe the rejection budget.** In a scratch TU that includes only `formula.hpp`, write a stand-in rejection loop over `std::array<Rational, N>`: mean, then squared deviations, then compare against `limit² · variance`, removing the most extreme, up to k passes. `static_assert` the fixture-B result (321/8, rejected {3, 5}) for N = 6. Then run synthetic samples at N = 12, 20 and 32 with k = 1, 2 and 4.
  - Build on cl, clang-cl, clang++ and g++ with **no** step-limit flags.
  - Record the largest N × (k + 1) that passes on each compiler, and the exact diagnostic where one fails.
  - Bracket each failing size with a passing one, and include a failing control.
- [ ] **Step 3: Probe T5, the exactly rounded square root.** Implement a stand-in of the algorithm in T5 over `std::uint64_t` with checked multiplication.
  - Check it against fixtures A, B and F in every `RoundingMode`.
  - Check it against 1,000 random radicands (rational, up to 10⁶, denominators up to 10⁴) at p = 0…4, comparing with a `long double` reference away from ties, at runtime.
  - Report the largest p at which a radicand of 10⁶ still fits.
  - Report that no `__int128` was used, and compile on all four compilers.
- [ ] **Step 4: Probe T7, the bound environment.** Build a stand-in `BoundEnvironment<Env, Binding>` and a stand-in placeholder node. Evaluate the following through it, and confirm each reads the caller's environment unchanged:
  - `var<Q>`, `constant`, arithmetic;
  - `banded_lookup` with the placeholder as its operand;
  - `interpolating_lookup`, `when(…)`, `rounded<>`, `numeric_value_of`;
  - a method's `RoundingRuleNode`.
  Then compile the placeholder against a plain `Environment`, and count the library messages on each of the four compilers. The target is exactly one, from an `if constexpr` gate.
- [ ] **Step 5: Probe T10, `sizeof(Step<Rational>)`.** Take a copy of phase 12's planned `Step` (with S5's fields added), then add `std::optional<RejectionRecord<Rational>>` and `std::optional<PrecisionRecord>` using the member lists in task 7 and task 4. Measure on cl and g++. Also measure the loose-fields alternative for comparison. Report the numbers, whatever they say.
- [ ] **Step 6: Probe T15, `abs`.** Compile the three cases in T15 on all four compilers under the project's warning flags. Report any ambiguity or warning, by compiler. If one appears, recommend `absolute`.
- [ ] **Step 7: Typeset the T11 spellings.** Typeset each LaTeX spelling in T11 under MathJax 3.2.2 (the site's pinned version) and under tectonic with `\usepackage[OT1]{fontenc}`:
  - `\overline{{m}_{i}}`, `s^{2}({m}_{i})`, `\operatorname{range}({m}_{i})`, `n({m}_{i})`;
  - `\left|x_A - x_B\right|`;
  - `\bar{x}_{\text{pass}}`, `n_{\text{pass}}`, `\text{level}`;
  - a `precision_limit` rendering with a `\frac` level.
  Check the plain and Markdown spellings against the Markdown guard's rules. Recommend one spelling per construct where a candidate fails.
- [ ] **Step 8: Measure the append-conflict set.** Run `git merge-tree` of a scratch branch (with one `StepKind`, one `Step` field and one `ConstantRewrite` specialisation appended) against the series branch's HEAD and, if it exists, phase 14's branch HEAD. List the conflicting hunks.
- [ ] **Step 9: Write "what I did not establish",** for example "not measured on g++-14 or Apple clang (CI only)".
- [ ] **Step 10: Hand the findings to the lead.** No commit.

---

## Task 2: An exactly rounded square root (T5)

**Files:**
- Create: `include/formula-cpp/rounded_root.hpp`, `test/rounded_root_tests.cpp`
- Create (negatives): `test/negative/rounded_sqrt_unit_dimension_mismatch.cpp`, `test/negative/rounded_sqrt_no_factory_dimension_mismatch.cpp`
- Modify: `include/formula-cpp/trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, `test/trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp`, `overlay_tests.cpp`

**Depends on:** task 1's rulings. No phase 12 task.

**Interfaces:**
- Consumes: `checked_exact_nth_root` (`rational.hpp:568`), `checked_round` (`rounding.hpp:255`), `RepRounding<double>`, `RoundingMode`, `DecimalPlaces`.
- Produces:
  - `template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand> struct RoundedRootNode: NodeBase { Radicand radicand; static constexpr Unit unit = U; static constexpr DecimalPlaces places = Places; static constexpr RoundingMode mode = Mode; static constexpr Dimension dimension = U.dimension; };`, with a class-body `static_assert(detail::RequireRootUnitMatches<U, Radicand>::value)`, which refuses when `U.dimension * U.dimension != Radicand::dimension`
  - `template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand> [[nodiscard]] constexpr auto rounded_sqrt(Radicand radicand) noexcept;`
  - `namespace detail { [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_square_root(Rational radicandInUnitSquared, DecimalPlaces places, RoundingMode mode) noexcept; }`: the one implementation, used by the node
  - `checked_evaluate_si<Rep>(RoundedRootNode const&, Env const&, Sink)`
  - `StepKind::RoundedRoot`
  - `ConstantRewrite` for `RoundedRootNode` (an operand node), and `detail::PrecedenceOf` (function-call precedence)

**Prevents:**
- Class 5: fixtures A, B and F separate `Floor`, `Ceiling` and the `Half*` modes, and rounding in SI.
- Class 4: `radicand` has no `{}`.
- Class 1: the report states the compilers the overflow headroom was measured on (task 1).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("rounded_sqrt rounds the exact root correctly, in the unit declared, in every mode", "[rounded_root]")
{
    using formula::RoundingMode;
    // Fixture A's variance, 427/125 g^2, stored in SI as kg^2.
    constexpr auto inputs = formula::environment(formula::Measured<MassSquared> { formula::Rational { 427, 125 } });
    // sqrt = 1.84824... g: Half* -> 1.85, Floor -> 1.84. Rounding in kg would give 0.00.
    constexpr auto half = formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero>(
        formula::var<MassSquared>);
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(half, inputs)->measurement().value() == formula::Rational { 37, 20 });
    constexpr auto down = formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, RoundingMode::Floor>(
        formula::var<MassSquared>);
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(down, inputs)->measurement().value() == formula::Rational { 46, 25 });
}

TEST_CASE("rounded_sqrt separates Ceiling from HalfAwayFromZero (fixture B)", "[rounded_root]")
{
    // 4057/600 g^2 -> 2.60032...: HalfAwayFromZero 2.60, Ceiling 2.61, and at 3 dp Ceiling 2.601.
}

TEST_CASE("an exactly representable root is the only tie, and the mode decides it (fixture F)", "[rounded_root]")
{
    // 9/4 g^2 -> 3/2 exactly: 0 dp HalfEven -> 2, HalfTowardZero -> 1, HalfAwayFromZero -> 2.
    // A Half* implementation comparing (2f+1)^2 with 4v on this path would
    // meet equality; the delegation to checked_round is what decides it.
}

TEST_CASE("the half-way test compares the remainder when the floor sits exactly on f*f + f", "[rounded_root]")
{
    // Task 1: fixtures A, B and F do not kill the mutation `q >= f*f + f`
    // (only 4 of 200,000 random radicands did). These two do:
    // 21/10 at 0 dp HalfAwayFromZero -> 1   (q = 2 = f*f + f, r/b = 1/10 < 1/4)
    // 23/10 at 0 dp HalfAwayFromZero -> 2   (q = 2 = f*f + f, r/b = 3/10 > 1/4)
}
TEST_CASE("rounded_sqrt of a negative radicand is a domain error, never a clamp to zero", "[rounded_root]") { }
TEST_CASE("rounded_sqrt of zero is zero exactly (Review Focus 1)", "[rounded_root]") { }
TEST_CASE("rounded_sqrt of an absent radicand is absent", "[rounded_root]") { }
TEST_CASE("rounded_sqrt reports overflow rather than a wrapped result", "[rounded_root]")
{
    // Task 1: an integer radicand of 10^6 fits at 6 dp and overflows at 7 dp.
    // Pin both sides; the overflow is ArithmeticError::Overflow, never a wrap.
}
TEST_CASE("rounded_sqrt under double is sqrt then the rounding mode", "[rounded_root]") { }
```

- [ ] **Step 2: Run them and confirm they fail to build.** `cmake --build --preset cl-debug`, never redirected.
- [ ] **Step 3: Implement** `detail::rounded_square_root` exactly as T5 states, then the node, the factory and `checked_evaluate_si`. Convert into U² with `checked_convert` on `U`'s factor squared, never by a hand-written ratio.
- [ ] **Step 4: Trace, render, document and vocabulary.**
  - Trace: `round(sqrt(#1), to 2 dp of g) [nearest, ties away from zero] = 1.85 g`.
  - Render: `round(sqrt(s2), to 2 dp of g)` in plain text, with the matching Markdown and LaTeX spellings.
  - Typeset the LaTeX under MathJax 3.2.2 and tectonic (OT1).
  - Add the node to the "every node kind" vocabulary method, and confirm the jurisdiction's symbol reaches all three surfaces.
- [ ] **Step 5: Overlay.** `with_constant<MassSquared>` reaches the radicand, as `ConstantRewriteOperand` does for `RoundNode`.
- [ ] **Step 6: Negatives and deletion checks.** Test a unit of the wrong dimension through the factory and through aggregate initialisation. Each EXPECTs the library's message, and REJECTs the dimension message of any node built inside.
- [ ] **Step 7: Mutations.** Each must fail the named test and nothing else:
  - rounding in SI;
  - a `Half*` comparison off by one (`>=` instead of `>`), which is unkillable on the irrational path by construction, so say so, and show that it *is* killed on fixture F's rational path, where the code must delegate;
  - `Ceiling` treated as `HalfAwayFromZero`;
  - a negative radicand clamped to 0.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.** Message: `feat(rounding): round a square root exactly to a declared granularity`.

---

## Task 3: Critical values keyed by sample size (T9)

**Files:**
- Create: `include/formula-cpp/critical_value.hpp`, `test/critical_value_tests.cpp`
- Create (negatives): `critical_value_sizes_descending.cpp`, `critical_value_sizes_duplicate.cpp`, `critical_value_size_zero.cpp`, `critical_value_short.cpp`, `critical_value_short_no_factory.cpp`, `critical_value_count_not_scalar.cpp`
- Modify: `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, and the surface tests

**Depends on:** task 1's rulings. No phase 12 task.

**Interfaces:**
- Consumes: `Corrections<K>` (`lookup.hpp:714`), the lookup file comment's miss rule, `detail::in_si`.
- Produces:
  - `template <std::size_t K> using SampleSizeTable = std::array<std::size_t, K>;`
  - `[[nodiscard]] constexpr bool sample_size_table_is_well_formed(SampleSizeTable<K> const&) noexcept;`, true when every size is ≥ 1 and sizes strictly ascend
  - `template <SampleSizeTable Sizes> struct RequireValidSampleSizeTable`, which names the offending pair through template arguments, as `RequireBandsAdjacent` does
  - `template <SampleSizeTable Sizes, Unit ResultUnit, Node Count> struct SampleSizeLookupNode: NodeBase { Corrections<Sizes.size()> corrections; Count count; static constexpr SampleSizeTable<Sizes.size()> sizes = Sizes; static constexpr Unit unit = ResultUnit; static constexpr Dimension dimension = ResultUnit.dimension; };`, where the class body asserts `RequireValidSampleSizeTable` and that `Count::dimension == dim::Scalar`
  - `template <SampleSizeTable Sizes, Unit ResultUnit, Node Count> [[nodiscard]] constexpr auto critical_value(Count count, Corrections<Sizes.size()> corrections) noexcept;`
  - `StepKind::SampleSizeLookup`, with `Step::lookupKey` set to the count (the existing field) and `Step::lookupFailure` on a miss
  - `ConstantRewrite` for the node

**Prevents:**
- Class 5: the table has a hole at 7, and the counts tested are on a row, in the hole, below the first row and above the last.
- Class 2: a descending table refuses once, and the arity check is gated behind it.
- Class 4: `corrections` and `count` have no `{}`, and `_no_factory` pins aggregate initialisation.

- [ ] **Step 1: Write the failing test.** The count comes from `var<Specimens>` (unit `One`) because task 5's `sample_count` does not exist yet. Use task 3's table from "The shared fixtures" (sizes {3, 4, 5, 6, 8} → 10, 30, 20, 50, 40, replaced after the task 3 review; the rows below give the values in force):

| count | expected | kills |
|---|---|---|
| 3 | 10 | an off-by-one row (would give 30) |
| 5 | 20 | a table assumed monotone (a binary search that expects ascending values) |
| 6 | 50 | — |
| 8 | 40 | a row read one position early (would give 50) |
| 7 | miss (`DomainError`) | nearest row (50 or 40), interpolation between them (45) |
| 2, 9 | miss | clamping to the ends |
| 5.5 | `DomainError` | truncation to 5 (20), rounding to 6 (50) |
| absent | absent | a default row |

- [ ] **Step 2: Run and confirm it fails. Implement. Run and confirm it passes.** `Rational`-only (S15), refused otherwise with the lookup's existing message shape.
- [ ] **Step 3: Renderings.** `critical(n, at 3, 4, 5, 6, 8)` in plain text. The declared sizes are printed in full and the values are not: they are data, as a lookup's corrections are. Add the Markdown and LaTeX spellings, typeset them, and run them through the Markdown guard. The trace reads `critical(#1) = 50 [critical value at n = 6]`, and a miss reads `no row for n = 7 (declared: 3, 4, 5, 6, 8)`.
- [ ] **Step 4: Page and vocabulary.** The node goes into the "every node kind" method. `document()` shows the declared sizes and the citation of the method.
- [ ] **Step 5: Negatives, deletion checks and mutations.** For the mutations: nearest-row fallback; comparison of the count as ≤ instead of ==; the runtime predicate and the `static_assert` using different checks. For that last one, make `sample_size_table_is_well_formed` accept duplicates and show that the negative `critical_value_sizes_duplicate` still fails, which proves the `static_assert` calls the predicate rather than its own copy.
- [ ] **Step 6: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(lookup): read a critical value from an author's table by sample size, and miss rather than guess`.

---

## Task 4: Precision limits evaluated in two declared passes (T7, T8), with `abs`

**Files:**
- Create: `include/formula-cpp/precision.hpp`, `test/precision_tests.cpp`
- Create (negatives): `precision_level_outside_limit.cpp`, `precision_level_in_own_level.cpp`, `precision_level_dimension_mismatch.cpp`, `precision_limit_no_factory_level_dimension.cpp`
- Modify: `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, `method_tests.cpp`, and the surface tests

**Depends on:** task 1's rulings (T7's spike and T15's `abs` result). No phase 12 task.

**Interfaces:**
- Consumes: `banded_lookup` (a placeholder as its operand), `exact_lookup`, `constraint()`, `check_method`, `detail::dispatch`.
- Produces:
  - `template <Node Operand> struct AbsoluteValueNode: NodeBase { Operand operand; static constexpr Dimension dimension = Operand::dimension; };` and `abs(Operand)`, or `absolute` if task 1 ruled so
  - `namespace detail { template <typename Env, typename... Bindings> class BoundEnvironment; }`, which forwards every environment member the shipped nodes use and answers `bound<Binding>()`. **Shared with task 7. Write it once.**
  - `enum class PrecisionKind : std::uint8_t { Repeatability, Reproducibility };`
  - `template <Described Q> struct PrecisionLevelNode: NodeBase { static constexpr Dimension dimension = Q::dimension; };` and `template <Described Q> inline constexpr PrecisionLevelNode<Q> precision_level {};`
  - `template <PrecisionKind K, Node Level, Node Limit> struct PrecisionLimitNode: NodeBase { Level level; Limit limit; static constexpr PrecisionKind kind = K; static constexpr Dimension dimension = Limit::dimension; };` and `precision_limit<K>(Level, Limit)`
  - `StepKind::AbsoluteValue`, `StepKind::PrecisionLevel`, `StepKind::PrecisionLimit`, plus `Step::precisionRecord` (`std::optional<std::size_t>`, an index into `Trace::precisionRecords`, a `std::vector<detail::PrecisionRecord>`, where `PrecisionRecord { PrecisionKind kind; std::size_t levelStep; }`)
  - `ConstantRewrite` for all three node kinds (`PrecisionLevelNode` is a known leaf)

**Prevents:**
- Class 2: each refusal in T7 is gated, and each negative REJECTs the message that would otherwise follow it: the environment's "provides no value" for an unbound placeholder, and the dimension message for a mismatched level.
- Class 5: fixtures P and Q separate the level taken as the mean from the level taken as either result, from a rounded level and from an unbound level.
- Class 6: a precision check is judged on what the limit *produced* at the level the pass produced, never per operand.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("a precision limit that depends on the level is evaluated at the level of the results it checks", "[precision]")
{
    // Fixture P: 40.0 g and 40.905 g; r(level) = 0.1 g + level / 50.
    constexpr auto level = (formula::var<ResultA> + formula::var<ResultB>) / formula::rat(2);
    constexpr auto r = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        level, formula::constant<unit::Gram>(formula::rat(1, 10)) + formula::rat(1, 50) * formula::precision_level<ResultA>);
    constexpr auto agree = formula::constraint(formula::abs(formula::var<ResultA> - formula::var<ResultB>) <= r,
                                               formula::Verdict { "repeat the determinations" });
    constexpr auto inputs = formula::environment(formula::Measured<ResultA> { formula::rat(40) },
                                                 formula::Measured<ResultB> { formula::Rational { 40905, 1000 } });
    // r = 0.90905 g >= 0.905 g: satisfied. Level taken as ResultA gives
    // r = 0.9 g (violated); an unbound level read as 0 gives 0.1 g (violated).
    STATIC_REQUIRE(formula::check(agree, inputs).is_satisfied());
}

TEST_CASE("rounding the level before it enters the limit is the author's, and it can change the verdict", "[precision]")
{
    // Same pair, level = rounded<unit::Gram, DecimalPlaces{0}, HalfAwayFromZero>(mean) = 40 g
    // -> r = 0.9 g < 0.905 g: violated. Pins that the rounding is honoured
    // and appears as its own step between the two passes.
}

TEST_CASE("a constant per level read from a table uses the band the level falls in (fixture Q)", "[precision]")
{
    // banded_lookup<unit::Gram, LevelBands, unit::Gram>(precision_level<ResultA>, { 0.5, 0.9, 1.4 })
    // 19.6 / 20.5 -> level 20.05 -> band 2 -> r = 0.9, d = 0.9 -> satisfied on the closed limit.
    // Level taken as ResultA would select band 1 (0.5) -> violated.
}

TEST_CASE("a reproducibility limit is the same arithmetic under a different name", "[precision]") { }
TEST_CASE("a nested precision limit reads its own level, not the outer one", "[precision]") { }
TEST_CASE("an absent result makes the level, the limit and the check absent or not checked", "[precision]")
{
    // The check reports NotChecked, never Satisfied (constraint.hpp's rule).
}
TEST_CASE("abs is the absolute value, and keeps the dimension", "[precision]") { }
```

- [ ] **Step 2: Run and confirm they fail. Implement.**
  - The evaluation order is visible. `PrecisionLimitNode`'s `checked_evaluate_si` dispatches `level` (pass 1). It stops there on error or absence. It then builds `BoundEnvironment<Env, LevelBinding<Rep>>` and dispatches `limit` (pass 2) against it.
  - The sink hears `precision_level_entered`/`_produced` around pass 1 and `precision_limit_entered`/`_produced` around the whole. These optional hooks follow the `variant_entered` shape: both in one `requires`.
- [ ] **Step 3: Run the tests and confirm they pass.**
- [ ] **Step 4: Trace.** The two passes read:
  - `#4 level (pass 1 of 2) = 40.4525 g` (operand: the level expression);
  - `#9 r at level #4 (pass 2 of 2) = 0.90905 g` (operands: the level step and the limit steps).

  Reproducibility reads `R at level …`. The placeholder step reads `level = 40.4525 g (bound by #9)`.
- [ ] **Step 5: Renderings.** Add a "no `|`" rule to the Markdown guard (`render_tests.cpp:1313`) and run the Markdown rendering of `abs(x_A - x_B)` through it. In the guide (task 10), a gallery table row holds a precision check, and `mkdocs build --strict` with the page diff shows the row intact. This pins the "no bare `|`" ruling: a bar spelling would silently truncate a table cell (task 1). Pin `r(0.1 g + 1/50 · level; level = (x_A + x_B) / 2)` in plain text, or the spelling task 1 chose, with the Markdown and LaTeX equivalents. Typeset the LaTeX. Read each aloud: a reader must be able to see that the limit depends on the results.
- [ ] **Step 6: The join with a method (phase 11's join lesson).**
  - A method whose `ConstraintSet` holds `agree` checks it through `check_method`, and the `AcceptanceChecked` step's operand is the constraint.
  - An overlay's `with_constant<Coefficient>` reaches a `var<Coefficient>` inside the limit expression.
  - A scoped vocabulary renames `ResultA`/`ResultB` in all three surfaces, and the placeholder stays `level`.
- [ ] **Step 7: Negatives and deletion checks** for the four refusals.
- [ ] **Step 8: Mutations.** Each must fail the named test and nothing else:
  - the level bound to the first operand of the level expression;
  - the placeholder answering 0;
  - pass 2 evaluated against the unbound environment;
  - the nested limit reading the outer level;
  - `abs` returning the operand unchanged, which the fixture must kill: order the pair so that x_A − x_B is negative.
- [ ] **Step 9: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(precision): evaluate a precision limit at the level of the results it checks, in two declared passes`.

---

## Task 5: Samples: count and mean over a series (T1–T3)

**Files:**
- Create: `include/formula-cpp/statistics.hpp`, `test/statistics_tests.cpp`, `test/statistics_cross_tu.hpp`, `test/statistics_cross_tu_b.cpp`
- Create (negatives): `sample_mean_of_single_value.cpp`, `sample_count_of_single_value.cpp`
- Modify: `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, and the surface tests

**Depends on:** **P12 T2, T3 and T5**.

**Interfaces:**
- Consumes: `SeriesNode`, `series<Q, N>`, `detail::dispatch_series`, `EvaluatedSeries`, `SeriesFailure` (P12 T2); `Step::elements`, `series_marker`, `render_operand` over `SeriesNode`, `ValueShape` (P12 T3); `ConstantRewrite<…, SeriesVarNode>` (P12 T5).
- Produces:
  - `template <typename S> concept SampleSource = SeriesNode<S> /* || task 9: observations || task 7: rejection */;`, where every `SampleSource` has `using quantity` (when it has one), `static constexpr Dimension dimension` and `static constexpr std::size_t capacity` (N for a series)
  - `namespace detail { template <typename Rep, std::size_t C> struct SampleValue { std::array<Rep, C> values; std::array<std::size_t, C> positions; std::size_t count; }; }`
  - `template <typename Rep, std::size_t C> using EvaluatedSample = std::expected<std::optional<detail::SampleValue<Rep, C>>, SeriesFailure>;`
  - `detail::dispatch_sample<Rep>(S const&, Env const&, Sink)`, which is `dispatch_series` plus T2's strict absence for a series
  - `template <SampleSource S> struct SampleCountNode: NodeBase { S sample; static constexpr Dimension dimension = dim::Scalar; };` and `sample_count(S)`
  - `template <SampleSource S> struct SampleMeanNode: NodeBase { S sample; static constexpr Dimension dimension = S::dimension; };` and `sample_mean(S)`
  - A `Node`-constrained overload of each factory whose body is one `static_assert`: `"formula: a sample statistic needs a sample (a series or observations), not a single value"`
  - `StepKind::SampleCount`, `StepKind::SampleMean`
  - `ConstantRewrite` for both, via `ConstantRewriteOperand`

**Prevents:**
- Class 2: the single-value refusal is gated, and each negative REJECTs the concept-failure noise.
- Class 5: every fixture element differs, and the mean is non-round (fixture A's 41.3 g, B's 2429/60 g).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("sample_mean averages every element, in coherent SI, and returns to the declared unit", "[statistics]")
{
    constexpr auto inputs = formula::environment(formula::measured_series<Mass>(
        formula::Measured<Mass> { formula::Rational { 402, 10 } }, formula::Measured<Mass> { formula::Rational { 398, 10 } },
        formula::Measured<Mass> { formula::Rational { 405, 10 } }, formula::Measured<Mass> { formula::rat(44) },
        formula::Measured<Mass> { formula::rat(40) }, formula::Measured<Mass> { formula::Rational { 433, 10 } }));
    // Fixture A: 41.3 g. A mean that dropped the last element gives 40.9;
    // one that divided by N - 1 gives 49.56.
    constexpr auto out = formula::checked_evaluate<Mass>(formula::sample_mean(formula::series<Mass, 6>), inputs);
    STATIC_REQUIRE(out->measurement().value() == formula::Rational { 413, 10 });
    STATIC_REQUIRE(formula::checked_evaluate<Count>(formula::sample_count(formula::series<Mass, 6>), inputs)
                       ->measurement().value() == formula::rat(6));
}

TEST_CASE("one absent determination makes the mean and the count absent (T2, Review Focus 5)", "[statistics]")
{
    // Element 2 absent -> both absent. A skip-absent mean gives 41.5 and a
    // count of 5; both are refused by this test.
}

TEST_CASE("a mean of one element is that element; a count of one is one (Review Focus 3)", "[statistics]") { }
TEST_CASE("a statistic over elementwise arithmetic is the statistic of the per-element values", "[statistics]")
{
    // sample_mean(series<A, 3> / series<B, 3>): a series expression is a
    // sample (T14) -- per-element ratios, then their mean.
}
```

- [ ] **Step 2: Run and confirm they fail. Implement. Run and confirm they pass.** An overflowing running sum fails at the element that overflowed, as `SeriesFailure { Overflow, i }`. The reduction relays `Overflow` and the trace keeps the position (S8).
- [ ] **Step 3: Trace.** `sample_mean(#1) = 41.3 g`, where `#1` is phase 12's series step with its elements. A fully absent series reads `(not measured)` and nowhere reads `0`.
- [ ] **Step 4: Renderings.** Pin `sample_mean(m(i))` and `sample_count(m(i))` in plain text and Markdown, and the LaTeX task 1 chose. Typeset the LaTeX. The results carry no marker (S14).
- [ ] **Step 5: Page.** The symbol table row for `m` has `shape == ValueShape::Series` and `length == 6`, once, however many statistics read it.
- [ ] **Step 6: The join (phase 11's lesson, phase 12's S17).** A method variant `sample_mean(series<Mass, 6>)` with a rounding rule to 1 dp:
  - it evaluates to 41.3 g;
  - it renders in a scoped vocabulary with the marker;
  - it documents and traces;
  - `with_constant<Mass>` on it is refused with **phase 12 task 5's** series-quantity message, which is reused and not re-worded.

  Add `sample_mean` and `sample_count` to the "every node kind" vocabulary method.
- [ ] **Step 7: Check across translation units.** Use a statistic formula in `statistics_cross_tu.hpp` from two TUs and **link**, following the repo's `*_cross_tu` pattern.
- [ ] **Step 8: Negatives, deletion checks and mutations.** For the mutations: divide by N instead of the count; skip absent elements; drop the last element. For each, report which test kills it and confirm nothing else does.
- [ ] **Step 9: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(statistics): count and average a sample, strictly, as the bridge from a series to one value`.

---

## Task 6: Dispersion (variance, range) and the joins with tasks 2–4

**Files:**
- Modify: `statistics.hpp`, `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `statistics_tests.cpp`, `precision_tests.cpp`, `rounded_root_tests.cpp`, and the surface tests
- Create (negatives): `sample_variance_of_single_value.cpp`

**Depends on:** tasks 2, 3, 4 and 5.

**Interfaces:**
- Produces:
  - `template <SampleSource S> struct SampleVarianceNode: NodeBase { S sample; static constexpr Dimension dimension = S::dimension * S::dimension; };` and `sample_variance(S)`
  - `template <SampleSource S> struct SampleRangeNode: NodeBase { S sample; static constexpr Dimension dimension = S::dimension; };` and `sample_range(S)`
  - `StepKind::SampleVariance`, `StepKind::SampleRange`
  - `ConstantRewrite` for both

**Prevents:**
- Class 5: variance with n − 1, not n. Fixture A gives 427/125 g², where division by n gives 2135/750 = 427/150, which the test tells apart.
- Class 6: the precision join is judged on the produced limit at the produced level.

- [ ] **Step 1: Write the failing tests.**
  - Fixture A: variance 427/125 g², stored as kg², and range 4.2 g.
  - Fixture B: variance 4057/600 g², range 8.0 g.
  - Fixture E, all equal (Review Focus 1): variance 0, range 0.
  - n = 1: variance `DomainError`, range 0 (Review Focus 3).
  - An absent element: both absent.
- [ ] **Step 2: Run and confirm they fail. Implement. Run and confirm they pass.** Compute the variance in two passes (the mean, then squared deviations). **Measured in task 6 (corrected after it):** the textbook one-pass formula Σx² − (Σx)²/n overflows `Rational` sooner at large *magnitudes* -- fixture A scaled by 2²⁵ where the two-pass form holds to 2³¹; by powers of ten the two part only between 10¹¹ and 10¹², and at 10⁴ neither overflows. At fine *resolution* it is the other way round: dividing by n before squaring puts n² into every deviation's denominator, and at 6 dp in g near 40 g with n = 6 the two-pass variance overflows on 460 of 1,000 random samples (task 6 review), where one-pass holds more often. Test the magnitude case at 2²⁵. The report states the scale at which each overflows.
- [ ] **Step 3: The join with task 2.** `rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero>(sample_variance(series<Mass, 6>))` gives 1.85 g (fixture A) and 2.60 g (fixture B), traces as two steps (variance, then rounded root), and renders `round(sqrt(sample_variance(m(i))), to 2 dp of g)`.
- [ ] **Step 4: The join with tasks 3 and 4, for n > 2 results.** Pin this constraint:

  `constraint(sample_range(series<Mass, 4>) <= critical_value<Sizes, unit::One>(sample_count(series<Mass, 4>), {…}) * precision_limit<PrecisionKind::Repeatability>(sample_mean(series<Mass, 4>), constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * precision_level<Mass>), verdict)`

  Pin it on a four-value sample whose range is chosen to sit between the limit at the true mean and the limit at the first value. Compute and state the numbers in the test comment, with each mutation's value.
- [ ] **Step 5: Review Focus 1.** Fixture E's `rounded_sqrt(sample_variance)` is 0 exactly, and a precision check over it is satisfied.
- [ ] **Step 6: Renderings, trace, page and vocabulary** for both new nodes, as in task 5 steps 3–6.
- [ ] **Step 7: Mutations.** For each, show it fails the named test and nothing else:
  - population variance (divide by n);
  - one-pass variance, which kills the overflow test only;
  - range as last − first, which the fixture kills because the extremes are not at the ends.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(statistics): sample variance and range, joined with exact roots and precision limits`.

---

## Task 7: Outlier rejection, the bounded fixed point (T4, T6, T10)

**Files:**
- Create: `include/formula-cpp/rejection.hpp`, `test/rejection_tests.cpp`
- Create (negatives): `rejection_bounds_swapped.cpp`, `rejection_at_most_zero.cpp`, `rejection_keep_at_least_zero.cpp`, `rejection_limit_dimension_mismatch.cpp`, `pass_mean_outside_rejection.cpp`, `pass_mean_in_rejected_sample.cpp`, `rejection_of_single_value.cpp`
- Modify: `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `statistics.hpp` (`SampleSource` gains the rejection node), `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, `method_tests.cpp`, and the surface tests

**Depends on:** tasks 4 (`BoundEnvironment`), 5 and 6. Phase 12 T3 for the element budget.

**Interfaces:**
- Consumes: `detail::BoundEnvironment` (task 4), `detail::dispatch_sample`, `SampleSource` (task 5), the variance arithmetic (task 6), `Verdict`, `Citation`, `Outcome`.
- Produces:
  - `enum class PerPass : std::uint8_t { MostExtreme, EveryExceeding };`, `enum class OnLimit : std::uint8_t { Keep, Reject };`
  - `template <std::size_t K> struct AtMost { static constexpr std::size_t value = K; };`, `template <std::size_t M> struct KeepAtLeast { static constexpr std::size_t value = M; };`
  - `template <Described Q> struct PassMeanNode: NodeBase { static constexpr Dimension dimension = Q::dimension; };` and `template <Described Q> inline constexpr PassMeanNode<Q> pass_mean {};`
  - `struct PassCountNode: NodeBase { static constexpr Dimension dimension = dim::Scalar; };` and `inline constexpr PassCountNode pass_count {};`
  - `template <Node Limit> struct DeviationFromMean { Limit limit; };` and `template <Node Limit> struct DeviationInStddevs { Limit limit; };`, with factories `deviation_from_mean(Limit)` and `deviation_in_stddevs(Limit)`
  - `template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, SampleSource S, typename Criterion> struct RejectionNode { S sample; Criterion criterion; Verdict verdict {}; Citation citation {}; static constexpr std::size_t capacity = S::capacity; static constexpr Dimension dimension = S::dimension; };`. It is a `SampleSource`, not a `Node`, and `sample` and `criterion` have no `{}`.
  - `without_outliers<P, L, AtMostT, KeepAtLeastT>(S, Criterion, Verdict, Citation = {})`
  - `template <Described Q, std::size_t C> class RejectionOutcome { public: Outcome<Q> outcome() const; std::span<RejectedElement const> rejected() const; std::size_t passes() const; /* survivors() */ };`, where `struct RejectedElement { std::size_t position; std::size_t pass; };` and the state is private. **Only the library builds one** (class 3).
  - `checked_evaluate_rejection<Result>(RejectionNode const&, Env const&, Sink = {})`, returning `std::expected<RejectionOutcome<Result, C>, SeriesFailure>`
  - `StepKind::PassMean`, `PassCount`, `RejectionPass`, `OutlierRejected`, `RejectionSettled`, `RejectionAborted`, and `Step::rejectionRecord` (`std::optional<std::size_t>`, an index into `Trace::rejectionRecords`, a `std::vector<detail::RejectionRecord<Rep>>`, where `RejectionRecord { std::size_t pass; std::size_t sampleSize; std::optional<std::size_t> position; std::optional<Rep> rejectedValue; std::optional<Rep> statistic; std::optional<Rep> limit; bool squared; CriterionKind criterion; PerPass perPass; OnLimit onLimit; std::size_t atMost; std::size_t keepAtLeast; Verdict verdict; }`), with the members as task 1 measured
  - `ConstantRewrite` for the placeholders (known leaves), the criteria and `RejectionNode`, which reaches into the limit only. `with_constant` on the sample's quantity reuses phase 12's refusal.

**Prevents:**
- Class 3: the rejected positions, the pass count and the verdict are readable only through `RejectionOutcome`'s accessors, and the trace fields are built only by `RecordingSink`. `RejectionNode` is a public aggregate, and its verdict and citation are the author's declaration, as a `Constraint`'s are. That is documented.
- Class 5: fixtures A–E.
- Class 6: ties are judged on the pass's full candidate set, and every tie test runs both input orders.
- Class 2: a bound swap refuses once, and the limit's dimension check is gated behind it.

- [ ] **Step 1: Write the failing tests, one per fixture behaviour**

```cpp
TEST_CASE("rejection re-runs the aggregate, and a value inside at first can be rejected later (fixture A)", "[rejection]")
{
    constexpr auto rejection = formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep,
                                                         formula::AtMost<2>, formula::KeepAtLeast<4>>(
        formula::series<Mass, 6>, formula::deviation_from_mean(formula::rat(6, 100) * formula::pass_mean<Mass>),
        formula::Verdict { "discard the determinations and repeat the test" });
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejection, fixtureA);
    // Pass 1 rejects element 3; pass 2 (mean 40.76) rejects element 5, which
    // was INSIDE at pass 1; pass 3 settles. A single-pass rejection settles
    // at 40.76 with only element 3 removed.
    STATIC_REQUIRE(out->outcome().measurement().value() == formula::Rational { 321, 8 });
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[0].position == 3 && out->rejected()[0].pass == 1);
    STATIC_REQUIRE(out->rejected()[1].position == 5 && out->rejected()[1].pass == 2);
    STATIC_REQUIRE(out->passes() == 3);
}

TEST_CASE("the bound turns one rejection too many into the author's verdict, never a value", "[rejection]")
{
    // Fixture A with AtMost<1>: outcome is verdict("discard the determinations
    // and repeat the test"), rejected() == {3}, and sample_mean over the same
    // rejection fails with DomainError.
}

TEST_CASE("most-extreme and every-exceeding differ, and so does the bound (fixture B)", "[rejection]")
{
    // MostExtreme/AtMost<1> -> 1977/50 g, {3}; EveryExceeding/AtMost<1> ->
    // verdict, rejected() empty, abort names 3 and 5; EveryExceeding/AtMost<2>
    // -> 321/8 g, {3, 5} both in pass 1.
}

TEST_CASE("deviation in standard deviations decides exactly, by squares (fixture B)", "[rejection]")
{
    // deviation_in_stddevs(rat(7, 4)): pass 2's z^2 = 13689/4445 > 49/16 by a
    // hair. A limit compared unsquared, a population variance, or a variance
    // that excludes the candidate each flips that decision; state the value
    // each gives in the comment.
}

TEST_CASE("two equally extreme values are rejected together, whatever order they were entered in (fixture C)", "[rejection]") { }
TEST_CASE("an element exactly on the limit is kept or rejected as declared (fixture D)", "[rejection]") { }
TEST_CASE("a sample of equal values has no outlier and settles in one pass (fixture E)", "[rejection]") { }
TEST_CASE("a rejection that would leave fewer than KeepAtLeast aborts", "[rejection]") { }
TEST_CASE("deviation in standard deviations on a two-value pass is a domain error, not a tie that empties the sample", "[rejection]") { }
TEST_CASE("an absent determination runs no pass and yields an empty outcome", "[rejection]") { }
```

- [ ] **Step 2: Run and confirm they fail.**
- [ ] **Step 3: Implement.**
  - The loop is exactly T4's, over a `detail::SampleValue` working copy. Each pass:
    1. binds `pass_mean`/`pass_count` via `BoundEnvironment`;
    2. dispatches the limit **once**;
    3. computes the statistic per element;
    4. collects the candidates (ties included);
    5. checks the bound;
    6. then either removes and records, or aborts and records.
  - `Rational`-only (S15).
  - The loop is statically bounded by `AtMostT::value + 1` iterations, and a comment says why, so that nobody adds a second cap.
- [ ] **Step 4: Run the tests and confirm they pass.** Also run Review Focus 1, 2, 3 and 5, each in its own test case.
- [ ] **Step 5: Trace (T10).** For fixture A, `AtMost<2>`, the rendered trace contains, in order:
  - the series step (elements within the budget);
  - `pass 1: 6 values, mean 41.3 g`;
  - `rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 2.7 g > 2.478 g (deviation from mean)`;
  - `pass 2: 5 values, mean 40.76 g`;
  - `rejected element 6 of 6 (43.3 g) in pass 2: …`;
  - `pass 3: 4 values, mean 40.125 g`;
  - `settled: 2 rejected, 4 remain`.

  The abort variant ends `element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Example Standard, 7.4]`. Positions render one-based (R-positions); the API's zero-based 3 and 5 read "element 4 of 6" and "element 6 of 6". Show that the whole thing stays within one `maxSteps` budget and that a budget of 4 truncates with S5's count.
- [ ] **Step 6: Renderings and the page.** The rejection criterion spells `abs(x - pass mean)`, never bars. The Markdown guard's "no `|`" rule (task 4) runs over the rejection's Markdown rendering, which pins task 1's truncation finding. Pin the T11 plain spelling with every required parameter stated, plus Markdown and LaTeX. Typeset the LaTeX. The page lists the criterion, the limit expression (rendered), the four bounds, the verdict and the citation.
- [ ] **Step 7: The join (phase 11's lesson).** A method variant `sample_mean(without_outliers<…>(series<Mass, 6>, deviation_from_mean(var<Tolerance> * pass_mean<Mass>), verdict))`:
  - evaluates;
  - is overlaid by `with_constant<Tolerance>(rat(3, 100))`, a jurisdiction's tighter tolerance, and the fixture must give a different result for 3 % than for 6 %: state both;
  - renders in a scoped vocabulary;
  - documents and traces.

  An aborted rejection inside `evaluate_method` gives `DomainError`, and the trace names the verdict (T13).
- [ ] **Step 8: Negatives and deletion checks.**
  - `rejection_bounds_swapped` passes `KeepAtLeast<4>, AtMost<2>` and must name both.
  - `pass_mean_in_rejected_sample` puts `pass_mean` inside the sample expression.
- [ ] **Step 9: Mutations.** Each must fail the named test and nothing else:
  - single pass only;
  - the limit evaluated once, before the loop;
  - ties broken by position;
  - `OnLimit` ignored;
  - the abort rejects up to k and settles;
  - `KeepAtLeast` checked after removal rather than before;
  - the statistic compared unsquared.
- [ ] **Step 10: Measure `sizeof(Step<Rational>)` on cl and g++ with the real `RejectionRecord`, and state it in the report against phase 12's 896 bytes and task 1's estimate. Then verify on all eight presets, plus Doxygen and mkdocs, and commit.** Message: `feat(rejection): reject outliers from a sample, re-running the mean until nothing more is rejected or the declared bound aborts`.

---

## Task 8: Table-driven rejection criteria (`gap_to_range`, critical values by pass size)

**Files:**
- Modify: `rejection.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `rejection_tests.cpp`, and the surface tests
- Create (negatives): `gap_to_range_every_exceeding.cpp`, `gap_to_range_limit_not_scalar.cpp`

**Depends on:** tasks 3 and 7.

**Interfaces:**
- Produces: `template <Node Limit> struct GapToRange { Limit limit; };`, `gap_to_range(Limit)`, and `CriterionKind::GapToRange`. The pairing `gap_to_range` + `PerPass::EveryExceeding` is refused: `"formula: gap_to_range examines only the lowest and the highest value; declare PerPass::MostExtreme"`.

**Prevents:**
- Class 5: "read the table at the current pass n" must be told apart from "read it at the original n". The deviation table in "The shared fixtures" is built to do that, and the gap table deliberately does not (it pins the three-pass trajectory instead).
- Class 2: the policy refusal is gated, and the limit-dimension check does not also fire.

- [ ] **Step 1: Write the failing tests.**
  - **The "original n" kill.** Fixture B with `deviation_in_stddevs(critical_value<Sizes, unit::One>(pass_count, { 90, 10, 20, 15, 60 }) * rat(1, 10))`, using the current pass's n. Pass 1 (n = 6, limit² 9/4) rejects 3. Pass 2 (n = 5, limit² 4, z² 13689/4445 ≈ 3.0796) settles at **1977/50 g** with {3}. The table read at the original n = 6 in every pass would reject 5 in pass 2 and settle at 321/8 (or abort under `AtMost<1>`). Put the per-pass arithmetic in the test comment.
  - Fixture B with `gap_to_range(critical_value<Sizes, unit::One>(pass_count, { 900, 700, 30, 45, 5 }) * rat(1, 100))`: three passes, rejecting 3 then 5, settling at 321/8 g. `AtMost<1>` aborts in pass 2. The trace shows each pass's gap ratio as the exact rational (47/80, 26/33, 3/7).
  - A pass whose n is not in the table (fixture G, seven elements) is a **miss**. It is relayed as `SeriesFailure { DomainError, std::nullopt }`, because the failure belongs to the limit and to no element (T1). It is never a default critical value, and the trace names the missing n.
  - Fixture E under `gap_to_range`: a zero range yields no candidate, not a division by zero.
- [ ] **Step 2: Run and confirm they fail. Implement. Run and confirm they pass.** `gap_to_range` computes both extremes' gaps, rejects the larger (both when tied, per T4), and reads its limit once per pass.
- [ ] **Step 3: Trace and renderings.**
  - Trace: `rejected element 6 of 6 (37.2 g) in pass 2: gap / range = 26/33 > 3/10 (gap to range; critical value at n = 5, scaled by 1/100)`. The limit's own lookup step is its operand.
  - Render: `gap to range > critical(pass n, at 3, 4, 5, 6, 8) * 1/100`.
- [ ] **Step 4: Negatives, deletion checks and mutations.** For the mutations: the table read at the original n; only the high extreme examined; the gap measured to the mean instead of the neighbour. For each, show it fails the named test and nothing else.
- [ ] **Step 5: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(rejection): gap-to-range and critical values read at each pass's sample size`.

---

## Task 9: Observations as a sample (a runtime number of determinations)

**Files:**
- Modify: `statistics.hpp` (`SampleSource` gains `ObservationsVarNode`), `rejection.hpp`, `statistics_tests.cpp`, `rejection_tests.cpp`, `document_tests.cpp`

**Depends on:** **P12 T10**, and tasks 5 and 7.

**Interfaces:**
- Consumes: `MeasuredObservations<Q, Capacity>`, `MeasuredObservations::from(std::span<Rational const>)`, `ObservationsVarNode`/`observations<Q, Capacity>`, `get_observations`, `ValueShape::Observations` (P12 T10).
- Produces: `detail::dispatch_sample` over observations, with `capacity = Capacity` and `count` = the runtime count. Every statistic and `without_outliers` accept it with no new factory.

**Prevents:**
- Class 5: the same determinations as a series and as observations give identical results, and a sample filled to exactly `Capacity` works.

- [ ] **Step 1: Write the failing tests.**
  - Fixture A entered as `observations<Mass, 8>` with 6 values gives the same mean, variance, range and rejection outcome as `series<Mass, 6>`.
  - `sample_count` is 6, not 8, which kills "count = capacity".
  - Exactly 8 values fill, and `from()` with 9 returns `std::unexpected`. Phase 12 already pins this, so re-assert it only through a statistic.
  - A rejection's `critical_value<…>(pass_count, …)` sees the runtime n.
- [ ] **Step 2: Run and confirm they fail. Implement. Run and confirm they pass.**
- [ ] **Step 3: Trace and page.** The observations step (phase 12's) is the operand of the statistic's step. The symbol table row has shape `Observations` and length `Capacity`, and the guide explains that `Capacity` is a bound, not a count.
- [ ] **Step 4: Mutations.** For each, show it fails the named test and nothing else:
  - count = capacity;
  - positions taken from the capacity slot rather than the entry order;
  - a series path used for observations, so that a capacity slot left unfilled reads as absent and makes the result absent.
- [ ] **Step 5: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `feat(statistics): take statistics over observations whose count is known only at run time`.

---

## Task 9b: Overflow census: how much 64-bit headroom real formulas leave

Added by the lead on 2026-09-26, at the owner's request. It answers one question with data: **does exact `Rational` over `std::int64_t` leave enough room for norm-shaped formulas, or does formula-cpp need wider intermediates?** Two follow-ups hang on the answer: G6 (128-bit intermediates) and G7 (a fixed-width wide-integer `Rep`). The census builds neither. No arbitrary-precision `BigInt` is in scope: it allocates, which turns `bad_alloc` into `std::terminate` in `noexcept` code, and it cannot be evaluated at compile time.

**Files:**
- Create: `test/overflow_census_tests.cpp`, and `docs/numeric-headroom.md` (added to `mkdocs.yml` navigation).
- Modify (only if Step 1 chooses the hook): `include/formula-cpp/detail/checked_int.hpp`.

**Depends on:** tasks 2–9. It runs last before the guide, so every statistic exists.

**Prevents:**
- Class 1: every headroom figure in the page comes from a run, named with its compiler.
- Class 7: the instrument is fed a case built to use all 63 bits, and must report 0 headroom for it.

- [ ] **Step 1: Choose the instrument, and say why in the report.**
  - **Preferred:** a test-only representation `CensusRational` that wraps `Rational` and specialises `RepTraits`. It records, per evaluation, the largest numerator magnitude and denominator seen, as bits used out of 63. It needs no library change.
  - **Check first** which paths bypass `RepTraits` and compute in `Rational` directly. Candidates are leaf SI conversion, `rounded_sqrt`, the rejection loop and the statistics internals. List them.
  - If the bypassed paths matter, add instead a hook in `detail/checked_int.hpp`, compiled only under `FORMULA_OVERFLOW_CENSUS` and guarded by `if !consteval`. It must cost nothing and add no symbol when the macro is off; prove that by comparing the release object size with the macro off against the base commit.
- [ ] **Step 2: The census set.** Run every formula the repository already has through the instrument:
  - every gallery and example formula from phases 1–11;
  - this phase's fixtures A–F, with the rejection at its largest fixture size;
  - `rounded_sqrt` at 2, 3, 4 and 6 decimal places;
  - phase 12's cumulative sums and interpolation fixtures;
  - two norm-*shaped* synthetic cases written for this task, with the numbers invented:
    - a 20-point mean, variance and range with 3-decimal inputs;
    - **named realistic case, from the task 6 review:** the variance of six masses near 40 g at 6 dp in g (microgram resolution), which overflows on 460 of 1,000 random samples -- for example 40.053270, 39.475922, 39.025798, 40.615904, 39.418416 and 40.131659 g. Report its headroom, and how often it fails, against G6's rule;
    - a 64-element grading curve with cumulative percentages and one interpolation.
  - Phase 15's least-squares fit is added to the census when phase 15 lands (the phase 15 plan carries that note).
- [ ] **Step 3: The instrument's own control.** A hand-built case whose intermediate is exactly `INT64_MAX`, or a product one bit short of it, must report 0 or 1 bits of headroom. A case one step further must report the library's `Overflow` error, never a figure.
- [ ] **Step 4: The page.** `docs/numeric-headroom.md` holds one table: formula, the largest numerator bits, the largest denominator bits, and the headroom (63 minus the larger). Give the compiler and commit it was measured on, and the lead's decision rule: **any realistic case under 8 bits of headroom triggers G6**. State plainly which cases are realistic and which are stress controls.
- [ ] **Step 5: A regression pin.** For the two norm-shaped cases, and for `rounded_sqrt` at 3 places, a test requires headroom ≥ the measured figure minus 4 bits. A later change that quietly eats headroom then fails loudly. Name the mutation it kills: remove the cross-reduction in `checked_mul`, and confirm this test fails.
- [ ] **Step 6: Verify on all eight presets, plus Doxygen and mkdocs, then commit.** Message: `test(numbers): measure how much 64-bit headroom real formulas leave`.

---

## Task 10: Guide, example and gallery

**Files:**
- Create: `examples/statistics.cpp`, `docs/statistics.md` ("Statistics, outliers and precision")
- Modify: `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md` (regenerated, never hand-edited), `mkdocs.yml`, `README.md`, `docs/index.md`, and the docs-output guard

**Depends on:** tasks 1–9, and **P12 T11**.

**The guide is this project's cheapest reachability probe.** Phase 9's most expensive defect was found by a guide author whose natural spelling did not compile. **If a spelling you reach for does not compile, that is a finding about the library.** Report it; do not work around it.

**What a reader is owed**, each decided by a ruling above:
1. What a sample is: a series when the method fixes the count, observations when it does not (T1). Absence is strict, and why (T2).
2. Why there is no `sample_stddev`, and how `rounded_sqrt` reports one exactly (T5). One sentence on double rounding when a method's rule differs from the variant's granularity.
3. Rejection:
   - the fixed point;
   - the four required parameters;
   - why a tie rejects both;
   - how an abort becomes the author's verdict;
   - a real trace of fixture A's two passes, captured, not described (T4, T10).
4. The three criteria, and how one criterion with a table-driven limit covers "a critical value by sample size" (T6). The critical values come from the author's table, never from the library, and the guide shows an obviously invented one.
5. Precision:
   - the two forms;
   - why the level is a placeholder;
   - the fixture P run, showing that rounding the level flips the verdict;
   - how the check joins a method's constraints (T8, T13).
6. What is not modelled (T14):
   - reading another laboratory's record (phase 14);
   - retrying with a further determination (phase 15, and the one-paragraph R7 distinction);
   - partitioning and best-of-two strategies (§16.8).

**Bars inside a Markdown table cell: measure, do not assume.** `abs` and `precision_limit` render as `\left|…\right|` and `\Big|_{…}` in LaTeX only; plain text and Markdown spell `abs(…)` and `r(…; level = …)`, because a bare `|` is a Markdown table's cell delimiter (task 4 ruling). Whether arithmatex shields those LaTeX bars from the table parser when the gallery or the guide puts LaTeX inside a Markdown *table cell* was **not measured** in task 4. Before any page does, build one such cell and check the rendered page. Step 6's `mkdocs build --strict` must include a LaTeX cell holding `abs` and a precision limit, not only the Markdown one.

**Gallery rules, bought in phase 11:** use `write_worked_formula(out, node)` for every worked section. Never hand-type a formula, a rendering or an expected output. Regenerate, and confirm that `gallery.is-current` passes.

- [ ] **Step 1: Write `examples/statistics.cpp`,** ending with `all checks passed: yes`. Register it with `formula_add_example` and a regex pinning every spelling the guide quotes, following `lookup_tables`' pattern.
- [ ] **Step 2: Run it and capture the real output.**
- [ ] **Step 3: Write `docs/statistics.md`,** quoting only captured output. Extend the `docs.<guide>-output` guard to `statistics.md`.
- [ ] **Step 4: Add to the gallery:** a mean with a rounded standard deviation, a rejection (settled and aborted) and a precision check. Regenerate and confirm `gallery.is-current`.
- [ ] **Step 5: Wire the guide into `mkdocs.yml` (nav: "Statistics, outliers and precision", after "Series and grading curves"), `README.md` and `docs/index.md`.**
- [ ] **Step 6: Verify on all eight presets, plus Doxygen 1.9.8 and `mkdocs build --strict`, then commit.** Message: `docs(statistics): add the statistics, outliers and precision guide, example and gallery entries`.

---

## Self-Review

**1. Spec coverage.**
- §16.4 #6:
  - statistics (tasks 5–6);
  - rejection re-runs the aggregate, possibly more than once (task 7, fixture A);
  - it can terminate in abort (task 7, the bound);
  - a bounded fixed point with a terminal invalid state (T4: bounded by `AtMost` by construction);
  - `{ value, rejected_indices, verdict }` (`RejectionOutcome`, task 7).
- §16.4 #7:
  - a constant per method and level from a table (task 4, fixture Q, and the `exact_lookup` note in T8);
  - a function of the result level, two-pass (task 4, fixture P).
- §16.3 #4, conditional aggregation: which observations enter the mean depends on the observations (task 7).
- §16.6: two-pass evaluation is a supported mode (task 4); verdicts in the trace (tasks 4 and 7); every result carries its origin (`Outcome`'s source for a settled rejection is `Derived`).
- §16.7: jurisdiction-overridable tolerances and coefficients (tasks 4 and 7, the overlay joins).
- §11: bounded rendering (T10, using S5's budget); no recursion (steps stay an arena).
- Tier B #8 (interpolation) belongs to phases 10/12, and Tier B #9 (units) to phases 3/8. Neither is in this phase's row.

**Not covered, and correctly not:** other laboratories' records and lineage (phase 14); bounded retry (phase 15); regression (phase 15); §16.8's partitioning, best-of-two and rolling windows (downstream); a method returning a verdict from a rejection rather than an error (G1).

**2. Placeholder scan.** There is no "TBD" and no "similar to Task N". Tasks 2–9 give the key failing test in full, or as a fixture table with every expected value computed. Every value was checked with Python's `fractions.Fraction`, including task 8's "original n" kill (the deviation table 90, 10, 20, 15, 60 scaled by 1/10). Two values are left to the implementer to compute, and each is named where it occurs: task 6 step 4's four-value sample, and task 7 step 7's result at 3 %. Both must be computed exactly and stated in the test comment.

**3. Type consistency.** These names mean one thing throughout:
- `SampleSource`, `detail::SampleValue`, `EvaluatedSample` and `detail::dispatch_sample`;
- `RejectionNode`, `RejectionOutcome`, `RejectedElement { position, pass }` and `checked_evaluate_rejection`;
- `PerPass`, `OnLimit`, `AtMost<K>` and `KeepAtLeast<M>`;
- `PrecisionKind`, `PrecisionLevelNode`/`precision_level<Q>` and `PrecisionLimitNode`/`precision_limit<K>`;
- `detail::BoundEnvironment` (tasks 4 and 7);
- `SampleSizeTable` and `critical_value`;
- `RoundedRootNode`/`rounded_sqrt`;
- `Step::rejectionRecord`/`Trace::rejectionRecords` and `Step::precisionRecord`/`Trace::precisionRecords`.

Phase 12's names are used as its plan spells them.

**4. Review Focus.** There are five lines, each with its test in the owning task: tasks 6/7, 7, 3/6/7, 7/4/2 and 5/7.

**Order.** Every dependency points backwards. Tasks 2–4 depend on no phase 12 code, and so can overlap phase 12's T2–T5. The joins that need both sides live in the later task: task 6 joins 2, 3 and 4 with samples; task 7 joins rejection with methods and overlays; task 9 joins observations.

## Follow-ups: recorded, not scheduled

- **G1. A method whose rejection aborts reports `Outcome::verdict`, not `DomainError`** (T13). This needs `evaluate_method` to return an `Outcome`, which is a phase 11 redesign.
- **G2. Memoised sub-results in the trace** (§11's back-reference). Without memoisation, two reductions over one rejection record the rejection twice (T4).
- **G3. A median or other robust aggregate as the rejection's aggregate,** if a real method needs one. It would come with its own criterion definitions.
- **G4. Fit statistics** (residuals, r²), phase 15's G2, built on this phase's statistics once both have merged.
- **G5. Move phases 12/14's per-kind Step payloads to side tables too** (lead's ruling on T10). Phase 13 keeps its data in `Trace` side tables at +32 bytes per step. Phase 12's element vectors (+120 bytes on every step) and phase 14's fields would shrink `Step` the same way.
- **G6. 128-bit intermediates inside the checked operations, keeping 64-bit storage.** Triggered by task 9b if any realistic case has under 8 bits of headroom. Portable through `__int128` (clang, gcc) and the `_mul128`/`_umul128` intrinsics (cl), with no allocation, and still evaluable at compile time.
- **G7. A fixed-width wide-integer `Rep`** (for example a rational over a 256-bit integer), only if G6 is not enough. It means making `Rational` generic over its integer, including leaf SI conversion, `Outcome`, rounding, lookups and `Step<Rational>`, so it would be a phase of its own. An arbitrary-precision `BigInt` stays out, because it allocates and cannot be evaluated at compile time.
