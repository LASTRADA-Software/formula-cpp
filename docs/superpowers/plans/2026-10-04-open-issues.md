# Open Issues Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close every issue open on 2026-10-04 (#11, #13, #14, #15, #16, #17, #18, #19, #20, #21) in one pull request.

**Architecture:** Three groups of independent changes.

- **Trace text (Lane A, Tasks 1–4).**
    - A scaled dimensionless unit without a symbol is refused where it is declared.
    - An inverse-only coherent unit is spelled with negative exponents (`kg^-1`).
    - A rounding clause names an unnamed unit by its size (`to 2 dp of 1/1000 kg`).
    - A precision limit's first pass borrows the unit of the step it restates.
    - The snap's on-a-value comparison is explained in a comment.
- **Numerics (Lane B, Tasks 5–6).**
    - The logarithm and exponential kernel takes 128-bit arguments.
    - The bit widths two guides quote are pinned: by a census hook in the fit kernel, and by a test of the opaque example's coefficients.
- **Final tasks (Tasks 7–10).** Merge Lane B, then:
    - `explain` and `checked_explain` refuse a series in the library's words;
    - two documentation corrections;
    - every internal development label is replaced by self-describing text;
    - the finish.

**Tech Stack:** C++23, header-only; Catch2 3.6 (`STATIC_REQUIRE`), the `test/negative/` harness, the CTest docs and census checks, Python 3 (the census generator; `decimal` and `fractions` for independent expected values).

**Spec:** `docs/superpowers/specs/2026-10-04-open-issues-design.md`. Read it before any task. Each task names the spec section it implements.

**Order and lanes:**

- **Lane A, trace text:**
    - Tasks 1 → 2 → 3 → 4, in `D:\formula-cpp` on branch `feature/open-issues`.
    - Task 3 uses Task 2's spelling.
- **Lane B, numerics:**
    - Tasks 5 → 6, each in its own agent-owned worktree.
    - Their commits go on the branch `feature/open-issues-numerics`. The controller creates it at the commit that adds this plan, and after each Lane B task moves it to that task's head (`git branch -f`).
    - Each Lane B task begins with `git reset --hard feature/open-issues-numerics` in its fresh worktree.
- **Shared files.** The lanes run at the same time. The only files they share are `CHANGELOG.md` and possibly a guide's trace sample.
- **After both lanes:**
    - Task 7 merges Lane B into `feature/open-issues`.
    - Then Tasks 8 → 9 → 10, in `D:\formula-cpp`.
    - Task 9 (#13) comes after the merge on purpose, so that it also catches any label either lane added.

**Line anchors** were read at `9178f3b`. Find each place by the name quoted beside its anchor, never by the number alone.

---

## Global Constraints

These bind every task.

- **C++23, header-only.** Nothing beyond the standard library in `include/`.
- **Worktrees:**
    - Lane A and Tasks 7–10 work in `D:\formula-cpp`.
    - A Lane B task works only in its own agent-owned worktree.
    - No task touches another lane's tree.
- **Gates run from PowerShell.** Bash's pipe mis-encodes `°` and fails `docs.*-output`.
    - `$S = C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\81d1061b-b25f-4c83-9f67-664a67264017\scratchpad`
    - `$T` = the task's tree.
- **Verify, the per-task gate.** Every command must print `ALL OK` / `MATRIX OK`:
    1. `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."`: the full cl-debug build, then every test except the negative ones.
    2. `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "<regex>"`, only for a task that adds or touches negative tests. It runs the negatives matching `<regex>` on cl-debug **and** clangcl-debug.
    3. Task 5 only, because `UInt128` computes natively only on GCC and Clang: `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad/posix-matrix.sh --tree <the worktree as a /mnt/d/... path> --presets "gcc-release clang-debug"`.
- **Quick loop:** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "<ctest -R regex>"`.
- **Run builds and tests in the foreground.**
    - Never wait on a background monitor of your own.
    - Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is a build error.
    - Prove from ctest's count that a filter selected something before trusting it.
    - Catch2 splits test filters on commas.
- **Baseline at `9178f3b`:** cl-debug passes 1569 of 1569 non-negative tests; the negatives pass 561 of 561 on cl-debug and clangcl-debug. Each task reports its total and the difference from the previous task in its lane, which must equal its stated delta.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:**
    - an SPDX header on every file, and no `NOLINT`;
    - core public headers include no `<string>`, `<vector>`, `<format>`, `<iostream>` or `<print>` (`hygiene.headers`);
    - every public `static_assert` message begins `formula: `;
    - a new public header goes into the install `FILE_SET` (`CMakeLists.txt`, `hygiene.installed-headers`) and into `test/consumer_globals_tests.cpp`'s includes (`hygiene.consumer-globals`).
- **Consumer globals.** `test/consumer_globals_tests.cpp` declares a list of `int` globals. No new parameter or local may reuse one: cl C4459 and g++ `-Wshadow` turn a reuse into a consumer's build error. Names on the list that this plan is tempted by:
    - `a`–`z`;
    - `value`, `values`, `result`, `sum`, `count`, `digits`, `numerator`, `denominator`, `quotient`, `sign`, `width`, `scale`, `scaled`, `factor`;
    - `low`, `high`, `lower`, `upper`, `limit`, `pattern`, `root`, `step`, `text`, `unit`, `kind`, `first`, `last`, `left`, `right`, `operand`, `number`, `total`, `range`, `rest`.

  Use descriptive names (`divisorWord`, `remainderSoFar`, `levelStep`). Member names are not affected.
- **Behaviour rule.**
    - Every computation that answers today gives the same answer afterwards.
    - The only refusals added are the ones the spec names: a scaled dimensionless unit without a symbol, and `explain` of a series. The latter is now refused in the library's words instead of by overload resolution.
    - Some `Overflow` refusals of the transcendental functions now answer. A refusal is never turned into a different number.
- **Display rule.** No number is shown in a unit other than the one written after it, and no rounding clause leaves its places' unit unnamed.
- **No internal labels in public text.**
    - Code, docs, commit messages and the PR never name tasks, lanes, plans, phases, spikes or reviewers, and never cite local progress notes.
    - Every sentence must make sense to a reader who never saw this plan.
    - Issue numbers may appear only in the PR body.
- **No third-party standard content.** Cite only `Example Standard N:YYYY`. Fixture values are plainly invented.
- **Style.**
    - Do not run clang-format on existing files; match the surrounding style by hand.
    - Every new public entity and member gets a Doxygen `///` comment.
- **Printing** is `std::print` / `std::println` only. Never `printf`, `puts` or iostream.
- **Error handling.** Check every `std::expected` result before use. Never unwrap unchecked, and never switch to a throwing form to shorten code.
- **Newest GCC only** (g++-14). No workaround for an older compiler.
- **Commits.** Conventional style (`feat(trace): …`, `fix: …`, `test: …`, `docs: …`). Every message ends with:

  ```
  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  Commit with `git commit -F <file>`; a here-string passed to `-F -` does not work in PowerShell.
- **CHANGELOG.md:**
    - Entries go under `## [Unreleased]` (create it at the top if absent, as `cmake/CheckChangelog.cmake` requires), in `### Added` / `### Changed` / `### Fixed` subsections.
    - A breaking change says so in its first words.

## Review Focus

The five inputs this spec implies but no obvious test reaches, most likely to bite first. Each one's test is in the task named.

1. **A dimensionless unit written at scale 1 in an unreduced form**, such as magnitude 7/7, with no symbol. It is at scale 1, so it must still compile. The refusal compares reduced fractions, never raw pairs. Task 1.
2. **An inverse-only unit with a fractional exponent or a named base:**
    - `kg^(-1/2)` and `JPY^-1`;
    - the mixed `EUR/JPY` and `m/s`, unchanged.
   The walker must accept every form the spelling can produce. Task 2.
3. **An unnamed unit whose magnitude is a whole number or exactly 1:**
    - a 1000 kg unit with no symbol rounds `to 2 dp of 1000 kg`;
    - a unit of magnitude 1 with no symbol, dimensioned, rounds `to 2 dp of 1 kg`. It is never bare, and never `of kg`.
   Task 3.
4. **A precision level that cannot borrow:**
    - a level computed by an expression whose last step holds a different value;
    - a constant level in an offset unit (°C).
   The first stays in the fallback unit. The second follows `borrowable_for_a_point`, and must not read as a difference. Task 4.
5. **The kernel at its new edges:**
    - exp of exactly 887/10. It passes the cap, and e^88.7 does not fit `Int128`, so the rounding's narrowing refuses it with `Overflow`;
    - exp of a negative argument with a 127-bit denominator, which rounds to 1;
    - ln of a 127-bit numerator over a 127-bit denominator with a ratio below 1.
   The native (g++, clang++) and portable (cl, clang-cl) paths give the same bits. Task 5.

## Execution

- **Briefs and reports:**
    - The controller writes each task's brief to `D:\formula-cpp\.superpowers\sdd\2026-10-04-open-issues\task-N-brief.md` (`.superpowers/` is git-ignored).
    - The implementer writes its report beside it, as `task-N-report.md`.
    - A Lane B implementer that cannot write outside its worktree writes the report to `.superpowers/task-N-report.md` inside its worktree and says so in its reply.
    - A report states:
        - the commits;
        - the test total and its delta;
        - each rule in *Global Constraints* the task touched, and how it was kept;
        - anything it could not do.
- **Agents.** Every task is implemented by `sdd-implementer` and reviewed by `sdd-reviewer`. A fix round goes back to `sdd-implementer`. Task 7's merge is run by the controller; an `sdd-implementer` resolves any conflict that is not mechanical.
- **Negative tests**, where a task adds one:
    1. Add `test/negative/<name>.cpp`, plus `formula_add_negative_test(<name> "<expected>" …)` in `test/CMakeLists.txt`.
    2. Register it first with a deliberately wrong expected text, and watch it fail.
    3. Register the right text, and watch it pass.
    4. Delete the guard it pins, confirm the case compiles, then restore the guard with a plain write.

---
## Lane A — trace text (#14, #17, #15, #16, #18)

Lane A works in `D:\formula-cpp` on branch `feature/open-issues`, Tasks 1 → 2 → 3 → 4, in that order. Task 3 uses
the spelling Task 2 writes; Tasks 1 and 4 are independent of the other two but run in the same tree, so the lane is
one sequence.

Line anchors were read at `9178f3b`. Find each place by the name quoted beside its anchor, never by the number
alone: every task in this lane inserts lines above the anchors of the next.

Throughout the lane, `$T = D:\formula-cpp`.

---

### Task 1: A scaled dimensionless unit must have a symbol (#14)

A dimensionless unit whose magnitude is not 1, or whose offset is not 0, and that has no symbol, cannot be written
truthfully anywhere: a trace shows one half in hundredths as `50`. It is refused wherever a `Unit` enters the
library. Spec §2.1.

The refusal is one class template, `detail::RequireNamedScaledScalar<Unit U>`, beside `RequireSameUnitDimension` in
`unit.hpp`. Every class template that holds a unit as a template argument asserts it in its body, next to the checks
it already makes. The new check is a property of the unit alone, so it is never gated behind another check: it
cannot repeat another check's message for the same mistake.

A quantity's unit is asserted where the four existing `DescribesConsistentDimension` checks already sit:
`RequireDescribed` (which `Measured<Q>` asserts), `VarNode`, `ObservationsVarNode` and `SeriesVarNode`. A program that
both measures and reads such a quantity is told once per site, exactly as it is told today about a quantity whose
declared dimension disagrees with its unit. The negative test for the quantity case uses `var<Q>` alone, so it sees
one message.

**Files:**
- Modify: `include/formula-cpp/unit.hpp`: a new `namespace detail` block after `RequireSameUnitDimension`
  (`:530-548`), holding `unnamed_scaled_scalar` and `RequireNamedScaledScalar`.
- Modify, one `static_assert` line each, in the class body beside the existing asserts:
    - `include/formula-cpp/quantity.hpp`: `RequireDescribed` (`:233-256`), gated on `Described<T>` through a new
      `detail::RequireDescribedUnitNamesItsScale`;
    - `include/formula-cpp/expression.hpp`: `VarNode` (`:46-58`), `ConstantNode` (`:75-84`);
    - `include/formula-cpp/observations.hpp`: `ObservationsVarNode` (`:50-60`);
    - `include/formula-cpp/series.hpp`: `SeriesVarNode` (`:60-72`), `SeriesConstantNode` (`:175-180`),
      `ElementwiseRoundNode` (`:481-495`);
    - `include/formula-cpp/rounding_node.hpp`: `RoundNode` (`:48-51`), `RoundSignificantNode` (`:74-77`);
    - `include/formula-cpp/rounded_root.hpp`: `RoundedRootNode` (`:279-284`);
    - `include/formula-cpp/opaque.hpp`: `RoundedOpaqueOutputNode` (`:1022-1027`);
    - `include/formula-cpp/lookup.hpp`: `BandedLookupNode` (`:765-770`, key and result), `ExactLookupNode`
      (`:1171-1176`, result), `InterpolatingLookupNode` (`:1805-1810`, key and result);
    - `include/formula-cpp/snap.hpp`: `SnapNode` (`:160-176`, key);
    - `include/formula-cpp/curve.hpp`: `DomainNode` (`:106-111`);
    - `include/formula-cpp/binning.hpp`: `BinnedNode` (`:129-144`, key);
    - `include/formula-cpp/critical_value.hpp`: `SampleSizeLookupNode` (`:376-383`, result);
    - `include/formula-cpp/escape.hpp`: `NumericValueNode` (`:82-91`);
    - `include/formula-cpp/conformity.hpp`: `Conformity` (`:343-350`);
    - `include/formula-cpp/method.hpp`: `RoundingRule` (`:1081-1083`);
    - `include/formula-cpp/overlay.hpp`: `RoundingOverride` (`:696-698`). Its `using rule = RoundingRule<…>` names
      the rule without instantiating it, so it asserts on its own.
- Modify: `include/formula-cpp/trace_render.hpp`: the comments of `spells_coherent_unit` (`:651-665`) and
  `shown_unit_text` (`:675-682`).
- Create: `test/negative/scaled_scalar_unit_without_symbol_var.cpp`,
  `test/negative/scaled_scalar_unit_without_symbol_constant.cpp`,
  `test/negative/scaled_scalar_unit_without_symbol_snap.cpp`.
- Modify: `test/CMakeLists.txt`: three `formula_add_negative_test` lines after `unit_currency_mismatch` (`:338-339`).
- Modify: `test/unit_tests.cpp`: one new `TEST_CASE` at the end of the file.
- Modify: `docs/dimensions.md` (the `symbolText` row of the field table, `:133`, and the paragraph after it),
  `docs/tracing.md` (`:377-380`), `CHANGELOG.md`.

**Interfaces:**
- Consumes: `Unit` (`unit.hpp:67`), `dim::Scalar`, `view(Symbol const&) -> std::string_view` (`dimension.hpp:256`),
  `Described<T>`, `Describe<T>::unit`.
- Produces:
    - `formula::detail::unnamed_scaled_scalar(Unit const&) noexcept -> bool` (constexpr): true for a dimensionless
      unit with no symbol whose magnitude is not 1 or whose offset is not 0.
    - `formula::detail::RequireNamedScaledScalar<Unit U>`: `::value` is `true`; completing it with such a unit
      fails with the message below.
    - The message, which later tasks and the CHANGELOG quote: `formula: a dimensionless unit with a scale must have a
      symbol (for example "%"), or the quantity must be declared in scale 1; the unit appears in this diagnostic as the
      template argument of RequireNamedScaledScalar`.
    - After this task, a dimensionless unit with no symbol that reaches a trace or a `render()` has scale 1. Task 3
      relies on it.

- [ ] **Step 1: Write the failing positive test.** Append to `test/unit_tests.cpp` (its namespace aliases are
  `formula::Unit` as `Unit`, `formula::dim` as `dim`, and `formula::unit` as `unit` — check the file's top and keep its
  spelling):

```cpp
TEST_CASE("a dimensionless unit with a scale and no symbol is the one a declaration refuses", "[unit]")
{
    // Hundredths with no symbol: one half would read 50, in a scale nothing names.
    constexpr Unit unlabelledHundredth { .dimension = dim::Scalar, .magnitudeNumerator = 1, .magnitudeDenominator = 100 };
    // The same scale with an offset only.
    constexpr Unit unlabelledShifted { .dimension = dim::Scalar, .offsetNumerator = 1, .offsetDenominator = 2 };
    // Scale 1 written as 7/7: still scale 1.
    constexpr Unit unlabelledSevenSevenths { .dimension = dim::Scalar, .magnitudeNumerator = 7, .magnitudeDenominator = 7 };
    // A dimensioned unit with no symbol is shown in the coherent unit instead, and is not refused.
    constexpr Unit unlabelledGram { .dimension = dim::Mass, .magnitudeNumerator = 1, .magnitudeDenominator = 1000 };

    STATIC_REQUIRE(formula::detail::unnamed_scaled_scalar(unlabelledHundredth));
    STATIC_REQUIRE(formula::detail::unnamed_scaled_scalar(unlabelledShifted));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unlabelledSevenSevenths));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unlabelledGram));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::One));
    // Every shipped scaled dimensionless unit has a symbol.
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::Percent));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::PerMille));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::PartsPerMillion));
    STATIC_REQUIRE(!formula::detail::unnamed_scaled_scalar(unit::MilligramPerKilogram));
    STATIC_REQUIRE(formula::detail::RequireNamedScaledScalar<unit::Percent>::value);
    STATIC_REQUIRE(formula::detail::RequireNamedScaledScalar<unit::One>::value);
}
```

- [ ] **Step 2: Run it to see it fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "refuses"`.
  Expected: BUILD FAILED, `unnamed_scaled_scalar` is not a member of `formula::detail`. A `STATIC_REQUIRE` failure is
  a build error, so read the build output, not ctest's.

- [ ] **Step 3: Add the predicate and the check.** In `include/formula-cpp/unit.hpp`, immediately after
  `RequireSameUnitDimension`'s closing `};` (`:548`), insert:

```cpp
namespace detail
{
    /// Whether @p candidate is a dimensionless unit with a scale and no symbol: a magnitude other than 1, or an
    /// offset other than 0, compared as fractions, and an empty symbol. A number in such a unit is in a scale
    /// nothing on its line can name -- one half in hundredths would read `50` -- and no spelling of the unit
    /// itself can name it either, since a dimensionless coherent unit is written as nothing. A dimensioned unit
    /// with no symbol is not one: its value is shown in the coherent unit, which its dimension spells.
    [[nodiscard]] constexpr bool unnamed_scaled_scalar(Unit const& candidate) noexcept
    {
        return candidate.dimension == dim::Scalar && view(candidate.symbolText).empty()
               && (candidate.magnitudeNumerator != candidate.magnitudeDenominator || candidate.offsetNumerator != 0);
    }

    /// Fails to compile when @p U is a dimensionless unit with a scale and no symbol (`unnamed_scaled_scalar`).
    /// Asserted in the class body of everything that holds a unit as a template argument -- a quantity's
    /// description, a variable, a constant, a rounding, a key or a result of a table, a conformity check -- so
    /// that such a unit is refused where it is written, never shown as a bare number in its scale.
    ///
    /// Same shape as `RequireSameUnitDimension` above, and the same caveat: it fires only when the type is
    /// completed, so write `::value`.
    template <Unit U>
    struct RequireNamedScaledScalar
    {
        static_assert(!unnamed_scaled_scalar(U),
                      "formula: a dimensionless unit with a scale must have a symbol (for example \"%\"), or the "
                      "quantity must be declared in scale 1; the unit appears in this diagnostic as the template "
                      "argument of RequireNamedScaledScalar");

        /// Always `true` once reached -- the `static_assert` above already failed compilation otherwise.
        static constexpr bool value = true;
    };
} // namespace detail
```

- [ ] **Step 4: Run the positive test to see it pass.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "refuses"`.
  Expected: `ALL OK`, 1 test.

- [ ] **Step 5: Write the three negative tests.** Create `test/negative/scaled_scalar_unit_without_symbol_var.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
//
// A quantity declared in hundredths with no symbol: one half would be shown as
// 50, a number in a scale no line names. Refused where the quantity is read,
// once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

struct Fraction: formula::Quantity<Fraction, "w", "an invented fraction", Hundredth>
{
};

inline constexpr auto fraction = formula::var<Fraction>;

int main()
{
    return fraction.dimension == formula::dim::Scalar ? 0 : 1;
}
```

  Create `test/negative/scaled_scalar_unit_without_symbol_constant.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
//
// A coefficient stated in hundredths with no symbol: refused where it is
// written, once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

inline constexpr auto half = formula::constant<Hundredth>(formula::Rational { 50 });

int main()
{
    return half.number == formula::Rational { 50 } ? 0 : 1;
}
```

  Create `test/negative/scaled_scalar_unit_without_symbol_snap.cpp`. The operand is in `unit::Percent`, which has a
  symbol, so the snap's key is the only unit refused:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
// REJECT: key unit does not measure
// REJECT: breakpoints do not strictly ascend
//
// A snap keyed in hundredths with no symbol: its permitted values would be
// written as numbers in a scale no line names. Refused where the snap is
// written, once, and by nothing else: the key measures what the operand does,
// and the permitted set ascends.
#include <formula-cpp/snap.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

struct Share: formula::Quantity<Share, "s", "an invented share", formula::unit::Percent>
{
};

inline constexpr formula::BreakpointTable<2> permitted { formula::breakpoint(25), formula::breakpoint(50) };

inline constexpr auto snap =
    formula::snapped<Hundredth, permitted, formula::SnapTie::TowardLower>(formula::var<Share>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
```

- [ ] **Step 6: Register them with a wrong text and watch them fail.** In `test/CMakeLists.txt`, after the
  `unit_currency_mismatch` registration (`:338-339`), add:

```cmake
# A dimensionless unit with a scale and no symbol: refused where a quantity
# declared in it is read, where a constant is stated in it, and where a table
# is keyed in it -- once each.
formula_add_negative_test(scaled_scalar_unit_without_symbol_var
    "formula: a dimensionless unit with a scale must be WRONG" EXPECT_COUNT 1)
formula_add_negative_test(scaled_scalar_unit_without_symbol_constant
    "formula: a dimensionless unit with a scale must be WRONG" EXPECT_COUNT 1)
formula_add_negative_test(scaled_scalar_unit_without_symbol_snap
    "formula: a dimensionless unit with a scale must be WRONG" EXPECT_COUNT 1
    REJECT "key unit does not measure" "breakpoints do not strictly ascend")
```

  Run: `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "scaled_scalar_unit_without_symbol"`.
  Expected: all three FAIL on both presets (the text is not found; before Step 7 the cases may also compile).

- [ ] **Step 7: Assert the check at every site.** Add the lines below, each directly after the last existing
  `static_assert` of the class body named (or as the body's first line where it has none). Keep each line's
  surroundings untouched.

  `quantity.hpp`, in `namespace detail` just above `RequireDescribed` (inside the existing `namespace formula`; open
  a `namespace detail { … }` block of its own there):

```cpp
namespace detail
{
    /// `RequireNamedScaledScalar` of a described type's unit, asked only once the type is described: an
    /// undescribed type has no unit to ask about, and is already refused, in full, by `RequireDescribed`.
    template <typename T, bool IsDescribed = Described<T>>
    struct RequireDescribedUnitNamesItsScale: std::true_type
    {
    };

    template <typename T>
    struct RequireDescribedUnitNamesItsScale<T, true>: RequireNamedScaledScalar<Describe<T>::unit>
    {
    };
} // namespace detail
```

  and in `RequireDescribed`, after its second `static_assert` (the dimension one, `:244-248`):

```cpp
    static_assert(detail::RequireDescribedUnitNamesItsScale<T>::value);
```

  If `quantity.hpp` does not already include `<type_traits>`, add it to its standard includes.

  `expression.hpp`, `VarNode`, after its `DescribesConsistentDimension` assert:

```cpp
    static_assert(detail::RequireNamedScaledScalar<Describe<Q>::unit>::value);
```

  `observations.hpp`, `ObservationsVarNode`, and `series.hpp`, `SeriesVarNode`, the same line after their
  `DescribesConsistentDimension` asserts.

  `expression.hpp`, `ConstantNode` (first line of the body), `series.hpp` `SeriesConstantNode` (after its `N > 0`
  assert), `series.hpp` `ElementwiseRoundNode`, `rounding_node.hpp` `RoundNode` and `RoundSignificantNode`,
  `rounded_root.hpp` `RoundedRootNode`, `opaque.hpp` `RoundedOpaqueOutputNode`, `curve.hpp` `DomainNode`,
  `escape.hpp` `NumericValueNode`, `conformity.hpp` `Conformity`, `method.hpp` `RoundingRule` (first line, before
  `public:`), `overlay.hpp` `RoundingOverride` (first line):

```cpp
    static_assert(detail::RequireNamedScaledScalar<U>::value);
```

  `lookup.hpp` `BandedLookupNode` and `InterpolatingLookupNode`:

```cpp
    static_assert(detail::RequireNamedScaledScalar<KeyUnit>::value);
    static_assert(detail::RequireNamedScaledScalar<ResultUnit>::value);
```

  `lookup.hpp` `ExactLookupNode` and `critical_value.hpp` `SampleSizeLookupNode`:

```cpp
    static_assert(detail::RequireNamedScaledScalar<ResultUnit>::value);
```

  `snap.hpp` `SnapNode` and `binning.hpp` `BinnedNode`:

```cpp
    static_assert(detail::RequireNamedScaledScalar<KeyUnit>::value);
```

  In a header outside `namespace formula::detail`, write `detail::` as above; inside one, drop the prefix as the
  neighbouring asserts do. Every one of these headers includes `unit.hpp` directly or through `expression.hpp`; if a
  build reports `RequireNamedScaledScalar` undeclared, add `#include <formula-cpp/unit.hpp>`.

- [ ] **Step 8: Register the right text and watch the negatives pass.** Replace `must be WRONG` with
  `must have a symbol` in the three registrations of Step 6.
  Run: `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "scaled_scalar_unit_without_symbol"`.
  Expected: `ALL OK`, three tests on each preset; on clangcl-debug each found the text exactly once.

- [ ] **Step 9: Prove each guard is what refuses.** One at a time, delete the `static_assert` line each case
  depends on (`VarNode`'s, `ConstantNode`'s, `SnapNode`'s), confirm with the quick loop that the case then compiles
  (`neg.ps1 … -Filter "scaled_scalar_unit_without_symbol_var"` reports it built), and restore the line with a plain
  write. Record the three results in the report.

- [ ] **Step 10: Update the comments in `trace_render.hpp`.** In `spells_coherent_unit`'s comment (`:651-661`),
  after "…so that every number on a line is in the unit written after it.", add:

```cpp
    /// A dimensionless unit with no symbol is always at scale 1 here:
    /// one with a scale is refused where it is written
    /// (`RequireNamedScaledScalar`, `unit.hpp`), so its bare number is the
    /// value.
```

  In `shown_unit_text`'s comment (`:675-677`), change "or nothing for a dimensionless value in a unit with no symbol."
  to "or nothing for a dimensionless value in a unit with no symbol, which is at scale 1."

- [ ] **Step 11: Document the rule.**
    - `docs/dimensions.md`, the field table's `symbolText` row (`:133`): change its purpose cell to
      "a fixed-capacity display symbol (a `Symbol`, not a `std::string_view`); required for a dimensionless unit with a
      scale".
    - `docs/dimensions.md`, after the paragraph that ends "…via `formula::view()` and the conversion functions below."
      (`:143`), add the paragraph:

      > A dimensionless unit with a scale or an offset must have a symbol. One half in hundredths with no symbol would
      > be shown as `50`, a number in a scale nothing names, and no spelling of the unit could name it: the coherent
      > dimensionless unit is written as nothing. Such a unit is refused wherever it is written -- as a quantity's
      > unit, a constant's, a rounding's, or a table's key or result -- with `formula: a dimensionless unit with a
      > scale must have a symbol`. A dimensioned unit may have no symbol: its values are shown in the coherent unit,
      > which its dimension spells.

    - `docs/tracing.md:377-380`: after "…since its number alone could not say what scale it is on." add the sentence
      "A dimensionless unit with a scale must have a symbol, so a bare number is always a value at scale 1."

- [ ] **Step 12: Write the CHANGELOG entry.** Under `## [Unreleased]` → `### Changed`, append:

```markdown
- **A dimensionless unit with a scale or an offset and no symbol is refused at compile time**, wherever it is
  written: as a quantity's unit, a constant's, a rounding's, or a table's key or result. A trace showed a value in
  such a unit as a bare number in a scale nothing named (one half in hundredths read `50`), and no spelling of the
  unit could name it. This breaks code that declares one: give the unit a symbol (`%`, `ppm`, or the author's own),
  or declare the quantity in scale 1. The refusal reads `formula: a dimensionless unit with a scale must have a
  symbol`. A dimensioned unit with no symbol is still accepted, and shown in the coherent unit.
```

- [ ] **Step 13: Verify.**
    1. `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."` → `ALL OK`.
    2. `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "scaled_scalar_unit_without_symbol|rounding|snap|lookup|unit_|quantity|constant|conformity|escape|binning|curve|critical|rounded|overlay|method|series|observations"`
       → `ALL OK`. The asserts added at Step 7 must not add a second message to any existing negative case:
       several assert `EXPECT_COUNT`.
  If `hygiene.documented-diagnostics` fails, a guide quotes a diagnostic at a header line that Step 7 moved: update
  that guide's quoted line number to the line the header's `static_assert` is on now, and nothing else in the quote.
  Expected count: **+1** non-negative test (1569 → 1570), **+3**
  negative tests.

- [ ] **Step 14: Commit.** Write the message to `$S\task-1-commit.txt`:

```
feat: refuse a dimensionless unit with a scale and no symbol

A number in such a unit is in a scale nothing names: a trace showed one
half in hundredths as 50, and no spelling of the unit could say so,
since the coherent dimensionless unit is written as nothing. Every class
that holds a unit as a template argument now refuses one, beside the
checks it already makes: a quantity's description and the three nodes
that read a quantity, a constant, every rounding, every table's key and
result, a numeric value, a conformity check, a rounding rule and its
override. A dimensioned unit with no symbol is still accepted and shown
in the coherent unit.

This breaks code that declares such a unit: give it a symbol, or declare
the quantity in scale 1.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Run: `git -C $T add -A include test docs CHANGELOG.md` then `git -C $T commit -F $S\task-1-commit.txt`.

---

### Task 2: Negative exponents for an inverse-only coherent unit (#17)

A coherent unit with nothing above the slash is spelt `1/kg` today, so in the fraction style a line reads
`20000/413 1/kg`, which reads as one fraction divided again. It becomes `kg^-1`. Units with a numerator keep the
slash. Spec §2.3.

**Files:**
- Modify: `include/formula-cpp/trace_render.hpp`: `coherent_unit_text` (`:595-650`), its body and its comment.
- Modify (re-pin):
    - `test/opaque_tests.cpp:1052` (`"1/s"` → `"s^-1"`), `:1066` (`"1/JPY"` → `"JPY^-1"`), `:1070`
      (`"1/(EUR s)"` → `"EUR^-1 s^-1"`), `:1204` (comment, `12700/103 1/m` → `12700/103 m^-1`), `:1212`
      (`"4. quotient of #3 = 12700/103 1/m\n"` → `"4. quotient of #3 = 12700/103 m^-1\n"`);
    - `test/trace_render_tests.cpp:2255`: `"3. #1 / #2 = 1500/137 1/kg; 500/71 1/kg; 1500/293 1/kg\n"` →
      `"3. #1 / #2 = 1500/137 kg^-1; 500/71 kg^-1; 1500/293 kg^-1\n"`;
    - `test/trace_shown_unit_tests.cpp:387` (comment, `the coherent unit, 1/kg.` → `the coherent unit, kg^-1.`),
      `:391` (`"3. #1 / #2 = 20000/413 1/kg\n"` → `"3. #1 / #2 = 20000/413 kg^-1\n"`);
    - `docs/dimensions.md:454` (`then \`1/JPY\`` → `then \`JPY^-1\``).
  Found with `git grep -nE ' 1/[a-zA-Z(]|"1/[a-zA-Z(]|\`1/[A-Za-z(]' -- test examples docs/*.md README.md include`
  at `9178f3b`. Re-run that search after Step 4; any hit that is a unit spelling (not a number such as
  `1/(2^63 - 1)`) is re-pinned the same way.
- Modify: `test/opaque_tests.cpp`: one new `TEST_CASE` after `"a named base dimension is spelt by its name and ahead
  of the SI units on its side"`.
- Modify: `test/trace_shown_unit_tests.cpp`: one line in `"every value a trace shows is in the unit written after
  it"` (`:683-711`).
- Modify: `docs/tracing.md:371-375`, `CHANGELOG.md`.

**Interfaces:**
- Consumes: `Dimension`, `Exponent { numerator, denominator }`, `detail::named_base_in_use` (`dimension.hpp:426`),
  `escaped_author_text` (`trace_render.hpp:185`).
- Produces: `detail::coherent_unit_text(Dimension) -> std::string` with the new spelling for a dimension that has no
  positive exponent. Its signature is unchanged. Task 3 moves its body into `render.hpp` and keeps this spelling.
- The whole-trace walker (`check_each_value_is_in_the_unit_written_after_it`, `trace_shown_unit_tests.cpp:273`)
  compares a line's unit text with `coherent_unit_text(dimension)`, so it accepts `kg^-1` and `kg^(-1/2)` without a
  change of its own.

- [ ] **Step 1: Write the failing test.** In `test/opaque_tests.cpp`, after the test case `"a named base dimension
  is spelt by its name and ahead of the SI units on its side"`, add:

```cpp
TEST_CASE("an inverse-only coherent unit is spelt with negative exponents", "[opaque][trace]")
{
    // `1/kg` after a fraction reads as the fraction divided again: `20000/413 1/kg`.
    CHECK(formula::detail::coherent_unit_text(formula::dim::Scalar / formula::dim::Mass) == "kg^-1");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Frequency) == "s^-1");
    CHECK(formula::detail::coherent_unit_text(formula::power(formula::dim::Length, -3)) == "m^-3");
    CHECK(formula::detail::coherent_unit_text(formula::dim::Scalar / (formula::dim::Length * formula::dim::Time))
          == "m^-1 s^-1");
    CHECK(formula::detail::coherent_unit_text(formula::nth_root(formula::dim::Scalar / formula::dim::Mass, 2))
          == "kg^(-1/2)");
    constexpr formula::Dimension yen = formula::base_dimension("JPY");
    CHECK(formula::detail::coherent_unit_text(formula::power(yen, -1)) == "JPY^-1");
    // A unit with a numerator keeps its slash.
    CHECK(formula::detail::coherent_unit_text(formula::dim::Velocity) == "m/s");
    CHECK(formula::detail::coherent_unit_text(formula::base_dimension("EUR") / yen) == "EUR/JPY");
}
```

  In `test/trace_shown_unit_tests.cpp`, in `"every value a trace shows is in the unit written after it"`, after the
  `var<UnnamedMass> * Rational { 2 } - var<TareMass>` line (`:710`), add:

```cpp
    // A pure number over a mass: kg^-1 after a fraction.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(Rational { 2 } / var<SampleMass>, inputs));
```

- [ ] **Step 2: Run it to see it fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "negative exponents"`.
  Expected: 1 test, FAILED, `"1/kg" == "kg^-1"` among the expansions.

- [ ] **Step 3: Rewrite `coherent_unit_text`.** Replace the function's body from `std::string above;` to the end
  with:

```cpp
        std::string above;
        std::string below;
        std::string inverse;
        std::size_t belowCount = 0;
        auto const place = [&](std::string_view symbolText, Exponent baseExponent) {
            if (baseExponent.numerator > 0)
                above += (above.empty() ? "" : " ")
                         + unitPower(symbolText, baseExponent.numerator, baseExponent.denominator);
            else if (baseExponent.numerator < 0)
            {
                below += (below.empty() ? "" : " ")
                         + unitPower(symbolText, -baseExponent.numerator, baseExponent.denominator);
                inverse += (inverse.empty() ? "" : " ")
                           + unitPower(symbolText, baseExponent.numerator, baseExponent.denominator);
                ++belowCount;
            }
        };
        for (std::size_t slot = 0; named_base_in_use(dimension, slot); ++slot)
            place(escaped_author_text(view(dimension.namedBases[slot].name)), dimension.namedBases[slot].exponent);
        for (BaseUnit const& base: bases)
            place(base.symbol, base.exponent);
        if (below.empty())
            return above;
        if (above.empty())
            return inverse;
        return above + "/" + (belowCount > 1 ? "(" + below + ")" : below);
```

  `unitPower` is unchanged: given a negative numerator it already writes `^-1` for -1/1 and `^(-1/2)` for -1/2.

  In the function's comment, change "`EUR s^2/(m^2 kg)` for euros per joule, `1/JPY`, `EUR/JPY`, `EUR^(1/2)`." to
  "`EUR s^2/(m^2 kg)` for euros per joule, `JPY^-1`, `EUR/JPY`, `EUR^(1/2)`.", and add a paragraph after the one that
  ends "…not as seconds squared of money per metre.":

```cpp
    ///
    /// A dimension with no positive exponent is written with negative
    /// exponents and no slash: `kg^-1`, `m^-1 s^-1`, `kg^(-1/2)`. After a
    /// number in the fraction style, `20000/413 1/kg` would read as one
    /// fraction divided again; `20000/413 kg^-1` cannot.
```

- [ ] **Step 4: Re-pin the texts that change.** Make the edits listed under **Files: Modify (re-pin)**, then re-run
  the search given there.

- [ ] **Step 5: Run the tests to see them pass.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "negative exponents|spelt|quotient never borrows|shown|trace_render"`
  (adjust to ctest names; prove the count is non-zero).
  Expected: `ALL OK`.

- [ ] **Step 6: Document it.**
    - `docs/tracing.md:371-375`: after "`kg/(m s^2)` for a pressure," insert "`s^-1` for a frequency -- a unit with
      nothing above the slash is written with negative exponents, so that `20000/413 kg^-1` cannot read as a fraction
      divided again --".
    - `CHANGELOG.md`, under `## [Unreleased]` → `### Changed`, append:

```markdown
- A coherent unit with no positive exponent is spelt with negative exponents in a trace: `kg^-1`, `s^-1`,
  `m^-1 s^-1`, `JPY^-1`, where it was `1/kg`, `1/s`, `1/(m s)`, `1/JPY`. After a number in the fraction style,
  `20000/413 1/kg` read as a fraction divided again. A unit with a numerator keeps its slash: `m/s`, `EUR/JPY`.
  A trace text pinned in a test changes where it showed such a unit.
```

- [ ] **Step 7: Verify.** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."` → `ALL OK`.
  `docs.*-output` and `docs.gallery` are in that run: if one fails on a `1/` unit spelling, the guide or the
  regenerated gallery is re-pinned as in Step 4, and the report names it.
  Expected count: **+1** non-negative test, **0** negative.

- [ ] **Step 8: Commit.** Message in `$S\task-2-commit.txt`:

```
fix(trace): spell an inverse-only coherent unit with negative exponents

A coherent unit with nothing above the slash was written 1/kg, so after
a fraction a line read 20000/413 1/kg: one fraction divided again, at a
glance. It is now kg^-1, m^-1 s^-1, JPY^-1, kg^(-1/2). A unit with a
numerator keeps its slash: m/s, EUR s^2/(m^2 kg).

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Run: `git -C $T add -A include test docs CHANGELOG.md` then `git -C $T commit -F $S\task-2-commit.txt`.

---

### Task 3: A rounding clause, a constant and a numeric value name an unnamed unit by its size (#15)

`round(#1, to 2 dp)` writes no unit clause when the rounding's unit has no symbol, while the value after it reads in
the coherent unit: "2 dp" then reads as places of a kilogram. The clause now names such a unit by its size in the
coherent unit: `round(#1, to 2 dp of 1/1000 kg) = 3/1000 kg`. An offset unit is named by its size and its zero:
`to 1 dp of 1 K from 27315/100 K`. Spec §2.2.

**Where the helper lives.** `render()` must write the same clause, and `trace_render.hpp` includes `render.hpp`, not
the other way round. So the spelling moves into `render.hpp`'s `detail` namespace, beside `unit_clause` and
`latex_unit` (`:452-476`), and takes how author text is written as a parameter: `render()` writes a symbol verbatim
in plain text and Markdown and escapes the whole unit text for LaTeX afterwards (`latex_unit`), while a trace escapes
each piece of author text (`escaped_author_text`). `render.hpp` already includes `<string>`; it is not one of the
core headers `hygiene.headers` keeps free of it. `coherent_unit_text(Dimension)` stays in `trace_render.hpp` with its
signature, as a one-line wrapper, so its callers and its tests do not change.

**Offset units are reachable.** `RoundNode` and `RoundSignificantNode` accept a unit with an offset (only the
dimension is checked, `RequireRoundingUnitMatches`, `rounding_node.hpp:35-44`); `RoundedRootNode` refuses one
(`RequireRootUnitWithoutOffset`, `rounded_root.hpp:79-89`), and so does `RoundedOpaqueOutputNode`
(`RequireRoundedOutputUnitWithoutOffset`, `opaque.hpp:939`). So the offset branch is tested, through `rounded<>`.

**Every rounding clause.** In the trace: `StepKind::Round` (`trace_render.hpp:1162-1163`), `RoundSignificant`
(`:1164-1166`), `RoundingRuleApplied` (`:1169-1170`, its `, in <unit>`), `ElementwiseRound` (`:1244-1246`),
`RoundedRoot` (`:1273-1275`), `RoundedOpaqueOutput` (`:1339-1342`) and `rounded_opaque_output_line` (`:3005-3008`).
In `render.hpp`: `RoundNode` (`:1330-1336`), `RoundSignificantNode` (`:1343-1356`), `RoundedRootNode` (`:1367-1377`),
`ElementwiseRoundNode` (`:1070-1087`) and `RoundedOpaqueOutputNode` (`:1949-1959`). The rounded transcendentals keep
passing `{}`: they round a pure number at scale 1.

**Files:**
- Modify: `include/formula-cpp/render.hpp`: new `AuthorTextSpelling`, `verbatim_text`, `coherent_unit_spelling` and
  `rounding_unit_text` after `latex_unit` (`:473-476`); the five render sites above; `rounding_call`'s comment
  (`:1256-1262`).
- Modify: `include/formula-cpp/trace_render.hpp`: `coherent_unit_text` (`:595-650`) becomes a wrapper; the seven
  trace sites above; `rounding_call_text`'s comment (`:1080-1082`).
- Modify: `test/trace_shown_unit_tests.cpp`: one new `TEST_CASE`, and one line in the walker test.
- Modify: `test/render_tests.cpp`: one new `TEST_CASE` after `"render: a significant-digits rounding node renders as
  round(..., to N sf of unit)"` (`:463`).
- Modify: `docs/rounding-and-conditionals.md:202-204`, `docs/tracing.md` (after `:380`), `CHANGELOG.md`.

**Interfaces:**
- Consumes: Task 2's spelling of `coherent_unit_text`; Task 1's guarantee that a symbol-less dimensionless unit is at
  scale 1; `Rational::make(std::int64_t, std::int64_t) -> std::expected<Rational, ArithmeticError>`;
  `fraction_text(Rational) -> NumberText` (`number_text.hpp:378`; bind it to a named local before `.view()`, which is
  deleted on an rvalue); `escaped_author_text(std::string_view) -> std::string`.
- Produces (all in `formula::detail`, `render.hpp`):
    - `using AuthorTextSpelling = std::string (*)(std::string_view);`
    - `verbatim_text(std::string_view) -> std::string`;
    - `coherent_unit_spelling(Dimension, AuthorTextSpelling) -> std::string`: the body Task 2 wrote;
    - `rounding_unit_text(Unit const&, AuthorTextSpelling) -> std::string`: the text after `of` (or `in`) in a
      rounding clause; empty only for a dimensionless unit at scale 1, or a malformed magnitude.
- `trace_render.hpp`'s `coherent_unit_text(Dimension)` keeps its signature and spelling.

- [ ] **Step 1: Write the failing tests.** In `test/trace_shown_unit_tests.cpp`, after the fixtures (after
  `struct Share …` at `:84-86`), add:

```cpp
// A Celsius scale with no symbol: an offset unit the rounding clause must name by its size and its zero.
inline constexpr formula::Unit UnnamedCelsius { .dimension = formula::dim::Temperature,
                                                .offsetNumerator = 27315,
                                                .offsetDenominator = 100 };
struct UnnamedReading: formula::Quantity<UnnamedReading, "T_u", "a reading on an unnamed scale", UnnamedCelsius>
{
};
```

  and after the test case `"a value in a unit with no symbol is shown in the coherent unit, with its symbol"`
  (`:314-320`), add:

```cpp
TEST_CASE("a rounding in a unit with no symbol names that unit by its size", "[trace-render][shown-unit][rounding]")
{
    // 3.141 of the unnamed gram to 2 places is 3.14 of it: 157/50000 kg. The
    // places count in the unnamed gram, and the line says so in the coherent
    // unit the value is written in.
    auto const masses = formula::environment(formula::Measured<UnnamedMass> { Rational { 3141, 1000 } });
    CHECK(trace_text(formula::rounded<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                         var<UnnamedMass>),
                     masses)
          == "1. m_u = 3141/1000000 kg\n"
             "2. round(#1, to 2 dp of 1/1000 kg) = 157/50000 kg [nearest, ties to even]\n");
    CHECK(trace_text(formula::rounded_to_digits<UnnamedGram, formula::SignificantDigits { 2 }, formula::RoundingMode::HalfEven>(
                         var<UnnamedMass>),
                     masses)
              .find("2. round(#1, to 2 sf of 1/1000 kg) = 31/10000 kg")
          != std::string::npos);
    // 20.5 on the unnamed Celsius scale, rounded to 0 places of it: 21, which
    // is 294.15 K. The places count from that scale's zero, 273.15 K.
    auto const readings = formula::environment(formula::Measured<UnnamedReading> { Rational { 41, 2 } });
    CHECK(trace_text(formula::rounded<UnnamedCelsius, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
                         var<UnnamedReading>),
                     readings)
          == "1. T_u = 5873/20 K\n"
             "2. round(#1, to 0 dp of 1 K from 27315/100 K) = 5883/20 K [nearest, ties away from zero]\n");
}
```

  In `"every value a trace shows is in the unit written after it"`, after the line Task 2 added, add:

```cpp
    // A rounding in a unit with no symbol.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::rounded<UnnamedGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(var<UnnamedMass>),
        inputs));
```

  In `test/render_tests.cpp`, add to the anonymous namespace's fixtures (after `struct Strength …`):

```cpp
// Units with no symbol, which a rounding clause names by their size in the coherent unit.
inline constexpr formula::Unit UnlabelledGram { .dimension = formula::dim::Mass,
                                                .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1000 };
inline constexpr formula::Unit UnlabelledCelsius { .dimension = formula::dim::Temperature,
                                                   .offsetNumerator = 27315,
                                                   .offsetDenominator = 100 };
inline constexpr formula::Unit UnlabelledPerGram { .dimension = formula::dim::Scalar / formula::dim::Mass,
                                                   .magnitudeNumerator = 1000 };
struct UnlabelledWeight: formula::Quantity<UnlabelledWeight, "w", "a mass in an unnamed unit", UnlabelledGram>
{
};
struct UnlabelledReading: formula::Quantity<UnlabelledReading, "t", "a reading on an unnamed scale", UnlabelledCelsius>
{
};
struct UnlabelledLoading: formula::Quantity<UnlabelledLoading, "q", "a count per unnamed gram", UnlabelledPerGram>
{
};
```

  and after the significant-digits rounding test case (`:463`), add:

```cpp
TEST_CASE("render: a rounding in a unit with no symbol names that unit by its size", "[render][rounding]")
{
    constexpr auto toHundredths =
        formula::rounded<UnlabelledGram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
            var<UnlabelledWeight>);
    CHECK(formula::render<Dialect::Plain>(toHundredths) == "round(w, to 2 dp of 1/1000 kg)");
    CHECK(formula::render<Dialect::LaTeX>(toHundredths) == "\\operatorname{round}_{2\\,\\mathrm{1/1000\\ kg}}(w)");
    CHECK(formula::render<Dialect::Plain>(
              formula::rounded_to_digits<UnlabelledGram, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfEven>(
                  var<UnlabelledWeight>))
          == "round(w, to 3 sf of 1/1000 kg)");
    CHECK(formula::render<Dialect::Plain>(
              formula::rounded<UnlabelledCelsius, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(
                  var<UnlabelledReading>))
          == "round(t, to 1 dp of 1 K from 27315/100 K)");
    CHECK(formula::render<Dialect::Plain>(
              formula::rounded<UnlabelledPerGram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(
                  var<UnlabelledLoading>))
          == "round(q, to 0 dp of 1000 kg^-1)");
    // A dimensioned unit of magnitude 1 with no symbol is still named by its
    // size, never bare and never "of kg" alone: the reader cannot tell it from
    // the coherent unit otherwise.
    CHECK(formula::render<Dialect::Plain>(
              formula::rounded<UnlabelledKilogram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                  var<UnlabelledHeft>))
          == "round(h, to 2 dp of 1 kg)");
    // A dimensionless unit at scale 1 still writes no clause.
    CHECK(formula::render<Dialect::Plain>(
              formula::rounded<formula::unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
                  formula::number(formula::Rational { 1, 3 })))
          == "round(1/3, to 2 dp)");
}
```

  Beside the file's other unlabelled fixtures, add the magnitude-1 one this case uses:

```cpp
// A mass unit of magnitude 1 with no symbol: the kilogram's size under no name.
inline constexpr formula::Unit UnlabelledKilogram { .dimension = formula::dim::Mass };
struct UnlabelledHeft: formula::Quantity<UnlabelledHeft, "h", "a mass in an unnamed unit of one kilogram", UnlabelledKilogram>
{
};
```

  (`render_tests.cpp` has no `using formula::Rational;`, hence the qualified name. Its `using formula::Dialect;` and
  `using formula::var;` are at `:92-93`, above the test case. If `render()` spells the constant `1/3` differently,
  for example bracketed, take the spelling the file already pins for a dimensionless fraction constant. The point of
  that check is that no `of` clause follows.)

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "names that unit by its size|every value a trace shows"`.
  Expected: the two new cases FAIL (`to 2 dp)` with no clause); the walker case passes already (the walker reads
  values only). Prove ctest ran 3 tests.

- [ ] **Step 3: Add the shared spelling to `render.hpp`.** Immediately after `latex_unit` (`:473-476`), inside
  `namespace detail`, add:

```cpp
    /// How a caller writes a piece of author text that it states inside a
    /// unit's spelling -- a unit's symbol, or a named base dimension's name:
    /// as it is, in `render()`'s text (`verbatim_text`), or escaped, in a
    /// trace line (`escaped_author_text`, `trace_render.hpp`).
    using AuthorTextSpelling = std::string (*)(std::string_view);

    /// @p authored as it is: `render()` writes a unit's symbol verbatim in
    /// plain text and Markdown, and LaTeX escapes the whole unit text
    /// afterwards (`latex_unit`).
    [[nodiscard]] inline std::string verbatim_text(std::string_view authored)
    {
        return std::string { authored };
    }
```

  Then **move** the body of `coherent_unit_text` from `trace_render.hpp` (as Task 2 left it) here, renamed and with
  the name spelling as a parameter, together with its whole comment:

```cpp
    /// <the comment of `coherent_unit_text`, moved, unchanged except: "Each
    /// name goes through `escaped_author_text`" becomes "Each name is written
    /// by @p spellName: escaped in a trace, as it is in `render()`">
    [[nodiscard]] inline std::string coherent_unit_spelling(Dimension dimension, AuthorTextSpelling spellName)
    {
        // <the body Task 2 wrote, with the one call
        //  `escaped_author_text(view(dimension.namedBases[slot].name))`
        //  replaced by `spellName(view(dimension.namedBases[slot].name))`>
    }
```

  Write the body out in full when you move it; the angle-bracket lines above say what changes, they are not code.
  Then add:

```cpp
    /// The unit a rounding's places or digits count in, as the clause after
    /// `of` names it: `round(m, to 2 dp of <this>)`.
    ///
    /// - A unit with a symbol: its symbol, as @p spellAuthorText writes it.
    /// - A dimensionless unit with no symbol: nothing, so the clause is
    ///   dropped. Such a unit is at scale 1 (`RequireNamedScaledScalar`,
    ///   `unit.hpp`), and its places are places of the bare number.
    /// - A dimensioned unit with no symbol: its size in the coherent unit,
    ///   exact, then that unit's spelling: `1/1000 kg`, `1000 kg^-1`. The
    ///   value after a trace's `=` is written in the same coherent unit, so
    ///   the line says what the places count in.
    /// - The same with an offset: its size and its zero, both in the coherent
    ///   unit: `1 K from 27315/100 K`. The places count steps of the size from
    ///   that zero, which is how the rounding computes them.
    ///
    /// Nothing for a magnitude or offset that names no rational (a zero
    /// denominator): a malformed unit, refused by every conversion, whose
    /// rounding never reaches a value to show.
    [[nodiscard]] inline std::string rounding_unit_text(Unit const& roundedIn, AuthorTextSpelling spellAuthorText)
    {
        std::string_view const symbolText = view(roundedIn.symbolText);
        if (!symbolText.empty())
            return spellAuthorText(symbolText);
        if (roundedIn.dimension == dim::Scalar)
            return {};
        std::expected<Rational, ArithmeticError> const unitSize =
            Rational::make(roundedIn.magnitudeNumerator, roundedIn.magnitudeDenominator);
        if (!unitSize.has_value())
            return {};
        std::string const coherentText = coherent_unit_spelling(roundedIn.dimension, spellAuthorText);
        NumberText const sizeText = fraction_text(*unitSize);
        std::string spelled = std::string { sizeText.view() } + " " + coherentText;
        if (roundedIn.offsetNumerator == 0)
            return spelled;
        std::expected<Rational, ArithmeticError> const unitZero =
            Rational::make(roundedIn.offsetNumerator, roundedIn.offsetDenominator);
        if (!unitZero.has_value())
            return {};
        NumberText const zeroText = fraction_text(*unitZero);
        return spelled + " from " + std::string { zeroText.view() } + " " + coherentText;
    }
```

  In `trace_render.hpp`, `coherent_unit_text` keeps a short comment and becomes:

```cpp
    /// The coherent unit of @p dimension spelt from its base units, as a
    /// trace line writes it: `coherent_unit_spelling` (`render.hpp`), with
    /// each named base dimension's name escaped as author text.
    [[nodiscard]] inline std::string coherent_unit_text(Dimension dimension)
    {
        return coherent_unit_spelling(dimension, escaped_author_text);
    }
```

  If `trace_render.hpp` declares `escaped_author_text` below `coherent_unit_text`, it does not (it is at `:185`, the
  wrapper at `:610`); keep that order.

- [ ] **Step 4: Use it at every rounding clause.**
  In `render.hpp`:
    - `RoundNode` (`:1330-1336`): replace `std::string { view(declaredUnit.symbolText) }` with
      `detail::rounding_unit_text(declaredUnit, detail::verbatim_text)`.
    - `RoundSignificantNode` (`:1343-1356`), `RoundedRootNode` (`:1367-1377`), `ElementwiseRoundNode`
      (`:1070-1087`): replace the initialiser of `unitSymbol` (`{ view(… .symbolText) }`) with
      `= detail::rounding_unit_text(declaredUnit, detail::verbatim_text)` (`roundedIn` in the elementwise one).
    - `RoundedOpaqueOutputNode` (`:1955-1958`): as `RoundNode`.
  In `trace_render.hpp`, replace `unit_symbol_text(<step>.unit)` with `rounding_unit_text(<step>.unit,
  escaped_author_text)` in exactly these places: `StepKind::Round`, `RoundSignificant`, `RoundingRuleApplied`,
  `ElementwiseRound`, `RoundedRoot`, `RoundedOpaqueOutput` (in `step_expression`), and the
  `rounding_call_text(opaque_output_label(...), …)` call in `rounded_opaque_output_line`. Leave `NumericValue`'s
  `unit_symbol_text(shownStep.sourceUnit)` as it is: it is no rounding (see the report note below).

  Update the comments that say "No unit clause for a unit with no symbol": in `rounding_call` (`render.hpp:1256-1262`)
  write "The unit is `rounding_unit_text`'s: a unit with no symbol is named by its size, and only a dimensionless
  unit at scale 1 has no clause."; in `rounding_call_text` (`trace_render.hpp:1080-1082`) replace "No unit clause for
  a unit with no symbol." with "The unit is `rounding_unit_text`'s (`render.hpp`), so that a unit with no symbol is
  named by its size, in the coherent unit the value after `=` is written in." In `ElementwiseRoundNode`'s render
  comment (`render.hpp:1066-1069`), replace "and dropped for a unit with no symbol" with "and, for a unit with no
  symbol, its size in the coherent unit (`rounding_unit_text`)".

- [ ] **Step 5: Run the tests to see them pass.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "by its size|every value a trace shows|rounding|spelt"`.
  Expected: `ALL OK`. If the Celsius case's first line or mode text differs from the pinned text in anything but the
  rounding clause, stop and report the actual line rather than re-pinning it: the clause is what this task changes.

- [ ] **Step 6: Document it.**
    - `docs/rounding-and-conditionals.md:202-204`: after "…renders as `numeric(<operand>, in <unit>)`." add: "A unit
      with no symbol is named by its size in the coherent unit, `round(m, to 2 dp of 1/1000 kg)`, and one with an
      offset by its size and its zero, `to 1 dp of 1 K from 27315/100 K`, so that the places say what they count in;
      only a dimensionless unit at scale 1 writes no unit clause, `round(x, to 2 dp)`."
    - `docs/tracing.md`, after the sentence Task 1 added at the end of the paragraph ending `:380`: "A rounding in a
      unit with no symbol names that unit by its size in the coherent unit: `round(#1, to 2 dp of 1/1000 kg) =
      157/50000 kg`."
    - `CHANGELOG.md`, `### Changed`:

```markdown
- A rounding in a unit with no symbol names that unit by its size in the coherent unit, in `render()` and in a
  trace: `round(#1, to 2 dp of 1/1000 kg) = 157/50000 kg`, where it wrote `round(#1, to 2 dp)`, which read as places
  of the kilogram written after it. A unit with an offset is named by its size and its zero, `to 1 dp of 1 K from
  27315/100 K`. Only a dimensionless unit at scale 1 still writes no unit clause.
```

- [ ] **Step 6b: The two other places that write an unnamed unit's number without its scale.** These were found
  while planning this task. They follow the same display rule, so they are fixed here, not filed:
    - `render()` of a constant in a dimensioned unit with no symbol (`render_node(ConstantNode<U> const&, …)`,
      `render.hpp:985-998`) writes `3` for 3 of an unnamed gram;
    - `numeric(x, in <unit>)` writes no clause for such a unit, in `render()` (`render_node(NumericValueNode<…>
      const&, …)`, `render.hpp:1405-1416`) and in a trace (`StepKind::NumericValue`, `trace_render.hpp:1171-1172`).

  1. **Write the failing tests first.** Add one test case to `test/trace_shown_unit_tests.cpp`, after the cases
     Step 1 added, using the same `UnnamedGram` fixture (magnitude 1/1000, mass, no symbol) and the file's existing
     environment helpers:

```cpp
TEST_CASE("a constant and a numeric value in a unit with no symbol say what scale their number is on",
          "[trace][units]")
{
    // A constant typed as 3 of a unit of 1/1000 kg with no symbol: render()
    // writes it in the coherent unit, as a trace does, never as a bare 3.
    auto const typedMass = formula::constant<UnnamedGram>(formula::Rational { 3 });
    CHECK(formula::render(typedMass) == "3/1000 kg");

    // numeric(x, in <unit>) names the unit its bare number is taken in by its
    // size, in render() and in the trace line alike.
    auto const bareMass = formula::numeric<UnnamedGram, "the table is in unnamed grams">(typedMass);
    CHECK(formula::render(bareMass) == "numeric(3/1000 kg, in 1/1000 kg)");
    std::string const traced = formula::render_trace(formula::explain<Count>(bareMass, formula::environment()));
    CHECK(traced.find("numeric(#1, in 1/1000 kg)") != std::string::npos);
}
```

     Adjust only the spelling of the factory calls (`constant`, `numeric`, `explain`, the quantity the numeric
     value yields, `render_trace`) to the forms the file and `include/formula-cpp/numeric.hpp` actually use. The
     three expected texts are the contract. If `numeric`'s quantity has a name other than `Count` in the file's
     fixtures, use the file's dimensionless quantity. Run it with `-Filter "say what scale"` and watch all three
     checks fail.

  2. **The constant.** In `render.hpp`, where Step 3 moved the coherent spelling, also move `spells_coherent_unit`
     and `shown_unit_of` from `trace_render.hpp` into `render.hpp`'s `detail` namespace, unchanged with their
     comments. `trace_render.hpp` uses them through `detail::` as before, so its call sites do not change. Then
     `render_node(ConstantNode<U> const&, …)` becomes:

```cpp
template <Dialect D, Unit U, Vocabulary V>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node, V const& vocabulary)
{
    constexpr Unit declaredUnit = U;
    // A unit with no symbol cannot say what scale its number is on, so the
    // constant is written in the coherent unit, exact, as a trace writes it
    // (`detail::spells_coherent_unit`).
    if constexpr (detail::spells_coherent_unit(declaredUnit, declaredUnit.dimension))
    {
        constexpr Unit coherentUnit = coherent(declaredUnit.dimension);
        std::expected<Rational, ArithmeticError> const inCoherent =
            checked_convert(node.number, declaredUnit, coherentUnit);
        std::string const numberText =
            inCoherent.has_value()
                ? detail::styled_number_text(*inCoherent, detail::typed_number_style(vocabulary).exact_only(), coherentUnit)
                : detail::not_shown_text(inCoherent.error());
        std::string const coherentText = detail::coherent_unit_spelling(declaredUnit.dimension, detail::verbatim_text);
        if constexpr (D == Dialect::LaTeX)
            return numberText + detail::unit_clause("\,", detail::latex_unit(coherentText));
        else
            return detail::number_with_unit(numberText, coherentText);
    }
    else if constexpr (D == Dialect::LaTeX)
        return detail::typed_number_text(node.number, declaredUnit, vocabulary)
               + detail::unit_clause("\,", detail::latex_unit(view(declaredUnit.symbolText)));
    else
        return detail::number_with_unit(detail::typed_number_text(node.number, declaredUnit, vocabulary),
                                        view(declaredUnit.symbolText));
}
```

     `spells_coherent_unit` must be `constexpr` for the `if constexpr`. Mark it `constexpr` when moving it; `view`
     and `Dimension`'s `==` already are. If `not_shown_text` lives in `trace_render.hpp`, move it into
     `render.hpp`'s `detail` with its comment too. Its text is the trace's `(not shown: …)`. Extend the constant's
     doc comment by one sentence: "A constant in a dimensioned unit with no symbol is written in the coherent
     unit, exact (`3/1000 kg`), for the reason a trace is."

  3. **`numeric`.** In `render_node(NumericValueNode<…> const&, …)`, replace
     `std::string const unitSymbol { view(declaredUnit.symbolText) };` with
     `std::string const unitSymbol = detail::rounding_unit_text(declaredUnit, detail::verbatim_text);`. In
     `trace_render.hpp`'s `StepKind::NumericValue` case, replace `unit_symbol_text(shownStep.sourceUnit)` with
     `rounding_unit_text(shownStep.sourceUnit, escaped_author_text)`. In `rounding_unit_text`'s comment (Step 3),
     change the first line to "The unit a rounding's places or digits count in, or a numeric value's bare number
     is taken in, as the clause after `of` or `in` names it."

  4. Re-run `-Filter "say what scale|by its size|every value a trace shows|render"` → `ALL OK`. A pinned text that
     changes is one of these two forms for a unit with no symbol. Re-pin it, and list it in the report. Any other
     change: stop and report.

  5. **Docs.** `docs/expressions.md`, where constants are described (search for "followed by its unit's symbol"):
     add "A constant in a dimensioned unit with no symbol is written in the coherent unit, exact: `3/1000 kg`." Then
     extend the CHANGELOG entry below by one sentence: "A constant in such a unit renders in the coherent unit
     (`3/1000 kg`, where it wrote a bare `3`), and `numeric(x, in <unit>)` names the unit by its size too."

- [ ] **Step 7: Verify.** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."` → `ALL OK`.
  `hygiene.headers` must stay green: no `<string>` reaches a core header (`render.hpp` already had it).
  Expected count: **+3** non-negative tests, **0** negative.

- [ ] **Step 8: Commit.** Message in `$S\task-3-commit.txt`:

```
fix: name a rounding's unit by its size when it has no symbol

A rounding to places of a unit with no symbol wrote no unit clause,
while the value after it read in the coherent unit: round(#1, to 2 dp) =
3/1000 kg let "2 dp" read as places of a kilogram. The clause now names
the unit by its size in the coherent unit, round(#1, to 2 dp of 1/1000
kg), and an offset unit by its size and its zero, to 1 dp of 1 K from
27315/100 K, in render() and in a trace alike. numeric(x, in <unit>)
names such a unit the same way, and render() writes a constant in it in
the coherent unit, 3/1000 kg, where it wrote a bare 3. The coherent unit's
spelling moves into render.hpp so that both can write it.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Run: `git -C $T add -A include test docs CHANGELOG.md` then `git -C $T commit -F $S\task-3-commit.txt`.

---

### Task 4: A precision limit's first pass borrows its unit; the snap compare explained (#16, #18)

**#16.** Pass 1 of a precision limit restates its level's value, but its unit is chosen from types alone
(`precision.hpp:1015`): the limit's first placeholder's quantity, else the level expression's quantity, else the
coherent unit. A constant level in grams therefore reads `40 g`, then `1/25 kg` on the pass-1 line. The pass-1 step
now borrows the unit of the step it restates, through `detail::restated_unit_or`, as pass 2 and a conditional already
do (`trace.hpp:3437-3441`); the static unit stays the fallback. Spec §2.4.

**#18.** A snap's "on a permitted value" test compares raw numerator/denominator pairs (`snap_suffix`,
`trace_render.hpp:2164`), while table rows compare values (`same_declared_bound`). The raw compare is exact: an exact
hit records one row twice from a single index (`Segment { Permitted[located->low], Permitted[located->low] }`,
`snap.hpp:133-134`), and `RequireValidBreakpointTable` keeps the permitted set strictly ascending by value, so two
different rows never hold one value. That is stated in a comment. Separately, `lookup_miss_text` spells its low bound
before it branches, and only one branch uses it. Spec §2.5.

**Files:**
- Modify: `include/formula-cpp/trace.hpp`: `RecordingSink::precision_level_produced` (`:3826-3871`), its body and
  comment.
- Modify: `include/formula-cpp/trace_render.hpp`: `snap_suffix` (`:2152-2165`), `lookup_miss_text` (`:874-911`).
- Modify: `test/trace_shown_unit_tests.cpp`: one new `TEST_CASE`, one line in the walker test.
- Modify: `docs/tracing.md:393-394`, `CHANGELOG.md`.

**Interfaces:**
- Consumes: `detail::restated_unit_or(std::vector<Step<Rep>> const&, std::vector<std::size_t> const& operands,
  Dimension, std::optional<Rep> const& restatedValue, Unit fallback) -> Unit` (`trace.hpp:2367-2381`): the last
  claimed operand's unit when it has a symbol, is of the dimension, holds exactly the restated value and passes
  `borrowable_for_a_point`; `fallback` otherwise.
- Produces: no new names. `Step::unit` of a `StepKind::PrecisionLevel` step holds the borrowed unit where one applies.

- [ ] **Step 1: Write the failing test.** In `test/trace_shown_unit_tests.cpp`, after the test case `"a conditional
  reads in its chosen branch's unit, offset or not"` (`:448-469`), add:

```cpp
TEST_CASE("a precision limit's first pass reads in the unit of the level it restates", "[trace-render][shown-unit][precision]")
{
    // The level is a constant in grams and the limit names no quantity, so
    // nothing in the types says grams: pass 1 reads off the step it restates,
    // 40 g, never 1/25 kg.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<unit::Gram>(Rational { 40 }), formula::constant<unit::Gram>(Rational { 1 })),
                     determinations)
              .starts_with("1. 40 g\n"
                           "2. level (pass 1 of 2) = #1 = 40 g\n"));
}
```

  In the same test case, pin the two levels that cannot borrow:

```cpp
    // A level constant in a unit with no symbol cannot lend its unit
    // (`restated_unit_or` borrows only a unit with a symbol): pass 1 stays in
    // the unit the types give, the coherent kilogram, as before.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<UnnamedGram>(Rational { 40000 }), formula::constant<unit::Gram>(Rational { 1 })),
                     determinations)
              .find("2. level (pass 1 of 2) = #1 = 40 kg
")
          != std::string::npos);
    // A level constant in degrees Celsius is a point on an offset scale, which
    // `borrowable_for_a_point` lets a restating step show: pass 1 reads in
    // degrees Celsius, as the constant's own line does, never as a kelvin
    // difference.
    CHECK(trace_text(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                         formula::constant<unit::DegreeCelsius>(Rational { 20 }),
                         formula::constant<unit::DegreeCelsius>(Rational { 1 })),
                     determinations)
              .find("2. level (pass 1 of 2) = #1 = 20 Â°C
")
          != std::string::npos);
```

  Use the file's own names for the unnamed-gram fixture and the Celsius unit. If `precision_limit` refuses a level
  of temperature at compile time, drop the Celsius check, and state the refusal's text in the report. That refusal
  is then what keeps an offset level from reaching pass 1. The task's count stays **+1**: these checks join the new
  test case.

  In `"every value of a rejection, a bill, the statistics, a precision limit and an opaque call is in the unit written
  after it"`, after the second precision-limit walker call (`:763-766`), add:

```cpp
    // A precision limit over a level constant in grams.
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::constant<unit::Gram>(Rational { 40 }),
                                                                        formula::constant<unit::Gram>(Rational { 1 })),
        pair));
```

- [ ] **Step 2: Run it to see it fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "first pass reads"`.
  Expected: 1 test, FAILED; the expansion shows `2. level (pass 1 of 2) = #1 = 1/25 kg`.

- [ ] **Step 3: Borrow the unit in pass 1.** In `precision_level_produced`, after the value is set (after the
  `else if (produced->has_value()) levelStep.value = **produced;` line) and before `std::size_t const levelIndex`,
  add:

```cpp
        // Pass 1 restates the level expression's value, so it reads in the
        // unit of the step it restates, as pass 2 and a conditional do: a
        // level constant in grams reads in grams on both lines. The unit the
        // types give stays the answer when nothing can be borrowed.
        levelStep.unit = detail::restated_unit_or(_trace->steps, levelStep.operands, levelStep.dimension,
                                                  levelStep.value, levelUnit);
```

  Replace the function's comment paragraph "Its value is in @p levelUnit, the unit of the quantity the limit's
  placeholders name, so that the level reads as the results do; its dimension is the level expression's." with:

```cpp
    /// It reads in the unit of the step it restates when that unit has a
    /// symbol and holds exactly its value (`detail::restated_unit_or`), as
    /// pass 2 does; otherwise in @p levelUnit, the unit of the quantity the
    /// limit's placeholders name, so that the level reads as the results do.
    /// Its dimension is the level expression's.
```

- [ ] **Step 4: Run it to see it pass, and find what else moved.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "first pass reads|precision|shown"`.
  Expected: the new case passes. A pinned pass-1 line in `test/precision_tests.cpp` or elsewhere changes only where
  its level's last step shows a unit with a symbol other than the one the types gave. Re-pin each such line to the
  borrowed unit, and list every re-pinned line in the report with its old and new text. A change to any other line is
  a defect of this step: stop and report it.

- [ ] **Step 5: Explain the snap compare, and spell the curve's low bound only where it is used.** In
  `trace_render.hpp`, `snap_suffix`, directly above `if (neighbours.low == neighbours.high)` (`:2164`), add:

```cpp
            // Raw pairs, not values, and exact all the same: an exact hit
            // records the one row it hit twice, from a single index
            // (`locate_and_snap`, `snap.hpp`), and the permitted set is
            // strictly ascending by value (`RequireValidBreakpointTable`), so
            // two different rows never hold one value. A lookup's segment and
            // a missed lookup's range compare by value
            // (`same_declared_bound`) because a table's rows can be typed as
            // different pairs of one number.
```

  In `lookup_miss_text`, replace the tail from `std::string const lowText = shown_bound_text(` to the function's last
  `return` with:

```cpp
        if (same_declared_bound(recorded.coveredRange->lowNumerator,
                                recorded.coveredRange->lowDenominator,
                                recorded.coveredRange->highNumerator,
                                recorded.coveredRange->highDenominator))
        {
            std::string const onlyRowText = shown_bound_text(
                recorded.coveredRange->lowNumerator, recorded.coveredRange->lowDenominator, keyUnit, numberStyle);
            return "outside the curve, whose only row is at "
                   + number_with_unit(onlyRowText, shown_unit_text(keyUnit, keyUnit.dimension));
        }
        return "outside the curve, which runs " + closed_range_text(*recorded.coveredRange, keyUnit, numberStyle);
```

  Keep the comment above it ("A curve with exactly one row covers…") where it is.

- [ ] **Step 6: Document it.**
    - `docs/tracing.md:393-394`: change "A precision limit reads in its second pass's." to "A precision limit reads
      in its second pass's, and its first pass in the unit of the level it restates: a level constant in grams reads
      in grams on both lines."
    - `CHANGELOG.md`, `### Changed`:

```markdown
- A precision limit's first pass reads in the unit of the level step it restates, as its second pass already did:
  a level constant declared in grams reads `40 g` on both lines, where the first pass read `1/25 kg`. The unit the
  limit's quantities give is still used when the level's step has none to lend.
```

- [ ] **Step 7: Verify.** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."` → `ALL OK`.
  Expected count: **+1** non-negative test, **0** negative. The snap and lookup tests (`snap`, `lookup`, `curve`,
  `shown`) pass unchanged: Step 5 changes no output.

- [ ] **Step 8: Commit.** Two commits, one per issue. First `$S\task-4a-commit.txt`:

```
fix(trace): read a precision limit's first pass in its level's unit

Pass 1 restates the level's value but took its unit from the types
alone: the limit's quantity, else the level expression's, else the
coherent unit. A level constant in grams read 40 g on its own line and
1/25 kg on the pass-1 line. Pass 1 now borrows the unit of the step it
restates, under the rule pass 2 and a conditional use; the unit the
types give stays the fallback.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Run: `git -C $T add include/formula-cpp/trace.hpp test/trace_shown_unit_tests.cpp docs/tracing.md CHANGELOG.md`
  plus any test file Step 4 re-pinned, then `git -C $T commit -F $S\task-4a-commit.txt`.
  Then `$S\task-4b-commit.txt`:

```
refactor(trace): say why a snap's exact hit compares raw pairs

A snap decides it sat on a permitted value by comparing two raw pairs,
where a lookup's rows compare values. That is exact: an exact hit
records one row twice, and the permitted set is strictly ascending by
value. A comment now says so. A missed curve lookup also spells its low
bound only on the path that writes it.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Run: `git -C $T add include/formula-cpp/trace_render.hpp` then `git -C $T commit -F $S\task-4b-commit.txt`.

---

**Lane A totals:** +5 non-negative tests (Task 1: +1, Task 2: +1, Task 3: +2, Task 4: +1), +3 negative tests (Task 1).

## Lane B — numerics (#20, #19)

Lane B is Task 5, then Task 6. Each task runs in the implementer's own agent-owned worktree:

- **Before the task:** the controller has set the branch `feature/open-issues-numerics` to the previous Lane B head. For Task 5 that is the commit that adds this plan.
- **After the task:** the controller moves the branch to the task's last commit with `git branch -f`.

The two tasks share `docs/numeric-headroom.md` and `CHANGELOG.md`, which is why they run one after the other.

In this lane, `$T` is the implementer's own worktree, as a Windows path (`D:\formula-cpp\.claude\worktrees\<name>`). `$TW` is the same path as WSL sees it (`/mnt/d/formula-cpp/.claude/worktrees/<name>`). `$SW` is the scratchpad as WSL sees it: `/mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad`.

**A correction to the spec, decided while planning (binds Task 5).** Spec §3.1 keeps the kernel's 128 fraction bits and says the existing narrowing alone gives every answer that fits. A model of the kernel shows otherwise:

- **The model.** `$S\laneb\kernel_sim.py` copies the C++ step for step.
- **What it shows.**
    - An exponential's enclosure is 512 · 2^-128 = 2^-119 of its value wide.
    - With a 128-bit `Rational`, a result near the top of the range is up to 2^127 last kept units, so its enclosure is up to 2^8 units wide.
    - So e^45 at 18 places and e^88 at 0 places are **always** undecided (`Overflow`) at 128 fraction bits.
    - The pinned evidence is `undecided == 38` in `test/transcendental_tests.cpp`: e^43 to e^44 at 17 and 18 places, which `Rational` can already hold.
- **The change.** The exponential alone moves to **192 fraction bits**, with a 192-bit ln 2.
    - Its enclosure is then 2^-183 of its value, so at worst 2^-56 of a last kept unit: the same margin the 64-bit kernel had.
    - The logarithms keep 128 bits. Their results are below 89, so 2^-120 is 2^-60 of a unit at 18 places.
    - `KernelLimbs` stays 12: the widest exponential numerator is below 2^321, and below 2^381 after `decide_rounding`'s 10^18.
- **Bounds checked against the spec:**
    - ln ends ≤ 254 units: correct.
    - log10 < 203 units: correct.
    - log10's widest product: below **2^262**. The spec's 2^264 holds but is loose.
    - Exponential's widest numerator: **2^321**, and **2^381** after 10^18. The spec's 2^257 and 2^317 assume 128 fraction bits.
    - Exponential's reduction: its argument error is **128** units, not 64, since k reaches 127. `ExponentialSlack` 512 still covers it, with 360 needed.

**A correction to the spec, found while planning (binds Task 6).** Spec §3.2 calls the headroom page's "256-bit" stale. It is not.

- The `census:least-squares` rows the sentence describes are computed by `LinearLeastSquares::compute_exact` (`least_squares.hpp`). That works in `exact_limbs = 8` limbs: 256 bits.
- The 12-limb, 384-bit kernel is `LinearLeastSquaresOfObservations`', which those rows do not use.
- So N is generated from `formula::LinearLeastSquares::exact_limbs * 32`, and the hook goes into `LinearLeastSquares::compute_exact`.
- Both hand-written figures are right today. The model `$S\laneb\fit_widths.py` gives 68 and 249 bits, with 71 sizes overflowing from 58, matching the census.
- The opaque guide's 65/73, 93/91 and 130/130 are also right (`$S\laneb\fifty_widths.py`).

---

### Task 5: The logarithm and exponential kernel takes 128-bit arguments (#20)

The kernel now takes the argument's numerator and denominator as `UInt128` magnitudes, instead of narrowing them to 64 bits:

- `scaled_quotient` becomes a long division over `UInt128` words.
- A logarithm's reduction allows k ≤ 126.
- The exponential computes with 192 fraction bits. Its reduction allows k ≤ 127, and its cap moves from 44 to 887/10.

The pinned 64-bit refusals become pinned answers.

**Files:**
- Modify: `include/formula-cpp/detail/transcendental.hpp`: the file comment (`:4-56`); the constants (`:68-112`); `kernel_word` (a three-word overload); `scaled_quotient` (`:114-147`); `exponential_series_lower` (`:174-195`); `natural_log_magnitude` (`:216-254`); `exponential_enclosure` (`:293-372`).
- Modify: `include/formula-cpp/rounded_transcendental.hpp`: `rounded_transcendental` (`:39-92`); the comments of `rounded_ln`, `rounded_log10` and `rounded_exp` (`:122-156`).
- Modify: `include/formula-cpp/detail/checked_int.hpp:13-17`, the file comment only. `narrow_to_int64` stays: `band.hpp` uses it.
- Modify: `docs/expressions.md:510-526`, `docs/numeric-headroom.md:424-431`, `CHANGELOG.md`.
- Test: `test/transcendental_tests.cpp`, `test/rounded_transcendental_tests.cpp`.

**Interfaces:**
- Consumes: `formula::detail::UInt128`, `UInt128Division`, `u128_divmod`, `u128_add`, `u128_sub`, `portable::shift_left` (`int128.hpp`); `wide_magnitude(Int128) -> UInt128` (`detail/checked_int.hpp:247`); `WideUnsigned<12>::from_u128` (`detail/wide_int.hpp`).
- Produces, in `formula::detail`:
    - `inline constexpr std::size_t ExponentialFractionBits = 192;`
    - `inline constexpr KernelWord ExponentialOne;` (2^192)
    - `inline constexpr KernelWord Ln2Lower192, Ln2Upper192;` (floor(ln 2 · 2^192), and that plus one)
    - `inline constexpr std::uint32_t TaylorTermLimit = 50;` (was 40)
    - `constexpr KernelWord kernel_word(std::uint64_t, std::uint64_t, std::uint64_t) noexcept;`
    - `template <std::size_t FractionBits> requires(FractionBits % 64 == 0 && FractionBits <= 192) constexpr ScaledQuotient scaled_quotient(UInt128 dividend, UInt128 divisor) noexcept;`. It replaces the 64-bit `scaled_quotient(std::uint64_t, std::uint64_t)`.
    - In `rounded_transcendental.hpp`: `inline constexpr Rational ExponentialArgumentCap { 887, 10 };`
    - Unchanged signatures: `natural_log_enclosure`, `decimal_log_enclosure`, `exponential_enclosure`, `Enclosure`. An exponential's enclosure denominators are now 2^192 and 2^(192+m), not 2^128 and 2^(128+m).

- [ ] **Step 1: Start the worktree from the lane's head.** In the worktree:

```powershell
git fetch --all --quiet
git reset --hard feature/open-issues-numerics
git log --oneline -1
```

Expected: the commit that adds this plan. Never touch `D:\formula-cpp` or another worktree.

- [ ] **Step 2: Write the failing tests in `test/rounded_transcendental_tests.cpp`.**

    1. Add `#include <limits>` and `#include <string_view>` to the includes. Add to the anonymous namespace, after `domainError`:

```cpp
/// The integer whose decimal digits are @p digits: a literal too wide for a built-in integer.
[[nodiscard]] constexpr Rational::Int integer_of(std::string_view digits)
{
    Rational::Int parsed {};
    for (char const each: digits)
        parsed = parsed * 10 + (each - '0');
    return parsed;
}
```

    2. Replace the whole test case `"rounded_transcendental: an exponential too large to hold is Overflow"` with:

```cpp
TEST_CASE("rounded_transcendental: an exponential answers wherever it fits a Rational, and is Overflow past that",
          "[rounded_transcendental]")
{
    // exp 89 is past 88.7, where e^x has long left the largest Rational, 2^127 - 1: refused before the kernel.
    STATIC_REQUIRE(expAt<DecimalPlaces { 6 }, RoundingMode::HalfAwayFromZero>(Rational { 89 }) == overflow);
    // exp 50 = 5184705528587072464087.4533229..., to 6 places.
    CHECK(expAt<DecimalPlaces { 6 }, RoundingMode::HalfAwayFromZero>(Rational { 50 })
          == Rational::from_decimal(integer_of("5184705528587072464087453323"), -6));
    // exp 43.7 = 9.52 * 10^18. Floor to whole 10^18s keeps 9 * 10^18; the nearest modes give 10^19.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 437, 10 }) == Rational { 9'000'000'000'000'000'000 });
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::HalfAwayFromZero>(Rational { 437, 10 })
          == Rational { 10'000'000'000'000'000'000ULL });
    // exp 44 = 1.29 * 10^19 floors to 12 * 10^18.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 44 }) == Rational { 12'000'000'000'000'000'000ULL });
    // exp 43 = 4727839468229346561.474457562744280370...: whole, and to all 18 places.
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 43 })
          == Rational { 4'727'839'468'229'346'561 });
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::Ceiling>(Rational { 43 }) == Rational { 4'727'839'468'229'346'562 });
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { 43 })
          == Rational::from_decimal(integer_of("4727839468229346561474457562744280370"), -18));
    // exp 45 = 34934271057485095348.034797233406099533 41..., to all 18 places: 38 digits, below 2^127.
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { 45 })
          == Rational::from_decimal(integer_of("34934271057485095348034797233406099533"), -18));
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { 45 })
          == Rational::from_decimal(integer_of("34934271057485095348034797233406099534"), -18));
    // exp 88 = 165163625499400185552832979626485876706.9...: whole, 39 digits, below 2^127.
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::Floor>(Rational { 88 })
          == Rational { integer_of("165163625499400185552832979626485876706") });
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::HalfEven>(Rational { 88 })
          == Rational { integer_of("165163625499400185552832979626485876707") });
    // exp 88.5 = 2.7 * 10^38 and exp 88.7 = 3.3 * 10^38 are past 2^127 at every places: through the
    // kernel, and Overflow.
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::Floor>(Rational { 885, 10 }) == overflow);
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 885, 10 }) == overflow);
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 887, 10 }) == overflow);
}
```

    3. In `"rounded_transcendental: absence and failures come first and in order"`, delete these lines: the comment beginning `// An argument whose numerator or denominator does not fit 64 bits`, the three `STATIC_REQUIRE`s of `ln 2^70`, `exp 2^-64` and `log10 2^70`, the comment `// 2^62 and 1/2^62, inside it, answer: ...`, and the two `CHECK`s after it. Keep the `log10 10^30` and `exp -2^70` checks and their comments, but change `// The rule below -43 comes first, whatever the argument's width: exp -2^70 is 0.` to `// The rule below -43 comes first: exp -2^70 is 0 without the kernel.`

    4. Add a new test case after it:

```cpp
TEST_CASE("rounded_transcendental: an argument as wide as a Rational holds is answered", "[rounded_transcendental]")
{
    constexpr Rational::Int largest = std::numeric_limits<Rational::Int>::max(); // 2^127 - 1
    constexpr Rational::Int twoTo126 = Rational::Int { 1 } << 126;
    // Through the kernel, so at run time. ln 2^70 = 48.520302639196171659..., log10 2^70 = 21.072099696478683664...
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { Rational::Int { 1 } << 70 })
          == Rational { 485203, 10000 });
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { Rational::Int { 1 } << 70 })
          == Rational::from_decimal(integer_of("48520302639196171659"), -18));
    CHECK(log10At<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { Rational::Int { 1 } << 70 })
          == Rational { 210721, 10000 });
    // exp 2^-64 = 1 + 5.4 * 10^-20 and exp 2^-62 = 1 + 2.2 * 10^-19: 1, and one unit up under Ceiling.
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1, Rational::Int { 1 } << 64 }) == Rational { 1 });
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { 1, Rational::Int { 1 } << 64 })
          == Rational::from_decimal(1'000'000'000'000'000'001, -18));
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1, Rational::Int { 1 } << 62 }) == Rational { 1 });
    // ln 2^62 = 42.97512..., as before.
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { Rational::Int { 1 } << 62 })
          == Rational { 429751, 10000 });
    // ln (2^127 - 1) = 88.029691931113054295..., and of its reciprocal the negation, which Floor takes down.
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { largest })
          == Rational::from_decimal(integer_of("88029691931113054295"), -18));
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { 1, largest })
          == Rational::from_decimal(-integer_of("88029691931113054296"), -18));
    // log10 (2^127 - 1) = 38.230809449325611792...
    CHECK(log10At<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { largest })
          == Rational::from_decimal(integer_of("38230809449325611792"), -18));
    CHECK(log10At<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { 1, largest })
          == Rational::from_decimal(-integer_of("38230809449325611793"), -18));
    // Two 127-bit integers next to each other: ln((2^126 + 1) / 2^126) = 1.18 * 10^-38. 0 at 18 places,
    // and one unit up under Ceiling.
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(Rational { twoTo126 + 1, twoTo126 }) == Rational {});
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { twoTo126 + 1, twoTo126 })
          == Rational::from_decimal(1, -18));
    // The same two the other way round, a ratio below 1 of two 127-bit integers: ln(2^126 / (2^126 + 1)) =
    // -1.18 * 10^-38. 0 at 18 places, and one unit down under Floor.
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(Rational { twoTo126, twoTo126 + 1 }) == Rational {});
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { twoTo126, twoTo126 + 1 })
          == Rational::from_decimal(-1, -18));
    // exp of -2^127 / (2^127 - 1), whose numerator is the minimum's magnitude: e^-1.000... = 0.367879441171442321595...
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { std::numeric_limits<Rational::Int>::min(), largest })
          == Rational::from_decimal(367'879'441'171'442'321, -18));
    // exp 1/(2^127 - 1) = 1 + 5.9 * 10^-39: 1, and one unit up under Ceiling.
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Floor>(Rational { 1, largest }) == Rational { 1 });
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { 1, largest })
          == Rational::from_decimal(1'000'000'000'000'000'001, -18));
}
```

  Every value above was computed with Python's `decimal` at 150 significant digits, as the file header says. To check one:

```python
from decimal import Decimal, getcontext, ROUND_FLOOR
getcontext().prec = 150
print((Decimal(2**127 - 1).ln() * 10**18).to_integral_value(rounding=ROUND_FLOOR))   # 88029691931113054295
print((Decimal(45).exp() * 10**18).to_integral_value(rounding=ROUND_FLOOR))          # 34934271057485095348034797233406099533
```

- [ ] **Step 3: Write the failing tests in `test/transcendental_tests.cpp`.**

    1. Add `#include <limits>` to the includes. Replace `word_of`, `at_most` and `pinned_by_published_digits` with these. The cross products need 16 limbs: an exponential's 192-bit numerator times a reference's 10^58 passes 384 bits.

```cpp
/// A word of 16 limbs, wide enough for the checks' cross products.
using CheckWord = detail::WideUnsigned<16>;

/// @p narrow, the same value, in a `CheckWord`.
[[nodiscard]] constexpr CheckWord widened_word(Word const& narrow)
{
    std::array<std::uint32_t, 16> limbsCopied {};
    for (std::size_t limbAt = 0; limbAt < detail::KernelLimbs; ++limbAt)
        limbsCopied[limbAt] = narrow.limb(limbAt);
    return CheckWord::from_limbs(limbsCopied);
}

/// The decimal digits @p decimalDigits as a word of @p Limbs limbs.
template <std::size_t Limbs = detail::KernelLimbs>
[[nodiscard]] constexpr detail::WideUnsigned<Limbs> word_of(std::string_view decimalDigits)
{
    detail::WideUnsigned<Limbs> parsed {};
    for (char const each: decimalDigits)
        parsed = *detail::add_small_checked_or_none(*detail::mul_small_checked_or_none(parsed, 10U),
                                                    static_cast<std::uint32_t>(each - '0'));
    return parsed;
}

/// Whether @p left <= @p right, for two signed ratios with positive denominators.
[[nodiscard]] constexpr bool at_most(Ratio const& left, Ratio const& right)
{
    bool const leftNegative = left.negative && !left.numerator.is_zero();
    bool const rightNegative = right.negative && !right.numerator.is_zero();
    if (leftNegative != rightNegative)
        return leftNegative;
    CheckWord const leftCross = *detail::mul_checked_or_none(widened_word(left.numerator), widened_word(right.denominator));
    CheckWord const rightCross = *detail::mul_checked_or_none(widened_word(right.numerator), widened_word(left.denominator));
    return leftNegative ? rightCross <= leftCross : leftCross <= rightCross;
}

/// Whether @p stored is floor(v * 2^@p fractionBits) for the value v below one whose first significant digits
/// are @p published, as many as it has characters: (D - 1) * 2^bits >= stored * 10^digits and
/// (D + 1) * 2^bits <= (stored + 1) * 10^digits. That interval is 2 * 2^bits / 10^digits units wide -- under
/// 0.07 for 128 bits and 40 digits, under 0.013 for 192 bits and 60 -- so it pins the floor.
[[nodiscard]] constexpr bool pinned_by_published_digits(Word const& stored, std::size_t fractionBits, std::string_view published)
{
    CheckWord const digitsValue = word_of<16>(published);
    CheckWord const unit = *detail::shift_left_checked_or_none(CheckWord::from_u64(1), fractionBits);
    CheckWord const tenToDigits = *detail::pow10<16>(published.size());
    CheckWord const storedWide = widened_word(stored);
    return *detail::mul_checked_or_none(storedWide, tenToDigits)
               <= *detail::mul_checked_or_none(*detail::sub_checked_or_none(digitsValue, CheckWord::from_u64(1)), unit)
           && *detail::mul_checked_or_none(*detail::add_small_checked_or_none(digitsValue, 1U), unit)
                  <= *detail::mul_checked_or_none(*detail::add_small_checked_or_none(storedWide, 1U), tenToDigits);
}
```

    2. In the file header comment, extend the published-values sentence with `and ln 2 to 60 digits, for the exponential's 192-bit constant: 0.693147180559945309417232121458176568075500134360255254120680...`.

    3. Widen the reference table. `struct Reference`'s `std::int64_t numerator;` and `std::int64_t denominator;` become `formula::Rational::Int numerator;` and `formula::Rational::Int denominator;`. Add before the table:

```cpp
constexpr Rational::Int largestInt = std::numeric_limits<Rational::Int>::max();  // 2^127 - 1
constexpr Rational::Int smallestInt = std::numeric_limits<Rational::Int>::min(); // -2^127
```

  The table becomes `std::array<Reference, 51>`. Append these twelve rows after the `44, 1` row. Their digits come from the header's Python, run by `$S\laneb\table_sim.py`.

```cpp
    { Transcendental::NaturalLogarithm, Rational::Int { 1 } << 70, 1, false, "4852030263919617165920624850207235976528", 38 },
    { Transcendental::NaturalLogarithm, largestInt, 1, false, "8802969193111305429598847942518842414558", 38 },
    { Transcendental::NaturalLogarithm, 1, largestInt, true, "8802969193111305429598847942518842414558", 38 },
    { Transcendental::NaturalLogarithm, (Rational::Int { 1 } << 126) + 1, Rational::Int { 1 } << 126, false,
      "1175494350822287507968736537222245677811", 77 },
    { Transcendental::DecimalLogarithm, Rational::Int { 1 } << 70, 1, false, "2107209969647868366496172263071451187377", 38 },
    { Transcendental::DecimalLogarithm, largestInt, 1, false, "3823080944932561179214483963001061439955", 38 },
    { Transcendental::DecimalLogarithm, 1, largestInt, true, "3823080944932561179214483963001061439955", 38 },
    { Transcendental::Exponential, 1, Rational::Int { 1 } << 64, false, "1000000000000000000054210108624275221701", 39 },
    { Transcendental::Exponential, 1, largestInt, false, "1000000000000000000000000000000000000005", 39 },
    { Transcendental::Exponential, smallestInt, largestInt, false, "3678794411714423215955237701614608674436", 40 },
    { Transcendental::Exponential, 45, 1, false, "3493427105748509534803479723340609953341", 20 },
    { Transcendental::Exponential, 877, 10, false, "1223562231638072508562388385422483006583", 1 },
```

  e^88 is **not** a row. Its 40 digits leave one decimal, `...706.9`, and that reference interval straddles a whole number, so the reference cannot decide it at 0 places where the kernel can. The rounded test above pins e^88.

    4. In `"every reference value is enclosed and rounds as the reference does in every mode"`:
        - Loop by index, since an `Int128` has no stream output for `INFO`: `for (std::size_t rowAt = 0; rowAt < references.size(); ++rowAt)` with `Reference const& row = references[rowAt];` and `INFO("row " << rowAt);`.
        - Replace the body of `if (!decided.has_value() && referenceDecided.has_value())` with `CHECK(decided.error() == formula::ArithmeticError::Overflow); ++undecided;`.
        - `REQUIRE(compared == 2457)` becomes `REQUIRE(compared == 3213)`, with the comment `// 51 rows, 9 places, 7 modes: a loop over nothing fails here.`
        - `CHECK(undecided == 38)` and its comment become:

```cpp
    // None: an exponential's 192 fraction bits and a logarithm's 128 decide every row the reference's 40
    // digits decide. At 128 fraction bits the exponentials of 43 to 44 at 17 and 18 places were not.
    CHECK(undecided == 0);
```

    5. In `"the stored ln 2 and log10(e) are the published values"`, change the two existing pins to `pinned_by_published_digits(detail::Ln2Lower, 128, "...")` and `pinned_by_published_digits(detail::Log10eLower, 128, "...")`, keeping their digits. Then add:

```cpp
    STATIC_REQUIRE(pinned_by_published_digits(detail::Ln2Lower192, 192,
                                              "693147180559945309417232121458176568075500134360255254120680"));
    STATIC_REQUIRE(detail::Ln2Upper192 == *detail::add_small_checked_or_none(detail::Ln2Lower192, 1U));
    // The exponential's ln 2 begins with the logarithms': floor(L192 / 2^64) = L128.
    STATIC_REQUIRE(detail::shift_right(detail::Ln2Lower192, 64) == detail::Ln2Lower);
```

    6. In `"the kernel re-derives its stored constants from its own series"`, `detail::scaled_quotient(1, 3)` becomes `detail::scaled_quotient<detail::KernelFractionBits>(detail::UInt128::from_u64(1), detail::UInt128::from_u64(3))`, and the same for `(1, 9)`. Its `unit` / `twoTo256` checks are unchanged.

    7. Rename `"transcendental kernel: an enclosure is at most 2^-118 wide"` to `"transcendental kernel: an enclosure is at most 2^-120 wide for a logarithm and 2^-183 of the value for an exponential"`:
        - Its first comment becomes `// Absolute for the logarithms, whose ends share the denominator 2^128: at most 2^8 units apart (2^-120).` and `// Relative for the exponential: upper - lower at most lower / 2^183, since its lower numerator is at least 2^192.`
        - `shift_left_checked_or_none(width, 118)` becomes `shift_left_checked_or_none(width, 183)`.

    8. Add a new test case before `"the kernel answers at compile time"`:

```cpp
TEST_CASE("transcendental kernel: a scaled quotient of 128-bit operands carries the bit its remainder shifts out",
          "[transcendental]")
{
    // The divisor 2^128 - 1 leaves a remainder above 2^127, whose doubling passes 2^128: the shifted-out bit
    // must still count. Checked against the general long division of the 384-bit word, a different route.
    detail::UInt128 const divisor { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } };
    detail::UInt128 const dividend { std::uint64_t { 1 } << 63, 5 };
    for (std::size_t const fractionBits: { std::size_t { 128 }, std::size_t { 192 } })
    {
        INFO("fraction bits " << fractionBits);
        detail::ScaledQuotient const scaled =
            fractionBits == 128 ? detail::scaled_quotient<128>(dividend, divisor) : detail::scaled_quotient<192>(dividend, divisor);
        detail::WideDivision<detail::KernelLimbs> const reference = detail::divmod(
            *detail::shift_left_checked_or_none(Word::from_u128(dividend), fractionBits), Word::from_u128(divisor));
        CHECK(scaled.below == reference.quotient);
        CHECK(scaled.exact == reference.remainder.is_zero());
        CHECK_FALSE(scaled.exact);
    }
    // An exact one: 3 * 2^100 / 2^100 is 3, to the last fraction bit.
    detail::UInt128 const twoTo100 { std::uint64_t { 1 } << 36, 0 };
    detail::ScaledQuotient const three = detail::scaled_quotient<192>(
        detail::UInt128 { std::uint64_t { 3 } << 36, 0 }, twoTo100);
    CHECK(three.exact);
    CHECK(three.below == *detail::shift_left_checked_or_none(Word::from_u64(3), 192));
}
```

  `WideDivision` and `divmod` are in `detail/wide_int.hpp`. Check their exact names there, and use what the header declares.

- [ ] **Step 4: Run the tests and see them fail.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "transcendental"
```

Expected: a **build failure** naming `ExponentialFractionBits`, `Ln2Lower192` or the new `scaled_quotient<...>` form, which do not exist yet. That is the red state.

To see the behavioural failures too, comment out the uses of the new names in `transcendental_tests.cpp` temporarily. `rounded_transcendental_tests.cpp` then compiles, and its new cases fail: `ln 2^70`, `exp 45` and `exp 88` answer `Overflow`. Restore the file afterwards with a plain write, not `git checkout`.

- [ ] **Step 5: Rewrite the kernel's file comment.** In `include/formula-cpp/detail/transcendental.hpp`, replace the `/// @file` block (`:4-56`, from `/// An integer kernel that encloses` through the cost paragraph) with:

```cpp
/// @file
/// An integer kernel that encloses the natural logarithm, the decimal logarithm and the exponential of a
/// rational: two ends between which the value certainly lies, computed in 384-bit fixed point with only
/// integer operations the language defines exactly, so that no floating-point mode enters and the same
/// inputs are meant to give the same bits, at compile time and at run time. It is what the rounded forms
/// (`rounded_transcendental.hpp`) round: when both ends round to the same decimal, that is the rounding of
/// the value.
///
/// ## The algorithm and its error bound
///
/// - **The argument** is a/b in lowest terms, as a `Rational` holds it: |a| <= 2^127 (the magnitude of
///   `Int128`'s minimum) and 0 < b < 2^127. The kernel reads the two magnitudes as `UInt128` and never narrows
///   them.
/// - **Two fixed points.** A value v is an integer V in `WideUnsigned<12>` (384 bits), V = floor(v 2^F): F =
///   128 fraction bits for a logarithm, whose value is below 89, and F = 192 for an exponential, whose value
///   can be as wide as a `Rational` -- up to 2^127 -- and must still be enclosed more narrowly than its last
///   kept unit. Every operation truncates a non-negative value, so a lower bound stays one; each upper bound
///   is the lower bound plus a slack derived here. No `<cmath>`, no floating point, no intrinsics, **and no
///   call of the general `divmod`** (too costly in a constant evaluation over 384 bits): the scaled quotients
///   are long divisions over `UInt128` words, whose whole part is `u128_divmod` -- the compiler's own 128-bit
///   integer where it has one, portable code elsewhere, the same quotient either way -- k is a binary search,
///   and the series' divisors are below 2^32 (`divmod_small`).
/// - **ln(a/b)**, a, b > 0, a != b, so both below 2^127. For a < b, ln(b/a) is taken and negated: the sign
///   comes from a < b and never from a rounding. So let a > b. B = b 2^k with B <= a < 2B, so k <= 126 and
///   a + B < 2^128. With z = (a - B)/(a + B), 0 <= z < 1/3, ln(a/b) = k ln 2 + 2 atanh(z), atanh(z) =
///   sum_{i>=0} z^(2i+1)/(2i+1). Z = floor(z 2^128); Z2 = floor(Z^2 / 2^128); P_0 = Z, P_{i+1} =
///   floor(P_i Z2 / 2^128); S = sum floor(P_i / (2i+1)) until P_i = 0. Then S <= atanh(z) 2^128. Deficits: Z2
///   is below z^2 2^128 by less than 2z + 1 < 2; the deficit d_i of P_i against z^(2i+1) 2^128 obeys d_0 < 1
///   and d_{i+1} < 2 z^(2i+1) + z^2 d_i + 1, so d_i < 2 throughout; each term is short by less than
///   1 + d_i/(2i+1); P_i is 0 by i = 41 (z^83 2^128 < 1), after which the tail is below 2.25/(2i+1). So
///   atanh(z) 2^128 - S < 41 + 2 (1 + 1/3 + ... + 1/81) + 2.25/83 < 47 <= `AtanhSlack` = 64. With
///   L = floor(ln 2 2^128): lower = k L + 2S, upper = k (L + 1) + 2 (S + 64) -- k + 128 <= 254 units of
///   2^-128 apart, under 2^-120, absolute.
/// - **log10** = ln log10(e). With M = floor(log10(e) 2^128): lower = floor(lower_ln M / 2^128),
///   upper = floor(upper_ln (M + 1) / 2^128) + 1, under 254 · 0.44 + 89 + 2 < 203 units apart, since
///   |ln(a/b)| <= ln(2^127) < 89. upper_ln is below 89 · 2^128 + 254 < 2^135 and M + 1 below 2^127, so the
///   widest product, upper_ln (M + 1), is below 2^262.
/// - **exp(x)**, x = a/b != 0, -43 <= x <= 887/10 (the rounded forms answer outside it), in F = 192 bits.
///   X = floor(|x| 2^192), exact or one below. L' = floor(ln 2 2^192). For x > 0, k is the largest integer
///   in [0, 127] with k (L' + 1) <= X (128 ln 2 > 887/10 bounds it), and R = X - k (L' + 1) <= r 2^192 for
///   r = x - k ln 2; for x < 0, m is the smallest in [1, 63] with m L' >= X' (X' = X + 1 when X is inexact;
///   63 ln 2 > 43 bounds it), R = m L' - X' <= r 2^192 for r = m ln 2 - |x|, and exp(x) = 2^-m exp(r).
///   Either way 0 <= R < L' + 1, so r < ln 2 + 2^-184, and r 2^192 - R < 128: one unit for X, and one per
///   multiple of ln 2's unit, k <= 127 or m <= 63. E = sum T_j, T_0 = 2^192,
///   T_j = floor(floor(T_{j-1} R / 2^192) / j), until T_j = 0 (by j = 43 for r < 0.7; `TaylorTermLimit` = 50
///   refuses a longer one), so E <= exp(R 2^-192) 2^192 <= exp(r) 2^192. Each T_j is short by
///   e_j < e_{j-1} r / j + 1 < 2, and the tail after the last term is below 3, so
///   exp(R 2^-192) 2^192 - E < 2 · 50 + 3; the 128 units of r add less than 2 · 1.0001 · 128 < 257. So
///   exp(r) 2^192 < E + 360 <= E + `ExponentialSlack` = 512: lower = E 2^k / 2^192,
///   upper = (E + 512) 2^k / 2^192 (for x < 0, denominator 2^(192+m)). Since E >= 2^192, the ends are at
///   most 2^-183 of the value apart: at the largest result a `Rational` holds, 2^127 last kept units, that is
///   2^-56 of one unit. E < 2^193, so the widest numerator, (E + 512) 2^127, is below 2^321;
///   `decide_rounding`'s scaling by up to 10^18 keeps it below 2^381.
/// - **Undecided.** When the two ends round differently, `decide_rounding` answers `Overflow`: the rounding
///   needs more bits than the kernel holds. The rounded decimal exists, so `Inexact` would be wrong, and a new
///   error enumerator would break `describe()` and consumers' switches.
///
/// ## What it costs
///
/// Measured on cl 19.51.36257, whose default constant-evaluation budget measured about 1 049 000 steps: one
/// enclosure costs between <LOW> and <HIGH> steps (ln 3: <N1>; ln (2^127 - 1) / 2^126: <N2>; log10 7: <N3>;
/// exp 1: <N4>; exp -43: <N5>; exp 88: <N6>), and a whole rounding of log10 2 to 3 places, with
/// `decide_rounding`, about <N7>.
```

  The `<…>` figures in the cost paragraph are the measurements Step 9 takes. Write the measured numbers in their place, rounded to hundreds as the old comment did. No `<` may remain.

- [ ] **Step 6: The constants, `kernel_word` and `scaled_quotient`.** In `include/formula-cpp/detail/transcendental.hpp`:

    1. After `inline constexpr std::size_t KernelFractionBits = 128;`, add:

```cpp
/// How many of an exponential's fixed-point bits are fraction: more than a logarithm's, so that an
/// exponential as wide as a `Rational` holds is still enclosed more narrowly than its last kept unit -- see
/// the file comment.
inline constexpr std::size_t ExponentialFractionBits = 192;
```

    2. Change the `/// How many of a fixed-point value's bits are fraction.` comment on `KernelFractionBits` to `/// How many of a logarithm's fixed-point bits are fraction.`

    3. Change `TaylorTermLimit`:

```cpp
/// More terms than the exponential's series takes for any r below 0.7 at 192 fraction bits (it ends by the
/// 43rd); reaching it is refused.
inline constexpr std::uint32_t TaylorTermLimit = 50;
```

    4. After the existing `kernel_word`, add the three-word overload:

```cpp
/// @p topWord * 2^128 + @p middleWord * 2^64 + @p bottomWord as a kernel word. 192 bits in 384: no step can
/// overflow.
[[nodiscard]] constexpr KernelWord kernel_word(std::uint64_t topWord, std::uint64_t middleWord, std::uint64_t bottomWord) noexcept
{
    return *add_checked_or_none(*shift_left_checked_or_none(kernel_word(topWord, middleWord), 64),
                                KernelWord::from_u64(bottomWord));
}
```

    5. After `Log10eUpper`, add:

```cpp
/// One in an exponential's fixed point, 2^192.
inline constexpr KernelWord ExponentialOne =
    *shift_left_checked_or_none(KernelWord::from_u64(1), ExponentialFractionBits);
/// floor(ln 2 * 2^192), the exponential's ln 2: ln 2 lies in [Ln2Lower192, Ln2Upper192] * 2^-192. Checked
/// against its published digits, and against `Ln2Lower`, in `transcendental_tests.cpp`.
inline constexpr KernelWord Ln2Lower192 =
    kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6AFULL, 0x40F3'4326'7298'B62DULL);
/// Ln2Lower192 + 1.
inline constexpr KernelWord Ln2Upper192 =
    kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6AFULL, 0x40F3'4326'7298'B62EULL);
```

    6. Replace the `ScaledQuotient` comment and the whole of `scaled_quotient` (`:114-147`):

```cpp
/// floor(dividend * 2^F / divisor), and whether that is exact.
struct ScaledQuotient
{
    /// The quotient, rounded down.
    KernelWord below;
    /// Whether nothing was rounded away.
    bool exact;
};

/// @p dividend * 2^FractionBits / @p divisor, by long division over `UInt128` words: the whole part from
/// `u128_divmod`, then the fraction bits one at a time, gathered 64 to a word. The running remainder stays
/// below the divisor, which is below 2^128; doubled, it can pass 2^128 only when it is then above the
/// divisor, so the bit it shifts out is kept as a carry and the subtraction, taken modulo 2^128, is exact.
/// 128 or 192 fraction bits: the kernel's two fixed points. The whole part is below 2^128, so with 192
/// fraction bits the quotient stays below 2^320: no step leaves the word. @pre @p divisor != 0.
template <std::size_t FractionBits>
    requires(FractionBits % 64 == 0 && FractionBits <= ExponentialFractionBits)
[[nodiscard]] constexpr ScaledQuotient scaled_quotient(UInt128 dividend, UInt128 divisor) noexcept
{
    UInt128Division const split = u128_divmod(dividend, divisor);
    UInt128 remaining = split.remainder;
    KernelWord quotientSoFar = KernelWord::from_u128(split.quotient);
    for (std::size_t wordAt = 0; wordAt < FractionBits / 64; ++wordAt)
    {
        std::uint64_t fractionWord = 0;
        for (int bitAt = 0; bitAt < 64; ++bitAt)
        {
            bool const carriedOut = (remaining.highWord >> 63) != 0;
            remaining = portable::shift_left(remaining, 1);
            fractionWord <<= 1;
            if (carriedOut || !(remaining < divisor))
            {
                remaining = u128_sub(remaining, divisor);
                fractionWord |= 1U;
            }
        }
        quotientSoFar =
            *add_checked_or_none(*shift_left_checked_or_none(quotientSoFar, 64), KernelWord::from_u64(fractionWord));
    }
    return { quotientSoFar, remaining.is_zero() };
}
```

  Keep `atanh_series_lower` as it is. In `exponential_series_lower`, change its comment to `/// A lower bound of exp(r) * 2^192 for r = @p fixedArgument * 2^-192 below 0.7 -- see the file comment; ...`. In its body, `KernelOne` becomes `ExponentialOne` (both uses) and `KernelFractionBits` becomes `ExponentialFractionBits`.

- [ ] **Step 7: The logarithm's reduction over 128 bits.** Replace `natural_log_magnitude` from its first line to the `atanh_series_lower(...)` call:

```cpp
/// The enclosure of |ln(@p positive)| -- see the file comment. @pre @p positive > 0 and != 1.
[[nodiscard]] constexpr std::optional<LogarithmMagnitude> natural_log_magnitude(Rational positive) noexcept
{
    // A positive Rational's numerator and its denominator are both below 2^127.
    UInt128 larger = wide_magnitude(positive.numerator());
    UInt128 smaller = wide_magnitude(positive.denominator());
    LogarithmSign const logarithmSign = larger < smaller ? LogarithmSign::Negative : LogarithmSign::Positive;
    if (logarithmSign == LogarithmSign::Negative)
        std::swap(larger, smaller);
    // B = smaller * 2^doublings <= larger < 2B. Both are below 2^127, so doublings <= 126 and the shift
    // stays in 128 bits.
    int doublings = larger.bit_width() - smaller.bit_width();
    if (larger < portable::shift_left(smaller, doublings))
        --doublings;
    UInt128 const base = portable::shift_left(smaller, doublings);
    // z = (a - B) / (a + B), and a + B < 2^128.
    std::optional<KernelWord> const series = atanh_series_lower(
        scaled_quotient<KernelFractionBits>(u128_sub(larger, base), u128_add(larger, base)).below);
```

  The rest of the function, from `if (!series)` on, is unchanged: `multiples` is still `static_cast<std::uint32_t>(doublings)`.

- [ ] **Step 8: The exponential in 192 bits.** Replace `exponential_enclosure` whole:

```cpp
/// exp(@p argument), enclosed -- see the file comment. @pre @p argument != 0 and -43 <= @p argument <= 887/10.
[[nodiscard]] constexpr std::optional<Enclosure> exponential_enclosure(Rational argument) noexcept
{
    bool const negative = argument.sign() < 0;
    // |a| <= 2^127, the minimum's magnitude, and 0 < b < 2^127: both read whole.
    ScaledQuotient const fixedMagnitude = scaled_quotient<ExponentialFractionBits>(
        wide_magnitude(argument.numerator()), wide_magnitude(argument.denominator()));
    std::optional<KernelWord> remainderBelow;
    std::uint32_t shifts = 0;
    if (!negative)
    {
        // The largest k in [0, 127] with k (L' + 1) <= X: 128 ln 2 > 887/10 >= x.
        std::uint32_t below = 0;
        std::uint32_t above = 128;
        while (below + 1 < above)
        {
            std::uint32_t const middle = (below + above) / 2;
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper192, middle);
            if (!multiple)
                return std::nullopt;
            if (*multiple <= fixedMagnitude.below)
                below = middle;
            else
                above = middle;
        }
        std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper192, below);
        remainderBelow = multiple ? sub_checked_or_none(fixedMagnitude.below, *multiple) : std::nullopt;
        shifts = below;
    }
    else
    {
        // The smallest m in [1, 63] with m L' >= X' (X rounded up): 63 ln 2 > 43 >= |x|.
        std::optional<KernelWord> const roundedUp = fixedMagnitude.exact
                                                        ? std::optional<KernelWord> { fixedMagnitude.below }
                                                        : add_small_checked_or_none(fixedMagnitude.below, 1U);
        if (!roundedUp)
            return std::nullopt;
        std::uint32_t below = 0;
        std::uint32_t above = 63;
        while (below + 1 < above)
        {
            std::uint32_t const middle = (below + above) / 2;
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower192, middle);
            if (!multiple)
                return std::nullopt;
            if (*multiple >= *roundedUp)
                above = middle;
            else
                below = middle;
        }
        std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower192, above);
        remainderBelow = multiple ? sub_checked_or_none(*multiple, *roundedUp) : std::nullopt;
        shifts = above;
    }
    if (!remainderBelow)
        return std::nullopt;
    std::optional<KernelWord> const series = exponential_series_lower(*remainderBelow);
    std::optional<KernelWord> const slacked = series ? add_small_checked_or_none(*series, ExponentialSlack) : std::nullopt;
    if (!series || !slacked)
        return std::nullopt;
    if (!negative)
    {
        std::optional<KernelWord> const nearer = shift_left_checked_or_none(*series, shifts);
        std::optional<KernelWord> const farther = shift_left_checked_or_none(*slacked, shifts);
        if (!nearer || !farther)
            return std::nullopt;
        return Enclosure { .lower = { .negative = false, .numerator = *nearer, .denominator = ExponentialOne },
                           .upper = { .negative = false, .numerator = *farther, .denominator = ExponentialOne } };
    }
    std::optional<KernelWord> const denominatorPower = shift_left_checked_or_none(ExponentialOne, shifts);
    if (!denominatorPower)
        return std::nullopt;
    return Enclosure { .lower = { .negative = false, .numerator = *series, .denominator = *denominatorPower },
                       .upper = { .negative = false, .numerator = *slacked, .denominator = *denominatorPower } };
}
```

  Then:
    - If `<bit>` has no remaining use in the header, remove its include. `std::bit_width` was used only by the old reduction.
    - `narrow_to_int64` is no longer called here. Leave it in `checked_int.hpp`, since `band.hpp` calls it. In that file's comment (`:13-17`), drop `, or to the transcendental kernel's 64-bit words (\`narrow_to_int64\`)` from the 64-bit bullet. The bullet then ends `(a rounded root's unit scale, a trace's unit quotient).`

  In `include/formula-cpp/rounded_transcendental.hpp`, inside `namespace detail`, before `rounded_transcendental`, add:

```cpp
    /// The largest argument the exponential's kernel takes, 88.7: below 128 ln 2 = 88.72..., which bounds
    /// its reduction (`detail/transcendental.hpp`), and above ln(2^127) = 88.03..., past which e^x leaves the
    /// largest `Rational` at every places. Above it the answer is `Overflow` without the kernel.
    inline constexpr Rational ExponentialArgumentCap { 887, 10 };
```

  In `rounded_transcendental`, `if (argument > Rational { 44 })` becomes `if (argument > ExponentialArgumentCap)`. Replace its doc comment (`:41-52`) with:

```cpp
    /// @p F of @p argument, rounded to @p places decimal places under @p roundingMode -- the correctly
    /// rounded decimal of the true value, rational or not. The decision, in order: a logarithm of zero or
    /// below is `DomainError`; places outside -18...18 are `Overflow`, as for `checked_round`; a special
    /// point -- the only values that can tie -- goes to `checked_round`; the exponential of more than 887/10
    /// is `Overflow` (`ExponentialArgumentCap`: no such value fits a `Rational`), and of less than -43 is
    /// below a quarter of the last kept unit at any places accepted, so 0, or one unit under `Ceiling` and
    /// `AwayFromZero`; everything else is the kernel's enclosure, rounded by `decide_rounding`, which
    /// answers `Overflow` when the kept integer does not fit -- e^88.5, at every places -- and when the two
    /// ends round differently. The kernel takes every argument a `Rational` holds. The special points are
    /// `RepFunctions<Rational>`'s, through `transcendental_of`: their value, and `Inexact` elsewhere.
```

  Replace the three factories' comments:

```cpp
/// The natural logarithm of `operand`, a dimensionless expression, rounded exactly to `Places` decimal
/// places: `rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Count> / var<InitialCount>)`.
///
/// The integer kernel (`detail/transcendental.hpp`) takes every argument a `Rational` holds: ln (2^127 - 1)
/// is 88.029691931113054295 at 18 places, under `Floor`. A rounding the kernel cannot decide is `Overflow`.
```

```cpp
/// The decimal logarithm of `operand`, rounded exactly to `Places` decimal places.
///
/// As for `rounded_ln`, every argument a `Rational` holds; a power of ten, 10^-38 up to 10^38, is answered
/// exactly before the kernel is asked: log10 10^30 is 30.
```

```cpp
/// The exponential of `operand`, rounded exactly to `Places` decimal places.
///
/// Checked in this order: an argument above 887/10 is `Overflow`, since e^x is then past the largest
/// `Rational`; one below -43 is 0, or one last kept unit under `Ceiling` and `AwayFromZero`, whatever its
/// width, so exp(-2^70) is 0; any other goes to the integer kernel (`detail/transcendental.hpp`), which
/// answers wherever the result fits the declared places -- e^45 to 18 places, e^88 to whole units -- and is
/// `Overflow` where it does not, as e^88.5 is at every places. A rounding the kernel cannot decide is
/// `Overflow` too, never a guess.
```

- [ ] **Step 9: Run the kernel tests to green, then measure the constant-evaluation cost.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "transcendental"
```

Expected: `ALL OK`. Prove from ctest's count that both files' cases ran: every `[transcendental]` and `[rounded_transcendental]` case, including the two new ones.

  If a table row mismatches, compare it against `$S\laneb\table_sim.py`, which models this exact kernel, before touching a slack. The slacks come from the derivation, not from the table.

  Then measure on cl. In `$S\probe` (outside the worktree), write `probe.cpp`:

```cpp
#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <limits>
constexpr bool probe()
{
    using formula::Rational;
#if PROBE_CASE == 1
    return formula::detail::natural_log_enclosure(Rational { 3 }).has_value();
#elif PROBE_CASE == 2
    return formula::detail::natural_log_enclosure(
               Rational { std::numeric_limits<Rational::Int>::max(), Rational::Int { 1 } << 126 })
        .has_value();
#elif PROBE_CASE == 3
    return formula::detail::decimal_log_enclosure(Rational { 7 }).has_value();
#elif PROBE_CASE == 4
    return formula::detail::exponential_enclosure(Rational { 1 }).has_value();
#elif PROBE_CASE == 5
    return formula::detail::exponential_enclosure(Rational { -43 }).has_value();
#elif PROBE_CASE == 6
    return formula::detail::exponential_enclosure(Rational { 88 }).has_value();
#else
    // The compile-time smoke test's whole rounding.
    auto const enclosure = formula::detail::decimal_log_enclosure(Rational { 2 });
    return formula::detail::decide_rounding(enclosure->lower, enclosure->upper, formula::DecimalPlaces { 3 },
                                            formula::RoundingMode::HalfEven)
           == Rational { 301, 1000 };
#endif
}
static_assert(probe());
int main() { return 0; }
```

  In a VS developer shell (`& $S\vsdev.ps1`, or the `Launch-VsDevShell.ps1` line from `cl.ps1`), bisect the smallest `/constexpr:steps` for which each case compiles:

```powershell
foreach ($case in 1..7) {
    $low = 1000; $high = 2000000
    while ($low + 100 -lt $high) {
        $middle = [int](($low + $high) / 2)
        cl /nologo /std:c++latest /permissive- /c /Zs /I"$T\include" /DPROBE_CASE=$case "/constexpr:steps$middle" "$S\probe\probe.cpp" *> $null
        if ($LASTEXITCODE -eq 0) { $high = $middle } else { $low = $middle }
    }
    "case ${case}: $high steps"
}
cl /? 2>&1 | Select-String "Version"
```

  Record the seven counts and the cl version in the file comment's cost paragraph (Step 5) and in the report.
    - **Target:** one enclosure (cases 1-6) at most 100 000 steps. Any case above 100 000 stops the task: report it to the controller with the counts.
    - **The rule that binds:** the compile-time smoke test (case 7, and `"transcendental kernel: the kernel answers at compile time"`) must compile under every compiler's **default** budget. If it ever fails, make the kernel cheaper. Never raise a budget: a consumer cannot be asked to.
    - Update that test's comment to the measured figure: `// ... measured at about <N7> steps on cl 19.51.36257, against a default budget of about 1 049 000.`
    - If case 7 moved by more than 10 %, update the `rounded_transcendental_tests.cpp` header's "about a quarter" to match the new figure.

- [ ] **Step 10: The other three compilers.** The compile-time smoke test compiles inside `formula-cpp-tests`, so build it on clang-cl, then build and test under WSL, where `UInt128` takes its native path:

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Preset clangcl-debug -Target formula-cpp-tests -NoTest
wsl bash $SW/posix-matrix.sh --tree $TW --presets "gcc-release clang-debug"
```

Expected: `BUILD OK (clangcl-debug)`, then `MATRIX OK`. Both g++-14 and clang++ pass every test, so the native and portable `u128_divmod` give the same kernel bits.

- [ ] **Step 11: The documentation.**

    1. `docs/expressions.md`: replace the paragraph from `The places are the method's own, and at most 18;` through `as \`rounded<...>(sqrt(x))\` does.` with:

```markdown
The places are the method's own, and at most 18; the result must fit a
`Rational` there, which any logarithm does: the `log10` of 10^18 - 1 is
reported to all 18 places. The integer kernel takes every argument a
`Rational` holds: `ln` of 2^127 - 1 is 88.029691931113054295 at 18 places,
under `Floor`. `exp` of more than 88.7 is `Overflow`, since e^x is then past
the largest `Rational`; below that it answers wherever the result fits the
declared places -- e^45 to 18 places, e^88 to whole units -- and is
`Overflow` where it does not, as e^88.5 is at every places. Two kinds of
argument never reach the kernel: a power of ten, 10^-38 up to 10^38, is
answered exactly, so `log10` of 10^30 is 30; and `exp` of less than -43 is
0, or one unit under `Ceiling` and `AwayFromZero`, whatever its width.
Only ln 1, log10 10^k and
exp 0 can tie, and the mode breaks the tie as `rounded<>` does: `log10` of
10^15 at -1 places is 20, 10 or 20 under `HalfAwayFromZero`,
`HalfTowardZero` and `HalfEven`. A rounding the computation cannot decide --
a value within its width of a rounding boundary, under 2^-120 for a
logarithm and under 2^-183 of the value for `exp` -- is `Overflow`, never a
guess. `rounded<...>(ln(x))` is not
`rounded_ln`: the plain logarithm fails before the rounding sees a value, as
`rounded<...>(sqrt(x))` does.
```

    2. `docs/numeric-headroom.md`: under "What this does not decide", delete the whole bullet that begins `- **The logarithm and exponential kernel** (\`detail/transcendental.hpp\`)` and ends `width.` Leave the bullets before and after it as they are.

    3. Check for any other statement of the old limit:

```powershell
git -C $T grep -nE "fit 64 bits|more than 44|above 44|2\^-118|exp 44" -- docs/*.md README.md include
```

  Each hit about the transcendental kernel is rewritten to the new limits. Hits about `Band`, `Breakpoint`, `Unit` fields or the `_r` literal are other 64-bit limits, and stay. Expected: no kernel hit is left.

    4. `CHANGELOG.md`: append to `### Changed` under `## [Unreleased]`:

```markdown
- **`rounded_ln`, `rounded_log10` and `rounded_exp` take every argument a `Rational` holds.** Their integer kernel
  narrowed the argument's numerator and denominator to 64 bits and answered `Overflow` beyond; `ln` of 2^70 is now
  48.5203 at 4 places. `rounded_exp` answers up to 88.7, past which no value fits a `Rational`, wherever the result
  fits the declared places: e^45 to 18 places, e^88 to whole units. The exponential is computed with 192 fraction
  bits, so a result as wide as a `Rational` is still decided: e^43 to 18 places, `Overflow` before though the result
  fits, now answers.
```

- [ ] **Step 12: Full verification.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."
```

Expected: `ALL OK`, with the total 2 above the previous Lane B head's. The new cases are `"transcendental kernel: a scaled quotient of 128-bit operands carries the bit its remainder shifts out"` and `"rounded_transcendental: an argument as wide as a Rational holds is answered"`; the renamed cases keep the count. `docs.numeric-headroom` and every `docs.*` test pass. The census does not see the kernel, and no census fixture rounds a logarithm or an exponential. If `docs.numeric-headroom` fails anyway, stop and report the diff.

- [ ] **Step 13: Commit, in two commits.**

```text
feat: let the logarithm and exponential kernel take 128-bit arguments

The kernel narrowed an argument's numerator and denominator to 64 bits
and refused wider ones with Overflow, though a Rational holds 128. It
now reads both as 128-bit magnitudes: the scaled quotient is a long
division over 128-bit words, a logarithm's reduction reaches 2^126, and
the exponential's reaches 2^127 under a cap of 88.7, past which no value
fits a Rational. The exponential computes with 192 fraction bits, so
that a result as wide as a Rational is still enclosed within a fraction
of its last kept unit: e^45 at 18 places and e^88 whole now answer.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  The first commit stages `include/formula-cpp/detail/transcendental.hpp`, `include/formula-cpp/rounded_transcendental.hpp`, `include/formula-cpp/detail/checked_int.hpp`, `test/transcendental_tests.cpp` and `test/rounded_transcendental_tests.cpp`. The second stages `docs/expressions.md`, `docs/numeric-headroom.md` and `CHANGELOG.md`:

```text
docs: state the logarithm and exponential limits of a 128-bit kernel

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  Write each message to a file in `$S` and commit with `git commit -F <file>`.

---

### Task 6: The guides' bit widths pinned by tests (#19)

Two figures in the guides are measured by nothing:

- The headroom page's widest intermediates of the exact curve fit, "68 bits … up to 249 of the 256". The census gains a width hook in `LinearLeastSquares::compute_exact`, and the page's least-squares table gains a generated column that replaces the sentence.
- The opaque guide's coefficient widths, 65/73, 93/91 and 130/130. A test computes them with the library's exact fit, and the guide cites it.

Both figures are right today; the models in `$S\laneb` reproduce them.

**Files:**
- Modify: `include/formula-cpp/detail/checked_int.hpp` (the census section, `:21-104`): `CensusRole::Wide`, `census_record_width`, `census_note_width`, `FORMULA_CENSUS_NOTE_WIDTH`, and the file comment.
- Modify: `include/formula-cpp/least_squares.hpp`: `LinearLeastSquares::compute_exact` (`:239-317`) notes every wide integer it forms. Include `<formula-cpp/detail/checked_int.hpp>`.
- Modify: `support/census_tally.hpp`, `support/census_tally.cpp`.
- Modify: `test/overflow_census_tests.cpp`: `Used`, `census_of`, `FitScan`, `scan_rounded_fit`, and the least-squares emission and checks (`:948-1018`).
- Regenerate: `docs/numeric-headroom.md`'s `census:least-squares` table. Hand-edit the prose above it (`:331-340`).
- Modify: `test/least_squares_tests.cpp` (`fifty_readings`, `:1282-1301`, and a new test case); `docs/opaque-and-retry.md:325-328`; `CHANGELOG.md`.

**Interfaces:**
- Consumes: Task 5's head (this task fast-forwards from it). Nothing from Task 5's code.
- Produces, census builds only (`FORMULA_OVERFLOW_CENSUS`):
    - `enum class CensusRole` gains `Wide`;
    - `void formula::detail::census_record_width(CensusRole role, std::size_t bitsUsed) noexcept;`, declared by the library and defined in `support/census_tally.cpp`;
    - `constexpr void census_note_width(CensusRole, std::size_t) noexcept;`
    - `FORMULA_CENSUS_NOTE_WIDTH(role, bitsUsed)`, which expands to nothing, its arguments unevaluated, outside a census build;
    - `formula_census::bits_used(CensusRole::Wide)`: the most bits any wide intermediate used since the last reset.

- [ ] **Step 1: Start the worktree from the lane's head.**

```powershell
git fetch --all --quiet
git reset --hard feature/open-issues-numerics
git log --oneline -2
```

Expected: Task 5's two commits on top. Never touch `D:\formula-cpp` or another worktree.

- [ ] **Step 2: Write the failing census checks.** In `test/overflow_census_tests.cpp`:

    1. `Used` gains a member after `unsignedBits`, with a comment: `/// The most bits a wide intermediate used (`CensusRole::Wide`), 0 when none was formed.` and `int wideBits;`. `census_of` reads it as its fifth initializer: `formula_census::bits_used(CensusRole::Wide)`. Grep the file for any other `Used {` and give each the fifth value. `headroom()` is unchanged: wide integers are not `Rational`'s.
    2. `FitScan` gains:

```cpp
    /// The most bits a wide intermediate of the fit used, over the sizes that answered; empty for a route
    /// that computes in `Rational` and forms none.
    std::optional<int> widestWide;
```

  `row()` appends one cell, `" | " + (widestWide.has_value() ? std::to_string(*widestWide) : std::string { "--" })`, before the closing `" |"`. Add `#include <optional>` to the includes if it is absent.
    3. In `scan_rounded_fit`, the `else` branch becomes:

```cpp
        else
        {
            found.leastHeadroom = std::min(found.leastHeadroom, used.headroom());
            found.widestWide = std::max(found.widestWide.value_or(0), used.wideBits);
        }
```

    4. In `"census: least squares over 2 to 128 points"`, the two header lines become:

```cpp
    std::string const wideHeader =
        "widest fit intermediate (of " + std::to_string(formula::LinearLeastSquares::exact_limbs * 32) + " bits)";
    emit("least-squares",
         "| data (invented) | sizes that overflow | first to overflow | least headroom otherwise | " + wideHeader + " |");
    emit("least-squares", "|---|---|---|---|---|");
```

  After `CHECK(rounded_fit_node_overflows<58>(distinct_denominators_point));`, add:

```cpp
    // How close the exact fit came to its width, at the sizes that answered: readings at 3 dp use 68 of its
    // bits, a different denominator on every point 249. The Rational routes form no wide integer.
    CHECK(roundedThree.widestWide == 68);
    CHECK(roundedDistinct.widestWide == 249);
    CHECK_FALSE(oneDecimal.widestWide.has_value());
    CHECK(formula::LinearLeastSquares::exact_limbs * 32 == 256);
```

- [ ] **Step 3: Write the widths test for the opaque guide.** In `test/least_squares_tests.cpp`, replace `fifty_readings` and its comment (`:1282-1301`) with a column helper and the same environment built from it:

```cpp
// Fifty readings at four decimals: t_k = k + 1 + (7919 k mod 997) / 10^4 s,
// L_k = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm. At eight, each
// gains (1237 k mod 10^4) / 10^8 s and (4111 k mod 10^4) / 10^8 mm.
// Reference values computed with Python's fractions.
struct FiftyColumns
{
    std::array<formula::Rational, 50> seconds;
    std::array<formula::Rational, 50> millimetres;
};

[[nodiscard]] FiftyColumns fifty_columns(std::int64_t const morePlaces)
{
    FiftyColumns made {};
    for (std::size_t at = 0; at < 50; ++at)
    {
        auto const position = static_cast<std::int64_t>(at);
        made.seconds[at] =
            rat((10'000 * (position + 1) + (7919 * position) % 997) * morePlaces + (1237 * position) % morePlaces,
                10'000 * morePlaces);
        made.millimetres[at] = rat((24'100'000 + 31'700 * position + (3217 * position) % 1009 - 504) * morePlaces
                                       + (4111 * position) % morePlaces,
                                   10'000 * morePlaces);
    }
    return made;
}

[[nodiscard]] auto fifty_readings(std::int64_t const morePlaces = 1)
{
    FiftyColumns const made = fifty_columns(morePlaces);
    return formula::environment(*formula::MeasuredObservations<Elapsed, 64>::from(made.seconds),
                                *formula::MeasuredObservations<Length, 64>::from(made.millimetres));
}
```

  After the test case `"a line through fifty readings at eight decimals overflows exactly and answers rounded"`, add:

```cpp
TEST_CASE("a line through fifty readings at eight decimals: how wide each exact output is",
          "[least-squares][observations]")
{
    // The widths docs/opaque-and-retry.md quotes. In coherent units -- seconds and metres -- as the fit sees
    // them, each output reduced to lowest terms as a Rational would hold it: the intercept and the slope fit
    // 127 bits, R^2 does not, so the call answers Overflow for all its outputs.
    FiftyColumns const atEight = fifty_columns(10'000);
    std::array<formula::Rational, 50> metres {};
    for (std::size_t at = 0; at < metres.size(); ++at)
        metres[at] = formula::Rational { atEight.millimetres[at].numerator(), atEight.millimetres[at].denominator() * 1000 };
    auto const exact = formula::LinearLeastSquaresOfObservations::compute_exact(
        std::span<formula::Rational const> { atEight.seconds }, std::span<formula::Rational const> { metres });
    REQUIRE(exact.has_value());
    auto const bitsOf = [&exact](std::size_t outputAt) {
        auto const lowest = formula::detail::reduced((*exact)[outputAt]);
        return std::array<std::size_t, 2> { lowest.numerator.bit_length(), lowest.denominator.bit_length() };
    };
    // intercept, slope, r squared: numerator bits, then denominator bits.
    CHECK(bitsOf(0) == std::array<std::size_t, 2> { 93, 91 });
    CHECK(bitsOf(1) == std::array<std::size_t, 2> { 65, 73 });
    CHECK(bitsOf(2) == std::array<std::size_t, 2> { 130, 130 });
}
```

  This test passes as soon as it compiles: it pins today's widths, which `$S\laneb\fifty_widths.py` computes with Python's fractions as 93/91, 65/73 and 130/130. If a figure differs, the test's is the one the guide states (Step 8). Report the difference.

- [ ] **Step 4: Run and see the census checks fail.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "census|fifty readings"
```

Expected: a build failure, since `CensusRole::Wide` does not exist yet.

- [ ] **Step 5: The width hook.** In `include/formula-cpp/detail/checked_int.hpp`, inside `#if defined(FORMULA_OVERFLOW_CENSUS)`:

    1. The enum and its comment become:

```cpp
/// What an integer the overflow census is told of was: a numerator or a
/// denominator handed to `Rational::make`, any other signed intermediate, an
/// unsigned one (`rounded_sqrt`'s, which has 128 bits to use), or an
/// intermediate of a computation in wide integers (`detail/wide_int.hpp`),
/// told as the bits it used.
enum class CensusRole : std::uint8_t
{
    Numerator,
    Denominator,
    Intermediate,
    Unsigned,
    Wide,
};
```

    2. After the `std::uint64_t` overload of `census_note`, add:

```cpp
/// Told how many bits an intermediate of a computation in wide integers used,
/// formed at run time: how near it came to its width. Declared here and
/// defined only by the census program, never by the library.
void census_record_width(CensusRole role, std::size_t bitsUsed) noexcept;

/// Tells the overflow census that a wide intermediate used @p bitsUsed bits,
/// unless this is a constant evaluation.
constexpr void census_note_width(CensusRole role, std::size_t bitsUsed) noexcept
{
    if !consteval
    {
        census_record_width(role, bitsUsed);
    }
}
```

    3. After the census definition of `FORMULA_CENSUS_NOTE`, add:

```cpp
    /// Tells the overflow census that a wide intermediate in @p role (a
    /// `CensusRole` enumerator's name) used @p bitsUsed bits.
    #define FORMULA_CENSUS_NOTE_WIDTH(role, bitsUsed) \
        ::formula::detail::census_note_width(::formula::detail::CensusRole::role, (bitsUsed))
```

    4. In the `#else` branch, after the empty `FORMULA_CENSUS_NOTE`, add:

```cpp
    /// Nothing: this is not a census build. The arguments are not evaluated.
    #define FORMULA_CENSUS_NOTE_WIDTH(role, bitsUsed) static_cast<void>(0)
```

    5. Add `#include <cstddef>` if absent. In the file comment's census paragraph, after `` `Rational::Int` holds real formulas use (`docs/numeric-headroom.md`). ``, add: `` A computation in wide integers that reports itself -- the exact curve fit, `LinearLeastSquares::compute_exact` -- tells it the bits each of its intermediates used, through `FORMULA_CENSUS_NOTE_WIDTH`. ``

  `support/census_tally.hpp`: change `bits_used`'s comment to `/// The bits the largest magnitude of @p role used since the last `reset` -- 0 when none was seen. For `CensusRole::Wide`, the most bits a wide intermediate used. The largest `Rational::Int` uses all but its sign bit; `rounded_sqrt`'s unsigned intermediates may use every bit.`

  `support/census_tally.cpp`:

```cpp
/// The most bits a wide intermediate used since the last reset.
std::size_t widestWideBits = 0;
```

  goes after `largestSeen`, in the anonymous namespace. In `namespace formula::detail`, after `census_record`:

```cpp
void census_record_width(CensusRole role, std::size_t bitsUsed) noexcept
{
    if (role == CensusRole::Wide && widestWideBits < bitsUsed)
        widestWideBits = bitsUsed;
}
```

  `bits_used` begins with `if (role == formula::detail::CensusRole::Wide) return static_cast<int>(widestWideBits);`. `largestSeen` keeps four slots, since `census_record` is never told of `Wide`. `reset` also sets `widestWideBits = 0;`. `signed_bits_used` is unchanged.

  `support/census_report.cpp` is unchanged: the examples table does not report wide integers.

- [ ] **Step 6: Note the fit's wide integers.** In `include/formula-cpp/least_squares.hpp`, add `#include <formula-cpp/detail/checked_int.hpp>` with the other `detail` includes. In `LinearLeastSquares::compute_exact`, add the notes below. Each goes right after the check that proves its value present, so no empty `optional` is ever read:

```cpp
        if (!pointScale || !valueScale)
            return std::unexpected { ArithmeticError::Overflow };
        FORMULA_CENSUS_NOTE_WIDTH(Wide, pointScale->bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, valueScale->bit_length());
```

  In the loop, after `if (!withPoint || !withValue || !withSquare || !withProduct) return ...;`. There `withSquare` and `withProduct` prove `squareTerm` and `productTerm` present.

```cpp
            FORMULA_CENSUS_NOTE_WIDTH(Wide, scaledPoint->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, scaledValue->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, squareTerm->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, productTerm->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, withPoint->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, withValue->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, withSquare->magnitude.bit_length());
            FORMULA_CENSUS_NOTE_WIDTH(Wide, withProduct->magnitude.bit_length());
```

  After `if (!countedSquares || ... || !pointsByProducts) return ...;`:

```cpp
        FORMULA_CENSUS_NOTE_WIDTH(Wide, countedSquares->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, squaredSum->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, countedProducts->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, crossSum->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, valuesBySquares->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, pointsByProducts->magnitude.bit_length());
```

  After `if (!pointSpread || !riseTerm || !interceptTerm) return ...;`:

```cpp
        FORMULA_CENSUS_NOTE_WIDTH(Wide, pointSpread->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, riseTerm->magnitude.bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, interceptTerm->magnitude.bit_length());
```

  After `if (!slopeNumerator || !sharedDenominator) return ...;`:

```cpp
        FORMULA_CENSUS_NOTE_WIDTH(Wide, slopeNumerator->bit_length());
        FORMULA_CENSUS_NOTE_WIDTH(Wide, sharedDenominator->bit_length());
```

  Extend `compute_exact`'s doc comment's last paragraph with: `The overflow census (`docs/numeric-headroom.md`) is told the bits each of these integers used.` Only this fit reports. `LinearLeastSquaresOfObservations` and the regression kernel do not: the page measures them by where they overflow.

- [ ] **Step 7: Run to green.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "census|fifty readings"
```

Expected: `ALL OK`. `docs.numeric-headroom` is **not** in this filter, and fails until Step 8, since the table gained a column.

  If `roundedThree.widestWide` or `roundedDistinct.widestWide` differ from 68 and 249, a value was noted twice or missed. Compare each noted value with `$S\laneb\fit_widths.py`'s `note` calls, which mirror Step 6 one for one.

- [ ] **Step 8: Regenerate the table and rewrite the prose.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Target formula-cpp-census-page -NoTest
git -C $T diff docs/numeric-headroom.md
```

  Expected diff: only the `census:least-squares` block changes. Its header gains `| widest fit intermediate (of 256 bits) |`, its rule `|---|` one more cell, the first three rows end `| -- |`, and the last two `| 68 |` and `| 249 |`. Every other figure is unchanged. Any other change stops the task: report it.

  Then, in the prose above the table (`:331-340`), replace from `4 decimal places of N/s, computed by \`LinearLeastSquares::compute_exact\` in` to `figure there says nothing of how close the fit came to its 256 bits.` with:

```markdown
4 decimal places of N/s, computed by `LinearLeastSquares::compute_exact` in
wide integers and rounded exactly; the node is checked against that at 57,
58 and 128 points. For those two rows the fourth column counts `Rational`'s
128-bit integers only, the rounded result and its conversion among them. The
fit itself computes in wider integers, and the last column gives the most
bits any of them used at the sizes that still answer: how near the fit came
to its width, which the column's heading states. The first three rows
compute in `Rational` and form no wide integer.
```

  Then edit `docs/opaque-and-retry.md:325-328`. `An exact fit through fifty readings at eight decimals does not fit\n\`Rational\`. Computed with Python's fractions, the slope is a fraction of 65\nand 73 bits and the intercept of 93 and 91, which fit, but R² needs 130 bits\nover 130.` becomes:

```markdown
An exact fit through fifty readings at eight decimals does not fit
`Rational`. The slope is a fraction of 65 and 73 bits and the intercept of 93
and 91, which fit, but R² needs 130 bits over 130 (`test/least_squares_tests.cpp`,
"a line through fifty readings at eight decimals: how wide each exact output is").
```

  If Step 3's test gave other figures, write those instead. Leave the sentence that follows, `` `opaque_output` then answers `Overflow` -- ... ``, as it is.

  `CHANGELOG.md`: append to `### Changed` under `## [Unreleased]`:

```markdown
- The numeric headroom page's least-squares table gives the most bits the exact curve fit's wide integers used, as
  the overflow census measures it, in place of figures no test checked; the opaque-operation guide's widths of an
  exact fit's outputs are pinned by a test.
```

- [ ] **Step 9: Full verification.**

```powershell
pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."
```

Expected: `ALL OK`, with the total 1 above Task 5's: the case `"a line through fifty readings at eight decimals: how wide each exact output is"`. The census cases are discovered from `formula-cpp-census-tests` and keep their count. `docs.numeric-headroom` and `docs.opaque-and-retry`-style checks pass. Hygiene: `hygiene.headers` is unaffected, since the macro adds no standard header but `<cstddef>`, which is allowed. No new public header.

- [ ] **Step 10: Commit, in two commits.**

```text
test: measure how wide the exact curve fit's integers grow

The overflow census is told the bits every wide integer of
LinearLeastSquares::compute_exact used, and the headroom page's
least-squares table gains a generated column with the widest at the
sizes that answer: 68 of 256 bits on readings at 3 decimal places, 249
on a different denominator for every point. A test pins the widths of
the exact outputs of the fifty-readings fit the opaque guide quotes.

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

  The first commit stages `include/formula-cpp/detail/checked_int.hpp`, `include/formula-cpp/least_squares.hpp`, `support/census_tally.hpp`, `support/census_tally.cpp`, `test/overflow_census_tests.cpp` and `test/least_squares_tests.cpp`. The second stages `docs/numeric-headroom.md`, `docs/opaque-and-retry.md` and `CHANGELOG.md`:

```text
docs: quote the exact fit's widths from the census and a test

Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
```

## Final tasks

These run on `feature/open-issues` in `D:\formula-cpp`, after Lane A's last task is committed there and Lane B's last task is committed on `feature/open-issues-numerics`. They run one after another, in this order: 7, 8, 9, 10. Task 9 (#13) is deliberately late: it rewrites comments in files both lanes changed, and running it after the merge also catches any label a lane added.

### Task 7: Merge Lane B and regenerate the census

The controller runs this task. A conflict it cannot resolve by the rules below, and every gate failure, goes to `sdd-implementer` as a fix round, with the conflicting hunks or the failing log in the brief. No `sdd-reviewer` review: a merge that resolves by these rules adds no code of its own. If a fix round changes code, that round is reviewed as usual.

**Files:**
- Modify (merge resolution only): `CHANGELOG.md`, and any file both lanes touched. The likely ones are:
    - `docs/numeric-headroom.md`: Lane B's table column; Lane A's trace spellings, if the page quotes any;
    - `docs/opaque-and-retry.md`: Lane B's pinned widths; Lane A's `kg^-1` or rounding-clause spellings in its trace blocks;
    - `docs/expressions.md`;
    - `test/opaque_tests.cpp`: Lane A's `coherent_unit_text` cases; Lane B's coefficient-width test, if it was put there.
- Regenerate: `docs/numeric-headroom.md` (census tables), `docs/gallery.md`.

**Interfaces:**
- Consumes: `feature/open-issues` at Lane A's last commit; `feature/open-issues-numerics` at Lane B's last commit (Task 6).
- Produces: `feature/open-issues` holding both lanes, with the census page and the gallery current. Tasks 8–10 build on this head.

- [ ] **Step 1: Confirm both lanes are finished.**
  In PowerShell, from `D:\formula-cpp`:

  ```powershell
  git status --short            # must print nothing tracked
  git log --oneline -1 feature/open-issues
  git log --oneline -1 feature/open-issues-numerics
  git merge-base --is-ancestor feature/open-issues feature/open-issues-numerics; $LASTEXITCODE   # 1: the lanes diverged, as expected
  ```

  Both heads must be the last commits the lanes' reports name. If the working tree is not clean, stop: a lane left something uncommitted.

- [ ] **Step 2: Merge with a merge commit.**

  ```powershell
  git merge --no-ff --no-commit feature/open-issues-numerics
  git status --short
  ```

  Resolve each conflict by these rules, never by taking one side wholesale:
    - **`CHANGELOG.md`:** keep every entry from both lanes under `## [Unreleased]`, each in its own `### Added` / `### Changed` subsection. The breaking-change entry for the scaled dimensionless unit stays first under `### Changed`.
    - **`docs/numeric-headroom.md`:** take Lane B's version. Step 3 regenerates its tables anyway. If Lane A changed a trace spelling in the page's prose, re-apply that spelling by hand.
    - **A guide (`docs/*.md`) both lanes changed:** keep both changes. Where one hunk holds both, apply Lane A's spelling (`kg^-1`, `to N dp of 1/1000 kg`) inside Lane B's text. Step 3's `docs.*-output` tests catch any text block that still disagrees with its program.
    - **A test file both lanes changed:** keep both lanes' test cases.
    - **`include/formula-cpp/*.hpp`:** the lanes touch different files (Lane A: `trace_render.hpp`, `trace.hpp`, `precision.hpp`, `quantity.hpp`, `render.hpp` and the unit checks; Lane B: `detail/transcendental.hpp`, `rounded_transcendental.hpp`, `detail/least_squares_kernel.hpp`, `detail/checked_int.hpp`). A conflict in a header means a lane strayed. Stop and send it to `sdd-implementer` with both lanes' reports.

  Then write the message to `$S\task7-merge-msg.txt`:

  ```
  Merge 128-bit logarithm arguments and pinned bit widths

  The logarithm and exponential kernel takes 128-bit arguments, and the
  bit widths two guides quote are measured by the census and by a test.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  ```powershell
  git add -A
  git commit -F "$S\task7-merge-msg.txt"
  ```

- [ ] **Step 3: Regenerate the census page and the gallery.**

  ```powershell
  pwsh -NoProfile -File $S\cl.ps1 -Tree D:\formula-cpp -Target formula-cpp-census-page -NoTest
  Set-Location D:\formula-cpp
  out\build\cl-debug\tools\gallery\formula-cpp-gallery.exe docs\gallery.md
  git diff --stat docs/numeric-headroom.md docs/gallery.md
  ```

  `cl.ps1 -Target` loads the Visual Studio environment and builds that one target, and `formula-cpp-census-page` builds the census programs it depends on, then rewrites the page. The gallery executable is built by the same `cl-debug` tree; if it is missing, run `pwsh -NoProfile -File $S\cl.ps1 -Tree D:\formula-cpp -NoTest` first.

  Expected change in `docs/numeric-headroom.md`:
    - the least-squares table holds the generated "widest fit intermediate (of 384 bits)" column Lane B added. The figures may differ from Lane B's commit only if Lane A's changes formed different integers. They form none: rendering is not in the census;
    - any other census row moves only if a lane's change formed new integers. Lane B's widened kernel is outside `Rational`'s census, so no row should move. **A row that moves is a finding: report it in the commit body, with the row before and after.**

  Expected change in `docs/gallery.md`: only Lane A's spellings (`kg^-1` after a numerator-less coherent unit, a rounding clause naming an unnamed unit by its size). Anything else is a defect: stop and send it to a fix round.

  For each `docs.<guide>-output` test that fails in Step 4, run its example (`out\build\cl-debug\examples\<example>.exe`), and replace, in the guide's ```` ```text ```` blocks, exactly the lines that changed with the program's lines.

- [ ] **Step 4: Run the Windows gate.**

  ```powershell
  pwsh -NoProfile -File $S\cl.ps1 -Tree D:\formula-cpp -Exclude "^negative\."
  pwsh -NoProfile -File $S\neg.ps1 -Tree D:\formula-cpp -Filter "."
  ```

  Both must print `ALL OK`. The non-negative total must equal: the baseline count, plus every Lane A task's delta, plus every Lane B task's delta. The negative total must equal the baseline negatives plus the negatives both lanes added. A difference is a lost or doubled test: find it before going on.

- [ ] **Step 5: Commit the regeneration.**
  If Step 3 or Step 4 changed any file, write `$S\task7-regen-msg.txt`:

  ```
  docs: regenerate the census page and the gallery

  <one sentence per page that changed: what moved, and why. For the census,
  name any row whose figures moved and its before and after.>

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  ```powershell
  git add docs/numeric-headroom.md docs/gallery.md docs/*.md
  git commit -F "$S\task7-regen-msg.txt"
  ```

  Delta: **0 tests** beyond the two lanes' sum.

### Task 8: `explain` and `checked_explain` refuse a series; two documentation corrections (#11, #21)

`explain<Q>(series, environment)` and `checked_explain<Q>(series, environment)` fail today with the compiler's "no matching function". `trace.hpp` has an `explain` and a `checked_explain` for a `Node` and for a `Yields`, and a series is neither, so overload resolution fails before any library check runs. This task adds a `SeriesNode` overload of each that refuses in the library's words and names `explain_series`, exactly as `evaluate`'s `SeriesNode` overloads (`evaluate.hpp:505-536`) refuse with `checked_evaluate_series`. Then it makes the two documentation corrections of #21. Two commits.

`trace_of`, `trace_of_si` and the `Yields` forms are **not** changed: they keep refusing a series with `RequireSingleValueExpression`'s message, and their negative tests (`trace_of_series_as_single`, `trace_of_si_series_as_single`, `yields_series_explain`) must pass unchanged.

**Files:**
- Modify: `include/formula-cpp/evaluate.hpp:185-201`: a new check, `detail::RequireSingleValueTraced`, right after `RequireSingleValueExpression`.
- Modify: `include/formula-cpp/trace.hpp`: a `SeriesNode` overload of `explain` after the `Node` overload (`:4588-4603`), and of `checked_explain` after the `Node` overload (`:4729-4745`).
- Create: `test/negative/explain_series_as_single.cpp`, `test/negative/checked_explain_series_as_single.cpp`.
- Modify: `test/CMakeLists.txt`: two `formula_add_negative_test` calls after `yields_series_explain` (`:535-537`).
- Modify: `docs/series.md:47-53`, `docs/tracing.md:223-228`.
- Modify: `README.md:247-252`.
- Modify: `docs/superpowers/specs/2026-10-03-int128-rational-design.md:86-87`.

**Interfaces:**
- Consumes: `detail::refused_already<T>()` (`expression.hpp:158`), the concepts `SeriesNode` (`expression.hpp:374`) and `Node` (`expression.hpp:42`), `Explained<Result, Rep>`, `CheckedExplainFailure<Rep>` (`trace.hpp`).
- Produces: `template <typename Expression> struct detail::RequireSingleValueTraced` with `static constexpr bool value = true`. Its message is exactly:
  `formula: this expression is a series, not a single value; explain it with explain_series, or reduce it to one value first (sum, interpolate_at)`.

- [ ] **Step 1: Write the two negative tests.**
  `test/negative/explain_series_as_single.cpp`:

  ```cpp
  // SPDX-License-Identifier: Apache-2.0
  // EXPECT: this expression is a series, not a single value; explain it with explain_series
  // REJECT: no matching
  //
  // A series handed to explain, which traces a single value. Refused in this
  // library's words, pointing at explain_series, the verb that gives a
  // series' derivation -- rather than as an overload nobody matched.
  #include <formula-cpp/formula.hpp>
  #include <formula-cpp/trace.hpp>

  struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
  {
  };

  inline constexpr auto inputs =
      formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                              formula::Measured<Retained> { formula::Rational { 210 } },
                                                              formula::Measured<Retained> { formula::Rational { 95 } }));

  int main()
  {
      return formula::explain<Retained>(formula::series<Retained, 3>, inputs).trace.empty() ? 1 : 0;
  }
  ```

  `test/negative/checked_explain_series_as_single.cpp`: the same file with these differences.
    - The comment's first sentence reads "A series handed to checked_explain, which traces a single value."
    - `main` is:

  ```cpp
  int main()
  {
      return formula::checked_explain<Retained>(formula::series<Retained, 3>, inputs).has_value() ? 0 : 1;
  }
  ```

- [ ] **Step 2: Register them with a deliberately wrong text, and watch them fail.**
  Append to `test/CMakeLists.txt`, after the `yields_series_explain` registration (`:535-537`):

  ```cmake
  # A bare series handed to explain or checked_explain, which trace a single
  # value: refused in the library's words, pointing at explain_series, the
  # verb that gives a series' derivation, rather than as an overload nobody
  # matched. trace_of and the bound forms above keep their own message.
  formula_add_negative_test(explain_series_as_single
      "explain it with explain_series -- WRONG ON PURPOSE" EXPECT_COUNT 1
      REJECT "no matching")
  formula_add_negative_test(checked_explain_series_as_single
      "explain it with explain_series -- WRONG ON PURPOSE" EXPECT_COUNT 1
      REJECT "no matching")
  ```

  Run: `pwsh -NoProfile -File $S\neg.ps1 -Tree D:\formula-cpp -Filter "explain_series_as_single|checked_explain_series_as_single"`.
  Expected: both FAIL on both presets. The build log names "no matching function" (cl: `C2672`), which is #11's defect.

- [ ] **Step 3: The check.**
  In `include/formula-cpp/evaluate.hpp`, directly after `RequireSingleValueExpression`'s closing `};` (`:201`), add:

  ```cpp
      /// Fails to compile when a series (`series.hpp`) is handed to a verb that
      /// traces one value -- `explain` or `checked_explain` (`trace.hpp`). The
      /// tracing counterpart of `RequireSingleValueExpression`: a caller who
      /// asked for a derivation is pointed at `explain_series`, the series verb
      /// that gives one. Named so the expression prints.
      template <typename Expression>
      struct RequireSingleValueTraced
      {
          // A series already refused (`refused`, `series.hpp`) is not asked
          // again: its own refusal is the one message for the mistake.
          static_assert(
              !SeriesNode<Expression> || requires { requires detail::refused_already<Expression>(); },
              "formula: this expression is a series, not a single value; explain it with "
              "explain_series, or reduce it to one value first (sum, interpolate_at)");

          static constexpr bool value = true;
      };
  ```

- [ ] **Step 4: The two overloads.**
  In `include/formula-cpp/trace.hpp`, directly after `explain`'s `Node` overload (the one ending `return explained;\n}` at `:4603`), add:

  ```cpp
  /// A series handed to `explain`: fails to compile, in this library's words,
  /// pointing at `explain_series`, which gives a series' outcome and its
  /// derivation. The body is the refusal and nothing else; what it returns is
  /// never seen.
  template <Described Result, typename Rep = Rational, SeriesNode Expression, typename Env, Vocabulary V = DefaultVocabulary>
  [[nodiscard]] Explained<Result, Rep> explain(Expression const&, Env const&, V const& = V {})
  {
      static_assert(detail::RequireSingleValueTraced<Expression>::value);
      return Explained<Result, Rep> {};
  }
  ```

  Directly after `checked_explain`'s `Node` overload (the one ending `return Explained<Result, Rep> { *checked, std::move(recorded) };\n}` at `:4745`), add:

  ```cpp
  /// A series handed to `checked_explain`: refused as `explain` refuses it,
  /// pointing at `explain_series`. The body is the refusal and nothing else;
  /// what it returns is never seen.
  template <Described Result, typename Rep = Rational, SeriesNode Expression, typename Env, Vocabulary V = DefaultVocabulary>
  [[nodiscard]] std::expected<Explained<Result, Rep>, CheckedExplainFailure<Rep>>
  checked_explain(Expression const&, Env const&, V const& = V {})
  {
      static_assert(detail::RequireSingleValueTraced<Expression>::value);
      return Explained<Result, Rep> {};
  }
  ```

  `Node` and `SeriesNode` are disjoint (a series derives from `SeriesNodeBase`, never `NodeBase`), so the new overloads cannot be ambiguous with the existing ones.

- [ ] **Step 5: Register the right text and watch both pass.**
  In the two registrations, replace `"explain it with explain_series -- WRONG ON PURPOSE"` with:

  ```cmake
      "this expression is a series, not a single value; explain it with explain_series" EXPECT_COUNT 1
  ```

  Run: `pwsh -NoProfile -File $S\neg.ps1 -Tree D:\formula-cpp -Filter "explain_series_as_single|checked_explain_series_as_single|trace_of_series_as_single|trace_of_si_series_as_single|yields_series_explain|evaluate_series_as_single"`.
  Expected: `ALL OK`, 6 cases on each preset. `EXPECT_COUNT` is checked only on clang-cl.

- [ ] **Step 6: Confirm the overloads are what the tests pin.**
  Delete the new `explain` overload alone (keep a copy of the file in `$S\trace.hpp.task8`). Re-run the Step 5 command. Expected: `explain_series_as_single` FAILS on both presets with "no matching function" in its log (the generic error is back), and every other case passes. Restore `trace.hpp` with a plain write from the copy. Do the same for the `checked_explain` overload and `checked_explain_series_as_single`. Restore, then re-run the Step 5 command: `ALL OK`.

- [ ] **Step 7: The guides.**
  In `docs/series.md`, replace the paragraph ending at `:53` and its code block:

  ```markdown
  compile. Handed to `checked_evaluate`, `evaluate` or `variant<Tag>`, it is
  refused in the library's words:

  ```
  static assertion failed: formula: this expression is a series, not a single value; evaluate it with checked_evaluate_series, or reduce it to one value first (sum, interpolate_at)
  ```
  ```

  with:

  ```markdown
  compile. Handed to `checked_evaluate`, `evaluate` or `variant<Tag>`, it is
  refused in the library's words:

  ```
  static assertion failed: formula: this expression is a series, not a single value; evaluate it with checked_evaluate_series, or reduce it to one value first (sum, interpolate_at)
  ```

  Handed to `explain` or `checked_explain`, which trace a single value, it is
  refused the same way, pointing at `explain_series`, the verb that gives a
  series' outcome together with its derivation:

  ```
  static assertion failed: formula: this expression is a series, not a single value; explain it with explain_series, or reduce it to one value first (sum, interpolate_at)
  ```
  ```

  In `docs/tracing.md`, after the paragraph that begins "`explain_series` and `explain_retry` share the shape" (`:223-228`), add a paragraph:

  ```markdown
  A series handed to `explain` or `checked_explain` does not compile: both
  trace a single value, and they say so in the library's words, pointing at
  `explain_series`. Reduce the series to one value first (`sum`,
  `interpolate_at`) to trace that value instead.
  ```

  `docs.series` and `docs.tracing` (if they exist as checked-guide tests) compare only `text` blocks with program output. These blocks are plain, so they are not compared.

- [ ] **Step 8: Verify.**
  - `pwsh -NoProfile -File $S\cl.ps1 -Tree D:\formula-cpp -Exclude "^negative\."` must print `ALL OK`. Delta: **0 tests**.
  - The Step 5 negative command must print `ALL OK`. Negative delta: **+2 tests**.

- [ ] **Step 9: Commit #11.**
  First add to `CHANGELOG.md`, under `## [Unreleased]` → `### Changed`:

  ```markdown
  - `explain` and `checked_explain` handed a series refuse it in the library's words, pointing at `explain_series`, instead of failing with "no matching function".
  ```

  Then write `$S\task8-commit-1.txt`:

  ```
  fix(trace): refuse a series handed to explain or checked_explain in the library's words

  A series handed to explain or checked_explain failed with the compiler's
  "no matching function": each had an overload for a single-value
  expression and one for a bound formula, and a series is neither. Each now
  has a series overload that refuses it as evaluate refuses one, pointing
  at explain_series, the verb that gives a series' derivation. trace_of
  and the bound forms refuse a series as before.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  No `Closes` line: the pull request's body closes the issues (Task 10).

  ```powershell
  git add include/formula-cpp/evaluate.hpp include/formula-cpp/trace.hpp test/negative/explain_series_as_single.cpp test/negative/checked_explain_series_as_single.cpp test/CMakeLists.txt docs/series.md docs/tracing.md CHANGELOG.md
  git commit -F "$S\task8-commit-1.txt"
  ```

- [ ] **Step 10: The README's duplicate link.**
  In `README.md:247-252`, the paragraph reads:

  ```markdown
  arguably a more important one. See [the tracing guide](docs/tracing.md) for
  the detail. Tracing costs nothing when nobody asks for it: a sink is passed by
  value, and the untraced path — `evaluate()`, `checked_evaluate()` — defaults
  to one that does nothing, adding no instruction the evaluator would not
  already emit once the call inlines, measured on all four compilers this
  library targets. See [the tracing guide](docs/tracing.md).
  ```

  Replace it with:

  ```markdown
  arguably a more important one. Tracing costs nothing when nobody asks for it:
  a sink is passed by value, and the untraced path — `evaluate()`,
  `checked_evaluate()` — defaults to one that does nothing, adding no
  instruction the evaluator would not already emit once the call inlines,
  measured on all four compilers this library targets. See
  [the tracing guide](docs/tracing.md).
  ```

  Check: `git grep -c "the tracing guide](docs/tracing.md)" README.md` drops by exactly one from what it printed before the edit.

- [ ] **Step 11: The 128-bit design document's checked forms.**
  The code (`include/formula-cpp/detail/checked_int.hpp:284-320`, `include/formula-cpp/int128.hpp:229-271`) does this:
    - `add_checked_or_none` and `sub_checked_or_none` for `Int128` form the sum and the difference on the two words' bit patterns with `u128_add` / `u128_sub`. These are the portable routines on every compiler (`int128.hpp:229-238` calls `portable::add` / `portable::subtract` with no native branch). They detect overflow from the operands' and the result's signs.
    - `mul_checked_or_none` multiplies the magnitudes with `u128_mul_checked`. That uses `__builtin_mul_overflow` on `unsigned __int128` where the compiler has it (`FORMULA_NATIVE_INT128`), and the portable `multiply_checked` elsewhere. Then it checks the product against the signed range.

  In `docs/superpowers/specs/2026-10-03-int128-rational-design.md:85-87`, replace:

  ```markdown
  - **Checked forms live in `detail/checked_int.hpp`,** beside the 64-bit ones, as `Int128` overloads of
    `add_checked_or_none`, `sub_checked_or_none` and `mul_checked_or_none`. Natively they use `__builtin_add_overflow`
    and its kin, which are `constexpr` on GCC and Clang. In software they use partial products.
  ```

  with:

  ```markdown
  - **Checked forms live in `detail/checked_int.hpp`,** beside the 64-bit ones, as `Int128` overloads of
    `add_checked_or_none`, `sub_checked_or_none` and `mul_checked_or_none`. The sum and the difference are formed on
    the two words' bit patterns with the portable routines, on every compiler, and an overflow is detected from the
    signs: calling `Int128`'s own `+` or `-` first would break their precondition that the exact result fits. Only
    the product uses the compiler's checked builtin, `__builtin_mul_overflow` on the unsigned magnitudes, where the
    compiler has `__int128`, and partial products in software elsewhere; it is then checked against the signed
    range.
  ```

- [ ] **Step 12: Commit #21.**
  `$S\task8-commit-2.txt`:

  ```
  docs: link the tracing guide once, and describe the checked 128-bit add as built

  The README's tracing paragraph linked the tracing guide twice; it now
  links it once, at the end. The 128-bit design document said the checked
  add and subtract use the compiler's overflow builtins. They compute on
  the words' bit patterns with the portable routines on every compiler, and
  detect overflow from the signs; only the multiply uses the native checked
  builtin.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  ```powershell
  git add README.md docs/superpowers/specs/2026-10-03-int128-rational-design.md
  git commit -F "$S\task8-commit-2.txt"
  ```

  No CHANGELOG entry: neither change touches the library.

### Task 9: Self-describing comments in place of development labels (#13)

More than 80 comments, test names, section banners, one guide sentence and two CMake comments refer to the project's development history: "phase 12", "phase 15's spike, step 9", "spec phase 8", "a spike compiled …". A reader cannot look any of these up. This task rewrites every one so that it states what it relies on in its own words:
- A comment that quotes a measurement keeps the measurement and says where it holds, by compiler and version where the comment or its neighbours already name them, or it points at the test that pins it.
- A reference that adds nothing is dropped.
- A section banner names the feature.
- A test name says what it tests.

`docs/superpowers/` is out of scope: it records how the work was planned.

**The search,** which must return nothing when this task is done:

```bash
git grep -nE '\b[Pp]hase [0-9]+|\bspike\b|\bstep [0-9]+\)' -- include test examples tools docs/*.md README.md cmake CMakeLists.txt
```

At `1ed39ed` it returns **139 lines in 51 files** (count with `| wc -l`). Lanes A and B may add a few; Step 6 catches them. Also fix the same kind of label where the search cannot see it, in a sentence that already holds a hit: "This phase adds" (`sink.hpp:209`), "this phase exists to get right" and "this whole phase's central argument" (`examples/CMakeLists.txt:109`, `:116`).

**Rules for every rewrite.**
- Edit by hand, in the file's own style. Do not run clang-format.
- Keep the sentence grammatical. Where the replacement below gives only the changed phrase, adjust the words around it so the sentence reads.
- **Never invent a fact.** A compiler, a version or a count goes in only where the comment, a neighbouring comment or a test already states it. Otherwise say "measured" without naming more, or point at the test.
- A label inside a Doxygen `///` comment stays a `///` comment. A banner keeps its width: pad with `-` to the same column.
- Spec section numbers ("spec sections 9 and 9.1", "section 16.7's demands") are outside this search and this task. Leave them.

**Files:** every file in the lists below; nothing else.

**Interfaces:**
- Consumes: `feature/open-issues` after Task 8.
- Produces: no code change. Two census test cases renamed (Step 4); no build file, script or document refers to their names (checked in Step 4).

- [ ] **Step 1: Re-run the search and save the list.**
  `git grep -nE '\b[Pp]hase [0-9]+|\bspike\b|\bstep [0-9]+\)' -- include test examples tools docs/*.md README.md cmake CMakeLists.txt > $S\task9-before.txt`, then count its lines. Compare with the lists in Steps 2–5. A hit at a different line number is the same hit moved by the lanes: find it by its text. A hit not listed is a lane's addition: rewrite it under the same rules in Step 6.

- [ ] **Step 2: Public headers, the guide and the CMake comments.**
  Each line gives `file:line`, the phrase as it stands, and its replacement.

  - `cmake/CheckInstalledHeaders.cmake:7`: "forgotten -- and it was. Phase 7 added sink.hpp, trace.hpp and trace_render.hpp and listed none of them" → "forgotten -- and it was: sink.hpp, trace.hpp and trace_render.hpp were once added and listed nowhere".
  - `docs/quantities.md:186-187`: "a spike compiled the five-parameter spelling with `dim::Mass` paired against `unit::Litre`, and all three compilers accepted the contradiction in silence" → "the five-parameter spelling, with `dim::Mass` paired against `unit::Litre`, compiled without a diagnostic on every compiler it was tried on".
  - `examples/CMakeLists.txt:109-116`: "the very outcome words and trace shape this phase exists to get right" → "the very outcome words and trace shape the constraints guide exists to show". "a withdrawn spelling and a stale guide during phase 8" → "a withdrawn spelling and a stale guide". "the not-checked case that is this whole phase's central argument" → "the not-checked case that is the guide's central argument".
  - `include/formula-cpp/band.hpp:26`: "and a spike compiled the rejection on all four compilers" → "and the rejection was measured on all four compilers".
  - `band.hpp:175`: "a spike compiled `template <BandTable Bands>` directly" → "`template <BandTable Bands>` compiles directly" (the sentence goes on "with alias-template deduction, on all four compilers").
  - `band.hpp:177`: "Consumers (phase 10 tasks 2-4) name a table" → "Consumers name a table".
  - `band.hpp:185`: "and by any runtime loader (phase 10 tasks 2-4) --" → "and by any runtime loader --".
  - `include/formula-cpp/citation.hpp:90`: "only the documentation walk and, from phase 7, the trace sink will notice it" → "only the documentation walk and the trace sink notice it".
  - `include/formula-cpp/constraint.hpp:247`: "that shipped a `StepKind::Pi` enumerator shadowing `formula::Pi` in phase 7 and broke GCC alone" → "that once shipped a `StepKind::Pi` enumerator shadowing `formula::Pi` and broke GCC alone".
  - `constraint.hpp:252`: "the same problem phase 8 solved for a `WhenNode`'s branch" → "the same problem a `WhenNode`'s branch has, solved".
  - `include/formula-cpp/error.hpp:46`: "Spec phase 5 renders this into the `invalid` arm of the evaluation result." → "An evaluation result's `invalid` arm is written with it."
  - `include/formula-cpp/lookup.hpp:36`: "A prior spike verified, separately, that" → "It was measured, separately, that".
  - `lookup.hpp:70`: "adding one would be phase 9's forbidden `bool satisfied()` in a new costume" → "adding one would be the refused `bool satisfied()` of constraints (`constraint.hpp`) in a new costume".
  - `lookup.hpp:92`: "A prior spike proved `InvalidReason::label` is" → "`InvalidReason::label` is, as measured,".
  - `lookup.hpp:169`: "`detail::FixedString` (phase 4), which is" → "`detail::FixedString`, which is".
  - `lookup.hpp:439`: "Nothing here reaches for phase 8's rounding" → "Nothing here reaches for rounding (`rounding.hpp`)".
  - `lookup.hpp:559-560`: "which would make a binary search valid. Phase 10 is the first thing in this codebase to need an interval search at all; a method's own published table" → "which would make a binary search valid. A method's own published table".
  - `lookup.hpp:906`: "a spike compiled `template <KeyTable Keys>` with both `Key` and `N` deduced from the template argument, on cl, clang-cl, clang++ and g++" → "`template <KeyTable Keys>` compiles with both `Key` and `N` deduced from the template argument, measured on cl, clang-cl, clang++ and g++".
  - `lookup.hpp:1398`: "a spike compiled `template <BandTable Bands>` with both the element type and `N` deduced on all four compilers" → "`template <BandTable Bands>` compiles with both the element type and `N` deduced, measured on all four compilers".
  - `include/formula-cpp/measured.hpp:60-61`: "Phase 5's expression layer may well want to, and if it does, this is the line to revisit" → "If a later layer needs to, this is the line to revisit".
  - `include/formula-cpp/method.hpp:167-169`: "Phase 10 settled that test for lookups -- a lookup *is* a node because it produces a quantity -- and it comes out the other way here." → "The same test makes a lookup a node, because a lookup produces a quantity, and it comes out the other way here."
  - `method.hpp:916`: "The same mistake was found, and fixed, in the lookup tables of phase 10." → "The lookup tables were once open to the same mistake, and are checked the same way."
  - `method.hpp:1702`: "the same ruling phase 9 made for `bool satisfied()` and phase 10 made for a lookup miss" → "the same ruling as for a constraint's `bool satisfied()` and a lookup miss".
  - `include/formula-cpp/opaque.hpp:224`: "(phase 15's spike, step 9)." → "(measured under MathJax 3.2.2 with the site's configuration)." (`render.hpp:1913` names that engine and version for the same spellings).
  - `opaque.hpp:1485-1486`: "errors of its own (phase 15's spike, step 7), the behaviour `checked_evaluate_series` records for its refusal" → "errors of its own -- the behaviour `checked_evaluate_series` records for its refusal, and what `opaque_throwing_compute`'s REJECTs pin".
  - `include/formula-cpp/overlay.hpp:16`: "A spike that built both shapes settled it:" → "Both shapes were built and compared:".
  - `overlay.hpp:2129`: "as it is for any series (phase 12's message)." → "as it is for any series (`RequireSingleValueExpression`'s message)." Check the message the refusal really gives with the existing negative test for this case before writing it; if it is another check's, name that one.
  - `include/formula-cpp/precision.hpp:116`: "Named `abs` after a spike:" → "Named `abs` after measuring it:" (the sentence goes on to name cl 19.51, clang-cl and clang++ 22.1.3, g++ 13.3 and 14.2).
  - `precision.hpp:341`: "as `DerivedQuantityNode`, and then phase 12's `sum` and elementwise nodes, once did" → "as `DerivedQuantityNode`, and then the series `sum` and elementwise nodes, once did".
  - `precision.hpp:569`: "// Phase 12's series kinds." → "// The series kinds (`series.hpp`)."
  - `precision.hpp:640`: "// Phase 14's snap and curves:" → "// Snaps and curves (`snap.hpp`, `curve.hpp`):".
  - `precision.hpp:668`: "// Phase 12's raw observations, and the classes they are binned into." → "// Raw observations, and the classes they are binned into."
  - `include/formula-cpp/quantity.hpp:92-93`: "A spike compiled that spelling with `dim::Mass` against `unit::Litre` and all three compilers accepted it in silence." → "That spelling, with `dim::Mass` against `unit::Litre`, compiled without a diagnostic on every compiler it was tried on."
  - `include/formula-cpp/rational.hpp:14`: "rounding is an explicit operation (rounding.hpp) and, from spec phase 8 on, a node in the expression tree." → "rounding is an explicit operation (rounding.hpp) and a node in the expression tree."
  - `include/formula-cpp/record.hpp:1577-1578`: "all the same -- measured by phase 14's spike on cl, clang-cl, clang++ and g++." → "all the same -- measured on cl, clang-cl, clang++ and g++."
  - `include/formula-cpp/render.hpp:22`: "the defect phase 8 published, when `round[to 1 dp of mm](d)` reached a page" → "a defect once published, when `round[to 1 dp of mm](d)` reached a page".
  - `render.hpp:99-100`: "Added in spec phase 8; every other rung keeps its original number." → "Added later than the others, at 0; every other rung keeps its original number."
  - `render.hpp:307`: "Chosen by the phase 12 spike: under MathJax 3.2.2" → "Chosen by measurement: under MathJax 3.2.2".
  - `render.hpp:329`: "chosen by phase 15's spike (step 9) for the same engines" → "measured under the same engines".
  - `render.hpp:1130`: "the spelling a spike typeset clean" → "a spelling measured to typeset clean under MathJax and tectonic".
  - `render.hpp:1446`: `// ------------------------------------------------------- phase 10: lookups` → `// ---------------------------------------------------------------- lookups`.
  - `render.hpp:1726`: "a spike measured a row whose formula held" → "measured: a row whose formula held". Keep the versions the sentence already names (python-markdown 3.10.3, pymdown-extensions 12.1).
  - `render.hpp:1835`: "typeset clean under MathJax 3.2.2 and tectonic by a spike --" → "measured to typeset clean under MathJax 3.2.2 and tectonic --".
  - `render.hpp:1913`: "The spellings are phase 15's spike's (step 9), measured under MathJax 3.2.2" → "The spellings were measured under MathJax 3.2.2".
  - `render.hpp:2177`: "measured by the phase-11 spike on clang++ 20.1.8" → "measured on clang++ 20.1.8".
  - `include/formula-cpp/retry.hpp:144`: "g++ 14.2 (measured in phase 15's spike, step 5)." → "g++ 14.2, as measured." Or drop the parenthesis: the sentence already says "fit one constant evaluation on cl 19.51, …".
  - `include/formula-cpp/rounding.hpp:184`: "series needs (spec phase 12)." → "series needs (`snap.hpp`)."
  - `include/formula-cpp/sink.hpp:207-209`: "Phase 5 published `checked_evaluate_si(node, environment)` as an extension point: … This phase adds a third parameter," → "`checked_evaluate_si(node, environment)` was published as an extension point: … The sink is a third parameter,".
  - `include/formula-cpp/statistics.hpp:10`: "That is a phase 12 series, `series<Q, N>`" → "That is a series (`series.hpp`), `series<Q, N>`".
  - `statistics.hpp:71`: "a phase 12 series," → "a series (`series.hpp`),".
  - `statistics.hpp:114`: "-- phase 12's `SeriesFailure`," → "-- a series' `SeriesFailure`,".
  - `include/formula-cpp/trace.hpp:611`: "Phase 9 refused `bool satisfied()` for exactly this shape of defect:" → "Constraints refuse a `bool satisfied()` (`constraint.hpp`) for exactly this shape of defect:".
  - `trace.hpp:1056-1057`: "the identical duplication phase 8 undid when it removed the member it had added to `WhenNode`" → "the identical duplication once undone by removing a member added to `WhenNode`".
  - `trace.hpp:4467-4468`: "as phase 12's series paths and phase 13's statistics paths were," → "as the series paths and the statistics paths were,".
  - `include/formula-cpp/trace_render.hpp:926-927`: "which is the defect phase 9 refused `bool satisfied()` over" → "which is the defect a constraint's `bool satisfied()` was refused over".

- [ ] **Step 3: Test comments, banners and CMake comments.**
  - `test/CMakeLists.txt:1080`: "sees through phase 12's series kinds" → "sees through the series kinds".
  - `test/CMakeLists.txt:2138`: "# Phase 14: records and the context." → "# Records and the context."
  - `test/CMakeLists.txt:2316`: "as phase 12's walks answer a refused series" → "as the walks answer a refused series".
  - `test/CMakeLists.txt:2372`: "when the evaluator's body was not gated (phase 15's spike, step 7): the REJECTs." → "when the evaluator's body was not gated: the REJECTs."
  - `test/band_tests.cpp:255`: "the way phase 10 tasks 2-4 will reach it" → "the way the lookup nodes reach it".
  - `test/conformity_tests.cpp:31`, `test/series_tests.cpp:16`: "(see the phase 12 plan)" → drop the parenthesis.
  - `test/dimension_cross_tu.hpp:5`: "Phase 1 established that the equivalent trick" → "The equivalent trick". Adjust the verb: "… with `decltype([]{})` gives each TU its OWN type …".
  - `test/document_tests.cpp:238`: "Measured while phase 10 added the lookup overloads:" → "Measured when the lookup overloads were added:".
  - `document_tests.cpp:487`: banner `phase 10: lookups` → `lookups`, same width.
  - `document_tests.cpp:668`: "which is the shape of the defect phase 8 published" → "which is the shape of a defect once published".
  - `document_tests.cpp:682`: "which is the property phase 8's published defect violated" → "which is the property that published defect violated".
  - `document_tests.cpp:857`: `// ---- A series in the symbol table (phase 12) ----` → `// ---- A series in the symbol table ----`.
  - `test/least_squares_tests.cpp:233-234`: "the spike's shape that overflows from 27 points (step 3)." → "a shape that overflows at 27 points, as the test below pins."
  - `test/lineage_tests.cpp:356`: "Phase 11's trace escape (`escaped_author_text`)" → "The trace's escape (`escaped_author_text`)".
  - `test/measured_tests.cpp:193`: "the same exact conversion phase 3 proved" → "the same exact conversion `unit_tests.cpp` pins".
  - `test/method_tests.cpp:166-170`: "What a reachability probe catches is ABSENCE -- phase 9 shipped an overload that worked, was tested, and no user could call; phase 10 shipped a `document()` walk that compiled for nothing. The spike compiled this pack and never ran it, so nothing until now had established that a stored variant still evaluates at all." → "What a reachability probe catches is ABSENCE: an overload that works and is tested but that no user can call, or a `document()` walk that compiles for nothing. Compiling this pack proves nothing about running it, so this establishes that a stored variant still evaluates at all."
  - `method_tests.cpp:387`: banner `phase 13: a precision check` → `a precision check`, same width.
  - `test/negative/method_tag_names_spelt_alike.cpp:7`: "(final review of phase 11, L3)" → drop the parenthesis.
  - `test/negative/method_variants_disagree.cpp:8-9`: "The rule inherited from phase 10 is "put the defect in the middle, …", and phase 10 read "middle" as a middle PAIR of four rows" → "The rule for a band table is "put the defect in the middle, …", and there "middle" means a middle PAIR of four rows".
  - `test/negative/opaque_throwing_compute.cpp:9`: "adds none of its own errors (phase 15's spike, step 7) -- the" → "adds none of its own errors -- the".
  - `test/negative/overlay_constant_inside_opaque_series.cpp:9`: "the refusal is phase 12's for a series" → "the refusal is the one for a series".
  - `test/opaque_tests.cpp:459`: "a hard error on clang, g++-14 and libc++ in phase 11 (defect class 4)" → "a hard error on clang, g++-14 and libc++ once".
  - `test/overflow_census_tests.cpp:197`: "// The phase 13 fixtures (rejection_tests.cpp's shared fixtures), in grams." → "// The statistics fixtures (rejection_tests.cpp's shared fixtures), in grams."
  - `overflow_census_tests.cpp:317`: `// ---- Least squares (phase 15), a spike's data shapes ----…` → `// ---- Least squares: three data shapes ----…`, same width.
  - `overflow_census_tests.cpp:320-321`: "Invented; the spike's offsets are replaced by primes, the rest of its generator kept." → "Invented: each shape's offsets are primes."
  - `overflow_census_tests.cpp:951`: "The spike's shapes, through the library's own fit:" → "The three shapes, through the library's own fit:".
  - `test/overlay_tests.cpp:810`: "Final review of phase 11, M3: a pinned method" → "A pinned method".
  - `overlay_tests.cpp:1399`: "Final re-review of phase 11, M4: every operation" → "Every operation".
  - `test/record_join_tests.cpp:363`: "over phase 12's accessors" → "over the series accessors".
  - `record_join_tests.cpp:413`: "Phase 12's invented classes and sizes:" → "Invented classes and sizes:".
  - `record_join_tests.cpp:427`: "The observations path phase 12 added records its own step;" → "The observations path records its own step;".
  - `test/record_statistics_tests.cpp:3`: "Phase 13's statistics, outlier rejections and precision limits" → "Statistics, outlier rejections and precision limits".
  - `record_statistics_tests.cpp:49`, `:103`, `:127`: "phase 13's fixture A" / "Phase 13's fixture A" / "Phase 13's fixture P" → "the statistics fixture A" / "The statistics fixture A" / "The statistics fixture P" (`rejection_tests.cpp`'s shared fixtures).
  - `test/render_tests.cpp`, banners `:422`, `:491`, `:549`, `:599`, `:767`, `:825`: drop the `phase 8: ` / `phase 9: ` / `phase 10: ` prefix, keeping the feature name and the width: `rounding`, `predicates`, `conditionals`, `numeric_value_of`, `constraints`, `lookups`.
  - `render_tests.cpp:570`: "This is the case phase 6's two rendering bugs generalise to:" → "This is the case two earlier rendering bugs generalise to:".
  - `render_tests.cpp:638-639`: "// ---- phase 8 fix round 1: nesting a new kind inside another new kind, in every dialect. This is the exact axis review …" → "// ---- nesting one node kind inside another, in every dialect. This is the exact axis a review …", same banner width.
  - `render_tests.cpp:1178`: "the operand a published page dropped in phase 8" → "the operand a published page once dropped".
  - `render_tests.cpp:1430`: "that is the whole lesson of phase 8, where two renderers each had passing tests" → "that is the whole lesson of the time two renderers each had passing tests".
  - `render_tests.cpp:1472-1473`: "// ---- phase 8 fix round 3: guard against the whole class of bug review round 3 found, not just this one" → "// ---- guard against the whole class of bug, not just one instance of it", same width.
  - `render_tests.cpp:1560-1561`: "// Phase 13: a bare `|`. … -- a spike measured a row whose formula held" → "// A bare `|`. … -- measured: a row whose formula held".
  - `render_tests.cpp:1568`: "// Phase 10 round 2: an asterisk." → "// An asterisk."
  - `render_tests.cpp:1668`: "// Phase 10's three lookup kinds." → "// The three lookup kinds."
  - `render_tests.cpp:1684`: "// Phase 14: a read from another record," → "// A read from another record,".
  - `render_tests.cpp:1836`: "(phase 12)" → drop, same width.
  - `render_tests.cpp:1867`: "(typeset clean in a spike)" → "(measured to typeset clean)".
  - `test/retry_tests.cpp:369`: "If phase 14 or any later change adds a member" → "If a change adds a member".
  - `test/sink_tests.cpp:116`: "written against the extension point as phase 5 published it:" → "written against the extension point as first published:".
  - `sink_tests.cpp:190`: "Threading a sink through every overload in phase 7 could have cost that" → "Threading a sink through every overload could have cost that".
  - `sink_tests.cpp:225`: "so this covers the parameter phase 7 added" → "so this covers the sink parameter".
  - `test/statistics_tests.cpp:288`: "is refused where it is written (phase 12)," → "is refused where it is written (`series.hpp`),".
  - `test/trace_render_tests.cpp:311`: banner `phase 8` → `rounding, predicates and conditionals`, same width. Read the tests under it first and name what they cover if that list is wrong.
  - `trace_render_tests.cpp:681`, `:1353`: "Phase 8 shipped exactly this shape of defect" → "This shape of defect once shipped".
  - `trace_render_tests.cpp:869`: banner `phase 10: lookups` → `lookups`.
  - `trace_render_tests.cpp:1173`: "Phase 9's `[not checked]` against `[else]` is the precedent:" → "A constraint's `[not checked]` against `[else]` is the precedent:".
  - `trace_render_tests.cpp:1861`: "Final re-review of phase 11, L3: each escaped today, and no test said so." → "Each is escaped, and these tests say so."
  - `trace_render_tests.cpp:2016`: "(phase 12)" → drop.
  - `trace_render_tests.cpp:2022`: "The shared fixture of the phase 12 plan:" → "The shared series fixture:".
  - `trace_render_tests.cpp:2621`: "Phase 11's escaping reaches the series lines" → "The trace's escaping reaches the series lines".
  - `test/trace_tests.cpp:367`: banner `phase 8` → the feature its tests cover (read them: rounding, predicates and conditionals), same width.
  - `trace_tests.cpp:748`: banner `phase 10: lookups` → `lookups`.
  - `trace_tests.cpp:1808`: "(phase 12)" → drop.
  - `test/unit_cross_tu.hpp:12-13`: "and phase 4's Quantity<Unit> is the consumer that will depend on this holding" → "and `Quantity<…, Unit>` is the consumer that depends on this holding".
  - `test/unit_tests.cpp:92`: `// ---- a Unit is a template argument, which is what phase 4 needs ----` → `// ---- a Unit is a template argument, which is what Quantity needs ----`.
  - `unit_tests.cpp:1000-1001`: "and phase 4's Quantity<Unit> is the consumer that will depend on it" → "and `Quantity<…, Unit>` is the consumer that depends on it".
  - `test/vocabulary_tests.cpp:701`: "// Phase 13's kinds, added rather than multiplied in:" → "// The statistics kinds, added rather than multiplied in:".
  - `vocabulary_tests.cpp:1327`, `:1396`: "(phase 12)" → drop, same width.
  - `vocabulary_tests.cpp:1398`: "Phase 11's lesson: two separately verified things do not verify their join." → "Two separately verified things do not verify their join."

- [ ] **Step 4: Rename the two census test cases.**
  In `test/overflow_census_tests.cpp`:
    - `:709`: `TEST_CASE("census: phase 13's fixtures", "[census]")` → `TEST_CASE("census: statistics, outlier rejections and spreads over the six fixtures", "[census]")`.
    - `:742`: `TEST_CASE("census: phase 12's cumulative sums and interpolation", "[census]")` → `TEST_CASE("census: cumulative sums and interpolation", "[census]")`.

  Then prove nothing refers to the old names:

  ```bash
  git grep -n "phase 13's fixtures\|phase 12's cumulative" -- . ':!docs/superpowers'
  ```

  Expected: no output. Two facts make the rename safe:
    - the census program is run whole by `docs.numeric-headroom` and `census.*`, never by test name;
    - its tables are keyed by `emit("statistics", …)` and the row labels, which do not change.

- [ ] **Step 5: Check the search, and the words the search cannot see.**

  ```bash
  git grep -nE '\b[Pp]hase [0-9]+|\bspike\b|\bstep [0-9]+\)' -- include test examples tools docs/*.md README.md cmake CMakeLists.txt
  git grep -niE '\bthis phase\b|\bphase-[0-9]+|\bfix round [0-9]|\breview round [0-9]|final (re-)?review of' -- include test examples tools docs/*.md README.md cmake CMakeLists.txt
  ```

  Both must print nothing. The second search catches labels the issue's pattern misses. Fix each hit it prints under the same rules.

- [ ] **Step 6: The lanes' additions.**
  Any hit from Step 1 that Steps 2–4 did not list is a lane's addition. Rewrite it under the same rules, then re-run Step 5. List each such hit and its replacement in the report.

- [ ] **Step 7: Verify.**
  - `pwsh -NoProfile -File $S\cl.ps1 -Tree D:\formula-cpp -Exclude "^negative\."` must print `ALL OK`. Delta: **0 tests**. The census runs, and `docs.numeric-headroom` passes: no row label changed.
  - `pwsh -NoProfile -File $S\neg.ps1 -Tree D:\formula-cpp -Filter "method_tag_names_spelt_alike|method_variants_disagree|opaque_throwing_compute|overlay_constant_inside_opaque_series"` must print `ALL OK`. Only comments changed, so this run proves the files still compile to the same refusal.
  - `git diff --stat master...HEAD -- include` lists the headers. `git diff -U0 HEAD~1 -- include test cmake examples docs | grep '^[+-]' | grep -v '^[+-]\s*\(//\|#\|///\)' | grep -v '^+++\|^---'` must print only the two `TEST_CASE` lines of Step 4 and lines of `docs/quantities.md`: no code changed.

- [ ] **Step 8: Commit.**
  `$S\task9-commit.txt`:

  ```
  docs: state what each comment relies on instead of naming development history

  Comments, test names and section banners referred to stages of the
  project's development and to one-off experiments by labels a reader
  cannot look up. Each now says what it relies on: a measurement keeps its
  compilers and versions or points at the test that pins it, a banner
  names its feature, and a reference that added nothing is gone. Two census
  test cases are renamed after what they cover.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  ```powershell
  git add -A include test examples cmake docs/quantities.md
  git commit -F "$S\task9-commit.txt"
  ```

  No CHANGELOG entry: nothing a consumer sees changes.

### Task 10: Finish

The controller runs this task. Fix rounds go to `sdd-implementer`; the whole-branch review and each re-review go to `sdd-reviewer`.

**Files:** none of its own; fix rounds touch what their findings name.

**Interfaces:**
- Consumes: `feature/open-issues` after Task 9.
- Produces: the pull request, CI green, marked ready for review.

- [ ] **Step 1: Copy the documentation script.**
  `docs-pages.sh` is still in the earlier scratchpad. Copy it into `$S`, and point its default tree at `D:\formula-cpp`:

  ```bash
  cp /c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/e2d32a6f-b4c6-5a36-8b79-7a082a83081b/scratchpad/docs-pages.sh "$S_BASH/docs-pages.sh"
  ```

  `$S_BASH` is `/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad`. Under WSL the same directory is `/mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad`. Always pass `--tree`; never rely on the defaults, which name the earlier worktree.

  `windows-matrix.ps1` calls `cl.ps1` from the earlier scratchpad (`$scratch` at its line 5). That copy is identical to `$S\cl.ps1`, so the script works as it is. Pass `-Tree D:\formula-cpp` every time.

- [ ] **Step 2: Whole-branch review.**
  Dispatch `sdd-reviewer` over `master..feature/open-issues`. Its brief names:
    - the spec, `docs/superpowers/specs/2026-10-04-open-issues-design.md`;
    - this plan's Global Constraints and Review Focus;
    - every task report under `D:\formula-cpp\.superpowers\sdd\2026-10-04-open-issues\`.

  It reviews the whole diff, not task by task. It checks in particular:
    - every acceptance criterion of the ten issues;
    - the breaking-change entry for the scaled dimensionless unit;
    - that no public text names a task, lane, plan, reviewer or local note;
    - that the pinned refusals of #20 that became answers were checked against an independent computation.

  Each finding goes to a fix round (`sdd-implementer`, with the finding and the reviewer's evidence). Then a scoped `sdd-reviewer` re-review of the fix, until no finding is open. Record each finding and its outcome in `$S\task10-review.md`.

- [ ] **Step 3: All eight presets.**

  ```powershell
  pwsh -NoProfile -File $S\windows-matrix.ps1 -Tree D:\formula-cpp
  ```

  Must print `MATRIX OK`: cl-debug, cl-release, clangcl-debug, clangcl-release.

  ```powershell
  wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad/posix-matrix.sh --tree /mnt/d/formula-cpp
  ```

  Must print `MATRIX OK`: gcc-release with g++-14, and clang-debug, clang-release and clang-ubsan with clang++-20. Run it in the foreground (it takes a while; use the Bash tool's longest timeout, or `run_in_background` and wait for its completion notice).

  The negative tests run inside each preset's `ctest`. Their `EXPECT_COUNT` is checked only off MSVC, so clang-cl, g++ and clang++ check the new counts.

  Each failure goes to a fix round with the failing preset's log (`D:\formula-cpp\out\matrix\<preset>.log`, or `out/build/<preset>-wsl.*.log`). Then this step again, every preset.

- [ ] **Step 4: Doxygen and the site.**

  ```powershell
  wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad/docs-pages.sh --tree /mnt/d/formula-cpp
  ```

  Must print `DOXYGEN OK`: warnings fail the `formula-cpp-docs-api` target.

  ```powershell
  Set-Location D:\formula-cpp
  python -m mkdocs build --strict --site-dir out\mkdocs-site
  ```

  Must exit 0 with no warning. `--site-dir` keeps the build out of the tracked tree.

- [ ] **Step 5: The pull request.**
  Push `feature/open-issues` (`git push -u origin feature/open-issues`). Open it as a draft with `contour-workflows:draft-pr`, so CI runs before it asks for review.
  - **Title:** `Trace units for unnamed and inverse units, 128-bit logarithm arguments, and the series refusal for explain`.
  - **Body**, written for a reader of the repository:

  ```markdown
  ## Traces

  - **A scaled dimensionless unit must have a symbol.** A dimensionless quantity in a unit with a scale and no symbol
    (hundredths, say) traced as a bare number in that scale: one half read `50`. Such a unit is now refused where it is
    declared. **Breaking:** give the unit a symbol (for example `"%"`), or declare the quantity in scale 1.
  - **A rounding names the unit its places count in.** `round(#1, to 2 dp)` on a quantity in a unit with no symbol now
    reads `round(#1, to 2 dp of 1/1000 kg)`, in the trace and in the rendered formula alike.
  - **An inverse unit cannot be misread as part of a fraction.** A coherent unit with nothing above the slash is
    written with negative exponents: `20000/413 kg^-1`, not `20000/413 1/kg`. Units with a numerator keep the slash
    (`m/s`).
  - **A precision limit's first pass reads in its level's unit.** A constant level declared in grams reads in grams on
    both lines, not in kilograms on the second.
  - A snap's "on a permitted value" test now explains why it compares the stored pairs, and a missed lookup no longer
    spells a bound it never writes.

  ## Numerics

  - **`rounded_ln`, `rounded_log10` and `rounded_exp` take 128-bit arguments.** The kernel narrowed its argument to 64
    bits and refused anything wider with `Overflow`; `ln 2^70` now answers. `rounded_exp`'s cap moves from 44 to
    887/10. Below the cap, whether an answer fits is decided by the result: e^88 at 0 places is answered.
  - **The bit widths two guides quote are measured.** The least-squares fit's widest intermediate is a generated column
    of the headroom page's census table, and the guide's figure for the fit's wide integers now reads 384 bits, from
    the constant. The opaque-operation example's coefficient widths are pinned by a test.

  ## Diagnostics and documentation

  - `explain` and `checked_explain` handed a series refuse it in the library's words, pointing at `explain_series`,
    instead of failing with "no matching function".
  - Comments, test names and section banners say what they rely on, not which stage of development produced them.
  - The README links the tracing guide once, and the 128-bit design document describes the checked add and subtract
    as built.

  Closes #11
  Closes #13
  Closes #14
  Closes #15
  Closes #16
  Closes #17
  Closes #18
  Closes #19
  Closes #20
  Closes #21
  ```

  Before pushing, check the body's figures against the branch:
    - the exp cap (887/10);
    - the kernel width (384);
    - that `e^88 at 0 places` is answered by a test.

  Change any figure the branch does not support.

- [ ] **Step 6: CI.**
  Watch the run (`gh pr checks --watch`). Every job must pass: the Windows, Linux and macOS (AppleClang) legs, install-and-consume, and the pages build. A failure goes to `contour-workflows:fix-ci` or a fix round, then a new push. Re-run the affected local preset first if the failure is a compiler the local matrix also covers.

- [ ] **Step 7: Ready for review.**
  When CI is green on the pushed head: `gh pr ready`. The owner reviews and merges, with a merge commit, which closes the ten issues. **Never merge it.**
