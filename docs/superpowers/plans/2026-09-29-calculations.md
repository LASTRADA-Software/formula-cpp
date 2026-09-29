# Calculations and Worksheets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Let an author bind each quantity to the expression that calculates it, and get back a calculation
whose dependency graph is known — and checked — at compile time, plus a worksheet that holds the inputs,
calculates each named value once, recalculates only what a change reaches, answers what-if questions on a
copy, accepts a person's value in place of a calculated one, and explains any result as one derivation per
named value that always describes the current inputs.

Today composition means embedding one expression tree in another (`examples/composition.cpp`,
`docs/expressions.md:356-412`): `var<Q>` always reads the environment (`evaluate.hpp:268-286`), a shared
sub-formula is evaluated twice (`evaluate.hpp:339-347`), nothing is cached, `Environment` is immutable
(`environment.hpp:537-717`), and there is no way to ask which values depend on which. The only binding of a
quantity to a formula is the overlays' `add_derived<Q>`, which inlines the definition into a `Method`
(`overlay.hpp:280-340, 2919-2923`).

**Architecture:**
- `define<Q>(expr)` → `Definition<Q, Expr>`; `calculation(defs...)` → `Calculation<Defs...>`. The quantities
  each definition reads are computed at compile time by a walk over the library's existing fail-closed
  children registry `detail::LevelChildren` (`precision.hpp:338-433`). A `detail::CalculationGraph<Defs...>`
  derives slots, direct reads/readers, transitive closures and a topological order with `std::uint64_t`
  masks (at most 64 quantities), and refuses cycles, duplicates and unknown node kinds with `formula: `
  diagnostics that name the quantities.
- `worksheet(calc, environment(...))` → `Worksheet<Calc>`: inputs as `Measured<I>`, results as
  `std::expected<Outcome<D>, ArithmeticError>`, change revisions, and counters. Each definition is evaluated
  by the unchanged `formula::checked_evaluate<D>` against a `detail::WorksheetView` that exposes only that
  definition's declared reads. Two small backward-compatible hooks in `evaluate.hpp` let a calculated read be
  traced as calculated (`source_of`) and let an upstream failure flow through the evaluator exactly as it
  would through an inlined formula (`checked_get`).
- Incremental recalculation walks the compile-time order: a stale value whose reads did not change since it
  was last verified is reused; a recalculated value equal to its old one keeps its old change revision, so
  its dependents are reused too (early cutoff).
- A derivation is regenerated on demand, one block per named value, where a read of another calculated
  value is one named step — so it always describes the current inputs, even after early cutoff.
- Text (graph description, Graphviz DOT, derivations, documentation) lives in the existing opt-in headers.

**Tech Stack:** C++23, header-only; Catch2 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness.

**Reference:** a prototype of the same idea exists outside this repository (a type-keyed calculation with a
runtime-discovered graph, runtime exceptions for a missing input or a cycle, and a derivation that can
describe an earlier path after early cutoff). This plan keeps its behaviour and removes those three
weaknesses. The worksheet's counts for the prototype's household-bill scenario must match it exactly
(task 5 and task 9).

## Global Constraints

These bind every task. Several exist because the alternative failed in an earlier phase.

- **C++23, header-only**, no dependency beyond the standard library in shipped headers ("Header-only, no
  dependencies", README.md:3).
- **Four toolchains, eight presets.** Must compile warning-free (`FORMULA_WERROR=ON`) on MSVC cl, clang-cl,
  clang++ and g++-14: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`,
  `clang-release`, `gcc-release` (the only one with `-Wshadow -Wconversion -Wpedantic -Werror`),
  `clang-ubsan`. Iterate on `cl-debug` plus `gcc-release`; the controller runs `verify_all.ps1` (all eight
  presets, Doxygen 1.9.8 in WSL, `mkdocs build --strict` on Windows Python) before accepting a hand-in.
- **How to build (Windows):** in PowerShell, dot-source the controller's
  `C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\49514aa0-ad35-4678-8da9-5881ab1d789a\scratchpad\devenv.ps1`,
  then `Set-Location <your worktree>` and **check `Get-Location` before every build** (a failed
  `Set-Location` once built another tree), then `cmake --preset cl-debug; cmake --build --preset cl-debug;
  ctest --preset cl-debug -j 8 --output-on-failure`. **POSIX:** write a script file and run
  `wsl bash /mnt/c/.../script.sh` — never `wsl bash -lc '...'` (it ate `$?`); put an `(exit 42)` control in
  any probe that reads an exit code. Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is
  a *build* error.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:** SPDX header on every file; no
  `NOLINT` anywhere; core public headers include no `<string>`, `<vector>`, `<format>` or `<iostream>`
  (opt-in headers only, `hygiene.headers`); every public `static_assert` message begins `formula: ` and is
  stable (negative tests match on it); never identify a type with `decltype([]{})`; every new header goes
  into the install `FILE_SET` in the root `CMakeLists.txt` (`hygiene.installed-headers`) and into
  `test/consumer_globals_tests.cpp`'s includes.
- **Defect classes.** Read `D:\formula-cpp\.superpowers\sdd\2026-09-25-methods-and-overlays\defect-classes.md`
  before starting. Your report says, for each of its nine classes, what you checked and how.
- **No third-party standard content.** Cite only invented `Example Standard N:YYYY` references. Never name
  a real standards body or standard number anywhere (tests, comments, commit messages, docs) —
  `hygiene.no-real-standards` scans every tracked file. Fixture numbers are plainly invented.
- **Negative tests** (`test/negative/*.cpp` + `formula_add_negative_test(<name> <expected-text>)` in
  `test/CMakeLists.txt`) assert both that the build fails and that it fails with this library's own text.
  Register each with a deliberately wrong expected string first and watch it fail, then the right one.
  **Deletion check** for each: delete the guard it pins, confirm the case then compiles, restore with a
  plain write (or `touch`) — a timestamp-preserving copy leaves ninja trusting stale outputs. Use
  `EXPECT_COUNT 1` where a second message could plausibly fire.
- **No `{}` default member initialiser on any member that holds an expression or a node.**
- **Names.** Under GCC `-Wshadow` and cl C4459 no parameter or local may hide one of the 258 globals in
  `test/consumer_globals_tests.cpp` (e.g. `result`, `value`, `index`, `text`, `step`, `first`, `name`,
  `left`, `right`, `entry`, `sign`, `count`, `symbol`, `number`, `width`, `out`, `ctx`, `fmt`, `format`,
  `mode`, `digits`, `i`, `j`, `k` — read the list). Check each new public name against the list too.
- **Documentation is read by humans.** Every ```` ```text ```` block in a guide is consecutive lines of its
  example's real output and every ```` ```cpp ```` block consecutive lines of its source (checked by
  `docs.<guide>-output` / `-snippets` tests where wired); a quoted compiler diagnostic comes from a real
  compile (`hygiene.documented-diagnostics`). No internal labels in any public text or commit message: no
  phase, task, lane, reviewer or tracker names — every sentence must make sense to a reader who never saw
  this plan. Doxygen comment on every public entity (Doxygen fails on undocumented ones).
- **Do not run clang-format** on existing files (CONTRIBUTING.md: it rewrites 99 files and breaks
  `var<Q> * x`). Match the surrounding style by hand.
- **Catch2 splits test filters on commas.** Prove a filter selected something before trusting its result.
- **Commits.** Conventional subject (`feat(unit): ...`, `test(...)`, `docs(...)`, `refactor(...)`), a body
  that explains why, and the last line exactly `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`.
  Never `--no-verify`, never amend or rewrite commits another task made, never commit files outside your
  task. Every commit builds and passes on its own.
- **You are the only writer in your worktree.** Do not dispatch subagents. Do not touch any other
  worktree or `D:\formula-cpp` itself. End your turn after the task's report; do not start the next task.
- **CHANGELOG.md** gets its entry under `## [Unreleased]` in the task that changes public behaviour.

---

## Where the codebase shaped this design (read before any task)

1. **The reads walk uses `LevelChildren`, not the overlay probes.** `detail::ConstantRewriteOf<Probe,
   Expr>::mentions` (`overlay.hpp:1364-1402`) answers yes/no for one quantity and builds a rewritten type per
   (definition, candidate) pair — wrong shape, expensive. `detail::LevelChildren<N>` (`precision.hpp:338-433`)
   lists each node kind's children as a `std::tuple` with `seen`; an unregistered library kind is refused
   (`RequireLevelChildrenFor`, `precision.hpp:391-408`). Entries: `precision.hpp:435-655` and `:806-811`
   (`PrecisionLimitNode`'s separate `bound` tuple), `overlay.hpp:439-452`, `rejection.hpp:625-642`,
   `retry.hpp:226-241`. Existing walks over it: `precision.hpp:827-838`, `rejection.hpp:468-511`.
   **Gap:** `RecordScopeNode` (`record.hpp:1342`) and `detail::RefusedSeriesScope` (`record.hpp:1394`) have no
   entry — the calculation walk handles both itself before consulting the registry.
2. **A slot whose read failed is still evaluated.** `when()` evaluates only the branch it takes and a binary
   node stops at a left-hand error (`evaluate.hpp:328-374`); a failed read in an untaken branch must not fail
   the slot. `get<Q>()` returns `Measured<Q>` and cannot carry a failure, hence the optional `checked_get`.
3. **Provenance is decided statically today.** `detail::report_input_source` (`evaluate.hpp:253-259`) uses
   the static `Env::is_entered<Q>`; `source_of<Q>()` is never called (`environment.hpp:676-685` anticipates a
   caller). A worksheet's provenance is runtime state, so the hook prefers a runtime `source_of<Q>()`; for
   `Environment`, `BoundEnvironment`, `AttemptEnvironment`, `AbsentEnvironment` (`record.hpp:399-404`) and
   `RecordContext` it gives the same answer, so nothing visible changes. A calculated read uses the existing
   `ValueSource::Derived` (`outcome.hpp:32-40`); `Step::inputSource`'s doc "Never `Derived`"
   (`trace.hpp:1195-1208`) changes.
4. **A real `Environment` cannot be built from the slots:** it cannot express `Derived`, carry an error, or
   change entered-ness at run time (`EntryTraits`, `environment.hpp:312-394`) — it would reproduce the
   "hand-chaining calls it measured" mislabel (`record.hpp:72-73`). A view is required.
5. **Naming.** A member called `get`, `evaluate` or `checked_evaluate` would hide `formula::checked_evaluate`
   inside the class (class-scope lookup suppresses ADL): use `calculate` / `checked_calculate`, and call
   `formula::checked_evaluate` fully qualified. The explain family uses one name per argument kind
   (`explain_series` `trace.hpp:4279`, `explain_retry` `:4367`): `explain_worksheet`.
6. `SymbolEntry::derivedAs` is specific to overlays (`document.hpp:86-100`): add `calculatedAs`.
7. **Vocabulary rule:** every surface that writes a symbol goes through `symbol_of<Q>(vocabulary)`
   (`vocabulary.hpp:385-389`, `cmake/CheckVocabularyReach.cmake`); add `calculation.hpp` to that script's list.
8. **Masks are `std::uint64_t`.** MSVC's constexpr budget is 100,000 steps, depth 512: every graph algorithm
   is O(N²) word operations (4,096 at the cap of 64 quantities).
9. **Class-scope `static constexpr` initialisers are not a complete-class context** (`environment.hpp:557-561`):
   compute the graph in `detail::CalculationGraph<Defs...>` with namespace-scope `consteval` functions.
10. **Adding an example changes a generated page**: every example has a census twin
    (`examples/CMakeLists.txt:14-41`) and `docs/numeric-headroom.md` holds a generated table — regenerate it
    with `formula-cpp-census-page` on cl.
11. `overlay.hpp:1490` has a local `using Definition = ...` — rename it `DefinitionRewrite` so it does not hide
    the new `formula::Definition`.

## Public API (namespace `formula`)

```cpp
// calculation.hpp (in formula.hpp)
template <Described Q, Node Expr> struct Definition { using quantity = Q; Expr expression; /* reads: detail */ };
template <Described Q, Node Expr> [[nodiscard]] constexpr Definition<Q, Expr> define(Expr expression) noexcept;
template <Described Q, SeriesNode S> [[nodiscard]] constexpr ... define(S) noexcept;   // refusal body only

template <typename... Defs> class Calculation { public: std::tuple<Defs...> definitions; };
template <typename... Defs> [[nodiscard]] constexpr Calculation<Defs...> calculation(Defs... definitions) noexcept;

// Graph queries: std::array<std::string_view, K> of symbols, in dependency order (inputs first)
template <Described Q, typename... Ds, Vocabulary V = DefaultVocabulary> constexpr auto dependencies_of(Calculation<Ds...> const&, V const& = V {}) noexcept; // direct reads
template <Described Q, ...> constexpr auto dependents_of(...) noexcept;   // direct readers
template <Described Q, ...> constexpr auto upstream_of(...) noexcept;     // transitive reads
template <Described Q, ...> constexpr auto affected_by(...) noexcept;     // transitive readers
template <typename... Ds, Vocabulary V = DefaultVocabulary> constexpr auto inputs_of(Calculation<Ds...> const&, V const& = V {}) noexcept;
template <typename... Ds, Vocabulary V = DefaultVocabulary> constexpr auto calculation_order(Calculation<Ds...> const&, V const& = V {}) noexcept;
template <Described Q, Described P, typename... Ds> [[nodiscard]] constexpr bool depends_on(Calculation<Ds...> const&) noexcept;
// + the same functions overloaded on Worksheet<Calc> const&

template <typename Calc> class Worksheet {
  public:
    [[nodiscard]] constexpr Calc const& calculation() const noexcept;
    template <Described... Qs> requires(sizeof...(Qs) > 0) [[nodiscard]] constexpr auto checked_calculate() noexcept;
        // one: std::expected<Outcome<Q>, ArithmeticError>; several: std::tuple<std::expected<...>...>
    template <Described... Qs> requires(sizeof...(Qs) > 0) [[nodiscard]] constexpr auto calculate();
        // one: Outcome<Q>; several: std::tuple<Outcome<Qs>...>; throws ArithmeticException (detail::or_throw, error.hpp:104)
    template <Described... Qs> [[nodiscard]] constexpr auto calculate(VarNode<Qs> const&...);          // calculate(var<Total>, var<NetDraw>)
    template <Described... Qs> [[nodiscard]] constexpr auto checked_calculate(VarNode<Qs> const&...) noexcept;
    template <typename... Es> requires(sizeof...(Es) > 0) constexpr Worksheet& set(Es... entries) & noexcept;
    template <typename... Es> requires(sizeof...(Es) > 0) [[nodiscard]] constexpr Worksheet set(Es... entries) && noexcept;
    template <typename... Es> requires(sizeof...(Es) > 0) [[nodiscard]] constexpr Worksheet with(Es... entries) const noexcept;
    template <Described Q> constexpr Worksheet& clear_override() & noexcept;
    template <Described Q> [[nodiscard]] constexpr bool is_overridden() const noexcept;
    [[nodiscard]] constexpr std::size_t recomputed() const noexcept;
    [[nodiscard]] constexpr std::size_t reused() const noexcept;
};
template <typename... Ds, typename... Entries>
[[nodiscard]] constexpr Worksheet<Calculation<Ds...>> worksheet(Calculation<Ds...> const& definitionSet, Environment<Entries...> const& inputEntries) noexcept;
```
- `set` accepts `Measured<Input>`, `Entered<Input>` and `Entered<Calculated>` (an override).
- The `calculate` family is non-const: it updates the cache and the counters (a const member that mutates
  would break "const means safe to read concurrently").
- `calculate<Input>()` returns `Outcome<Input>::value(m, Measured | ManuallyEntered)`.

Opt-in headers:
- `trace.hpp`: `explain_worksheet<Result>(Worksheet<Calc>&, V const& = V {})` → `ExplainedWorksheet<Result, Calc, V>`.
- `trace_render.hpp`: `render_derivation(ExplainedWorksheet const&, TraceRenderOptions)`.
- `render.hpp`: `render<D>(Calculation const&, V const&)` (one `symbol = expression` line per definition),
  `describe_graph(calc, vocabulary)`, `to_dot(calc, vocabulary)`.
- `document.hpp`: `document<D>(Calculation const&, V const&)`.

## Architecture details

**Reads walk (`detail::CalculationReads<N>`).** Primary: `using Children = LevelChildren<std::remove_cv_t<N>>`;
`!Children::seen` → `RequireCalculationSeesNode<N>` (style of `overlay.hpp:1196-1221`); result = `JoinQuantities`
of the children's reads, plus `Children::bound` when present (`precision.hpp:806-811`). Specialisations:
`VarNode<Q>` → `QuantityList<Q>` (exact match, so `OverriddenConstantNode` and `DerivedQuantityNode` fall
through to their registry entries: a constant reads nothing, a derived quantity reads what its expression
reads); `SeriesVarNode`, `ObservationsVarNode`, `RecordScopeNode` → refused; `RefusedSeriesScope` → empty.
Retry context nodes are leaves (the evaluator's existing refusal still fires). `WhenNode` lists `P, Then, Else`
(`precision.hpp:505-508`) → a conservative "may read" set, documented. `Definition<Q, Expr>`:
`static_assert(RequireDefinitionMeasuresQuantity<Q, Expr>::value)` guarded by `refused_already<Expr>()`
(`expression.hpp:157-169`, pattern `overlay.hpp:2827-2837`); `using reads = UniqueQuantities<CalculationReadsOf<Expr>>`;
`static constexpr bool valid`.

**Type lists (`detail/type_list.hpp`).** Move `QuantityList`/`JoinQuantities` there from
`overlay.hpp:2284-2309` (include it from `overlay.hpp`); add `UniqueQuantities`, `QuantitiesWithout<List, Exclude>`,
`quantity_index_v<Q, List>` (a fold in the style of `Environment::index_of`, `environment.hpp:700-707`),
`QuantityAt<I, List>`; keep `index_in_tuple`.

**`detail::CalculationGraph<Defs...>`.** Slots: inputs = reads that are not defined, deduplicated in order of
first appearance (declaration order, then left to right); then definitions in declaration order;
N = NI + ND ≤ 64 (`RequireCalculationWithinCapacity<N>`). `std::array<std::uint64_t, N>`: `reads` (empty for
inputs), `readers` (transpose), `upstream` / `downstream` (Warshall over bit rows). `order`: Kahn, always taking
the lowest ready index (inputs first, then definitions in declaration order wherever valid). Self-reference
(own bit in `reads`) and cycle members (own bit in `upstream`) become
`QuantityList<QuantityAt<members[I]>...>` for `RequireAcyclicDefinitions<...>`. `valid` gates every later
check with `std::conditional_t<valid, …, std::true_type>` (as `overlay.hpp:2880-2888`), so each refusal is one
message. Queries `holds<Q>`, `defines<Q>`, `slot_of<Q>`, `depends_on<Q, P>`; name arrays by walking `order`
and emitting `symbol_of<QuantityAt<i>>(vocabulary)` per set bit (`std::array` sized `std::popcount(mask)`).

**Worksheet storage.** Its type depends only on `Calc`. `Calc _definitionSet` (a copy: nothing dangles, `with()`
works); `std::tuple<Measured<I>...> _inputs`; `std::tuple<std::expected<Outcome<D>, ArithmeticError>...> _results`;
masks `_entered`, `_overridden`, `_hasValue`, `_stale`; `std::array<std::uint64_t, N> _changedAt, _verifiedAt`;
`std::uint64_t _revision = 1`; `std::size_t _recomputed, _reused`. A `detail::WorksheetAccess` friend gives the
view access (`OverlayNodeAccess` pattern, `overlay.hpp:409-431`).

**Algorithm (no recursion, no run-time cycle check).**
- `need(requested)`: start with `requested`; walk `order` backwards; for each slot in `need` that is not
  overridden, add its `reads` (overrides prune their upstream).
- `refresh(need)`: one member function per worksheet type folding over the defined slots in order, calling
  `refresh_slot<S>()` when S is in `need`.
- `refresh_slot<S>()`: overridden, or has a value and not stale → return. Has a value and every read r has
  `changedAt[r] <= verifiedAt[S]` → reuse: clear stale, `verifiedAt = revision`, `++_reused`. Otherwise
  `fresh = formula::checked_evaluate<D>(definition, detail::WorksheetView<Worksheet, graph::reads[S]>{*this})`,
  `++_recomputed`; early cutoff: if `fresh == old` (`std::expected ==`, value and source) leave `changedAt`;
  set `verifiedAt`, `hasValue`, clear `stale`.
- `set(...)`: `RequireDistinctSettings` plus one check per entry. Compare each entry with its slot (an
  input's (measurement, entered) pair; an override's `Outcome`); an unchanged entry is a no-op. If anything
  changed: one `++_revision`; for each changed slot set `changedAt`/`verifiedAt` and
  `_stale |= graph::downstream[slot]`. An override sets the overridden bit, `Outcome::value(m, ManuallyEntered)`
  and the has-value bit.
- `clear_override<D>()`: clear overridden and has-value (forces a recompute), bump the revision, mark
  `downstream[D]` stale.
- `with()`: copy, then `set`; the counters carry over.

**The view (`detail::WorksheetView<Sheet, Visible>`).** `provides<Q> = holds<Q> && (Visible >> slot_of<Q> & 1u)`
(direct reads only — a missed read fails with `RequireProvided`, never a silent stale read);
`is_entered<Q> = false` (so `checked_evaluate<D>` never short-circuits); `is_entered_series<Q> = false`;
`get<Q>()` (a failed slot reads absent); `checked_get<Q>()` → `std::expected<Measured<Q>, ArithmeticError>`
(declared return type); `source_of<Q>()`: input → entered ? `ManuallyEntered` : `Measured`; defined →
overridden ? `ManuallyEntered` : `Derived`. Members `Sheet const* _sheet; std::uint64_t* _readInto;` (non-null
only in explain: records the reads actually made).

**Evaluator hooks (`evaluate.hpp`, backward compatible).** `report_input_source<Q>(node, environment, sink)`
prefers `{ environment.template source_of<Q>() } -> std::same_as<ValueSource>`, then `is_entered`, then
nothing. The `VarNode` evaluator reads through `checked_get` when it exists with that exact return type; on
an error it reports the source, then `detail::report_failure<Rep>` (`evaluate.hpp:219-226`); move the shared
tail (absent / `in_si` / `produced`) into a helper. Same `source_of` preference in `report_replaced_entry`
(`overlay.hpp:479-493`). `BoundEnvironment` (`precision.hpp:174-239`) and `AttemptEnvironment`
(`retry.hpp:263-353`) forward `checked_get` when the wrapped type has it. No new sink hook.

**Failure and absence.** A slot is always evaluated; a read of a failed slot fails at its `VarNode` with the
same `ArithmeticError` and the parent relays or short-circuits exactly as today; an untaken `when()` branch
never reads it. An absent input gives `Empty` downstream. One documented difference: a calculated value is
stored in its declared unit and converted back to SI when read — exact, but a conversion overflow could fail
where the inlined expression would not.

**Derivation.** `explain_worksheet<Result>`: (1) `outcome = sheet.checked_calculate<Result>()`; (2)
`wanted = bit(Result)`; walk `order` backwards over defined slots in `wanted`: an overridden slot's block is
one `Variable` step (evaluate `var<D>` against a view where only D is visible); otherwise re-run
`formula::checked_evaluate<D>(definition, view{readInto = &actual}, RecordingSink{entry.trace, vocabulary})`
and `wanted |= actual`; (3) input blocks for the inputs actually read, in slot order. Result first, inputs
last; an untaken branch gets no block. `WorksheetEntry { std::string_view symbol; std::size_t slot;
WorksheetEntryKind kind /* Calculated, Overridden, Input */; Unit unit; std::optional<Rational> value /*
declared unit */; std::optional<ArithmeticError> error; Trace<Rational> trace; }`; `ExplainedWorksheet<Result,
Calc, V>` holds `std::expected<Outcome<Result>, ArithmeticError> outcome` (failure included, as
`ExplainedSeries`, `trace.hpp:4264-4272`), `std::vector<WorksheetEntry> entries`, a copy of `Calc`, and `V`
(`FORMULA_NO_UNIQUE_ADDRESS`). **Why it is never stale:** every fresh, non-overridden slot satisfies
`value[S] == f_S(values[reads(S)])` — a recompute establishes it; a reuse happens only when no read's
`changedAt` passed `verifiedAt[S]`, and `changedAt` moves on every value change; evaluation is deterministic.
So the re-run reproduces the stored value and describes the current reads. No `Cutoff` policy is needed.

**Text.** `escaped_step_line` (`trace_render.hpp:2540-2634`): a `Variable` step with `inputSource == Derived`
gets `", calculated"` (comma style, like `", entered by hand"`, `:2600-2606`); absent → `"(no value)"`
(next to `enteredButEmpty`, `:2544-2556`). `render_derivation`: one `StepLimit` budget; each header, step and
input line costs 1; header `symbol = <render<Plain>(definition, vocabulary)> = <value in declared unit>`
(a failed value shows `describe(error)`, an empty one `(no value)`); step lines from `detail::step_line`
(`:2667-2677`) numbered per block; an `inputs` block; the footer wording of `render_trace` (`:2724-2733`);
honours `TraceRenderOptions` including `.numbers` when that member exists on the branch it lands on.
`render_derivation(x, {})` must not compile. Proposed shape:
```text
total = subtotal + vat = 591311/5000 EUR
  1. subtotal = 4969/50 EUR, calculated
  2. vat = 94411/5000 EUR, calculated
  3. #1 + #2 = 591311/5000
net_draw = 250 kWh, entered by hand in place of monthly_load - self_used
inputs
  fridge_w = 400 W
  price = 1/4 EUR/kWh, entered by hand
... 12 further steps not shown
```
`describe_graph`: `inputs: a, b` then `name <- reads` padded to the widest name, defined quantities in
topological order and reads in dependency order. `to_dot`: `digraph calculation {\n  rankdir=LR;\n  node
[fontname="Helvetica"];\n…}` with inputs `shape=box`, calculated `shape=ellipse`, `"` and `\` escaped.
Forwarders keep the allowed one-line shape `return render<D>(node, DefaultVocabulary {});`
(`CheckVocabularyReach.cmake:49-52`) or use `V const& = V {}`.

**`document(calculation)`.** `Documentation::formula = render(calc)`; `symbols`: a row per defined quantity in
topological order with `SymbolEntry::calculatedAs = render_in(dialect, definition, vocabulary)`
(`document.hpp:324-337`), then each definition walked with `detail::collect` (`:569-578`) adding input rows
(rows deduplicate by type, `:541-558`); `citations`: every `documented()` citation found.

## Diagnostics (all in `detail`; the message text is tested API)

1. `RequireDefinitionMeasuresQuantity<Q,Expr>`: "formula: this definition's expression measures a different dimension from the quantity it defines; every read of the quantity would be a value it does not measure -- the quantity and the expression appear in this diagnostic as the template arguments Q and Expr of RequireDefinitionMeasuresQuantity"
2. `RequireSingleValueDefinition<Expr>`: "formula: define<Q> takes an expression of one value, and this is a series; a calculation holds single values -- the expression appears in this diagnostic as the template argument of RequireSingleValueDefinition"
3. `RequireSingleValueReadsInCalculation<Q>`: "formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations; read it with var<Q> -- the quantity appears in this diagnostic as the template argument Q of RequireSingleValueReadsInCalculation"
4. `RequireNoRecordReadInCalculation<Role>`: "formula: a calculation's definitions read the worksheet's own values, and this one reads from another record with from_record; evaluate that formula against a record_context instead -- the role appears in this diagnostic as the template argument of RequireNoRecordReadInCalculation"
5. `RequireCalculationSeesNode<N>`: "formula: this definition holds a node kind the calculation cannot see inside; a quantity read there would be missing from the dependency graph and read out of date, so the calculation refuses it -- the node kind appears in this diagnostic as the template argument N of RequireCalculationSeesNode"
6. `RequireDefinitions<Ts...>`: "formula: calculation(...) takes only definitions made by define<Q>(expression); the arguments appear in this diagnostic as the template arguments of RequireDefinitions"
7. `RequireSomeDefinition`: "formula: a calculation defines at least one quantity; calculation() with no definitions calculates nothing"
8. `RequireDistinctDefinitions<Qs...>`: "formula: this calculation defines the same quantity more than once; first-wins and last-wins are equally arbitrary, so neither is guessed -- the quantities defined appear in this diagnostic as the template arguments of RequireDistinctDefinitions"
9. `RequireDefinitionNotSelfReferential<Q>`: "formula: this definition reads the quantity it defines, so it can never be calculated -- the quantity appears in this diagnostic as the template argument Q of RequireDefinitionNotSelfReferential"
10. `RequireAcyclicDefinitions<QuantityList<...>>`: "formula: these definitions read one another in a cycle, so none of them can be calculated first -- the quantities on the cycle appear in this diagnostic as the template arguments of RequireAcyclicDefinitions"
11. `RequireCalculationWithinCapacity<Count>`: "formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more; the count appears in this diagnostic as the template argument of RequireCalculationWithinCapacity -- split it into two, the second reading the first's results as inputs"
12. `RequireCalculationQuantity<Q,Calc>`: "formula: this calculation neither defines nor reads this quantity; the quantity and the calculation appear in this diagnostic as the template arguments of RequireCalculationQuantity"
13. `RequireCalculationInput<Q,Env>`: "formula: this worksheet's environment provides no value for an input of its calculation; supply one, as Measured<Q>::absent() if it was not measured -- the input and the environment appear in this diagnostic as the template arguments of RequireCalculationInput"
14. `RequireEntryReadByCalculation<Entry>`: "formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines; an entry nobody reads would silently do nothing, most likely because it names the wrong quantity -- the entry appears in this diagnostic as the template argument of RequireEntryReadByCalculation"
15. `RequireCalculatedOverriddenByEntry<Q>` (environment and `set`): "formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement; override a calculated value by hand with entered(Measured<Q> { ... }) -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedOverriddenByEntry"
16. `RequireWorksheetSingleValueEntry<Entry>`: "formula: a worksheet holds single values, and this entry is a series or raw observations -- the entry appears in this diagnostic as the template argument of RequireWorksheetSingleValueEntry"
17. `RequireDistinctSettings<Es...>`: "formula: set() was given the same quantity more than once; first-wins and last-wins are equally arbitrary, so neither is guessed -- the entries appear in this diagnostic as the template arguments of RequireDistinctSettings"
18. `RequireSettableQuantity<Q,Calc>`: "formula: set() names a quantity this worksheet's calculation neither reads nor defines; the quantity and the calculation appear in this diagnostic as the template arguments of RequireSettableQuantity"
19. `RequireWorksheetResult<Q,Calc>`: "formula: this worksheet's calculation neither defines nor reads the quantity asked for; the quantity and the calculation appear in this diagnostic as the template arguments of RequireWorksheetResult"
20. `RequireCalculatedQuantity<Q>`: "formula: clear_override names an input of the calculation; only a calculated quantity is overridden by hand, and an input is simply set again -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedQuantity"

One must-not-compile case per diagnostic (12 and 19 each their own), `EXPECT_COUNT 1` where a cascade is
possible (cycle, missing input, series read). The capacity case builds 65 quantities as
`Quantity<Tag<I>, …>` through an `index_sequence`.

## The shared fixture: a household bill (use it in tests from task 4 on)

10 inputs — fridge power (W), fridge hours per day (h), oven power (kW), oven hours (h), heater power (kW),
heater hours (h), solar yield per month (kWh), grid price, feed-in tariff, base fee; 15 calculated values:
fridge_kw = fridge_w (unit conversion), fridge_kwh = fridge_kw · fridge_h, oven_kwh, heater_kwh, daily_load =
sum of the three, monthly_load = daily_load · 30, self_used = solar · 4/5, exported = solar − self_used,
net_draw = monthly_load − self_used, grid_cost = net_draw · price, feed_in_credit = exported · feed_in,
energy_cost = grid_cost − feed_in_credit, subtotal = energy_cost + base_fee, vat = subtotal · 19/100,
total = subtotal + vat. Values: 200 W, 24 h, 5/2 kW, 1 h, 3/2 kW, 4 h, 150 kWh, 8/25 per kWh, 2/25 per kWh,
25/2 base fee. Expected: total = 591311/5000 (118.2622), net_draw = 279 kWh. **Counts:** first run 15
recomputed / 0 reused; price 8/25 → 1/4: 5/0 (total 950215/10000 = 95.0215); price 1/4 again: 0/0; base fee
25/2 → 15: 3/0; fridge 400 W for 12 h (same daily energy — early cutoff at fridge_kwh): 2 recomputed / 8
reused; a `with()` copy with solar 200 kWh: 9 recomputed (net draw 239 kWh), the original then 0/0. Until the
units, money and decimal branches reach `master`, tests declare the units they need locally (as
`examples/composition.cpp:46-48` declares `Euro`) — money as `dim::Scalar` for now; task 9 switches the example
to shipped units and real currency dimensions.

---

## Task 1: Type lists

**Files:** `include/formula-cpp/detail/type_list.hpp`, `include/formula-cpp/overlay.hpp`, `test/compile_time_tests.cpp`.
- [ ] Move `QuantityList`/`JoinQuantities`; add `UniqueQuantities`, `QuantitiesWithout`, `quantity_index_v`,
  `QuantityAt`; rename overlay's local alias to `DefinitionRewrite`. Compile-time tests for each utility.
  No behaviour change: full suite green untouched.
- [ ] Commit: `refactor(type_list): quantity lists shared beyond overlays`.

## Task 2: Environment hooks for calculated reads

**Files:** `include/formula-cpp/evaluate.hpp`, `precision.hpp`, `retry.hpp`, `overlay.hpp`, `environment.hpp`
(doc), `sink.hpp` (doc), `trace.hpp` (`Step::inputSource` doc), `trace_render.hpp` (`, calculated`,
`(no value)`), tests (`evaluate_tests.cpp`, `precision_tests.cpp`, `trace_render_tests.cpp`), `CHANGELOG.md`.
- [ ] The `source_of` preference and the optional `checked_get` exactly as in "Evaluator hooks"; forwarding
  in `BoundEnvironment` and `AttemptEnvironment`; the two trace wordings.
- [ ] Tests with a test-local environment type: `checked_get` failure propagates and is recorded as a
  failure at the variable step; `source_of` is preferred over `is_entered`; `Derived` renders `, calculated`;
  an absent derived read renders `(no value)`; `BoundEnvironment` forwards. **Existing output unchanged**:
  `gallery.is-current` and every `docs.*` test pass untouched.
- [ ] CHANGELOG (`### Changed`: an environment's runtime `source_of` now decides the source a trace
  records). Commit: `feat(evaluate): an environment can report a read's source and failure at run time`.

## Task 3: Definitions and the reads walk

**Files:** new `include/formula-cpp/calculation.hpp` (definitions part), `formula.hpp`, root `CMakeLists.txt`
(FILE_SET), `test/calculation_tests.cpp` (new), `test/CMakeLists.txt`, negative cases for diagnostics 1–5,
`test/consumer_globals_tests.cpp`.
- [ ] `Definition`, `define`, `CalculationReads`, diagnostics 1–5. Coverage test: the walk sees every
  single-value node kind — reuse the every-kind expressions in `vocabulary_tests.cpp:1100-1140` (`Cube`,
  `Cylinder`, `Rounded`) and assert the read lists.
- [ ] Commit: `feat(calculation): define<Q>(expression), and the quantities it reads, known at compile time`.

## Task 4: The calculation graph and its queries

**Files:** `calculation.hpp`, `cmake/CheckVocabularyReach.cmake`, `test/calculation_tests.cpp`, negative cases
for diagnostics 6–12.
- [ ] `CalculationGraph`, `calculation()`, diagnostics 6–12, the queries with a vocabulary.
- [ ] Compile-time tests on the bill fixture: `inputs_of` (the 10 inputs in first-appearance order),
  `dependencies_of<FridgeKwh>` == {fridge_h, fridge_kw} (dependency order), `dependents_of<SelfUsed>` ==
  {exported, net_draw}, `affected_by<Price>` == {grid_cost, energy_cost, subtotal, vat, total},
  `upstream_of<NetDraw>` (14 names), `depends_on` inside a `static_assert`; definitions declared out of order
  are reordered; a `when()` definition reads both branches; names follow a vocabulary.
- [ ] Commit: `feat(calculation): a dependency graph checked at compile time, and queries over it`.

## Task 5: The worksheet

**Files:** `calculation.hpp`, `test/calculation_tests.cpp`, negative cases for diagnostics 13–20, `CHANGELOG.md`.
- [ ] Storage, view, refresh, `calculate` variants, `set`/`with`/overrides/`clear_override`, counters,
  diagnostics 13–20, the query overloads on `Worksheet`.
- [ ] Runtime tests: the fixture's exact counts and values (see the fixture); chaining `set` on a temporary;
  several results; the `var<>` tag form with structured bindings; an input's source read back; override →
  clear → recompute; an override supplied in the environment; division by zero propagates and `calculate`
  throws `ArithmeticException`; an untaken `when` branch still produces a value; an absent input gives `Empty`.
  **Differential test:** worksheet results equal `checked_evaluate` of the hand-inlined expression over
  several input sets. **Incremental == from scratch** over a table of `set` sequences.
- [ ] Compile-time test: a small fixture (4–5 definitions) does a `set` and a recalculation inside
  `STATIC_REQUIRE`; check value and counters. Keep it small (MSVC constexpr budget).
- [ ] CHANGELOG `### Added`. Commit: `feat(calculation): worksheets that recalculate only what a change reaches`.

## Task 6: Derivations

**Files:** `trace.hpp`, `test/calculation_trace_tests.cpp` (new), `test/CMakeLists.txt`.
- [ ] `explain_worksheet`, `WorksheetEntry`, `ExplainedWorksheet` as specified.
- [ ] Tests: block order (result first, inputs last); calculated reads have `inputSource == Derived`; after the
  fridge swap with early cutoff the derivation shows 400 W and 12 h (never 200 W); each block's root equals the
  stored value; an override is its own block; a quantity only in an untaken `when` branch has no block; a
  failed result is included.
- [ ] Commit: `feat(trace): a worksheet's derivation, one block per named value`.

## Task 7: Text

**Files:** `trace_render.hpp`, `render.hpp`, `test/calculation_trace_tests.cpp`, `test/render_tests.cpp`, a
negative case `calculation_render_derivation_no_step_limit` (expected text `StepLimit`).
- [ ] `render(calc)`, `describe_graph`, `to_dot`, `render_derivation`; exact-text tests of all four, including
  the step-limit footer, and that the DOT escapes a quote.
- [ ] Commit: `feat(render): describe a calculation's graph, as text and as Graphviz DOT, and render a derivation`.

## Task 8: `document(calculation)`

**Files:** `document.hpp`, `test/document_tests.cpp`.
- [ ] `SymbolEntry::calculatedAs`, `document(calc)` as specified; tests for row order, `calculatedAs` text,
  input rows deduplicated, citations collected.
- [ ] Commit: `feat(document): document a calculation, each calculated value with its definition`.

## Task 9: The electricity bill and the calculations guide

**Precondition:** the units, money-dimension and decimal-display branches are on `master`; rebase this branch
first (the controller does it). This task uses `unit::Watt`, `Kilowatt`, `Hour`, `KilowattHour`,
`formula::base_dimension`, and `TraceRenderOptions::numbers` / `NumberStyle`.

**Files:** new `examples/electricity_bill.cpp`, `examples/CMakeLists.txt`, new `docs/calculations.md`,
`mkdocs.yml`, `README.md`, `docs/index.md`, `docs/expressions.md`, `docs/numeric-headroom.md` (regenerated),
`CHANGELOG.md`, `test/consumer_globals_tests.cpp` probe.
- [ ] `examples/electricity_bill.cpp`: the fixture above as a real program, declaring **its own currencies**
  with `base_dimension`: `Euro` (`"EUR"`, 2 decimals), `EuroCent` (1/100 of it), `EuroPerKilowattHour`
  (`Euro.dimension / dim::Energy`, magnitude so that 1 EUR/kWh is right, 4 decimals), and `Yen` (`"JPY"`,
  0 decimals) with a `static_assert` that EUR and JPY measure different dimensions (comment: an exchange rate
  is data, not a constant). VAT (19/100) and the self-use share (4/5) are dimensionless constants; the total is
  rounded to cents with a rounding node. It prints, with decimals: first run (`118.26 EUR`, net draw `279 kWh`,
  recomputed 15, reused 0), price 0.32 → 0.25 (`95.02 EUR`, 5/0), the same price again (0/0), base fee → 15
  (`98.00 EUR`, 3/0), fridge 400 W for 12 h (2/8), `describe_graph`, `affected_by<Price>`,
  `upstream_of<NetDraw>`, `dependents_of<SelfUsed>`, the DOT, a `render_derivation` showing `fridge_w = 400 W`
  after the cutoff, a what-if copy with solar 200 kWh (`85.14 EUR`, `239 kWh`, 9 recomputed) with the original
  unchanged (0/0), a manual override of `net_draw` and its clearing, and a division-by-zero failure
  propagating. Self-checks every number; ends `all checks passed: yes`.
- [ ] `examples/CMakeLists.txt`: `formula_add_example(electricity_bill electricity_bill.cpp <regex>)` pinning the
  counts, the four totals and `all checks passed: yes`; `docs.calculations-output` / `docs.calculations-snippets`
  wired like `examples/CMakeLists.txt:136-147`, with `EXAMPLE_EXE` = this example and `GUIDE` =
  `docs/calculations.md`.
- [ ] `docs/calculations.md`, for a C++ developer who has never seen the library: why (composition inlines a
  definition, so a shared sub-result is evaluated again and nothing remembers it); `define` / `calculation`; the
  graph known at compile time (queries, `static_assert(depends_on…)`, `describe_graph`, DOT); worksheets and
  inputs; `calculate` (one result, several, the `var<>` form, checked vs throwing); `set`, early cutoff and the
  counters; `with()` for what-if; derivations and why they are never stale; overrides by hand; failure and
  absence; refusals (a `<!-- snippet: not from the example -->` block per refusal with its verbatim message from a
  real compile); limits (single values only, no `from_record`, 64 quantities, a "may read" graph under `when`, the
  declared-unit round trip). Link it from `docs/expressions.md`'s composition section.
- [ ] `mkdocs.yml` nav after "Tracing and audit trails"; `docs/index.md`; README guide-table row
  `| [Calculations and worksheets](docs/calculations.md) | Named values defined by expressions, a dependency graph checked at compile time, a worksheet that recalculates only what a change reaches, what-if copies, overrides, and a derivation per named value |`
  and status row `| Calculations: definitions, dependency graph, incremental worksheets | shipped |`; the
  consumer-globals sentence if its list changes; CHANGELOG; regenerate `docs/numeric-headroom.md` on cl.
- [ ] Commit: `docs(calculations): an electricity bill as a worksheet, and a guide to calculations`.
