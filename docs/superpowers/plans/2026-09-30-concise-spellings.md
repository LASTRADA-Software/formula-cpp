# Concise Spellings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make formula-cpp less verbose to use by adding short spellings for what the examples repeat most, without taking any spelling away. Then rewrite every example with them and print through `std::print` / `std::println`.

**Architecture:** Every new spelling is an **addition** that forwards to what already exists, and every existing spelling keeps compiling. The additions:

- a `consteval` exact-decimal literal;
- wider input factories;
- `number_of()`, which returns "the number, or nothing";
- `describe()` and `std::formatter` for the remaining enums and types;
- `traced()`, plus an `explain_*` twin for every evaluation verb;
- `DecimalRounding`, which states a rounding once;
- throwing twins for the `Measured` operations;
- `yields<Q>(expr)`, which names the result quantity once, where the formula is declared.

The last task group rewrites all 19 examples, and the eight guides that are checked line-for-line against them.

**Tech Stack:** C++23, header-only. Catch2 3.6 via CPM, `STATIC_REQUIRE`, the `test/negative/` harness, the ctest docs checks (`cmake/CheckGuideSnippets.cmake`), and `<print>`.

**Spec:** the *Design* section below. Task 0 saves it verbatim as `docs/superpowers/specs/2026-09-30-concise-spellings-design.md` and this plan as `docs/superpowers/plans/2026-09-30-concise-spellings.md`.

**Order:** tasks run in number order. Each library task (1–9) is self-contained. Tasks 11–15 consume all of them. Line anchors were read at master `eb5eed8`. Find each place by the name quoted beside its anchor, never by the number alone.

---

## Design

### Why

The 19 programs in `examples/` repeat a few shapes, and those shapes make the library feel verbose:

| Shape | ≈ sites in examples | Evidence |
|---|---|---|
| `formula::Rational { 273, 10 }`, `*Rational::make(..)`, `*Rational::from_decimal(..)` | ~260 | **38 files define their own `rat()`**. Header comments call a `rat()` that does not exist (`expression.hpp:87,94`, `rejection.hpp:12,149,223-238`, `lookup.hpp`, `precision.hpp:837`, `series.hpp:193`). |
| `Measured<Q> { Rational { n } }`; `measured_series<Q>(Measured<Q>{..}, …)` repeating `Q` for every element | ~115, plus 20 series | `series.cpp` layers `m<Q>()` over `rat()`. |
| `r.has_value() && r->is_value() && r->measurement().value() == X` | ~60 | 7 local helpers (`valueOf`, `exact` ×2, `exact_text`, `fraction_string`, …) |
| A hand-built trace, `Trace<> t{}; RecordingSink<> s{t}; (void) verb(…, s);`, then evaluating again for the value | 8 helpers, 8 inline blocks | `explain*` exists only for a Node, a series, a retry and a worksheet. |
| printf noise: `%.*s`, `static_cast<long long>`, `.to_double()`, `? "yes" : "no"` | ~250 | Only `display.cpp` and `electricity_bill.cpp` use the existing `std::format` support. |
| An enum turned into text by hand | 3 switches | No `describe()` for `ConstraintOutcomeKind`, `RetryEnd`, … |
| `<unit::X, DecimalPlaces{N}, RoundingMode::M>` restated at every use | ~25 | A standard states its rounding once. |
| The result quantity named again at every call | ~100 | Mostly inside per-file helper lambdas. |

### Decisions (owner, 2026-09-30)

- All four addition groups are in scope: literals and inputs; reading and printing; a trace from every verb; stating a rule once.
- **Bound formulas are in scope.** `yields<Q>(expr)` names the result once. The author still names it; nothing is deduced from the expression.
- **`std::print` / `std::println` replace printf and iostream** throughout the examples, tools, support code and docs, not only in the examples.

### Rules nothing here may bend

- Exactness: no implicit `double`, and a literal is exact or refused.
- Absent is not zero.
- The author names the result quantity.
- No unit is guessed for a bare number.
- No macros.
- Every fallible operation keeps its `checked_` form.
- One mistake produces one message, which begins `formula: ` or names a `formula_` guard.

### Considered and not done

| Idea | Why not |
|---|---|
| Deduce the result from the expression | The result is the author's choice (`evaluate.hpp:469-472`). `yields` keeps it the author's choice. |
| A defaulted quantity tag via `decltype([]{})` | CONTRIBUTING invariant 6: the type differs in each translation unit, which was verified to fail at link time. |
| A macro to declare quantities | The design requires traceability without macros (`docs/superpowers/specs/2026-09-23-formula-cpp-design.md:194-201`). |
| Implicit `double` → `Rational` | Inexact. `_r` gives the short spelling exactly. |
| Deduce `constant<unit::X>`'s unit, or a lookup's key unit, from the other operand | A standard states 47.3 kN against a quantity declared in N, and a table in mm may key a quantity declared in m. |
| A default `render_trace` limit or format rounding mode | Both are deliberate (`trace_render.hpp:15`, `format.hpp:460`). |
| `var<Q> = 180` binding | It would be an assignment operator on a const object that assigns nothing. `Measured<Q> { 180 }` already compiles. |
| `yields` over a curve | `checked_evaluate_curve` names two results (domain and values). Out of scope. |
| `DecimalRounding` for `rounded_ln` / `log10` / `exp` | These take no unit, and `<DecimalPlaces{4}, Mode>` is already short. |
| Migrating the ~2600 `rat()` uses in `test/` | Out of scope; tests adopt `_r` as they are touched. |

---

## Global Constraints

These bind every task.

- **C++23, header-only**, no dependency beyond the standard library in `include/`.
- **Worktree.** All work happens in `D:\formula-cpp\.claude\worktrees\concise-spellings` on `feature/concise-spellings`, branched from master `eb5eed8`. Never touch `D:\formula-cpp` itself or another worktree. Only the controller writes outside the worktree, where it keeps local progress notes (see *Execution*).
- **Verification per task: the Windows compilers only, MSVC `cl` and `clang-cl`** (owner, 2026-09-30, for speed; this replaces the earlier MSVC + g++-14 rule). No WSL build runs during Tasks 1–15. The full suite (all eight presets, g++-14 and clang++ under WSL, Doxygen 1.9.8, `mkdocs build --strict`) runs once, in Task 16, and is driven to green there. Set `$S = C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\b0c0e78c-1b2d-4d4c-a792-0760adeb4a03\scratchpad` and `$T = D:\formula-cpp\.claude\worktrees\concise-spellings`.
  - **Verify(`<regex>`)**, the per-task gate, is two commands, and both must print `ALL OK`:
    1. `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."`: the full cl-debug build, then every test except the negative ones (unit, compile-time, hygiene, example, docs, census).
    2. `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "<regex>"`: the negative tests matching `<regex>`, on cl-debug **and** clangcl-debug. `EXPECT_COUNT` is checked only off MSVC (`test/CMakeLists.txt:202-209`), so clang-cl is what proves one message per mistake. Each negative case builds only its own target, so no full clang-cl build runs.

    `<regex>` names the task's new negatives **and** the existing ones in the same area, which each task lists. A task with no negatives runs command 1 only.
  - **Quick loop:** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "<ctest -R regex>"`.
  - **g++-only findings arrive late.** A `-Wshadow` hit, a `consteval` call evaluated behind a false `&&` (`opaque.hpp:588-592`), or a libstdc++ difference surfaces only in Task 16. Guard against the known ones while writing: the consumer-globals names below, and staged checks in their own `if constexpr`. Budget one fix round in Task 16.
  - Never redirect a build to `/dev/null`, because a `STATIC_REQUIRE` failure is a *build* error. Prove a `-Filter` selected something, from ctest's count, before trusting it.
  - **Counts are deltas.** Task 0 records the cl-debug baseline. Each task reports its total and the difference, which must equal its stated delta ("+N cases, +M negative").
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:**
  - an SPDX header on every file, and no `NOLINT`;
  - core public headers include no `<string>`, `<vector>`, `<format>`, `<iostream>` or `<print>` (`hygiene.headers`), so formatters live in `format.hpp` and explain twins in `trace.hpp`;
  - every public `static_assert` message begins `formula: ` and is stable;
  - never identify a type with `decltype([]{})`;
  - a new header goes into the install `FILE_SET` (`CMakeLists.txt:41-106`, `hygiene.installed-headers`) and into `test/consumer_globals_tests.cpp`'s includes (`hygiene.consumer-globals`).
- **Consumer globals.** `test/consumer_globals_tests.cpp:130-153` declares 258 `int` globals. No new parameter or local may reuse one: g++ `-Wshadow` and cl C4459 turn it into a consumer's build error. Names this plan is tempted by that are **on the list** include: `bound`, `bounds`, `expression`, `sink`, `outcome`, `outcomes`, `result`, `value`, `values`, `number`, `format`, `unit`, `kind`, `source`, `environment`, `element`, `elements`, `measurement`, `mode`, `text`, `label`, `name`, `digits`, `count`, `first`, `last`, `size`, `data`, `view`, `string`, `input`, `output`, `lower`, `upper`, `low`, `high`. Use `boundFormula`, `recordingSink`, `evaluated`, `spelling`, `shownIn`, and similar names instead.
  - Every new entry point is also exercised in `consumer_globals_tests.cpp`, with its result checked in `consumer_globals_run_tests.cpp`. The probe guards only what it instantiates.
- **Defect classes.** Before each task, read `D:\formula-cpp\.superpowers\sdd\2026-09-25-methods-and-overlays\defect-classes.md`. Each task's report says what was checked for each of its nine classes. The ones this plan hits most:
  - **2, one mistake → two messages:** gate every forwarding overload.
  - **4:** no `{}` default member initialiser on a member holding an expression, such as `Yields::expression`.
  - **5:** fixtures must tell right from wrong.
  - **7:** every negative case gets a deletion check.
- **Negative tests.** A negative test is `test/negative/<name>.cpp` plus `formula_add_negative_test(<name> "<expected>" …)` in `test/CMakeLists.txt`; ctest names it `negative.<name>`.
  - Register it with a deliberately wrong expected text, and watch it fail. Then register the right text and watch it pass.
  - **Deletion check:** delete the guard and confirm the case compiles; then restore the file **with a plain write**.
  - A guard reached from a `consteval` call is matched by its **name**, with no `EXPECT_COUNT`: clang and clang-cl report a failed consteval call twice (`test/CMakeLists.txt:303-307`).
- **Documentation is checked.** Eight guides are held line-for-line to their example by `docs.<guide>-output` and `docs.<guide>-snippets` (`examples/CMakeLists.txt:45-326`):
  - `dimensions`, `calculations`, `statistics`, `methods-and-overlays`, `series`, `records`, `opaque-and-retry`, `display`, plus `docs.readme-display-output`;
  - every ```` ```cpp ```` block must be consecutive source lines of the example, and every ```` ```text ```` block consecutive lines of its real output;
  - **so a checked guide changes only in the same task as its example** (Tasks 11–15). Library tasks document in Doxygen, in `CHANGELOG.md`, and in the *unchecked* guides named in each task;
  - a refusal quoted in `docs/` must be a header's message verbatim (`hygiene.documented-diagnostic-text`).
- **No internal labels in public text.** No task, phase, plan, lane or reviewer names in code, docs, commit messages or the PR. Every sentence must make sense to a reader who never saw this plan. Never cite the controller's local progress notes.
- **No third-party standard content.** Cite only `Example Standard N:YYYY` (`hygiene.no-real-standards`). Fixture values are plainly invented, and a size-like value is not a Renard R40 number (100, 106, 112, … 450, 475, 500, …). Primes such as 103, 127, 139, 163, 197 work.
- **Do not run clang-format** on existing files; match the surrounding style by hand. A Doxygen `///` comment goes on every new public entity and member.
- **Printing:** new or touched code prints with `std::print` / `std::println`. No new `printf`, `puts` or iostream anywhere. The one exception is `support/fail_without_dialogs.cpp`, whose CRT-failure handler must neither allocate nor throw.
- **CHANGELOG.md:** entries go under `## [Unreleased]` (`CHANGELOG.md:7`), in `### Added` / `### Changed` subsections that Task 1 creates, in the task that changes public behaviour.
- **Commits:** a conventional subject, a body that says why, and the last line exactly `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`. One commit per task. Every commit builds and passes on its own. Never `--no-verify`, never amend another task's commit.
- **Progress notes:** the controller keeps local progress notes (see *Execution*). Implementers report to the controller and never edit them.

## Review Focus

These are the five inputs most likely to bite a user that no task's happy-path tests reach. Each line's test is added to the task that owns the code.

1. **A `_r` spelling that is valid C++ but means something else**: `017_r` (octal to a C++ reader), `0x1F_r`, `1e19_r`, 19 fractional digits, `.5_r`, `5._r`, `1'000_r`. Each must be exact or refused at compile time by a named guard, never silently wrong. *Task 1.*
2. **A wrong `measured_series` element**: `Measured<OtherQ>`, `double`, a wide unsigned, `bool`. Each draws one library message, never an overload list. *Task 2.*
3. **ADL ambiguity with a consumer's same-named helper**: `describe(ConstraintOutcomeKind)` already exists locally in `constraints.cpp:78` and `methods_and_overlays.cpp:219`. Removing both is part of Task 4, and a CHANGELOG *Changed* entry records the rule, as 0.2.0 did (`CHANGELOG.md:285-290`). *Task 4.*
4. **A `Yields` misused**: evaluated for another quantity, holding a series evaluated as a single value, or wrapped around `documented()`. The first two draw one library message each; the third works. An explicit same-quantity result is allowed. *Task 9.*
5. **Printed output that changes by accident**: `%f` of `to_double()` printed `0.600000`, where `{}` of a `Rational` prints `0.6`, and a `Measured` adds its unit symbol. Every intended change updates the README or guide block in the same commit; everything else stays byte-identical. The gallery (`tools/gallery`) must match `docs/gallery.md` byte for byte. *Tasks 10–15.*

## File map

| File | Responsibility | Tasks |
|---|---|---|
| `include/formula-cpp/rational.hpp` | `formula::literals::operator""_r` and its guards | 1 |
| `include/formula-cpp/environment.hpp` | `not_measured`; the wider `measured_series` | 2 |
| `include/formula-cpp/band.hpp`, `lookup.hpp` | `band(Rational, Rational)`, `breakpoint(Rational)` | 2 |
| `include/formula-cpp/outcome.hpp` | `number_of`; `describe(ValueSource / OutcomeKind)` | 3, 4 |
| `include/formula-cpp/retry.hpp`, `rejection.hpp` | `number_of` for their outcomes; `describe(RetryEnd)` | 3, 4 |
| `include/formula-cpp/constraint.hpp`, `series.hpp` | `describe(ConstraintOutcomeKind / FailureSite)` | 4 |
| `include/formula-cpp/format.hpp` | formatters for `Outcome`, `Unit`, `Dimension` and the described enums | 5 |
| `include/formula-cpp/vocabulary.hpp`, `render.hpp` | `symbol_of<Q>()`; `render(x, RenderOptions)` | 5 |
| `include/formula-cpp/trace.hpp` | `Traced`, `traced`, the `explain_*` twins | 6, 9 |
| `include/formula-cpp/rounding.hpp` and the rounding factories | `DecimalRounding`, `SignificantRounding`, `declared_rounding`, overloads | 7 |
| `include/formula-cpp/measured.hpp` | `convert_to`, `round_to_declared`, `within_bounds`; dimension check at compile time | 8 |
| `include/formula-cpp/yields.hpp` (new), `evaluate.hpp` users | `Yields`, `yields`, overloads that deduce the result | 9 |
| `tools/gallery/main.cpp`, `support/census_report.cpp`, `test/overflow_census_tests.cpp`, `test/package/main.cpp` | `std::print` | 10 |
| `examples/*.cpp`, `docs/*.md`, `README.md` | the rewrite | 11–15 |

---

## Execution

- **The controller** (this session) owns the worktree, dispatch and review gates, and keeps local progress notes.
- **Progress notes.** The controller keeps local progress notes outside the repository, with one row per group of tasks (Setup 0; Literals and inputs 1–2; Reading and printing 3–5; Traces 6; Rules stated once 7–8; Bound formulas 9; Printing 10; Examples 11–15; Finish 16), and updates them at every state change, in the same turn: dispatched, reported, reviewed, fixed, landed, blocked.
- **Speed (owner, 2026-09-30: as fast as possible).**
  - Per-task gates run on Windows compilers only (*Global Constraints*).
  - A task's review may run while the next task's implementer starts, when the next task touches none of the reviewed task's files. `CHANGELOG.md`, `test/CMakeLists.txt` and the consumer-globals files are shared, so the implementer appends to them after the review's fixes land. A review fix is then its own commit on top, never an amend.
  - Build trees persist between tasks (never delete `out/build/*`), so each gate builds incrementally.
- **Mode (owner, 2026-09-30): subagent-driven** (`superpowers:subagent-driven-development`). One fresh implementer per task, then a fresh reviewer, then the next task, and one whole-branch review in Task 16. The tasks share interfaces (Tasks 11–15 consume 1–9), and a shipped mistake in a public spelling is expensive to take back.

---

### Task 0: Setup, baseline, progress notes, and the `<print>` probe

**Files:**
- Create: `docs/superpowers/specs/2026-09-30-concise-spellings-design.md` (the *Design* section, verbatim)
- Create: `docs/superpowers/plans/2026-09-30-concise-spellings.md` (this file, verbatim)
- Modify: `examples/simple.cpp` (probe only)

**Interfaces:** Produces the worktree, the scripts in `$S` and both baselines.

- [ ] **Step 1: Create the worktree.** Use `superpowers:using-git-worktrees`: `git -C D:\formula-cpp worktree add .claude/worktrees/concise-spellings -b feature/concise-spellings eb5eed8`.
- [ ] **Step 2: Copy the scripts.** Copy `cl.ps1`, `gcc14.sh`, `verify.ps1`, `windows-matrix.ps1`, `posix-matrix.sh` and `docs-pages.sh` from `C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\6031eb20-da56-4aa2-bb6a-de8c11d53bfc\scratchpad\` into `$S`.
  - In each, replace the old session id `6031eb20-da56-4aa2-bb6a-de8c11d53bfc` with `b0c0e78c-1b2d-4d4c-a792-0760adeb4a03`.
  - Replace the default tree `next-features` with `concise-spellings`.
  - `grep -n "6031eb20\|next-features" $S/*` must print nothing.

  Then write `$S\neg.ps1`, which runs the negative tests matching a filter on both Windows compilers without a full build:

```powershell
# Run the negative tests matching -Filter on cl-debug and clangcl-debug. Each negative case builds only its own
# target, so neither preset needs a full build; a preset is configured on first use.
# Usage: pwsh -NoProfile -File neg.ps1 [-Tree <worktree>] -Filter <regex over negative test names>
param(
    [string]$Tree = "D:\formula-cpp\.claude\worktrees\concise-spellings",
    [Parameter(Mandatory = $true)][string]$Filter
)
$ErrorActionPreference = "Stop"
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
Set-Location $Tree
foreach ($preset in @("cl-debug", "clangcl-debug")) {
    if (-not (Test-Path "out\build\$preset\build.ninja")) {
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { Write-Host "CONFIGURE FAILED ($preset)"; exit 10 }
    }
    ctest --preset $preset -j 12 -R "negative\.($Filter)"
    if ($LASTEXITCODE -ne 0) { Write-Host "TESTS FAILED ($preset)"; exit 12 }
}
Write-Host "ALL OK (negatives: $Filter)"
exit 0
```

  Prove it can fail. Run `neg.ps1 -Filter "rational_from_floating_point"` with that case's expected text temporarily wrong in the build tree's generated `negative/<name>.expect.cmake`. Expected: `TESTS FAILED`. Restore the text; expected: `ALL OK`.
- [ ] **Step 3: Baseline.** Run `pwsh -NoProfile -File $S\cl.ps1 -Tree $T` (the full cl-debug suite, negatives included) and `neg.ps1 -Filter ".*"` once for clang-cl's negatives. Expected: `ALL OK` from both. Record the cl-debug total.
- [ ] **Step 4: Progress notes.** Start the controller's local progress notes for this plan, as *Execution* says.
- [ ] **Step 5: Probe `<print>` on every CI leg.** Rewrite `examples/simple.cpp`'s output line to use `std::println`, and change nothing else:

```cpp
#include <print>
// …
    std::println("{} = {} ({})",
                 formula::Describe<Gradient>::symbol,
                 result.measurement().value().to_double(),
                 result.is_value() ? "computed" : "no value");
```

  Run `cl.ps1 -Tree $T -Filter "example\.simple"`. Expected: PASS. The draft PR's CI answers for the other compilers.
- [ ] **Step 6: Commit** the spec, the plan and the probe as one commit, `docs: plan concise spellings for the public API`, whose body says `simple.cpp` now prints with `std::println`.
- [ ] **Step 7: Push and open a draft PR** (the owner's standing rule is to open the PR early as a draft): `git push -u origin feature/concise-spellings`, then `gh pr create --draft` with a title and body that describe the goal, not the plan.
  - Watch the **macOS-15 AppleClang** leg.
  - **If it fails on `<print>`, stop and report to the owner** with the log. The options are a newer Xcode on the runner or Homebrew LLVM; this plan does not pick one.

### Task 1: The exact decimal literal `_r`

**Files:**
- Modify: `include/formula-cpp/rational.hpp`: guards beside `Pi` (`:516`), literal after it
- Create: `test/rational_literal_tests.cpp` (registered in `test/CMakeLists.txt:22-106`)
- Create: `test/negative/rational_literal_{out_of_range,too_many_places,not_a_decimal,octal}.cpp`
- Modify: `docs/numbers.md` (a section, *Writing an exact decimal*), `CHANGELOG.md`, `test/consumer_globals_tests.cpp`, `test/consumer_globals_run_tests.cpp`

**Interfaces:**
- Produces: `formula::literals::operator""_r(char const*) -> Rational`, which is `consteval`, in `inline namespace literals`, so both `using namespace formula::literals;` and `formula::operator""_r` work. Also the guards `detail::formula_rational_literal_out_of_range()` and `detail::formula_rational_literal_not_a_decimal()`.

- [ ] **Step 1: Write the failing tests** in `test/rational_literal_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace formula::literals;
using formula::Rational;

TEST_CASE("_r: an integer spelling is that integer", "[rational][literal]")
{
    STATIC_REQUIRE(457_r == Rational { 457 });
    STATIC_REQUIRE(0_r == Rational {});
    STATIC_REQUIRE(1'000'003_r == Rational { 1'000'003 });
    STATIC_REQUIRE(9'223'372'036'854'775'807_r == Rational { formula::detail::IntMax });
}

TEST_CASE("_r: a decimal spelling is its exact value, not the nearest double", "[rational][literal]")
{
    STATIC_REQUIRE(27.3_r == Rational { 273, 10 });
    STATIC_REQUIRE(0.47_r == Rational { 47, 100 }); // the double 0.47 is not 47/100
    STATIC_REQUIRE(0.0213_r == Rational { 213, 10'000 });
    STATIC_REQUIRE(.5_r == Rational { 1, 2 });
    STATIC_REQUIRE(5._r == Rational { 5 });
}

TEST_CASE("_r: trailing fractional zeros cost nothing", "[rational][literal]")
{
    STATIC_REQUIRE(4.210_r == Rational { 421, 100 });
    STATIC_REQUIRE(1.500000000000000000000000_r == Rational { 3, 2 }); // 24 places, most of them zeros
}

TEST_CASE("_r: an exponent scales exactly", "[rational][literal]")
{
    STATIC_REQUIRE(1.5e-3_r == Rational { 3, 2'000 });
    STATIC_REQUIRE(7e2_r == Rational { 700 });
    STATIC_REQUIRE(1.3E+2_r == Rational { 130 });
    STATIC_REQUIRE(1'3.7e1_r == Rational { 137 });
}

TEST_CASE("_r: a minus sign is Rational's own negation", "[rational][literal]")
{
    STATIC_REQUIRE(-27.3_r == Rational { -273, 10 });
}
```

- [ ] **Step 2: Run the tests; they fail.** `$S\cl.ps1 -Filter "literal"`. Expected: BUILD FAILED, `operator""_r` not found.
- [ ] **Step 3: Implement** in `rational.hpp`. Add `#include <cstdlib>` if it is missing.

```cpp
namespace detail
{
    /// A `_r` literal whose exact value `Rational` cannot hold: too many
    /// significant digits, or a denominator above `Int`'s range. Deliberately
    /// not `constexpr`, like `formula_exponent_out_of_range`
    /// (`dimension.hpp`): the literal operator is `consteval`, so reaching
    /// this fails to compile and the diagnostic names it.
    [[noreturn]] inline void formula_rational_literal_out_of_range()
    {
        std::abort();
    }

    /// A `_r` literal spelled as something other than a decimal: a
    /// hexadecimal or binary integer, or an integer with a leading zero,
    /// which C++ reads as octal. Same mechanism as above.
    [[noreturn]] inline void formula_rational_literal_not_a_decimal()
    {
        std::abort();
    }

    /// The exact value of a decimal literal's spelling: digits, an optional
    /// fraction, an optional exponent, and digit separators.
    consteval Rational rational_from_spelling(char const* spelling)
    {
        std::size_t at = 0;
        bool const hasPoint = [&] {
            for (std::size_t probe = 0; spelling[probe] != '\0'; ++probe)
                if (spelling[probe] == '.' || spelling[probe] == 'e' || spelling[probe] == 'E')
                    return true;
            return false;
        }();
        if (spelling[0] == '0' && spelling[1] != '\0' && !hasPoint)
            formula_rational_literal_not_a_decimal(); // 017, 0x1F, 0b101
        Int mantissa = 0;
        int scale = 0;              // decimal exponent the digits carry
        int pendingZeros = 0;       // fractional zeros not yet multiplied in
        bool inFraction = false;
        for (; spelling[at] != '\0' && spelling[at] != 'e' && spelling[at] != 'E'; ++at)
        {
            char const symbolAt = spelling[at];
            if (symbolAt == '\'')
                continue;
            if (symbolAt == '.')
            {
                inFraction = true;
                continue;
            }
            if (symbolAt < '0' || symbolAt > '9')
                formula_rational_literal_not_a_decimal();
            int const digitValue = symbolAt - '0';
            if (inFraction && digitValue == 0)
            {
                ++pendingZeros;
                continue;
            }
            for (int zero = 0; zero <= pendingZeros; ++zero) // the zeros, then this digit's place
            {
                if (mantissa > (IntMax - (zero == pendingZeros ? digitValue : 0)) / 10)
                    formula_rational_literal_out_of_range();
                mantissa = mantissa * 10 + (zero == pendingZeros ? digitValue : 0);
            }
            if (inFraction)
                scale -= pendingZeros + 1;
            pendingZeros = 0;
        }
        if (!inFraction)
            scale += pendingZeros; // unreachable for an integer spelling; kept for symmetry
        if (spelling[at] == 'e' || spelling[at] == 'E')
        {
            ++at;
            bool const negative = spelling[at] == '-';
            if (spelling[at] == '-' || spelling[at] == '+')
                ++at;
            int written = 0;
            for (; spelling[at] != '\0'; ++at)
            {
                if (spelling[at] == '\'')
                    continue;
                written = written * 10 + (spelling[at] - '0');
                if (written > 1'000)
                    formula_rational_literal_out_of_range();
            }
            scale += negative ? -written : written;
        }
        std::expected<Rational, ArithmeticError> const made = Rational::from_decimal(mantissa, scale);
        if (!made)
            formula_rational_literal_out_of_range();
        return *made;
    }
} // namespace detail

inline namespace literals
{
    /// An exact decimal: `27.3_r` is 273/10, never the `double` nearest it.
    /// An exponent scales exactly (`1.5e-3_r` is 3/2000); `-27.3_r` is
    /// `Rational`'s own negation. A spelling `Rational` cannot hold, or one
    /// that is not a decimal (`0x1F_r`, and `017_r`, which C++ reads as
    /// octal), fails to compile, naming `formula_rational_literal_out_of_range`
    /// or `formula_rational_literal_not_a_decimal`.
    consteval Rational operator""_r(char const* spelling)
    {
        return detail::rational_from_spelling(spelling);
    }
} // namespace literals
```

  The fixture `1'3.7e1_r` checks that separators are skipped in the mantissa. The trailing-zeros case checks that `pendingZeros` defers fraction zeros, so 24 places that are mostly zeros still fit.
- [ ] **Step 4: The negative cases.**
  - Each file includes `<formula-cpp/rational.hpp>`, declares `using namespace formula::literals;`, and holds one `constexpr formula::Rational refused = <literal>;` and `int main() {}`:
    - `rational_literal_out_of_range`: `9'223'372'036'854'775'808_r`
    - `rational_literal_too_many_places`: `0.0000000000000000001_r`
    - `rational_literal_not_a_decimal`: `0x1F_r`
    - `rational_literal_octal`: `017_r`
  - Register them after the exponent sentinels (`test/CMakeLists.txt:287-289`), with a comment naming the NAME-not-message mechanism and no `EXPECT_COUNT`:

```cmake
formula_add_negative_test(rational_literal_out_of_range "formula_rational_literal_out_of_range")
formula_add_negative_test(rational_literal_too_many_places "formula_rational_literal_out_of_range")
formula_add_negative_test(rational_literal_not_a_decimal "formula_rational_literal_not_a_decimal")
formula_add_negative_test(rational_literal_octal "formula_rational_literal_not_a_decimal")
```

  Follow the wrong-text-first protocol, then do the deletion check on each guard call.
- [ ] **Step 5: Probe, docs and CHANGELOG.**
  - Add `27.3_r` to the consumer-globals probe, and check its value in the run test.
  - Add a *Writing an exact decimal* section to `docs/numbers.md`. It says why a literal and not `double`, lists the refusals, and says `Rational { 1, 3 }` or `1_r / 3` remains the spelling for a fraction.
  - In `CHANGELOG.md`, create `### Added` under `## [Unreleased]` with the literal's entry.
- [ ] **Step 6: Verify(`rational_literal_.*|rational_from_.*|exponent_.*`).** Expected: `ALL OK`, +5 cases, and +4 negative tests passing on cl-debug and clangcl-debug.
- [ ] **Step 7: Commit** `feat(rational): add the exact decimal literal _r`.

### Task 2: Shorter inputs

**Files:**
- Modify: `include/formula-cpp/environment.hpp:167-176` (`measured_series`), `band.hpp:103-111`, `lookup.hpp:1318-1327`
- Modify: every header doc comment that calls the nonexistent `rat(`: `grep -n "rat(" include/` (13 lines), rewritten with `_r` or `Rational { n, d }`
- Test: `test/environment_tests.cpp`, `band_tests.cpp`, `lookup_tests.cpp`, `measured_tests.cpp`
- Create: `test/negative/measured_series_element_{other_quantity,double,wide_unsigned}.cpp`
- Modify: `docs/quantities.md` (*Supplying values*), `CHANGELOG.md`, the consumer-globals probe and run test

**Interfaces:**
- Consumes: `_r` (Task 1).
- Produces:
  - `formula::NotMeasured` and `inline constexpr NotMeasured not_measured {}`;
  - `measured_series<Q>(elements...)`, where each element is a `Measured<Q>`, anything convertible to `Rational`, or `not_measured`;
  - `band(Rational low, Rational high) -> Band`;
  - `breakpoint(Rational key) -> Breakpoint`.

- [ ] **Step 1: Write the failing tests.**

```cpp
// environment_tests.cpp
TEST_CASE("measured_series: plain numbers and not_measured stand for elements", "[environment][series]")
{
    using namespace formula::literals;
    constexpr auto mixed = formula::measured_series<Retained>(127, 10.3_r, formula::not_measured, formula::Rational { 1, 3 });
    STATIC_REQUIRE(mixed.size() == 4);
    STATIC_REQUIRE(mixed[0] == formula::Measured<Retained> { 127 });
    STATIC_REQUIRE(mixed[1] == formula::Measured<Retained> { formula::Rational { 103, 10 } });
    STATIC_REQUIRE(mixed[2].is_absent());
    STATIC_REQUIRE(mixed[3] == formula::Measured<Retained> { formula::Rational { 1, 3 } });
    // The old spelling is unchanged.
    constexpr auto spelled = formula::measured_series<Retained>(formula::Measured<Retained> { 127 }, formula::Measured<Retained>::absent());
    STATIC_REQUIRE(spelled[0] == mixed[0]);
    STATIC_REQUIRE(spelled[1].is_absent());
}

// measured_tests.cpp -- pins a spelling the examples adopt
TEST_CASE("Measured: an integer is a present value without spelling Rational", "[measured]")
{
    STATIC_REQUIRE(formula::Measured<SpecimenMass> { 139 }.value() == formula::Rational { 139 });
}

// band_tests.cpp
TEST_CASE("band: bounds given as exact numbers", "[band]")
{
    using namespace formula::literals;
    STATIC_REQUIRE(formula::band(83.7_r, 97.3_r) == formula::band(837, 10, 973, 10));
    STATIC_REQUIRE(formula::band(0, 127) == formula::band(0, 1, 127, 1));
}

// lookup_tests.cpp
TEST_CASE("breakpoint: a key given as an exact number", "[lookup]")
{
    using namespace formula::literals;
    STATIC_REQUIRE(formula::breakpoint(12.7_r) == formula::breakpoint(127, 10));
    STATIC_REQUIRE(formula::breakpoint(127) == formula::Breakpoint { 127, 1 }); // the integer overload still wins
}
```

  Use each file's existing fixture quantity (`Retained`, `SpecimenMass`); if a file has none, declare one with a prime-valued fixture.
- [ ] **Step 2: Run the tests; they fail.** `cl.ps1 -Filter "measured_series|band|breakpoint|Measured"`. Expected: BUILD FAILED.
- [ ] **Step 3: Implement.**

```cpp
// environment.hpp, replacing measured_series (:167-176)
/// An element of `measured_series` that was not measured:
/// `measured_series<Retained>(127, formula::not_measured, 139)`.
struct NotMeasured
{
    /// Every `NotMeasured` is the same.
    [[nodiscard]] constexpr bool operator==(NotMeasured const&) const noexcept = default;
};

/// The spelling of an absent element -- see `NotMeasured`.
inline constexpr NotMeasured not_measured {};

namespace detail
{
    /// Fails to compile when an element of `measured_series<Q>` is a value of
    /// another quantity. A `double` is refused by `Rational` itself, in its
    /// own words, and never reaches this.
    template <Described Q, typename Given>
    struct RequireSeriesElementOf
    {
        static constexpr bool isMeasuredOfAnother = !std::is_same_v<Given, Measured<Q>> && requires { typename Given::quantity_tag_probe; };
        static_assert(std::is_same_v<Given, Measured<Q>> || std::is_same_v<Given, NotMeasured> || std::is_constructible_v<Rational, Given>,
                      "formula: an element of measured_series<Q> is a Measured<Q> of that one quantity, an exact number, or "
                      "formula::not_measured; the quantity and the element's type appear in this diagnostic as the template "
                      "arguments of RequireSeriesElementOf");
        static constexpr bool value = true;
    };

    /// @p given as the series element it stands for.
    template <Described Q, typename Given>
    [[nodiscard]] constexpr Measured<Q> series_element(Given given) noexcept
    {
        if constexpr (std::is_same_v<Given, Measured<Q>>)
            return given;
        else if constexpr (std::is_same_v<Given, NotMeasured>)
            return Measured<Q>::absent();
        else
            return Measured<Q> { Rational { given } };
    }
} // namespace detail

/// Builds a series from its elements, in order, and counts them. Each element
/// is a `Measured<Q>`, an exact number (`127`, `10.3_r`, a `Rational`), or
/// `not_measured`: `measured_series<Retained>(127, 10.3_r, formula::not_measured)`.
/// `Q` is stated rather than deduced, so a `Measured` of another quantity is refused.
template <Described Q, typename... Given>
[[nodiscard]] constexpr auto measured_series(Given... given) noexcept
{
    static_assert((detail::RequireSeriesElementOf<Q, Given>::value && ...));
    return MeasuredSeries<Q, sizeof...(Given)> { std::array<Measured<Q>, sizeof...(Given)> { detail::series_element<Q>(given)... } };
}
```

  - Drop the `isMeasuredOfAnother` probe line if unused. The only point of the check is one message per wrong element.
  - A `double` element must draw only `Rational`'s message ("formula: a floating-point value is not an exact rational"). `std::is_constructible_v<Rational, double>` is true, because the refusing constructor exists, so `RequireSeriesElementOf` stays silent and `Rational { given }` fires.
  - A wide unsigned element likewise draws only `Rational`'s own unsigned message.

```cpp
// band.hpp, after band(n, d, n, d)
/// Builds a `Band` from its low (inclusive) and high (exclusive) bound as
/// exact numbers: `band(83.7_r, 97.3_r)`, `band(0, 127)`.
[[nodiscard]] constexpr Band band(Rational lowBound, Rational highBound) noexcept
{
    return { lowBound.numerator(), lowBound.denominator(), highBound.numerator(), highBound.denominator() };
}

// lookup.hpp, after breakpoint(n, d)
/// Builds a `Breakpoint` from its key as an exact number: `breakpoint(12.7_r)`.
/// An integer still takes the overload above, so `breakpoint(127)` is unchanged.
[[nodiscard]] constexpr Breakpoint breakpoint(Rational keyValue) noexcept
{
    return { keyValue.numerator(), keyValue.denominator() };
}
```

- [ ] **Step 4: Negative cases.**

```cmake
formula_add_negative_test(measured_series_element_other_quantity
    "formula: an element of measured_series<Q> is a Measured<Q> of that one quantity" EXPECT_COUNT 1
    REJECT "no matching" "cannot convert" "could not convert")
formula_add_negative_test(measured_series_element_double
    "formula: a floating-point value is not an exact rational" EXPECT_COUNT 1
    REJECT "an element of measured_series<Q>")
formula_add_negative_test(measured_series_element_wide_unsigned
    "formula: this unsigned type can hold values above Rational's maximum" EXPECT_COUNT 1
    REJECT "an element of measured_series<Q>")
```

  The existing `measured_series_short` and `measured_series_padded_*` negatives must still pass unchanged.
- [ ] **Step 5: Header comments, docs, probe, CHANGELOG.**
  - Replace each `rat(` in `include/` comments.
  - `docs/quantities.md` gains *Supplying values*: `Measured<Q> { 139 }`, `Measured<Q> { 10.3_r }`, and `measured_series` with plain numbers and `not_measured`.
  - Add the probe calls and the *Added* entries.
- [ ] **Step 6: Verify(`measured_series_.*|entered_series_.*|environment_series_.*|rational_from_.*`).** Expected: `ALL OK`, +4 cases, +3 negatives, and every existing negative in that set still passing.
- [ ] **Step 7: Commit** `feat(environment): accept plain numbers and not_measured in measured_series`.

### Task 3: `number_of`, the number or nothing

**Files:**
- Modify: `include/formula-cpp/outcome.hpp` (after `Outcome`, `:195`), `retry.hpp` (after `RetryOutcome`), `rejection.hpp` (after `RejectionOutcome`)
- Test: `test/outcome_tests.cpp`, `retry_tests.cpp`, `rejection_tests.cpp`, `method_tests.cpp` (an `Evaluated`)
- Modify: `docs/expressions.md` (*Reading a result*), `CHANGELOG.md`, the probe

**Interfaces:**
- Produces `formula::number_of(x) -> std::optional<Rational>`, with overloads for:
  - `Measured<Q> const&`
  - `Outcome<Q> const&` (engaged only when `is_value()`)
  - `std::optional<Rational> const&`
  - `std::expected<T, E> const&` for any `T` with a `number_of`
  - `RetryOutcome<R> const&` and `RejectionOutcome<Q, N> const&`, through their `outcome()`

  Because `optional == Rational` is false when the optional is empty, `number_of(x) == 0.5_r` is a complete check.

- [ ] **Step 1: Write the failing tests.**

```cpp
// outcome_tests.cpp
TEST_CASE("number_of: the number of a value, nothing for every other kind", "[outcome]")
{
    using Outcome = formula::Outcome<Strength>;
    STATIC_REQUIRE(formula::number_of(Outcome::value(formula::Measured<Strength> { 139 }, formula::ValueSource::Derived)) == formula::Rational { 139 });
    STATIC_REQUIRE(formula::number_of(Outcome::value(formula::Measured<Strength> { 139 }, formula::ValueSource::ManuallyEntered)) == formula::Rational { 139 });
    STATIC_REQUIRE(!formula::number_of(Outcome::empty()).has_value());
    STATIC_REQUIRE(!formula::number_of(Outcome::verdict({ "repeat the test" })).has_value());
    STATIC_REQUIRE(!formula::number_of(Outcome::invalid({ "discarded" })).has_value());
    STATIC_REQUIRE(formula::number_of(formula::Measured<Strength> { 139 }) == formula::Rational { 139 });
    STATIC_REQUIRE(!formula::number_of(formula::Measured<Strength>::absent()).has_value());
}

TEST_CASE("number_of: an error is nothing, a success is its number", "[outcome]")
{
    using Checked = std::expected<formula::Outcome<Strength>, formula::ArithmeticError>;
    constexpr Checked succeeded = formula::Outcome<Strength>::value(formula::Measured<Strength> { 163 }, formula::ValueSource::Derived);
    constexpr Checked failed = std::unexpected { formula::ArithmeticError::Overflow };
    STATIC_REQUIRE(formula::number_of(succeeded) == formula::Rational { 163 });
    STATIC_REQUIRE(!formula::number_of(failed).has_value());
    // Evaluated<Rational> is an expected of an optional.
    constexpr formula::Evaluated<formula::Rational> evaluated = std::optional<formula::Rational> { formula::Rational { 197 } };
    STATIC_REQUIRE(formula::number_of(evaluated) == formula::Rational { 197 });
}
```

  Add one test each in `retry_tests.cpp` and `rejection_tests.cpp`, using an existing fixture's accepted outcome, `formula::number_of(*accepted) == <its value>`. **Defect class 5:** choose a fixture whose value differs from the empty and verdict cases.
- [ ] **Step 2: Run the tests; they fail.** Expected: BUILD FAILED, `number_of` undeclared.
- [ ] **Step 3: Implement.**

```cpp
// outcome.hpp
/// The number @p measured holds, or nothing when it is absent.
template <Described Q>
[[nodiscard]] constexpr std::optional<Rational> number_of(Measured<Q> const& measured) noexcept
{
    return measured.stored();
}

/// The number @p produced holds when it is a value -- derived, measured or
/// entered -- and nothing for an empty, a verdict or an invalid outcome,
/// which `kind()` tells apart.
template <Described Q>
[[nodiscard]] constexpr std::optional<Rational> number_of(Outcome<Q> const& produced) noexcept
{
    if (!produced.is_value())
        return std::nullopt;
    Measured<Q> const held = produced.measurement();
    return held.stored();
}

/// @p held itself: what `Evaluated<Rational>` holds on success.
[[nodiscard]] constexpr std::optional<Rational> number_of(std::optional<Rational> const& held) noexcept
{
    return held;
}

/// The number a successful @p checked holds, or nothing on failure -- so
/// `formula::number_of(checked_evaluate<Q>(...)) == 0.5_r` is a complete check.
template <typename T, typename E>
    requires requires(T const& succeeded) { number_of(succeeded); }
[[nodiscard]] constexpr std::optional<Rational> number_of(std::expected<T, E> const& checked) noexcept
{
    if (!checked.has_value())
        return std::nullopt;
    return number_of(*checked);
}

// retry.hpp / rejection.hpp: one each, through outcome()
/// The number the outcome a retry ended with holds -- see `number_of(Outcome)`.
template <Described R>
[[nodiscard]] constexpr std::optional<Rational> number_of(RetryOutcome<R> const& ended) noexcept
{
    return number_of(ended.outcome());
}
```

  Add `#include <optional>` where it is missing.
- [ ] **Step 4: Docs, probe, CHANGELOG.** `docs/expressions.md` gets *Reading a result*: `number_of`, and why it is an `optional` and not a zero. Add the probe entries and the *Added* entry.
- [ ] **Step 5: Verify** (command 1 only; no negatives). Expected: `ALL OK`, +4 cases.
- [ ] **Step 6: Commit** `feat(outcome): add number_of, the number a result holds or nothing`.

### Task 4: `describe()` for the remaining enums

**Files:**
- Modify: `outcome.hpp` (`ValueSource`, `OutcomeKind`), `constraint.hpp:48` (`ConstraintOutcomeKind`), `retry.hpp:89` (`RetryEnd`), `series.hpp:738` (`FailureSite`)
- Modify: `examples/constraints.cpp:78-92` and `examples/methods_and_overlays.cpp:219-233` (delete the local `describe`, which would now be ambiguous by ADL)
- Test: `outcome_tests.cpp`, `constraint_tests.cpp`, `retry_tests.cpp`, `series_tests.cpp`
- Modify: `CHANGELOG.md` (*Added*, and *Changed* for the ADL rule)

**Interfaces:** Produces `describe(E) -> std::string_view` for those five enums. Each returns a lowercase phrase with no trailing punctuation, as `describe(ArithmeticError)` does (`error.hpp:45-64`). The words:

| Enum | Words |
|---|---|
| `ConstraintOutcomeKind` | `satisfied`, `violated`, `not checked`, `invalid`. These are exactly the examples' words, so their output is unchanged. |
| `ValueSource` | `derived`, `measured`, `manually entered` |
| `OutcomeKind` | `value`, `empty`, `verdict`, `invalid` |
| `RetryEnd` | `accepted`, `exhausted`, `not judgeable`, `not recorded`, `failed`, `manually entered` |
| `FailureSite` | one lowercase phrase per enumerator, read from its Doxygen comment |

- [ ] **Step 1: Write the failing tests:** one `STATIC_REQUIRE(formula::describe(E::X) == "…")` per enumerator of each enum.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement** each `describe` as a `switch` over every enumerator, beside its enum, in `describe(ArithmeticError)`'s shape, ending in a fallback like `return "unknown constraint outcome";`.
- [ ] **Step 4: Remove the two example helpers.**
  - Delete `describe(formula::ConstraintOutcomeKind)` from `constraints.cpp` and `methods_and_overlays.cpp`. Their calls now resolve to the library's.
  - Run `grep -rn "describe(formula::\(ValueSource\|OutcomeKind\|RetryEnd\|FailureSite\|ConstraintOutcomeKind\)" test examples tools docs`. Every hit is a now-ambiguous helper: delete it, or call `::describe`.
  - `ctest -R "example\.(constraints|methods_and_overlays)|docs\.methods"` must pass with **unchanged output**.
- [ ] **Step 5: CHANGELOG.** Add an *Added* entry. Add a *Changed* entry modelled on `CHANGELOG.md:285-290`: an unqualified call of `describe` with one of these enums now finds the library's by ADL, and a consumer's own `describe` for the same enum has to be renamed or called as `::describe`.
- [ ] **Step 6: Verify** (command 1 only). Expected: `ALL OK`, +5 cases.
- [ ] **Step 7: Commit** `feat: describe constraint outcomes, retry ends, value sources, outcome kinds and failure sites`.

### Task 5: Formatters, `symbol_of<Q>()`, and `render` without a placeholder vocabulary

**Files:**
- Modify: `include/formula-cpp/format.hpp`:
  - factor `formatter<Measured<Q>>`'s parse and write (`:595-640`) into `detail::parse_measured_format_field<Q>` and `detail::format_measured<Q>`;
  - add the new specialisations after it.
- Modify: `vocabulary.hpp:426-430` (`symbol_of`), `render.hpp` (beside `:2770`)
- Test: `test/format_tests.cpp`, `vocabulary_tests.cpp`, `render_tests.cpp`
- Modify: `CHANGELOG.md` and the probe. `docs/display.md` is checked, so its section lands with `display.cpp` in Task 12.

**Interfaces:**
- Consumes the `describe` overloads from Task 4.
- Produces:
  - `std::formatter<Outcome<Q>>`, with `Measured<Q>`'s grammar. A value is written as the `Measured`. An empty outcome is `(not measured)`. A verdict or invalid outcome is its label, padded by the spec's fill, alignment and width; places and mode do not apply to it.
  - `std::formatter<Unit>`: its symbol.
  - `std::formatter<Dimension>`: exponents joined by spaces (`L^2 M^-3`, `L^(1/2)`), named bases by name, and `(dimensionless)` for a pure number. This is exactly `dimensions_and_units.cpp:24-58`'s spelling, without the leading space.
  - `std::formatter<E>` for each enum in `detail::formats_by_describe`: `ArithmeticError`, `RoundingMode`, `BoundsCheck`, `SnapTie`, `Monotone`, `CumulativeDirection`, `ValueSource`, `OutcomeKind`, `ConstraintOutcomeKind`, `RetryEnd`, `FailureSite`. It writes `describe(e)` with `formatter<string_view>`'s spec.
  - `symbol_of<Q>(vocabulary = DefaultVocabulary {})`.
  - `render(x, RenderOptions)` and `render<D>(x, RenderOptions)`.

- [ ] **Step 1: Write the failing tests.**

```cpp
// format_tests.cpp (Kilojoule-quantity fixture as in the existing Measured tests)
TEST_CASE("format: an outcome writes its value, or says why it has none", "[format]")
{
    using O = formula::Outcome<Energy>;
    CHECK(std::format("{}", O::value(formula::Measured<Energy> { formula::Rational { 26, 5 } }, formula::ValueSource::Derived)) == "5.2 kJ");
    CHECK(std::format("{:.3HalfEven}", O::value(formula::Measured<Energy> { formula::Rational { 26, 5 } }, formula::ValueSource::Derived)) == "5.200 kJ");
    CHECK(std::format("{}", O::empty()) == "(not measured)");
    CHECK(std::format("{:>18}", O::verdict({ "repeat the test" })) == "   repeat the test");
    CHECK(std::format("{:.2HalfEven}", O::invalid({ "discarded" })) == "discarded");
}

TEST_CASE("format: a unit is its symbol, a dimension its exponents", "[format]")
{
    CHECK(std::format("{}", formula::unit::Kilojoule) == "kJ");
    CHECK(std::format("{}", formula::dim::Mass / formula::dim::Volume) == "L^-3 M^1");
    CHECK(std::format("{}", formula::nth_root(formula::dim::Length, 2)) == "L^(1/2)");
    CHECK(std::format("{}", formula::dim::Scalar) == "(dimensionless)");
}

TEST_CASE("format: a described enumeration is its words, aligned like a string", "[format]")
{
    CHECK(std::format("{}", formula::ArithmeticError::Overflow) == "overflow in exact arithmetic");
    CHECK(std::format("[{:<12}]", formula::ConstraintOutcomeKind::Violated) == "[violated    ]");
    CHECK(std::format("{}", formula::ValueSource::ManuallyEntered) == "manually entered");
}

// vocabulary_tests.cpp
STATIC_REQUIRE(formula::symbol_of<Rise>() == formula::symbol_of<Rise>(formula::DefaultVocabulary {}));

// render_tests.cpp
CHECK(formula::render(ratio, formula::RenderOptions { .numbers = formula::NumberStyle::exact_decimal() })
      == formula::render(ratio, formula::DefaultVocabulary {}, { .numbers = formula::NumberStyle::exact_decimal() }));
```

  Before asserting the Dimension output, read it from `ctest -R example.dimensions_and_units -V`. The expected strings above follow the example's order (L, M, T, I, Theta, N, J, then named bases), and the test must use the example's real output.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement.**

```cpp
namespace formula::detail
{
/// The formula enumerations `std::format` writes through their `describe()`:
/// one row per enumeration, so a new one is a new row.
template <typename E>
inline constexpr bool formats_by_describe = false;
template <>
inline constexpr bool formats_by_describe<ArithmeticError> = true;
// … one line each for RoundingMode, BoundsCheck, SnapTie, Monotone, CumulativeDirection,
//   ValueSource, OutcomeKind, ConstraintOutcomeKind, RetryEnd, FailureSite

/// A dimension's exponents, as `std::format` writes them -- see `formatter<Dimension>`.
[[nodiscard]] inline std::string dimension_text(Dimension const& shown);
} // namespace formula::detail

namespace std
{
/// `std::format` of a `formula::Outcome<Q>`: a value as `Measured<Q>` writes
/// it, in the same grammar; an empty outcome as `(not measured)`; a verdict or
/// an invalid outcome as its label, padded by the spec's fill, alignment and
/// width -- a rounding in the spec does not apply to words.
template <formula::Described Q>
struct formatter<formula::Outcome<Q>, char>
{
    constexpr auto parse(std::format_parse_context& parseContext)
    {
        return formula::detail::parse_measured_format_field<Q>(parseContext, _spec);
    }

    template <typename FormatContext>
    auto format(formula::Outcome<Q> const& shown, FormatContext& formatContext) const
    {
        if (shown.is_verdict())
            return formula::detail::write_formatted_number(shown.verdict_label(), std::string_view {}, _spec, formatContext.out());
        if (shown.is_invalid())
            return formula::detail::write_formatted_number(shown.reason_label(), std::string_view {}, _spec, formatContext.out());
        return formula::detail::format_measured<Q>(shown.measurement(), _spec, formatContext.out());
    }

  private:
    formula::detail::NumberFormatSpec _spec {};
};

/// `std::format` of a `formula::Unit`: its symbol, as a string is written.
template <>
struct formatter<formula::Unit, char>: formatter<string_view, char>
{
    template <typename FormatContext>
    auto format(formula::Unit const& shownIn, FormatContext& formatContext) const
    {
        return formatter<string_view, char>::format(formula::view(shownIn.symbolText), formatContext);
    }
};

/// `std::format` of a `formula::Dimension`: `L^2 M^-3`, `L^(1/2)`, named
/// bases by name, `(dimensionless)` for a pure number.
template <>
struct formatter<formula::Dimension, char>: formatter<string_view, char>
{
    template <typename FormatContext>
    auto format(formula::Dimension const& shown, FormatContext& formatContext) const
    {
        std::string const spelled = formula::detail::dimension_text(shown);
        return formatter<string_view, char>::format(spelled, formatContext);
    }
};

/// `std::format` of a formula enumeration: its `describe()` words, aligned
/// and padded as a string is.
template <typename E>
    requires formula::detail::formats_by_describe<E>
struct formatter<E, char>: formatter<string_view, char>
{
    template <typename FormatContext>
    auto format(E shown, FormatContext& formatContext) const
    {
        return formatter<string_view, char>::format(describe(shown), formatContext);
    }
};
} // namespace std
```

  - `formatter<Measured<Q>>` keeps its behaviour exactly; only its body moves into the two `detail` helpers. All existing `format_tests.cpp` cases must pass unchanged.
  - `symbol_of`: `template <Described Q, Vocabulary V = DefaultVocabulary> constexpr std::string_view symbol_of(V const& vocabulary = V {})`.
  - `render`: two overloads beside `render.hpp:2770`, each forwarding to `render<D>(node, DefaultVocabulary {}, renderOptions)`. They are not ambiguous: `Vocabulary` is a closed concept that `RenderOptions` does not satisfy, and a braced list cannot deduce `V`. If `document` has a `(x, V, RenderOptions)` form, give it the same twin.
- [ ] **Step 4: Probe, CHANGELOG.** Add a probe line formatting an `Outcome`, a `Unit`, a `Dimension` and an enum, and add the *Added* entries.
- [ ] **Step 5: Verify(`.*format.*`).** Expected: `ALL OK`, +5 cases, and no existing format test changed. The regex covers the existing format-spec negatives, since the parse was refactored.
- [ ] **Step 6: Commit** `feat(format): format outcomes, units, dimensions and enumerations; default symbol_of's vocabulary`.

### Task 6: A trace from every evaluation verb

**Files:**
- Modify: `include/formula-cpp/trace.hpp`: `Traced` and `traced` before `Explained` (`:4327`); twins after `explain_retry` (`:4507`); re-express `explain_series` (`:4410-4419`) and `explain_retry` (`:4489-4507`) through `traced`
- Test: `test/trace_tests.cpp` (`traced`), `method_tests.cpp`, `curve_tests.cpp`, `rejection_tests.cpp`, `constraint_tests.cpp`, `conformity_tests.cpp` (one twin each, beside that verb's fixtures)
- Modify: `docs/tracing.md` (*Tracing any evaluation*), `CHANGELOG.md`, the probe

**Interfaces:**
- Produces:
  - `template <typename R> struct Traced { R outcome; Trace<Rational> trace {}; };`
  - `traced(evaluation, vocabulary = DefaultVocabulary {}) -> Traced<R>`, where `evaluation` is callable with a `RecordingSink<Rational, V>`;
  - twins, each exactly the verb's result plus its trace:
    - `explain_method<Tag>(m, env, V = {}) -> Traced<Evaluated<Rational>>`
    - `explain_check_method(m, env, V = {})`
    - `explain_curve<DomainQ, ValueQ>(curve, env, V = {})`
    - `explain_rejection<Q>(rejection, env, V = {})`
    - `explain_check(constraint, env, V = {})`
    - `explain_check_all(set, env, V = {})`
    - `explain_conformity(conformity, env, V = {})`

- [ ] **Step 1: Write the failing tests.** For each twin, in the verb's own test file, write the old hand-built form and the twin, and assert both parts agree:

```cpp
TEST_CASE("explain_method: the method's value and the trace a RecordingSink records", "[method][trace]")
{
    formula::Trace<> handBuilt {};
    formula::Evaluated<formula::Rational> const direct =
        formula::evaluate_method<Cube>(compressiveStrength, specimen, formula::RecordingSink<> { handBuilt });
    auto const explained = formula::explain_method<Cube>(compressiveStrength, specimen);
    CHECK(explained.outcome == direct);
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 100 }) == formula::render_trace(handBuilt, { .maxSteps = 100 }));
    CHECK(!explained.trace.empty());
}
```

  In `trace_tests.cpp`, test `traced` with a lambda over `checked_evaluate<Q>`, and assert the outcome equals `checked_evaluate<Q>`'s. Add one case where the evaluation fails (division by zero): the failure is in `outcome`, and the trace holds the steps up to it.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement.**

```cpp
/// What an evaluation returned, together with how it was reached.
template <typename R>
struct Traced
{
    /// Exactly what the evaluation returned, failure included.
    R outcome;
    /// Every step the evaluation recorded -- empty when nothing was derived,
    /// as for an entered value; see `explain`.
    Trace<Rational> trace {};
};

/// Runs @p evaluation with a `RecordingSink` writing every symbol as
/// @p vocabulary says, and returns what it returned with the trace it
/// recorded -- so any verb that takes a sink can be traced in one call:
/// `traced([&](auto recordingSink) { return check_method(m, env, recordingSink); })`.
template <typename F, Vocabulary V = DefaultVocabulary>
    requires std::invocable<F&, RecordingSink<Rational, V>>
[[nodiscard]] auto traced(F&& evaluation, V const& vocabulary = V {})
    -> Traced<std::remove_cvref_t<std::invoke_result_t<F&, RecordingSink<Rational, V>>>>
{
    Trace<Rational> recorded {};
    auto evaluated = std::invoke(evaluation, RecordingSink<Rational, V> { recorded, vocabulary });
    return { std::move(evaluated), std::move(recorded) };
}

/// Evaluates @p m for @p Tag and records how -- `evaluate_method`'s traced twin.
template <typename Tag, typename M, typename Env, Vocabulary V = DefaultVocabulary>
[[nodiscard]] auto explain_method(M const& m, Env const& environmentGiven, V const& vocabulary = V {})
{
    return traced([&](auto recordingSink) { return evaluate_method<Tag>(m, environmentGiven, recordingSink); }, vocabulary);
}
// … explain_check_method, explain_curve<DomainResult, ValueResult>, explain_rejection<Result>,
//   explain_check, explain_check_all and explain_conformity in the same three lines, each
//   documented as "<verb>'s traced twin".
```

  - Parameter names must avoid the consumer globals: `environmentGiven`, not `environment`; `recordingSink`, not `sink`.
  - Make `explain_series` and `explain_retry` call `traced` internally and move the result into their existing return types. Their tests must pass unchanged.
- [ ] **Step 4: Docs, probe, CHANGELOG.**
  - `docs/tracing.md`, *Tracing any evaluation*: `traced` and the twins, and the `{ outcome, trace }` shape they share with `explain_series` and `explain_retry`.
  - The probe instantiates `traced` and two twins.
- [ ] **Step 5: Verify** (command 1 only). Expected: `ALL OK`, +9 cases, and no existing trace test changed.
- [ ] **Step 6: Commit** `feat(trace): trace any evaluation with traced, and give every verb an explain twin`.

### Task 7: State a rounding once: `DecimalRounding`

**Files:**
- Modify: `include/formula-cpp/rounding.hpp` (the two types and `declared_rounding`)
- Modify, one forwarding overload beside each existing factory:
  - `rounding_node.hpp:101-112` (`rounded`, `rounded_to_digits`)
  - `method.hpp:1135-1140` (`rounding_rule`)
  - `overlay.hpp:726-740` (both `with_rounding`)
  - `opaque.hpp:1051` (`rounded_output`)
  - `rounded_root.hpp:324` (`rounded_sqrt`)
  - `series.hpp:520` (`rounded_elementwise`, single-places form)
  - `precision.hpp` / `statistics.hpp`, wherever `grep -n "Unit U, DecimalPlaces Places, RoundingMode Mode" include/` shows another **public factory**. Class templates and `detail` stay as they are.
- Test: `rounding_node_tests.cpp`, `method_tests.cpp`, `overlay_tests.cpp`, `rounded_output_tests.cpp`, `rounded_root_tests.cpp`, `series_tests.cpp`
- Create: `test/negative/decimal_rounding_unit_dimension_mismatch.cpp`
- Modify: `docs/rounding-and-conditionals.md` (*Naming a rounding once*), `CHANGELOG.md`, the probe

**Interfaces:**
- Produces:
  - `struct DecimalRounding { Unit unit; DecimalPlaces places; RoundingMode mode; }` and `struct SignificantRounding { Unit unit; SignificantDigits digits; RoundingMode mode; }`, both structural and usable as template arguments;
  - `constexpr DecimalRounding declared_rounding(Unit, RoundingMode)`, whose places are `unit.decimals`;
  - overloads `rounded<R>(x)`, `rounded_to_digits<S>(x)`, `rounding_rule<R>()`, `with_rounding<R>(citation)`, `with_rounding<R>()` (refused as today), `rounded_output<"name", R>(call)`, `rounded_sqrt<R>(x)` and `rounded_elementwise<R>(s)`.
- **Not named `Rounding`:** `overlay.hpp:3177` has a template parameter of that name.

- [ ] **Step 1: Write the failing tests.** Each asserts that the node or rule built with the value has **exactly the same type** as the one built with the triple:

```cpp
TEST_CASE("DecimalRounding: the same node as the three arguments it names", "[rounding_node]")
{
    constexpr formula::DecimalRounding tenthMillimetre { formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero };
    using ByValue = decltype(formula::rounded<tenthMillimetre>(formula::var<Diameter>));
    using ByTriple = decltype(formula::rounded<formula::unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(formula::var<Diameter>));
    STATIC_REQUIRE(std::is_same_v<ByValue, ByTriple>);
}

TEST_CASE("declared_rounding: the places a unit declares", "[rounding]")
{
    constexpr formula::DecimalRounding cents = formula::declared_rounding(formula::unit::Kilogram, formula::RoundingMode::HalfEven);
    STATIC_REQUIRE(cents.places == formula::DecimalPlaces { formula::unit::Kilogram.decimals });
}
```

  Write one `std::is_same_v` case per overload, in that factory's test file.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement.**

```cpp
// rounding.hpp
/// A rounding to decimal places, named once and used wherever a method rounds
/// the same way: which unit the places are of, how many, and which way to go.
/// `constexpr DecimalRounding tenthMpa { unit::Megapascal, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero };`
/// then `rounded<tenthMpa>(x)`, `rounding_rule<tenthMpa>()`. Every factory
/// that takes the three arguments separately also takes this.
struct DecimalRounding
{
    /// The unit the places are counted in.
    Unit unit;
    /// How many decimal places of `unit` to keep.
    DecimalPlaces places;
    /// Which way to break ties, and which way to go.
    RoundingMode mode;
};

/// A rounding to significant digits, named once -- `DecimalRounding`'s
/// counterpart for `rounded_to_digits`.
struct SignificantRounding
{
    /// The unit the digits are counted in.
    Unit unit;
    /// How many significant digits to keep.
    SignificantDigits digits;
    /// Which way to break ties, and which way to go.
    RoundingMode mode;
};

/// A rounding to the decimal places @p roundedIn declares, under @p roundingMode.
[[nodiscard]] constexpr DecimalRounding declared_rounding(Unit roundedIn, RoundingMode roundingMode) noexcept
{
    return DecimalRounding { roundedIn, DecimalPlaces { roundedIn.decimals }, roundingMode };
}

// rounding_node.hpp, beside rounded<U, Places, Mode>
/// `operand` rounded as @p R names: `rounded<tenthMpa>(var<Strength>)`.
template <DecimalRounding R, Node Operand>
[[nodiscard]] constexpr auto rounded(Operand operand) noexcept
{
    return rounded<R.unit, R.places, R.mode>(operand);
}
```

  - `rounding.hpp` must see `Unit`. If it does not include `unit.hpp`, move the two types into `unit.hpp`, or into a header both include, rather than create an include cycle.
  - Overload resolution needs nothing extra. A `DecimalRounding` cannot initialise the old overload's `Unit U` parameter, so that overload drops out by substitution failure. Three explicit arguments cannot fit the new overload's `<DecimalRounding, typename>` head.
  - A unit of the wrong dimension still draws the rounding node's own message, once.
- [ ] **Step 4: Negative case.** `decimal_rounding_unit_dimension_mismatch.cpp` applies a `DecimalRounding` in `unit::Gram` to `var<Diameter>`:

```cmake
formula_add_negative_test(decimal_rounding_unit_dimension_mismatch
    "formula: this rounding node names a unit that does not measure the dimension of the expression it rounds" EXPECT_COUNT 1)
```

- [ ] **Step 5: Docs, probe, CHANGELOG.** `docs/rounding-and-conditionals.md` gets *Naming a rounding once*, with `declared_rounding`.
- [ ] **Step 6: Verify(`decimal_rounding_.*|rounded_.*|rounding_.*|method_.*rounding.*|overlay_.*rounding.*|series_round_.*`).** Expected: `ALL OK`, +8 cases, +1 negative, and the existing rounding negatives unchanged.
- [ ] **Step 7: Commit** `feat(rounding): name a rounding once with DecimalRounding`.

### Task 8: Throwing twins for `Measured`, and a conversion refused at compile time

**Files:**
- Modify: `include/formula-cpp/measured.hpp:173-216`
- Test: `test/measured_tests.cpp`. Find any test pinning `checked_convert_to`'s runtime `DomainError` with `grep -n "checked_convert_to" test/`; each such case becomes the negative below.
- Create: `test/negative/measured_convert_dimension_mismatch.cpp`
- Modify: `docs/quantities.md`, `CHANGELOG.md` (*Added*, and *Changed* for the compile-time refusal), the probe

**Interfaces:**
- Produces:
  - `convert_to<R>(Measured<Q>) -> Measured<R>`
  - `round_to_declared(Measured<Q>, RoundingMode) -> Measured<Q>`
  - `within_bounds(Measured<Q>) -> BoundsCheck`

  Each throws `ArithmeticException`, via `detail::or_throw` of its `checked_` form (`error.hpp:7-10`). `checked_convert_to<R>(Measured<Q>)` now refuses mismatched dimensions at compile time.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST_CASE("convert_to: the throwing twin of checked_convert_to", "[measured]")
{
    STATIC_REQUIRE(formula::convert_to<VolumeInCubicMetres>(formula::Measured<Rise> { 457 }) == *formula::checked_convert_to<VolumeInCubicMetres>(formula::Measured<Rise> { 457 }));
    STATIC_REQUIRE(formula::convert_to<VolumeInCubicMetres>(formula::Measured<Rise>::absent()).is_absent());
}

TEST_CASE("round_to_declared and within_bounds: throwing twins", "[measured]")
{
    // A value that rounds differently under HalfEven and HalfAwayFromZero (defect class 5).
    CHECK(formula::round_to_declared(formula::Measured<SpecimenMass> { formula::Rational { 2'125, 1'000 } }, formula::RoundingMode::HalfEven)
          == *formula::checked_round_to_declared(formula::Measured<SpecimenMass> { formula::Rational { 2'125, 1'000 } }, formula::RoundingMode::HalfEven));
    CHECK(formula::within_bounds(formula::Measured<SpecimenMass>::absent()) == formula::BoundsCheck::NotMeasured);
}
```

- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement.**

```cpp
namespace detail
{
    /// Fails to compile when a measurement is converted into a quantity of
    /// another dimension: no such conversion exists, and both dimensions are
    /// known here.
    template <Described From, Described To>
    struct RequireConvertibleQuantities
    {
        static_assert(Describe<From>::dimension == Describe<To>::dimension,
                      "formula: these two quantities measure different dimensions, so no conversion between them exists; "
                      "the two quantities appear in this diagnostic as the template arguments of RequireConvertibleQuantities");
        static constexpr bool value = true;
    };
} // namespace detail

template <Described R, Described Q>
[[nodiscard]] constexpr std::expected<Measured<R>, ArithmeticError> checked_convert_to(Measured<Q> value) noexcept
{
    static_assert(detail::RequireConvertibleQuantities<Q, R>::value);
    if (value.is_absent())
        return Measured<R> {};
    // … the conversion unchanged, without the runtime dimension check
}

/// Throwing spelling of `checked_convert_to`, for callers who would only rethrow.
template <Described R, Described Q>
[[nodiscard]] constexpr Measured<R> convert_to(Measured<Q> measured)
{
    return detail::or_throw(checked_convert_to<R>(measured));
}
// round_to_declared and within_bounds likewise.
```

  - Update `checked_convert_to`'s comment: the dimensions are now checked where the call is written, whether or not a value is present.
  - The parameter names in these signatures stay as the surrounding code has them. `value` is on the consumer-globals list, but this file already uses it and cl never reports a function template's parameter. Do not rename existing ones.
- [ ] **Step 4: Negative case.**

```cmake
formula_add_negative_test(measured_convert_dimension_mismatch
    "formula: these two quantities measure different dimensions, so no conversion between them exists" EXPECT_COUNT 1)
```

- [ ] **Step 5: Docs, probe, CHANGELOG.** In `docs/quantities.md`, describe the twins and move the conversion's refusal to compile time. *Changed*: a `checked_convert_to` between dimensions that differ used to compile and return `DomainError`; now it does not compile.
- [ ] **Step 6: Verify(`measured_.*|unit_dimension_mismatch|dimension_mismatch`).** Expected: `ALL OK`, +2 cases, +1 negative, and the runtime `DomainError` cases removed (report how many).
- [ ] **Step 7: Commit** `feat(measured): add throwing twins and refuse a conversion across dimensions where it is written`.

### Task 9: Bound formulas: `yields<Q>(expr)`

**Files:**
- Create: `include/formula-cpp/yields.hpp`, which includes `evaluate.hpp` and holds `Yields`, `yields`, and the `evaluate` / `checked_evaluate` overloads
- Modify, one forwarding overload each:
  - `series.hpp` (`checked_evaluate_series`)
  - `rejection.hpp:1430` (`checked_evaluate_rejection`)
  - `trace.hpp` (`explain`, `checked_explain`, `explain_series`, `explain_rejection`)
  - `render.hpp` and `document.hpp` (forward to the inner expression)
  - `calculation.hpp:442-446` (`define`)
- Modify: `include/formula-cpp/formula.hpp` (include `yields.hpp`), `CMakeLists.txt:41-106` (`FILE_SET`), `test/consumer_globals_tests.cpp` (include and probe)
- Create: `test/yields_tests.cpp` (registered in `test/CMakeLists.txt`) and `test/negative/yields_{result_dimension_mismatch,relabelled,series_as_single}.cpp`
- Modify: `docs/expressions.md` (*Naming the result once*), `CHANGELOG.md`

**Interfaces:**
- Consumes: `explain_rejection` (Task 6).
- Produces:
  - `template <Described Q, typename E> struct Yields { using quantity = Q; static constexpr bool valid; E expression; };`
  - `yields<Q>(E) -> Yields<Q, E>`
  - For every verb in *Files*, an overload `verb<Result = detail::ResultOfYields>(Yields<Q, E> const&, …)`. It returns what `verb<Q>(boundFormula.expression, …)` returns. An explicit `Result` equal to `Q` is accepted; any other `Result` is refused.

- [ ] **Step 1: Write the failing tests** in `test/yields_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;
using Rise = formula::Quantity<struct YieldsRiseTag, "h", "height gained", unit::Millimetre>;
using Run = formula::Quantity<struct YieldsRunTag, "L", "horizontal distance covered", unit::Millimetre>;
using Gradient = formula::Quantity<struct YieldsRatioTag, "s", "road gradient", unit::One>;

constexpr auto ratio = formula::yields<Gradient>(var<Rise> / var<Run>);
constexpr auto batch = formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });
} // namespace

TEST_CASE("yields: the result quantity is named once, where the formula is written", "[yields]")
{
    STATIC_REQUIRE(std::is_same_v<decltype(formula::checked_evaluate(ratio, batch)), std::expected<formula::Outcome<Gradient>, formula::ArithmeticError>>);
    STATIC_REQUIRE(formula::checked_evaluate(ratio, batch) == formula::checked_evaluate<Gradient>(ratio.expression, batch));
    STATIC_REQUIRE(formula::checked_evaluate<Gradient>(ratio, batch) == formula::checked_evaluate(ratio, batch));
    STATIC_REQUIRE(formula::number_of(formula::evaluate(ratio, batch)) == formula::Rational { 163, 307 });
}

TEST_CASE("yields: explain, render and document see the formula itself", "[yields]")
{
    auto const explained = formula::explain(ratio, batch);
    CHECK(explained.outcome == formula::explain<Gradient>(ratio.expression, batch).outcome);
    CHECK(formula::render(ratio) == formula::render(ratio.expression));
    CHECK(formula::document(ratio).formula == formula::document(ratio.expression).formula);
}

TEST_CASE("yields: around documented(), and as a calculation's definition", "[yields]")
{
    constexpr auto cited = formula::yields<Gradient>(formula::documented(var<Rise> / var<Run>, { .title = "Road gradient", .reference = "Example Standard 1:2020" }));
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate(cited, batch)) == formula::Rational { 163, 307 });
    constexpr auto definition = formula::define(ratio);
    STATIC_REQUIRE(std::is_same_v<typename decltype(definition)::quantity, Gradient>);
}
```

  Add one case each for a series (`checked_evaluate_series(yields<Q>(series expression), env)`) and for a rejection, using fixtures copied from `series_tests.cpp` and `rejection_tests.cpp`.
- [ ] **Step 2: Run them; they fail.**
- [ ] **Step 3: Implement** `yields.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A formula bound to the quantity it computes, named once where the formula
/// is written: `constexpr auto ratio = yields<Gradient>(var<Rise> / var<Run>);`
/// then `evaluate(ratio, environment)`. The author still names the result --
/// nothing is deduced from the expression, whose dimension does not name a
/// quantity (`evaluate.hpp`) -- but only once. A `Yields` is not a node: it is
/// the top of a formula. Nest `documented()` inside it, not around it, and
/// reuse the formula inside another through `.expression`.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/quantity.hpp>

#include <expected>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// The result a verb is asked for when a `Yields` supplies it.
    struct ResultOfYields
    {
    };

    /// Whether @p E computes @p Q's dimension; true for an expression that
    /// publishes none, which its verb checks instead.
    template <Described Q, typename E>
    [[nodiscard]] consteval bool yields_measures() noexcept
    {
        if constexpr (requires { E::dimension; })
            return refused_already<E>() || E::dimension == Describe<Q>::dimension;
        else
            return true;
    }

    /// Fails to compile when a `Yields` is evaluated for another quantity than
    /// the one it names.
    template <typename Result, Described Q>
    struct RequireYieldsResult
    {
        static_assert(std::is_same_v<Result, ResultOfYields> || std::is_same_v<Result, Q>,
                      "formula: this formula names its result quantity with yields; evaluate it for that quantity, or "
                      "name none -- the two quantities appear in this diagnostic as the template arguments of "
                      "RequireYieldsResult");
        static constexpr bool value = std::is_same_v<Result, ResultOfYields> || std::is_same_v<Result, Q>;
    };
} // namespace detail

/// A formula and the quantity it computes -- built by `yields<Q>(expression)`.
template <Described Q, typename E>
struct Yields
{
    static_assert(!requires { E::dimension; } || RequireResultDimension<Q, E>::value);

    /// The quantity this formula computes.
    using quantity = Q;

    /// Whether the check above holds, asked without firing it, so that a verb
    /// given a refused `Yields` adds no second message.
    static constexpr bool valid = detail::yields_measures<Q, E>();

    /// The formula. Deliberately no `{}` default member initialiser -- see
    /// `Corrections` (`lookup.hpp`).
    E expression;
};

/// @p formulaExpression, bound to the quantity @p Q it computes.
template <Described Q, typename E>
[[nodiscard]] constexpr Yields<Q, E> yields(E formulaExpression) noexcept
{
    return Yields<Q, E> { formulaExpression };
}

/// `checked_evaluate<Q>(boundFormula.expression, ...)`, `Q` taken from the `Yields`.
template <typename Result = detail::ResultOfYields, Described Q, typename E, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Q>, ArithmeticError> checked_evaluate(Yields<Q, E> const& boundFormula,
                                                                                    Env const& environmentGiven,
                                                                                    Sink recordingSink = {}) noexcept
{
    if constexpr (!detail::RequireYieldsResult<Result, Q>::value || !Yields<Q, E>::valid)
        return Outcome<Q>::empty(); // refused already, where the mistake is
    else
        return checked_evaluate<Q>(boundFormula.expression, environmentGiven, recordingSink);
}

/// Throwing spelling of the overload above.
template <typename Result = detail::ResultOfYields, Described Q, typename E, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Q> evaluate(Yields<Q, E> const& boundFormula, Env const& environmentGiven, Sink recordingSink = {})
{
    return detail::or_throw(checked_evaluate<Result>(boundFormula, environmentGiven, recordingSink));
}

} // namespace formula
```

  - `RequireResultDimension` lives in `evaluate.hpp`'s `detail`. Use its real name and namespace.
  - The other overloads (`checked_evaluate_series`, `checked_evaluate_rejection`, `explain`, `checked_explain`, `explain_series`, `explain_rejection`, `render`, `render<D>`, `document`, `define`) follow the same shape in their own headers, and each includes `yields.hpp`. The rendering overloads forward `boundFormula.expression` with no result check: rendering names no result.
  - `define` is `template <typename Result = detail::ResultOfYields, Described Q, Node E> define(Yields<Q, E> const&) -> Definition<Q, E>`.
- [ ] **Step 4: Negative cases.**

```cmake
formula_add_negative_test(yields_result_dimension_mismatch
    "formula: this result quantity does not measure the dimension this expression computes" EXPECT_COUNT 1)
formula_add_negative_test(yields_relabelled
    "formula: this formula names its result quantity with yields" EXPECT_COUNT 1
    REJECT "no matching")
formula_add_negative_test(yields_series_as_single
    "this expression is a series, not a single value; evaluate it with checked_evaluate_series" EXPECT_COUNT 1
    REJECT "formula: this formula names its result quantity with yields")
```

  Deletion-check each case. For the first one, also evaluate the refused `Yields` in the negative file. `EXPECT_COUNT 1` then proves the `valid` gate stops the verb's second message (defect class 2).
- [ ] **Step 5: Install and hygiene.** Add `yields.hpp` to `FILE_SET`, to `formula.hpp`, and to the consumer-globals includes, and probe `evaluate(ratio, batch)`.
- [ ] **Step 6: Docs, CHANGELOG.** `docs/expressions.md` gets *Naming the result once*. It covers why this is not deduction, the nesting rule with `documented()`, and `.expression` for reuse.
- [ ] **Step 7: Verify(`yields_.*|evaluate_.*|define_.*|calculation_.*|rejection_result_dimension`).** Expected: `ALL OK`, +5 cases, +3 negatives, and `hygiene.installed-headers` and `hygiene.consumer-globals` passing.
- [ ] **Step 8: Commit** `feat: bind a formula to its result quantity with yields`.

### Task 10: `std::print` outside the examples

**Files:**
- Modify: `tools/gallery/main.cpp` (26 printf calls), `support/census_report.cpp`, `test/overflow_census_tests.cpp`, `test/package/main.cpp`
- Modify: `support/fail_without_dialogs.cpp:59`, comment only: keep `std::fputs`, and say why. It runs in a CRT invalid-parameter handler that is `noexcept` and must neither allocate nor throw, and `std::print` may do both.

**Interfaces:** None. Output must be byte-identical.

- [ ] **Step 1: Capture a baseline.** Run the gallery and census programs and keep their outputs in `$S\baseline\`.
- [ ] **Step 2: Convert.**
  - `std::printf("%s\n", x.c_str())` becomes `std::println("{}", x)`.
  - `%.*s` of a view becomes `{}`.
  - A `%d` / `%zu` becomes `{}`.
  - Keep the text literally the same.
  - Do not use `std::print(stdout, …)` where plain `std::print(…)` does.
- [ ] **Step 3: Verify byte-identity.** The same programs' outputs are `fc /b`-identical to the baseline. Then run Verify (command 1 only): `ALL OK`, with the gallery check and the census page check passing. Also check the `Package` workflow's compiler has `<print>` (`.github/workflows/package.yml`). If it does not, keep `test/package/main.cpp` as it is and say so in the report. The package build itself is proved in Task 16's CI run.
- [ ] **Step 4: Commit** `refactor: print with std::print in tools, support code and tests`.

### Tasks 11–15: Rewrite the examples

**The same rules apply to every example task. A reviewer rejects a task that breaks any of them:**

1. **Library spellings**:
   - `_r` for every decimal;
   - `Measured<Q> { n }`;
   - `measured_series<Q>(values…)`;
   - `band(a_r, b_r)` and `breakpoint(x_r)`;
   - `number_of(x) == v` for value checks;
   - `yields<Q>` for a formula evaluated more than once;
   - `traced` or an `explain_*` twin instead of any `Trace<> + RecordingSink` block;
   - `DecimalRounding` for a rounding used more than once;
   - throwing twins where the example unwrapped a `checked_` call it never handled;
   - `auto const` where the type was spelled twice;
   - `.contains()` instead of `.find() != npos`.
2. **Delete every local helper the library now covers**: `rat`, `m<Q>`, `valueOf`, `exact`, `exact_text`, `fraction_string`, `strengthOf` / `diameterOf`-style builders that only wrap `environment(Measured{…})`, trace helpers, `endName`, `outcome_word` and `print_dimension`. Keep a helper that states something about the example's domain.
3. **Print with `std::println`**, formatting library values directly (`{}` of a `Rational`, `Measured`, `Outcome`, `Unit`, `Dimension` or enum). No `%.*s`, `.data()`, `.c_str()` or `static_cast<long long>`, and no `.to_double()` just to print.
4. **Output.** Keep every line a doc, the README or a ctest regex quotes **byte-identical**. Change a line only on purpose, and update its quote in the same commit. Run the example before and after, and report the diff of its output, which must be empty or intended line by line.
5. **Keep every self-check** and the final `all checks passed: yes`, which the ctest `PASS_REGULAR_EXPRESSION` pins. Evaluate once, and check what was printed.
6. **Docs in step.** Every ```` ```cpp ```` block quoted from a rewritten file is updated to the new lines, and the unchecked guides' "verbatim" claims are made true. Checked guides must pass `docs.<guide>-output` and `docs.<guide>-snippets`.
7. **Measure.** Report the file's line count and `grep -c` of `Rational {`, `Measured<`, `measurement().value()` and `RecordingSink`, before and after.

Each example task:
- runs Verify (command 1 only): `ALL OK`, no delta in the unit tests, and the examples and docs tests passing;
- commits as `docs(examples): <what the examples now show>`;
- names no task.

### Task 11: The small examples and the README

**Files:** `examples/simple.cpp`, `expressions.cpp`, `quantities.cpp`, `exact_numbers.cpp`, `citations.cpp`, `tracing.cpp`, `composition.cpp`; `README.md`; `docs/expressions.md`, `quantities.md`, `numbers.md`, `citations.md`, `tracing.md`

- [ ] **Step 1:** Rewrite by the rules. Representative target for `expressions.cpp` §5:

```cpp
constexpr auto gradient = formula::yields<Gradient>(var<Rise> / var<Run>);
// …
    auto const batch = formula::environment(formula::Measured<Rise> { 180 }, formula::Measured<Run> { 300 },
                                            formula::entered(formula::Measured<Gradient> { 0.5_r }));
    auto const ratio = formula::checked_evaluate(gradient, batch);
    std::println("{} = {} ({})", formula::symbol_of<Gradient>(), *ratio, ratio->source());
    // …
    bool const overrideWinsOutright = ratio && ratio->is_overridden() && formula::number_of(ratio) == 0.5_r;
```

  - `exact_numbers.cpp` gains the self-check it lacks, plus a final `all checks passed: yes`. Keep its pinned line `ten tenths == one: yes`.
- [ ] **Step 2: Update the README.**
  - Update every README snippet, and its quoted output, to the real new code and output.
  - Fix the README's `evaluate` / `checked_evaluate` drift against `citations.cpp`.
  - `docs.readme-display-output` belongs to Task 12; do not touch those blocks here.
- [ ] **Step 3:** Verify, then commit.

### Task 12: Dimensions and display

**Files:** `examples/dimensions_and_units.cpp`, `display.cpp`; `docs/dimensions.md`, `display.md`; README display blocks

- [ ] **Step 1:** `dimensions_and_units.cpp`: delete `print_exponent` and `print_dimension`, and print with `std::println("{} = {}", label, dimension)`. Each printed line stays as `docs/dimensions.md` quotes it.
- [ ] **Step 2:** `display.cpp`:
  - Delete `rat`.
  - Use `measured_series<DishWeighing>(4.21_r, …)`, `render(x, { .numbers = … })` without `DefaultVocabulary {}`, and `traced` or `checked_explain` instead of the four `Trace<>` blocks.
  - Add a short section that formats an `Outcome`, a `Unit`, a `Dimension` and an enum. Its lines become `docs/display.md`'s new section *Formatting outcomes, units, dimensions and enumerations*, with a ```` ```text ```` block of its real output.
- [ ] **Step 3:** Verify, including `docs.dimensions-*`, `docs.display-*` and `docs.readme-display-output`, then commit.

### Task 13: Constraints, rounding and lookup tables

**Files:** `examples/constraints.cpp`, `rounding_and_conditionals.cpp`, `lookup_tables.cpp`; `docs/constraints.md`, `rounding-and-conditionals.md`, `lookup-tables.md`

- [ ] **Step 1:** Rewrite by the rules. Representative target for `rounding_and_conditionals.cpp`:

```cpp
constexpr formula::DecimalRounding wholeMillimetre { unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero };
constexpr formula::DecimalRounding tenthMillimetre { unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero };
constexpr auto coarseInput = formula::rounded<wholeMillimetre>(var<Diameter>);
constexpr auto sizeAdjustedDiameter =
    formula::when(var<Diameter> > formula::constant<unit::Millimetre>(17.3_r), coarseInput, formula::rounded<tenthMillimetre>(var<Diameter>));
```

  - `constraints.cpp` replaces its hand-built trace (`:99-106`) with `explain_check`.
  - `lookup_tables.cpp`:
    - `valueOf` / `errorOf` become `number_of` / `.error()`;
    - the table in `SizeBands` uses `band(0, 127)`, `band(127, 173)`, …, with its comments kept, because the guide quotes them;
    - the manual trace becomes `checked_explain`, which keeps the failure's trace, where `explain` would throw.
- [ ] **Step 2:** Verify, then commit.

### Task 14: Methods, records and series

**Files:** `examples/methods_and_overlays.cpp`, `records.cpp`, `series.cpp`; `docs/methods-and-overlays.md`, `records.md`, `series.md`

- [ ] **Step 1:** Rewrite by the rules.
  - `methods_and_overlays.cpp`:
    - `derivationOf` / `acceptanceOf` become `explain_method` / `explain_check_method`;
    - the method's rounding becomes `rounding_rule<tenthMpa>()` over a named `DecimalRounding`;
    - `**result` becomes `number_of(result)`.
  - `series.cpp`:
    - delete `rat`, `m<Q>`, the three trace helpers and `outcome_word`;
    - use `measured_series<Retained>(130, 210, 95, 340, 28)`;
    - `{:}` of `ConstraintOutcomeKind` replaces `outcome_word`.
  - `records.cpp`: `exact()` becomes `number_of`, and `traceOf` becomes `traced` / `checked_explain`.
- [ ] **Step 2:** Verify (`docs.methods-and-overlays-*`, `docs.records-*`, `docs.series-*`), then commit.

### Task 15: Statistics, opaque operations and retry, and the electricity bill

**Files:** `examples/statistics.cpp`, `opaque_and_retry.cpp`, `electricity_bill.cpp`; `docs/statistics.md`, `opaque-and-retry.md`, `calculations.md`

- [ ] **Step 1:** Rewrite by the rules.
  - `statistics.cpp`:
    - delete `rat` and `fraction_string`;
    - the five `without_outliers<…>(…, repeatTest, rejectionRule)` calls share one local `constexpr` of the repeated arguments (a small lambda or a named node);
    - the rounded root uses a `DecimalRounding`;
    - every trace-then-evaluate-again pair becomes one `explain_rejection` or `checked_explain`.
  - `opaque_and_retry.cpp`:
    - the 87 `formula::Rational {` become `_r` or integers;
    - `endName` becomes `{}` of `RetryEnd` (its words change from `Accepted` to `accepted`: update the guide's output in the same commit);
    - the five identical `retry<…>(fromZero, halving, settled, repeatDetermination, settledCitation)` share one local;
    - `rounded_output<"slope", millimetrePerSecond, DecimalPlaces { 4 }, RoundingMode::HalfEven>` becomes a named `DecimalRounding`.
  - `electricity_bill.cpp`:
    - `rounded<EuroCent, DecimalPlaces { 0 }, …>` becomes `rounded<formula::declared_rounding(EuroCent, …)>`;
    - `sheet.set(Measured<Price> { Rational { 1, 4 } })` becomes `sheet.set(Measured<Price> { 0.25_r })`;
    - `render(bill, DefaultVocabulary {}, { … })` becomes `render(bill, { … })`;
    - `std::printf("%s\n", std::format(…).c_str())` becomes `std::println(…)`.
- [ ] **Step 2:** Verify (`docs.statistics-*`, `docs.opaque-and-retry-*`, `docs.calculations-*`, `census.*`), then commit.

### Task 16: Finish

**Files:** none new.

This is the first time the non-Windows compilers see the branch; the task's job is to drive **every** compiler to green. Set `$SW = /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/b0c0e78c-1b2d-4d4c-a792-0760adeb4a03/scratchpad` and `$TW = /mnt/d/formula-cpp/.claude/worktrees/concise-spellings`.

- [ ] **Step 1: The full suite, everything at once.** Start these concurrently (Windows and WSL do not share a build tree):
  - `pwsh -NoProfile -File $S\windows-matrix.ps1 -Tree $T`: cl-debug, cl-release, clangcl-debug and clangcl-release, each with every negative test. Must print `MATRIX OK`.
  - `wsl bash $SW/posix-matrix.sh --tree $TW`: gcc-release with g++-14, clang-debug, clang-release and clang-ubsan. Must print `MATRIX OK`.
  - `wsl bash $SW/docs-pages.sh --tree $TW` (Doxygen 1.9.8, no warnings), and `mkdocs build --strict` on Windows.
  - At the same time, push the branch so the draft PR's CI runs the macOS AppleClang leg and the `Package` workflow.
- [ ] **Step 2: Fix to green.**
  - Group every failure by cause: g++ `-Wshadow`, a message counted twice off MSVC, libc++ or AppleClang, Doxygen, or a docs check.
  - Fix one group per commit, each named for what it fixes, never for this plan.
  - After each fix, re-run only the legs that failed. After the last fix, run Step 1 once more in full.
  - Repeat until every local leg prints `MATRIX OK` and every CI job is green.
- [ ] **Step 2a: Whole-branch review.** Dispatch one fresh reviewer, on the most capable model, over `git diff eb5eed8...HEAD`, against this plan's *Design*, *Global Constraints* and *Review Focus*. Run it in parallel with Step 1, since it reads code and builds nothing. Fix what it finds with one commit per finding group, then finish with Step 2's full run.
- [ ] **Step 3: Measure.** Totals over `examples/`, before (`eb5eed8`) and after: lines, and the counts of `Rational {`, `Measured<`, `measurement().value()`, `%.*s`, `RecordingSink` and local `rat(`.
- [ ] **Step 4: PR.** Update the draft PR's title and body (`/contour-workflows:update-pr`) with what changed, the before/after counts, and the two *Changed* entries (the ADL rule for `describe`, and the compile-time conversion refusal). Mark it ready once every CI job is green (`/contour-workflows:fix-ci` for any that is not). **Do not merge** without the owner.
- [ ] **Step 5: Progress notes.** Mark every row of the progress notes landed or done, and state what waits for the owner.

---

## Self-review (writing-plans checklist)

1. **Spec coverage:**

   | Design decision | Task |
   |---|---|
   | Literals and inputs | 1–2 |
   | Reading and printing | 3–5 |
   | A trace from every verb | 6 |
   | A rule stated once | 7–8 |
   | Bound formulas | 9 |
   | `std::print` everywhere | 0, 10, 11–15 |
   | Examples rewritten | 11–15 |
   | Docs in step | every task |

   Nothing in *Design* is without a task.
2. **Placeholder scan:**
   - Library tasks carry their code.
   - "Likewise" appears only where the plan gives one full overload and the rest differ solely in the verb's name, and each such family is named in full.
   - Example tasks carry rules plus a representative target, because pre-writing 19 files would go stale on the first line.
   - Values the implementer must read from real output are said to be read, not invented: the Dimension spelling, the `FailureSite` words, and the examples' outputs.
3. **Type consistency:** these names are used identically in every task: `number_of`, `Traced { outcome; trace; }`, `traced`, `explain_method` / `explain_check_method` / `explain_curve` / `explain_rejection` / `explain_check` / `explain_check_all` / `explain_conformity`, `DecimalRounding { unit; places; mode; }`, `SignificantRounding`, `declared_rounding`, `Yields { expression }`, `yields`, `detail::ResultOfYields`, `not_measured` / `NotMeasured`, and `operator""_r`.
4. **Review Focus:** each of the five lines has its test in its owning task: 1 → Task 1, Steps 1 and 4; 2 → Task 2, Step 4; 3 → Task 4, Steps 4–5; 4 → Task 9, Steps 1 and 4; 5 → Task 10, Step 3, and rules 4 and 6 of Tasks 11–15.
