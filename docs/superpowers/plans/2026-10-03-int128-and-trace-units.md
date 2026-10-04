# Int128 and Trace Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close issue #2 and issue #1 in one pull request.

- **#2:** every computed value in a trace prints with a unit, read off its operand steps where that is safe and the coherent unit's symbol otherwise.
- **#1:** `Rational` stores a new `formula::Int128`, so realistic laboratory statistics stop overflowing.

**Architecture:** Two independent changes, built in two lanes that merge at the end.

- **Trace units.** The recorder (`RecordingSink::produced`, `trace.hpp`) chooses a borrowed unit from the operand steps. The renderer (`value_in_declared_unit`, `trace_render.hpp`) writes the coherent unit's symbol after any dimensioned value whose unit has none.
- **Int128.**
  - A new header, `int128.hpp`, adds `Int128`. It is held as two 64-bit words everywhere; the compiler's `__int128` computes where it has one, and portable `constexpr` code everywhere else.
  - A prep task moves every reader of `numerator()` and `denominator()` onto width-agnostic helpers while `Rational` is still 64-bit, with no behaviour change.
  - A switch task then flips `Rational::Int` to `Int128` and re-measures the overflow census.

**Tech Stack:** C++23, header-only; Catch2 3.6 (`STATIC_REQUIRE`), the `test/negative/` harness, the CTest docs and census checks, Python 3 for `tools/census/exact_sizes.py`.

**Specs (read both before any task):**

- `docs/superpowers/specs/2026-10-03-trace-units-design.md`: issue #2, Tasks 1–3.
- `docs/superpowers/specs/2026-10-03-int128-rational-design.md`: issue #1, Tasks 4–7.

**Two refinements of the `Int128` spec, decided while planning, which bind Tasks 4–7.** The spec is amended to match, in the commit that adds this plan:

1. **No conversion to a built-in integer at all.** The approved spec listed `explicit` conversions. Without them, every `static_cast<std::uint64_t>(r.denominator())` in the library becomes a compile error at the switch instead of a silent cut to 64 bits. Narrowing is spelled `to_int64()` and `to_uint64()`, which return `std::optional`.
2. **One storage layout.** `Int128` is two `std::uint64_t` words on every compiler. Multiplication, division and the overflow check run on the compiler's `unsigned __int128` where it has one, converting in and out, which optimises to nothing. The spec's native-storage variant would have meant two class layouts.

**Order and lanes:**

- Lane A, trace units, is Tasks 1 → 2 → 3, in worktree `D:\formula-cpp\.claude\worktrees\int128-and-trace-units`, on branch `feature/int128-and-trace-units`.
- Lane B, Int128, is Tasks 4 → 5 → 6 → 7, in worktree `D:\formula-cpp\.claude\worktrees\int128-core`, on branch `feature/int128-core`. That branch is cut from `feature/int128-and-trace-units` at the commit that adds this plan.
- The lanes run at the same time and share no file except `CHANGELOG.md` and `docs/numeric-headroom.md` (generated).
- Task 8 merges Lane B into `feature/int128-and-trace-units`, regenerates what both touched, and runs the full verification.

**Line anchors** were read at `ff148f1`. Find each place by the name quoted beside its anchor, never by the number alone.

---

## Global Constraints

These bind every task.

- **C++23, header-only.** Nothing beyond the standard library in `include/`.
- **Worktrees.** Each lane works only in its own worktree: Lane A in `D:\formula-cpp\.claude\worktrees\int128-and-trace-units`, Lane B in `D:\formula-cpp\.claude\worktrees\int128-core`. Never touch `D:\formula-cpp` itself or the other lane's worktree.
- **Gates run from PowerShell.** Bash's pipe mis-encodes `°` and fails `docs.*-output`.
    - `$S = C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\e2d32a6f-b4c6-5a36-8b79-7a082a83081b\scratchpad`
    - `$T` = the lane's worktree.
- **Verify, the per-task gate.** Every command must print `ALL OK` / `MATRIX OK`:
    1. `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."`: the full cl-debug build, then every test except the negative ones.
    2. `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "<regex>"`, only for a task that adds or touches negative tests. It runs the negatives matching `<regex>` on cl-debug **and** clangcl-debug; `EXPECT_COUNT` is checked only off MSVC.
    3. Task 4 only, because the native `__int128` path exists only on GCC and Clang: `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/e2d32a6f-b4c6-5a36-8b79-7a082a83081b/scratchpad/posix-matrix.sh --tree /mnt/d/formula-cpp/.claude/worktrees/int128-core --presets "gcc-release clang-debug"`.
- **Quick loop:** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "<ctest -R regex>"`.
- **Run builds and tests in the foreground.** Never wait on a background monitor of your own. Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is a build error. Prove from ctest's count that a filter selected something before trusting it. Catch2 splits test filters on commas.
- **Baseline at `ff148f1`:** cl-debug passes 1527 of 1527 non-negative tests; clangcl-debug passes 560 of 560 negative tests. Each task reports its total and the difference from the previous task's, which must equal its stated delta.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:**
    - an SPDX header on every file, and no `NOLINT`;
    - core public headers include no `<string>`, `<vector>`, `<format>`, `<iostream>` or `<print>` (`hygiene.headers`), so formatters live in `format.hpp`;
    - every public `static_assert` message begins `formula: `;
    - a new public header goes into the install `FILE_SET` (`CMakeLists.txt:41-106`, `hygiene.installed-headers`) and into `test/consumer_globals_tests.cpp`'s includes (`hygiene.consumer-globals`).
- **Consumer globals.** `test/consumer_globals_tests.cpp:130-153` declares a list of `int` globals. No new parameter or local may reuse one: cl C4459 and g++ `-Wshadow` turn a reuse into a consumer's build error. Names on the list that this plan is tempted by:
    - `a`–`z`;
    - `value`, `values`, `result`, `sum`, `count`, `digits`, `numerator`, `denominator`, `quotient`, `sign`, `width`, `scale`, `scaled`, `factor`;
    - `low`, `high`, `lower`, `upper`, `limit`, `pattern`, `root`, `step`, `text`, `unit`, `kind`, `first`, `last`, `left`, `right`, `operand`, `number`, `total`, `range`, `rest`.

  Use descriptive names (`leftOperand`, `magnitudeOf`, `highWord`, `rootSoFar`). Member names are not affected.
- **Behaviour rule (#1 spec §4).** Every computation that answers today gives the same answer afterwards. Some that are refused with `Overflow` today now answer. A refusal is never turned into a different number.
- **Display rule (#2 spec §2).** No computed dimensioned value prints without a unit, and no number is shown in a unit other than the one written after it.
- **No internal labels in public text.** Code, docs, commit messages and the PR never name tasks, lanes, plans or reviewers, and never cite local progress notes. Every sentence must make sense to a reader who never saw this plan.
- **No third-party standard content.** Cite only `Example Standard N:YYYY`. Fixture values are plainly invented.
- **Style.** Do not run clang-format on existing files; match the surrounding style by hand. Every new public entity and member gets a Doxygen `///` comment.
- **Printing** is `std::print` / `std::println` only. Never `printf`, `puts` or iostream.
- **Error handling.** Check every `std::expected` result before use. Never unwrap unchecked, and never switch to a throwing form to shorten code.
- **Newest GCC only** (g++-14). No workaround for an older compiler.
- **Commits.** Conventional style (`feat(trace): …`, `fix: …`, `test: …`, `docs: …`). Every message ends with:

  ```
  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  Commit with `git commit -F <file>`; a here-string passed to `-F -` does not work in PowerShell.
- **CHANGELOG.md:** entries go under `## [Unreleased]` (create it at the top if absent, as `cmake/CheckChangelog.cmake` requires), in `### Added` / `### Changed` subsections.

## Review Focus

The five inputs these specs imply but no obvious test reaches, most likely to bite first. Each one's test is in the task named.

1. **A unit with no symbol that is not the coherent one** (`UnnamedGram`: grams with an empty symbol, in `test/rejection_tests.cpp:45`). The value must be shown in the coherent unit with its symbol (`3/1000 kg`), never as a bare number in an unnamed scale. Task 1.
2. **`Int128`'s minimum, −2^127, as a numerator.**
    - `Rational::make(min, 1)` succeeds;
    - `checked_negate`, `checked_abs` and `checked_reciprocal` of it are refused;
    - `fraction_text` spells all 39 digits;
    - the magnitude 2^127 is handled.

    Task 6.
3. **A `Rational` too wide for a 64-bit structural type** (`breakpoint(Rational)`, `band(Rational, Rational)`, `point_in`, `unit_quotient`, `rounded_square_root_in`'s units). Each site refuses as its contract says, and nothing is truncated. Task 6.
4. **The longest spelling.** A 39-digit numerator over a 39-digit denominator, and a 39-digit whole part rounded to 18 places with the `≈` marker, both fit `NumberText`. Task 5 (capacity) and Task 6 (values that reach it).
5. **Negative division, remainder and right shift in `Int128`.** They truncate toward zero, the remainder takes the dividend's sign, and `>>` is arithmetic. The native and portable routes agree, and so does the double conversion on ties. Task 4.

## Execution

- **Briefs and reports.** The controller writes each task's brief to `D:\formula-cpp\.superpowers\sdd\2026-10-03-int128-and-trace-units\task-N-brief.md`. The implementer writes its report beside it, as `task-N-report.md`. A report states:
    - the commits;
    - the test total and its delta;
    - each rule in *Global Constraints* that the task touched, and how it was kept;
    - anything it could not do.
- **Agents.** Every task is implemented by `sdd-implementer` and reviewed by `sdd-reviewer`. A fix round goes back to `sdd-implementer`.
- **Negative tests**, where a task adds one: `test/negative/<name>.cpp`, plus `formula_add_negative_test(<name> "<expected>" …)` in `test/CMakeLists.txt`. Register it first with a deliberately wrong expected text and watch it fail. Then register the right text and watch it pass. Then delete the guard it pins, confirm the case compiles, and restore the guard with a plain write.

---

## Lane A — units on every computed trace value (issue #2)

### Task 1: The coherent unit's symbol at render time

The renderer gives every dimensioned value whose unit has no symbol the coherent unit and its spelling. This is rule (c) of the trace-units spec, §4. No recording changes yet, so `#2 * #3 = 1239/500000` becomes `1239/500000 kg`, and Task 2 turns it into `1239/500 g`.

**Files:**
- Modify: `include/formula-cpp/trace_render.hpp`:
    - `coherent_unit_text` (`:2473-2526`) moves above `value_in_declared_unit` (`:1484-1520`);
    - `value_in_declared_unit` changes;
    - so do `rejection_value_text`'s squared branch (`:2213-2238`) and `opaque_value_text` (`:2528-2543`).
- Create: `test/trace_shown_unit_tests.cpp`, registered in `test/CMakeLists.txt`'s `add_executable(formula-cpp-tests` list (`:22`).
- Re-pin:
    - every `test/*.cpp` expectation the change moves;
    - `examples/display.cpp:208`, and the example pass regexes in `examples/CMakeLists.txt`;
    - the ```` ```text ```` blocks of the checked guides;
    - `docs/gallery.md` (regenerated);
    - `docs/numeric-headroom.md` (regenerated, only if `docs.numeric-headroom` fails).

**Interfaces:**
- Consumes: `detail::coherent_unit_text(Dimension) -> std::string`, `coherent(Dimension) -> Unit`, `view(Symbol) -> std::string_view`, `Dimension::operator*`.
- Produces: `detail::value_in_declared_unit(Step<Rational> const&, std::optional<Rational> const&, NumberStyle) -> std::string`. Its signature is unchanged; its rule is the new one. Task 3's test reads it.

- [ ] **Step 1: Write the failing tests.** Create `test/trace_shown_unit_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// A trace step's number is always shown with the unit it is in: borrowed from
// the operand steps where that is safe, the coherent unit's symbol otherwise,
// and nothing only for a dimensionless value.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

struct SampleMass: formula::Quantity<SampleMass, "m", "sample mass", unit::Gram>
{
};
struct TareMass: formula::Quantity<TareMass, "m_t", "tare mass", unit::Gram>
{
};
struct Edge: formula::Quantity<Edge, "a", "edge length", unit::Millimetre>
{
};
struct Breadth: formula::Quantity<Breadth, "b", "edge breadth", unit::Millimetre>
{
};

// Grams with no symbol: a scale the number alone cannot name.
inline constexpr formula::Unit UnnamedGram { .dimension = formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000 };
struct UnnamedMass: formula::Quantity<UnnamedMass, "m_u", "mass in an unnamed unit", UnnamedGram>
{
};

template <typename Expression, typename Bound>
std::string trace_text(Expression const& formulaExpression, Bound const& inputs)
{
    formula::Trace<> recorded {};
    formula::RecordingSink<> recordingSink { recorded };
    (void) formula::checked_evaluate_si<Rational>(formulaExpression, inputs, recordingSink);
    return formula::render_trace(recorded, { .maxSteps = 20 });
}
} // namespace

TEST_CASE("a product of two lengths names the coherent unit of an area", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<Edge> { Rational { 5 } },
                                             formula::Measured<Breadth> { Rational { 8 } });
    CHECK(trace_text(var<Edge> * var<Breadth>, inputs)
          == "1. a = 5 mm\n"
             "2. b = 8 mm\n"
             "3. #1 * #2 = 1/25000 m^2\n");
}

TEST_CASE("a value in a unit with no symbol is shown in the coherent unit, with its symbol", "[trace-render][shown-unit]")
{
    // 3 of an unnamed gram is 3/1000 kg: shown bare, the 3 would claim a scale
    // nothing on the line names.
    auto const inputs = formula::environment(formula::Measured<UnnamedMass> { Rational { 3 } });
    CHECK(trace_text(var<UnnamedMass> * Rational { 2 }, inputs).starts_with("1. m_u = 3/1000 kg\n"));
}

TEST_CASE("a dimensionless value is still a bare number", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } });
    CHECK(trace_text(var<SampleMass> / var<TareMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. m_t = 7 g\n"
             "3. #1 / #2 = 59/10\n");
}
```

  Add `trace_shown_unit_tests.cpp` to `add_executable(formula-cpp-tests` in `test/CMakeLists.txt`, after `trace_render_tests.cpp`.

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "shown"`.
  Expected: two of the three fail, on `1/25000` with no `m^2` and on `m_u = 3`. The dimensionless case passes. Check that ctest reports 3 tests.

- [ ] **Step 3: Move `coherent_unit_text` above `value_in_declared_unit`.** Cut the whole function, with its doc comment, from `:2473-2526` and paste it immediately above `value_in_declared_unit`'s doc comment (`:1484`). Change only the comment's last sentence, from "For an opaque output shown in no input's unit, so that a slope in metres per second does not read as a pure number." to: "Written after every dimensioned value whose unit has no symbol, so that a slope in metres per second does not read as a pure number."

- [ ] **Step 4: Rewrite `value_in_declared_unit`.** Replace its body and the paragraph of its comment that says the number is spelled "never padded in a unit nobody declared". The body becomes:

```cpp
    [[nodiscard]] inline std::string value_in_declared_unit(Step<Rational> const& recorded,
                                                            std::optional<Rational> const& storedValue,
                                                            NumberStyle numberStyle)
    {
        if (!storedValue.has_value())
            return std::string { NotMeasuredText };

        // A unit with no symbol cannot say what scale its number is on. A
        // dimensioned value is then shown in the coherent unit and followed by
        // that unit's spelling (`coherent_unit_text`), so that no computed
        // value prints as a bare number and no number is shown in a scale its
        // line does not name. A dimensionless value is a bare number either
        // way.
        bool const spellsCoherent = view(recorded.unit.symbolText).empty() && !(recorded.dimension == dim::Scalar);
        Unit const shownUnit = spellsCoherent ? coherent(recorded.dimension) : recorded.unit;
        std::expected<Rational, ArithmeticError> const shown =
            checked_convert(*storedValue, coherent(recorded.dimension), shownUnit);
        // Unreachable for a `Step` the recorder built -- it records a unit of
        // the step's own dimension -- but a `Step` is a public aggregate and a
        // caller may fill one in by hand. Refusing to print is the only
        // honest answer: the alternative is a number in a scale the line
        // claims it is not in.
        if (!shown)
            return not_shown_text(shown.error());
        std::expected<NumberText, ArithmeticError> const spelled = checked_shown_text(*shown, numberStyle, shownUnit);
        if (!spelled)
            return not_shown_text(spelled.error());

        std::string valueText { spelled->view() };
        std::string const unitSymbol = spellsCoherent ? coherent_unit_text(recorded.dimension) : unit_symbol_text(shownUnit);
        if (!unitSymbol.empty())
            valueText += " " + unitSymbol;
        return valueText;
    }
```

  `coherent(...)` has no symbol, so `detail::is_unlabelled` still holds for it. A coherent value is still never padded, and its approximation still extends to the first significant digit. Only the suffix is new.

- [ ] **Step 5: Squared deviations.** In `rejection_value_text`'s squared branch, replace from `Unit const shownUnit = recorded.unit;` down to the `" " + unitSymbol + "2"` append with:

```cpp
        bool const spellsCoherent = view(recorded.unit.symbolText).empty() && !(recorded.dimension == dim::Scalar);
        Unit const shownUnit = spellsCoherent ? coherent(recorded.dimension) : recorded.unit;
        std::expected<Rational, ArithmeticError> const magnitude =
            Rational::make(shownUnit.magnitudeNumerator, shownUnit.magnitudeDenominator);
        std::expected<Rational, ArithmeticError> const magnitudeSquared =
            magnitude.has_value() ? checked_mul(*magnitude, *magnitude) : magnitude;
        std::expected<Rational, ArithmeticError> const shown =
            magnitudeSquared.has_value() ? checked_div(si, *magnitudeSquared) : magnitudeSquared;
        if (!shown)
            return not_shown_text(shown.error());
        std::expected<NumberText, ArithmeticError> const spelled =
            checked_number_text(*shown, trimmed(numberStyle), shownUnit);
        if (!spelled)
            return not_shown_text(spelled.error());
        std::string valueText { spelled->view() };
        // A unit with a symbol squares as the library writes squares (`g2`);
        // the coherent one is spelt from its bases (`K^2`).
        if (spellsCoherent)
            valueText += " " + coherent_unit_text(recorded.dimension * recorded.dimension);
        else if (std::string const unitSymbol = unit_symbol_text(shownUnit); !unitSymbol.empty())
            valueText += " " + unitSymbol + "2";
        return valueText;
```

- [ ] **Step 6: `opaque_value_text` stops appending twice.** Its body becomes the following, and its comment drops "and -- when that unit has no symbol of its own but a dimension -- followed by the coherent unit's spelling":

```cpp
        Step<Rational> outputShape {};
        outputShape.dimension = dimension;
        outputShape.unit = shownUnit;
        return value_in_declared_unit(outputShape, storedValue, numberStyle);
```

- [ ] **Step 7: Run the new tests.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "shown"`. Expected: 3 of 3 pass.

- [ ] **Step 8: Re-pin the tests.** Run Verify step 1 and collect every failing case. For each failing expectation, decide which kind of change it is:
    1. A dimensioned value that printed bare now has its coherent spelling appended. Examples:
        - `#2 * #3 = 1239/500000` → `#2 * #3 = 1239/500000 kg`;
        - `2. #1^2 = 36` → `2. #1^2 = 36 kg^2`;
        - `sample_variance(#1) = 427/125000000` → `… kg^2`;
        - `if #1 > #2 then #3 = 60000000` → `… 60000000 kg/(m s^2)`;
        - `#1 - #2 = 108000000` → `… 108000000 m^2 kg/s^2`;
        - `#1 * #2 = 80` (EUR) → `80 EUR`;
        - a °C squared deviation → `… K^2`.
    2. A value in a unit with no symbol (`UnnamedGram` in `test/rejection_tests.cpp:45`) now shows in the coherent unit with its spelling.

  Update each expectation to the program's new text, line by line. **Any other difference is a defect: stop and report it.** That includes a dimensionless value gaining a unit, a number changing other than by the unit conversion of kind 2, and padding appearing on a coherent value. Test comments that state the old rule are rewritten in Task 3, not here. The largest files:

    | File | Lines | Notes |
    |---|---|---|
    | `calculation_trace_tests.cpp` | 38 | |
    | `trace_render_tests.cpp` | 32 | |
    | `vocabulary_tests.cpp` | 27 | |
    | `retry_tests.cpp` | 21 | |
    | `rejection_tests.cpp` | 21 | 9 are dimensionless and do not change |
    | `precision_tests.cpp` | 13 | |
    | `join_tests.cpp` | 5 | |
    | `statistics_tests.cpp` | 3 | |
    | `record_trace_tests.cpp` | 2 | |
    | `rounded_output_tests.cpp`, `record_render_tests.cpp`, `quantity_alias_tests.cpp`, `overlay_tests.cpp` | 1 each | |
    | `opaque_tests.cpp` | — | opaque outputs already showed the spelling; they must not change |

- [ ] **Step 9: Re-pin the examples.**
    - `examples/display.cpp:208` checks `tareTraceText.contains("3. #1 - #2 = 0.12\n")`. Update it to the new text of that line, which keeps `0.12` and gains ` kg`; keep its message ("a unit nobody declared is not padded").
    - Update every `formula_add_example(... PASS_REGULAR_EXPRESSION ...)` in `examples/CMakeLists.txt:14-326` that a failing `example.*` test names.

- [ ] **Step 10: Regenerate the gallery and the checked guides.**
    1. `out\build\cl-debug\tools\gallery\formula-cpp-gallery.exe docs\gallery.md`, from `$T`, in PowerShell.
    2. For each failing `docs.<guide>-output` test, run its example (`out\build\cl-debug\examples\<example>.exe`) and replace, in the guide's ```` ```text ```` blocks, exactly the lines that changed with the program's lines.
    3. Leave plain ```` ``` ```` blocks and prose for Task 3.
    4. If `docs.numeric-headroom` fails, build the target `formula-cpp-census-page` (`cmake --build --preset cl-debug --target formula-cpp-census-page`) and confirm with `git diff docs/numeric-headroom.md` that only the examples table changed. Rendering into the coherent unit forms no new integers, so it most likely does not fail.

- [ ] **Step 11: Verify.** Verify step 1 must print `ALL OK`. Delta: **+3 tests**.

- [ ] **Step 12: Commit.**
  Message: `feat(trace): write the coherent unit after every computed value that has no unit of its own`.
  Body: the rule from Step 4's comment, in two sentences, and that a value in a unit with no symbol is shown in the coherent unit. Then the `Signed-off-by` line.

### Task 2: Units borrowed from the operand steps

Rules (a), (b), negation, `abs` and the pass-through rule of the trace-units spec, §3. After this task, `#2 * #3 = 1239/500000 kg` reads `#2 * #3 = 1239/500 g`.

**Files:**
- Modify: `include/formula-cpp/trace.hpp`:
    - `detail::scaled_operand` (`:2225-2241`) gains a `BinaryNode` specialisation;
    - new helpers sit beside `operand_unit_or` (`:2141-2156`): `same_scale_and_symbol`, `binary_unit_or`, `UnarySide`, `restated_unit_or`;
    - `RecordingSink::produced` (`:3250-3269`) applies them.
- Test: `test/trace_shown_unit_tests.cpp`.
- Re-pin: as in Task 1, Steps 8–10.

**Interfaces:**
- Consumes: `detail::borrowable(Unit const&)`, `detail::borrowable_for_a_point(Unit const&)`, `detail::operand_unit_or(...)`, `detail::RecordsOwnStep<N>`, `detail::BinarySides<N>`, `detail::StepKindOf<N>`.
- Produces, all in `formula::detail`:
    - `constexpr bool same_scale_and_symbol(Unit const&, Unit const&) noexcept`;
    - `template <typename N, typename Rep> constexpr Unit binary_unit_or(std::vector<Step<Rep>> const&, std::vector<std::size_t> const&, Unit fallback) noexcept`;
    - `template <typename N> struct UnarySide`, with `using inner = Operand;` for `UnaryNode` and `AbsoluteValueNode`;
    - `template <typename Rep> constexpr Unit restated_unit_or(std::vector<Step<Rep>> const&, std::vector<std::size_t> const&, Dimension, std::optional<Rep> const&, Unit fallback) noexcept`.

- [ ] **Step 1: Write the failing tests.** Append to `test/trace_shown_unit_tests.cpp`, inside the anonymous namespace after `UnnamedMass`:

```cpp
struct HeavyMass: formula::Quantity<HeavyMass, "M", "heavy mass", unit::Kilogram>
{
};
struct StartTemperature: formula::Quantity<StartTemperature, "T_0", "start temperature", unit::Celsius>
{
};
struct EndTemperature: formula::Quantity<EndTemperature, "T_1", "end temperature", unit::Celsius>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

// Grams read to four decimals: grams still, whatever the precision.
inline constexpr formula::Unit FineGram { .dimension = formula::dim::Mass,
                                          .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 1000,
                                          .symbolText = formula::symbol("g"),
                                          .decimals = 4 };
struct FineMass: formula::Quantity<FineMass, "m_f", "finely read mass", FineGram>
{
};

template <typename Expression, typename Bound>
formula::Trace<> recorded_trace(Expression const& formulaExpression, Bound const& inputs)
{
    formula::Trace<> recorded {};
    formula::RecordingSink<> recordingSink { recorded };
    (void) formula::checked_evaluate_si<Rational>(formulaExpression, inputs, recordingSink);
    return recorded;
}
```

  Then append these test cases at the end of the file:

```cpp
TEST_CASE("a value scaled by a pure number reads in its own unit", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    // On the right, as the outlier-rejection limit 6 % of the mean is written.
    CHECK(trace_text(Rational { 3, 50 } * var<SampleMass>, inputs)
          == "1. 3/50\n"
             "2. m = 413/10 g\n"
             "3. #1 * #2 = 1239/500 g\n");
    // On the left.
    CHECK(trace_text(var<SampleMass> * Rational { 3, 50 }, inputs)
          == "1. m = 413/10 g\n"
             "2. 3/50\n"
             "3. #1 * #2 = 1239/500 g\n");
    // Divided by a pure number.
    CHECK(trace_text(var<SampleMass> / Rational { 2 }, inputs)
          == "1. m = 413/10 g\n"
             "2. 2\n"
             "3. #1 / #2 = 413/20 g\n");
    // A pure number divided by a mass is no mass: the coherent unit, 1/kg.
    CHECK(trace_text(Rational { 2 } / var<SampleMass>, inputs)
          == "1. 2\n"
             "2. m = 413/10 g\n"
             "3. #1 / #2 = 20000/413 1/kg\n");
}

TEST_CASE("a sum of two values in one unit reads in it, at the finer precision", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } },
                                             formula::Measured<FineMass> { Rational { 12345, 10000 } });
    CHECK(trace_text(var<SampleMass> - var<TareMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. m_t = 7 g\n"
             "3. #1 - #2 = 343/10 g\n");
    // Grams declared at different decimals are grams: the sum is shown in
    // grams, and at the finer of the two precisions.
    formula::Trace<> const mixedPrecision = recorded_trace(var<SampleMass> + var<FineMass>, inputs);
    REQUIRE(mixedPrecision.steps.size() == 3);
    CHECK(formula::view(mixedPrecision.steps[2].unit.symbolText) == "g");
    CHECK(mixedPrecision.steps[2].unit.decimals == 4);
}

TEST_CASE("a sum of values in two units reads in the coherent unit", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<HeavyMass> { Rational { 1 } });
    CHECK(trace_text(var<SampleMass> + var<HeavyMass>, inputs)
          == "1. m = 413/10 g\n"
             "2. M = 1 kg\n"
             "3. #1 + #2 = 10413/10000 kg\n");
}

TEST_CASE("a difference of two Celsius readings is an interval in kelvin, not a reading", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } },
                                             formula::Measured<EndTemperature> { Rational { 25 } });
    CHECK(trace_text(var<EndTemperature> - var<StartTemperature>, inputs)
          == "1. T_1 = 25 \xc2\xb0" "C\n"
             "2. T_0 = 20 \xc2\xb0" "C\n"
             "3. #1 - #2 = 5 K\n");
}

TEST_CASE("a negation and an absolute value keep their operand's unit, but not an offset one", "[trace-render][shown-unit]")
{
    auto const grams = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } });
    CHECK(trace_text(-var<SampleMass>, grams)
          == "1. m = 413/10 g\n"
             "2. -#1 = -413/10 g\n");
    CHECK(trace_text(formula::abs(-var<SampleMass>), grams)
          == "1. m = 413/10 g\n"
             "2. -#1 = -413/10 g\n"
             "3. abs(#2) = 413/10 g\n");
    // -(20 degC) is no reading at -20 degC: the coherent unit.
    auto const celsius = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } });
    CHECK(trace_text(-var<StartTemperature>, celsius)
          == "1. T_0 = 20 \xc2\xb0" "C\n"
             "2. -#1 = -5863/20 K\n");
}

TEST_CASE("a conditional reads in its chosen branch's unit, offset or not", "[trace-render][shown-unit]")
{
    auto const strengths = formula::environment(formula::Measured<Strength> { Rational { 60 } });
    CHECK(trace_text(formula::when(var<Strength> > formula::constant<unit::Megapascal>(Rational { 473, 10 }),
                                   var<Strength>,
                                   formula::constant<unit::Megapascal>(Rational { 0 })),
                     strengths)
          == "1. f = 60 MPa\n"
             "2. 473/10 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60 MPa\n");
    // A branch's value is a point on its scale, so a Celsius branch reads in
    // degrees Celsius.
    auto const readings = formula::environment(formula::Measured<StartTemperature> { Rational { 20 } },
                                               formula::Measured<EndTemperature> { Rational { 25 } });
    CHECK(trace_text(formula::when(var<EndTemperature> > var<StartTemperature>, var<EndTemperature>, var<StartTemperature>),
                     readings)
          == "1. T_1 = 25 \xc2\xb0" "C\n"
             "2. T_0 = 20 \xc2\xb0" "C\n"
             "3. T_1 = 25 \xc2\xb0" "C\n"
             "4. if #1 > #2 then #3 = 25 \xc2\xb0" "C\n");
}
```

  The negated Celsius value: −(293.15 K) = −5863/20 K. If the evaluator refuses a negated offset reading for its own reasons, take the line the program prints, as long as it is in `K`. A conditional's exact spelling (`if #1 > #2 then #3`) is pinned at `test/trace_render_tests.cpp:520-530`; if the program's line differs only in that spelling, follow the program.

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "shown"`. Expected: the six new cases fail, each on a coherent-unit line where a borrowed unit is expected. The three Task 1 cases still pass.

- [ ] **Step 3: Extend `scaled_operand` to single values.** After the `ElementwiseBinaryNode` specialisation (`trace.hpp:2234-2241`), add:

```cpp
    template <BinaryOperator Op, Node Left, Node Right>
    inline constexpr std::optional<std::size_t> scaled_operand<BinaryNode<Op, Left, Right>> =
        Op == BinaryOperator::Multiply && Left::dimension == dim::Scalar && !(Right::dimension == dim::Scalar)
            ? std::optional<std::size_t> { 1 }
        : (Op == BinaryOperator::Multiply || Op == BinaryOperator::Divide) && Right::dimension == dim::Scalar
                && !(Left::dimension == dim::Scalar)
            ? std::optional<std::size_t> { 0 }
            : std::nullopt;
```

  In the primary's comment, change "of @p S, an elementwise binary node," to "of @p S, a binary node -- a single value's or an elementwise one --".

- [ ] **Step 4: Add the helpers.** Directly after `operand_unit_or` (`:2156`), and before `RecordsOwnStep`, add `same_scale_and_symbol`:

```cpp
    /// Whether @p leftUnit and @p rightUnit show values on one scale under one
    /// name: the same dimension, factor, offset and symbol. Their declared
    /// decimals and bounds may differ -- two gram readings are grams whatever
    /// precision each was declared at -- which is why this is not
    /// `Unit::operator==`.
    [[nodiscard]] constexpr bool same_scale_and_symbol(Unit const& leftUnit, Unit const& rightUnit) noexcept
    {
        return leftUnit.dimension == rightUnit.dimension && leftUnit.magnitudeNumerator == rightUnit.magnitudeNumerator
               && leftUnit.magnitudeDenominator == rightUnit.magnitudeDenominator
               && leftUnit.offsetNumerator == rightUnit.offsetNumerator
               && leftUnit.offsetDenominator == rightUnit.offsetDenominator
               && view(leftUnit.symbolText) == view(rightUnit.symbolText);
    }
```

  After `scaled_operand`'s specialisations (they must precede it), add `binary_unit_or`, `UnarySide` and `restated_unit_or`:

```cpp
    /// The unit a single value's binary step is shown in. For a product with
    /// exactly one pure number, or a quotient by one, it is the other
    /// operand's unit: 3/50 of a mean in grams is grams. For a sum or a
    /// difference of two values shown on one scale under one name, it is
    /// that unit, at the finer of their two declared precisions. Either way
    /// only when each side recorded the one step claimed for it, and the
    /// unit is `borrowable` and of the step's own dimension -- read off the
    /// operand steps, never off a type, so that what they show is what
    /// carries over. @p fallback otherwise: the coherent unit, which the
    /// renderer names.
    template <typename N, typename Rep>
    [[nodiscard]] constexpr Unit binary_unit_or(std::vector<Step<Rep>> const& steps,
                                                std::vector<std::size_t> const& operands,
                                                Unit fallback) noexcept
    {
        if constexpr (!RecordsOwnStep<typename BinarySides<N>::left> || !RecordsOwnStep<typename BinarySides<N>::right>)
            return fallback;
        else
        {
            if (operands.size() != 2)
                return fallback;
            Unit const& leftUnit = steps[operands[0]].unit;
            Unit const& rightUnit = steps[operands[1]].unit;
            if constexpr (scaled_operand<N>.has_value())
            {
                Unit const& scaledUnit = *scaled_operand<N> == 0 ? leftUnit : rightUnit;
                return scaledUnit.dimension == N::dimension && borrowable(scaledUnit) ? scaledUnit : fallback;
            }
            else if constexpr (StepKindOf<N>::value == StepKind::Add || StepKindOf<N>::value == StepKind::Subtract)
            {
                if (!(leftUnit.dimension == N::dimension) || !borrowable(leftUnit)
                    || !same_scale_and_symbol(leftUnit, rightUnit))
                    return fallback;
                Unit shared = leftUnit;
                shared.decimals = leftUnit.decimals < rightUnit.decimals ? rightUnit.decimals : leftUnit.decimals;
                return shared;
            }
            else
                return fallback;
        }
    }

    /// The one operand type of a negation or an absolute value. Undefined
    /// for every other kind.
    template <typename N>
    struct UnarySide;

    template <UnaryOperator Op, Node Operand>
    struct UnarySide<UnaryNode<Op, Operand>>
    {
        using inner = Operand;
    };

    template <Node Operand>
    struct UnarySide<AbsoluteValueNode<Operand>>
    {
        using inner = Operand;
    };

    /// The unit of a step whose value restates its last claimed step's -- a
    /// conditional's chosen branch, a precision limit's second pass: that
    /// step's unit, when it has a symbol, is of @p dimension, and holds
    /// exactly @p restatedValue. The value is a point on that step's scale,
    /// so an offset unit may be shown (`borrowable_for_a_point`). Comparing
    /// the values means the unit can never be claimed for a number it is not.
    /// @p fallback otherwise.
    template <typename Rep>
    [[nodiscard]] constexpr Unit restated_unit_or(std::vector<Step<Rep>> const& steps,
                                                  std::vector<std::size_t> const& operands,
                                                  Dimension dimension,
                                                  std::optional<Rep> const& restatedValue,
                                                  Unit fallback) noexcept
    {
        if (operands.empty() || !restatedValue.has_value())
            return fallback;
        Step<Rep> const& lastClaimed = steps[operands.back()];
        if (!(lastClaimed.dimension == dimension) || !lastClaimed.value.has_value()
            || !(*lastClaimed.value == *restatedValue) || !borrowable_for_a_point(lastClaimed.unit))
            return fallback;
        return lastClaimed.unit;
    }
```

  `UnaryNode`, `AbsoluteValueNode` and `UnaryOperator` must be visible at that point. `StepKindOf<UnaryNode<…>>` at `:1765` and `StepKindOf<AbsoluteValueNode<…>>` at `:1838` already use them, so they are.

- [ ] **Step 5: Apply them in `produced`.** In `RecordingSink::produced`, replace the block at `:3265-3269` ("Which side a binary step's operand stood on") with:

```cpp
        // Which side a binary step's operand stood on, when it has one, and
        // the unit it is shown in: its scaled operand's or its operands'
        // shared one, when `binary_unit_or` finds one.
        if constexpr (detail::StepKindOf<N>::value == StepKind::Add || detail::StepKindOf<N>::value == StepKind::Subtract
                      || detail::StepKindOf<N>::value == StepKind::Multiply
                      || detail::StepKindOf<N>::value == StepKind::Divide)
        {
            detail::record_operand_sides<N>(nodeStep, _trace->steps);
            nodeStep.unit = detail::binary_unit_or<N>(_trace->steps, nodeStep.operands, nodeStep.unit);
        }

        // A negation and an absolute value are on their operand's scale:
        // -(3 g) is -3 g. Not on an offset one's: -(20 degC) is no reading at
        // -20 degC (`borrowable`).
        if constexpr (requires { typename detail::UnarySide<N>::inner; })
            if constexpr (detail::RecordsOwnStep<typename detail::UnarySide<N>::inner>)
                nodeStep.unit =
                    detail::operand_unit_or(_trace->steps, nodeStep.operands, nodeStep.dimension, nodeStep.unit);

        // A conditional's value is its chosen branch's, and a precision
        // limit's is its second pass's: each reads in that step's unit.
        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional
                      || detail::StepKindOf<N>::value == StepKind::PrecisionLimit)
            nodeStep.unit = detail::restated_unit_or(_trace->steps, nodeStep.operands, nodeStep.dimension, nodeStep.value,
                                                     nodeStep.unit);
```

  A precision limit's line names its last claimed operand as the step it restates (`operand_reference(recorded.operands.back())`, `trace_render.hpp:1264`), so the value comparison finds that step.

- [ ] **Step 6: Run the new tests.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "shown"`. Expected: 9 of 9 pass.

- [ ] **Step 7: Re-pin.** As in Task 1, Steps 8–10, with these kinds of change:
    1. A binary step now shows its scaled operand's unit:
        - `#2 * #3 = 1239/500000 kg` → `#2 * #3 = 1239/500 g`;
        - `#1 * #2 = 432000000 m^2 kg/s^2` → `#1 * #2 = 120 kWh`.
    2. A sum or difference in one unit now shows it: `#1 - #2 = 108000000 m^2 kg/s^2` → `#1 - #2 = 30 kWh`.
    3. A negation, an `abs` or a conditional now shows its operand's or branch's unit: `if #1 > #2 then #3 = 60000000 kg/(m s^2)` → `… = 60 MPa`.
    4. A precision limit now shows its restated step's unit (`docs/gallery.md:597`, `docs/statistics.md:328`).
    5. A step that copies its operand's unit (documented, variant, record scope, opaque output) follows that operand.

  A value now in a declared unit is styled as that unit's values are: padded to its decimals under a padded style, rounded to them under an approximating one. So `≈0.004` (`docs/display.md:199`) becomes a gram value at the gram's declared decimals. **Any other difference is a defect: stop and report it.** That includes a borrowed unit with an offset on a sum, a difference, a scaling or a negation, and a number that changed beyond the unit conversion. Then regenerate the gallery and the checked guides' text blocks, as in Task 1, Step 10.

- [ ] **Step 8: Verify.** Verify step 1 must print `ALL OK`. Delta: **+6 tests**.

- [ ] **Step 9: Commit.**
  Message: `feat(trace): show a computed value in the unit its operands are shown in, where that is safe`.
  Body: one sentence each for scaling, sums and differences, negation and `abs`, and restated values; that an offset unit is never borrowed for a sum, difference, scaling or negation. Then the `Signed-off-by` line.

### Task 3: Every printed value re-derived, and the documentation of the rule

This task covers acceptance criterion 4 of issue #2: no step prints a number in a unit other than the one written after it. It is proved over whole traces. Then every guide, doc comment and changelog line that states the old rule is brought up to date.

**Files:**
- Modify: `test/trace_shown_unit_tests.cpp`: the walker test.
- Modify, the library's doc comments:
    - `include/formula-cpp/trace.hpp`: the `Step::unit` comment (`:970-997`) and the recording comment in `produced` (`:3093-3117`);
    - `include/formula-cpp/trace_render.hpp`: `:25-39`, `:104-123`, `:3342-3350`, `:3398-3404`.
- Modify, the guides' prose and their plain ```` ``` ```` blocks:
    - `docs/tracing.md:286-345`, `:439`, `:559`;
    - `docs/display.md:161-218`, `:359-373`, with `examples/display.cpp` (see Step 5);
    - `docs/dimensions.md:447-455`, `docs/statistics.md:110-112`, `docs/series.md:106-113`, `docs/calculations.md:597-599`;
    - `docs/expressions.md:601-612`, `docs/lookup-tables.md:717-752`, `docs/rounding-and-conditionals.md:228-264`;
    - `docs/methods-and-overlays.md:86-90`, `docs/constraints.md`, `docs/records.md`.
- Modify: `tools/gallery/main.cpp:954`, the prose it writes into `docs/gallery.md:455`. Then regenerate `docs/gallery.md`.
- Modify: the test comments that state the old rule, e.g. `test/trace_render_tests.cpp:101-104`, and any test name that says a computed value has no unit.
- Modify: `CHANGELOG.md`.

**Interfaces:**
- Consumes, all from Tasks 1–2: `formula::detail::value_in_declared_unit`, `formula::detail::coherent_unit_text`, `formula::detail::unit_symbol_text`, `formula::checked_convert`, `formula::coherent`.
- Produces: nothing new.

- [ ] **Step 1: Write the walker test.** Append to `test/trace_shown_unit_tests.cpp`. Add `#include <optional>`, `#include <string_view>`, `#include <vector>` and `#include <cstddef>` to the includes. In the anonymous namespace:

```cpp
/// The decimal digits at the start of @p spelled, as an exact number, and
/// @p spelled advanced past them; nothing when it does not start with one.
std::optional<Rational> take_whole(std::string_view& spelled)
{
    if (spelled.empty() || spelled.front() < '0' || spelled.front() > '9')
        return std::nullopt;
    Rational parsed {};
    while (!spelled.empty() && spelled.front() >= '0' && spelled.front() <= '9')
    {
        std::expected<Rational, formula::ArithmeticError> const shifted = formula::checked_mul(parsed, Rational { 10 });
        if (!shifted)
            return std::nullopt;
        std::expected<Rational, formula::ArithmeticError> const added =
            formula::checked_add(*shifted, Rational { spelled.front() - '0' });
        if (!added)
            return std::nullopt;
        parsed = *added;
        spelled.remove_prefix(1);
    }
    return parsed;
}

/// A value as a trace writes it in the fraction style: `-a/b unit`, `a`,
/// `a/b`, each with or without a unit after a space.
struct ShownValue
{
    Rational shownNumber;
    std::string_view unitText;
};

std::optional<ShownValue> parse_shown(std::string_view spelled)
{
    bool const negative = spelled.starts_with('-');
    if (negative)
        spelled.remove_prefix(1);
    std::optional<Rational> const wholeNumber = take_whole(spelled);
    if (!wholeNumber)
        return std::nullopt;
    Rational parsed = *wholeNumber;
    if (spelled.starts_with('/'))
    {
        spelled.remove_prefix(1);
        std::optional<Rational> const below = take_whole(spelled);
        if (!below)
            return std::nullopt;
        std::expected<Rational, formula::ArithmeticError> const divided = formula::checked_div(parsed, *below);
        if (!divided)
            return std::nullopt;
        parsed = *divided;
    }
    if (negative)
    {
        std::expected<Rational, formula::ArithmeticError> const negated = formula::checked_negate(parsed);
        if (!negated)
            return std::nullopt;
        parsed = *negated;
    }
    if (spelled.starts_with(' '))
        spelled.remove_prefix(1);
    else if (!spelled.empty())
        return std::nullopt;
    return ShownValue { parsed, spelled };
}

/// For every step of @p recorded that holds a value: the text after the
/// number names the step's own unit, the coherent unit, or -- only for a
/// dimensionless step -- nothing; and the number, read back from that unit
/// into the coherent one, is exactly the value recorded. The unit is taken
/// from the text, not from the rule that chose it, so a value written in one
/// scale and labelled with another fails here.
void check_each_value_is_in_the_unit_written_after_it(formula::Trace<> const& recorded)
{
    std::size_t checkedSteps = 0;
    for (formula::Step<Rational> const& recordedStep: recorded.steps)
    {
        if (!recordedStep.value.has_value() || recordedStep.error.has_value())
            continue;
        std::string const shownText =
            formula::detail::value_in_declared_unit(recordedStep, recordedStep.value, formula::NumberStyle::fraction());
        INFO("step shown as: " << shownText);
        std::optional<ShownValue> const parsed = parse_shown(shownText);
        REQUIRE(parsed.has_value());
        formula::Unit const coherentUnit = formula::coherent(recordedStep.dimension);
        std::optional<formula::Unit> namedUnit;
        if (!parsed->unitText.empty() && parsed->unitText == formula::detail::unit_symbol_text(recordedStep.unit))
            namedUnit = recordedStep.unit;
        else if (!parsed->unitText.empty() && parsed->unitText == formula::detail::coherent_unit_text(recordedStep.dimension))
            namedUnit = coherentUnit;
        else if (parsed->unitText.empty() && recordedStep.dimension == formula::dim::Scalar)
            namedUnit = recordedStep.unit;
        REQUIRE(namedUnit.has_value());
        std::expected<Rational, formula::ArithmeticError> const backInCoherent =
            formula::checked_convert(parsed->shownNumber, *namedUnit, coherentUnit);
        REQUIRE(backInCoherent.has_value());
        CHECK(*backInCoherent == *recordedStep.value);
        ++checkedSteps;
    }
    CHECK(checkedSteps > 0);
}
```

  Then the test case:

```cpp
TEST_CASE("every value a trace shows is in the unit written after it", "[trace-render][shown-unit]")
{
    auto const inputs = formula::environment(formula::Measured<SampleMass> { Rational { 413, 10 } },
                                             formula::Measured<TareMass> { Rational { 7 } },
                                             formula::Measured<HeavyMass> { Rational { 1 } },
                                             formula::Measured<Edge> { Rational { 5 } },
                                             formula::Measured<Breadth> { Rational { 8 } },
                                             formula::Measured<StartTemperature> { Rational { 20 } },
                                             formula::Measured<EndTemperature> { Rational { 25 } },
                                             formula::Measured<Strength> { Rational { 60 } },
                                             formula::Measured<UnnamedMass> { Rational { 3 } },
                                             formula::Measured<FineMass> { Rational { 12345, 10000 } });
    // A power, a quotient in the coherent unit, scaling, sums in one unit and
    // in two, an offset difference, negations of both kinds, an absolute
    // value, conditionals over both kinds of branch, and a unit with no symbol.
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::pow<2>(var<Edge>) / var<Breadth> + var<Edge>, inputs));
    check_each_value_is_in_the_unit_written_after_it(
        recorded_trace(formula::abs(var<SampleMass> - var<HeavyMass>) * Rational { 3, 50 } + var<FineMass>, inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::when(var<EndTemperature> > var<StartTemperature>, var<EndTemperature> - var<StartTemperature>,
                      -var<StartTemperature>),
        inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(
        formula::when(var<Strength> > formula::constant<unit::Megapascal>(Rational { 473, 10 }), var<Strength> / Rational { 2 },
                      -var<Strength>),
        inputs));
    check_each_value_is_in_the_unit_written_after_it(recorded_trace(var<UnnamedMass> * Rational { 2 } - var<TareMass>, inputs));
}
```

  The second conditional's branches are dimensionally the same (MPa); the first's both read in kelvin or Celsius. If `formula::pow` is spelled differently, use the form in `test/trace_render_tests.cpp:89` (`formula::pow<2>(var<Mass>)`).

- [ ] **Step 2: Run it.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "shown"`. Expected: 10 of 10 pass.

  Then prove it can fail. Temporarily change Task 1's `spellsCoherent ? coherent(recorded.dimension) : recorded.unit` to `recorded.unit` in `value_in_declared_unit`. The new test must fail on the `UnnamedMass` formula: `3` is written, `kg` is named, and `3 kg` is not the recorded value. Restore the line with a plain write and re-run it to green.

- [ ] **Step 3: The doc comments.**
    - **`Step::unit`** (`trace.hpp:970-997`). State the rule:
        - a variable, constant or rounding shows its declared unit;
        - a value scaled by a pure number shows its operand's unit, and so does a sum or difference on one scale under one name;
        - a negation and an absolute value show their operand's unit;
        - a conditional and a precision limit show the unit of the step they restate;
        - an offset unit is never borrowed for a sum, difference, scaling or negation;
        - everything else is the coherent unit, which the renderer writes after the number, spelt from its bases.

      `docs/tracing.md:308-325` quotes this comment; quote the new one there word for word.
    - **The comment in `produced`** (`:3093-3117`) says "Anything computed has no declared unit, so the coherent one is the truthful answer". Add: "-- until the rules below borrow one from the operand steps".
    - **`trace_render.hpp:25-39` and `:104-123`** (`TraceRenderOptions::numbers`). They say a value in no declared unit is "never padded" and prints bare. Keep the styling sentence; it is still true of the coherent unit. Replace "bare" with "followed by the coherent unit's spelling, `kg/m^3`".
    - **`trace_render.hpp:3342-3350` and `:3398-3404`.** Bring the derivation example up to date with a real line from the gallery.

- [ ] **Step 4: The guides.** Change each place listed under *Files* so that every sentence and every plain ```` ``` ```` block matches what the library now prints. A plain block that mirrors an example program's output is copied from the program. A hand-written block is rewritten by the rules in Step 3, and each number keeps its value in the unit now written. Where a guide states the old rule, e.g. `docs/tracing.md` "A computed value is shown in the coherent unit, with no symbol", it states the new one. `docs/statistics.md`'s outlier-rejection walkthrough now reads `#2 * #3 = 1239/500 g`; its prose says the limit is 3/50 of the mean, in grams.

- [ ] **Step 5: The display guide's section "A value in a unit nobody declared"** (`docs/display.md:161-218`, `:359-373`). Its dish and tare examples now borrow grams, so they no longer show such a value.
    - Change `examples/display.cpp`'s demonstration (the code that builds `tareTraceText`, around `:190-210`) so that one step is still in the coherent unit, e.g. an area from two lengths in mm (`m^2`).
    - Keep the check at `:208` asserting that such a value is not padded, now on the area's line.
    - Rewrite the section around that output: a value in the coherent unit carries its base-unit spelling, is never padded, and approximates to its first significant digit.
    - `docs.display-output` and `docs.display-snippets` hold the guide to the example. Every ```` ```cpp ```` block quoting the changed code must be consecutive source lines of it.

- [ ] **Step 6: The gallery generator's prose.** `tools/gallery/main.cpp:954` writes the sentence that becomes `docs/gallery.md:455`. Change it to the new rule, then regenerate: `out\build\cl-debug\tools\gallery\formula-cpp-gallery.exe docs\gallery.md`.

- [ ] **Step 7: CHANGELOG.** Under `## [Unreleased]` → `### Changed`, add:

```markdown
- **Every computed value in a trace shows a unit.** A value scaled by a pure number, and a sum or difference of
  values shown in one unit, read in that unit: the outlier-rejection limit `#2 * #3 = 1239/500000` is now
  `#2 * #3 = 1239/500 g`. A negation and an absolute value read in their operand's unit, and a conditional and a
  precision limit in the unit of the step they restate. An offset unit is never borrowed for a sum, difference,
  scaling or negation: the difference of two Celsius readings reads in `K`. Any other dimensioned value is shown in
  the coherent unit, followed by its spelling from the base units (`427/125000000 kg^2`, `60000000 kg/(m s^2)`), and
  so is a value in a unit that has no symbol. A dimensionless value is still a bare number. A trace text pinned in
  a test changes wherever it showed a dimensioned value bare.
```

- [ ] **Step 8: Verify.** Verify step 1 must print `ALL OK`. Delta: **+1 test**. Then grep the guides for bare dimensioned values that remain: `Select-String -Path docs\*.md -Pattern '#\d+ [-+*/] #\d+ = [-0-9/≈.]+$'`. Every hit must be a dimensionless step; name each in the report.

- [ ] **Step 9: Commit.**
  Message: `docs(trace): state the shown-unit rule, and prove every value is in the unit written after it`.
  Then the `Signed-off-by` line.

---

## Lane B — a 128-bit `Rational` (issue #1)

Lane B works in its own worktree. Before Task 4 the controller creates it from the plan commit:

```powershell
git -C D:\formula-cpp\.claude\worktrees\int128-and-trace-units worktree add D:\formula-cpp\.claude\worktrees\int128-core -b feature/int128-core <plan commit>
```

### Task 4: `formula::Int128`

A signed 128-bit integer with one API on every compiler, its `std::formatter`, and its tests. Nothing uses it yet.

**Files:**
- Create: `include/formula-cpp/int128.hpp`.
- Modify: `include/formula-cpp/format.hpp`: `#include <formula-cpp/int128.hpp>`, and `std::formatter<formula::Int128, char>` beside `formatter<formula::Rational, char>` (`:576-603`).
- Modify: `CMakeLists.txt`: `include/formula-cpp/int128.hpp` in the install `FILE_SET` (`:41-106`), in alphabetical order.
- Modify: `test/consumer_globals_tests.cpp`: include the header and use it. Check the result in `test/consumer_globals_run_tests.cpp`, following that file's pattern.
- Create: `test/int128_tests.cpp`, registered in `test/CMakeLists.txt`'s `add_executable(formula-cpp-tests` list after `checked_int_tests.cpp`.
- Create: `test/negative/int128_format_spec_not_understood.cpp`, registered in `test/CMakeLists.txt` beside `format_spec_not_understood`, with the same expected text.

**Interfaces:**
- Produces, in `formula`:
    - `class Int128` with:
        - `Int128()`;
        - the implicit `template <std::integral T> Int128(T)`, for every type but `bool` and nothing wider than 64 bits;
        - `static from_words(std::uint64_t high, std::uint64_t low)`, `high_word()`, `low_word()`;
        - `is_negative()`, `fits_int64()`, `to_int64() -> std::optional<std::int64_t>`, `to_uint64() -> std::optional<std::uint64_t>`, `to_double()`;
        - `==` and `<=>`;
        - hidden friends `+ - * / %`, unary `- +`, `<< >>` (by `int`);
        - compound assignments, and `++`/`--` in both forms.
    - `std::numeric_limits<formula::Int128>` and `std::formatter<formula::Int128, char>`.
- Produces, in `formula::detail`:
    - `struct UInt128 { std::uint64_t highWord; std::uint64_t lowWord; }`, with `from_u64`, `fits_u64`, `is_zero`, `bit_width`, `==` and `<=>`;
    - `struct UInt128Division { UInt128 quotient; UInt128 remainder; }`;
    - `namespace portable { add, subtract, multiply_words, multiply, multiply_checked, shift_left, shift_right, divide }`;
    - `u128_add`, `u128_sub`, `u128_mul_words`, `u128_mul`, `u128_mul_checked`, `u128_divmod`, `u128_countr_zero`, `u128_gcd`, `u128_isqrt`, `u128_pow10`;
    - `struct DecimalSpelling { char characters[39]; int length; }` and `u128_decimal(UInt128)`;
    - `magnitude(Int128) -> UInt128` and `signed_from_magnitude(UInt128, bool) -> Int128`;
    - with native support, `NativeUInt128`, `to_native` and `from_native`;
    - the macro `FORMULA_NATIVE_INT128`, 0 or 1.

- [ ] **Step 1: Write the failing tests.** Create `test/int128_tests.cpp`. Every expected value below was computed with Python's arbitrary-precision integers and reduced to 128-bit two's complement.

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// formula::Int128: two's complement arithmetic on 128 bits, the same at
// compile time and at run time, and the same through the compiler's own
// 128-bit integer and through the portable code. Expected values were
// computed with Python's integers.
#include <formula-cpp/format.hpp>
#include <formula-cpp/int128.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
using formula::Int128;
using formula::detail::UInt128;

constexpr Int128 words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
{
    return Int128::from_words(highWord, lowWord);
}

constexpr UInt128 unsigned_words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
{
    return UInt128 { highWord, lowWord };
}

struct ArithmeticCase
{
    Int128 leftOperand;
    Int128 rightOperand;
    Int128 added;
    Int128 subtracted;
    Int128 multiplied;
    Int128 divided;
    Int128 remaining;
};

// Sums, differences and products wrap; quotients round toward zero; a
// remainder takes the dividend's sign.
constexpr std::array<ArithmeticCase, 9> arithmeticCases { {
    { words(0x7fffffffffffffff, 0xffffffffffffffff), words(0x0000000000000000, 0x0000000000000003),
      words(0x8000000000000000, 0x0000000000000002), words(0x7fffffffffffffff, 0xfffffffffffffffc),
      words(0x7fffffffffffffff, 0xfffffffffffffffd), words(0x2aaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa),
      words(0x0000000000000000, 0x0000000000000001) },
    { words(0x8000000000000000, 0x0000000000000000), words(0x0000000000000000, 0x0000000000000007),
      words(0x8000000000000000, 0x0000000000000007), words(0x7fffffffffffffff, 0xfffffffffffffff9),
      words(0x8000000000000000, 0x0000000000000000), words(0xedb6db6db6db6db6, 0xdb6db6db6db6db6e),
      words(0xffffffffffffffff, 0xfffffffffffffffe) },
    { words(0x0000000000000001, 0x0000000000003039), words(0x0000000000000000, 0xffffffffffffffff),
      words(0x0000000000000002, 0x0000000000003038), words(0x0000000000000000, 0x000000000000303a),
      words(0x0000000000003037, 0xffffffffffffcfc7), words(0x0000000000000000, 0x0000000000000001),
      words(0x0000000000000000, 0x000000000000303a) },
    { words(0xfffffffe7116f009, 0x3c8c1f11b1c0f52e), words(0x0000000000000000, 0x0db4da5f49f8b478),
      words(0xfffffffe7116f009, 0x4a40f970fbb9a9a6), words(0xfffffffe7116f009, 0x2ed744b267c840b6),
      words(0x4b38a08ad7aa0090, 0x0e176230a1674590), words(0xffffffffffffffff, 0xffffffe2e56b6274),
      words(0xffffffffffffffff, 0xf326734031d13ece) },
    { words(0x7fffffffffffffff, 0xffffffffffffffff), words(0x8000000000000000, 0x0000000000000001),
      words(0x0000000000000000, 0x0000000000000000), words(0xffffffffffffffff, 0xfffffffffffffffe),
      words(0xffffffffffffffff, 0xffffffffffffffff), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0x0000000000000000, 0x0000000000000000) },
    { words(0x0000001000000000, 0x0000000000000001), words(0xffffffffffffffff, 0xffffffeffffffffb),
      words(0x0000000fffffffff, 0xffffffeffffffffc), words(0x0000001000000000, 0x0000001000000006),
      words(0xffffffafffffffff, 0xffffffeffffffffb), words(0xffffffffffffffff, 0x0000000050000000),
      words(0x0000000000000000, 0x0000000190000001) },
    { words(0xffffffffffffffff, 0xfffffffffffffffb), words(0x0000000000000000, 0x0000000000000003),
      words(0xffffffffffffffff, 0xfffffffffffffffe), words(0xffffffffffffffff, 0xfffffffffffffff8),
      words(0xffffffffffffffff, 0xfffffffffffffff1), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0xffffffffffffffff, 0xfffffffffffffffe) },
    { words(0x0000000000000000, 0x0000000000000005), words(0xffffffffffffffff, 0xfffffffffffffffd),
      words(0x0000000000000000, 0x0000000000000002), words(0x0000000000000000, 0x0000000000000008),
      words(0xffffffffffffffff, 0xfffffffffffffff1), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0x0000000000000000, 0x0000000000000002) },
    { words(0xffffffffffffffff, 0x0000000000000000), words(0xffffffffffffffff, 0x8000000000000000),
      words(0xfffffffffffffffe, 0x8000000000000000), words(0xffffffffffffffff, 0x8000000000000000),
      words(0x8000000000000000, 0x0000000000000000), words(0x0000000000000000, 0x0000000000000002),
      words(0x0000000000000000, 0x0000000000000000) },
} };

constexpr bool every_arithmetic_case_holds() noexcept
{
    for (ArithmeticCase const& checked: arithmeticCases)
    {
        if (!(checked.leftOperand + checked.rightOperand == checked.added)
            || !(checked.leftOperand - checked.rightOperand == checked.subtracted)
            || !(checked.leftOperand * checked.rightOperand == checked.multiplied)
            || !(checked.leftOperand / checked.rightOperand == checked.divided)
            || !(checked.leftOperand % checked.rightOperand == checked.remaining))
            return false;
    }
    return true;
}

struct GcdCase
{
    UInt128 leftOperand;
    UInt128 rightOperand;
    UInt128 common;
};

constexpr std::array<GcdCase, 6> gcdCases { {
    // 2^127 - 1 and 2^64 - 1: 2^gcd(127, 64) - 1 = 1.
    { unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff), unsigned_words(0, 0xffffffffffffffff), unsigned_words(0, 1) },
    // 2^90 3^20 and 2^80 3^25 5: 2^80 3^20.
    { unsigned_words(0x033f506e44000000, 0), unsigned_words(0x03da5faed52f0000, 0), unsigned_words(0x0000cfd41b910000, 0) },
    { unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff), unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff),
      unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff) },
    { unsigned_words(0, 0), unsigned_words(0x0000001000000000, 0), unsigned_words(0x0000001000000000, 0) },
    // 6 * 10^30 and 4 * 10^25 + 2.
    { unsigned_words(0x0000004bbb0bace1, 0xa6bd937d80000000), unsigned_words(0x0000000000211654, 0x5850052128000002),
      unsigned_words(0, 6) },
    { unsigned_words(7, 0), unsigned_words(0x4d, 0), unsigned_words(7, 0) },
} };
} // namespace

TEST_CASE("Int128 adds, subtracts, multiplies and divides as a 128-bit two's complement integer", "[int128]")
{
    for (ArithmeticCase const& checked: arithmeticCases)
    {
        CHECK(checked.leftOperand + checked.rightOperand == checked.added);
        CHECK(checked.leftOperand - checked.rightOperand == checked.subtracted);
        CHECK(checked.leftOperand * checked.rightOperand == checked.multiplied);
        CHECK(checked.leftOperand / checked.rightOperand == checked.divided);
        CHECK(checked.leftOperand % checked.rightOperand == checked.remaining);
    }
}

TEST_CASE("Int128 computes the same at compile time", "[int128]")
{
    STATIC_REQUIRE(every_arithmetic_case_holds());
}

TEST_CASE("Int128 converts from every built-in integer exactly, and to none without asking", "[int128]")
{
    STATIC_REQUIRE(Int128 { -5 } == words(~std::uint64_t { 0 }, ~std::uint64_t { 0 } - 4));
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() } == words(0, ~std::uint64_t { 0 }));
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::int64_t>::min() } == words(~std::uint64_t { 0 }, std::uint64_t { 1 } << 63));
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Int128, bool>);
    // No conversion to a built-in integer, implicit or explicit: narrowing is
    // to_int64() or to_uint64(), which say when the value does not fit.
    STATIC_REQUIRE_FALSE(std::is_convertible_v<Int128, std::int64_t>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<std::int64_t, Int128>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<std::uint64_t, Int128>);
    STATIC_REQUIRE(Int128 { -1 }.to_int64() == std::optional<std::int64_t> { -1 });
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() }.to_int64() == std::nullopt);
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() }.to_uint64()
                   == std::optional<std::uint64_t> { std::numeric_limits<std::uint64_t>::max() });
    STATIC_REQUIRE(Int128 { -1 }.to_uint64() == std::nullopt);
    STATIC_REQUIRE(words(1, 0).to_int64() == std::nullopt);
}

TEST_CASE("Int128 orders as a signed integer and shifts arithmetically", "[int128]")
{
    constexpr Int128 smallest = std::numeric_limits<Int128>::min();
    constexpr Int128 largest = std::numeric_limits<Int128>::max();
    STATIC_REQUIRE(smallest == words(std::uint64_t { 1 } << 63, 0));
    STATIC_REQUIRE(largest == words(~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 }));
    STATIC_REQUIRE(smallest < Int128 { -1 });
    STATIC_REQUIRE(Int128 { -1 } < Int128 { 0 });
    STATIC_REQUIRE(words(0, ~std::uint64_t { 0 }) < words(1, 0));
    STATIC_REQUIRE(0 < largest);
    STATIC_REQUIRE((Int128 { -8 } >> 1) == Int128 { -4 });
    STATIC_REQUIRE((Int128 { -1 } >> 127) == Int128 { -1 });
    STATIC_REQUIRE((smallest >> 64) == words(~std::uint64_t { 0 }, std::uint64_t { 1 } << 63));
    STATIC_REQUIRE((Int128 { 1 } << 127) == smallest);
    STATIC_REQUIRE((Int128 { 3 } << 64) == words(3, 0));
    STATIC_REQUIRE(-smallest == smallest);
    STATIC_REQUIRE(std::numeric_limits<Int128>::digits == 127);
    STATIC_REQUIRE(std::numeric_limits<Int128>::is_signed);
}

TEST_CASE("Int128 converts to the nearest double, ties to even", "[int128]")
{
    CHECK(words(0x7fffffffffffffff, 0xffffffffffffffff).to_double() == 0x1p+127);
    CHECK(words(0x8000000000000000, 0).to_double() == -0x1p+127);
    // 2^64 + 2^11 is half way between two doubles: to even, 2^64.
    CHECK(words(1, 0x800).to_double() == 0x1p+64);
    // One more and it is past half way.
    CHECK(words(1, 0x801).to_double() == 0x1.0000000000001p+64);
    // 2^64 + 3 * 2^11 is half way again: to even, upwards this time.
    CHECK(words(1, 0x1800).to_double() == 0x1.0000000000002p+64);
    CHECK(words(0xffffffefffffffff, 0xffff800000000000).to_double() == -0x1p+100);
    CHECK(words(0xffffffefffffffff, 0xffff7fffffffffff).to_double() == -0x1.0000000000001p+100);
    CHECK(Int128 { -7 }.to_double() == -7.0);
}

TEST_CASE("the 128-bit greatest common divisor, square root and powers of ten", "[int128]")
{
    for (GcdCase const& checked: gcdCases)
    {
        CHECK(formula::detail::u128_gcd(checked.leftOperand, checked.rightOperand) == checked.common);
        CHECK(formula::detail::u128_gcd(checked.rightOperand, checked.leftOperand) == checked.common);
    }
    CHECK(formula::detail::u128_isqrt(unsigned_words(~std::uint64_t { 0 }, ~std::uint64_t { 0 })) == ~std::uint64_t { 0 });
    CHECK(formula::detail::u128_isqrt(unsigned_words(std::uint64_t { 1 } << 63, 0)) == 0xb504f333f9de6484);
    CHECK(formula::detail::u128_isqrt(unsigned_words(1, 0)) == std::uint64_t { 1 } << 32);
    // (2^64 - 1)^2 and one below it.
    CHECK(formula::detail::u128_isqrt(unsigned_words(0xfffffffffffffffe, 1)) == ~std::uint64_t { 0 });
    CHECK(formula::detail::u128_isqrt(unsigned_words(0xfffffffffffffffe, 0)) == 0xfffffffffffffffe);
    CHECK(formula::detail::u128_pow10(38) == unsigned_words(0x4b3b4ca85a86c47a, 0x098a224000000000));
    CHECK(formula::detail::u128_pow10(39) == std::nullopt);
    CHECK(formula::detail::u128_pow10(-1) == std::nullopt);
}

TEST_CASE("Int128 formats as its decimal digits", "[int128][format]")
{
    CHECK(std::format("{}", Int128 { 0 }) == "0");
    CHECK(std::format("{}", Int128 { -1 }) == "-1");
    CHECK(std::format("{}", std::numeric_limits<Int128>::max()) == "170141183460469231731687303715884105727");
    CHECK(std::format("{}", std::numeric_limits<Int128>::min()) == "-170141183460469231731687303715884105728");
    CHECK(std::format("{}", words(1, 0)) == "18446744073709551616");
    CHECK(std::format("{}", words(0x4b3b4ca85a86c47a, 0x098a224000000000)) == "100000000000000000000000000000000000000");
}

TEST_CASE("the 128-bit division, product and greatest common divisor keep their defining identities", "[int128]")
{
    // splitmix64, seeded: a mix of 64-bit, 128-bit and boundary operands.
    std::uint64_t state = 20261003;
    auto const nextWord = [&state] {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31);
    };
    for (int drawn = 0; drawn < 4000; ++drawn)
    {
        std::uint64_t const shape = nextWord() % 4;
        UInt128 const dividend { shape == 0 ? std::uint64_t { 0 } : nextWord(), nextWord() };
        UInt128 const divisor { shape < 2 ? std::uint64_t { 0 } : nextWord() >> (nextWord() % 64), nextWord() | 1U };
        formula::detail::UInt128Division const split = formula::detail::u128_divmod(dividend, divisor);
        CHECK(split.remainder < divisor);
        std::optional<UInt128> const recombined = formula::detail::u128_mul_checked(split.quotient, divisor);
        REQUIRE(recombined.has_value());
        CHECK(formula::detail::u128_add(*recombined, split.remainder) == dividend);
        UInt128 const common = formula::detail::u128_gcd(dividend, divisor);
        CHECK(formula::detail::u128_divmod(dividend, common).remainder.is_zero());
        CHECK(formula::detail::u128_divmod(divisor, common).remainder.is_zero());
        CHECK(formula::detail::u128_gcd(formula::detail::u128_divmod(dividend, common).quotient,
                                        formula::detail::u128_divmod(divisor, common).quotient)
              == UInt128::from_u64(1));
    }
}

#if FORMULA_NATIVE_INT128
TEST_CASE("the portable 128-bit arithmetic agrees with the compiler's own", "[int128]")
{
    using namespace formula::detail;
    std::uint64_t state = 1272026;
    auto const nextWord = [&state] {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31);
    };
    for (int drawn = 0; drawn < 4000; ++drawn)
    {
        UInt128 const leftOperand { nextWord() % 3 == 0 ? std::uint64_t { 0 } : nextWord(), nextWord() };
        UInt128 const rightOperand { nextWord() % 3 == 0 ? std::uint64_t { 0 } : nextWord() >> (nextWord() % 64),
                                     nextWord() | 1U };
        NativeUInt128 const nativeLeft = to_native(leftOperand);
        NativeUInt128 const nativeRight = to_native(rightOperand);
        CHECK(portable::multiply(leftOperand, rightOperand) == from_native(nativeLeft * nativeRight));
        CHECK(portable::divide(leftOperand, rightOperand).quotient == from_native(nativeLeft / nativeRight));
        CHECK(portable::divide(leftOperand, rightOperand).remainder == from_native(nativeLeft % nativeRight));
        NativeUInt128 nativeProduct = 0;
        bool const nativeOverflowed = __builtin_mul_overflow(nativeLeft, nativeRight, &nativeProduct);
        std::optional<UInt128> const portableProduct = portable::multiply_checked(leftOperand, rightOperand);
        CHECK(portableProduct.has_value() == !nativeOverflowed);
        if (portableProduct.has_value())
            CHECK(*portableProduct == from_native(nativeProduct));
        CHECK(portable::multiply_words(leftOperand.lowWord, rightOperand.lowWord)
              == from_native(static_cast<NativeUInt128>(leftOperand.lowWord) * rightOperand.lowWord));
    }
}
#endif
```

  Create `test/negative/int128_format_spec_not_understood.cpp`, matching `test/negative/format_spec_not_understood.cpp`'s shape:

```cpp
// SPDX-License-Identifier: Apache-2.0
// An Int128 is written as its decimal digits; any spec but the empty one is
// refused where the format string is written.
#include <formula-cpp/format.hpp>

#include <format>
#include <string>

std::string probe()
{
    return std::format("{:x}", formula::Int128 { 255 });
}
```

  Register it with `formula_add_negative_test(int128_format_spec_not_understood "formula_number_format_spec_not_understood" …)`, copying the arguments of `format_spec_not_understood`'s registration.

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "int128"`.
  Expected: a build failure, because `formula-cpp/int128.hpp` does not exist.

- [ ] **Step 3: Write `include/formula-cpp/int128.hpp`.**

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// `formula::Int128`, the signed 128-bit integer `Rational` stores its
/// numerator and denominator in.
///
/// One class, with one API on every compiler. It is held as two 64-bit words
/// in two's complement everywhere. Where the compiler has a 128-bit integer
/// -- GCC, Clang and AppleClang -- multiplication, division and the overflow
/// check are carried out in it; elsewhere, in portable `constexpr` code on the
/// two words. That is cl, and clang-cl too: it accepts `__int128`, but
/// dividing one calls compiler-rt's `__divti3`, which the MSVC linker does
/// not supply. Both routes give the same bits for every operation, at compile
/// time and at run time, so a number is the same on every compiler.
///
/// Division, the remainder and the greatest common divisor take a 64-bit
/// route whenever their operands fit 64 bits, which nearly every number a
/// formula forms does.
///
/// **No conversion to a built-in integer type**, implicit or explicit.
/// Narrowing is spelled `to_int64()` and `to_uint64()`, which answer
/// `std::nullopt` when the value does not fit, so that no code can cut a
/// 128-bit value down to 64 bits without saying what happens when it does
/// not fit.
///
/// The operators behave as a built-in signed integer's do, except that none
/// is undefined: a sum, difference or product that does not fit wraps
/// around, and a division by zero is a precondition violation. The library's
/// own arithmetic never relies on wrapping; it uses the checked forms in
/// `detail/checked_int.hpp`.

#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>

#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
    /// 1 where the compiler's own 128-bit integer carries `Int128`'s
    /// multiplication and division, 0 where the portable code does. Internal:
    /// not part of the library's contract.
    #define FORMULA_NATIVE_INT128 1
#else
    #define FORMULA_NATIVE_INT128 0
#endif

namespace formula::detail
{

/// An unsigned 128-bit integer, as two 64-bit words, the more significant
/// first so that the defaulted comparisons order it numerically. It holds
/// magnitudes: 2^127, the magnitude of `Int128`'s minimum, has no signed
/// counterpart.
struct UInt128
{
    /// Bits 64 to 127.
    std::uint64_t highWord = 0;
    /// Bits 0 to 63.
    std::uint64_t lowWord = 0;

    /// @p whole, widened.
    [[nodiscard]] static constexpr UInt128 from_u64(std::uint64_t whole) noexcept { return UInt128 { 0, whole }; }

    /// Whether it fits 64 bits.
    [[nodiscard]] constexpr bool fits_u64() const noexcept { return highWord == 0; }

    /// Whether it is zero.
    [[nodiscard]] constexpr bool is_zero() const noexcept { return highWord == 0 && lowWord == 0; }

    /// How many bits writing it takes: 0 for zero, 128 at most.
    [[nodiscard]] constexpr int bit_width() const noexcept
    {
        return highWord != 0 ? 64 + static_cast<int>(std::bit_width(highWord)) : static_cast<int>(std::bit_width(lowWord));
    }

    /// Numeric equality.
    [[nodiscard]] constexpr bool operator==(UInt128 const&) const noexcept = default;
    /// Numeric order: the more significant word first.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(UInt128 const&) const noexcept = default;
};

/// A quotient and a remainder.
struct UInt128Division
{
    /// The quotient, rounded toward zero.
    UInt128 quotient {};
    /// What is left over: below the divisor.
    UInt128 remainder {};
};

/// The portable arithmetic on two words. Every compiler builds it and the
/// tests check it; `Int128` uses it where the compiler has no 128-bit
/// integer of its own.
namespace portable
{
    /// The sum, wrapping past 2^128.
    [[nodiscard]] constexpr UInt128 add(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        std::uint64_t const lowSum = leftOperand.lowWord + rightOperand.lowWord;
        std::uint64_t const carried = lowSum < leftOperand.lowWord ? 1U : 0U;
        return UInt128 { leftOperand.highWord + rightOperand.highWord + carried, lowSum };
    }

    /// The difference, wrapping below zero.
    [[nodiscard]] constexpr UInt128 subtract(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        std::uint64_t const borrowed = leftOperand.lowWord < rightOperand.lowWord ? 1U : 0U;
        return UInt128 { leftOperand.highWord - rightOperand.highWord - borrowed, leftOperand.lowWord - rightOperand.lowWord };
    }

    /// The full product of two words, from their 32-bit halves.
    [[nodiscard]] constexpr UInt128 multiply_words(std::uint64_t leftWord, std::uint64_t rightWord) noexcept
    {
        constexpr std::uint64_t HalfMask = 0xFFFF'FFFFU;
        std::uint64_t const lowLow = (leftWord & HalfMask) * (rightWord & HalfMask);
        std::uint64_t const highLow = (leftWord >> 32) * (rightWord & HalfMask);
        std::uint64_t const lowHigh = (leftWord & HalfMask) * (rightWord >> 32);
        std::uint64_t const highHigh = (leftWord >> 32) * (rightWord >> 32);
        std::uint64_t const middle = (lowLow >> 32) + (highLow & HalfMask) + (lowHigh & HalfMask);
        return UInt128 { highHigh + (highLow >> 32) + (lowHigh >> 32) + (middle >> 32),
                         (middle << 32) | (lowLow & HalfMask) };
    }

    /// The product's low 128 bits.
    [[nodiscard]] constexpr UInt128 multiply(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        UInt128 product = multiply_words(leftOperand.lowWord, rightOperand.lowWord);
        // Each cross term lands in the high word; what passes 2^128 wraps away.
        product.highWord += leftOperand.highWord * rightOperand.lowWord + leftOperand.lowWord * rightOperand.highWord;
        return product;
    }

    /// The product, or nothing when it needs more than 128 bits.
    [[nodiscard]] constexpr std::optional<UInt128> multiply_checked(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        if (leftOperand.highWord != 0 && rightOperand.highWord != 0)
            return std::nullopt;
        UInt128 product = multiply_words(leftOperand.lowWord, rightOperand.lowWord);
        // At most one of the two cross terms is not zero.
        UInt128 const crossTerm = leftOperand.highWord != 0 ? multiply_words(leftOperand.highWord, rightOperand.lowWord)
                                                            : multiply_words(leftOperand.lowWord, rightOperand.highWord);
        if (crossTerm.highWord != 0)
            return std::nullopt;
        std::uint64_t const raisedHigh = product.highWord + crossTerm.lowWord;
        if (raisedHigh < product.highWord)
            return std::nullopt;
        product.highWord = raisedHigh;
        return product;
    }

    /// Shifted left by @p places, below 128; the bits shifted out are lost.
    [[nodiscard]] constexpr UInt128 shift_left(UInt128 operandValue, int places) noexcept
    {
        if (places == 0)
            return operandValue;
        if (places >= 64)
            return UInt128 { operandValue.lowWord << (places - 64), 0 };
        return UInt128 { (operandValue.highWord << places) | (operandValue.lowWord >> (64 - places)),
                         operandValue.lowWord << places };
    }

    /// Shifted right by @p places, below 128, with zeros shifted in.
    [[nodiscard]] constexpr UInt128 shift_right(UInt128 operandValue, int places) noexcept
    {
        if (places == 0)
            return operandValue;
        if (places >= 64)
            return UInt128 { 0, operandValue.highWord >> (places - 64) };
        return UInt128 { operandValue.highWord >> places,
                         (operandValue.lowWord >> places) | (operandValue.highWord << (64 - places)) };
    }

    /// Long division, one bit of the quotient a step. @pre @p divisor is not zero.
    [[nodiscard]] constexpr UInt128Division divide(UInt128 dividend, UInt128 divisor) noexcept
    {
        if (dividend < divisor)
            return UInt128Division { UInt128 {}, dividend };
        int const shiftedBy = dividend.bit_width() - divisor.bit_width();
        UInt128 shiftedDivisor = shift_left(divisor, shiftedBy);
        UInt128 quotientSoFar {};
        UInt128 remaining = dividend;
        for (int place = shiftedBy; place >= 0; --place)
        {
            quotientSoFar = shift_left(quotientSoFar, 1);
            if (!(remaining < shiftedDivisor))
            {
                remaining = subtract(remaining, shiftedDivisor);
                quotientSoFar.lowWord |= 1U;
            }
            shiftedDivisor = shift_right(shiftedDivisor, 1);
        }
        return UInt128Division { quotientSoFar, remaining };
    }
} // namespace portable

#if FORMULA_NATIVE_INT128
/// The compiler's own unsigned 128-bit integer. `__extension__` keeps
/// `-Wpedantic` quiet under `-std=c++23`; nothing outside this header and its
/// tests names it, and it is never handed to the standard library, whose
/// type traits do not count it as an integer in that mode.
__extension__ typedef unsigned __int128 NativeUInt128;

/// @p operandValue as the compiler's own integer.
[[nodiscard]] constexpr NativeUInt128 to_native(UInt128 operandValue) noexcept
{
    return (static_cast<NativeUInt128>(operandValue.highWord) << 64) | operandValue.lowWord;
}

/// The compiler's own integer @p operandValue as two words.
[[nodiscard]] constexpr UInt128 from_native(NativeUInt128 operandValue) noexcept
{
    return UInt128 { static_cast<std::uint64_t>(operandValue >> 64), static_cast<std::uint64_t>(operandValue) };
}
#endif

/// The sum, wrapping past 2^128.
[[nodiscard]] constexpr UInt128 u128_add(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return portable::add(leftOperand, rightOperand);
}

/// The difference, wrapping below zero.
[[nodiscard]] constexpr UInt128 u128_sub(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return portable::subtract(leftOperand, rightOperand);
}

/// The full product of two words.
[[nodiscard]] constexpr UInt128 u128_mul_words(std::uint64_t leftWord, std::uint64_t rightWord) noexcept
{
#if FORMULA_NATIVE_INT128
    return from_native(static_cast<NativeUInt128>(leftWord) * rightWord);
#else
    return portable::multiply_words(leftWord, rightWord);
#endif
}

/// The product's low 128 bits.
[[nodiscard]] constexpr UInt128 u128_mul(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
#if FORMULA_NATIVE_INT128
    return from_native(to_native(leftOperand) * to_native(rightOperand));
#else
    return portable::multiply(leftOperand, rightOperand);
#endif
}

/// The product, or nothing when it needs more than 128 bits.
[[nodiscard]] constexpr std::optional<UInt128> u128_mul_checked(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
#if FORMULA_NATIVE_INT128
    NativeUInt128 product = 0;
    if (__builtin_mul_overflow(to_native(leftOperand), to_native(rightOperand), &product))
        return std::nullopt;
    return from_native(product);
#else
    return portable::multiply_checked(leftOperand, rightOperand);
#endif
}

/// Quotient and remainder. @pre @p divisor is not zero.
[[nodiscard]] constexpr UInt128Division u128_divmod(UInt128 dividend, UInt128 divisor) noexcept
{
    if (dividend.fits_u64() && divisor.fits_u64())
        return UInt128Division { UInt128::from_u64(dividend.lowWord / divisor.lowWord),
                                 UInt128::from_u64(dividend.lowWord % divisor.lowWord) };
#if FORMULA_NATIVE_INT128
    NativeUInt128 const nativeDividend = to_native(dividend);
    NativeUInt128 const nativeDivisor = to_native(divisor);
    return UInt128Division { from_native(nativeDividend / nativeDivisor), from_native(nativeDividend % nativeDivisor) };
#else
    return portable::divide(dividend, divisor);
#endif
}

/// How many zero bits end @p operandValue. @pre it is not zero.
[[nodiscard]] constexpr int u128_countr_zero(UInt128 operandValue) noexcept
{
    return operandValue.lowWord != 0 ? std::countr_zero(operandValue.lowWord) : 64 + std::countr_zero(operandValue.highWord);
}

/// The greatest common divisor; that of 0 and n is n. Binary: no step
/// divides, where each of Euclid's would be a 128-bit division. Both
/// operands of 64 bits, at the start or on the way, finish in 64 bits.
[[nodiscard]] constexpr UInt128 u128_gcd(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    if (leftOperand.fits_u64() && rightOperand.fits_u64())
        return UInt128::from_u64(std::gcd(leftOperand.lowWord, rightOperand.lowWord));
    if (leftOperand.is_zero())
        return rightOperand;
    if (rightOperand.is_zero())
        return leftOperand;
    int const sharedTwos = u128_countr_zero(
        UInt128 { leftOperand.highWord | rightOperand.highWord, leftOperand.lowWord | rightOperand.lowWord });
    leftOperand = portable::shift_right(leftOperand, u128_countr_zero(leftOperand));
    for (;;)
    {
        rightOperand = portable::shift_right(rightOperand, u128_countr_zero(rightOperand));
        if (rightOperand < leftOperand)
            std::swap(leftOperand, rightOperand);
        rightOperand = portable::subtract(rightOperand, leftOperand);
        if (rightOperand.is_zero())
            break;
        if (leftOperand.fits_u64() && rightOperand.fits_u64())
        {
            leftOperand = UInt128::from_u64(std::gcd(leftOperand.lowWord, rightOperand.lowWord));
            break;
        }
    }
    return portable::shift_left(leftOperand, sharedTwos);
}

/// The largest r with r * r <= @p radicand, which is below 2^64 for every
/// radicand: one bit of the root a step, from the top.
[[nodiscard]] constexpr std::uint64_t u128_isqrt(UInt128 radicand) noexcept
{
    std::uint64_t rootSoFar = 0;
    for (int bitAt = 63; bitAt >= 0; --bitAt)
    {
        std::uint64_t const candidate = rootSoFar | (std::uint64_t { 1 } << bitAt);
        if (!(radicand < u128_mul_words(candidate, candidate)))
            rootSoFar = candidate;
    }
    return rootSoFar;
}

/// 10^@p exponent for 0 to 38, and nothing otherwise: 10^38 is the largest
/// power of ten below 2^127.
[[nodiscard]] constexpr std::optional<UInt128> u128_pow10(int exponent) noexcept
{
    if (exponent < 0 || exponent > 38)
        return std::nullopt;
    UInt128 power = UInt128::from_u64(1);
    for (int multiplied = 0; multiplied < exponent; ++multiplied)
        power = u128_mul(power, UInt128::from_u64(10));
    return power;
}

/// The decimal digits of a 128-bit magnitude: at most 39.
struct DecimalSpelling
{
    /// The digits, most significant first; the first `length` are written.
    char characters[39] {};
    /// How many digits there are: 1 for zero.
    int length = 0;
};

/// @p magnitudeShown's decimal digits.
[[nodiscard]] constexpr DecimalSpelling u128_decimal(UInt128 magnitudeShown) noexcept
{
    char reversed[39] {};
    int produced = 0;
    do
    {
        UInt128Division const split = u128_divmod(magnitudeShown, UInt128::from_u64(10));
        reversed[produced] = static_cast<char>('0' + split.remainder.lowWord);
        ++produced;
        magnitudeShown = split.quotient;
    } while (!magnitudeShown.is_zero());
    DecimalSpelling written {};
    written.length = produced;
    for (int at = 0; at < produced; ++at)
        written.characters[at] = reversed[produced - 1 - at];
    return written;
}

} // namespace formula::detail

namespace formula
{

/// A signed 128-bit integer: `Rational`'s numerator and denominator. See
/// this header's file comment for how it computes, and why it converts to no
/// built-in integer type.
class Int128
{
  public:
    /// Zero.
    constexpr Int128() noexcept = default;

    /// @p whole, exactly: every built-in integer of up to 64 bits fits. Not
    /// `bool`, which is no number.
    template <typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
    constexpr Int128(T whole) noexcept:
        _highWord { sign_word(whole) },
        _lowWord { static_cast<std::uint64_t>(whole) }
    {
    }

    /// The value whose two's complement words are @p highWord and @p lowWord.
    [[nodiscard]] static constexpr Int128 from_words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
    {
        Int128 made {};
        made._highWord = highWord;
        made._lowWord = lowWord;
        return made;
    }

    /// Bits 64 to 127 of the two's complement form.
    [[nodiscard]] constexpr std::uint64_t high_word() const noexcept { return _highWord; }
    /// Bits 0 to 63 of the two's complement form.
    [[nodiscard]] constexpr std::uint64_t low_word() const noexcept { return _lowWord; }

    /// Whether it is below zero.
    [[nodiscard]] constexpr bool is_negative() const noexcept { return (_highWord >> 63) != 0U; }

    /// Whether `std::int64_t` holds it.
    [[nodiscard]] constexpr bool fits_int64() const noexcept
    {
        return _highWord == ((_lowWord >> 63) != 0U ? ~std::uint64_t { 0 } : std::uint64_t { 0 });
    }

    /// It as a `std::int64_t`, or nothing when it does not fit.
    [[nodiscard]] constexpr std::optional<std::int64_t> to_int64() const noexcept
    {
        if (!fits_int64())
            return std::nullopt;
        // Well defined since C++20: conversion to a signed type is modular.
        return static_cast<std::int64_t>(_lowWord);
    }

    /// It as a `std::uint64_t`, or nothing when it is negative or does not fit.
    [[nodiscard]] constexpr std::optional<std::uint64_t> to_uint64() const noexcept
    {
        if (_highWord != 0U)
            return std::nullopt;
        return _lowWord;
    }

    /// The nearest `double`, ties to even. Named, as `Rational::to_double` is,
    /// so that every loss of exactness is visible where it happens.
    [[nodiscard]] constexpr double to_double() const noexcept
    {
        if (fits_int64())
            return static_cast<double>(static_cast<std::int64_t>(_lowWord));
        detail::UInt128 const magnitudeOf = magnitude_pattern();
        int const dropped = magnitudeOf.bit_width() - 64;
        detail::UInt128 const kept = detail::portable::shift_right(magnitudeOf, dropped);
        // A sticky bit below the 53 a double keeps: rounding to nearest then
        // ties only when the bits dropped are exactly half a unit.
        bool const inexact = !(detail::portable::shift_left(kept, dropped) == magnitudeOf);
        double widened = static_cast<double>(kept.lowWord | (inexact ? 1U : 0U));
        for (int doubled = 0; doubled < dropped; ++doubled)
            widened *= 2.0;
        return is_negative() ? -widened : widened;
    }

    /// Numeric equality.
    [[nodiscard]] constexpr bool operator==(Int128 const&) const noexcept = default;

    /// Numeric order.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(Int128 const& compared) const noexcept
    {
        if (_highWord != compared._highWord)
            return static_cast<std::int64_t>(_highWord) <=> static_cast<std::int64_t>(compared._highWord);
        return _lowWord <=> compared._lowWord;
    }

    /// The sum, wrapping past the range.
    [[nodiscard]] friend constexpr Int128 operator+(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::portable::add(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The difference, wrapping past the range.
    [[nodiscard]] friend constexpr Int128 operator-(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::portable::subtract(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The product, wrapping past the range.
    [[nodiscard]] friend constexpr Int128 operator*(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::u128_mul(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The quotient, rounded toward zero. @pre @p divisor is not zero.
    [[nodiscard]] friend constexpr Int128 operator/(Int128 dividend, Int128 divisor) noexcept
    {
        Int128 const quotientMagnitude =
            from_pattern(detail::u128_divmod(dividend.magnitude_pattern(), divisor.magnitude_pattern()).quotient);
        return dividend.is_negative() != divisor.is_negative() ? -quotientMagnitude : quotientMagnitude;
    }

    /// The remainder, of the dividend's sign. @pre @p divisor is not zero.
    [[nodiscard]] friend constexpr Int128 operator%(Int128 dividend, Int128 divisor) noexcept
    {
        Int128 const remainderMagnitude =
            from_pattern(detail::u128_divmod(dividend.magnitude_pattern(), divisor.magnitude_pattern()).remainder);
        return dividend.is_negative() ? -remainderMagnitude : remainderMagnitude;
    }

    /// Shifted left by @p places, below 128.
    [[nodiscard]] friend constexpr Int128 operator<<(Int128 operandValue, int places) noexcept
    {
        return from_pattern(detail::portable::shift_left(operandValue.as_pattern(), places));
    }

    /// Shifted right by @p places, below 128, copying the sign into the bits
    /// vacated: -8 >> 1 is -4.
    [[nodiscard]] friend constexpr Int128 operator>>(Int128 operandValue, int places) noexcept
    {
        detail::UInt128 const shifted = detail::portable::shift_right(operandValue.as_pattern(), places);
        if (!operandValue.is_negative() || places == 0)
            return from_pattern(shifted);
        detail::UInt128 const signFill =
            detail::portable::shift_left(detail::UInt128 { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } }, 128 - places);
        return from_pattern(detail::UInt128 { shifted.highWord | signFill.highWord, shifted.lowWord | signFill.lowWord });
    }

    /// The negation, wrapping: the minimum's is itself.
    [[nodiscard]] friend constexpr Int128 operator-(Int128 operandValue) noexcept
    {
        return from_pattern(detail::portable::subtract(detail::UInt128 {}, operandValue.as_pattern()));
    }

    /// Itself.
    [[nodiscard]] friend constexpr Int128 operator+(Int128 operandValue) noexcept { return operandValue; }

    /// `*this = *this + rightOperand`.
    constexpr Int128& operator+=(Int128 rightOperand) noexcept { return *this = *this + rightOperand; }
    /// `*this = *this - rightOperand`.
    constexpr Int128& operator-=(Int128 rightOperand) noexcept { return *this = *this - rightOperand; }
    /// `*this = *this * rightOperand`.
    constexpr Int128& operator*=(Int128 rightOperand) noexcept { return *this = *this * rightOperand; }
    /// `*this = *this / rightOperand`.
    constexpr Int128& operator/=(Int128 rightOperand) noexcept { return *this = *this / rightOperand; }
    /// `*this = *this % rightOperand`.
    constexpr Int128& operator%=(Int128 rightOperand) noexcept { return *this = *this % rightOperand; }
    /// `*this = *this << places`.
    constexpr Int128& operator<<=(int places) noexcept { return *this = *this << places; }
    /// `*this = *this >> places`.
    constexpr Int128& operator>>=(int places) noexcept { return *this = *this >> places; }
    /// Adds one.
    constexpr Int128& operator++() noexcept { return *this += 1; }
    /// Subtracts one.
    constexpr Int128& operator--() noexcept { return *this -= 1; }
    /// Adds one, answering the value before.
    constexpr Int128 operator++(int) noexcept
    {
        Int128 const before = *this;
        ++*this;
        return before;
    }
    /// Subtracts one, answering the value before.
    constexpr Int128 operator--(int) noexcept
    {
        Int128 const before = *this;
        --*this;
        return before;
    }

  private:
    template <typename T>
    [[nodiscard]] static constexpr std::uint64_t sign_word(T whole) noexcept
    {
        if constexpr (std::is_signed_v<T>)
            return whole < 0 ? ~std::uint64_t { 0 } : std::uint64_t { 0 };
        else
            return 0;
    }

    [[nodiscard]] constexpr detail::UInt128 as_pattern() const noexcept { return detail::UInt128 { _highWord, _lowWord }; }

    [[nodiscard]] static constexpr Int128 from_pattern(detail::UInt128 bitPattern) noexcept
    {
        return from_words(bitPattern.highWord, bitPattern.lowWord);
    }

    [[nodiscard]] constexpr detail::UInt128 magnitude_pattern() const noexcept
    {
        return is_negative() ? detail::portable::subtract(detail::UInt128 {}, as_pattern()) : as_pattern();
    }

    std::uint64_t _highWord = 0;
    std::uint64_t _lowWord = 0;
};

namespace detail
{
    /// The magnitude of @p operandValue: 2^127 for the minimum, which has no
    /// signed counterpart.
    [[nodiscard]] constexpr UInt128 magnitude(Int128 operandValue) noexcept
    {
        UInt128 const wordPattern { operandValue.high_word(), operandValue.low_word() };
        return operandValue.is_negative() ? portable::subtract(UInt128 {}, wordPattern) : wordPattern;
    }

    /// The `Int128` of magnitude @p magnitudeOf, negative when @p negative.
    /// @pre it fits: @p magnitudeOf is below 2^127, or is 2^127 and @p negative.
    [[nodiscard]] constexpr Int128 signed_from_magnitude(UInt128 magnitudeOf, bool negative) noexcept
    {
        Int128 const positive = Int128::from_words(magnitudeOf.highWord, magnitudeOf.lowWord);
        return negative ? -positive : positive;
    }
} // namespace detail

} // namespace formula

namespace std
{
/// `formula::Int128`'s limits, stated as a built-in signed integer's are.
template <>
class numeric_limits<formula::Int128>
{
  public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr std::float_round_style round_style = std::round_toward_zero;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr int digits = 127;
    static constexpr int digits10 = 38;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;

    /// -2^127.
    [[nodiscard]] static constexpr formula::Int128 min() noexcept
    {
        return formula::Int128::from_words(std::uint64_t { 1 } << 63, 0);
    }
    /// -2^127.
    [[nodiscard]] static constexpr formula::Int128 lowest() noexcept { return min(); }
    /// 2^127 - 1.
    [[nodiscard]] static constexpr formula::Int128 max() noexcept
    {
        return formula::Int128::from_words(~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 });
    }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 epsilon() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 round_error() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 infinity() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 quiet_NaN() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 signaling_NaN() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 denorm_min() noexcept { return 0; }
};
} // namespace std
```

  If a hygiene check refuses the `#define` or the `std::` specialisation, follow the check's message. `FORMULA_NO_UNIQUE_ADDRESS` is precedent for an internal macro.

- [ ] **Step 4: The formatter.** In `include/formula-cpp/format.hpp`, add `#include <formula-cpp/int128.hpp>` and `#include <algorithm>`. After `formatter<formula::Rational, char>` (`:603`) add:

```cpp
/// `std::format` of a `formula::Int128`: its decimal digits, with a `-` when
/// it is negative, as `{}` writes a built-in integer. The empty spec is the
/// only one; any other calls `formula_number_format_spec_not_understood`, a
/// compile error in a literal format string.
///
/// Owned by this library: a consumer's own specialisation of it would define
/// it twice, which breaks the one-definition rule.
template <>
struct formatter<formula::Int128, char>
{
    /// Accepts only the empty spec.
    constexpr auto parse(std::format_parse_context& parseContext)
    {
        auto const specAt = parseContext.begin();
        if (specAt != parseContext.end() && *specAt != '}')
            formula::detail::formula_number_format_spec_not_understood();
        return specAt;
    }

    /// Writes @p shown's digits.
    template <typename FormatContext>
    auto format(formula::Int128 const& shown, FormatContext& formatContext) const
    {
        formula::detail::DecimalSpelling const spelled = formula::detail::u128_decimal(formula::detail::magnitude(shown));
        auto writtenTo = formatContext.out();
        if (shown.is_negative())
            *writtenTo++ = '-';
        return std::copy_n(spelled.characters, spelled.length, writtenTo);
    }
};
```

  Also add `formula::Int128` to the file comment's list of owned specialisations (`format.hpp:44-52`).

- [ ] **Step 5: Install it, and probe it.**
    - Add `include/formula-cpp/int128.hpp` to the `FILE_SET` in `CMakeLists.txt`.
    - In `test/consumer_globals_tests.cpp`, add `#include <formula-cpp/int128.hpp>` in alphabetical order. Add a use that instantiates the constructor template, every operator and `std::format`, following the file's pattern. Check its result in `consumer_globals_run_tests.cpp`.
    - Add `int128_tests.cpp` to `add_executable(formula-cpp-tests` in `test/CMakeLists.txt`, after `checked_int_tests.cpp`.

- [ ] **Step 6: Run the tests.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "int128"`. Expected: 8 pass. The native agreement test is not compiled on cl.
  Then `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "int128_format_spec_not_understood"`, following the negative-test protocol in *Execution*: wrong text first, then a deletion check of the guard call in `parse`, with the guard restored by a plain write.

- [ ] **Step 7: Verify, including the native path.**
    1. Verify step 1 must print `ALL OK`. Delta: **+8 tests**.
    2. Verify step 2 with `-Filter "int128_format_spec_not_understood|format_spec_not_understood"`. Delta: **+1 negative**.
    3. Verify step 3, on `gcc-release` and `clang-debug` under WSL, must print `MATRIX OK`. There the native agreement test runs, making 9 `[int128]` tests. Report its count.

- [ ] **Step 8: Commit.**
  Message: `feat: add formula::Int128, a 128-bit integer that is native where the compiler has one and constexpr everywhere`.
  Then the `Signed-off-by` line.

### Task 5: Readers of `Rational` made width-agnostic, with no change in behaviour

`Rational::Int` is still `std::int64_t` throughout this task. Every function that reads a numerator or a denominator, or builds a `Rational::Int` from a magnitude, moves onto helpers that work at 64 bits now and at 128 bits after Task 6. **Every test result stays exactly as it was**, the overflow census included. The new helpers get their own tests.

**Files:**
- Modify: `include/formula-cpp/detail/checked_int.hpp`:
    - include `int128.hpp`;
    - the census hook takes a `UInt128`;
    - add `wide_magnitude`, `narrow_to_int64`, `int_from_pattern`;
    - add `Int128` overloads of `add/sub/mul_checked_or_none`, `floor_divmod`, `decimal_digits` and `mul_pow10`, and `gcd(UInt128, UInt128)`.
- Modify: `support/census_tally.cpp`, `support/census_tally.hpp`: keep `UInt128` magnitudes. Headroom stays out of 63 here.
- Modify: `include/formula-cpp/detail/wide_int.hpp`: `WideUnsigned::from_u128`, `to_u128`.
- Modify: `include/formula-cpp/rational.hpp`: add `detail::rational_int_from_magnitude` after the class.
- Modify: `include/formula-cpp/number_text.hpp`:
    - `NumberTextCapacity` and `LongestNumberText`;
    - `put_whole(UInt128)`;
    - `exact_decimal_digits`, `has_exact_decimal`, `fraction_text`, `checked_decimal_text` and `moves_away_from_zero` move to 128-bit magnitudes.
- Modify: `include/formula-cpp/rounding.hpp`: `at_least_pow10`, `checked_decimal_exponent`.
- Modify: `include/formula-cpp/detail/wide_rounding.hpp`: `wide_from_rational`, `scaled_to_denominator`, `round_wide_ratio`, `narrow_wide_ratio`.
- Modify: `include/formula-cpp/detail/transcendental.hpp`: `natural_log_magnitude` (`:218-248`) and `exponential_enclosure` (`:288-293`) refuse a fraction wider than 64 bits.
- Modify: `include/formula-cpp/critical_value.hpp`: `find_sample_size` (`:283-297`) and `as_sample_size` (`:299-308`), with their callers `critical_value.hpp:456` and `trace.hpp:2847-2856`.
- Modify: `include/formula-cpp/detail/least_squares_kernel.hpp:136-141`.
- Modify: `include/formula-cpp/band.hpp:116-119`, `include/formula-cpp/lookup.hpp:1330-1335`, `include/formula-cpp/trace.hpp:2542-2549` (`point_in`), `include/formula-cpp/trace.hpp:2720-2731` (`unit_quotient`), `include/formula-cpp/rounded_root.hpp:266-279` (`rounded_square_root_in`'s units).
- Test: `test/checked_int_tests.cpp`, `test/wide_rounding_tests.cpp`, `test/rational_tests.cpp`.

**Interfaces:**
- Consumes, from Task 4: `UInt128`, `UInt128Division`, `u128_*`, `magnitude(Int128)`, `signed_from_magnitude`, `Int128`.
- Produces, in `formula::detail`:
    - `wide_magnitude(Int) -> UInt128` and `wide_magnitude(Int128) -> UInt128`;
    - `narrow_to_int64(Int)` and `narrow_to_int64(Int128) -> std::optional<Int>`;
    - `int_from_pattern(UInt128, std::type_identity<Int>) -> Int` and `int_from_pattern(UInt128, std::type_identity<Int128>) -> Int128`;
    - `add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none` on `Int128`, each `-> std::optional<Int128>`;
    - `struct WideDivMod { Int128 quotient; Int128 remainder; }` and `floor_divmod(Int128, Int128) -> WideDivMod`;
    - `decimal_digits(Int128) -> int`, `mul_pow10(Int128, int) -> std::optional<Int128>` (exponents 0–18 as on 64 bits), and `gcd(UInt128, UInt128) -> UInt128`;
    - `census_record(CensusRole, UInt128)`, with `census_note` overloads for `UInt128` and `std::uint64_t`;
    - `rational_int_from_magnitude(UInt128, bool) -> std::optional<Rational::Int>`;
    - `WideUnsigned<L>::from_u128(UInt128)` (`L >= 4`) and `WideUnsigned<L>::to_u128() -> std::optional<UInt128>`;
    - `as_sample_size(Rational) -> std::optional<UInt128>` and `find_sample_size<Sizes>(UInt128)`;
    - `formula_band_bound_out_of_range()` and `formula_breakpoint_key_out_of_range()`, non-`constexpr` `[[noreturn]]` guards.

- [ ] **Step 1: Write the failing tests.** Add to `test/checked_int_tests.cpp` (add `#include <limits>` and `#include <type_traits>` if absent):

```cpp
TEST_CASE("the 128-bit checked operations refuse exactly at the range's ends", "[checked-int]")
{
    using formula::Int128;
    using formula::detail::add_checked_or_none;
    using formula::detail::mul_checked_or_none;
    using formula::detail::sub_checked_or_none;
    constexpr Int128 largest = std::numeric_limits<Int128>::max();
    constexpr Int128 smallest = std::numeric_limits<Int128>::min();
    STATIC_REQUIRE(add_checked_or_none(largest - 1, Int128 { 1 }) == std::optional<Int128> { largest });
    STATIC_REQUIRE(add_checked_or_none(largest, Int128 { 1 }) == std::nullopt);
    STATIC_REQUIRE(add_checked_or_none(smallest, Int128 { -1 }) == std::nullopt);
    STATIC_REQUIRE(sub_checked_or_none(Int128 { -1 }, largest) == std::optional<Int128> { smallest });
    STATIC_REQUIRE(sub_checked_or_none(smallest, Int128 { 1 }) == std::nullopt);
    STATIC_REQUIRE(sub_checked_or_none(largest, Int128 { -1 }) == std::nullopt);
    // 2^63 * 2^64 = 2^127: one past the largest, and exactly the smallest when negative.
    constexpr Int128 twoTo63 = Int128 { 1 } << 63;
    constexpr Int128 twoTo64 = Int128 { 1 } << 64;
    STATIC_REQUIRE(mul_checked_or_none(twoTo63, twoTo64) == std::nullopt);
    STATIC_REQUIRE(mul_checked_or_none(-twoTo63, twoTo64) == std::optional<Int128> { smallest });
    STATIC_REQUIRE(mul_checked_or_none(twoTo63, -twoTo64) == std::optional<Int128> { smallest });
    STATIC_REQUIRE(mul_checked_or_none(smallest, Int128 { -1 }) == std::nullopt);
    STATIC_REQUIRE(mul_checked_or_none(smallest, Int128 { 1 }) == std::optional<Int128> { smallest });
    STATIC_REQUIRE(mul_checked_or_none(Int128 { 0 }, smallest) == std::optional<Int128> { Int128 { 0 } });
}

TEST_CASE("the helpers that read an integer of either width", "[checked-int]")
{
    using formula::Int128;
    using formula::detail::UInt128;
    STATIC_REQUIRE(formula::detail::wide_magnitude(std::int64_t { -5 }) == UInt128::from_u64(5));
    STATIC_REQUIRE(formula::detail::wide_magnitude(std::numeric_limits<std::int64_t>::min())
                   == UInt128::from_u64(std::uint64_t { 1 } << 63));
    STATIC_REQUIRE(formula::detail::wide_magnitude(std::numeric_limits<Int128>::min()) == UInt128 { std::uint64_t { 1 } << 63, 0 });
    STATIC_REQUIRE(formula::detail::narrow_to_int64(Int128 { -7 }) == std::optional<std::int64_t> { -7 });
    STATIC_REQUIRE(formula::detail::narrow_to_int64(Int128 { 1 } << 63) == std::nullopt);
    STATIC_REQUIRE(formula::detail::int_from_pattern(UInt128 { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } - 4 },
                                                     std::type_identity<Int128> {})
                   == Int128 { -5 });
    STATIC_REQUIRE(formula::detail::int_from_pattern(UInt128 { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } - 4 },
                                                     std::type_identity<std::int64_t> {})
                   == std::int64_t { -5 });
    STATIC_REQUIRE(formula::detail::floor_divmod(Int128 { -7 }, Int128 { 2 }).quotient == Int128 { -4 });
    STATIC_REQUIRE(formula::detail::floor_divmod(Int128 { -7 }, Int128 { 2 }).remainder == Int128 { 1 });
    STATIC_REQUIRE(formula::detail::decimal_digits(std::numeric_limits<Int128>::max()) == 39);
    STATIC_REQUIRE(formula::detail::decimal_digits(Int128 { 0 }) == 1);
    STATIC_REQUIRE(formula::detail::mul_pow10(Int128 { 3 }, 18) == std::optional<Int128> { Int128 { 3'000'000'000'000'000'000LL } });
    STATIC_REQUIRE(formula::detail::mul_pow10(Int128 { 3 }, 19) == std::nullopt);
}
```

  Add to `test/wide_rounding_tests.cpp`:

```cpp
TEST_CASE("a wide integer holds 128 bits exactly, and says when it holds more", "[wide-int]")
{
    using formula::detail::UInt128;
    using formula::detail::WideUnsigned;
    constexpr UInt128 widest { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } };
    STATIC_REQUIRE(WideUnsigned<4>::from_u128(widest).to_u128() == std::optional<UInt128> { widest });
    STATIC_REQUIRE(WideUnsigned<8>::from_u128(UInt128 { 0x0123456789abcdef, 0xfedcba9876543210 }).to_u128()
                   == std::optional<UInt128> { UInt128 { 0x0123456789abcdef, 0xfedcba9876543210 } });
    constexpr auto twoTo128 = formula::detail::shift_left_checked_or_none(WideUnsigned<8>::from_u64(1), 128);
    STATIC_REQUIRE(twoTo128.has_value());
    STATIC_REQUIRE(twoTo128->to_u128() == std::nullopt);
}
```

  `shift_left_checked_or_none` is `wide_int.hpp`'s checked left shift (`:251`). Pass the shift count in the type its signature declares.

  Add to `test/rational_tests.cpp`:

```cpp
TEST_CASE("a Rational::Int is built from a magnitude, or refused when it does not fit", "[rational]")
{
    using formula::detail::UInt128;
    using formula::detail::rational_int_from_magnitude;
    constexpr auto largestMagnitude = formula::detail::wide_magnitude(std::numeric_limits<formula::Rational::Int>::max());
    STATIC_REQUIRE(rational_int_from_magnitude(UInt128::from_u64(5), true) == std::optional<formula::Rational::Int> { -5 });
    STATIC_REQUIRE(rational_int_from_magnitude(largestMagnitude, false)
                   == std::optional<formula::Rational::Int> { std::numeric_limits<formula::Rational::Int>::max() });
    STATIC_REQUIRE(rational_int_from_magnitude(formula::detail::u128_add(largestMagnitude, UInt128::from_u64(1)), true)
                   == std::optional<formula::Rational::Int> { std::numeric_limits<formula::Rational::Int>::min() });
    STATIC_REQUIRE(rational_int_from_magnitude(formula::detail::u128_add(largestMagnitude, UInt128::from_u64(1)), false)
                   == std::nullopt);
}
```

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "checked-int|wide-int|rational"`. Expected: a build failure naming the missing helpers.

- [ ] **Step 3: `checked_int.hpp`.**
    - Add `#include <formula-cpp/int128.hpp>` and `#include <type_traits>`.
    - Replace the census declarations (`:66-78`): `census_record` takes `UInt128`; `census_note` has a `UInt128` overload and a `std::uint64_t` one that widens:

```cpp
/// Told the magnitude of an integer formed at run time, as 128 bits. Declared
/// here and defined only by the census program, never by the library.
void census_record(CensusRole role, UInt128 magnitudeSeen) noexcept;

/// Tells the overflow census of @p magnitudeSeen, unless this is a constant
/// evaluation.
constexpr void census_note(CensusRole role, UInt128 magnitudeSeen) noexcept
{
    if !consteval
    {
        census_record(role, magnitudeSeen);
    }
}

/// Tells the overflow census of a 64-bit @p magnitudeSeen.
constexpr void census_note(CensusRole role, std::uint64_t magnitudeSeen) noexcept
{
    census_note(role, UInt128::from_u64(magnitudeSeen));
}
```

  In the file comment, change "how many of the 63 bits real formulas use" to "how many of the bits `Rational::Int` holds real formulas use".

  At the end of the namespace, add:

```cpp
// ---- 128 bits: `Int128`'s checked operations, and helpers that read
// `Rational::Int` whatever its width -------------------------------------------

/// @p operandValue's magnitude as 128 bits.
[[nodiscard]] constexpr UInt128 wide_magnitude(Int operandValue) noexcept
{
    return UInt128::from_u64(magnitude(operandValue));
}

/// @p operandValue's magnitude: 2^127 for the minimum.
[[nodiscard]] constexpr UInt128 wide_magnitude(Int128 operandValue) noexcept
{
    return magnitude(operandValue);
}

/// @p operandValue as a 64-bit integer, which it always is.
[[nodiscard]] constexpr std::optional<Int> narrow_to_int64(Int operandValue) noexcept
{
    return operandValue;
}

/// @p operandValue as a 64-bit integer, or nothing when it does not fit.
[[nodiscard]] constexpr std::optional<Int> narrow_to_int64(Int128 operandValue) noexcept
{
    return operandValue.to_int64();
}

/// The 64-bit integer whose two's complement bits are @p wordPattern's low
/// word. @pre the value fits: the high word is the low word's sign.
[[nodiscard]] constexpr Int int_from_pattern(UInt128 wordPattern, std::type_identity<Int>) noexcept
{
    // Well defined since C++20: conversion to a signed type is modular.
    return static_cast<Int>(wordPattern.lowWord);
}

/// The `Int128` whose two's complement bits are @p wordPattern.
[[nodiscard]] constexpr Int128 int_from_pattern(UInt128 wordPattern, std::type_identity<Int128>) noexcept
{
    return Int128::from_words(wordPattern.highWord, wordPattern.lowWord);
}

[[nodiscard]] constexpr std::optional<Int128> add_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    Int128 const added = leftOperand + rightOperand;
    if (leftOperand.is_negative() == rightOperand.is_negative() && added.is_negative() != leftOperand.is_negative())
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(added));
    return added;
}

[[nodiscard]] constexpr std::optional<Int128> sub_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    Int128 const subtracted = leftOperand - rightOperand;
    if (leftOperand.is_negative() != rightOperand.is_negative() && subtracted.is_negative() != leftOperand.is_negative())
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(subtracted));
    return subtracted;
}

[[nodiscard]] constexpr std::optional<Int128> mul_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    std::optional<UInt128> const productMagnitude = u128_mul_checked(magnitude(leftOperand), magnitude(rightOperand));
    if (!productMagnitude)
        return std::nullopt;
    bool const negative = !productMagnitude->is_zero() && leftOperand.is_negative() != rightOperand.is_negative();
    UInt128 const largestMagnitude = negative ? UInt128 { std::uint64_t { 1 } << 63, 0 }
                                              : UInt128 { ~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 } };
    if (largestMagnitude < *productMagnitude)
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, *productMagnitude);
    return signed_from_magnitude(*productMagnitude, negative);
}

/// The greatest common divisor of two 128-bit magnitudes (`u128_gcd`).
[[nodiscard]] constexpr UInt128 gcd(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return u128_gcd(leftOperand, rightOperand);
}

/// `floor_divmod`'s answer on 128 bits.
struct WideDivMod
{
    /// The floored quotient.
    Int128 quotient {};
    /// The remainder, in `[0, divisor)`.
    Int128 remainder {};
};

/// Floored division on 128 bits, as `floor_divmod` on 64. @pre `divisor > 0`.
[[nodiscard]] constexpr WideDivMod floor_divmod(Int128 dividend, Int128 divisor) noexcept
{
    Int128 truncated = dividend / divisor;
    Int128 remainderLeft = dividend % divisor;
    if (remainderLeft < 0)
    {
        --truncated;
        remainderLeft += divisor;
    }
    return { truncated, remainderLeft };
}

/// Number of decimal digits in `|operandValue|`: at most 39. Zero has one.
[[nodiscard]] constexpr int decimal_digits(Int128 operandValue) noexcept
{
    return u128_decimal(magnitude(operandValue)).length;
}

/// `operandValue * 10^exponent`, for the exponents 0 to 18 `mul_pow10` takes
/// on 64 bits, or nothing on overflow or an exponent out of that range.
[[nodiscard]] constexpr std::optional<Int128> mul_pow10(Int128 operandValue, int exponent) noexcept
{
    std::optional<Int> const powerOfTen = pow10(exponent);
    if (!powerOfTen)
        return std::nullopt;
    return mul_checked_or_none(operandValue, Int128 { *powerOfTen });
}
```

- [ ] **Step 4: The census tally keeps 128 bits.**
    - In `support/census_tally.cpp`, replace `std::array<std::uint64_t, 4> largestSeen {};` with `std::array<formula::detail::UInt128, 4> largestSeen {};`.
    - Give `census_record` the signature `void census_record(CensusRole role, UInt128 magnitudeSeen) noexcept` and the body `if (largestSeen[slot(role)] < magnitudeSeen) largestSeen[slot(role)] = magnitudeSeen;`.
    - `bits_used` returns `largestSeen[slot(role)].bit_width()`.
    - Keep `signed_bits_used`'s `std::min(63, ...)` as it is: Task 6 changes the width.
    - In `census_tally.hpp`, change the comment "INT64_MAX uses 63; `rounded_sqrt`'s unsigned intermediates may use 64" to "the largest `Rational::Int` uses all but its sign bit; `rounded_sqrt`'s unsigned intermediates may use every bit".

- [ ] **Step 5: `WideUnsigned` to and from 128 bits.** In `detail/wide_int.hpp`, add `#include <formula-cpp/int128.hpp>`, and after `from_u64` (`:56-62`):

```cpp
    /// @p narrow, exactly. Four limbs hold it.
    [[nodiscard]] static constexpr WideUnsigned from_u128(UInt128 narrow) noexcept
        requires(Limbs >= 4)
    {
        std::array<std::uint32_t, Limbs> held {};
        held[0] = static_cast<std::uint32_t>(narrow.lowWord & 0xFFFF'FFFFU);
        held[1] = static_cast<std::uint32_t>(narrow.lowWord >> 32U);
        held[2] = static_cast<std::uint32_t>(narrow.highWord & 0xFFFF'FFFFU);
        held[3] = static_cast<std::uint32_t>(narrow.highWord >> 32U);
        return from_limbs(held);
    }
```

  And after `to_u64`:

```cpp
    /// This value as 128 bits, or nothing when it needs more.
    [[nodiscard]] constexpr std::optional<UInt128> to_u128() const noexcept
    {
        for (std::size_t limbAt = 4; limbAt < Limbs; ++limbAt)
            if (_limbs[limbAt] != 0U)
                return std::nullopt;
        auto const limbOrZero = [this](std::size_t limbIndex) -> std::uint64_t {
            return limbIndex < Limbs ? static_cast<std::uint64_t>(_limbs[limbIndex]) : std::uint64_t { 0 };
        };
        return UInt128 { (limbOrZero(3) << 32U) | limbOrZero(2), (limbOrZero(1) << 32U) | limbOrZero(0) };
    }
```

- [ ] **Step 6: `rational_int_from_magnitude`.** In `rational.hpp`, directly after `class Rational`'s closing `};`:

```cpp
namespace detail
{
    /// The `Rational::Int` of magnitude @p magnitudeOf, negative when
    /// @p negative, or nothing when `Rational::Int` cannot hold it: one
    /// spelling for code that builds a numerator from a wide magnitude,
    /// whatever `Rational::Int`'s width.
    [[nodiscard]] constexpr std::optional<Rational::Int> rational_int_from_magnitude(UInt128 magnitudeOf,
                                                                                     bool negative) noexcept
    {
        UInt128 const largestPositive = wide_magnitude(std::numeric_limits<Rational::Int>::max());
        UInt128 const largestAllowed = negative ? u128_add(largestPositive, UInt128::from_u64(1)) : largestPositive;
        if (largestAllowed < magnitudeOf)
            return std::nullopt;
        UInt128 const wordPattern = negative ? u128_sub(UInt128 {}, magnitudeOf) : magnitudeOf;
        return int_from_pattern(wordPattern, std::type_identity<Rational::Int> {});
    }
} // namespace detail
```

- [ ] **Step 7: `number_text.hpp`.**
    - `NumberTextCapacity` (`:43`) becomes `128`. Its comment, if any, names the longest text.
    - `LongestNumberText` (`:254-260`) becomes the following, and the `static_assert` stays:

```cpp
    /// The longest text this header spells: the marker, a sign, the 39 digits
    /// of 2^127, then a point and 18 places or a slash and a 39-digit
    /// denominator, a space, and a unit symbol of `SymbolCapacity` bytes --
    /// `view(Symbol const&)` returns that many from a symbol with no
    /// terminator.
    inline constexpr std::size_t LongestNumberText =
        ApproximationMarker.size() + 1 + 39 + 1 + 39 + 1 + SymbolCapacity;
```

    - In `NumberTextAccess`, beside `put_whole(NumberText&, std::uint64_t)`, add:

```cpp
        /// Appends @p wholeNumber in decimal: at most 39 digits.
        static constexpr void put_whole(NumberText& spelled, UInt128 wholeNumber) noexcept
        {
            DecimalSpelling const written = u128_decimal(wholeNumber);
            for (int at = 0; at < written.length; ++at)
                put(spelled, written.characters[at]);
        }
```

    - `exact_decimal_digits` (`:278-302`), with its comments kept:

```cpp
    [[nodiscard]] constexpr std::optional<NumberText> exact_decimal_digits(Rational shownValue, int minimumPlaces) noexcept
    {
        UInt128 const divisor = wide_magnitude(shownValue.denominator());
        UInt128Division const scaleSplit = u128_divmod(UInt128::from_u64(ExactDecimalScale), divisor);
        if (!scaleSplit.remainder.is_zero())
            return std::nullopt;

        UInt128Division const shownSplit = u128_divmod(wide_magnitude(shownValue.numerator()), divisor);
        // The remainder is below the divisor, which divides 10^18, so both it
        // and the cofactor 10^18 / divisor fit 64 bits, and so does their
        // product, which stays below 10^18.
        std::uint64_t fractional = shownSplit.remainder.lowWord * scaleSplit.quotient.lowWord;
        char fractionDigits[ExactDecimalPlaces] {};
        for (int place = ExactDecimalPlaces - 1; place >= 0; --place)
        {
            fractionDigits[place] = static_cast<char>('0' + fractional % 10U);
            fractional /= 10U;
        }
        int shownPlaces = ExactDecimalPlaces;
        while (shownPlaces > minimumPlaces && fractionDigits[shownPlaces - 1] == '0')
            --shownPlaces;

        NumberText spelled = NumberTextAccess::blank();
        if (shownValue.sign() < 0)
            NumberTextAccess::put(spelled, '-');
        NumberTextAccess::put_whole(spelled, shownSplit.quotient);
        NumberTextAccess::put_fraction(spelled, fractionDigits, shownPlaces);
        return spelled;
    }
```

    - `moves_away_from_zero`: its first two parameters become `UInt128 remainderLeft, UInt128 divisor`. Inside, `if (remainderLeft.is_zero())` and `UInt128 const distanceUp = u128_sub(divisor, remainderLeft);`; the `<` and `>` comparisons stay as written.
    - `has_exact_decimal`'s body becomes:
      `return detail::u128_divmod(detail::UInt128::from_u64(detail::ExactDecimalScale), detail::wide_magnitude(shownValue.denominator())).remainder.is_zero();`
    - `fraction_text`: both `put_whole` calls take `detail::wide_magnitude(...)` of the numerator and of the denominator.
    - `checked_decimal_text` (`:436-500`), every 64-bit magnitude becomes a `detail::UInt128`:

```cpp
    detail::UInt128 const divisor = detail::wide_magnitude(unrounded.denominator());
    detail::UInt128Division const shownSplit = detail::u128_divmod(detail::wide_magnitude(unrounded.numerator()), divisor);
    detail::UInt128 wholePart = shownSplit.quotient;
    detail::UInt128 remainderLeft = shownSplit.remainder;
```

      The digit loop's body becomes the following. Update the comment above it to "after it the sum is below 2 * divisor < 2^128".

```cpp
        detail::UInt128 tenfold {};
        int nextDigit = 0;
        for (int added = 0; added < 10; ++added)
        {
            tenfold = detail::u128_add(tenfold, remainderLeft);
            if (!(tenfold < divisor))
            {
                tenfold = detail::u128_sub(tenfold, divisor);
                ++nextDigit;
            }
        }
        fractionDigits[place] = static_cast<char>('0' + nextDigit);
        remainderLeft = tenfold;
```

      Then:
        - `lastKeptOdd`'s whole-part case: `(wholePart.lowWord & 1U) != 0U`;
        - `++wholePart;` becomes `wholePart = detail::u128_add(wholePart, detail::UInt128::from_u64(1));`, its comment changed to "below 2^127: a value with a remainder has a denominator of at least 2";
        - `wholePart == 0U` becomes `wholePart.is_zero()`;
        - `remainderLeft != 0U` becomes `!remainderLeft.is_zero()`.

      In the doc comment, "in `std::uint64_t`" becomes "in 128-bit unsigned integers". Task 7 rewrites the `IntMax/3` example.

- [ ] **Step 8: `rounding.hpp`.** `detail::at_least_pow10` (`:210-237`) becomes:

```cpp
    /// Whether `|numerator| / denominator >= 10^exponent`, exactly and without
    /// ever constructing 10^exponent as a Rational -- which is impossible at the
    /// extremes of the representable range. A scaled side beyond the largest
    /// `Rational::Int` already decides the comparison, and is never formed.
    [[nodiscard]] constexpr bool at_least_pow10(UInt128 magnitudeNumerator,
                                                UInt128 magnitudeDenominator,
                                                int exponent) noexcept
    {
        constexpr UInt128 Largest = wide_magnitude(std::numeric_limits<Rational::Int>::max());
        if (exponent >= 0)
        {
            std::optional<UInt128> const powerOfTen = u128_pow10(exponent);
            std::optional<UInt128> const scaledDenominator =
                powerOfTen ? u128_mul_checked(magnitudeDenominator, *powerOfTen) : std::nullopt;
            // Beyond the largest numerator, the quotient is below 10^exponent.
            if (!scaledDenominator || Largest < *scaledDenominator)
                return false;
            FORMULA_CENSUS_NOTE(Intermediate, *scaledDenominator);
            return !(magnitudeNumerator < *scaledDenominator);
        }
        std::optional<UInt128> const powerOfTen = u128_pow10(-exponent);
        std::optional<UInt128> const scaledNumerator =
            powerOfTen ? u128_mul_checked(magnitudeNumerator, *powerOfTen) : std::nullopt;
        if (!scaledNumerator || Largest < *scaledNumerator)
            return true;
        FORMULA_CENSUS_NOTE(Intermediate, *scaledNumerator);
        return !(*scaledNumerator < magnitudeDenominator);
    }
```

  This is the old function's decision in every case, and it notes the same intermediates to the census. At 64 bits, an exponent of 19 or more scales any denominator past `Largest`, exactly where the old `> 18` returned. In `checked_decimal_exponent` (`:248-249`), the two magnitudes become `detail::UInt128 const ... = detail::wide_magnitude(examinedValue.numerator())` and `... .denominator()`.

- [ ] **Step 9: `wide_rounding.hpp`.**
    - `wide_from_rational`: both `from_u64(...)` calls become `WideUnsigned<L>::from_u128(wide_magnitude(...))`, of the numerator and of the denominator. The comment's second sentence becomes "Four limbs at least, so that a 128-bit numerator fits."
    - `scaled_to_denominator`: gains `requires(L >= 4)`, and both `from_u64(...)` become `from_u128(wide_magnitude(...))`.
    - `round_wide_ratio`'s tail (`:146-155`) becomes:

```cpp
    std::optional<WideUnsigned<L>> const kept =
        awayFromZero ? add_small_checked_or_none(split.quotient, 1U) : split.quotient;
    std::optional<UInt128> const keptMagnitude = kept ? kept->to_u128() : std::nullopt;
    std::optional<Rational::Int> const mantissa =
        keptMagnitude ? rational_int_from_magnitude(*keptMagnitude, inLowestTerms.negative) : std::nullopt;
    if (!mantissa)
        return std::unexpected { ArithmeticError::Overflow };
    return Rational::from_decimal(*mantissa, -places.value);
```

    - `narrow_wide_ratio`'s body after `reduced` (`:167-175`) becomes:

```cpp
    std::optional<UInt128> const numeratorMagnitude = inLowestTerms.numerator.to_u128();
    std::optional<UInt128> const denominatorMagnitude = inLowestTerms.denominator.to_u128();
    std::optional<Rational::Int> const signedNumerator =
        numeratorMagnitude ? rational_int_from_magnitude(*numeratorMagnitude, inLowestTerms.negative) : std::nullopt;
    std::optional<Rational::Int> const positiveDenominator =
        denominatorMagnitude ? rational_int_from_magnitude(*denominatorMagnitude, false) : std::nullopt;
    if (!signedNumerator || !positiveDenominator)
        return std::unexpected { ArithmeticError::Overflow };
    return Rational::make(*signedNumerator, *positiveDenominator);
```

- [ ] **Step 10: `transcendental.hpp`.** The kernel works on values below 2^63, so a wider fraction is beyond it and is refused, as any other kernel overflow is.
    - In `natural_log_magnitude`, replace the two `static_cast<std::uint64_t>` lines (`:220-221`) with:

```cpp
    std::optional<std::int64_t> const numeratorWord = narrow_to_int64(positive.numerator());
    std::optional<std::int64_t> const denominatorWord = narrow_to_int64(positive.denominator());
    // The kernel works on a fraction of two values below 2^63; a wider one is
    // beyond it.
    if (!numeratorWord || !denominatorWord)
        return std::nullopt;
    auto larger = static_cast<std::uint64_t>(*numeratorWord);
    auto smaller = static_cast<std::uint64_t>(*denominatorWord);
```

    - In `exponential_enclosure`, the same narrowing goes before `scaled_quotient`. Use `static_cast<std::uint64_t>(*denominatorWord)`, and for the numerator `magnitude(*numeratorWord)`, the 64-bit overload. On failure `return std::nullopt;`.

- [ ] **Step 11: `critical_value.hpp`, and the trace's sample-size step.**
    - `find_sample_size` takes `UInt128 sampleSize` and compares `UInt128::from_u64(static_cast<std::uint64_t>(Sizes[rowIndex])) == sampleSize`.
    - `as_sample_size` returns `std::optional<UInt128>`, with the body `if (evaluatedCount.denominator() != 1 || evaluatedCount.numerator() < 0) return std::nullopt; return wide_magnitude(evaluatedCount.numerator());`. Add to its comment: "a count beyond 2^64 - 1 is still a count, which no table declares".
    - `critical_value.hpp:456` declares `std::optional<detail::UInt128> const sampleSize`.
    - In `trace.hpp:2847-2856`:

```cpp
            std::optional<UInt128> const sampleSize = as_sample_size(*operandValue);
            if (!sampleSize.has_value())
            {
                step.lookupFailure = LookupFailure::NotACount;
                return;
            }
            // A count beyond 2^64 - 1 is one no table declares: it misses,
            // with no key a step can hold.
            if (sampleSize->fits_u64())
                step.lookupKey = sampleSize->lowWord;
```

      Keep the two `if`s after it, passing `*sampleSize` to `find_sample_size`.

- [ ] **Step 12: `least_squares_kernel.hpp:136-141`.**

```cpp
        UInt128 const denominatorValue = wide_magnitude(observed.denominator());
        if (denominatorValue.fits_u64() && denominatorValue.lowWord <= 0xFFFF'FFFFU
            && divmod_small(common, static_cast<std::uint32_t>(denominatorValue.lowWord)).remainder == 0)
            continue;
        std::optional<WideUnsigned<L>> const grown =
            lcm_checked_or_none(common, WideUnsigned<L>::from_u128(denominatorValue));
```

  Add `requires(L >= 4)` to `common_denominator` if the compiler asks for it.

- [ ] **Step 13: The structural types narrow on purpose.** `Band`, `Breakpoint` and `Unit` hold `std::int64_t`, so they stay structural types.
    - **`band.hpp:116-119`**, `band(Rational, Rational)`. Add `#include <cstdlib>`. Before the function, add:

```cpp
namespace detail
{
    /// A `band` bound that a `Band`'s `std::int64_t` numerator or denominator
    /// cannot hold. Deliberately not `constexpr`: reaching it in a constant
    /// expression fails to compile, naming it. At run time it ends the
    /// program -- a `Band` is a template argument, built at compile time,
    /// and has no way to carry a failure.
    [[noreturn]] inline void formula_band_bound_out_of_range()
    {
        std::abort();
    }
} // namespace detail
```

      and the body:

```cpp
    std::optional<std::int64_t> const lowTop = detail::narrow_to_int64(lowBound.numerator());
    std::optional<std::int64_t> const lowBottom = detail::narrow_to_int64(lowBound.denominator());
    std::optional<std::int64_t> const highTop = detail::narrow_to_int64(highBound.numerator());
    std::optional<std::int64_t> const highBottom = detail::narrow_to_int64(highBound.denominator());
    if (!lowTop || !lowBottom || !highTop || !highBottom)
        detail::formula_band_bound_out_of_range();
    return { *lowTop, *lowBottom, *highTop, *highBottom };
```

      Add to its comment: "A bound beyond 64 bits fails to compile, naming `formula_band_bound_out_of_range`."
    - **`lookup.hpp:1330-1335`**, `breakpoint(Rational)`: the same, with `formula_breakpoint_key_out_of_range` and the locals `keyTop`, `keyBottom`.
    - **`trace.hpp`, `point_in`**:

```cpp
        std::optional<std::int64_t> const keyTop = narrow_to_int64(stated->numerator());
        std::optional<std::int64_t> const keyBottom = narrow_to_int64(stated->denominator());
        if (!keyTop || !keyBottom)
            return std::nullopt;
        return Breakpoint { *keyTop, *keyBottom };
```

    - **`trace.hpp`, `unit_quotient`**: narrow `magnitude->numerator()` and `->denominator()` the same way, `return std::nullopt` when either does not fit, and use the narrowed values in `quotientUnit`.
    - **`rounded_root.hpp`, `rounded_square_root_in`**: narrow `unitFactor`'s and `factorSquared`'s numerator and denominator, and `return std::unexpected { ArithmeticError::Overflow };` when any does not fit.

- [ ] **Step 14: Run the new tests, then Verify.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "checked-int|wide-int|rational"`, then Verify step 1. Both must print `ALL OK`. Delta: **+4 tests**. Every census figure, gallery line and checked guide is unchanged, so no regeneration happens in this task. If `docs.numeric-headroom` or `gallery.is-current` fails, a reader's behaviour changed: find it and fix it.

- [ ] **Step 15: Commit.**
  Message: `refactor: read Rational's integers through helpers that hold 128 bits`.
  Body: no behaviour change; the readers, the census tally and the wide integers now carry 128-bit magnitudes, ready for a wider `Rational::Int`. Then the `Signed-off-by` line.

### Task 6: `Rational` over `Int128`, and the census re-measured

Switch `Rational::Int` to `Int128`, widen `rounded_sqrt` to 128 bits, re-measure the overflow census, and pin the issue's cases, which now answer.

**Files:**
- Modify: `include/formula-cpp/rational.hpp`: `Int`, `make`, `from_double_exact`, `to_double`, `checked_negate`, `checked_add`, `checked_mul`, `exact_integer_root`, `checked_exact_nth_root`; a constructor from `Int`; the file comment.
- Modify: `include/formula-cpp/rounded_root.hpp`: `mul_unsigned_or_none`, `unsigned_pow10`, `rounded_square_root` move to `UInt128`; `integer_square_root` is replaced by `u128_isqrt`.
- Modify: `support/census_tally.cpp` (`signed_bits_used` out of 127), `support/census_report.cpp` ("of 127"), `cmake/CheckCensusPage.cmake:77` ("of 127").
- Modify: `test/overflow_census_tests.cpp`, `tools/census/exact_sizes.py`, `CMakeLists.txt:209,213`.
- Regenerate: `docs/numeric-headroom.md`'s tables.
- Modify: `examples/opaque_and_retry.cpp` and its guide `docs/opaque-and-retry.md`, plus `examples/CMakeLists.txt:280`, so the fit still refuses.
- Modify: every test the switch breaks. See Step 7.
- Modify: `test/package/main.cpp` and `tools/gallery/main.cpp`, where they store `numerator()` or `denominator()` in a built-in integer.
- Create: `test/negative/band_bound_out_of_range.cpp`, `test/negative/breakpoint_key_out_of_range.cpp`.
- Test: `test/rational_tests.cpp`, `test/statistics_tests.cpp`, `test/lookup_tests.cpp`.

**Interfaces:**
- Consumes: everything from Tasks 4 and 5.
- Produces:
    - `Rational::Int` is `formula::Int128`;
    - `Rational(Int whole)`;
    - `std::numeric_limits<Rational::Int>` holds the bounds; no `detail::IntMax` / `IntMin` is used for `Rational`'s range any more.

- [ ] **Step 1: Write the failing tests.** Add to `test/rational_tests.cpp` (add `#include <formula-cpp/number_text.hpp>` and `#include <formula-cpp/rounding.hpp>` if absent):

```cpp
TEST_CASE("a Rational holds 128-bit numerators and denominators", "[rational]")
{
    using formula::Int128;
    using formula::Rational;
    constexpr Int128 largest = std::numeric_limits<Int128>::max();
    constexpr Int128 smallest = std::numeric_limits<Int128>::min();
    STATIC_REQUIRE(std::is_same_v<Rational::Int, Int128>);
    // The minimum is a numerator; its negation, absolute value and
    // reciprocal are not representable, and are refused.
    constexpr auto lowest = Rational::make(smallest, 1);
    STATIC_REQUIRE(lowest.has_value());
    STATIC_REQUIRE(lowest->numerator() == smallest);
    STATIC_REQUIRE(formula::checked_negate(*lowest).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_abs(*lowest).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_reciprocal(*lowest).error() == formula::ArithmeticError::Overflow);
    // One past the largest is refused where the 64-bit sum used to be.
    STATIC_REQUIRE(formula::checked_add(Rational { largest }, Rational { 1 }).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_add(Rational { std::numeric_limits<std::int64_t>::max() }, Rational { 1 }).has_value());
}

TEST_CASE("the longest numbers a Rational holds are spelled in full", "[rational][number-text]")
{
    using formula::Int128;
    using formula::Rational;
    constexpr Int128 largest = std::numeric_limits<Int128>::max();
    CHECK(formula::fraction_text(*Rational::make(std::numeric_limits<Int128>::min(), 1)).view()
          == "-170141183460469231731687303715884105728");
    CHECK(formula::fraction_text(*Rational::make(largest, largest - 1)).view()
          == "170141183460469231731687303715884105727/170141183460469231731687303715884105726");
    auto const thirdOfLargest = formula::checked_decimal_text(*Rational::make(largest, 3), formula::DecimalPlaces { 18 },
                                                              formula::RoundingMode::HalfEven, formula::DecimalPadding::Trimmed);
    REQUIRE(thirdOfLargest.has_value());
    CHECK(thirdOfLargest->view() == "56713727820156410577229101238628035242.333333333333333333");
}
```

  Add to `test/statistics_tests.cpp`. Its fixtures name a mass quantity and a variance quantity in grams squared; use those names:

```cpp
TEST_CASE("the variance of six masses read to the microgram answers exactly", "[statistics]")
{
    // The sample that overflowed 64 bits in kg^2: in g^2 its variance is
    // 2026588050217/6000000000000, exactly.
    auto const sixAtMicrograms = formula::measured_series<Mass>(
        formula::Rational { 40053270, 1000000 }, formula::Rational { 39475922, 1000000 }, formula::Rational { 39025798, 1000000 },
        formula::Rational { 40615904, 1000000 }, formula::Rational { 39418416, 1000000 }, formula::Rational { 40131659, 1000000 });
    auto const variance = formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 6>),
                                                                  formula::environment(sixAtMicrograms));
    REQUIRE(variance.has_value());
    REQUIRE(variance->is_value());
    CHECK(variance->measurement().value() == formula::Rational { 2026588050217, 6000000000000 });
}
```

  Build the environment the way `statistics_tests.cpp`'s existing cases do. The census test's `series_environment<Mass>(...)` (`test/overflow_census_tests.cpp:240`) shows one way.

  Create `test/negative/band_bound_out_of_range.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// A Band's bounds are 64-bit pairs, so that it stays a template argument; a
// bound beyond 64 bits fails to compile, naming the guard.
#include <formula-cpp/band.hpp>
#include <formula-cpp/rational.hpp>

constexpr formula::Band tooWide =
    formula::band(formula::Rational { formula::Int128 { 1 } << 70 }, formula::Rational { formula::Int128 { 1 } << 71 });
```

  and `test/negative/breakpoint_key_out_of_range.cpp` the same way, with `formula::breakpoint(formula::Rational { formula::Int128 { 1 } << 70 })` from `<formula-cpp/lookup.hpp>`. Register both with `formula_add_negative_test`, expecting `formula_band_bound_out_of_range` and `formula_breakpoint_key_out_of_range`. Follow the protocol in *Execution*, including the deletion check.

- [ ] **Step 2: Run them to see them fail.**
  Run: `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "rational|statistics"`. Expected: a build failure (`std::is_same_v<Rational::Int, Int128>`, and `Rational { Int128 }`).

- [ ] **Step 3: Switch `rational.hpp`.**
    - Add `#include <formula-cpp/int128.hpp>`.
    - The file comment: "An exact rational number over std::int64_t." becomes "An exact rational number over `formula::Int128`."
    - `using Int = detail::Int;` becomes:

```cpp
    /// The signed integer numerator and denominator are stored in: 128 bits
    /// (`int128.hpp`).
    using Int = Int128;
```

    - The integer constructor keeps its constraint. Its `static_assert` can no longer fire, since every built-in integer up to 64 bits fits `Int128`. Remove it, and its comment's paragraph about unsigned types as wide as `Int`. Replace them with: "Every built-in integer type fits `Int` exactly." If a negative test pins that `static_assert` (`grep -r "can hold values above Rational's maximum" test`), delete the test and its registration, and say so in the report: what used to be refused is now exact.
    - Add, after it:

```cpp
    /// An `Int`, exactly.
    constexpr Rational(Int whole) noexcept:
        _numerator { whole }
    {
    }
```

    - The template constructor's initialiser becomes `_numerator { whole }`.
    - `make` (`:97-125`):

```cpp
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> make(Int dividend, Int divisor) noexcept
    {
        if (divisor == 0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        FORMULA_CENSUS_NOTE(Numerator, detail::magnitude(dividend));
        FORMULA_CENSUS_NOTE(Denominator, detail::magnitude(divisor));
        if (dividend == 0)
            return Rational {};

        // Reduce in the unsigned domain so that the minimum is an ordinary operand.
        detail::UInt128 const numeratorMagnitude = detail::magnitude(dividend);
        detail::UInt128 const denominatorMagnitude = detail::magnitude(divisor);
        detail::UInt128 const common = detail::gcd(numeratorMagnitude, denominatorMagnitude);
        detail::UInt128 const reducedNumerator = detail::u128_divmod(numeratorMagnitude, common).quotient;
        detail::UInt128 const reducedDenominator = detail::u128_divmod(denominatorMagnitude, common).quotient;

        bool const negative = (dividend < 0) != (divisor < 0);

        constexpr detail::UInt128 PositiveLimit = detail::magnitude(std::numeric_limits<Int>::max());
        detail::UInt128 const numeratorLimit =
            negative ? detail::u128_add(PositiveLimit, detail::UInt128::from_u64(1)) : PositiveLimit;
        if (PositiveLimit < reducedDenominator || numeratorLimit < reducedNumerator)
            return std::unexpected { ArithmeticError::Overflow };

        Rational made {};
        made._numerator = detail::signed_from_magnitude(reducedNumerator, negative);
        made._denominator = detail::signed_from_magnitude(reducedDenominator, false);
        return made;
    }
```

    - `from_double_exact`:
        - `if (shifted >= 63)` becomes `if (shifted >= 127)`, and `if (-shifted >= 63)` becomes `if (-shifted >= 127)`;
        - `auto scaledReduced = static_cast<Int>(reduced);` becomes `Int scaledReduced { reduced };`;
        - `Int { 1 } << shifted` stays, now an `Int128` shift.

      A double whose exact value needs 64 to 126 bits now answers.
    - `to_double` returns `_numerator.to_double() / _denominator.to_double();`.
    - `checked_negate`: `operandValue.numerator() == std::numeric_limits<Rational::Int>::min()`.
    - `checked_add`: `common` becomes:

```cpp
    Rational::Int const common = detail::signed_from_magnitude(
        detail::gcd(detail::magnitude(leftOperand.denominator()), detail::magnitude(rightOperand.denominator())), false);
```

      Its "Known limitation" paragraph says "both numerators near 2^127 over denominators sharing a large factor", and drops the sentence about 128-bit intermediates.
    - `checked_mul`: `leftCross` and `rightCross` become `detail::signed_from_magnitude(detail::gcd(detail::magnitude(...numerator()), detail::magnitude(...denominator())), false)`.
    - `checked_exact_nth_root`:
        - `radicand.numerator() == detail::IntMin` becomes `== std::numeric_limits<Rational::Int>::min()`;
        - `degree >= 63` becomes `degree >= 127`, and its comment says "2^127 alone exceeds the largest `Int`". Without this a 2^64 numerator at degree 64, now representable, would be called `Inexact`: a wrong refusal.
    - `rational_from_spelling` keeps its 64-bit `Int` mantissa: the `_r` literal's caps do not change.

- [ ] **Step 4: `rounded_root.hpp` in 128 bits.**
    - `mul_unsigned_or_none` takes and returns `UInt128`:

```cpp
    [[nodiscard]] constexpr std::optional<UInt128> mul_unsigned_or_none(UInt128 leftFactor, UInt128 rightFactor) noexcept
    {
        std::optional<UInt128> const product = u128_mul_checked(leftFactor, rightFactor);
        if (product)
            FORMULA_CENSUS_NOTE(Unsigned, *product);
        return product;
    }
```

    - `unsigned_pow10(std::int64_t exponent) -> std::optional<UInt128>` is `exponent < 0 || exponent > 38 ? std::nullopt : u128_pow10(static_cast<int>(exponent))`.
    - Delete `integer_square_root`.
    - In `rounded_square_root`, from `auto const wholeNumerator` (`:191`) to `keptDigits` (`:252`):

```cpp
        UInt128 const wholeNumerator = wide_magnitude(radicandInUnitSquared.numerator());
        UInt128 const wholeDenominator = wide_magnitude(radicandInUnitSquared.denominator());
        auto const doubledPlaces = std::int64_t { 2 } * places.value;

        // v * S = wholePart + leftover / divisor.
        UInt128 wholePart {};
        UInt128 leftover {};
        UInt128 divisor = wholeDenominator;
        if (doubledPlaces >= 0)
        {
            std::optional<UInt128> const powerOfTen = unsigned_pow10(doubledPlaces);
            if (!powerOfTen)
                return std::unexpected { ArithmeticError::Overflow };
            UInt128Division const split = u128_divmod(wholeNumerator, wholeDenominator);
            std::optional<UInt128> const scaledWhole = mul_unsigned_or_none(split.quotient, *powerOfTen);
            // Below wholeDenominator * scale, so it fits whenever that does.
            std::optional<UInt128> const scaledPart = mul_unsigned_or_none(split.remainder, *powerOfTen);
            if (!scaledWhole || !scaledPart)
                return std::unexpected { ArithmeticError::Overflow };
            UInt128Division const partSplit = u128_divmod(*scaledPart, wholeDenominator);
            wholePart = u128_add(*scaledWhole, partSplit.quotient);
            if (wholePart < *scaledWhole)
                return std::unexpected { ArithmeticError::Overflow };
            FORMULA_CENSUS_NOTE(Unsigned, wholePart);
            leftover = partSplit.remainder;
        }
        else
        {
            std::optional<UInt128> const shrink = unsigned_pow10(-doubledPlaces);
            std::optional<UInt128> const widened = shrink ? mul_unsigned_or_none(wholeDenominator, *shrink) : std::nullopt;
            if (!widened)
                return std::unexpected { ArithmeticError::Overflow };
            divisor = *widened;
            UInt128Division const split = u128_divmod(wholeNumerator, divisor);
            wholePart = split.quotient;
            leftover = split.remainder;
        }

        std::uint64_t const floorDigits = u128_isqrt(wholePart);
        // f^2 + f is below (f + 1)^2 <= 2^128, so it fits.
        UInt128 const halfwayWhole = u128_add(u128_mul_words(floorDigits, floorDigits), UInt128::from_u64(floorDigits));
        bool const aboveHalfway = halfwayWhole < wholePart
                                  || (wholePart == halfwayWhole && u128_divmod(divisor, UInt128::from_u64(4)).quotient < leftover);

        UInt128 keptDigits = UInt128::from_u64(floorDigits);
        UInt128 const raisedDigits = u128_add(keptDigits, UInt128::from_u64(1));
        switch (roundingMode)
        {
            case RoundingMode::Floor:
            case RoundingMode::TowardZero:
                break;
            case RoundingMode::Ceiling:
            case RoundingMode::AwayFromZero:
                keptDigits = raisedDigits;
                break;
            case RoundingMode::HalfAwayFromZero:
            case RoundingMode::HalfTowardZero:
            case RoundingMode::HalfEven:
                keptDigits = aboveHalfway ? raisedDigits : keptDigits;
                break;
        }

        // At most 2^64, which `Rational::Int` holds.
        return Rational::from_decimal(signed_from_magnitude(keptDigits, false), -places.value);
```

    - In the doc comment (`:166-174`), "Every intermediate is a `std::uint64_t`; there is no 128-bit integer, because cl has none" becomes "Every intermediate is a 128-bit unsigned integer (`detail::UInt128`)".
    - In the same comment, "2^64" becomes "2^128"; "an integer radicand of about 10^6 fits at 6 places and overflows at 7" becomes "fits at 16 places and overflows at 17"; and "1.8 * 10^(19 - 2|p|)" becomes "3.4 * 10^(38 - 2|p|)".

- [ ] **Step 5: The census measures 127 bits.**
    - `support/census_tally.cpp`: `signed_bits_used` returns `std::min(127, ...)`, with the comment "`Rational::Int`'s minimum, -2^127, uses every bit there is, and no more".
    - `census_tally.hpp`: "what is left of 127 is the headroom".
    - `support/census_report.cpp`: the line reads `... headroom {} of 127` and computes `127 - formula_census::signed_bits_used()`.
    - `cmake/CheckCensusPage.cmake:77`: `headroom ([0-9]+) of 127`.
    - In `test/overflow_census_tests.cpp`:
        1. `Used::headroom()` computes from 127 instead of 63.
        2. The instrument's control (`:670-704`) is rebuilt at 128 bits:

```cpp
TEST_CASE("the census reports 0 bits of headroom for the largest Int128, and Overflow one step further", "[census]")
{
    constexpr formula::Int128 largest = std::numeric_limits<formula::Int128>::max();
    // (2^126 - 1) + 2^126 = 2^127 - 1 from two 126- and 127-bit operands,
    // built outside the count: the sum's 127 bits are the addition's own
    // intermediate, which only add_checked_or_none's hook reports.
    Rational const lowHalf { (formula::Int128 { 1 } << 126) - 1 };
    Rational const highHalf { formula::Int128 { 1 } << 126 };
    Used const atTheLimit =
        census_of([&] { REQUIRE(formula::checked_add(lowHalf, highHalf).value() == Rational { largest }); });
    CHECK(atTheLimit.headroom() == 0);
    CHECK(atTheLimit.intermediateBits == 127);
    // 2^63 * 2^62 = 2^125, one bit short of using all 127: the product is
    // mul_checked_or_none's intermediate, from operands of 64 and 63 bits.
    Rational const factorA { formula::Int128 { 1 } << 63 };
    Rational const factorB { formula::Int128 { 1 } << 62 };
    Used const oneShort = census_of(
        [&] { REQUIRE(formula::checked_mul(factorA, factorB).value() == Rational { formula::Int128 { 1 } << 125 }); });
    CHECK(oneShort.headroom() == 1);
    CHECK(oneShort.intermediateBits == 126);
    // One step further is the library's Overflow, never a figure: a product
    // that overflows leaves the count with its operands' 65 bits at most.
    CHECK(formula::checked_add(Rational { largest }, rat(1)).error() == formula::ArithmeticError::Overflow);
    Rational const tooWideA { formula::Int128 { 1 } << 64 };
    Rational const tooWideB { formula::Int128 { 1 } << 63 };
    Used const overflowed = census_of(
        [&] { REQUIRE(formula::checked_mul(tooWideA, tooWideB).error() == formula::ArithmeticError::Overflow); });
    CHECK(overflowed.intermediateBits <= 65);
    CHECK(overflowed.numeratorBits <= 65);
    // A constant evaluation tells the census nothing.
    Used const constant = census_of([] {
        constexpr auto added = formula::checked_add(Rational { 1 << 20 }, Rational { 1 << 20 });
        static_assert(added.has_value());
    });
    CHECK(constant.headroom() == 127);
}
```

        3. The named realistic case (`:777-783`) now answers:

```cpp
    // The named realistic case: six masses at micrograms, which overflowed 64
    // bits in kg^2, now answer exactly.
    auto const named = formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 6>),
                                                               series_environment<Mass>(sixAtMicrograms));
    REQUIRE(named.has_value());
    REQUIRE(named->is_value());
    CHECK(named->measurement().value() == Rational { 2026588050217, 6000000000000 });
    auto const namedRejection =
        formula::checked_evaluate_rejection<Mass>(rejection_of<6>(sevenQuarters), series_environment<Mass>(sixAtMicrograms));
    CHECK(namedRejection.has_value());
```

        4. The regression pins (`:823-842`): re-measure on cl-debug and replace the three headroom figures, and the numerator and unsigned bit counts, with the new measurements, keeping the "less 4 bits" margin. The comment says they were measured "at the commit that stores `Rational` in 128 bits".
        5. The cylinder pins (`:855-934`): no diameter overflows now. Pin that list as empty, and pin the strength at 139 mm to exactly `Rational { 13976660729400, 2375042831981 }` MPa.
        6. The least-squares and regression pins (`:985`, `:1047`): replace them with the new measured sizes.
- [ ] **Step 6: The exact companion sizes against 128 bits.** In `tools/census/exact_sizes.py`:
    - The module comment says 128 bits where it says 64, and "a signed 128-bit integer's 127".
    - `fits` becomes:

```python
LIMIT = (1 << 127) - 1


def fits(value):
    """Whether an exact value is a Rational over formula::Int128: |numerator|
    and the denominator at most 2^127 - 1 (the numerator may also be -2^127)."""
    return abs(value.numerator) <= LIMIT and value.denominator <= LIMIT
```

    - `main` also tracks the widest SI value, and prints exactly these lines:

```
six masses near 40 g at 4 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 52 bits), in 0 of 1000 in g2 (declared; widest 32 bits)
six masses near 40 g at 5 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 59 bits), in 0 of 1000 in g2 (declared; widest 39 bits)
six masses near 40 g at 6 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 65 bits), in 0 of 1000 in g2 (declared; widest 45 bits)
4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in Pa (SI) the exact strength does not fit 128 bits at 0 of 63 (widest 64 bits)
4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in MPa (declared) it does not fit at 0 of 63 (widest 44 bits)
```

    - The self-check's output does not change.
    - `CMakeLists.txt:213`'s `PASS_REGULAR_EXPRESSION` becomes:
      `"at 6 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 \\(SI. widest 65 bits\\), in 0 of 1000 in g2 \\(declared. widest 45 bits\\)[\r\n]+[^\r\n]*in Pa \\(SI\\) the exact strength does not fit 128 bits at 0 of 63 \\(widest 64 bits\\)[\r\n]+[^\r\n]*in MPa \\(declared\\) it does not fit at 0 of 63 \\(widest 44 bits\\)"`.
    - If `docs/numeric-headroom.md`'s `census:exact` block is parsed by a regex in `cmake/CheckCensusPage.cmake`, update that regex to the new line shape.

- [ ] **Step 7: Make the suite build, then re-pin what moved.** Build with Verify step 1. The switch turns every remaining narrowing into a compile error. Fix each site the way Task 5 fixed its kind: `wide_magnitude`, `narrow_to_int64`, `rational_int_from_magnitude`, `to_int64()`. Never use a cast. This includes `test/package/main.cpp` and `tools/gallery/main.cpp`. Then classify every failing test:
    1. **A refusal that now answers** (`Overflow` at a 64-bit bound):
        - move the refusal to the 128-bit bound, so it is still tested;
        - pin the new answer exactly, with its value worked out by hand or in Python;
        - say so in the report.

       `test/rational_tests.cpp:314-347` is one: it names itself the test to flip.
    2. **A census figure** (`census.*`): Step 5 covers these.
    3. **A rounding or spelling limit stated in 64-bit terms**, e.g. `checked_round`'s `IntMax/3` case in `test/number_text_tests.cpp`: pin what the library now answers, if it now answers.

  **Any test whose answer changed from one number to another is a defect: stop and report it.**

- [ ] **Step 8: Keep the least-squares example refusing.** `examples/opaque_and_retry.cpp` fits fifteen points with a different denominator on each, to show a fit refusing with `Overflow`. `examples/CMakeLists.txt:280` pins it.
    - Find the smallest number of such points, built the same way, at which `LinearLeastSquares::compute` now overflows.
    - Change the example to that number.
    - Update its pass regex and `docs/opaque-and-retry.md`'s text block and prose (the count of points).

- [ ] **Step 9: Regenerate the census page and the gallery.**
    - Build the target `formula-cpp-census-page`, and check with `git diff docs/numeric-headroom.md`:
        - the resolution table reads `0 of 1000` overflowed everywhere;
        - the cylinder table lists no diameter;
        - every realistic row keeps at least 8 bits.
    - Regenerate the gallery and confirm with `git diff docs/gallery.md` that it is unchanged. A change there means a value's text moved: report it.

- [ ] **Step 10: Verify.**
    - Verify step 1 must print `ALL OK`. Delta: **+3 tests**, plus or minus the flipped and removed ones, each named in the report.
    - Verify step 2 with `-Filter "band_bound_out_of_range|breakpoint_key_out_of_range|band_|breakpoint_|rational_"`. Delta: **+2 negative**, minus any that Step 3 removed.

- [ ] **Step 11: Commit.**
  Message: `feat: store Rational's numerator and denominator in 128 bits`.
  The body says:
    - the 6 dp variance, the rejection by standard deviations and the cylinder strength now answer, with the census's new figures;
    - the `_r` literal, `from_decimal` and rounding keep their limits;
    - `rounded_sqrt` works in 128 bits;
    - narrowing into the 64-bit structural types is refused, not cut.

  Then the `Signed-off-by` line.

### Task 7: The documentation of a 128-bit `Rational`

**Files:**
- Modify: `docs/numeric-headroom.md`'s prose. Its tables were regenerated in Task 6.
- Modify: `docs/numbers.md:119`, `docs/calculations.md:987`, `docs/display.md:253`, `docs/statistics.md:357`, and the README wherever it states the integer width.
- Modify these doc comments:
    - `include/formula-cpp/number_text.hpp`: the `IntMax/3` example at `:394-402`, and `ExactDecimalScale`'s "the largest power of ten `Rational::Int` holds";
    - `include/formula-cpp/function.hpp:146-148`: "-18 to 18" becomes "-38 to 38";
    - `include/formula-cpp/rounded_transcendental.hpp:46` and any other comment that states a 64-bit range as `Rational`'s;
    - `include/formula-cpp/detail/checked_int.hpp`'s file comment.
- Modify: `CHANGELOG.md`.

- [ ] **Step 1: Find every stale statement.**
  Run: `Select-String -Path include\formula-cpp\*.hpp, include\formula-cpp\detail\*.hpp, docs\*.md, README.md -Pattern '64-bit|63 bits|2\^63|INT64|int64_t.*Rational|10\^18, the largest|IntMax'`.
  Each hit is either about `Rational`'s range, and is rewritten for 128 bits, or about something genuinely 64-bit (`Unit`'s fields, the `_r` literal's mantissa, `DecimalPlaces`' 18), and stays. List both kinds in the report.

- [ ] **Step 2: Rewrite the headroom page's prose** around its regenerated tables:
    - **"The answer":** enough for every realistic case measured, with the 6 dp variance, the rejection and the cylinder figures from the tables.
    - **Remove the paragraphs that left the choice open.**
    - **Replace "Whether 128-bit intermediate arithmetic is enough…"** with a short section on what was chosen and why:
        - 128-bit intermediates alone could not have helped: `checked_mul` already reduces before it multiplies, so an overflowing product is an overflowing result;
        - the values are stored in SI, where the variances and strengths needed 64 bits or more;
        - so the stored integer was widened to 128 bits, native where the compiler has a 128-bit integer and portable `constexpr` elsewhere.
    - **The rule for acting stays:** under 8 bits of headroom recommends wider arithmetic.
    - **"Headroom"** is `127` minus the bits used, and `rounded_sqrt`'s unsigned intermediates have 128.
    - **The stress-control table** reads 2^126 and 2^127.
    - **"Which cases decide" and "What this does not decide"** describe the new state.

  Do not cite the issue as open.

- [ ] **Step 3: CHANGELOG.** Under `## [Unreleased]`:

```markdown
### Added

- **`formula::Int128`** (`int128.hpp`), a signed 128-bit integer with one API on every compiler: the compiler's own
  128-bit integer computes where it has one (GCC, Clang), portable `constexpr` code everywhere else (cl, clang-cl).
  It converts to no built-in integer implicitly or explicitly; `to_int64()` and `to_uint64()` say when a value does
  not fit. `std::format` writes it in decimal.

### Changed

- **`Rational` stores its numerator and denominator in `formula::Int128`**, so `Rational::Int` is `Int128` and a
  `Rational` is 32 bytes. Realistic laboratory statistics that overflowed 64 bits now answer: the sample variance of
  masses read to 6 decimal places of a gram, rejection by standard deviations at that resolution, and a cylinder's
  strength at every diameter measured (`docs/numeric-headroom.md`). Code that stored `numerator()` or `denominator()`
  in a built-in integer must narrow with `to_int64()`. The `_r` literal, `Rational::from_decimal`'s exponents and
  rounding's decimal places keep their limits of 18.
- `rounded_sqrt` computes in 128 bits, and answers at more places before it reports `Overflow`.
- `NumberTextCapacity` is 128, so that a 39-digit numerator over a 39-digit denominator fits a `NumberText`.
- `band(Rational, Rational)` and `breakpoint(Rational)` refuse a bound or key that does not fit their 64-bit fields:
  in a constant expression it fails to compile, naming `formula_band_bound_out_of_range` or
  `formula_breakpoint_key_out_of_range`.
```

- [ ] **Step 4: Verify.** Verify step 1 must print `ALL OK`. Delta: **+0 tests**. Also build the Doxygen target: `cmake --build --preset cl-debug --target formula-cpp-docs-api`. It must report no warnings, or report the CI Doxygen run in Task 8 if the local Doxygen is missing.

- [ ] **Step 5: Commit.**
  Message: `docs: describe Rational's 128-bit range, and the headroom it measures`.
  Then the `Signed-off-by` line.

---

## Task 8: Merge the lanes, verify everything, and open the pull request

The controller runs this task, with `sdd-implementer` for fix rounds and `sdd-reviewer` for the whole-branch review.

- [ ] **Step 1: Merge Lane B into the feature branch.**
  In `D:\formula-cpp\.claude\worktrees\int128-and-trace-units`: `git merge --no-ff feature/int128-core`, then resolve:
    - `CHANGELOG.md`: keep both lanes' entries.
    - `docs/numeric-headroom.md`: take Lane B's, then regenerate it in Step 2.
    - Any test file both lanes touched: keep both changes.
    - `include/formula-cpp/trace.hpp` and `trace_render.hpp`: the lanes touched different functions.

- [ ] **Step 2: Regenerate and run the Windows gate.**
    - Build `formula-cpp-census-page`.
    - Regenerate the gallery.
    - Run Verify step 1, and Verify step 2 with `-Filter "."`, all negatives on both Windows compilers.
    - Commit any regeneration as `docs: regenerate the census page and the gallery for both changes`.

- [ ] **Step 3: The full matrix.**
    - `pwsh -NoProfile -File $S\windows-matrix.ps1 -Tree $T` must print `MATRIX OK`: cl-debug, cl-release, clangcl-debug, clangcl-release.
    - `wsl bash .../posix-matrix.sh --tree /mnt/d/formula-cpp/.claude/worktrees/int128-and-trace-units` must print `MATRIX OK`: gcc-release with g++-14, and clang-debug, clang-release and clang-ubsan.
    - `wsl bash .../docs-pages.sh --tree ...` must print `DOXYGEN OK`, and `mkdocs build --strict` must succeed.
    - Each failure goes to a fix round, `sdd-implementer` with the failing log, then this step again.

- [ ] **Step 4: Whole-branch review.**
  Run `sdd-reviewer` over `master..feature/int128-and-trace-units`, against both specs, this plan's Global Constraints and its Review Focus. Its findings go to fix rounds; re-review until it is clean.

- [ ] **Step 5: Pull request.**
    - Push `feature/int128-and-trace-units`.
    - Open the PR with `contour-workflows:create-pr`. The title is "A unit on every computed trace value, and a 128-bit Rational". The body ends with:

      ```
      Closes #1
      Closes #2
      ```

    - Drive CI to green, including macOS AppleClang and install-and-consume. Its `test/package/main.cpp` was adapted in Task 6.
    - **Ask the owner before merging.** Merge as a merge commit, which closes both issues.
