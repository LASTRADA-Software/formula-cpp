# Lookup Tables Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a method select a value from a table three ways — by an exact key, by which band a measurement falls in, and by interpolating between two rows — with a band table that has a gap or an overlap rejected rather than silently mis-bucketed.

**Architecture:** A table's *structure* — its bands — lives in the type as an array of int64 numerator/denominator pairs, so gaps and overlaps are a `static_assert`. Its *contents* may be populated at runtime, the way `ConstantNode` already holds a `Rational` behind a compile-time shape. A lookup is an ordinary `Node`, so it rides the existing evaluation, rendering, tracing and documentation machinery. A lookup that matches nothing yields an error carrying structured fields, never a substituted number and never a composed sentence.

**Tech Stack:** C++23, header-only, Catch2 via CPM, CMake presets for MSVC cl / clang-cl / clang++ / GCC.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — §17 row 10, §16.3 item 3, and §16.1, which bounds what this phase may contain.

**Design decisions and spike evidence:** `.superpowers/sdd/phase-10-prep/design-decisions.md` and `spike-findings.md`. Read both. Every interface below was compiled on all four compilers before this plan was written.

## The scoping constraint — read before anything else

Spec §16.1:

> **Test methods are code; product specifications are data.** A method's algebra is fixed and belongs in a library. The limit tables a product is judged against are master data, registered per customer, per region, per contract. formula-cpp expresses methods and **never specification tables**.

This phase builds the lookup *mechanism* a method's own algebra uses — a correction factor the method publishes as part of its procedure. The limit values a customer registers against a product stay outside the library. If a task finds itself designing storage for per-customer limits, it has left the phase.

And §3 still binds: **no real published table's values, thresholds, clause numbers or citations enter this repo.** Invented example standards only, exactly as phases 6–9 did.

## Global Constraints

- **C++23**, header-only, no dependency beyond the standard library in shipped headers.
- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. GCC alone runs `-Wshadow -Wconversion -Wpedantic -Werror` and catches what the others miss.
- Verify on **all four Windows presets plus GCC in WSL** (`~/fcpp-gcc`). Never `out/build/review` or `out/build/fix3`.
- **Baseline: 359 tests green** at `master` (`5c340c8`).
- **Branch from `master`**, never from a previous phase's branch tip. Phase 9 inherited a merge conflict caused solely by that.
- **Never pipe a build to `/dev/null`.** A `STATIC_REQUIRE` failure is a *build* error; hiding it lets the next `ctest` report a pass from the previous binary.
- When restoring a file, **copy rather than move, then `touch` it**.
- **One working tree, one writer** — and a reviewer running mutations is a writer. A symptom involving text that appears in no commit is a concurrency symptom until proven otherwise; check `git grep` and `git log -S` before blaming the build system.
- **Position is part of a test's meaning.** A test whose interesting case sits last proves nothing about ordering or early exit; a two-sided operation tested on one side proves nothing about the other. A table has rows, so this applies with unusual force: a test whose interesting row is first or last proves nothing about row selection.
- **A test that fails for the wrong reason proves nothing either.** A cross-surface test must not locate either surface by the thing it is comparing.
- **Reachable is not the same as present.** Phase 9 shipped an overload that worked, was tested, and no user could call. For every public entity, a test must reach it the way a user would.
- Every documentation snippet comes from **a real compiled run**.
- **No release, no tag**, ever, by anyone.
- `docs/gallery.md` is **generated** by `tools/gallery/main.cpp` and checked by `gallery.is-current`. Edit the tool; never hand-edit the page.
- **Do not hardcode `/std:c++23`.** The build requests the standard via `set(CMAKE_CXX_STANDARD 23)`, `CMAKE_CXX_STANDARD_REQUIRED ON` and `target_compile_features(... cxx_std_23)`. The spike found this machine's `cl` silently no-ops a literal `/std:c++23` and drops to pre-C++17, and `clang-cl` drops to C++14. CMake picks correctly and errors rather than downgrading; leave it alone.

## Interfaces that already exist — compiled, not guessed

The spike verified all of these on cl, clang-cl, clang++ and GCC.

| Fact | Evidence |
|---|---|
| **`Rational` cannot be an NTTP** — all four reject it, private members | spike Q1; `dimension.hpp`'s comment on `Exponent` already said so |
| **`Unit` works as an NTTP** on all four | spike Q1 |
| **`std::array<Band, N>` of plain-int64 structs works as an NTTP** on all four, alias-template deduction included | spike Q1 |
| **A node may hold runtime state behind a compile-time shape** | `ConstantNode` does exactly this; spike Q2 built and ran a `TableNode` generalising it |
| **`InvalidReason::label` is a non-owning `std::string_view`** and dangles for a generated message | spike Q3, pointer-identity demonstration |
| **No `InvalidReason` or `Verdict` is constructed anywhere in `include/`** — every label is an author's literal | spike Q3 |
| **No interval or binary-search facility exists.** This phase is first | spike Q4 |

The two shapes to copy:

```cpp
// expression.hpp:76 -- runtime state behind a compile-time shape
template <Unit U>
struct ConstantNode: NodeBase
{
    Rational number {};                                    // runtime
    static constexpr Unit unit = U;                        // in the type
    static constexpr Dimension dimension = U.dimension;    // in the type
};

// unit.hpp:114 -- the int64 numerator/denominator pair convention
struct Bounds
{
    bool present = false;
    std::int64_t lowNumerator = 0;
    std::int64_t lowDenominator = 0;
    // ... high pair ...
    [[nodiscard]] constexpr bool operator==(Bounds const&) const noexcept = default;
};
```

**What the spike did NOT establish, and task 2 must therefore specify rather than infer:** how compile-time band *boundaries* and runtime table *contents* combine on one node. Two verified pieces do not verify their join.

## File Structure

- **Create** `include/formula-cpp/band.hpp` — `Band`, band-array validation, the diagnostics a bad table produces.
- **Create** `include/formula-cpp/lookup.hpp` — the three lookup node kinds and their factories.
- **Create** `test/band_tests.cpp`, `test/lookup_tests.cpp`, and `test/negative/` cases per task.
- **Modify** `include/formula-cpp/render.hpp`, `trace.hpp`, `trace_render.hpp`, `document.hpp`, `formula.hpp` (umbrella), `CMakeLists.txt` (install `FILE_SET`), `test/CMakeLists.txt`.
- **Create** `docs/lookup-tables.md`, `examples/lookup_tables.cpp`; **modify** `mkdocs.yml`, `docs/index.md`, `README.md`, `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md`.

`hygiene.installed-headers` fails the moment a new header exists without being added to the install `FILE_SET`. Expect it; do not work around it.

---

## Task 1: Bands, and a table that has a gap or an overlap does not compile

**Files:** create `include/formula-cpp/band.hpp`, `test/band_tests.cpp`, `test/negative/band_gap.cpp`, `test/negative/band_overlap.cpp`; modify `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Produces: `Band`, `BandTable<N>` (or whatever the array alias is called — decide and say why), a `constexpr` validation predicate, and the `static_assert` wiring. Tasks 2–4 consume all of it.

**This is the task the spec emphasises**, and its own words are the requirement:

> Band tables must be validated for gaps and overlaps, because real published tables contain typos and undefined regions: **the loader must say so rather than silently mis-bucket.**

A table with bands 30–40 and 45–50 leaves 42 undefined. One with 30–40 and 38–45 gives two answers at 39. Both occur in real published documents, because people type them.

**Bands are int64 numerator/denominator pairs, not `Rational`** — the spike proved `Rational` cannot be an NTTP on any of the four compilers, and `dimension.hpp` already said so. Follow `Bounds`' shape.

**Bands are half-open `[low, high)`.** A published table writing "30 to 40" then "40 to 50" leaves 40 ambiguous, and every real table does this somewhere. Half-open resolves it one way; the guide must tell the caller to reconcile that against their source document rather than presenting it as an implementation detail. **Decide whether the last band needs a closed upper bound** — "up to and including the maximum" is a real thing tables say — and say which way you went and why.

**One predicate, used twice.** The same `constexpr` function must serve the `static_assert` and any runtime loader. Writing it twice guarantees they diverge; this project has watched two surfaces drift apart four times now.

- [ ] **Step 1: Write the failing tests.** A well-formed table validates. A gapped one does not. An overlapping one does not. A single-band table validates. An empty table — decide what that means and test it. Adjacent bands sharing an endpoint validate (that is the half-open case working).
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run to verify the tests pass.**
- [ ] **Step 5: The negative tests are the point of this task.** `test/negative/` asserts both that the build fails *and* that the message contains the library's own `static_assert` text — those strings are tested API. A gapped table and an overlapping table must each produce a message naming which band pair is at fault. "Invalid table" is not good enough: the author needs to know *where*.
- [ ] **Step 6: Mutation.** Make the validator accept a gap; confirm the negative test fails to fail. Restore. Then the same for overlap. Report both directions.
- [ ] **Step 7: All four presets and GCC, then commit.**

**Position matters here.** Put the gap between the *middle* two bands in at least one test, not only between the last two. A validator that only checks the final adjacency passes a last-position test and misses everything else.

---

## Task 2: The banded lookup node

**Files:** create `include/formula-cpp/lookup.hpp`, `test/lookup_tests.cpp`; modify `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: task 1's `Band` and validation; `ConstantNode`'s shape; `Node`, `NodeBase`, `Unit`, `Dimension`, `Rational`.
- Produces: the banded lookup node and its factory. Tasks 5–8 consume it.

**Specify the join the spike did not verify.** Compile-time bands and runtime contents on one node is this task's real work. State the shape explicitly in a comment: what is in the type, what is runtime state, and why each is where it is. `ConstantNode` is the precedent — `Rational number` runtime, `unit` and `dimension` in the type.

**A lookup that matches nothing has not found a value.** It has found nothing. There is **no** default-value parameter, no nearest-band fallback, no first-band fallback. That is phase 9's forbidden `bool satisfied()` in a new costume: an API that must answer something for the unresolved case, where every answer is a lie.

**The miss carries structured fields, not a sentence.** The spike proved `InvalidReason::label` is a non-owning `string_view` that dangles for a generated message, and that no label anywhere in `include/` is anything but an author's literal. So report the value, its unit, and the table's identity as fields, and let the renderer compose prose — exactly as `Step` carries `comparison` and `granularity` while `trace_render.hpp` turns them into text.

- [ ] **Step 1: Write the failing tests** — a value in the first band, in a middle band, in the last band, exactly on a boundary (the half-open case), below the lowest band, above the highest. The last two are misses and must be reported as such.
- [ ] **Step 2: Run to verify failure.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run to verify the tests pass.**
- [ ] **Step 5: Mutation** — make a miss return the nearest band's value instead of a miss. Confirm the below-range and above-range tests fail. Restore. Report both directions. **If either test still passes, that test is not testing the property.**
- [ ] **Step 6: All four presets and GCC, then commit.**

---

## Task 3: The exact lookup node

**Files:** modify `include/formula-cpp/lookup.hpp`, `test/lookup_tests.cpp`

A category key selects a row. The key is not a quantity — it is a discriminator such as a specimen shape or an apparatus variant.

**Decide how a key is spelled and justify it.** `detail::FixedString` works as an NTTP (phase 4) and enum keys are the obvious alternative. Consider which produces a better diagnostic when a key is absent, since that is the failure a method author will actually hit.

**A missing key is a miss**, handled exactly as task 2 handles an out-of-range value. The two must agree; do not invent a second vocabulary for the same situation.

- [ ] **Steps 1–6:** as task 2, with a test for a key that is present, one absent, and — since position is part of a test's meaning — a key in the first, middle and last row.

---

## Task 4: The interpolating lookup node

**Files:** modify `include/formula-cpp/lookup.hpp`, `test/lookup_tests.cpp`

A value between two rows produces a value appearing in no row.

**Exactness is the question this task must answer.** `Rational` is exact, and linear interpolation between two rationals is exact — so say plainly whether the result stays exact, and if any operation cannot, say which and why. Phase 8 added rounding precisely so a method can state where precision is lost; if interpolation loses it silently, that is a defect.

**Interpolating outside the table's range is not interpolation.** Decide whether it is a miss or an extrapolation, and if you allow extrapolation, it must be something the caller asks for explicitly rather than gets by default.

- [ ] **Steps 1–6:** as task 2, including a value exactly on a row (which must return that row, not interpolate), a value between rows, and a value outside the range.

---

## Task 5: Render the three lookup kinds

**Files:** modify `include/formula-cpp/render.hpp`, `test/render_tests.cpp`

A lookup is a `Node`, so it needs no separate entry point the way `Constraint` did — **verify that against `Node`'s actual requirements rather than assuming it**.

**Read the output as a reader, in every dialect, and report what you read.** This project has shipped or caught five rendering defects that were structurally well-formed, brackets balanced, string comparisons passing, and wrong only when read — including a `[...]` spelling CommonMark parsed as a link, which dropped the operand from a published page.

**Extend the existing Markdown guard** asserting no Markdown rendering contains `](` or a bare `[`. Do not write a second guard beside it.

**Do not introduce a fourth punctuation style.** The trace uses bracketed suffixes; `require` and `if` are keywords naming their own slots. A lookup's rendering must not resemble a call whose operand positions mean something other than a reader expects — that is the rule `when(#1, #2, #3)` taught this project the hard way.

- [ ] **Steps 1–5:** tests in three dialects, standalone and nested inside a product and a power; read aloud; mutation on the operand order.

---

## Task 6: Trace the three lookup kinds

**Files:** modify `include/formula-cpp/trace.hpp`, `include/formula-cpp/trace_render.hpp`, `test/trace_tests.cpp`, `test/trace_render_tests.cpp`

**A derivation must say which row was selected and why.** A trace step reading only "42" explains nothing; the reader needs to see that 42 came from the band containing the input. That is the whole point of §11.

**Check every new `StepKind` enumerator against the names in namespace `formula` on GCC.** Phase 7 shipped `StepKind::Pi`, which shadowed `formula::Pi`; all four Windows presets passed and GCC alone rejected it.

**A miss must render distinguishably from a value.** Phase 9's `[not checked]` versus `[else]` is the precedent — a reader must never confuse "no row matched" with "the matched row held zero".

- [ ] **Steps 1–6:** as the other tasks, plus a test comparing the rendered and traced spellings against each other rather than against literals, since that is the cross-surface drift phase 8 shipped and phase 9 closed only after a review found it.

---

## Task 7: Walk the three lookup kinds in `document()`

**Files:** modify `include/formula-cpp/document.hpp`, `test/document_tests.cpp`

`collect` is declared in two blocks — forward declarations and definitions. Add to both, matching the convention every existing overload follows. **Do not write a comment claiming the forward declarations are required for correctness**; they are not, ADL finds every overload regardless of order, and the existing comment says so accurately.

**Every test must reach the feature the way a user would** — through `formula::document(...)`, not `formula::detail::collect`. Phase 9 shipped a `collect` overload that worked, was tested through the detail path, and no public API could reach.

- [ ] **Steps 1–6:** as the other tasks, with each new overload named against the test covering it, and a mutation per overload.

---

## Task 8: Guide, example and gallery

**Files:** create `docs/lookup-tables.md`, `examples/lookup_tables.cpp`; modify `mkdocs.yml`, `docs/index.md`, `README.md`, `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md`

**The argument this guide must make with a worked case, not prose:** a table with a gap does not compile, and the message says where. Show the gapped table, show the diagnostic, and say why a library that accepted it would be worse than useless — it would produce a number for an input the method never defined.

**Show a miss, and show that it is not a number.** Same reason phase 9's guide had to show `NotChecked` rather than describe it.

**Say what this phase is not for.** §16.1's distinction belongs in the guide: this is the mechanism a method's own algebra uses, and the limit tables a product is judged against are master data that live elsewhere. A reader who misses that will try to store customer limits here.

**Write the guide early enough to act as a reachability probe.** Phase 9's most expensive defect was found by a guide author discovering the natural spelling did not compile. Do not leave that discovery until after five tasks have shipped.

- [ ] **Steps 1–6:** example compiled and run first; every guide block pasted from that run; gallery entry added to the tool and regenerated; nav and status tables updated; **the Doxygen target and `mkdocs build --strict` both run** — they are the only local exercise of the two CI legs that broke after phase 7.
