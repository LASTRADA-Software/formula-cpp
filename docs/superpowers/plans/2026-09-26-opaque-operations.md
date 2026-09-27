# Opaque Operations and Bounded Retry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a formula use a computation that no expression tree can state, such as a least-squares fit whose coefficients feed a reported quantity, and let a method repeat a step until a condition holds. Both stay honest in the trace. An opaque operation shows its name, its citation, its inputs, its outputs and the fact that its inside is not shown. A bounded retry shows every attempt, and it always ends.

**Architecture:**
- An **opaque operation** is a type the library or a consumer declares. It has a static name, declared input shapes, named outputs whose dimensions follow from the inputs' dimensions, and a `noexcept` `compute` that receives **only evaluated input values**, never the environment.
- `opaque<Op>(citation, inputs...)` builds an `OpaqueCall`. That is not a `Node`, because it has several outputs. `opaque_output<"name">(call)` is a `Node`, and it is how a coefficient enters ordinary arithmetic.
- The library, not the consumer, records the operation's trace step, so a consumer's operation is traced **without** opening the closed `detail::StepKindOf` registry (`sink.hpp`'s `detail::dispatch` comment).
- The library ships one operation: ordinary least squares of a curve's values on its domain (`linear_least_squares`).
- A **bounded retry** is a top-level evaluation, as a `Constraint` or a `Method` is, and it is not a `Node`. It evaluates an attempt expression at most `MaxAttempts` times, where `MaxAttempts` is a template argument. Each attempt can read its own number, the previous attempt's result, and, from task 8, the matching element of a series of recorded attempts. A condition is judged after each attempt. The retry ends by accepting a value or by running out of attempts, and running out yields the method's **verdict** through `Outcome<Result>`. That is the first evaluation path that produces `Outcome::verdict`, the arm §16.2 designed it for.

**Tech Stack:** C++23, header-only. Catch2 via CPM. `STATIC_REQUIRE` for compile-time behaviour, plus the `test/negative/` must-not-compile harness, whose asserted message strings are tested API.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md`:
- §16.5 Tier C #10 (regression is not expressible as an expression tree; it must be an opaque named operation over a series);
- §16.5 Tier C #11 (a bounded retry / fixpoint shape, and not a solver);
- §16.8 (the boundary: plant lifecycles, assignment problems and graphical constructions stay downstream);
- §16.2 (evaluation returns a sum type that includes a verdict);
- §17 row 15;
- §11 (the two trace scars).

**Builds on:** the phase 12 plan, `D:\formula-cpp-series\docs\superpowers\plans\2026-09-26-series.md`, and its rulings S1–S17. Every phase 12 name below is quoted from that plan **as planned**. Phase 12 is being executed now, so task 1 checks each name against what actually merged before any other task starts.

**Spike (task 1): done, 2026-09-26.** Findings are in `.superpowers/sdd/2026-09-26-opaque-operations/task-1-spike.md`, and the probes are beside it in `spike/`. Each decision below carries its result as a **Spike result** paragraph, and the rulings made after the plan was written are carried as **Ruling since** paragraphs.

**Read before any task:** `.superpowers/sdd/2026-09-25-methods-and-overlays/defect-classes.md`, including class 9. Every task names the classes its steps are built to prevent. Every implementer's report says, for each of the nine classes, what they checked and how.

---

## Corrections since the plan (2026-09-27, against master `d09657e`)

The plan was written against phase 12 *as planned*. The branch starts from master `d09657e`, the phase 12 merge, and every name the plan quotes was checked against those headers. Where a later section still reads otherwise, **this section wins**.

**C1. Branch point and baseline.** `phase-15-opaque-operations` starts at `d09657e`. `ctest --preset cl-debug` runs **1037** tests there, all passing. A task reporting fewer has lost tests.

**C2. Phase 12's names, as merged.** Every row of "From phase 12, as planned" now exists:

| Name | Where, as merged | Drift from the plan |
|---|---|---|
| `SeriesNodeBase`, `concept SeriesNode` | `expression.hpp:313,319` | none |
| `MeasuredSeries<Q, N>`, `measured_series<Q>(…)` | `environment.hpp:95,173` | none |
| `Environment::get_series<Q, N>()`, `is_entered_series<Q>` | `environment.hpp:617,573` | none |
| `Environment::get_observations<Q, Capacity>()` | `environment.hpp:651` | **new** (raw observations, `binning.hpp`). See C6 |
| `SeriesValue<Rep, N>::elements` | `series.hpp:733` | none |
| `SeriesFailure { error, element, site }` | `series.hpp:764` | **a third field**, `FailureSite site` (`series.hpp:752`). See C7 |
| `EvaluatedSeries<Rep, N>`, `detail::dispatch_series<Rep>` | `series.hpp:800,882` | none |
| `series<Q, N>`, `SeriesVarNode` | `series.hpp:92,65` | none |
| `Step::elements`, `Step::failedElement`, `Step::failureSite` | `trace.hpp:847,853,905` | `failureSite` is new |
| `detail::series_marker<D>` | `render.hpp:282` | line moved |
| `ValueShape` | `document.hpp:36` | none |
| `detail::HearsSeries` (`series_entered`/`series_produced`) | `series.hpp:814` | none |
| `SumNode`, its `checked_evaluate_si` | `series.hpp:699,1171` | none |
| `ConstantRewrite` for series nodes | `overlay.hpp:1637` onwards | none |
| `CurveExpression`, `CurveNode { domainSeries, valueSeries }`, `curve(D, V)` | `curve.hpp:141,166,192` | none |
| `CurveValue<Rep, N> { domain, values }`, `EvaluatedCurve<Rep, N>` | `curve.hpp:465,481` | `EvaluatedCurve` is `std::expected<CurveValue, SeriesFailure>` |
| `detail::dispatch_curve<Rep>`, `detail::HearsCurve` (`curve_entered`/`curve_produced`) | `curve.hpp:498,489` | the curve seam, as merged |
| `domain<U, Points>` | `curve.hpp:130` | **a variable template, not a function**: write `domain<U, Points>`, never `domain<U, Points>()` |
| `RequireOverlaySeesNode`, `ConstantRewrite` | `overlay.hpp:1124,1310` | lines moved |

**C3. A curve evaluates only with `Rep = Rational`** (`detail::require_exact_curve`, `curve.hpp:507`), because ordering and locating points compare. So an opaque call with a `Curve` input is Rational-only too, and task 4's "least squares works in double" cannot go through a curve. Task 4 tests `LinearLeastSquares::compute<double>` directly over spans (the operation stays generic over `Rep`), and pins that `checked_evaluate_si<double>` over a fit draws `require_exact_curve`'s message once.

**C4. A curve's domain must strictly ascend when it is evaluated** (`detail::judge_domain`, `curve.hpp:544`). A repeated or out-of-order point fails the *curve*, with its own `DomainError`, before any fit runs. Three task 4 tests change:
- "the order the points are listed in does not change the fit" calls `LinearLeastSquares::compute<Rational>` directly on the same pairs permuted (4, 1, 7, 2 s). Through a curve that order is the curve's `NotAscending` failure.
- "all domain values equal" through a curve is the curve's `DuplicatePoint` failure, which the opaque step records as **`Propagated`**, not `Own`. The fit's own `DomainError` (S_xx = 0) is reached through a curve by a **one-point** curve, and by all-equal spans through `compute` directly. Review Focus 1 is amended to match.
- An absent point or value stays absent in the curve (S7), so O4's absence rule applies to a curve input unchanged.

**C5. Relaying a failure.** `detail::report_failure<Rep>(node, sink, error)` (`evaluate.hpp:220`) is the one way a node relays an operand's failure, because GCC 13 reports a false `-Wmaybe-uninitialized` on a named `std::expected` passed to the sink and then returned. `OpaqueOutputNode`'s evaluator builds its result in a lambda and hands it to `sink.produced` once, as `SumNode` and `InterpolateAlongNode` do, or uses `report_failure` where it relays.

**C6. `AttemptEnvironment` forwards seven members, not six.** Master's `Environment` has `provides`, `is_entered`, `is_entered_series`, `get`, `get_series`, `get_observations` and `source_of`. `index_of` is still private. Task 6's forwarding guard lists all seven.

**C7. A relayed series failure keeps its position only when it names an element.** `SeriesFailure::site` says what `element` counts. An opaque call relays a series input's failure through `detail::relayed_failure`'s rule (`series.hpp:928`): with the element for `FailureSite::ResultElement`, without it for `InputObservation`.

**C8. Side tables have a precedent on master: `Trace::conformityLimits`** (`trace.hpp:914,958`, rows `{ step; rows }`), read back by `detail::conformity_limits_of(trace, step)` and passed to `detail::step_line(recorded, budget, limits)`. The opaque and retry side tables follow that shape. Phase 13 has not merged, so "as phase 13 does" in O6's ruling reads "as `conformityLimits` does". `sizeof(Step<Rational>)` is re-measured at the branch point in task 3 and pinned by its test; phase 12 added fields after the spike measured 816.

**C9. Author-text escaping** is `detail::escaped_author_text` (`trace_render.hpp:125`), applied by `step_line` through `detail::EscapedStep` to what a `Step` holds. A side-table row is not in the `Step`, so the opaque and retry line helpers escape its author text (a verdict label, a citation) with that same function. The empty-citation words are `detail::noCitationGiven`, `(no citation given)` (`trace_render.hpp:823`). The opaque step reuses it rather than spelling the words again.

**C10. Pass-through units.** `detail::RecordsStep`, `PassedThrough` and `PassesThroughRecordedStep` (`trace.hpp:1119-1146`) decide when a step states its value in its operand's unit. An `OpaqueOutput` step is **not** a pass-through (it selects one of several outputs of another dimension), so it gets no `PassedThrough` entry, and its unit is the coherent unit of its dimension.

**C11. The lookup-key LaTeX note in the ledger is resolved.** Master sets a lookup row's words in `\mathrm{…}` escaped by `detail::latex_math_words` (`detail/latex_math.hpp:59`, `render.hpp:532`), so `\text{fit\_2}` no longer occurs. The opaque LaTeX spelling stays spike step 9's `\text{<name>}(…)_{\text{<output>}}`: a restricted name needs no escaping, and `render.hpp` already sets fixed words in `\text{…}` (`cumulative`'s direction, `when`'s `\text{if }`).

**C12. O1 amendment 3 is still open.** Master's `detail::refused_already<T>()` (`series.hpp:285`), which reads a node's `static constexpr bool refused`, is exactly the marker option (a) describes, and the series, curve and conformity nodes already gate their checks on it. `RequireResultDimension` and `RequireAddendsAgree` do not consult it. Until the lead rules, task 2's `opaque_dimensions_refused` negative builds the output without evaluating it, as planned.

**C13. The numbers rule.** Every size-like fixture value has three significant digits, is not a Renard R40 preferred number in any decade and is not a sieve designation. The fixtures below were changed to follow it:
- the opaque core's readings are now **127, 103, 191, 139 g** (were 12, 7, 19, 9 g; 12 and 7 are sieve designations);
- the least-squares two-point case is now **(1 s, 10.3 mm), (3 s, 13.9 mm)** (was 10 and 13 mm; 10 is R40 100 in another decade);
- the retry fixpoint's step is now **6.08 g** and its tolerance **0.76 g** (were 6 g and 0.75 g; 6.00 is R40);
- the recorded determinations are now **41.3, 43.9, 42.7, 45.7 g** with a tolerance of **1.27 g** (were 41.2, 43.9, 42.5, 45.0 g and 1.5 g; 42.5, 45.0 and 1.50 are R40).

The least-squares fixture's lengths (10.2, 10.9, 12.1, 14.3 mm) already follow the rule and are kept. Its times (1, 2, 4, 7 s) are not size-like and are kept, because every hand-computed coefficient below depends on them.

**C14. Phase 13's overflow census** (`FORMULA_OVERFLOW_CENSUS`) has not merged. The owner-requested least-squares census case before task 9 is deferred until both phases are on master, and is not part of this branch.

---

## Design decisions, and the lead's rulings (2026-09-26)

Each decision gives a recommendation and its reason. **SPIKE** marks a decision that rests on a measurement task 1 makes. Every decision has been ruled on. Task 2 does not start until task 1's findings have confirmed the SPIKE items, or the lead has amended them.

### O1. An opaque operation is a declared type, traced by the library, and open to consumers

**Recommendation:** add a public `concept OpaqueOperation`. A conforming type declares:
- `static constexpr std::string_view name`, which must not be blank (the same `detail::saysSomething` test `numeric_value_of`'s justification uses, `escape.hpp:52`);
- `static constexpr std::array<InputShape, K> shapes`, where `enum class InputShape : std::uint8_t { Single, Series, Curve }`;
- `static constexpr std::array<std::string_view, M> outputs`: the output names, which must be distinct, non-blank and at least one;
- `static consteval std::optional<std::array<Dimension, M>> output_dimensions(std::array<Dimension, K> inputs) noexcept`. It returns `std::nullopt` when the operation does not accept inputs of those dimensions. For a `Curve` input, the curve contributes **two** dimensions, domain then values, so `K` counts dimensions and not arguments. Task 2 pins that rule;
- `template <typename Rep> static constexpr std::expected<std::array<Rep, M>, ArithmeticError> compute(Args...) noexcept`. Here a `Single` input arrives as `Rep`, a `Series` input as `std::span<Rep const>`, and a `Curve` input as two `std::span<Rep const>`, domain first. Every value is in coherent SI.

**Why:**
- §16.5 #10 asks for a *named* operation. A name that lives in the type cannot be set or changed per call, so it cannot be forged (defect class 3).
- `compute` receives values only and never the environment. So everything the operation reads is an input the trace shows, and an overlay that fixes a quantity reaches every use of it (O7). An operation that could read the environment would be an unseen input, and `RequireOverlaySeesNode` (`overlay.hpp:1011`) exists to refuse exactly that.
- The library records the step. A consumer's own `Node` kind cannot appear in a recorded trace today at all: it has no `StepKindOf` entry, which `sink.hpp`'s `detail::dispatch` comment measured on g++ 13.3 and cl 19.51. An opaque operation is therefore the **traced** way for a consumer to add a computation, and the guide says so.
- `noexcept` is **required, and checked with `static_assert(noexcept(...))`**, because evaluation is `noexcept` throughout. A throwing `compute` would call `std::terminate`, and this project forbids that path.

**Not offered:** an operation that returns a verdict, or a series. The first is §16.2's job and belongs to the retry and to constraints. The second would need S13's deferred "method reports a series".

**Ruling (2026-09-26): accepted as recommended.** Consumers may declare their own opaque operations.

**Spike result (step 7): confirmed, with two amendments and one open item.**
1. **The `noexcept` check** works on all five compilers measured (cl 19.51, clang-cl and clang++ 22.1.3, g++ 13.3, g++-14 14.2), each with one message. It lives in `opaque<Op>()`, the factory. The evaluator's body is **gated** by `if constexpr (noexcept(Op::template compute<Rep>(…)))`: without the gate, g++-14 follows our message with seven errors of its own (`~expected()`, "flows off the end"), the behaviour `series.hpp:771` records.
2. **Names are restricted, and not escaped** (from step 9). `name` and every entry of `outputs` must be non-empty and hold only ASCII letters, digits and single spaces. The new refusal `RequireOpaqueNameReadable` enforces this at compile time. The reason: the library's existing LaTeX escaping inside `\text{}` is shown to readers as literal backslashes by the site's MathJax 3.2.2 (step 9), while a restricted name needs no escaping in any dialect. Hyphens and other punctuation were not measured, so they are not allowed.
3. **Open, and needs a ruling: a refused dimension draws two messages when the output is used.** `output_dimensions` returning `std::nullopt` gives one message when the output node is merely built, and **two** when it is used under `checked_evaluate<Result>` on all five compilers. The second is `RequireResultDimension`, because a refused call has no true dimension and any fallback disagrees with something downstream. That is defect class 2. The options:
   - **(a) recommended:** a refused output carries a `detail::DimensionRefused` marker, and the library's dimension checks that read a `Node`'s dimension treat a marked operand as agreeing. Those checks are `RequireResultDimension`, `RequireAddendsAgree`, phase 12's elementwise dimension check, the rounding and escape unit checks, and the lookup key checks. Task 2 lists every such check with a grep, gates each one, and pins it with a negative case that REJECTs the second message. This costs a small edit in several headers.
   - **(b)** document the second message as a known exception. This contradicts class 2 as written.
   - **(c)** refuse the call inside `opaque<Op>()` and give the returned call no `opaque_output` at all. The compiler's own "no matching function" then becomes the second message, which is worse than (b).

### O2. Several outputs: a call is not a `Node`; each output is

**Recommendation:** `opaque<Op>(Citation, inputs...)` returns `OpaqueCall<Op, Inputs...>`, and `opaque_output<"slope">(call)` returns `OpaqueOutputNode<Index, Call>`, a `Node` whose dimension is `Op::output_dimensions(...)[Index]`. An output name the operation does not declare is refused at compile time in this library's words, **once** (defect class 2). The output is chosen by name rather than by index, because `opaque_output<1>` reads as nothing in a method's source, while `opaque_output<"slope">` reads as the method.

The factory is spelled `opaque_output`, not `output`: under GCC `-Wshadow` a namespace-scope `formula::output` would collide with every parameter or local named `output`, and those are common.

**Ruling: accepted as recommended.**

### O3. Each output used re-evaluates its call. **SPIKE (cost only)**

**Recommendation:** `OpaqueOutputNode` evaluates its whole call, so a formula that uses both the intercept and the slope runs the fit twice and records it twice. The trace is then *repetitive*, never false: each copy is the complete derivation of the number beside it.

**Why not share:** the trace is an arena in which a step's operands are claimed exactly once (`trace.hpp`, `marks`/`unclaimed`). A shared sub-result needs the back-reference §11 describes and the recorder does not yet implement. That is a change to the recorder for every node kind, and it is recorded as follow-up **G1**.

**The spike measures** a two-output formula on a 64-point curve, to confirm that it stays inside the constant-evaluation budget phase 12 measured (about 1,800 node-evaluations on clang-cl and clang++ 22.1.3, and about 2,200 on cl 19.51).

**Ruling: accepted.** The trace is repetitive, never false. G1 stays a follow-up.

### O4. Absence and failure follow phase 12's S7 and S8, strictly

**Recommendation:**
- An absent `Single` input, or **any** absent element of a `Series` or `Curve` input, makes the whole call absent. `compute` is then **not called**. The operation cannot choose to skip absent elements, because a fit over "the points someone happened to enter" is the confident wrong number S7 refuses.
- A series input that fails carries its `SeriesFailure`. The call relays the error through the scalar channel, and the trace keeps the element position, just as `sum` does (S8).
- `compute` returning `std::unexpected` is the operation's **own** failure. For example, a fit whose domain values are all equal returns `DomainError`.
- The trace separates the operation's own failure from a relayed one through `enum class OpaqueFailure : std::uint8_t { None, Own, Propagated, Undetermined }`. This follows `LookupFailure` (`trace.hpp`) for the same reason: a step carrying `DomainError` is otherwise ambiguous between "the fit was degenerate" and "an input failed two levels down".

**Ruling: accepted as recommended.**

### O5. The citation is required at every call, and an empty one is shown, not hidden

**Recommendation:** `opaque<Op>(Citation, inputs...)` has no default citation argument. The operation's *name* is static (O1), but *which standard tells you to use it here* belongs to the call site.

A `Citation` is runtime data made of `std::string_view` fields, which cannot be a template argument, so an empty one cannot be refused at compile time. Instead, `render_trace` and `document()` print it as **`no citation given`** rather than printing nothing. That follows `Documentation::replacedBy`'s precedent (`document.hpp:107`): an uncited step must not read the same as a cited one.

**Alternative:** a `FixedString` template argument, `opaque<Op, "Example Standard 12, clause 3">(...)`. That can be refused when blank, but it cannot carry `Citation`'s five fields and it breaks the designated-initialiser style every other citation uses.

**Ruling: accepted as recommended.** The citation is required, and an empty one is shown as `no citation given`. The `FixedString` alternative is not built.

**Ruling since (2026-09-26):** every overlay operation now requires a citation, and an empty one renders **`(no citation given)`**. The opaque call uses the same words and the same parentheses in `render_trace` and `document()`, so that the two surfaces cannot drift. The citation is author text, so it passes through `detail::step_line`'s escaping (the next ruling, under O6).

### O6. The trace step: what went in, what came out, and a marker that cannot be switched off. **SPIKE (spelling)**

**Recommendation:**
- `StepKind::OpaqueOperation` holds the call. Its operands are the input steps. It carries `Step::operationName`, `Step::citation` and `Step::opaqueOutputs` (a `std::vector<OpaqueOutputValue<Rep>>` of `{ std::string_view name; Dimension dimension; std::optional<Rep> value; }`).
- `StepKind::OpaqueOutput` selects one output of the call step it claims.
- `trace_render` always ends an `OpaqueOperation` line with ` [inside not shown]`. The suffix depends only on `kind`, and **no field, option or operation member can turn it off.**
- The step line lists every output, so each output costs one unit of `maxSteps`, as each series element does under S5, and a truncated list says how much it left out.

A dedicated `opaqueOutputs` field rather than a reuse of phase 12's `Step::elements`: those are positions in one series, and these are differently dimensioned named results. One field holding both would have a name that lies about one of them, which is the reasoning of `Step::granularity`'s comment (`trace.hpp`).

**Formula rendering, which the spike decides:**
- Plain `linear least squares(t(i), L(i)).slope`.
- Markdown with the call in backticks.
- LaTeX `\operatorname{linear\ least\ squares}\left({t}_{i}, {L}_{i}\right)_{\text{slope}}`.

The spike typesets each candidate under **MathJax 3.2.2 with the site configuration** and **tectonic with `\usepackage[OT1]{fontenc}`**, and runs each Markdown spelling through the `render_tests.cpp` Markdown guard (no bare `[`, no `](`). An operation name that holds `_`, `%`, `&` or `#` must also typeset. The spike reports whether the existing `detail/name_text.hpp` escaping already covers those characters, or which ones it misses.

**Ruling: accepted as recommended.** Task 1 decides the spellings.

**Ruling since (2026-09-26): per-kind trace data lives in `Trace` side tables keyed by step index, with +0 bytes on `Step`,** as phase 13 does. This replaces the `Step::operationName`, `Step::opaqueOutputs`, `Step::opaqueFailure` and `Step::outputIndex` fields above, and task 7's `Step` fields, with side-table rows (tasks 3 and 7). `Step::kind`, `value`, `error`, `citation`, `operands` and `failedElement` are reused as they are.

**Ruling since (2026-09-26): author text in trace lines is escaped by `detail::step_line`** (`\`, `[`, `]`, `;` and control characters). A verdict label and a citation are author text and go through it. Operation and output names are restricted at compile time (O1, amendment 2), so they contain nothing to escape, but they still go through the same function rather than around it.

**Ruling since (2026-09-26): positions render one-based.** An element position in an opaque step's failure renders one-based, as phase 12's `failedElement` does. See R2 for attempt numbers.

**Spike result (step 10):**
- `sizeof(Step<Rational>)` is 816 bytes at phase 12's `525441c`. The first draft's inline fields would have made it 912 (+96, +11.8 % per step).
- Four side tables make `Trace` 192 bytes instead of 96, an increase of 96 bytes once per trace. Their rows (56, 16, 24 and 40 bytes) exist only for steps of those kinds.
- The figures are identical on cl 19.51, clang++ 22.1.3 (MSVC STL), g++ 13.3 and g++-14 (libstdc++).

**Spike result (step 9): the spellings are decided.** They were measured under MathJax 3.2.2 (site configuration and strict), tectonic 0.17.0 with `[OT1]{fontenc}`, and python-markdown 3.10.3:
- **LaTeX:** `\text{linear least squares}({t}_{i}, {L}_{i})_{\text{slope}}`
- **Plain:** `linear least squares(t(i), L(i)).slope`
- **Markdown:** ``linear least squares(`t(i)`, `L(i)`).slope``, with symbols in their own code spans, as phase 12 does.

`\operatorname{linear\ least\ squares}` also typesets, but it has to rewrite each space as `\ `. With names restricted to letters, digits and single spaces, `\text{…}` needs no rewriting at all.

### O7. An opaque call joins methods, overlays and vocabularies like any node

**Recommendation:**
- `opaque_output<…>(…)` is a `Node`, so it can be a method variant, and a method can round it.
- `ConstantRewrite<Sub, OpaqueOutputNode<I, Call>>` rewrites *through the call's inputs*. Rewriting is sound because `compute` reads nothing else (O1). Without that specialisation, `with_constant` on a method holding a fit would draw `RequireOverlaySeesNode`, a refusal that is correct but needless here.
- A jurisdiction that uses a different fit replaces the variant wholesale, with the shipped `replace_variant`. Nothing new is needed.
- Symbols in the call's inputs follow the vocabulary. The operation's name does **not**: it is the operation's own text, like `numeric(…)`. Translating it would be a vocabulary for operations, which nothing has asked for.

**Ruling: accepted as recommended.**

### O8. The library ships ordinary least squares, and nothing else

**Recommendation:** `LinearLeastSquares` fits `y = a + b·x` to a curve's `(domain, values)` pairs. Its outputs are `"intercept"` (dimension of the values) and `"slope"` (values ÷ domain). It is exact for `Rational` and generic over `Rep` through `RepTraits`.

A domain with fewer than two distinct values returns its own `DomainError`. An intermediate beyond `Rational`'s range returns `Overflow`, which the spike measures (O9).

The factory is `linear_least_squares(CurveExpression, Citation)`.

**Why a curve and not two series:** a curve already pairs a domain with values of equal length (phase 12's S11), and a curve's domain can be a `domain<…>()` the method declares. Two loose series would re-derive that pairing.

**Not offered:** a line through the origin, weighted fits, polynomial fits, residuals, a coefficient of determination, or confidence bands. The last three are statistics (phase 13's territory) and are recorded as follow-up **G2**. The others would be added when a real method needs them, as consumer operations first.

**Ruling: accepted as recommended.** A curve input; outputs `intercept` and `slope` only.

### O9. `Rational` overflow in the fit is measured, not guessed. **SPIKE**

Least squares sums products of every pair, and `Rational` keeps exact numerators and denominators in `std::int64_t` (`rational.hpp`). A fit over many points with awkward denominators can overflow where a `double` fit would not.

**The spike measures** at what size the fit overflows for three data shapes: integers, one decimal place, and mixed denominators such as thirds with sevenths. It uses N = 5, 20 and 64 on cl, clang-cl, clang++ and g++. The guide states the measured result, and task 4 pins it with a test: an overflowing fit returns `Overflow` and never a wrong number. Tests keep N × nodes under phase 12's about 1,000 per `STATIC_REQUIRE`.

**Ruling: accepted as recommended.**

**Spike result (step 3):** the results are identical on all five compilers measured.
- Integers, one decimal place, and thirds mixed with sevenths never overflow for N from 2 to 128.
- Three-decimal readings at a magnitude of about 1,000 first overflow at **N = 34**, and 60 of the 127 sizes overflow, **not monotonically**: N = 64 passes.
- A different denominator on every point first overflows at **N = 15**.
- **Overflow depends on the data far more than on N,** so the guide states no "safe N". It states that an overflowing fit returns `Overflow` and never a number, and that `double` is the fallback representation.
- The **centred** form is used. It failed at the same first N as the uncentred form, at slightly fewer sizes (60 against 63).
- Task 4's overflow test uses the distinct-denominators shape at N = 15.

### R1. The retry is a bounded loop over one attempt expression, with a verdict when it runs out

**Recommendation:**

```cpp
enum class FirstJudged : std::uint8_t { AtFirstAttempt, AtSecondAttempt };

retry<Result, MaxAttempts, FirstJudged::AtSecondAttempt>(
    starting_from(expression),   // optional first argument: attempt 0's value
    attempt,                     // a Node, evaluated once per attempt
    accept,                      // a Predicate, judged after each attempt from FirstJudged on
    Verdict { "repeat the test" },
    Citation { ... });
```

Inside `attempt` and `accept`, three nodes are available:
- `attempt_number` is dimensionless and 1-based;
- `previous_attempt<Result>` is the prior attempt's result, or `starting_from`'s value at attempt 1;
- `this_attempt<Result>` is available in `accept` only.

Task 8 adds `attempt_input<Q>`.

**Why this shape:**
- §16.5 #11 found "repeat a step until a condition holds", and no genuine root-finding. A retry either recomputes from its own last answer (a fixpoint) or reads a new recorded determination each time (task 8). This one shape covers both, and it is not a solver: it never chooses a next guess by any rule other than the author's own expression.
- `FirstJudged` is **required**, and it is an `enum class` with no default. "Repeat until two successive results agree" cannot be judged at attempt 1, and "repeat until the result reaches a threshold" can. Either default would be silently wrong for the other.
- `previous_attempt<Result>` is named by the result quantity, so its dimension is known when the attempt expression is built. `RequireAttemptDimension` refuses an attempt expression whose dimension is not `Result`'s.

**Ruling: accepted as recommended.**

### R2. It always ends, and it ends in exactly one of six ways

**Recommendation:** `MaxAttempts` is a template argument, and the loop is a `for` over it, so there is no path that runs unboundedly. The retry refuses at compile time:
- `MaxAttempts == 0`;
- `MaxAttempts > 64`, the cap (64, per R2's ruling);
- `FirstJudged::AtSecondAttempt` with `MaxAttempts == 1`, a condition that could never be judged.

Each refusal is gated so that one mistake draws one message (defect class 2).

The six ends, `enum class RetryEnd : std::uint8_t`:

| End | When | `Outcome<Result>` |
|---|---|---|
| `Accepted` | `accept` held at attempt *k* | value of attempt *k*, `Derived` |
| `Exhausted` | `accept` never held in `MaxAttempts` | **`verdict`** (the retry's `Verdict`) |
| `NotJudgeable` | `accept` or the attempt value was absent | `empty` |
| `NotRecorded` (task 8) | attempt *k* needed a recorded determination that is absent | `empty` |
| `Failed` | attempt *k* or its judgement failed arithmetically | none: `std::unexpected(RetryFailure { error, attempt })` |
| `ManuallyEntered` | the environment holds `entered<Result>` | value, `ManuallyEntered`; no attempt is run |

**Why a cap:** §16.5 #11 found retries to be "shallow". A cap is what makes a typo such as `retry<R, 100000, …>` a compile error rather than a trace of a hundred thousand steps. 64 is a recommendation, and the spike measures that a 64-attempt retry of a five-node expression fits the constant-evaluation budget, or reports the N at which it stops fitting.

**`NotJudgeable` stops rather than continues:** an absent comparison is not "not yet". It means the retry cannot tell, and continuing would silently convert "unknown" into "try again".

**Ruling: accepted as recommended.** The cap is 64 unless task 1 step 5 measures that 64 does not fit on some compiler. In that case the cap is the largest count that passes on every compiler measured, and the lead is told.

**Spike result (step 5): 64 is confirmed.** A 64-attempt retry of a five-node attempt with a four-node judgement fits one constant expression on all five compilers measured. 256 also passes on cl, clang++ and g++. 512 fails on cl 19.51 and clang++ 22.1.3.

**Ruling since, applied here: positions render one-based, and the API stays zero-based, as `SeriesFailure::element` is.**
- `RetryFailure::attempt` and `RetryOutcome::accepted_at()` are **zero-based positions**. Attempt 4 is `3`.
- `attempts_made()` is a count.
- `attempt_number`, the node, is the method's own 1-based *k*, because it is a value in the author's algebra and not a position.
- Every text the library writes (trace, render, document) says `attempt 4 of 4`.

The tests below are amended to match. **If the lead prefers 1-based attempt positions in the API, as a named exception to the library's rule, say so.** That would be the first exception to the rule.

### R3. The retry is not a `Node`: the main trade-off in the phase

**Recommendation:** evaluate a retry with `checked_evaluate_retry<Rep = Rational>(retry, environment, sink)`, returning `std::expected<RetryOutcome<Result>, RetryFailure>`. `RetryOutcome<Result>` is a class with accessors (`outcome()`, `end()`, `attempts_made()`, `accepted_at()`), not a public aggregate (defect class 3).

**Why:** running out of attempts must produce a *verdict* (§16.2: "repeat the test"). The scalar channel every `Node` shares, `Evaluated<Rep>` (`evaluate.hpp:150`), has no room for one, and phase 10 refused to widen it for every node kind (`lookup.hpp:80-100`). The alternatives are each worse:
- **absence** reads as "not measured", which is false;
- **`DomainError`** reads as a mistake in the data, which is also false;
- a **new `ArithmeticError` enumerator** puts a method's verdict in the arithmetic error channel and loses the verdict's label on the way up.

**What it costs:** a retry cannot sit inside a larger formula, and it cannot be a method variant in this phase. The retry's attempt expression *is* the formula, and `Result` is the reported quantity, which fits the fixpoint and determination cases found. The cost is recorded as follow-up **G3** (a retry as a method variant, which needs a method result that can be a verdict).

**The alternative, considered and not built:** a `RetryNode` whose exhaustion policy the author declares, either `OnExhausted::UseLastAttempt` (a `Node`, whose trace says "not accepted; last attempt used") or a verdict (top level only). That doubles the surface. Recommended against unless a consumer needs a retry inside arithmetic.

**Ruling: accepted.** A retry is not a `Node`. It is evaluated at top level through `checked_evaluate_retry` into `RetryOutcome`. `RetryNode` with `OnExhausted` is **not built**, and G3 stays a follow-up.

### R4. `previous_attempt` with nothing before it is the author's mistake, and says so

**Recommendation:** at attempt 1 with no `starting_from`, `previous_attempt<Result>` fails with its **own** `DomainError`. The retry ends `Failed` at attempt 1, and the trace step reads `previous attempt: none before attempt 1`. It does not read as absent, because absence would read as "not measured".

`previous_attempt`, `this_attempt` and `attempt_number` used outside any retry are refused at compile time. Their evaluator requires `detail::AttemptEnvironment`, and a plain `Environment` draws one message. `this_attempt` inside the *attempt* expression, which would be circular, is refused at compile time by the environment's phase parameter.

**Ruling: accepted as recommended.**

### R5. The attempt context wraps the environment. **SPIKE**

**Recommendation:** `detail::AttemptEnvironment<Env, Rep, Phase>` holds a reference to the caller's environment plus the attempt number, the previous value and, when judging, this attempt's value. It forwards what every shipped node calls on an environment: `get<Q>()`, `provides<Q>`, `is_entered<Q>`, `source_of<Q>()`, and phase 12's `get_series<Q, N>()` and `is_entered_series<Q>`. Everything else still reads the specimen's data, unchanged.

**The spike compiles** the wrapper against every shipped node kind and phase 12's series nodes on cl, clang-cl, clang++ and g++. It also confirms that `Environment`'s own gated refusals (`RequireProvided`, the series/single cross refusals) still fire **once** through the wrapper and not twice.

**Phase 14 risk, flagged:** phase 14 adds a cross-sample / cross-test evaluation context. If it adds members to `Environment` that nodes call, the wrapper must forward them. Task 6 re-checks the forwarded set against `environment.hpp` at its branch point, and a test (task 6, step 6) fails if a public member of `Environment` is not forwarded.

**Ruling: accepted as recommended.**

**Spike result (step 6): confirmed on all five compilers measured.** The wrapper forwards the six members `provides`, `is_entered`, `is_entered_series`, `get`, `get_series` and `source_of`; `index_of` is private.
- One expression of every shipped node kind, phase 12's elementwise and constant series, `checked_evaluate` and `checked_evaluate_series` all give identical results through it.
- A forwarding control, whose `get()` answers absent, fails.
- A missing quantity gives exactly one message, `RequireProvided`'s.
- `previous_attempt` outside a retry gives exactly one message, its own.

### R6. No `abs` node: a two-sided agreement is written with `when`

"Two successive results agree within *t*" needs `|this − previous| ≤ t`. The library has no absolute value and no logical `and` over predicates. It can be written with what ships:

```cpp
when(this_attempt<R> >= previous_attempt<R>,
     this_attempt<R> - previous_attempt<R>,
     previous_attempt<R> - this_attempt<R>) <= constant<unit::Gram>(rat(3, 2))
```

**Recommendation:** do not add `abs` in this phase. The guide shows this spelling, and follow-up **G4** records `abs`. Adding a node kind means a `StepKindOf` entry, a render case in three dialects, a document walk, a `ConstantRewrite` and a vocabulary test. That is a task of its own, and not one §16.5 #11 asks for.

**Ruling: accepted.** No `abs` in this phase. It is follow-up G4.

### R7. Relation to phase 13, which is planned in parallel

Phase 13's outlier rejection "re-runs the aggregate, possibly more than once, and can terminate in abort" (§16.4 #6). That is also a bounded fixed point with a terminal state. If each phase builds its own loop, there are two trace shapes for "repeated until".

**Recommendation:** phase 15 does **not** depend on phase 13. There were two options:
- (a) phase 13 keeps its own rejection node, and phase 15's guide explains the difference: rejection changes *which data* enters an aggregate, while a retry recomputes *an expression*;
- (b) phase 13's rejection is expressed later as a retry.

(a) is recommended. Rejection's result carries `rejected_indices`, which a retry has no reason to know about, and forcing the two together would make both worse.

**Coordination items the lead should pass to phase 13's planner:**
- the `StepKind` enumerator names both phases add, so they do not collide (`RetryAttempt`, `RetryConcluded`, `OpaqueOperation`, `OpaqueOutput`, `AttemptNumber`, `PreviousAttempt`, `ThisAttempt`, `AttemptInput`);
- the namespace-scope names (`retry`, `opaque`, `opaque_output`, `linear_least_squares`, `attempt_number`, `previous_attempt`, `this_attempt`, `attempt_input`, `starting_from`), for GCC `-Wshadow`;
- `sizeof(Step<Rational>)`. Phase 12 took it from 776 to 896 bytes, and both phases will add more (task 1 re-measures after phase 13 merges).

**Ruling: (a).** Phase 13 keeps its own rejection node. Phase 15's guide explains the difference: rejection changes *which data* enters an aggregate, while a retry recomputes *an expression*. The lead has passed the coordination items above to the phase 13 planner.

---

## Dependencies

Phase 12's task numbers come from its plan. "Merged" means merged to the branch this phase starts from. **No task needs phase 13.** Two tasks carry a phase 13 *coordination* point, and none carries a code dependency.

| Phase 15 task | Needs from phase 12 | Needs from phase 13 | Needs from phase 14 | Earliest start |
|---|---|---|---|---|
| **1. Spike** | Nothing to *start*. The series probes use phase 12's merged headers if task 2 has merged, and stand-ins otherwise. The final verification of the interface check (step 1) needs **phase 12 merged**. | Nothing. It re-measures `sizeof(Step)` after 13 merges, if 13 is in. | Nothing | **Now.** Finish it after phase 12 merges. |
| **2. Opaque core** | **T2** (`SeriesNode`, `EvaluatedSeries`, `SeriesFailure`, `dispatch_series`, `MeasuredSeries`). **T4** only for the fixture that feeds an elementwise series into a call. | Nothing | Nothing | After phase 12 **T4** |
| **3. Opaque on every surface** | **T3** (`Step::elements`, `failedElement`, series marker `detail::series_marker<D>`, `ValueShape`, series hooks). | Coordination only: `Step` layout and `StepKind` names | Nothing | After phase 12 **T3**, and after phase 15 task 2 |
| **4. Least squares** | **T9** (`CurveExpression`, `CurveNode`, `EvaluatedCurve`, `domain<…>()`, the curve's evaluation seam) | Nothing (residuals and r² are follow-up G2, *after* phase 13) | Nothing | After phase 12 **T9**, and after phase 15 task 3 |
| **5. Methods and overlays** | **T5** (`ConstantRewrite` for series nodes, the series-quantity overlay refusal) | Nothing | Nothing | After phase 12 **T5**, and after phase 15 task 4 |
| **6. Retry core** | Nothing (scalar attempts). `AttemptEnvironment` forwards `get_series` once **T2** has merged. | Nothing (R7 coordination) | **Re-check** the forwarded `Environment` members if 14 has merged (R5) | **After phase 12 merges** (lead's ruling). Technically it could start on `master` now, but that would mean rebasing `trace.hpp` |
| **7. Retry on every surface** | **T3** (the `Step` layout and element rendering budget it extends) | Coordination only: `Step` layout | Nothing | After phase 12 **T3**, and after phase 15 task 6 |
| **8. Recorded attempts** | **T2** (`MeasuredSeries`, `get_series`, the length refusal's words) and **T3** (a series variable on the trace) | Nothing | Nothing | After phase 12 **T3**, and after phase 15 task 7 |
| **9. Guide, example, gallery** | **T11** (the series guide this one links to) | Link to phase 13's guide if merged, and the R7 paragraph | Nothing | After tasks 1–8 and phase 12 **T11** |

**Scheduling (lead's ruling):** phase 15 as a whole is scheduled after phase 12 merges.

**Lanes (lead's ruling): the default is serial, 1 through 9.** The lead decides at scheduling time whether to run in parallel instead, depending on how many lanes are free and whether phase 12's T9 has slipped. Both options are recorded here, and the plan does not choose between them:
- **Serial (the default):** 1, 2, 3, 4, 5, 6, 7, 8, 9.
- **Parallel:** an opaque lane (2→3→4→5) and a retry lane (6→7→8), in separate worktrees, after task 1. Both lanes edit `trace.hpp`, `trace_render.hpp`, `render.hpp` and `document.hpp` for different node kinds, so each merge costs the other lane a rebase. Task 9 follows both lanes.

**Critical path:** phase 12 T9 (curves) gates task 4. If phase 12 slips at T9, tasks 6–8 can proceed ahead of task 4. Their only phase 12 needs are T2 and T3.

---

## Global Constraints

- **C++23**, header-only, no dependency beyond the standard library in shipped headers (§3).
- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. Verify every task on all eight presets: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`, `clang-release`, `gcc-release`, `clang-ubsan`. Iterate on `cl-debug` + `gcc-release`, and hand in after the controller's `verify_all.ps1 -Root <tree>`.
- **Doxygen 1.9.8 (in WSL), zero warnings, and `mkdocs build --strict` (Windows Python) on every task**, not only before the merge.
- **No norm content** (§3):
  - No identifier, clause number, equation, threshold, constant or data set from any real standard, anywhere.
  - Cite invented `Example Standard` references only.
  - Generic textbook least squares is fine. No test or example may present a fit, tolerance or attempt count as a real method's.
  - The fixture numbers below are invented and deliberately irregular. Do not "tidy" them.
- **No abort, terminate or assert pop-ups.** Refusals are compile-time `static_assert`s in this library's words, or runtime results. `compute` must be `noexcept`, and that is checked. Nothing new may `throw` from a `noexcept` path. Examples link `support/fail_without_dialogs.cpp` (already done by `formula_add_example`).
- **Termination is structural.** A retry's attempt count is a template argument with a compile-time cap (R2). No loop in this phase has a runtime-determined bound, except the fit's loop over a series whose length is static (S1).
- **Never hardcode `/std:c++23`.** Never redirect a build to `/dev/null`. `FORMULA_WERROR=ON` everywhere.
- Every negative test asserts **both** that the build fails **and** that the output holds this library's own `static_assert` text. Where a second message could plausibly fire, it carries `// REJECT:`. Every negative test gets a **deletion check**: delete the guard, confirm the case compiles, then restore with a plain write or `touch`.
- **No `{}` default member initialiser on any member that holds an expression or a node** (defect class 4). That covers `OpaqueCall::inputs`, `OpaqueOutputNode::call`, `Retry::attempt`, `Retry::accept`, `StartingValue::expression` and every context node's operand, if any.
- **Names.** Under GCC `-Wshadow`, no parameter or local in namespace `formula` may be named `opaque`, `opaque_output`, `linear_least_squares`, `retry`, `attempt_number`, `previous_attempt`, `this_attempt`, `attempt_input` or `starting_from`. Under cl C4459, follow afcbf88's renaming convention, and avoid `result`, `value`, `index`, `text`, `step`, `first`, `mark`, `position`, `numerator`, `denominator`, `lhs`, `rhs`, `attempt` and `outputs` as locals. Check every new `StepKind` enumerator on GCC under `-Wshadow`.
- Every new header goes into the install `FILE_SET` (`hygiene.installed-headers` fails otherwise) and into `formula.hpp`. `trace.hpp` and `document.hpp` stay out of the umbrella.
- **Coherent SI and the method's rounding decide every fixture** (defect class 5). Every assertion names the plausible wrong implementation its value tells apart, *after* SI conversion and any rounding.
- **Constant-evaluation budget:** keep N × (nodes evaluated per element) under about 1,000 per `STATIC_REQUIRE`, which is phase 12's measured rule. A retry counts `MaxAttempts × (attempt nodes + accept nodes)`. Use runtime `REQUIRE` above that.
- A quoted compiler diagnostic in `docs/` must come from a real compile (`hygiene.documented-diagnostics`). Every ```` ```text ```` block in the guide must be whole consecutive lines of the example's real output.
- `hygiene.vocabulary-reach` scans `render.hpp`, `document.hpp`, `trace.hpp` and `trace_render.hpp`. Every new symbol spelling goes through `symbol_of<Q>(vocabulary)`.
- **Catch2 splits test filters on commas.** Prove a filter's selection count on the unmutated build before trusting a mutation run.
- **Baseline:** task 1 counted 622 `TEST_CASE`s and 187 negative files at phase 12's `525441c`, which is not the branch point. Task 2 records the `ctest` count at the real branch point, after phase 12 merges. A task reporting fewer tests than that has lost tests.
- **Defect class 9:** no identifier from a real standard, and fixture numbers must be plainly invented. The fixtures below are irregular on purpose.
- **Explicitly specialising library internals is outside the contract** (lead's ruling). A consumer extends the library only through `OpaqueOperation` (O1), never by specialising `ConstantRewrite`, `StepKindOf` or any other `detail::` template. The guide says so.
- **Trace data for the new kinds lives in `Trace` side tables keyed by step index, with +0 bytes on `Step`** (O6's ruling since).

## Review Focus

Five input classes the spec implies and no feature test naturally exercises, most likely first. Each has its pinning test in the owning task.

1. **A degenerate fit.** Every domain value is equal, there is one point, or there are two points. Expected: the first two return the fit's *own* `DomainError`, which the trace calls the operation's own failure, and never a slope of zero or infinity. Two points give the exact line through them. *Pinned in task 4, step 1.* **Amended (C4):** a curve refuses equal domain values itself, so through a curve that case is the curve's own `DuplicatePoint` failure, relayed and recorded `Propagated`; the fit's own `DomainError` is pinned by a one-point curve and by equal values handed to `compute` directly.
2. **An absent element in a series given to an opaque operation.** Expected: the whole call is absent and `compute` is never called. The test's operation records whether it was called, so "absent because `compute` coped" cannot pass. *Pinned in task 2, step 1.*
3. **A condition met on exactly the last permitted attempt, and a tolerance met with equality.** Expected: `Accepted` at attempt `MaxAttempts`, not `Exhausted`. `≤` accepts at equality, where `<` would take one attempt more. *Pinned in task 6, step 1.*
4. **An attempt that fails in the middle** (division by zero at attempt 2 of 4). Expected: `Failed` with `attempt == 1` (zero-based; the text says `attempt 2`), attempts 3 and 4 never evaluated (the trace has no step for them), and no value. *Pinned in tasks 6 and 7.*
5. **A retry whose trace is longer than the render budget.** Expected: `render_trace` stays within the one `maxSteps` budget and its footer counts what it cut. The `Trace` object itself still holds every attempt. *Pinned in task 7, step 1.*

---

## Interfaces that already exist: read, not guessed

| Fact | Where |
|---|---|
| `NodeBase`, `concept Node`, every scalar node holding its children with no `{}` | `expression.hpp:35-40`, `:159-189` |
| `Evaluated<Rep> = std::expected<std::optional<Rep>, ArithmeticError>`, `checked_evaluate<Result>`, the `entered` override short-circuit | `evaluate.hpp:150`, `:340` |
| `detail::dispatch` and the closed-registry limitation for consumer nodes | `sink.hpp:143` and its comment |
| The optional-hook-pair pattern (`variant_entered`/`variant_produced`, both or neither) | `sink.hpp` (`VariantSelection` comment) |
| `NumericValueNode`, `detail::saysSomething`, the mandatory-justification precedent | `escape.hpp:52`, `:80` |
| `RepTraits<Rep>` (`add`, `subtract`, `multiply`, `divide`, `from`) | `evaluate.hpp:60-140` |
| `Outcome<Q>`, `Verdict`, `ValueSource`, `OutcomeKind` (the `verdict` arm, produced by no evaluation today) | `outcome.hpp` |
| `Citation`, `DocumentedNode` | `citation.hpp` |
| `PredicateNode`, `concept Predicate`, `Comparison` | `predicate.hpp:34,71,107` |
| `WhenNode`, `when(...)` | `conditional.hpp:74,98` |
| `Environment`: `provides`, `is_entered`, `get`, `source_of`, `RequireProvided` | `environment.hpp:136-234` |
| `StepKind`, `Step<Rep>`, `LookupFailure` (the own/relayed precedent), `RecordingSink`, `marks`/`unclaimed` | `trace.hpp:40,213,318,715,1184` |
| `render_trace`, `StepLimit`, `step_line`, `citation_suffix`, `justification_suffix` | `trace_render.hpp:54,761,916` |
| `render_node(NumericValueNode …)`, the `numeric(…, in …)` spelling | `render.hpp:850` |
| `detail::literal_words_in_dialect<D>`: **not** used for opaque names, because its LaTeX output shows literal backslashes on MathJax (spike step 9) | `render.hpp:356` |
| `Documentation`, `replacedBy` (the "an uncited entry still appears" precedent), `detail::Walk`, `collect` | `document.hpp:96-120,165,452` |
| `ConstantRewrite`, `RequireOverlaySeesNode`, `SubstitutedIn` | `overlay.hpp:1011,1164,1652` |

**From phase 12, as planned.** Task 1 step 1 verifies every row against what merged:

| Name | Phase 12 task |
|---|---|
| `SeriesNodeBase`, `concept SeriesNode` (**in `expression.hpp:313,319`**), `S::length`, `S::dimension` | T2, verified at `525441c` |
| `MeasuredSeries<Q, N>` (`environment.hpp:84`), `measured_series<Q>(…)`, `Environment::get_series<Q, N>()`, `is_entered_series<Q>` | T2, verified |
| `SeriesValue<Rep, N>` (default-initialise it, never `{}`: cl C4459, see its comment), `SeriesFailure { error, std::optional<std::size_t> element }`, `EvaluatedSeries<Rep, N>`, `detail::dispatch_series<Rep>` | T2, verified |
| `series<Q, N>`, `SeriesVarNode` | T2, verified |
| `Step::elements`, `Step::failedElement`, `detail::series_marker<D>` (**`render.hpp:242`**), `ValueShape`, `series_entered`/`series_produced` via `detail::HearsSeries` | T3, verified |
| Elementwise operators, `series_constant` | T4, verified at `525441c` |
| `sum`, `ConstantRewrite` for series nodes | T5, merged; see C2 |
| `CurveExpression`, `CurveNode { domainSeries, valueSeries }`, `curve(D, V)`, `CurveValue`, `EvaluatedCurve`, `domain<U, Points>` | T9, merged; see C2 (`domain` is a variable template) and C3/C4 |

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/opaque.hpp` *(new)* | `InputShape`, `concept OpaqueOperation`, the operation checks, `OpaqueCall`, `opaque<Op>()`, `OpaqueOutputNode`, `opaque_output<"…">()`, evaluation |
| `include/formula-cpp/least_squares.hpp` *(new)* | `LinearLeastSquares`, `linear_least_squares()` |
| `include/formula-cpp/retry.hpp` *(new)* | `FirstJudged`, `RetryEnd`, `StartingValue`/`starting_from`, the context nodes, `detail::AttemptEnvironment`, `Retry`, `retry<…>()`, `RetryOutcome`, `RetryFailure`, `checked_evaluate_retry`, and in task 8 `AttemptInputNode`/`attempt_input` |
| `include/formula-cpp/sink.hpp` | documents the optional `opaque_*` and `retry_*`/`attempt_*` hooks |
| `include/formula-cpp/trace.hpp`, `trace_render.hpp` | new `StepKind`s, `OpaqueFailure`, the side tables (`Step` itself unchanged), the hooks, rendering within budget |
| `include/formula-cpp/render.hpp`, `document.hpp` | render and walk opaque outputs and retries; `Documentation::opaqueOperations` |
| `include/formula-cpp/overlay.hpp` | `ConstantRewrite` and `SubstitutedIn` for `OpaqueOutputNode` |
| `include/formula-cpp/formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt` | umbrella, `FILE_SET`, test registration |
| `test/opaque_tests.cpp`, `least_squares_tests.cpp`, `retry_tests.cpp`, `opaque_cross_tu.hpp`, `opaque_cross_tu_b.cpp` *(new)* | per-surface tests |
| `test/trace_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp`, `overlay_tests.cpp`, `method_tests.cpp` | extended per task |
| `examples/opaque_and_retry.cpp`, `docs/opaque-and-retry.md` *(new)* | the example, and the guide "Opaque operations and bounded retry" |

`opaque.hpp` and `least_squares.hpp` are split so that a reviewer can accept the mechanism and reject the fit, or the reverse. `retry.hpp` shares nothing with either of them except the trace.

## The shared fixtures

Every number here is invented. Each has been computed by hand, and each is chosen so that the plausible wrong implementations named beside it give different answers.

**Opaque core (task 2): `SeriesSpan`, a consumer operation defined in the test.** Its outputs are `lowest`, `highest` and `span`, over one `Series` input of any dimension. The input is `series<Reading, 4>` (`Reading`, `"r"`, grams) = **127, 103, 191, 139 g** (C13). In coherent SI that is 127/1000, 103/1000, 191/1000 and 139/1000 kg.
- Correct: lowest 103 g, highest 191 g, span 88 g (11/125 kg).
- First/last instead of min/max: 127 and 139, span 12.
- Reversed input: the same min/max, so a test with a *different* order is added for the `Series` span-passing check.
- Grams passed as if coherent: 7000 times too large after conversion back.

**Least squares (task 4).** `Elapsed` (`"t"`, seconds) and `Length` (`"L"`, millimetres), 4 points:

| t (s) | 1 | 2 | 4 | 7 |
|---|---|---|---|---|
| L (mm) | 10.2 | 10.9 | 12.1 | 14.3 |

n = 4, Σt = 14, ΣL = 47.5, Σt² = 70, ΣtL = 180.5, S_tt = 21 and S_tL = 14.25.
- **slope = 19/28 mm/s** (0.678571…). In SI that is 19/28000 m/s.
- **intercept = 9.5 mm**, which is 19/2000 m.

Wrong implementations, all distinct from 19/28:
- secant from first to last point: 41/60 ≈ 0.6833;
- x-on-y regression, inverted: 9.6875/14.25 ≈ 0.6798;
- through the origin: 180.5/70 ≈ 2.5786;
- inputs swapped: 14.25/9.6875 ≈ 1.4710.

**Rounding hazard (class 5):** at 2 dp in mm/s, the right answer, the secant and x-on-y *all* give 0.68. Any test that rounds the slope rounds to **3 dp** (0.679 against 0.683 and 0.680), or asserts the exact rational.

**Retry fixpoint (task 6).** `Estimate` (`"w"`, grams). The attempt is `w_k = 6.08 g + w_{k−1} / 2` (C13), starting from 0 g, and accepted when `w_k − w_{k−1} ≤ t` (the sequence rises, so the sign is known; the code writes it as `w_{k−1} − w_k ≥ −t`):

| k | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| w_k (g) | 6.08 | 9.12 | 10.64 | 11.40 | 11.78 |
| w_k − w_{k−1} (g) | 6.08 | 3.04 | 1.52 | 0.76 | 0.38 |

- With t = 0.76 g (19/25 g), `MaxAttempts = 4`, and accepting when `w_k − w_{k−1} ≤ t`: **Accepted at 4, 11.4 g** (57/5 g), on the last permitted attempt and at equality.
- With `<` instead of `≤` and `MaxAttempts = 5`: 11.78 g (589/50 g) at 5.
- With `MaxAttempts = 3`: **Exhausted**, with the verdict `"repeat the determination"`.
- Off by one in the loop bound (running `MaxAttempts − 1`): exhausted at 3 in the first case, so the first case fails.

**Recorded attempts (task 8).** `Determination` (`"d"`, grams), `series<Determination, 4>` = **41.3, 43.9, 42.7, 45.7 g** (C13). Attempt *k*'s result is determination *k*. It is accepted `FirstJudged::AtSecondAttempt` when two successive results agree within 1.27 g, which R6's `when` spelling expresses:
- |43.9 − 41.3| = 2.6, so not accepted at 2;
- |42.7 − 43.9| = 1.2, so **Accepted at 3, 42.7 g**.

Wrong implementations:
- never stopping: 45.7;
- reporting the earlier of the agreeing pair: 43.9;
- judging at attempt 1 against an absent previous: `Failed` at 1.

The variant with element 3 absent gives **`NotRecorded` at 3**, empty, and element 4 is never read.

---

## Task 1: Spike: measure what O3, O6, O9, R2 and R5 depend on, and check phase 12's names

**Status: done, 2026-09-26.** Findings are in `.superpowers/sdd/2026-09-26-opaque-operations/task-1-spike.md`. The decisions above carry each result. One item is open: O1 amendment 3, the refused-dimension second message.

**Files:**
- Create: `.superpowers/sdd/phase-15-prep/spike/*` (probes; untracked), `.superpowers/sdd/phase-15-prep/task-1-spike.md` (findings; untracked)

**Interfaces:**
- Consumes: phase 12's merged headers where available, and stand-ins otherwise. Each stand-in is labelled as one in the findings.
- Produces: `task-1-spike.md`, one section per probe. Each section gives the exact command, the compiler and version, the observed output, a **failing control**, and a verdict. A final section lists "what I did not establish". The lead amends the design decisions from it before task 2 starts.

**Prevents:** class 1 (every later claim about compilers cites this file, naming the compilers it names and no others), class 8 (the instruments are the real ones: MathJax 3.2.2 with the site configuration, tectonic with `[OT1]{fontenc}`, Doxygen 1.9.8 in WSL, `wsl bash <file>` with a control that prints 42).

- [ ] **Step 1: Check phase 12's interface table against what merged.** For each row of "From phase 12, as planned", grep the merged header and record the actual spelling and file:line. Report every difference as a proposed amendment to this plan. Do not silently adapt a later task.
- [ ] **Step 2: Record the baseline test count** on the branch point, on `cl-debug` and `gcc-release`.
- [ ] **Step 3: O9, fit overflow.** Write a stand-in least-squares loop over `std::array<Rational, N>` that uses `RepTraits<Rational>`'s checked operations. Record the first failing N, or "no failure up to 64", for each data shape (integers; one decimal place; thirds mixed with sevenths), at N = 5, 20 and 64, on cl, clang-cl, clang++ and g++. The control is a data set built to overflow at N = 5, which must return `Overflow`.
- [ ] **Step 4: O3, constant-evaluation budget.** Evaluate a two-output formula (intercept + slope × t) over the stand-in at N = 5, 20 and 64 inside one `static_assert`. The control is the same assertion with a wrong expected value, which must be refused. Report the largest passing N per compiler.
- [ ] **Step 5: R2, the retry cap.** Evaluate a stand-in retry of a five-node attempt expression at 16, 32 and 64 attempts inside one `static_assert` on each compiler. Report the largest passing count. If 64 fails anywhere, propose the cap as the largest count that passes on every compiler measured.
- [ ] **Step 6: R5, environment forwarding.** Wrap `formula::Environment` in a stand-in `AttemptEnvironment` that forwards `get`, `provides`, `is_entered`, `source_of` (and `get_series`/`is_entered_series` if phase 12 T2 has merged). Evaluate one expression of every shipped node kind through it. Then evaluate a `var<Q>` that the environment does not provide, and count the messages on each compiler. It must be **one**, the shipped `RequireProvided` text. Count by hand and record the count.
- [ ] **Step 7: O1, the `noexcept` check.** Show that `static_assert(noexcept(Op::template compute<Rational>(…)))` refuses a `compute` declared without `noexcept` and accepts one declared with it, on all four compilers. Then compile a `consteval output_dimensions` that returns `std::nullopt`, and confirm that the library-shaped refusal (`RequireOpaqueAcceptsDimensions`) fires once.
- [ ] **Step 8: O2, output-name lookup.** Compile `opaque_output<"slope">` against a stand-in operation whose `outputs` hold `"intercept"` and `"slope"`: it must resolve to index 1. Compile it with `"slop"` and confirm exactly one library message on each compiler.
- [ ] **Step 9: O6 and R1, spellings.** Typeset each candidate under MathJax 3.2.2 (site configuration, and again with `noundefined` removed) and under tectonic with `\usepackage[OT1]{fontenc}`, each inline and displayed. The controls are a clean formula and `\frac{1}{`. The candidates are:
  - `\operatorname{linear\ least\ squares}\left({t}_{i}, {L}_{i}\right)_{\text{slope}}`;
  - the same with an operation name holding `_`, `%`, `&` and `#`, escaped by `detail/name_text.hpp`'s current function (report which characters it does and does not escape);
  - `{w}_{k}`, `{w}_{k-1}`, `{w}_{0}` and `{d}_{k}` (R1's attempt markers, following phase 12's S14 `{x_m}_{i}`).

  Run each Markdown candidate through python-markdown with `mkdocs.yml`'s extensions. The control is `` `m_r`[i](x) ``, which must become a link.
- [ ] **Step 10: `sizeof(Step<Rational>)`.** Add O6's and task 7's fields to a copy of the merged `Step`, and measure the size on cl, clang++ (MSVC STL) and g++ (libstdc++). If phase 13 has merged, measure after it. Report the growth, and note the side-record alternative without proposing it.
- [ ] **Step 11: Write `task-1-spike.md`** and message the lead. Nothing else in this plan starts until the lead has amended the decisions.

---

## Task 2: Opaque operations: declaration, call, output, evaluation

**Files:**
- Create: `include/formula-cpp/opaque.hpp`, `test/opaque_tests.cpp`, `test/opaque_cross_tu.hpp`, `test/opaque_cross_tu_b.cpp`
- Create (negatives): `test/negative/opaque_blank_name.cpp`, `opaque_unreadable_name.cpp`, `opaque_duplicate_output.cpp`, `opaque_unknown_output.cpp`, `opaque_throwing_compute.cpp`, `opaque_wrong_arity.cpp`, `opaque_wrong_shape.cpp`, `opaque_dimensions_refused.cpp`, `opaque_series_lengths_differ.cpp`, `opaque_output_as_series.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `CMakeLists.txt` (`FILE_SET`), `test/CMakeLists.txt`

**Interfaces:**
- Consumes: phase 12 T2's `SeriesNode`, `EvaluatedSeries`, `SeriesFailure` and `detail::dispatch_series<Rep>`, and T4's elementwise operators (fixture only).
- Produces:
  - `enum class InputShape : std::uint8_t { Single, Series, Curve };` (`Curve` is declared here and accepted from task 4, where the curve seam exists. Until then a `Curve` shape is refused by `RequireCurveInputsAvailable`. Task 4 deletes that refusal and its negative.)
  - `template <typename Op> concept OpaqueOperation`, requiring `name`, `shapes`, `outputs`, `output_dimensions` as specified in O1. `compute` is checked separately, so that its message names it.
  - `template <OpaqueOperation Op, typename... Inputs> struct OpaqueCall { std::tuple<Inputs...> inputs; Citation citation; static constexpr std::array<Dimension, Op::outputs.size()> output_dimensions; };`, with no `{}` on `inputs`.
  - `template <OpaqueOperation Op, typename... Inputs> [[nodiscard]] constexpr auto opaque(Citation citation, Inputs... inputs) noexcept;`. `Citation` comes first and is not deduced, so that a designated initialiser works. That order was verified for `documented` on cl 19.51, clang-cl 22 and g++ 13.3 (`citation.hpp`), and it is re-verified here on all four.
  - `template <std::size_t I, typename Call> struct OpaqueOutputNode: NodeBase { Call call; static constexpr Dimension dimension = Call::output_dimensions[I]; static constexpr std::string_view output = Call::operation::outputs[I]; };`
  - `template <detail::FixedString Name, typename Call> [[nodiscard]] constexpr auto opaque_output(Call call) noexcept;`
  - `detail::evaluate_call<Rep>(Call const&, Env const&, Sink) -> OpaqueEvaluated<Rep, M>`, where `OpaqueEvaluated<Rep, M> = std::expected<std::optional<std::array<Rep, M>>, OpaqueCallFailure>` and `struct OpaqueCallFailure { ArithmeticError error; OpaqueFailure origin; std::optional<std::size_t> element; };`
  - `checked_evaluate_si<Rep>(OpaqueOutputNode<I, Call> const&, Env const&, Sink = {})`, returning `Evaluated<Rep>`
  - `enum class OpaqueFailure : std::uint8_t { None, Own, Propagated, Undetermined };`, declared here so that the evaluator can say whose failure it is. Task 3 records it.
  - `detail::RequireOpaqueNameReadable<…>`: a name or output name holding anything but ASCII letters, digits and single spaces is refused at compile time (O1, amendment 2).
  - **Where the checks sit (spike step 7):** the `noexcept` check and the dimension check are made in `opaque<Op>()`, the factory. `checked_evaluate_si(OpaqueOutputNode …)` gates its whole body with `if constexpr`, because without the gate g++-14 adds seven errors to the one message.
  - **Blocked on the lead's ruling for O1 amendment 3:** how a refused dimension avoids a second message downstream. Until then `opaque_dimensions_refused` builds the output and does not evaluate it, which gives one message on all five compilers measured.

**Prevents:**
- **Class 2:** every refusal is a `RequireOpaque…` struct, and the call's `output_dimensions` is `conditional_t`-gated behind arity, shape and dimension acceptance. So one bad call draws one message. Each negative carries a REJECT for the next refusal down. `opaque_wrong_arity` REJECTs `"does not accept inputs of these dimensions"`, because arity is the mistake and dimensions were never compared.
- **Class 3:** the name and output names are the operation type's statics. No member of `OpaqueCall` or `OpaqueOutputNode` names the operation.
- **Class 4:** `inputs` and `call` have no `{}`. A cross-TU test and a `std::tuple` default-constructibility probe (the shape that broke clang, g++-14 and libc++ in phase 11) compile on all eight presets.
- **Class 5:** the fixture's four elements are all different and out of order (12, 7, 19, 9), so min/max against first/last and a unit error are each told apart.

- [ ] **Step 1: Write the failing tests.**

```cpp
namespace
{
struct Reading: formula::Quantity<Reading, "r", "an invented reading", unit::Gram> {};
struct Lowest: formula::Quantity<Lowest, "r_lo", "the lowest reading", unit::Gram> {};
struct Spread: formula::Quantity<Spread, "r_sp", "the spread of the readings", unit::Gram> {};

// A consumer's operation. Declared here, in the test, on purpose: the point
// is that a consumer can write one and it is traced without any library change.
struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 3> outputs { "lowest", "highest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 3>>
        output_dimensions(std::array<formula::Dimension, 1> in) noexcept
    {
        return std::array { in[0], in[0], in[0] };
    }

    // Counts calls, so a test can tell "absent because compute was never
    // called" from "absent because compute coped" (Review Focus 2).
    static inline int calls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 3>, formula::ArithmeticError>
        compute(std::span<Rep const> readings) noexcept
    {
        if (!std::is_constant_evaluated())
            ++calls;
        Rep lo = readings[0];
        Rep hi = readings[0];
        for (Rep const& reading: readings)
        {
            if (reading < lo) lo = reading;
            if (hi < reading) hi = reading;
        }
        auto const spread = formula::RepTraits<Rep>::subtract(hi, lo);
        if (!spread) return std::unexpected { spread.error() };
        return std::array { lo, hi, *spread };
    }
};

constexpr auto readings = formula::environment(formula::measured_series<Reading>(
    formula::Measured<Reading> { rat(127) }, formula::Measured<Reading> { rat(103) },
    formula::Measured<Reading> { rat(191) }, formula::Measured<Reading> { rat(139) }));

constexpr auto span_call = formula::opaque<SeriesSpan>(
    { .title = "Spread of readings", .reference = "Example Standard 12", .section = "4.2" },
    formula::series<Reading, 4>);
} // namespace

TEST_CASE("an opaque operation's outputs are nodes, each in its declared dimension", "[opaque]")
{
    // min/max, not first/last: first/last would give 127 and 139. Coherent SI in,
    // declared unit out: a unit slip would be off by a factor of 1000.
    constexpr auto lowest = formula::checked_evaluate<Lowest>(formula::opaque_output<"lowest">(span_call), readings);
    STATIC_REQUIRE(lowest->measurement().value() == rat(103));
    constexpr auto spread = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), readings);
    STATIC_REQUIRE(spread->measurement().value() == rat(88));
}

TEST_CASE("an output enters ordinary arithmetic", "[opaque]")
{
    // span / 2 + lowest = 147 g: the output is a Node like any other.
    constexpr auto mid = formula::opaque_output<"span">(span_call) / formula::rat(2)
                         + formula::opaque_output<"lowest">(span_call);
    STATIC_REQUIRE(formula::checked_evaluate<Lowest>(mid, readings)->measurement().value() == rat(147));
}

TEST_CASE("one absent element makes the whole call absent, and compute is never called", "[opaque]")
{
    SeriesSpan::calls = 0;
    auto const gap = formula::environment(formula::measured_series<Reading>(
        formula::Measured<Reading> { rat(127) }, formula::Measured<Reading>::absent(),
        formula::Measured<Reading> { rat(191) }, formula::Measured<Reading> { rat(139) }));
    auto const outcome = formula::checked_evaluate<Spread>(formula::opaque_output<"span">(span_call), gap);
    REQUIRE(outcome.has_value());
    CHECK(outcome->is_empty());
    CHECK(SeriesSpan::calls == 0);   // not "compute skipped the gap and said 191 - 127"
}

TEST_CASE("a failing series input fails the call, and the error is the input's, not the operation's", "[opaque]")
{
    // series<Reading,4> / var<Divisor> with the divisor zero: every element fails
    // at element 0. The output relays DivisionByZero; task 3 checks the trace
    // records Propagated with element 0.
}

TEST_CASE("compute's own failure is relayed as its error", "[opaque]")
{
    // A second test-local operation whose compute returns DomainError for a
    // series whose elements are all equal; the output is std::unexpected(DomainError).
}
```

- [ ] **Step 2: Run it and confirm it fails to build** on `cl-debug`. Never redirect the output.
- [ ] **Step 3: Implement `opaque.hpp`** with the interfaces above.
  - `evaluate_call` dispatches each input in order: `detail::dispatch<Rep>` for a `Node`, `detail::dispatch_series<Rep>` for a `SeriesNode`. It stops at the first failure and marks it `Propagated`. An input that was never evaluated is not entered, which is the contract `sink.hpp` already documents for a failing sibling.
  - Absence is decided after every input has been asked, following `BinaryNode`'s rule (`evaluate.hpp:302`), so that an error in a present input is not hidden behind another input's absence.
  - It copies present series values into `std::array<Rep, N>` and passes `std::span<Rep const>`.
  - It calls `Op::template compute<Rep>` and converts nothing. Values stay in coherent SI.
  - The file comment says, in these words: *"compute receives evaluated values only, never the environment; that is what lets the trace show every input and an overlay reach every use of a quantity."*
- [ ] **Step 4: Run the tests and confirm they pass.** First confirm that the filter `[opaque]` selects the expected count on the unmutated build.
- [ ] **Step 5: Write the ten negatives,** each with `// EXPECT:` and, where a second message could fire, `// REJECT:`. The messages:
  - `"formula: an opaque operation's name must say something; one that is empty or blank leaves the trace unable to say what ran"`
  - `"formula: an opaque operation's name and output names may hold only ASCII letters, digits and single spaces, so that every dialect shows them as written"` (`opaque_unreadable_name`, with an `_` in an output name)
  - `"formula: an opaque operation declares two outputs with the same name; opaque_output could not tell them apart"`
  - `"formula: this opaque operation has no output of that name; its declared outputs appear in this diagnostic as the template argument of RequireOpaqueOutputNamed"`
  - `"formula: an opaque operation's compute must be noexcept; evaluation cannot throw, and an exception escaping it would end the program"`
  - `"formula: this opaque call passes a different number of inputs than the operation declares"`
  - `"formula: this opaque call passes a series where the operation declares a single value, or the reverse"`
  - `"formula: this opaque operation does not accept inputs of these dimensions"`
  - `"formula: this opaque operation's series inputs differ in length"`
  - `"formula: this expression is a series, not a single value; ..."`, phase 12's shipped message. `opaque_output_as_series` passes a whole call to `checked_evaluate` and must draw **only** the message that says an opaque call is not a value: `"formula: an opaque call has several outputs; choose one with opaque_output<\"name\">(call)"`. It REJECTs the series message.
- [ ] **Step 6: Apply the deletion check to every negative.** Delete the guard, confirm the case compiles or its REJECT fires, then restore with a plain write. Report each negative against the guard it pins.
- [ ] **Step 7: Check across translation units.** Declare `span_call` in `opaque_cross_tu.hpp`, use it from two TUs, and link.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.**

```bash
git add include/formula-cpp/opaque.hpp include/formula-cpp/formula.hpp CMakeLists.txt test/CMakeLists.txt \
        test/opaque_tests.cpp test/opaque_cross_tu.hpp test/opaque_cross_tu_b.cpp test/negative/opaque_*.cpp
git commit -m "feat(opaque): declare a named operation over evaluated inputs and use its outputs as nodes"
```

---

## Task 3: An opaque operation on every surface: trace, render, document, vocabulary

**Files:**
- Modify: `include/formula-cpp/sink.hpp`, `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`
- Modify: `test/trace_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp`, `opaque_tests.cpp`

**Interfaces:**
- Consumes: task 2's `OpaqueCall`, `OpaqueOutputNode`, `evaluate_call` and `OpaqueFailure`, and phase 12 T3's series-on-trace fields and `detail::series_marker<D>`.
- Produces:
  - `StepKind::OpaqueOperation` and `StepKind::OpaqueOutput`, each checked on GCC under `-Wshadow` against `opaque`, `opaque_output`, `OpaqueCall` and `OpaqueOutputNode`.
  - `template <typename Rep> struct OpaqueOutputValue { std::string_view name; Dimension dimension; std::optional<Rep> value; };`
  - **Side tables on `Trace`, and nothing new on `Step`** (O6's ruling since): `Trace::opaqueSteps` (`std::vector<OpaqueStepData<Rep>>`, each row `{ std::size_t step; std::string_view operationName; std::vector<OpaqueOutputValue<Rep>> outputs; OpaqueFailure failure; }`) and `Trace::opaqueOutputSteps` (rows `{ std::size_t step; std::size_t outputIndex; }`). Each has a lookup by step index, `opaque_data(trace, step)`, returning a pointer or null. Spike step 10 measured the rows at 56 and 16 bytes and `Step` unchanged at 816.
  - A test `STATIC_REQUIRE`s `sizeof(Step<Rational>)` against its value at the branch point, so that a field added to `Step` by mistake fails.
  - Optional sink hooks `opaque_entered(OpaqueCallInfo const&)` and `opaque_produced(OpaqueCallInfo const&, OpaqueEvaluated<Rep, M> const&)`, asked for together in one `requires`, as `variant_entered`/`variant_produced` are. `struct OpaqueCallInfo { std::string_view name; Citation citation; std::span<std::string_view const> outputs; std::span<Dimension const> dimensions; };` is plain data, so a sink needs no operation type.
  - `render_node<D>(OpaqueOutputNode const&, V const&)`, with spike step 9's spellings: LaTeX `\text{linear least squares}({t}_{i}, {L}_{i})_{\text{slope}}`, Plain `linear least squares(t(i), L(i)).slope`, Markdown ``linear least squares(`t(i)`, `L(i)`).slope``.
  - `detail::collect` for `OpaqueOutputNode`, which walks the call's inputs and appends to `Documentation::opaqueOperations`.
  - `struct OpaqueOperationEntry { std::string_view name; Citation citation; std::vector<std::string_view> outputs; };` and `Documentation::opaqueOperations` (one entry per distinct call site met, in order). A cited call's citation also joins `citations`.

**Prevents:**
- **Class 3:** `[inside not shown]` is emitted from `step_line` on `kind == StepKind::OpaqueOperation` alone. Every author text on the line, the citation above all, goes through `step_line`'s escaping (the lead's ruling). The test that pins it hand-builds a `Step` with every other field emptied and still expects the suffix. `operationName` is written only by `RecordingSink`.
- **Class 1:** every trace string in this task is pinned exactly by a test. No comment claims how a dialect typesets without citing task 1.
- **Class 2:** a failing input draws one failed input step and one `Propagated` operation step. The test counts the error lines by hand, because REJECT cannot refuse a second copy of the same text.

- [ ] **Step 1: Write the failing tests.** Each uses the crossed-over vocabulary pattern (`vocabulary_tests.cpp:61`).

```cpp
TEST_CASE("an opaque step names the operation, its citation, every output, and that its inside is not shown",
          "[opaque][trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink sink { trace, formula::vocabulary(formula::renames<Reading>("R")) };
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"span">(span_call), readings, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 20 });
    // Pinned exactly by the implementer from the real output; these fragments
    // are what the line must contain.
    CHECK(text.find("series span(#1)") != std::string::npos);
    CHECK(text.find("lowest = 103/1000 kg, highest = 191/1000 kg, span = 11/125 kg") != std::string::npos);
    CHECK(text.find("[inside not shown]") != std::string::npos);
    CHECK(text.find("Example Standard 12") != std::string::npos);
    CHECK(text.find("span of #2") != std::string::npos);        // the OpaqueOutput step
    CHECK(text.find("R = ") != std::string::npos);              // input in the sink's vocabulary
}

TEST_CASE("the inside-not-shown marker depends on the kind alone", "[opaque][trace]")
{
    formula::Step<> bare {};
    bare.kind = formula::StepKind::OpaqueOperation;   // no name, no outputs, no citation
    formula::Trace<> trace {};
    trace.steps.push_back(bare);
    CHECK(formula::render_trace(trace, { .maxSteps = 5 }).find("[inside not shown]") != std::string::npos);
}

TEST_CASE("an uncited opaque call says so, in the trace and on the page", "[opaque][trace][document]")
{
    // opaque<SeriesSpan>({}, ...) renders "(no citation given)" in render_trace
    // and gets an OpaqueOperationEntry with an empty Citation in document().
}

TEST_CASE("an opaque step tells its own failure from a relayed one", "[opaque][trace]")
{
    // compute's DomainError -> the opaque side-table row's failure == Own; a zero divisor in the input
    // series -> Propagated, with failedElement == 0. Count "division by zero"
    // occurrences in render_trace by hand: exactly one input step and one relay.
}

TEST_CASE("an opaque step's outputs share the render budget and say how many were cut", "[opaque][trace]")
{
    // maxSteps small enough to cut after "lowest": the line ends "... 2 more".
}

TEST_CASE("an opaque output renders as a call to the named operation in every dialect", "[opaque][render]")
{
    // Plain, Markdown and LaTeX, pinned exactly to task 1's chosen spellings,
    // with the input's series marker from phase 12 (S14) and the vocabulary's
    // symbol. The Markdown string passes the render_tests.cpp:1313 guard.
}
```

- [ ] **Step 2: Run the tests and confirm they fail to build.**
- [ ] **Step 3: Implement.**
  - `RecordingSink` records the call step from `opaque_produced` and claims every step since `opaque_entered` as its operands. That covers the input steps, and it is the `variant_produced` precedent.
  - The output step comes through `StepKindOf<OpaqueOutputNode<…>>` and claims the call step.
  - `record` sets the side-table row's `failure` from `OpaqueCallFailure::origin`. It never re-derives it.
  - A consumer input node that contributed no step makes a relayed failure `Undetermined`, as `LookupFailure` does.
- [ ] **Step 4: Run the tests and confirm they pass,** first confirming the filter's selection count.
- [ ] **Step 5: Mutations.** Each mutation must fail at least one test in this task, and the report names which:
  - delete the `[inside not shown]` emission;
  - drop one output from the line;
  - swap `Own` and `Propagated`;
  - print nothing for an empty citation;
  - use the default vocabulary in the input step.

  Restore each with a plain write.
- [ ] **Step 6: Verify on all eight presets, plus Doxygen 1.9.8 and mkdocs.** Confirm that `hygiene.vocabulary-reach` still passes.
- [ ] **Step 7: Commit.** Message: `feat(opaque): trace, render and document an opaque operation, saying its inside is not shown`.

---

## Task 4: Ordinary least squares over a curve

**Files:**
- Create: `include/formula-cpp/least_squares.hpp`, `test/least_squares_tests.cpp`
- Create (negatives): `least_squares_not_a_curve.cpp`
- Delete: task 2's `RequireCurveInputsAvailable` and its negative
- Modify: `opaque.hpp` (the `Curve` input shape), `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: phase 12 T9's `CurveExpression`, `curve(D, V)`, `EvaluatedCurve` and the curve evaluation seam, as task 1 step 1 recorded their merged spelling.
- Produces:
  - In `opaque.hpp`, `InputShape::Curve`: `evaluate_call` evaluates a curve through phase 12's seam and passes two `std::span<Rep const>`, domain first, contributing two dimensions to `output_dimensions`.
  - `struct LinearLeastSquares`: `name = "linear least squares"`, `shapes = { InputShape::Curve }`, `outputs = { "intercept", "slope" }`, `output_dimensions({x, y}) = { y, y / x }` (using `dimension.hpp`'s division), and `compute<Rep>(std::span<Rep const> x, std::span<Rep const> y)`.
  - `template <CurveExpression C> [[nodiscard]] constexpr auto linear_least_squares(C curve, Citation citation) noexcept;`, returning `OpaqueCall<LinearLeastSquares, C>`.

**Prevents:**
- **Class 5:** the fixture's four wrong implementations all differ from 19/28 (see the fixture). A rounded assertion rounds to 3 dp, never 2.
- **Class 1:** the overflow behaviour the guide states is task 1's measurement, quoted with its compilers.
- **Class 6:** a fit of the same points listed in a different order must give the same coefficients, and that is tested. Order must not matter.

- [ ] **Step 1: Write the failing tests.**

```cpp
namespace
{
struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", unit::Second> {};
struct Length: formula::Quantity<Length, "L", "an invented length", unit::Millimetre> {};
struct Rate: formula::Quantity<Rate, "v", "an invented rate of change", unit::MillimetrePerMinute> {};
struct Offset: formula::Quantity<Offset, "L_0", "an invented starting length", unit::Millimetre> {};

constexpr auto points = formula::environment(
    formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(1) }, formula::Measured<Elapsed> { rat(2) },
                                      formula::Measured<Elapsed> { rat(4) }, formula::Measured<Elapsed> { rat(7) }),
    formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) }, formula::Measured<Length> { rat(109, 10) },
                                     formula::Measured<Length> { rat(121, 10) }, formula::Measured<Length> { rat(143, 10) }));

constexpr auto fit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>),
    { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
} // namespace

TEST_CASE("least squares gives the exact slope and intercept", "[least-squares]")
{
    // 19/28 mm/s is 19/28 * 60 = 285/7 mm/min in Rate's declared unit. Not the
    // secant (41/60 mm/s), not x-on-y (~0.6798), not through-origin (~2.5786),
    // not swapped (~1.4710).
    constexpr auto slope = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), points);
    STATIC_REQUIRE(slope->measurement().value() == rat(285, 7));
    constexpr auto intercept = formula::checked_evaluate<Offset>(formula::opaque_output<"intercept">(fit), points);
    STATIC_REQUIRE(intercept->measurement().value() == rat(19, 2));
}

TEST_CASE("the order the points are listed in does not change the fit", "[least-squares]")
{
    // Same four pairs listed 4, 1, 7, 2 s: identical coefficients (defect class 6).
}

TEST_CASE("a fit whose domain values are all equal, or which has one point, is the fit's own domain error",
          "[least-squares]")
{
    // A one-point curve -> std::unexpected(DomainError), and the trace records Own
    // (Review Focus 1). t = 3, 3, 3, 3 through compute directly -> DomainError;
    // through a curve it is the curve's DuplicatePoint, recorded Propagated (C4).
    // Never slope 0.
}

TEST_CASE("two points give the exact line through them", "[least-squares]")
{
    // (1 s, 10.3 mm), (3 s, 13.9 mm) (C13): slope 9/5 mm/s, intercept 17/2 mm.
}

TEST_CASE("a fit that exceeds Rational's range says Overflow, never a wrong number", "[least-squares]")
{
    // Spike step 3's shape: point k at ((k+1)/(k+2), (2k+3)/(k+3)), N = 15 -> Overflow on
    // all five compilers measured; runtime REQUIRE. The guide states no "safe N": overflow
    // depends on the data (three-decimal readings of ~2500 fail from N = 34, not monotonically).
}

TEST_CASE("least squares works in double, to within the representation", "[least-squares]")
{
    // LinearLeastSquares::compute<double> on the same points in SI: slope within 1e-12
}
```

- [ ] **Step 2: Run them and confirm they fail to build.**
- [ ] **Step 3: Implement.**
  - Compute S_xx and S_xy by centred sums from x̄ and ȳ, using `RepTraits<Rep>`'s checked operations throughout. Spike step 3 measured the centred form overflowing at the same first N as the uncentred form, and at fewer sizes (60 against 63 of 127). The comment cites that measurement.
  - S_xx = 0 returns `DomainError`.
  - The `least_squares_not_a_curve` negative passes two loose series and expects `"formula: linear_least_squares fits a curve; pair the domain and the values with curve(domain, values)"`.
- [ ] **Step 4: Run the tests and confirm they pass.** Check the trace line for the fit against task 3's spelling with the side-table row's `failure == Own` in the degenerate case.
- [ ] **Step 5: Mutations:**
  - regress x on y;
  - drop the centring;
  - return slope 0 for S_xx = 0;
  - swap the output order.

  Each must fail a named test.
- [ ] **Step 6: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 7: Commit.** Message: `feat(least-squares): fit a straight line to a curve as a named opaque operation`.

---

## Task 5: Opaque outputs in methods and overlays

**Files:**
- Modify: `include/formula-cpp/overlay.hpp`
- Modify: `test/method_tests.cpp`, `overlay_tests.cpp`, `vocabulary_tests.cpp`, `opaque_tests.cpp`

**Interfaces:**
- Consumes: tasks 2–4, and phase 12 T5's `ConstantRewrite` for series nodes (a fit's inputs are series or curves).
- Produces:
  - `ConstantRewrite<Sub, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>>>`: `known` if every input's rewrite is known, `mentions` if any input mentions, and `apply` rebuilding the call with rewritten inputs and the **same** citation.
  - `SubstitutedIn<OpaqueOutputNode<…>>`, following `SubstitutedInOperand`.

**Prevents:**
- **Class 6:** an overlay that fixes a quantity read in the fit's input is judged on the rewritten *result*. The test applies `with_constant` to a method whose variant reads the quantity both inside and outside the call, in both orders of the overlay's operations.
- **Class 2:** a `with_constant` on a quantity the method reads as a series inside a call draws phase 12's `"one constant cannot stand for a series"` message **once**, and not `RequireOverlaySeesNode` as well. The negative REJECTs the latter.

- [ ] **Step 1: Write the failing tests.**
  - A method with two variants. `Fitted` is `rounded<unit::MillimetrePerMinute, 1>(opaque_output<"slope">(fit))`, and `Nominal` is a constant.
  - `evaluate_method<Fitted>` gives 285/7 mm/min ≈ 40.714, rounded to 1 dp as 40.7. That rounding is chosen because the secant gives 41.0 and x-on-y gives 40.79 → 40.8, both different.
  - An overlay replaces the variant (`replace_variant`) with a secant formula written as ordinary arithmetic over `interpolate_at`, and the trace shows `ReplacedVariant`, not the opaque step.
  - An overlay `with_constant`s a scalar quantity `Correction` read *outside* the call. `known` holds, and the rewrite leaves the call untouched.
  - A scoped vocabulary renames `Length` to `"l"` in the opaque step's input.
- [ ] **Step 2: Run them and confirm they fail** (today: `RequireOverlaySeesNode`).
- [ ] **Step 3: Implement the two specialisations.**
- [ ] **Step 4: Run the tests and confirm they pass.** Apply the deletion check to the new negative `overlay_constant_inside_opaque_series.cpp` if one is added. Otherwise state which existing negative pins the single message.
- [ ] **Step 5: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 6: Commit.** Message: `feat(overlay): rewrite through an opaque operation's inputs, which are all it reads`.

---

## Task 6: Bounded retry: attempts, judgement, and the six ways it ends

**Files:**
- Create: `include/formula-cpp/retry.hpp`, `test/retry_tests.cpp`
- Create (negatives): `retry_zero_attempts.cpp`, `retry_over_cap.cpp`, `retry_second_attempt_of_one.cpp`, `retry_attempt_dimension.cpp`, `retry_previous_outside.cpp`, `retry_this_attempt_in_attempt.cpp`, `retry_accept_not_predicate.cpp`
- Modify: `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `PredicateNode`/`Predicate`, `Outcome`, `Verdict`, `Citation` and `Environment` (read, R5), plus phase 12 T2's `get_series` for forwarding only.
- Produces:
  - `enum class FirstJudged : std::uint8_t { AtFirstAttempt, AtSecondAttempt };`
  - `enum class RetryEnd : std::uint8_t { Accepted, Exhausted, NotJudgeable, NotRecorded, Failed, ManuallyEntered };`. `NotRecorded` is declared here and first produced in task 8.
  - `template <Node E> struct StartingValue { E expression; };` and `starting_from(E)`
  - `struct AttemptNumberNode: NodeBase { static constexpr Dimension dimension = dim::Scalar; };` and `inline constexpr AttemptNumberNode attempt_number {};`
  - `template <Described R> struct PreviousAttemptNode: NodeBase { using quantity = R; static constexpr Dimension dimension; };` and `previous_attempt<R>`. The same shape gives `ThisAttemptNode<R>` and `this_attempt<R>`.
  - `enum class AttemptPhase : std::uint8_t { Attempting, Judging };` and `detail::AttemptEnvironment<Env, Rep, AttemptPhase P>`, which forwards `get`, `provides`, `is_entered`, `source_of`, `get_series`, `get_observations` and `is_entered_series` (C6), and adds `attempt()`, `previous()` and (Judging only) `current()`.
  - `template <Described R, std::size_t Max, FirstJudged J, typename Start, Node A, Predicate P> struct Retry { Start start; A attempt; P accept; Verdict onExhausted; Citation citation; };`, with no `{}` on `start`, `attempt` or `accept`. `Start` is `StartingValue<E>` or `NoStartingValue`.
  - `template <Described R, std::size_t Max, FirstJudged J, …> [[nodiscard]] constexpr auto retry(…) noexcept;`, in two overloads, with and without a `StartingValue` first.
  - `struct RetryFailure { ArithmeticError error; std::size_t attempt; };`, where `attempt` is zero-based and the text shows it one-based
  - `template <Described R> class RetryOutcome`, with `outcome()`, `end()`, `attempts_made()` (a count) and `accepted_at()` (`std::optional<std::size_t>`, a **zero-based** position, following the library's rule; see R2's ruling since). Only `checked_evaluate_retry` constructs one, through a `detail::` key.
  - `template <typename Rep = Rational, Described R, …, typename Env, typename Sink = NullSink> [[nodiscard]] constexpr std::expected<RetryOutcome<R>, RetryFailure> checked_evaluate_retry(Retry<…> const&, Env const&, Sink = {}) noexcept;`

**Prevents:**
- **Class 2:** the three bound refusals are one struct with ordered `if constexpr` branches, so `retry<R, 0, AtSecondAttempt>` says "zero attempts" and not also "second attempt of one". `retry_previous_outside` evaluates `checked_evaluate<R>(previous_attempt<R> + …)` and draws exactly one message.
- **Class 3:** `RetryOutcome` has no public constructor or setters. `end()` and `accepted_at()` are what the trace summary is built from, and user code cannot set them.
- **Class 5:** the fixpoint fixture separates `≤` from `<`, the loop bound from off-by-one, and accepted from exhausted (see the fixture).
- **Class 6:** "does it accept" is judged on each attempt's *produced* value, and never on an intermediate.

- [ ] **Step 1: Write the failing tests.**

```cpp
namespace
{
struct Estimate: formula::Quantity<Estimate, "w", "an invented iterated estimate", unit::Gram> {};

constexpr auto halving = formula::constant<unit::Gram>(rat(152, 25))   // 6.08 g (C13)
                         + formula::previous_attempt<Estimate> / formula::rat(2);
constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                         >= formula::constant<unit::Gram>(rat(-19, 25));   // w_k - w_{k-1} <= 0.76 g, rising

constexpr auto cite = formula::Citation { .title = "Settled estimate", .reference = "Example Standard 12", .section = "6" };
constexpr auto nothing = formula::environment();
} // namespace

TEST_CASE("a retry accepts on exactly the last permitted attempt, at equality", "[retry]")
{
    constexpr auto four = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<unit::Gram>(rat(0))), halving, settled,
        formula::Verdict { "repeat the determination" }, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == 3);                               // the 4th attempt, zero-based; not exhausted
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(57, 5));    // 11.4 g; a strict < would exhaust at 4
}

TEST_CASE("a retry that runs out of attempts returns the method's verdict, not a value", "[retry]")
{
    constexpr auto three = formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<unit::Gram>(rat(0))), halving, settled,
        formula::Verdict { "repeat the determination" }, cite);
    constexpr auto ran = formula::checked_evaluate_retry(three, nothing);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Exhausted);
    STATIC_REQUIRE(ran->attempts_made() == 3);
    STATIC_REQUIRE(ran->outcome().is_verdict());
    STATIC_REQUIRE(ran->outcome().verdict().label == "repeat the determination");
}

TEST_CASE("an attempt that fails stops the retry and names the attempt", "[retry]")
{
    // attempt = constant<Gram>(rat(152, 25)) / (attempt_number - rat(2)) ... divides by zero
    // at attempt 2 -> std::unexpected(RetryFailure { DivisionByZero, 1 }) (zero-based); task 7
    // checks the trace has no step for attempts 3 and 4 (Review Focus 4).
}

TEST_CASE("previous_attempt with no starting value is the author's mistake at attempt 1", "[retry]")
{
    // No starting_from, FirstJudged::AtFirstAttempt, attempt reads previous_attempt
    // -> Failed, RetryFailure { DomainError, 0 } (zero-based: the first attempt); never empty.
}

TEST_CASE("an absent judgement stops the retry as not judgeable, not as another try", "[retry]")
{
    // accept compares against var<Tolerance>, absent in the environment ->
    // NotJudgeable after attempt 1, outcome empty, attempts_made() == 1.
}

TEST_CASE("a result entered by a person is returned as entered and no attempt runs", "[retry]")
{
    // environment(entered(Measured<Estimate>{rat(113, 10)})) -> ManuallyEntered (11.3 g),
    // attempts_made() == 0, source() == ManuallyEntered.
}
```

- [ ] **Step 2: Run them and confirm they fail to build.**
- [ ] **Step 3: Implement `retry.hpp`.**
  - The loop is `for (std::size_t k = 1; k <= Max; ++k)`, and no other exit condition exists beyond the ends in R2. The file comment states that termination is structural.
  - `previous()` holds the starting value (or nothing), then each attempt's value.
  - `FirstJudged::AtSecondAttempt` skips `accept` at `k == 1`, and the attempt then counts as "not judged" rather than "rejected".
  - The six ends map to `Outcome` exactly as R2's table says.
  - The `ManuallyEntered` short-circuit mirrors `checked_evaluate` (`evaluate.hpp:340`).
- [ ] **Step 4: Write the seven negatives,** each with its message.
  - The cap message names the cap and says why: `"formula: a retry allows at most 64 attempts; the methods this shape exists for repeat a step a few times, and a larger count is almost always a typo"`. The cap is 64 unless task 1 step 5 amends it (R2's ruling).
  - `retry_previous_outside` expects `"formula: previous_attempt, this_attempt and attempt_number are only meaningful inside a retry"` and REJECTs `RequireProvided`'s text.
  - Apply the deletion check to each.
- [ ] **Step 5: Run the tests and confirm they pass.** Confirm the filter count, then run the mutations, each of which must fail a named test:
  - `<` for `≤` in the fixture's accept;
  - `k < Max`;
  - continue on an absent judgement;
  - return the last value on exhaustion.
- [ ] **Step 6: Add the forwarding guard.** Spike step 6 found these six at `525441c`: `provides`, `is_entered`, `is_entered_series`, `get`, `get_series`, `source_of`. Add a test that lists `Environment`'s public members by hand and `STATIC_REQUIRE`s that `AttemptEnvironment` answers each. Its comment says: *"if phase 14 or any later change adds a member nodes call, add it here and forward it"* (R5).
- [ ] **Step 7: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 8: Commit.** Message: `feat(retry): repeat an attempt a bounded number of times and end in a value or the method's verdict`.

---

## Task 7: A retry on every surface: every attempt in the trace, rendered and documented

**Files:**
- Modify: `include/formula-cpp/sink.hpp`, `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`
- Modify: `test/trace_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp`, `retry_tests.cpp`

**Interfaces:**
- Consumes: task 6, and phase 12 T3's `Step` layout and `detail::series_marker<D>` (the `{w}_{k}` family follows it).
- Produces:
  - `StepKind::RetryAttempt` (one per attempt run; operands: the attempt's derivation and, when judged, the judgement's two sides). It reuses `Step::comparison`, and its own data goes in the side table `Trace::attemptSteps`, rows `{ std::size_t step; std::size_t attemptNumber; AttemptJudgement judgement; }` (24 bytes, spike step 10). `enum class AttemptJudgement : std::uint8_t { NotJudged, Accepted, Rejected, NotJudgeable }`.
  - `StepKind::RetryConcluded` (operands: the attempt steps). It reuses `Step::citation`, and its own data goes in `Trace::retrySteps`, rows `{ std::size_t step; std::size_t attemptLimit; RetryEnd end; std::string_view verdictLabel; }` (40 bytes). The verdict label is author text and goes through `step_line`'s escaping. Nothing is added to `Step`.
  - `StepKind::AttemptNumber`, `PreviousAttempt` and `ThisAttempt` through `StepKindOf`. `PreviousAttempt`'s own failure at attempt 1 renders `previous attempt: none before attempt 1`.
  - Optional hooks: `retry_entered`/`retry_produced` and `attempt_entered`/`attempt_produced`, each pair asked for in one `requires`.
  - `explain_retry<Rep>(Retry const&, Env const&, V const& = V {})`, returning `ExplainedRetry<R> { std::expected<RetryOutcome<R>, RetryFailure> outcome; Trace<Rep> trace; }`.
  - `render<D>(Retry const&, V const&)` and `document<D>(Retry const&, V const&)`, with spike step 9's markers: LaTeX `{w}_{k}`, `{w}_{k-1}`, `{w}_{0}`, `{d}_{k}`; Plain `w(k)`, `w(k-1)`, `w(0)`; Markdown `` `w(k)` ``. There is one helper, `detail::attempt_marker<D>`, beside `series_marker` in `render.hpp`. For example, plain text reads: `up to 4 attempts: w(k) = 152/25 g + w(k-1) / 2, starting from w(0) = 0 g; accept when w(k-1) - w(k) >= -19/25 g; otherwise: repeat the determination (pinned from the real output)`.

**Prevents:**
- **Class 1 (the S5 condition):** a render cut inside a retry says how much it cut, and the `Trace` still holds every attempt. The test checks both.
- **Class 3:** the retry and attempt side-table rows are written only by `RecordingSink` from the `RetryOutcome` it was handed.
- **Class 5:** the trace test uses the exhausted fixture (three attempts, none accepted) *and* the accepted one, so a renderer that prints "accepted" for the last attempt of any retry fails.

- [ ] **Step 1: Write the failing tests.**
  - For the four-attempt fixture, the trace has **exactly four** `RetryAttempt` steps. Attempts 1–3 are `Rejected` and attempt 4 is `Accepted`, and one `RetryConcluded` step reads `accepted at attempt 4 of 4`. Every string is pinned exactly from the real output.
  - For the three-attempt fixture, three `Rejected` attempts and `exhausted after 3 of 3: repeat the determination`, with the citation.
  - In the failing-attempt case, there are exactly two `RetryAttempt` steps and none numbered 3 or 4 (Review Focus 4), and the concluded step reads `failed at attempt 2: division by zero`.
  - Budget: `retry<Estimate, 16, …>` of the fixpoint with an unreachable tolerance, rendered with `maxSteps = 10`. The output has at most 10 step lines and a footer with the exact count cut, and `trace.steps` still holds all 16 attempt steps (Review Focus 5).
  - Vocabulary: a scoped vocabulary renames `Estimate` to `"m"`, and every attempt step, the render and the document read `m`.
  - Document: `Documentation` lists the retry's citation and a symbol row for `Estimate` marked as iterated. The row's description says so. **Ruling:** use the description, not a new `ValueShape`, because `ValueShape` describes the shape of the *input* a quantity takes, and an iterated result is still one value.
- [ ] **Step 2: Run them and confirm they fail to build.**
- [ ] **Step 3: Implement.**
  - The recorder opens a mark at `retry_entered` and at each `attempt_entered`.
  - `attempt_produced` claims that attempt's steps, and `retry_produced` claims the attempt steps.
  - Nothing is recorded for an attempt that did not run.
- [ ] **Step 4: Run the tests and confirm they pass.** Then run the mutations, each of which must fail a named test:
  - record a step for unrun attempts;
  - print `Accepted` for the last attempt unconditionally;
  - drop the footer count.
- [ ] **Step 5: Verify on all eight presets, plus Doxygen 1.9.8 and mkdocs.** Confirm that `hygiene.vocabulary-reach` passes.
- [ ] **Step 6: Commit.** Message: `feat(retry): show every attempt, its judgement and how the retry ended`.

---

## Task 8: Recorded attempts: a retry that reads a new determination each time

**Files:**
- Modify: `include/formula-cpp/retry.hpp`, `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`
- Create (negatives): `retry_attempt_input_length.cpp`, `retry_attempt_input_outside.cpp`
- Modify: `test/retry_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`

**Interfaces:**
- Consumes: tasks 6–7, and phase 12 T2's `MeasuredSeries`, `get_series` and length-mismatch message, plus T3's series variable on the trace.
- Produces:
  - `template <Described Q> struct AttemptInputNode: NodeBase { using quantity = Q; static constexpr Dimension dimension = Q::dimension; };` and `attempt_input<Q>`.
  - When evaluated in an `AttemptEnvironment`, it reads element `attempt() - 1` of `get_series<Q, Max>()`.
  - An absent element ends the retry `NotRecorded` at that attempt, **unless** the retry has already ended.
  - `StepKind::AttemptInput`, which renders as the symbol with task 1's attempt marker (`d(k)` / `{d}_{k}`), and `RetryEnd::NotRecorded`'s concluded line `attempt 3 not recorded`.
  - `retry_attempt_input_length` refuses `series<Determination, 3>` in a `retry<…, 4, …>` in phase 12's words, naming both lengths.

**Prevents:**
- **Class 2:** reading a series of the wrong length draws the length message once, and not also `RequireProvided`.
- **Class 5:** the fixture's fourth element (45.7) is what a non-stopping retry would report. Its third (42.7) is the correct answer, and its second (43.9) is the earlier-of-the-pair mistake.
- **Class 6:** an absent element *after* acceptance is never read. The test puts it at position 4 and expects `Accepted` at 3, and puts it at position 3 and expects `NotRecorded`.

- [ ] **Step 1: Write the failing tests.**

```cpp
namespace
{
struct Determination: formula::Quantity<Determination, "d", "an invented determination", unit::Gram> {};
struct Agreed: formula::Quantity<Agreed, "d_a", "an invented agreed determination", unit::Gram> {};

constexpr auto agree = formula::when(formula::this_attempt<Agreed> >= formula::previous_attempt<Agreed>,
                                     formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>,
                                     formula::previous_attempt<Agreed> - formula::this_attempt<Agreed>)
                       <= formula::constant<unit::Gram>(rat(127, 100));   // 1.27 g (C13)

constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Determination>, agree, formula::Verdict { "repeat the test" },
    { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });
} // namespace

TEST_CASE("a retry over recorded determinations stops at the first pair that agrees", "[retry][recorded]")
{
    constexpr auto four = formula::environment(formula::measured_series<Determination>(
        formula::Measured<Determination> { rat(413, 10) }, formula::Measured<Determination> { rat(439, 10) },
        formula::Measured<Determination> { rat(427, 10) }, formula::Measured<Determination> { rat(457, 10) }));
    constexpr auto ran = formula::checked_evaluate_retry(successive, four);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == 2);                                 // the 3rd attempt, zero-based (R2)
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(427, 10));   // 42.7, not 45.7 nor 43.9
}

TEST_CASE("a determination the method needed but nobody recorded ends the retry as not recorded",
          "[retry][recorded]")
{
    // element 3 absent -> NotRecorded at 3, outcome empty, attempts_made() == 3.
    // element 4 absent, 1-3 as above -> Accepted at 3 (element 4 never read).
}
```

- [ ] **Step 2: Run them and confirm they fail to build.**
- [ ] **Step 3: Implement.** `AttemptInputNode`'s evaluator reports an absent element to the retry through the `AttemptEnvironment`, by setting a `notRecorded` flag the loop reads. It does not report it as a plain absent value, which would end the retry as `NotJudgeable` and mislabel it.
- [ ] **Step 4: Write both negatives and apply the deletion check.**
- [ ] **Step 5: Run the tests and confirm they pass.** Then run the mutations, each of which must fail a named test:
  - treat not-recorded as `NotJudgeable`;
  - read element `attempt()` instead of `attempt() - 1`;
  - judge at attempt 1 despite `AtSecondAttempt`.
- [ ] **Step 6: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 7: Commit.** Message: `feat(retry): read a new recorded determination at each attempt, and say when one is missing`.

---

> **Owner-requested addition (2026-09-26):** task 4's `linear_least_squares` fixtures are added to phase 13's overflow census (`test/overflow_census_tests.cpp`, `docs/numeric-headroom.md`) once phase 13 has merged. Report the fit's headroom, both the 20-point and the largest fixture, in task 4's report.

## Task 9: Guide, example and gallery

**Files:**
- Create: `examples/opaque_and_retry.cpp`, `docs/opaque-and-retry.md` ("Opaque operations and bounded retry")
- Modify: `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md` (regenerated, never hand-edited), `mkdocs.yml`, `README.md`, `docs/index.md`, `docs/tracing.md` (the consumer-node paragraph), and the docs-output guard

**The guide is this project's cheapest reachability probe.** If a spelling you reach for does not compile, that is a finding about the library. Report it; do not work around it.

**What a reader is owed**, each decided by a ruling above:
1. Why a fit is not an expression tree, and what "opaque" does and does not hide (O1, O6). Show the trace line with `[inside not shown]`, not a description of it.
2. How to write one's own operation, and why it receives values and never the environment (O1, O7). `docs/tracing.md`'s paragraph on consumer nodes gains one sentence pointing to opaque operations as the traced alternative.
3. That using two outputs runs the operation twice, and why (O3, G1).
4. Least squares: exact in `Rational`, the degenerate case, and the measured overflow limit with its compilers (O8, O9).
5. That a citation is required, and what an empty one looks like on the page (O5).
6. The retry: the six ends, shown by running them (R2); why running out is a verdict and not a missing value (R3); `FirstJudged`, and the attempt-1 mistake (R1, R4).
7. The two-sided agreement written with `when` (R6).
8. One paragraph on how a retry differs from phase 13's outlier rejection, as the lead rules R7. Link that guide if it has merged.
9. That this is not a solver (§16.5 #11), and that §16.8's constructions (hysteresis state machines, set partitioning, graphical constructions) remain out of scope.

**Gallery rules, bought in phase 11:** use `write_worked_formula(out, node)` for every worked section. Never hand-type a formula, a rendering or an expected output. Regenerate, and confirm that `gallery.is-current` passes.

- [ ] **Step 1: Write `examples/opaque_and_retry.cpp`,** ending with `all checks passed: yes`. Register it with `formula_add_example` and a regex pinning every spelling the guide quotes.
- [ ] **Step 2: Run it, and capture the real output.**
- [ ] **Step 3: Write `docs/opaque-and-retry.md`,** quoting only captured output. Extend the `docs.<guide>-output` guard to it.
- [ ] **Step 4: Add the least-squares fit and the four-attempt retry to the gallery,** then regenerate and confirm `gallery.is-current`.
- [ ] **Step 5: Wire the guide into `mkdocs.yml` (nav: "Opaque operations and bounded retry"), `README.md` and `docs/index.md`.**
- [ ] **Step 6: Verify on all eight presets, plus Doxygen 1.9.8 and `mkdocs build --strict`, then commit.** Message: `docs(opaque): add the opaque operations and bounded retry guide, example and gallery entries`.

---

## Self-Review

**1. Spec coverage.**
- §16.5 #10: an opaque named operation over a series (tasks 2–3), regression (task 4), and its coefficients feeding a reported quantity (task 4's `checked_evaluate<Rate>`, task 5's method variant).
- §16.5 #11: the bounded retry/fixpoint shape (task 6), not a solver (R1, guide item 9), shallow (R2's cap).
- §16.2: running out yields a verdict through `Outcome` (R3, task 6).
- §16.1: a manually entered result short-circuits the retry (task 6), and an opaque output under `checked_evaluate` inherits the shipped `entered` short-circuit, which task 2 tests implicitly through `checked_evaluate` and task 5 explicitly through a method.
- §16.7: vocabulary (tasks 3, 5, 7) and overlays (task 5).
- §16.8: the boundary is restated (guide item 9), and nothing here models state machines or partitioning.
- §11: bounded rendering (tasks 3 and 7). Teardown needs nothing new, because steps stay an arena of indices.
- §19: the guide (task 9).

**Not covered, and correctly not:**
- statistics outputs of a fit (G2, after phase 13);
- a retry inside arithmetic or as a method variant (G3);
- `abs` (G4);
- shared sub-results in the trace (G1);
- root-finding (§16.5 #11 found none).

**2. Placeholder scan.** There is no "TBD" and no "similar to Task N". The tests whose bodies are described in comments name their exact inputs and expected values. The spellings that depend on task 1 are marked as pinned from task 1's output, not invented here.

**3. Type consistency.** These names mean one thing throughout:
- `OpaqueCall`, `OpaqueOutputNode`, `opaque`, `opaque_output`;
- `OpaqueFailure { None, Own, Propagated, Undetermined }`, `OpaqueCallFailure`, `OpaqueEvaluated`;
- `InputShape`;
- `Retry`, `retry`, `RetryOutcome`, `RetryFailure { error, attempt }`, `RetryEnd`;
- `FirstJudged`, `AttemptPhase`, `AttemptJudgement`;
- `starting_from`, `attempt_number`, `previous_attempt`, `this_attempt`, `attempt_input`;
- `checked_evaluate_retry`, `explain_retry`.

`StepKind` enumerators carry no factory's spelling: `OpaqueOperation` against `opaque`, `RetryAttempt`/`RetryConcluded` against `retry`, and `AttemptNumber` against `attempt_number`, which differ in case. Each is still checked on GCC `-Wshadow`.

**4. Review Focus.** There are five lines, and each has its test in the owning task: tasks 4, 2, 6, 6/7 and 7.

## Follow-ups: recorded, not scheduled

- **G1. Shared sub-results in the trace.** A call used for two outputs is evaluated and recorded once, with a back-reference, as §11 describes. This needs a recorder change for every node kind.
- **G2. Fit statistics.** Residuals, the coefficient of determination and the standard error of the slope, built on phase 13's statistics once it has merged.
- **G3. A retry as a method variant, or inside arithmetic.** This needs a method result that can be a verdict, or R3's `OnExhausted::UseLastAttempt` alternative.
- **G4. An `abs` node**, and, separately, logical `and`/`or` over predicates.
