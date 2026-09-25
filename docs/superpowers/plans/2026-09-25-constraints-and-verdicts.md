# Constraints and Verdicts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a constraint — a relationship a standard states purely to validate — a first-class declaration alongside a formula, evaluating to a verdict rather than a value and appearing in the trace as its own step.

**Architecture:** A `Constraint` pairs an existing phase-8 `PredicateNode` with the `Verdict` to reach when the predicate does not hold, plus a `Citation` saying where the rule comes from. Checking one yields a four-state `ConstraintOutcome`, never a `bool`. Constraints are peers of expressions, not nodes in them, so each of rendering, tracing and `document()` gets its own entry point for them, the way `PredicateNode` already does.

**Tech Stack:** C++23, header-only, Catch2 via CPM, CMake presets for MSVC cl / clang-cl / clang++ / GCC.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — §9 ("Constraints are peers of formulas"), §16.2, §17 row 9.

**Design decisions:** `.superpowers/sdd/phase-9-prep/design-decisions.md`. Read it; it records why each choice was made and which are load-bearing.

## Global Constraints

- **C++23**, header-only, no dependency beyond the standard library in shipped headers.
- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. GCC is the only configuration running `-Wshadow -Wconversion -Wpedantic -Werror` and is the one that catches what the others miss.
- Verify on **all four Windows presets plus GCC in WSL** (`~/fcpp-gcc`). Do not use `out/build/review` or `out/build/fix3`.
- **Baseline: 331 tests green** at `master` (`6787b3e`).
- **The GCC/WSL leg can silently link a stale binary, and an incremental build will not tell you.** Task 6 hit this: a WSL rebuild produced a binary printing `unchecked` where the source says `not checked` — text that exists nowhere in the tree. `ninja -t clean` plus a full rebuild fixed it immediately. The cause is timestamp lag across DrvFs, between Windows-mounted sources and a native Linux build directory, and it is most likely right after rapid edit cycles such as mutation testing. **Before reporting the GCC leg green after any round involving repeated edits, `ninja -t clean` and rebuild.** "ninja: no work to do" is not reassurance here; it is the symptom.
- **Never pipe a build to `/dev/null`.** A `STATIC_REQUIRE` failure is a *build* error; hiding build output lets the following `ctest` report a pass from the previous binary. This project has reproduced that trap three times, once deliberately.
- When restoring a file, **copy rather than move, then `touch` it** — a moved-back file can land with an mtime older than its object and ninja will skip the rebuild. "ninja: no work to do" after an edit means something is wrong.
- Open source; **no third-party standard text may enter the repo**. Invented example standards ("Example Standard 5:2020") are the established convention and are fine.
- **No release, no tag**, ever, by anyone.
- `docs/gallery.md` is **generated** by `tools/gallery/main.cpp` and checked by the `gallery.is-current` test. Edit the tool and regenerate; never hand-edit the page.
- Every documentation snippet must be **pasted from a real compiled run**. Phase 8 withdrew two rendering spellings after they reached the guide.
- **A test whose interesting case sits last proves nothing about early exit or ordering.** Task 2 found this the hard way: mutating `check_all` to stop at the first `NotChecked` was invisible to the straightforward test, because `NotChecked` happened to be the last constraint and nothing observable changed. Only a reversed-order section caught it. The same family has now bitten this project three times — phase 8's arity mutation passed all five tests because the failure was placed where nothing short-circuits, and a `collect(walk, node.rhs)` walk went entirely untested because every predicate put its variable on the left. **Position is part of a test's meaning.** When a task tests a sequence, an asymmetric pair, or a two-sided operation, place the interesting case where an error would actually be observable, and test both arrangements.

## Interfaces that already exist — read from the tree at `6787b3e`, not guessed

Eight things this project's plans asserted in phase 8 turned out to be wrong when someone ran them. These were read:

| Thing | Where | Exact shape |
|---|---|---|
| `Comparison` | `predicate.hpp` | `enum class`; `Less`, `LessOrEqual`, `Greater`, `GreaterOrEqual`, `Equal`, `NotEqual` |
| `PredicateNode<Op, Left, Right>` | `predicate.hpp` | members `lhs`, `rhs`; `static constexpr Comparison comparison`. **Not** a `Node` — no `dimension` |
| `checked_evaluate_predicate` | `predicate.hpp:172` | `template <typename Rep = Rational, Comparison Op, Node Left, Node Right, typename Env, typename Sink = NullSink>` returning `std::expected<std::optional<bool>, ArithmeticError>`, taking `(predicate, environment, sink = {})` |
| `Verdict` | `outcome.hpp:57` | `struct { std::string_view label; }`, memberwise `==` |
| `InvalidReason` | `outcome.hpp` | `struct { std::string_view label; }` |
| `Citation` | `citation.hpp:36` | `std::string_view` members `title`, `reference`, `section`, `equation`, `text` |
| `DocumentedNode<Inner>` | `citation.hpp:58` | `template <Node Inner>` — **cannot wrap a constraint**, which is why a constraint carries its own `Citation` |
| `ArithmeticError` | `arithmetic.hpp` | the error alternative of `Evaluated<Rep>` |
| `StepKind` | `trace.hpp` | `Variable, Constant, PiConstant, Negate, Add, Subtract, Multiply, Divide, Power, Root, Documented, Round, RoundSignificant, Conditional, NumericValue` |
| `Step<Rep>` | `trace.hpp` | fields include `kind`, `symbol`, `citation`, `exponent`, `granularity`, `mode`, `comparison`, `branch`, `dimension`, `unit`, `sourceUnit`, `justification`, `value`, `error`, `operands` |
| `RecordingSink<Rep>` | `trace.hpp` | `entered(N const&)`, `produced(N const&, Evaluated<Rep> const&)`; reads node facts through `if constexpr` blocks in `produced` |
| `collect` | `document.hpp` | declared in a forward-declaration block (~line 95) **and** defined (~line 128); five phase-8 overloads already exist |

**`std::string_view` is this project's convention for labels**, pointing at static storage — `Citation` uses it for all five of its fields and `Verdict` for its one. Do not invent an owning string type for phase 9; match the convention. If a runtime-assembled verdict is ever needed that is a separate decision for a later phase, and it would change `Citation` too.

## File Structure

- **Create** `include/formula-cpp/constraint.hpp` — `ConstraintOutcome`, `Constraint`, `constraint()`, `check()`, and checking a set.
- **Create** `test/constraint_tests.cpp` — evaluation behaviour.
- **Create** `test/negative/` cases as each task requires.
- **Modify** `include/formula-cpp/trace.hpp` — `StepKind::Constraint`, the verdict on `Step`.
- **Modify** `include/formula-cpp/trace_render.hpp` — the `Constraint` step's text.
- **Modify** `include/formula-cpp/render.hpp` — rendering a constraint in three dialects.
- **Modify** `include/formula-cpp/document.hpp` — a `collect` overload reaching the predicate's **both** sides.
- **Modify** `include/formula-cpp/formula.hpp` (umbrella), `CMakeLists.txt` (install `FILE_SET`), `test/CMakeLists.txt`.
- **Create** `docs/constraints.md`, `examples/constraints.cpp`; **modify** `mkdocs.yml`, `docs/index.md`, `README.md`, `tools/gallery/main.cpp`, `docs/gallery.md`.

`hygiene.installed-headers` will fail the moment `constraint.hpp` exists without being added to the install `FILE_SET`. That check was added after phase 7 shipped three uninstalled headers, and it caught `rounding_node.hpp` within minutes of it existing. Expect it, do not work around it.

---

## Task 1: `ConstraintOutcome` and `Constraint`

**Files:**
- Create: `include/formula-cpp/constraint.hpp`
- Create: `test/constraint_tests.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `PredicateNode`, `checked_evaluate_predicate` (`predicate.hpp`); `Verdict` (`outcome.hpp`); `Citation` (`citation.hpp`); `NullSink` (`sink.hpp`).
- Produces: `ConstraintOutcomeKind`, `ConstraintOutcome`, `Constraint<P>`, `constraint(...)`, `check<Rep>(constraint, environment, sink = {})`. Tasks 2-6 all consume these.

**The safety property this task exists to establish.** A predicate has three outcomes — it holds, it does not hold, or it never resolved because an input was absent or the arithmetic broke. A constraint must carry that distinction out. **A constraint whose predicate could not be evaluated must never report satisfied.** A check that silently passes when its input is missing is worse than no check: it produces a record saying the specimen was verified when nothing verified it.

Four states, and `NotChecked` and `Invalid` stay separate for the same reason `Empty` and `Invalid` are separate on `Outcome` — "nobody measured the input" and "we tried and the arithmetic broke" call for different action from whoever reads the report.

- [ ] **Step 1: Write the failing tests.**

These use the same fixture shape `test/predicate_tests.cpp` already uses — a
local `strengthOf` helper building an environment, `constexpr auto` results,
`CHECK` on them. Copy that file's spellings; they were read from it:

```cpp
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto strengthOf(long long value)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { value } });
}

// An environment that measured nothing at all, for the not-checked case.
[[nodiscard]] constexpr auto nothingMeasured()
{
    return formula::environment(formula::Measured<Strength> {});
}

inline constexpr auto minimumStrength =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" },
                        formula::Citation { .title = "Minimum compressive strength",
                                            .reference = "Example Standard 7:2020",
                                            .section = "5.1" });

TEST_CASE("constraint: a satisfied relationship yields no verdict", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, strengthOf(45));
    STATIC_REQUIRE(outcome.is_satisfied());
    CHECK_FALSE(outcome.verdict().has_value());
}

TEST_CASE("constraint: a violated relationship carries the verdict it was declared with", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, strengthOf(20));
    STATIC_REQUIRE(outcome.is_violated());
    REQUIRE(outcome.verdict().has_value());
    CHECK(outcome.verdict()->label == std::string_view { "reject the specimen" });
}

TEST_CASE("constraint: an unmeasured input is not checked, and is never satisfied", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, nothingMeasured());
    STATIC_REQUIRE(outcome.is_not_checked());
    STATIC_REQUIRE(!outcome.is_satisfied());   // the property this whole task exists for
    STATIC_REQUIRE(!outcome.is_violated());    // and it is not a failure either
}

TEST_CASE("constraint: arithmetic that breaks while checking is invalid, not satisfied", "[constraint]")
{
    // A rule whose predicate divides by a measured zero. Build it with the
    // same shape predicate_tests.cpp uses for its own arithmetic-failure case
    // -- read that test rather than inventing a second way to provoke one.
    constexpr auto outcome = formula::check(dividesByZero, strengthOf(0));
    STATIC_REQUIRE(outcome.is_invalid());
    STATIC_REQUIRE(!outcome.is_satisfied());
    REQUIRE(outcome.error().has_value());
}
```

**`STATIC_REQUIRE` failures are build errors, not test failures.** If the
build output is hidden, the following `ctest` reports a pass from the previous
binary — this project has reproduced that three times. Keep build output
visible.

Confirm `formula::Measured<Strength> {}` is actually how an absent measurement
is spelled before relying on it; `quantities.md` or `measured.hpp` is the
place to check. If it is spelled differently, use the real spelling and note
the correction in your report.

- [ ] **Step 2: Run to verify failure.** Expect a build error — `constraint.hpp` does not exist.

- [ ] **Step 3: Implement.** `ConstraintOutcome` holds the kind, an optional `Verdict`, and an optional `ArithmeticError`. `Constraint<P>` holds the predicate, the verdict and the citation. `check` mirrors `checked_evaluate_predicate`'s signature exactly — `template <typename Rep = Rational, typename P, typename Env, typename Sink = NullSink>` — and maps its three-state return onto the four states.

Mapping, and it is the whole implementation: error → `Invalid`; `std::nullopt` → `NotChecked`; `true` → `Satisfied`; `false` → `Violated`.

**Do not add a `bool satisfied()` or an `operator bool`.** The temptation is strongest for callers who "just want to know if it passed", and that accessor is where the safety property dies — it must answer *something* for the two unresolved states, and either answer is a lie. Write that reasoning in a comment; someone will propose it.

- [ ] **Step 4: Run to verify the tests pass.**

- [ ] **Step 5: Prove the safety property is load-bearing.** Change the mapping so `std::nullopt` yields `Satisfied` instead of `NotChecked`. Confirm the unmeasured-input test fails. Restore, confirm it passes. Report both directions.

- [ ] **Step 6: All four presets and GCC, then commit.**

---

## Task 2: Checking a set of constraints reports every one

**Files:** modify `include/formula-cpp/constraint.hpp`, `test/constraint_tests.cpp`

A specimen can fail two checks at once, and a report naming only the first sends someone back for a second round.

**Every constraint is evaluated. There is no short-circuit.** This is deliberately the opposite of `when()`, which evaluates only the branch it takes — and the reason differs: `when()` skips a branch because evaluating it could raise an error that has nothing to do with the answer, whereas every constraint is about the answer. **Say this in a comment**, because a reader who has just read `conditional.hpp` will assume short-circuiting is the house style and "fix" it.

**Interfaces:** produces `check_all<Rep>(environment, sink, constraints...)` or a `constraints(...)` bundle — decide which, and say why in your report. Phase 11 will bundle constraints into a `Method` via `formula::constraints(dimensional_tolerance)` per spec §9.1, so prefer the shape that phase 11 can reuse rather than one it would have to replace.

- [ ] **Step 1: Write the failing tests** — two constraints both violated, asserting both verdicts come back in declaration order; one satisfied and one violated; one violated and one not-checked, asserting the not-checked one is still reported rather than dropped.
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run to verify the tests pass.**
- [ ] **Step 5: Mutation** — make it stop at the first violation. Confirm the both-violated test fails. Restore, confirm. Report both directions.
- [ ] **Step 6: All four presets and GCC, then commit.**

---

## Task 3: A constraint's verdict is its own trace step

**Files:** modify `include/formula-cpp/trace.hpp`, `include/formula-cpp/trace_render.hpp`, `include/formula-cpp/constraint.hpp`, `test/trace_tests.cpp`, `test/trace_render_tests.cpp`

The spec requires this twice — §9 "appearing in the trace as its own step" and §9.1 "each constraint's verdict".

**Check the new enumerator against every name in namespace `formula` before committing, on GCC.** Phase 7 shipped `StepKind::Pi`, which shadowed `formula::Pi` and broke GCC alone while all four Windows presets passed; it is spelled `PiConstant` now for that reason. `StepKind::Constraint` will share a name with the new public `Constraint` type — verify under `-Wshadow`, and if it complains, rename the enumerator, not the type.

**The spelling rule phase 8 paid for.** The `Conditional` step originally read `when(#1, #2, #3)`, positionally identical to the public `when(predicate, then, else)` but meaning something else entirely, so a reader mapped every operand to the wrong thing. The condition derived from fixing it:

> a trace's call spelling is safe only when its operand positions are in bijection with the positions a reader already has for that name.

So do **not** spell this step as a call resembling `constraint(predicate, verdict)`. Follow the `Conditional` fix — keywords naming their own slots, as in `if #1 > #2 then #3`. A shape like `require #1 >= #2 — satisfied` / `— reject the specimen` borrows no API name and needs no prose to decode.

Reach the comparison the way the `Conditional` step already does: `std::remove_cvref_t<decltype(...)>::comparison` inside the `if constexpr` block that already exists in `RecordingSink::produced`. **Do not add a member to `Constraint` to expose it** — phase 8 added exactly that to `WhenNode` and then removed it, 44 lines of public surface the corrected mechanism did not need.

A constraint is not a `Node`, so `entered`/`produced` — constrained on `Node` — will not accept it. Phase 8 solved the same problem for `when()`'s branch with an optional `sink.branch_taken(node, thenTaken)` hook called through `if constexpr (requires { ... })`. Follow that pattern; it is established and approved.

- [ ] **Step 1: Write the failing tests** — a trace over a satisfied constraint, a violated one, a not-checked one and an invalid one, checking the step kind, the verdict recorded, and the operand indices; then `render_trace` output for each.
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Verify the text reads correctly at every arity.** A constraint's step claims the predicate's operand steps, and — exactly as with `Conditional` — if the predicate's **left** side errors the right is never dispatched, so the step can carry one operand rather than two. Phase 8's renderer comment said "two … or three" and had never anticipated the one-operand case. Work out the arities this step can actually have, test each, and state the invariant in a comment.
- [ ] **Step 5: Mutation** — record the wrong verdict; confirm a test fails; restore; confirm. Report both directions.
- [ ] **Step 6: All four presets and GCC, then commit.**

---

## Task 4: Render a constraint in three dialects

**Files:** modify `include/formula-cpp/render.hpp`, `test/render_tests.cpp`

A constraint is not a `Node`, so it needs its own `render` entry point, the way `PredicateNode` got one in phase 8 — there is already a second `render()` overload pair constrained on `Predicate` rather than `Node` to copy.

**Read the output as a person would, in every dialect, before you believe a passing test.** This project has produced four rendering defects whose text was structurally well-formed, brackets balanced, string comparisons passing, and wrong only when read — and the worst of them, a `[...]` spelling that CommonMark parsed as a link, made the operand vanish from the published page entirely.

**A Markdown guard test already exists** — it asserts no Markdown rendering contains `](` or `[`. Extend it to cover constraints. It is the only check in the suite that is not blind to that class.

- [ ] **Step 1: Write the failing tests** — a constraint in Plain, Markdown and LaTeX; and the verdict's place in the rendered text, or a stated reason for leaving it out. A rendered constraint states the *rule*; whether the consequence belongs in the formula text is a judgement call — make it, and say why in a comment, as task 5 of phase 8 did for `RoundingMode`.
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run, then read every rendering aloud as a reader.** Report what you read, not only that the tests passed.
- [ ] **Step 5: All four presets and GCC, then commit.**

---

## Task 5: Walk a constraint in `document()`

**Files:** modify `include/formula-cpp/document.hpp`, `test/document_tests.cpp`

A constraint has a `Citation` and a predicate whose two sides hold variables. Both must reach the documentation, or a formula page silently omits the rules that validate it.

`collect` is declared in **two** blocks — forward declarations around line 95 and definitions from about 128. Add to both, matching the convention all thirteen existing overloads follow. (The forward declarations are *not* required for correctness — ADL via `Walk&` in `formula::detail` finds every overload regardless of order, confirmed by removing all of them — the comment above that block now says so accurately. Match the convention anyway.)

**Cover both sides of the predicate, and test them independently.** The task-7 review found `collect(walk, node.rhs)` completely uncovered because every predicate in the tests put the variable on the left and a constant on the right; deleting that line left the whole suite green. The whole-branch review then found the identical asymmetry one level over, in the evaluator. Use a two-variable predicate, and mutate each side away separately.

- [ ] **Step 1: Write the failing tests** — a constraint's citation reaching the documentation; a variable on the predicate's **left** reaching the symbol table; a variable on its **right** reaching it, as a separate case.
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run to verify the tests pass.**
- [ ] **Step 5: Mutation, both directions separately** — drop the left walk, confirm which tests fail; restore; drop the right walk, confirm; restore. Report all four results.
- [ ] **Step 6: All four presets and GCC, then commit.**

---

## Task 6: Guide, example and gallery

**Files:** create `docs/constraints.md`, `examples/constraints.cpp`; modify `mkdocs.yml`, `docs/index.md`, `README.md`, `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md`

**Every snippet and every output block must come from a real compiled run.** Phase 8 withdrew two rendering spellings after they had already reached the guide; pasting what you expect output to say rather than what it said is exactly how a withdrawn spelling survives.

**The guide must make the safety argument concretely, with a worked case where the input is missing.** Show that the constraint reports "not checked" rather than "satisfied", and say plainly why a library that reported "satisfied" there would be producing a false record of verification. That is the reason this phase exists, and prose alone does not carry it — show the outcome.

**Examples are ctest cases** whose stdout must match a regular expression, so the example must print something stable. `examples/CMakeLists.txt` records in its own comment that a failing example once put a modal "Debug Error!" dialog on a maintainer's screen. No bare `assert`, no unhandled error path.

Add a gallery entry showing a constraint's verdict as a trace step, alongside the formula it validates.

Update the status tables in `README.md` and `docs/index.md`: constraints move from planned to shipped; lookup tables, methods, series and statistics stay planned.

- [ ] **Step 1: Write the example, compile and run it.**
- [ ] **Step 2: Write the guide, pasting every block from that run.**
- [ ] **Step 3: Add the gallery entry to the tool; regenerate; confirm `gallery.is-current` passes.**
- [ ] **Step 4: Update nav and status tables.**
- [ ] **Step 5: Build the Doxygen target and run `mkdocs build --strict`.** Both are clean at `master` and must stay clean — these are the two legs that went red in CI after phase 7 merged, and no other local step exercises them.
- [ ] **Step 6: All four presets and GCC, then commit.**
