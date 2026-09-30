# Declared Precision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a formula declare the precision a value the exact layer cannot hold is reported at -- a unit,
decimal places and a rounding mode -- and compute the correctly rounded decimal of the true value in integer
arithmetic, starting with `rounded_output<"slope", U, Places, Mode>(fit)` for an opaque operation's output that
`Rational` cannot hold, and leaving every unrounded form exact-or-refuse.

**Architecture:**
- Two `detail::` headers, pure integer arithmetic: `detail/wide_int.hpp` (`WideUnsigned<Limbs>`, 32-bit limbs,
  every product in `std::uint64_t`, every overflow an empty `std::optional`) and `detail/wide_rounding.hpp`
  (the decimal a `WideRatio` rounds to, built to agree with `checked_round`, and `decide_rounding` for an
  enclosure).
- `opaque.hpp` gains `RoundedOpaqueOutputNode` and `rounded_output`, the fused counterpart of
  `opaque_output`, as `rounded_sqrt` is of `rounded<>(sqrt())`. It evaluates the call's inputs as today and
  then calls the operation's optional `compute_exact` hook (wide integers) or, without one,
  `compute<Rational>` and rounds exactly in `U`. A sink hearing the call is told
  `OpaqueValues::RoundedWhereUsed` and handed `OpaqueEvaluated<Rational, 0>`: presence and failure, no value.
- The trace gets `StepKind::RoundedOpaqueOutput`; the call's line names its outputs without values
  (`intercept, slope: rounded where used`), the output's own line states the rounding. Every registry that
  knows `OpaqueOutputNode` learns the new node. `LinearLeastSquares` gains `compute_exact` (8 limbs), so a
  fit that overflows `Rational` from 34 points answers to 128.

**Tech Stack:** C++23, header-only; Catch2 3.6 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness; the
overflow census (`formula-cpp-census-tests`, `docs.numeric-headroom`).

**Spec:** `docs/superpowers/specs/2026-09-29-declared-precision-design.md` (read it in full before Task 1).

## Contract adjustments

The interface the two parallel plans (logarithms and exponentials; regression over observations) rely on is
kept by **name and signature**. What reading the code changed, each minimal:

1. **`WideUnsigned` is constrained and has two helpers.** `template <std::size_t Limbs> requires(Limbs >= 2)
   class WideUnsigned` -- `from_u64` needs two limbs. Added: `static constexpr WideUnsigned from_limbs(
   std::array<std::uint32_t, Limbs> const&) noexcept` and `constexpr std::array<std::uint32_t, Limbs> const&
   limbs() const noexcept`, because the free functions (`add_checked_or_none`, `divmod`, ...) must build a
   result and the class keeps `_limbs` private. `limb(at)` answers 0 for a position past the top.
2. **Parameter names differ from the contract's comments, types do not.** `value`, `lower`, `upper` and `unit`
   are consumer globals (`test/consumer_globals_tests.cpp:125-144`, cl C4459 under /WX); the parameters are
   `unrounded`, `lowerBound`, `upperBound`, `roundedIn`. Callers are unaffected.
3. **`round_wide_ratio`, `rounded_in_unit` and `narrow_wide_ratio` reduce their argument first** (binary gcd).
   Measured in a probe of this plan's algorithm (below): without it, the least-squares tests' fixture of
   distinct denominators fails *in the rounding* from 52 points although `compute_exact` answered; with it, the
   first failure is `compute_exact`'s own, at 58. The contract's "need not be reduced" holds for callers.
4. **`OpaqueStepData` gains `answered` beside the contract's `values`.** On the rounded route no output holds a
   value, so "the call was absent" and "the call answered" are otherwise indistinguishable to the renderer.
5. **The rounded route tells a sink `OpaqueEvaluated<Rational, 0>`.** `opaque_produced(info, result)` receives
   an evaluation with no values -- failure, absence, or an empty array for "answered". `RecordingSink`'s hook
   is already a template on `M` (`trace.hpp:3952-3953`); a sink written for one fixed `M` does not hear the
   rounded call (`detail::HearsOpaque`, `opaque.hpp:1038-1042`) and its output line then says
   `[inside not shown]`, the existing fallback.
6. **`RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin = detail::NamedOpaqueOutput>`**: `OpaqueOutputNode`'s
   parameters (`opaque.hpp:816`) with the rounding triple before `Origin`, which keeps its default.
7. **The `compute_exact` hook is detected only when** `Op::exact_limbs >= 4` (a `Rational` numerator times a
   unit factor must fit), the return type is exactly `std::expected<std::array<detail::WideRatio<exact_limbs>,
   M>, ArithmeticError>`, and the call is `noexcept`. Otherwise `compute<Rational>` is used, silently -- the
   hook is internal (`detail::declares_compute_exact<Op, Inputs...>`).
8. **Helpers added** (the other plans may use them): `detail::WideSigned<L>` with `add_checked_or_none`,
   `sub_checked_or_none`, `mul_checked_or_none` overloads, `detail::lcm_checked_or_none`, `detail::reduced`,
   `detail::scaled_to_denominator`, `detail::evaluate_rounded_call`, `detail::unrounded`.
9. **Renamed (controller's ruling):** every wide-integer function returning `std::optional` is `*_checked_or_none`
   -- `checked_add`/`sub`/`mul`/`mul_small`/`add_small`/`shift_left`/`lcm` became `add_checked_or_none`,
   `sub_checked_or_none`, `mul_checked_or_none`, `mul_small_checked_or_none`, `add_small_checked_or_none`,
   `shift_left_checked_or_none`, `lcm_checked_or_none` -- because `checked_int.hpp:113-116` reserves the
   `checked_` prefix for `std::expected`, and its own `std::optional` helpers are already
   `add_checked_or_none`/`sub_checked_or_none`/`mul_checked_or_none` (`:118-140`); `checked_int.hpp` is not
   modified.
10. **`docs/display.md`'s new section names no logarithm.** `ln` and `exp` do not exist on this branch; the
    section is written about `rounded_sqrt` and `rounded_output`, and the logarithms plan adds its sentence.
11. **Controller's addition, folded into Task 1:** `template <std::size_t L> struct WideSmallDivision {
    WideUnsigned<L> quotient; std::uint32_t remainder; };` and `template <std::size_t L> constexpr
    WideSmallDivision<L> divmod_small(WideUnsigned<L> const& dividend, std::uint32_t divisor) noexcept`
    (precondition `divisor != 0`; limb-level long division with `std::uint64_t` intermediates), and
    `mul_checked_or_none` skips the zero limbs of its left operand (tested in both operand orders with a zero-heavy
    operand). Measured by the logarithms planner on cl 19.51.36257: the logarithms kernel drops from
    104,000-159,000 to 5,000-68,000 constant-evaluation steps per call with `divmod_small`, and its worst call
    is 31,000 steps at 12 limbs (21,000 at 8) with the zero-limb skip.

## Global Constraints

These bind every task. Several exist because the alternative failed in an earlier phase.

- **C++23, header-only**, no dependency beyond the standard library in shipped headers. `detail/wide_int.hpp`
  includes only `<array>`, `<compare>`, `<cstddef>`, `<cstdint>`, `<optional>`; `detail/wide_rounding.hpp`
  only `rational.hpp`, `rounding.hpp`, `unit.hpp`, `error.hpp`, `detail/wide_int.hpp` and `<cstddef>`,
  `<cstdint>`, `<expected>`, `<optional>`.
- **Integer arithmetic only** in the exact path: no `<cmath>`, no intrinsic, no `__int128` (cl has none), no
  floating point. Every product of two limbs is formed in `std::uint64_t`.
- **Per task, verify on MSVC `cl-debug` and g++-14 only** (the owner's rule; all eight presets, Doxygen and
  mkdocs run once at the very end, by the controller):
  - Windows: `pwsh -NoProfile -File C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\6031eb20-da56-4aa2-bb6a-de8c11d53bfc\scratchpad\cl.ps1 [-Filter <ctest -R regex>] [-Target <target>] [-NoTest]`
    (enters the VS dev shell, configures `cl-debug` in `out/build/cl-debug` if needed, builds, runs `ctest -j 12`;
    prints `ALL OK` / `BUILD FAILED` / `TESTS FAILED` and exits non-zero on failure).
  - g++-14 (WSL): `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/6031eb20-da56-4aa2-bb6a-de8c11d53bfc/scratchpad/gcc14.sh [ctest -R regex]`
    (configures `--preset gcc-release -B out/build/gcc14-release -DCMAKE_CXX_COMPILER=g++-14` -- the only
    preset with `-Wshadow -Wconversion -Wpedantic -Werror` -- builds, runs ctest). **Never `wsl bash -lc`.**
  - Baseline at `c551c5d`: 1808/1808 tests on both. Report both counts at the end of each task.
  - A `STATIC_REQUIRE` failure is a *build* error: never redirect a build's output away.
  - Doxygen per task on the changed public headers: run the `formula-cpp-docs-api` target if Doxygen is
    available (`cl.ps1 -Target formula-cpp-docs-api -NoTest`), else say in the report that it was not run.
    Doxygen fails on an undocumented entity: document every new one, `detail::` included, as this code base does.
- **Constant-evaluation budget.** cl's documented default of 100,000 steps is **not** its effective default.
  Measured on cl 19.51.36257: its diagnostic names "evaluation exceeding step limit of 1048576
  (/constexpr:steps<NUMBER>)"; a 349,436-iteration loop compiles at the default and needs `/constexpr:steps` of
  about 1,049,814; 20 full 256-bit binary `divmod`s fit one constant expression and 40 do not; one binary
  `divmod` over 12 limbs costs about 130,000 steps, which is why `divmod_small` exists (Task 1). clang-cl 22.1.3
  took 62,000-95,000 steps per call of the logarithms kernel and compiled at its default; clang++ and g++ were
  not measured. So: `STATIC_REQUIRE` only at 4 limbs with small operands; **every test that runs
  `LinearLeastSquares::compute_exact` (8 limbs) is a run-time `CHECK`**.
- **UBSan and `-Wconversion`.** No shift of a 32-bit limb by 32 or more (split counts into `count / 32` and
  `count % 32`, and never shift by `32 - 0`); every narrowing is a `static_cast`; unsigned subtraction is
  modular and masked (`(fromLimb - taken) & 0xFFFF'FFFFU`), never signed.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:** SPDX header on every file; no `NOLINT`;
  core public headers include no `<string>`, `<vector>`, `<format>` or `<iostream>` (`hygiene.headers`); every
  public `static_assert` message begins `formula: ` and is stable (negative tests match on it); never identify a
  type with `decltype([]{})`; every new header goes into the install `FILE_SET` (root `CMakeLists.txt:44-50`,
  `hygiene.installed-headers`); every new *public* header into `test/consumer_globals_tests.cpp`'s includes
  (none is added here: both new headers are `detail/`).
- **Defect classes.** Read `D:\formula-cpp\.superpowers\sdd\2026-09-25-methods-and-overlays\defect-classes.md`
  before Task 1. Each task's report says, for each of its nine classes, what you checked and how.
- **No third-party standard content.** Cite only invented `Example Standard N:YYYY` references. Never name a
  real standards body or standard number anywhere (tests, comments, commit messages, docs) --
  `hygiene.no-real-standards` scans every tracked file. Fixture numbers here are plainly invented (primes
  103, 127, 139, 163, 197, ...; readings such as 127.3 g).
- **Negative tests** (`test/negative/*.cpp` + `formula_add_negative_test(<name> <expected-text> ...)` in
  `test/CMakeLists.txt`) assert that the build fails *and* fails with this library's own text. Register each
  with a deliberately **wrong** expected string first and watch the test fail, then the right one. **Deletion
  check** for each: delete the guard it pins, confirm the case then compiles (the test fails), restore with a
  **plain write** (or `touch`) -- a timestamp-preserving copy leaves ninja trusting stale outputs. Use
  `EXPECT_COUNT 1` where a second message could plausibly fire (cl does not count; g++ does).
- **No `{}` default member initialiser on any member that holds an expression, a node or a call**
  (`RoundedOpaqueOutputNode::call` has none, as `OpaqueOutputNode::call` has none, `opaque.hpp:822`).
- **Names.** Under g++ `-Wshadow` and cl C4459 no parameter or local may hide one of the ~258 globals in
  `test/consumer_globals_tests.cpp:125-147` -- read the list. Traps for this work: `value`, `result`, `count`,
  `size`, `sum`, `total`, `scale`, `scaled`, `factor`, `part`, `next`, `level`, `spread`, `slope`,
  `intercept`, `points`, `fit`, `numerator`, `denominator`, `quotient`, `sign`, `width`, `position`, `low`,
  `high`, `lower`, `upper`, `lo`, `hi`, `lhs`, `rhs`, `left`, `right`, `input`, `output`, `info`, `ratio`,
  `rate`, `unit`, `mode`, `digits`, `index`, `n`, `k`, `x`, `y`, `d`, `q`, `r`. cl reports a non-template
  function's locals and parameters wherever the header is included (so `LinearLeastSquares::compute_exact` and
  every `inline` function in `trace_render.hpp` are checked by merely including them), a template's locals
  once instantiated, and never a function template's parameters. `StepKind` enumerators are checked under g++
  `-Wshadow` against every name in namespace `formula` (`trace.hpp:60-64`).
- **Documentation is read by humans.** Every ```` ```text ```` block in a guide is consecutive lines of its
  example's real output and every ```` ```cpp ```` block consecutive lines of its source (`docs.<guide>-output`
  / `-snippets`); a quoted compiler diagnostic comes from a real compile (`hygiene.documented-diagnostics`,
  `hygiene.documented-diagnostic-text`). No internal labels in any public text or commit message: no phase,
  task, lane, reviewer, plan or tracker names -- every sentence must make sense to a reader who never saw this
  plan. No sentence may claim what nobody measured ("every compiler", "confirmed"); name the compilers.
- **Do not run clang-format** on existing files (it rewrites 99 files and breaks `var<Q> * x`). Match the
  surrounding style by hand.
- **Catch2 splits test filters on commas**, and `cl.ps1 -Filter` is a *ctest* `-R` regex over test names.
  Every new `TEST_CASE` name here starts with a filterable prefix (`wide unsigned:`, `wide rounding:`,
  `rounded output:`) and holds no comma or bracket. Prove a filter selected something (the ctest count) before
  trusting it.
- **Commits.** One per task, conventional subject, a body that explains why, and the last line exactly
  `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`. Commit with `git commit -m "<subject>" -m
  "<body>" -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`. Never `--no-verify`, never amend or
  rewrite another task's commit, never commit files outside your task. Every commit builds and passes alone.
- **You are the only writer in the worktree** `D:\formula-cpp\.claude\worktrees\next-features`. Do not
  dispatch subagents. Do not touch another worktree or `D:\formula-cpp` itself. End your turn after the task's
  report; do not start the next task.
- **CHANGELOG.md** gets its entry under `## [Unreleased]` (`CHANGELOG.md:7`; `### Added` at :9, `### Changed`
  at :178) in the task that changes public behaviour.

---

## Findings the design depends on (from `c30f261`)

1. `opaque_output`'s name parameter is `detail::FixedString Name` (`opaque.hpp:841`); the node is
   `OpaqueOutputNode<std::size_t I, typename Call, typename Origin = detail::NamedOpaqueOutput>`
   (`opaque.hpp:816-835`), with `index`, `output`, `dimension` and a `detail::RefusedFlag refused`. An unknown
   name is refused once (`RequireOpaqueOutputNamed`, `:753-763`) and yields a node at `detail::unknownOutput`
   with `UnnamedOpaqueOutput` origin; a hand-built bad position is refused by `RequireOpaqueOutputPosition`
   (`:788-796`).
2. `Unit` is a structural struct used as a template argument (`unit.hpp:66-87`, `template <Unit U>` in
   `rounding_node.hpp:48`); its offset is `offsetNumerator`/`offsetDenominator`. `rounded_sqrt` refuses an
   offset unit in the same shape we need (`rounded_root.hpp:79-88`), gated on its dimension check.
3. `RepRounding<Rational>::round_in(value, unit, places, mode)` (`rounding_node.hpp:130-151`) converts into the
   unit, `checked_round`s and converts back; `RepRounding<double>` refuses at compile time with "formula: a
   rounding node cannot be evaluated with Rep = double" (`:178-197`).
4. The recorder reads a node's `unit`, `places` and `mode` by `requires` (`trace.hpp:3026-3027`,
   `3091-3100`), and pushes `OpaqueOutputStepData` only for `StepKind::OpaqueOutput` (`:3249-3251`).
   `step_expression`'s `switch` has **no default** (`trace_render.hpp:905-1137`); every other `switch` over a
   step kind there has one.
5. `Step` has 47 fields, pinned (`test/opaque_tests.cpp:712-722`): nothing is added to `Step`. The call's data
   lives in `OpaqueStepData` (`trace.hpp:1390-1412`), which has 5 members today.
6. `RecordingSink::opaque_produced` is `template <std::size_t M>` (`trace.hpp:3952-3953`) and records outputs
   `outputAt < M` (`:3978-3990`). `HearsOpaque<Sink, Rep, M>` asks for both hooks (`opaque.hpp:1038-1042`).
7. Registries that name `OpaqueOutputNode` and so must name the new node: `StepKindOf` (`trace.hpp:1913-1917`),
   `LevelChildren` (`precision.hpp:666-672`; it also drives the calculation, rejection and retry walks,
   `calculation.hpp:300-325`), `ConstantRewrite` (`overlay.hpp:1947-1994`), `SubstitutedIn`
   (`overlay.hpp:2503-2508`), `document`'s `collect` (declared `document.hpp:541-542`, defined `:1186-1210`),
   `render_node` (`render.hpp:1834-1862`). Nothing else in `include/` names it.
8. A calculation holds single values (`calculation.hpp:182`, `:352`): the worksheet test uses an operation over
   two single values, not the fit.
9. `formula::detail` already has the non-templates `add_checked_or_none`, `sub_checked_or_none` and
   `mul_checked_or_none` over `(Int, Int)` (`checked_int.hpp:118`, `:126`, `:134`), `gcd(std::uint64_t,
   std::uint64_t)` (`:144`) and `pow10(int)` (`:184`); the wide functions of the same names overload them and
   **no call can be ambiguous**. The wide ones are function templates whose parameters are `WideUnsigned<L>`
   or `WideSigned<L>`, so `L` is never deduced from an `Int`, `std::uint64_t` or `int` argument and the
   template drops out -- every existing call, including the unqualified `mul_checked_or_none(power, middle)`
   inside `detail` (`rational.hpp:543`) and the qualified ones (`rational.hpp:184`, `:327-333`, `:372-373`,
   `rounding.hpp:106`), resolves to the non-template exactly as today; `pow10<L>` takes `L` explicitly, which
   the non-template `pow10` cannot accept. The other way round, neither wide type converts implicitly from or
   to an integer (`WideUnsigned` has only a defaulted constructor, `from_u64`/`from_limbs` factories and no
   conversion operator; `WideSigned` is an aggregate), so a wide call never reaches an `(Int, Int)`
   non-template. Task 1 pins both directions in one translation unit that includes both headers (the carries
   case). `formula::checked_add(Rational, Rational)` and its siblings share no name with any of these.
10. `docs/*.md` quote line numbers only of `expression.hpp`, `environment.hpp`, `format.hpp`, `band.hpp`,
    `lookup.hpp` and `calculation.hpp`; this plan edits none of them.
11. The consumer-globals probe has 80 checks (`test/consumer_globals_run_tests.cpp:16`).
12. The example `opaque_and_retry` prints "fifteen distinct denominators: overflow in exact arithmetic"
    (`examples/opaque_and_retry.cpp:293-297`) and has a census twin; editing it changes the page's examples
    table, which is cl's (`cmake/CheckCensusPage.cmake:14-19`).

### Measured for this plan (probe in the controller's scratch directory, cl 19.51.36257, `/std:c++latest /W4 /permissive- /O2`)

A probe implemented Task 1's `WideUnsigned` operations, Task 2's `reduced`, `round_wide_ratio` and
`rounded_in_unit`, and Task 6's `compute_exact` exactly as this plan spells them, against this worktree's headers:

- `divmod`, `mul_checked_or_none` and `gcd` agree with 64-bit arithmetic on 20,000 random operand pairs each; the
  top-limb division `(2^128 - 1) / (2^127 + 1)` is quotient 1, remainder `2^127 - 2`.
- `round_wide_ratio(wide_from_rational<4>(v), places, mode)` equals `checked_round(v, places, mode)` in all
  **43,729** cases `checked_round` accepts over the grid of Task 2 (17 numerators x 11 denominators x places
  -18..18 x 7 modes, plus `IntMin`, `IntMax`, `IntMin/3`, `IntMax/2`), 0 mismatches.
- `rounded_in_unit` equals `RepRounding<Rational>::round_in` in all **64,582** accepted cases over the same
  values, 5 units (g, mm, mm/min, min, %), places -3..6, 7 modes.
- The 4-point fixture through `compute_exact` at 8 limbs and `rounded_in_unit` **is a constant expression on
  cl** (`static_assert` compiled), with reduction.
- The census shapes at 8 limbs, slope rounded to 4 dp of N/s: readings at 1 dp and at 3 dp near 2410 N never
  fail for 2..128 points (the exact route fails at 57 sizes from 34 at 3 dp); a different denominator on every
  point fails at **71 of 127 sizes, first at 58**, in `compute_exact` (at 16 limbs: 17 of 127, first 112). The
  least-squares tests' own distinct fixture (lengths in mm) fails first at **58**, in `compute_exact`.
  Wherever the exact route answered, the rounded route agreed in all 7 modes, both outputs: 1,778 + 966 + 182
  comparisons, 0 differences.
- Exact values, checked by hand in `fractions`: fixture slope 19/28 mm/s at 4 dp is `3393/5000` (HalfEven,
  HalfAwayFromZero, HalfTowardZero, Ceiling, AwayFromZero) and `1357/2000` (Floor, TowardZero); intercept 19/2
  mm at 0 dp is 10 (HalfEven, HalfAwayFromZero, Ceiling, AwayFromZero) and 9 (HalfTowardZero, Floor,
  TowardZero); at -1 dp it is 10 except Floor and TowardZero, 0. 285/7 mm/min at 2 dp is `4071/100`, `1018/25`
  under Ceiling and AwayFromZero. The 15-point distinct fixture's slope is 1.93724895... mm/s, `4843/2500` at 4
  dp (`19373/10000` under Ceiling, AwayFromZero), intercept -0.01768847... mm, `-177/10000` (HalfX, Floor,
  AwayFromZero) and `-11/625` (Ceiling, TowardZero). 4843/2500 mm/s is 14529/125 = 116.232 mm/min.

## Review Focus

1. **Negative values under the sign-dependent modes** (Floor, Ceiling, TowardZero, AwayFromZero) on the wide
   route: a fit whose intercept is negative must round as `checked_round` would, not by magnitude alone. Pinned
   in Task 2 (`-19/2` in every mode; the negative enclosure) and Task 6 (the 15-point intercept, -0.0177 vs
   -0.0176 by mode).
2. **A declared unit whose factor is not a power of ten, and negative places**: 285/7 mm/min at 2 dp and 19/2
   mm at -1 dp must equal `rounded<...>(opaque_output<...>)` in every mode. Pinned in Task 2 (the 64,582-case
   unit sweep) and Task 6 (the node-level equivalence).
3. **A rounding that fails after the call answered** -- the kept integer leaves `Rational::Int` (2^40 x 2^40) --
   must fail the output alone, the call's line still saying it answered. Pinned in Task 2 (`IntMin` accepted,
   2^63 and 2^70 refused) and Tasks 3 and 5 (`WideProduct`).
4. **One call used both rounded and plain in one formula**: two runs, two call lines -- one with values, one
   "rounded where used" -- one page entry, and each output right. Pinned in Task 4 (page) and Task 5 (trace).
5. **An operation whose exact hook is not taken** -- not `noexcept`, fewer than 4 limbs -- falls back to
   `compute<Rational>` and therefore to its `Overflow`, never to a half-declared hook. Pinned in Task 3.

---

### Task 1: Wide unsigned integers (`detail/wide_int.hpp`)

**Files:**
- Create: `include/formula-cpp/detail/wide_int.hpp`
- Create: `test/wide_int_tests.cpp`
- Modify: `CMakeLists.txt:44-50` (FILE_SET: add `detail/wide_int.hpp` after `detail/type_list.hpp`)
- Modify: `test/CMakeLists.txt:95-96` (add `wide_int_tests.cpp` after `format_tests.cpp`)

**Interfaces:**
- Consumes: nothing.
- Produces (namespace `formula::detail`, all `constexpr noexcept`, every function a template on `std::size_t L`):
  `WideUnsigned<Limbs>` (`bits`, `from_u64`, `from_limbs`, `limbs()`, `to_u64`, `is_zero`, `bit_length`,
  `limb`, `==`, `<=>`); `add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none`,
  `mul_small_checked_or_none(WideUnsigned<L> const&, std::uint32_t)`, `add_small_checked_or_none(WideUnsigned<L>
  const&, std::uint32_t)`, `shift_left_checked_or_none(WideUnsigned<L> const&, std::size_t)` ->
  `std::optional<WideUnsigned<L>>`; `shift_right(WideUnsigned<L> const&, std::size_t)`
  -> `WideUnsigned<L>`; `WideDivision<L> { quotient; remainder; }` and `divmod(dividend, divisor)`;
  `WideSmallDivision<L> { WideUnsigned<L> quotient; std::uint32_t remainder; }` and `divmod_small(WideUnsigned<L>
  const& dividend, std::uint32_t divisor)` (precondition `divisor != 0`); `gcd(WideUnsigned<L>,
  WideUnsigned<L>)`; `lcm_checked_or_none(WideUnsigned<L> const&, WideUnsigned<L> const&)`;
  `pow10<L>(std::size_t)` -> `std::optional<WideUnsigned<L>>`; `WideRatio<L> { bool negative = false; WideUnsigned<L> numerator; WideUnsigned<L>
  denominator; }`; `WideSigned<L> { bool negative = false; WideUnsigned<L> magnitude; }` with `add_checked_or_none`,
  `sub_checked_or_none`, `mul_checked_or_none` overloads -> `std::optional<WideSigned<L>>`.

- [ ] **Step 1: Write the failing tests** -- `test/wide_int_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/wide_int.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace
{
using W4 = formula::detail::WideUnsigned<4>;
using W8 = formula::detail::WideUnsigned<8>;
using S4 = formula::detail::WideSigned<4>;
using Int = formula::detail::Int;
// Each using-declaration also brings in the non-template `Int` overload of the
// same name from checked_int.hpp, so this file resolves both families together.
using formula::detail::add_checked_or_none;
using formula::detail::add_small_checked_or_none;
using formula::detail::divmod;
using formula::detail::divmod_small;
using formula::detail::gcd;
using formula::detail::lcm_checked_or_none;
using formula::detail::mul_checked_or_none;
using formula::detail::mul_small_checked_or_none;
using formula::detail::pow10;
using formula::detail::shift_left_checked_or_none;
using formula::detail::shift_right;
using formula::detail::sub_checked_or_none;

constexpr std::uint64_t allBits64 = std::numeric_limits<std::uint64_t>::max();
constexpr W4 topBit4 = W4::from_limbs({ 0U, 0U, 0U, 0x8000'0000U });
constexpr W4 allBits4 = W4::from_limbs({ 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0xFFFF'FFFFU });

/// A reproducible stream of 64-bit operands (a linear congruential
/// generator with Knuth's MMIX constants), so that a failure names its case.
struct Operands
{
    std::uint64_t state;

    std::uint64_t draw() noexcept
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
};
} // namespace

TEST_CASE("wide unsigned: a value round-trips through 64 bits and reports its width", "[wide-int]")
{
    STATIC_REQUIRE(W4::bits == 128);
    STATIC_REQUIRE(W8::bits == 256);
    STATIC_REQUIRE(W4 {}.is_zero());
    STATIC_REQUIRE(W4 {}.bit_length() == 0);
    STATIC_REQUIRE(W4::from_u64(1).bit_length() == 1);
    STATIC_REQUIRE(W4::from_u64(allBits64).to_u64() == allBits64);
    STATIC_REQUIRE(W4::from_u64(allBits64).bit_length() == 64);
    STATIC_REQUIRE(W4::from_limbs({ 0U, 0U, 1U, 0U }).bit_length() == 65);
    STATIC_REQUIRE(topBit4.bit_length() == 128);
    STATIC_REQUIRE(!topBit4.to_u64().has_value());
    STATIC_REQUIRE(W4::from_u64(0x1'0000'0002ULL).limb(0) == 2U);
    STATIC_REQUIRE(W4::from_u64(0x1'0000'0002ULL).limb(1) == 1U);
    STATIC_REQUIRE(W4::from_u64(7).limb(9) == 0U); // past the top
    STATIC_REQUIRE(W4::from_u64(7).limbs()[0] == 7U);
}

TEST_CASE("wide unsigned: carries cross 2^32 and 2^64 and overflow at the top is reported", "[wide-int]")
{
    STATIC_REQUIRE(*add_small_checked_or_none(W4::from_u64(0xFFFF'FFFFULL), 1U) == W4::from_u64(0x1'0000'0000ULL));
    STATIC_REQUIRE(*add_checked_or_none(W4::from_u64(allBits64), W4::from_u64(1)) == W4::from_limbs({ 0U, 0U, 1U, 0U }));
    STATIC_REQUIRE(!add_checked_or_none(topBit4, topBit4).has_value());
    STATIC_REQUIRE(!add_small_checked_or_none(allBits4, 1U).has_value());
    STATIC_REQUIRE(*sub_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_u64(1)) == W4::from_u64(allBits64));
    STATIC_REQUIRE(!sub_checked_or_none(W4::from_u64(1), W4::from_u64(2)).has_value()); // negative, never wrapped
    STATIC_REQUIRE(sub_checked_or_none(W4::from_u64(2), W4::from_u64(2))->is_zero());

    // Same names, two families, never ambiguous: `Int` operands reach the
    // non-templates of checked_int.hpp (a template on L is never deduced from
    // an Int), and wide operands the templates (no wide type converts to Int).
    STATIC_REQUIRE(std::is_same_v<decltype(add_checked_or_none(Int { 2 }, Int { 3 })), std::optional<Int>>);
    STATIC_REQUIRE(*add_checked_or_none(Int { 2 }, Int { 3 }) == 5);
    STATIC_REQUIRE(!mul_checked_or_none(formula::detail::IntMax, Int { 2 }).has_value());
    STATIC_REQUIRE(*sub_checked_or_none(Int { 2 }, Int { 5 }) == -3); // Int subtracts below zero
    STATIC_REQUIRE(std::is_same_v<decltype(sub_checked_or_none(W4 {}, W4 {})), std::optional<W4>>);
    STATIC_REQUIRE(std::is_same_v<decltype(mul_checked_or_none(S4 {}, S4 {})), std::optional<S4>>);
    STATIC_REQUIRE(gcd(std::uint64_t { 12 }, std::uint64_t { 18 }) == 6U);
}

TEST_CASE("wide unsigned: products are schoolbook and never wrap", "[wide-int]")
{
    // (2^64 - 1)^2 = 2^128 - 2^65 + 1.
    STATIC_REQUIRE(*mul_checked_or_none(W4::from_u64(allBits64), W4::from_u64(allBits64))
                   == W4::from_limbs({ 1U, 0U, 0xFFFF'FFFEU, 0xFFFF'FFFFU }));
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_limbs({ 0U, 0U, 1U, 0U })).has_value());
    STATIC_REQUIRE(!mul_checked_or_none(topBit4, W4::from_u64(2)).has_value());
    STATIC_REQUIRE(mul_checked_or_none(allBits4, W4 {})->is_zero());
    STATIC_REQUIRE(*mul_small_checked_or_none(W4::from_u64(1'000'000'000'000'000'000ULL), 10U)
                   == W4::from_u64(10'000'000'000'000'000'000ULL));
    // 10^20 needs 67 bits: past 64, inside 128.
    STATIC_REQUIRE(!mul_small_checked_or_none(W4::from_u64(1'000'000'000'000'000'000ULL), 100U)->to_u64().has_value());
    STATIC_REQUIRE(!mul_small_checked_or_none(topBit4, 2U).has_value());
    // A zero-heavy operand on either side, since the zero limbs of the left
    // one are skipped: 7 * 2^32 times 2^64 - 1 is 7 * 2^96 - 7 * 2^32.
    constexpr W4 sparse = W4::from_limbs({ 0U, 7U, 0U, 0U });
    constexpr W4 dense = W4::from_u64(allBits64);
    STATIC_REQUIRE(*mul_checked_or_none(sparse, dense) == W4::from_limbs({ 0U, 0xFFFF'FFF9U, 0xFFFF'FFFFU, 6U }));
    STATIC_REQUIRE(*mul_checked_or_none(dense, sparse) == *mul_checked_or_none(sparse, dense));
    // 2^96 times 2^64 overflows whichever operand is on the left.
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 0U, 1U }), W4::from_limbs({ 0U, 0U, 1U, 0U })).has_value());
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_limbs({ 0U, 0U, 0U, 1U })).has_value());
}

TEST_CASE("wide unsigned: short division by a 32-bit divisor agrees with long division", "[wide-int]")
{
    constexpr auto byThree = divmod_small(allBits4, 3U);
    STATIC_REQUIRE(byThree.remainder == 0U);
    STATIC_REQUIRE(byThree.quotient == W4::from_limbs({ 0x5555'5555U, 0x5555'5555U, 0x5555'5555U, 0x5555'5555U }));
    // 2^128 - 1 = (2^32 - 1)(2^96 + 2^64 + 2^32 + 1): the largest divisor, no remainder.
    constexpr auto byLargest = divmod_small(allBits4, 0xFFFF'FFFFU);
    STATIC_REQUIRE(byLargest.quotient == W4::from_limbs({ 1U, 1U, 1U, 1U }));
    STATIC_REQUIRE(byLargest.remainder == 0U);
    STATIC_REQUIRE(divmod_small(*pow10<4>(38), 10U).quotient == *pow10<4>(37));
    STATIC_REQUIRE(divmod_small(W4::from_u64(7), 9U).quotient.is_zero());
    STATIC_REQUIRE(divmod_small(W4::from_u64(7), 9U).remainder == 7U);

    // Against the binary divmod, on 20000 wide dividends and 32-bit divisors.
    Operands operands { 0x6A09'E667'F3BC'C909ULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::array<std::uint32_t, 4> dividendLimbs {};
        for (std::uint32_t& each: dividendLimbs)
            each = static_cast<std::uint32_t>(operands.draw() >> 32U);
        std::uint64_t const shaped = operands.draw();
        auto const divisorValue = static_cast<std::uint32_t>((shaped >> (shaped % 32U + 32U)) | 1U);
        W4 const dividendValue = W4::from_limbs(dividendLimbs);
        auto const shortSplit = divmod_small(dividendValue, divisorValue);
        auto const longSplit = divmod(dividendValue, W4::from_u64(divisorValue));
        if (shortSplit.quotient == longSplit.quotient && longSplit.remainder == W4::from_u64(shortSplit.remainder))
            ++agreed;
    }
    CHECK(agreed == 20000);
}

TEST_CASE("wide unsigned: shifts move whole limbs and parts of limbs", "[wide-int]")
{
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(1), 32) == W4::from_u64(0x1'0000'0000ULL));
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(1), 127) == topBit4);
    STATIC_REQUIRE(!shift_left_checked_or_none(W4::from_u64(1), 128).has_value());
    STATIC_REQUIRE(!shift_left_checked_or_none(topBit4, 1).has_value());
    STATIC_REQUIRE(shift_left_checked_or_none(W4 {}, 500)->is_zero());
    // 0x8000'0001 * 2^33 = 2^64 + 2^33.
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(0x8000'0001ULL), 33) == W4::from_limbs({ 0U, 2U, 1U, 0U }));
    STATIC_REQUIRE(shift_right(W4::from_limbs({ 0U, 2U, 1U, 0U }), 33) == W4::from_u64(0x8000'0001ULL));
    STATIC_REQUIRE(shift_right(topBit4, 127) == W4::from_u64(1));
    STATIC_REQUIRE(shift_right(W4::from_u64(0x1'0000'0000ULL), 32) == W4::from_u64(1));
    STATIC_REQUIRE(shift_right(allBits4, 128).is_zero());
    STATIC_REQUIRE(shift_right(allBits4, 0) == allBits4);
}

TEST_CASE("wide unsigned: division leaves a remainder below the divisor", "[wide-int]")
{
    // A divisor with its top bit set: the step that shifts the remainder past 2^128.
    constexpr auto atTop = divmod(allBits4, W4::from_limbs({ 1U, 0U, 0U, 0x8000'0000U }));
    STATIC_REQUIRE(atTop.quotient == W4::from_u64(1));
    STATIC_REQUIRE(atTop.remainder == W4::from_limbs({ 0xFFFF'FFFEU, 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0x7FFF'FFFFU }));
    constexpr auto byThree = divmod(allBits4, W4::from_u64(3));
    STATIC_REQUIRE(byThree.remainder.is_zero());
    STATIC_REQUIRE(byThree.quotient == W4::from_limbs({ 0x5555'5555U, 0x5555'5555U, 0x5555'5555U, 0x5555'5555U }));
    constexpr auto belowDivisor = divmod(W4::from_u64(7), W4::from_u64(9));
    STATIC_REQUIRE(belowDivisor.quotient.is_zero());
    STATIC_REQUIRE(belowDivisor.remainder == W4::from_u64(7));
    STATIC_REQUIRE(*pow10<4>(38) == *mul_checked_or_none(*pow10<4>(19), *pow10<4>(19)));
    STATIC_REQUIRE(!pow10<4>(39).has_value()); // 10^39 > 2^128
    STATIC_REQUIRE(divmod(*pow10<4>(38), *pow10<4>(19)).quotient == *pow10<4>(19));

    // Against 64-bit division, and back through the product, on 20000 pairs.
    Operands operands { 0x9E37'79B9'7F4A'7C15ULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::uint64_t const dividendValue = operands.draw();
        std::uint64_t const shaped = operands.draw();
        std::uint64_t const divisorValue = (shaped >> (shaped % 60U)) | 1U;
        auto const split = divmod(W4::from_u64(dividendValue), W4::from_u64(divisorValue));
        auto const product = mul_checked_or_none(W4::from_u64(dividendValue), W4::from_u64(divisorValue));
        auto const back = divmod(*product, W4::from_u64(divisorValue));
        if (split.quotient.to_u64() == dividendValue / divisorValue && split.remainder.to_u64() == dividendValue % divisorValue
            && back.quotient == W4::from_u64(dividendValue) && back.remainder.is_zero())
            ++agreed;
    }
    CHECK(agreed == 20000);

    // q * d + r == n with r < d, at 256 bits, on 2000 pairs of every size.
    int identities = 0;
    for (int drawn = 0; drawn < 2000; ++drawn)
    {
        std::array<std::uint32_t, 8> dividendLimbs {};
        std::array<std::uint32_t, 8> divisorLimbs {};
        std::size_t const divisorTop = static_cast<std::size_t>(operands.draw() % 8U) + 1U;
        for (std::size_t at = 0; at < 8; ++at)
        {
            dividendLimbs[at] = static_cast<std::uint32_t>(operands.draw() >> 32U);
            divisorLimbs[at] = at < divisorTop ? static_cast<std::uint32_t>(operands.draw() >> 32U) : 0U;
        }
        divisorLimbs[0] |= 1U;
        W8 const dividendValue = W8::from_limbs(dividendLimbs);
        W8 const divisorValue = W8::from_limbs(divisorLimbs);
        auto const split = divmod(dividendValue, divisorValue);
        auto const product = mul_checked_or_none(split.quotient, divisorValue);
        auto const rebuilt = product ? add_checked_or_none(*product, split.remainder) : std::nullopt;
        if (rebuilt && *rebuilt == dividendValue && split.remainder < divisorValue)
            ++identities;
    }
    CHECK(identities == 2000);
}

TEST_CASE("wide unsigned: gcd and lcm agree with 64-bit arithmetic", "[wide-int]")
{
    // 3 * 2^64 and 9 * 2^32: 3 * 2^32.
    STATIC_REQUIRE(gcd(W4::from_limbs({ 0U, 0U, 3U, 0U }), W4::from_limbs({ 0U, 9U, 0U, 0U })) == W4::from_limbs({ 0U, 3U, 0U, 0U }));
    STATIC_REQUIRE(gcd(W4 {}, W4::from_u64(12)) == W4::from_u64(12));
    STATIC_REQUIRE(gcd(W4::from_u64(12), W4 {}) == W4::from_u64(12));
    STATIC_REQUIRE(*lcm_checked_or_none(W4::from_u64(6), W4::from_u64(10)) == W4::from_u64(30));
    STATIC_REQUIRE(lcm_checked_or_none(W4 {}, W4::from_u64(10))->is_zero());
    STATIC_REQUIRE(!lcm_checked_or_none(topBit4, W4::from_u64(3)).has_value());

    Operands operands { 0x2545'F491'4F6C'DD1DULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::uint64_t const shapedLeft = operands.draw();
        std::uint64_t const shapedRight = operands.draw();
        std::uint64_t const leftValue = shapedLeft >> (shapedLeft % 40U);
        std::uint64_t const rightValue = (shapedRight >> (shapedRight % 40U)) * static_cast<std::uint64_t>(drawn % 7 + 1);
        if (gcd(W4::from_u64(leftValue), W4::from_u64(rightValue)).to_u64() == gcd(leftValue, rightValue))
            ++agreed;
    }
    CHECK(agreed == 20000);
}

TEST_CASE("wide unsigned: comparison reads the top limb first", "[wide-int]")
{
    STATIC_REQUIRE(W4::from_limbs({ 0U, 0U, 1U, 0U }) > W4::from_u64(allBits64));
    STATIC_REQUIRE(W4::from_limbs({ 0xFFFF'FFFFU, 0U, 0U, 0U }) < W4::from_limbs({ 0U, 1U, 0U, 0U }));
    STATIC_REQUIRE((W4::from_u64(5) <=> W4::from_u64(5)) == std::strong_ordering::equal);
}

TEST_CASE("wide unsigned: a signed value adds and multiplies as integers do", "[wide-int]")
{
    constexpr S4 minusSeven { true, W4::from_u64(7) };
    constexpr S4 five { false, W4::from_u64(5) };
    STATIC_REQUIRE(add_checked_or_none(minusSeven, five)->negative);
    STATIC_REQUIRE(add_checked_or_none(minusSeven, five)->magnitude == W4::from_u64(2));
    STATIC_REQUIRE(!add_checked_or_none(S4 { true, W4::from_u64(5) }, five)->negative); // zero is never negative
    STATIC_REQUIRE(add_checked_or_none(S4 { true, W4::from_u64(5) }, five)->magnitude.is_zero());
    STATIC_REQUIRE(sub_checked_or_none(five, minusSeven)->magnitude == W4::from_u64(12));
    STATIC_REQUIRE(!sub_checked_or_none(five, minusSeven)->negative);
    STATIC_REQUIRE(mul_checked_or_none(minusSeven, minusSeven)->magnitude == W4::from_u64(49));
    STATIC_REQUIRE(!mul_checked_or_none(minusSeven, minusSeven)->negative);
    STATIC_REQUIRE(!mul_checked_or_none(minusSeven, S4 {})->negative);
    STATIC_REQUIRE(!add_checked_or_none(S4 { false, topBit4 }, S4 { false, topBit4 }).has_value());

    // Against std::int64_t on operands below 2^30, where nothing overflows.
    Operands operands { 0x0123'4567'89AB'CDEFULL };
    int agreed = 0;
    auto const signedOf = [](S4 const& held) {
        auto const heldMagnitude = static_cast<std::int64_t>(*held.magnitude.to_u64());
        return held.negative ? -heldMagnitude : heldMagnitude;
    };
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        auto const leftValue = static_cast<std::int64_t>(operands.draw() >> 34U) - (std::int64_t { 1 } << 29);
        auto const rightValue = static_cast<std::int64_t>(operands.draw() >> 34U) - (std::int64_t { 1 } << 29);
        S4 const leftWide { leftValue < 0, W4::from_u64(formula::detail::magnitude(leftValue)) };
        S4 const rightWide { rightValue < 0, W4::from_u64(formula::detail::magnitude(rightValue)) };
        auto const added = add_checked_or_none(leftWide, rightWide);
        auto const subtracted = sub_checked_or_none(leftWide, rightWide);
        auto const multiplied = mul_checked_or_none(leftWide, rightWide);
        if (signedOf(*added) == leftValue + rightValue && signedOf(*subtracted) == leftValue - rightValue
            && signedOf(*multiplied) == leftValue * rightValue && !(added->magnitude.is_zero() && added->negative))
            ++agreed;
    }
    CHECK(agreed == 20000);
}
```

- [ ] **Step 2: Add the test file to the build and see it fail.** In `test/CMakeLists.txt`, after line 96
  (`format_tests.cpp`), add `    wide_int_tests.cpp`. Run
  `pwsh -NoProfile -File ...\scratchpad\cl.ps1 -Filter "wide unsigned"`.
  Expected: `BUILD FAILED`, `C1083: Cannot open include file: 'formula-cpp/detail/wide_int.hpp'`.

- [ ] **Step 3: Implement `include/formula-cpp/detail/wide_int.hpp`.** Every function documented. The code:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Fixed-width unsigned integers wider than 64 bits, for the exact arithmetic
/// behind a declared precision (`rounded_output`, `opaque.hpp`): a value the
/// 64-bit `Rational` cannot hold is computed here exactly, and only its
/// rounding is ever written.
///
/// `WideUnsigned<Limbs>` holds `Limbs` limbs of 32 bits, least significant
/// first. Every product of two limbs is formed in `std::uint64_t`: cl has no
/// 128-bit integer (`rounded_root.hpp` gives the same reason), and nothing here
/// uses an intrinsic or floating point, so a result depends on its operands
/// alone.
///
/// **Overflow is reported, never wrapped.** Every operation that can leave the
/// width answers `std::optional`, empty when it would, and is named
/// `*_checked_or_none` as the `Int` helpers of `checked_int.hpp` are;
/// `sub_checked_or_none` is empty for a negative difference too. Unlike
/// `checked_add(Rational, Rational)` and its siblings, which answer
/// `std::expected<Rational, ArithmeticError>`, each of these has one way to
/// fail, and its caller names the error -- `ArithmeticError::Overflow`
/// throughout this library.
///
/// No shift here is by 32 or more on a 32-bit limb, which would be undefined
/// behaviour: shift counts are split into whole limbs and a part below 32.
///
/// A signed integer is a sign beside a magnitude (`WideSigned`), and a fraction
/// a sign beside two magnitudes (`WideRatio`). Zero is never negative.

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace formula::detail
{

/// An unsigned integer of `32 * Limbs` bits, zero when default-constructed. A
/// value type with no arithmetic operators: every operation that can overflow
/// is a free function below, answering `std::optional`, so that no expression
/// wraps silently.
template <std::size_t Limbs>
    requires(Limbs >= 2)
class WideUnsigned
{
  public:
    /// How many bits it holds.
    static constexpr std::size_t bits = 32 * Limbs;

    /// Zero.
    constexpr WideUnsigned() noexcept = default;

    /// @p narrow, exactly: two limbs always hold it.
    [[nodiscard]] static constexpr WideUnsigned from_u64(std::uint64_t narrow) noexcept
    {
        std::array<std::uint32_t, Limbs> held {};
        held[0] = static_cast<std::uint32_t>(narrow & 0xFFFF'FFFFU);
        held[1] = static_cast<std::uint32_t>(narrow >> 32U);
        return from_limbs(held);
    }

    /// The integer whose limbs are @p held, least significant first.
    [[nodiscard]] static constexpr WideUnsigned from_limbs(std::array<std::uint32_t, Limbs> const& held) noexcept
    {
        WideUnsigned made;
        made._limbs = held;
        return made;
    }

    /// Every limb, least significant first.
    [[nodiscard]] constexpr std::array<std::uint32_t, Limbs> const& limbs() const noexcept { return _limbs; }

    /// This value as a `std::uint64_t`, or nothing when it needs more than 64 bits.
    [[nodiscard]] constexpr std::optional<std::uint64_t> to_u64() const noexcept
    {
        for (std::size_t at = 2; at < Limbs; ++at)
            if (_limbs[at] != 0)
                return std::nullopt;
        return (static_cast<std::uint64_t>(_limbs[1]) << 32U) | _limbs[0];
    }

    /// Whether this is zero.
    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
        for (std::uint32_t const each: _limbs)
            if (each != 0)
                return false;
        return true;
    }

    /// How many bits writing it takes: 0 for zero, 1 for one, 65 for 2^64.
    [[nodiscard]] constexpr std::size_t bit_length() const noexcept
    {
        for (std::size_t at = Limbs; at > 0; --at)
        {
            std::uint32_t topLimb = _limbs[at - 1];
            if (topLimb == 0)
                continue;
            std::size_t topWidth = 0;
            while (topLimb != 0)
            {
                topLimb >>= 1U;
                ++topWidth;
            }
            return 32 * (at - 1) + topWidth;
        }
        return 0;
    }

    /// Limb @p at, counted from the least significant; zero past the top, so
    /// that a caller reading one limb beyond needs no bounds check of its own.
    [[nodiscard]] constexpr std::uint32_t limb(std::size_t at) const noexcept { return at < Limbs ? _limbs[at] : 0U; }

    /// Limb-wise equality, which for this representation is value equality.
    friend constexpr bool operator==(WideUnsigned const&, WideUnsigned const&) noexcept = default;

    /// The values' order, read from the most significant limb down.
    friend constexpr std::strong_ordering operator<=>(WideUnsigned const& leftOperand,
                                                      WideUnsigned const& rightOperand) noexcept
    {
        for (std::size_t at = Limbs; at > 0; --at)
            if (leftOperand._limbs[at - 1] != rightOperand._limbs[at - 1])
                return leftOperand._limbs[at - 1] <=> rightOperand._limbs[at - 1];
        return std::strong_ordering::equal;
    }

  private:
    std::array<std::uint32_t, Limbs> _limbs {};
};
```

  The free functions, in this order (each takes and returns what the Interfaces block says; parameter names
  as shown, all clear of the consumer globals):

```cpp
/// @p augend plus @p addend, or nothing when the sum leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> add_checked_or_none(WideUnsigned<L> const& augend,
                                                                   WideUnsigned<L> const& addend) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t carry = 0;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const limbSum = std::uint64_t { augend.limb(at) } + addend.limb(at) + carry;
        held[at] = static_cast<std::uint32_t>(limbSum & 0xFFFF'FFFFU);
        carry = limbSum >> 32U;
    }
    if (carry != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p minuend less @p subtrahend, or nothing when that is negative.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> sub_checked_or_none(WideUnsigned<L> const& minuend,
                                                                   WideUnsigned<L> const& subtrahend) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t borrow = 0;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const taken = std::uint64_t { subtrahend.limb(at) } + borrow;
        std::uint64_t const fromLimb = minuend.limb(at);
        // Modular, so well defined; masked to the limb.
        held[at] = static_cast<std::uint32_t>((fromLimb - taken) & 0xFFFF'FFFFU);
        borrow = fromLimb < taken ? 1U : 0U;
    }
    if (borrow != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p multiplicand times @p multiplier, schoolbook, or nothing when the
/// product leaves the width. A limb product plus a limb plus a carry is at
/// most 2^64 - 1, so each step fits `std::uint64_t`. A zero limb of
/// @p multiplicand is skipped whole, which is what keeps a product with a
/// small or sparse left operand cheap in a constant evaluation; the answer
/// is the same in either operand order.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> mul_checked_or_none(WideUnsigned<L> const& multiplicand,
                                                                   WideUnsigned<L> const& multiplier) noexcept
{
    std::array<std::uint32_t, L> held {};
    for (std::size_t outer = 0; outer < L; ++outer)
    {
        std::uint64_t const multiplicandLimb = multiplicand.limb(outer);
        if (multiplicandLimb == 0)
            continue;
        std::uint64_t carry = 0;
        for (std::size_t inner = 0; inner + outer < L; ++inner)
        {
            std::uint64_t const term = multiplicandLimb * multiplier.limb(inner) + held[inner + outer] + carry;
            held[inner + outer] = static_cast<std::uint32_t>(term & 0xFFFF'FFFFU);
            carry = term >> 32U;
        }
        if (carry != 0)
            return std::nullopt;
        // A non-zero limb of the multiplier that would land past the top.
        for (std::size_t inner = L - outer; inner < L; ++inner)
            if (multiplier.limb(inner) != 0)
                return std::nullopt;
    }
    return WideUnsigned<L>::from_limbs(held);
}
```

  `mul_small_checked_or_none(multiplicand, std::uint32_t smallFactor)` and `add_small_checked_or_none(augend,
  std::uint32_t smallAddend)`: one carry chain each (`term = limb * smallFactor + carry`; `limbSum = limb + carry` with carry
  starting at `smallAddend`), empty when the final carry is non-zero.

```cpp
/// @p shifted times 2^@p shiftCount, or nothing when a set bit would leave the
/// width. Zero shifts to zero by any count.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> shift_left_checked_or_none(WideUnsigned<L> const& shifted,
                                                                          std::size_t shiftCount) noexcept
{
    if (shifted.is_zero())
        return shifted;
    if (shiftCount >= WideUnsigned<L>::bits || shifted.bit_length() + shiftCount > WideUnsigned<L>::bits)
        return std::nullopt;
    std::array<std::uint32_t, L> held {};
    std::size_t const wholeLimbs = shiftCount / 32;
    auto const bitShift = static_cast<unsigned>(shiftCount % 32);
    for (std::size_t at = L; at > wholeLimbs; --at)
    {
        std::size_t const fromAt = at - 1 - wholeLimbs;
        std::uint32_t moved = shifted.limb(fromAt) << bitShift;
        if (bitShift != 0 && fromAt > 0)
            moved |= shifted.limb(fromAt - 1) >> (32U - bitShift);
        held[at - 1] = moved;
    }
    return WideUnsigned<L>::from_limbs(held);
}

/// @p shifted divided by 2^@p shiftCount, truncated: zero from the width on.
template <std::size_t L>
[[nodiscard]] constexpr WideUnsigned<L> shift_right(WideUnsigned<L> const& shifted, std::size_t shiftCount) noexcept
{
    std::array<std::uint32_t, L> held {};
    if (shiftCount >= WideUnsigned<L>::bits)
        return WideUnsigned<L>::from_limbs(held);
    std::size_t const wholeLimbs = shiftCount / 32;
    auto const bitShift = static_cast<unsigned>(shiftCount % 32);
    for (std::size_t at = 0; at + wholeLimbs < L; ++at)
    {
        std::uint32_t moved = shifted.limb(at + wholeLimbs) >> bitShift;
        if (bitShift != 0)
            moved |= static_cast<std::uint32_t>(shifted.limb(at + wholeLimbs + 1) << (32U - bitShift));
        held[at] = moved;
    }
    return WideUnsigned<L>::from_limbs(held);
}

/// A quotient and its remainder: `divmod`'s answer.
template <std::size_t L>
struct WideDivision
{
    /// The quotient, truncated.
    WideUnsigned<L> quotient;
    /// What is left, always below the divisor.
    WideUnsigned<L> remainder;
};

/// @p dividend divided by @p divisor, by binary long division: one bit of the
/// dividend at a time into the remainder, the divisor subtracted whenever it
/// fits. Simple, and obviously right: `remainder < divisor` holds before each
/// step. When the divisor's top bit is set, the shifted remainder can pass
/// 2^bits -- the bit shifted out says so -- and remainder minus divisor is then
/// below the divisor, so the modular subtraction is exact.
///
/// @pre @p divisor is not zero; every caller establishes it.
template <std::size_t L>
[[nodiscard]] constexpr WideDivision<L> divmod(WideUnsigned<L> const& dividend, WideUnsigned<L> const& divisor) noexcept
{
    std::array<std::uint32_t, L> quotientLimbs {};
    std::array<std::uint32_t, L> remainderLimbs {};
    for (std::size_t bitsLeft = dividend.bit_length(); bitsLeft > 0; --bitsLeft)
    {
        std::size_t const bitAt = bitsLeft - 1;
        bool const carriedOut = (remainderLimbs[L - 1] >> 31U) != 0;
        for (std::size_t at = L; at > 1; --at)
            remainderLimbs[at - 1] = (remainderLimbs[at - 1] << 1U) | (remainderLimbs[at - 2] >> 31U);
        remainderLimbs[0] = (remainderLimbs[0] << 1U) | ((dividend.limb(bitAt / 32) >> (bitAt % 32)) & 1U);
        if (carriedOut || !(WideUnsigned<L>::from_limbs(remainderLimbs) < divisor))
        {
            std::uint64_t borrow = 0;
            for (std::size_t at = 0; at < L; ++at)
            {
                std::uint64_t const taken = std::uint64_t { divisor.limb(at) } + borrow;
                std::uint64_t const fromLimb = remainderLimbs[at];
                remainderLimbs[at] = static_cast<std::uint32_t>((fromLimb - taken) & 0xFFFF'FFFFU);
                borrow = fromLimb < taken ? 1U : 0U;
            }
            quotientLimbs[bitAt / 32] |= std::uint32_t { 1 } << (bitAt % 32);
        }
    }
    return WideDivision<L> { WideUnsigned<L>::from_limbs(quotientLimbs), WideUnsigned<L>::from_limbs(remainderLimbs) };
}

/// A quotient and its remainder below a 32-bit divisor: `divmod_small`'s answer.
template <std::size_t L>
struct WideSmallDivision
{
    /// The quotient, truncated.
    WideUnsigned<L> quotient;
    /// What is left, below the divisor.
    std::uint32_t remainder;
};

/// @p dividend divided by the 32-bit @p divisor, one limb at a time from the
/// top: each step divides `carried * 2^32 + limb`, which is below
/// `divisor * 2^32` and so fits `std::uint64_t`. Far cheaper than `divmod`
/// in a constant evaluation -- one step per limb, not per bit.
///
/// @pre @p divisor is not zero; every caller establishes it.
template <std::size_t L>
[[nodiscard]] constexpr WideSmallDivision<L> divmod_small(WideUnsigned<L> const& dividend, std::uint32_t divisor) noexcept
{
    std::array<std::uint32_t, L> quotientLimbs {};
    std::uint64_t carried = 0;
    for (std::size_t at = L; at > 0; --at)
    {
        std::uint64_t const partial = (carried << 32U) | dividend.limb(at - 1);
        quotientLimbs[at - 1] = static_cast<std::uint32_t>(partial / divisor);
        carried = partial % divisor;
    }
    return WideSmallDivision<L> { WideUnsigned<L>::from_limbs(quotientLimbs), static_cast<std::uint32_t>(carried) };
}

/// The greatest common divisor, by the binary algorithm: `gcd(0, n) == n`.
template <std::size_t L>
[[nodiscard]] constexpr WideUnsigned<L> gcd(WideUnsigned<L> leftOperand, WideUnsigned<L> rightOperand) noexcept
{
    if (leftOperand.is_zero())
        return rightOperand;
    if (rightOperand.is_zero())
        return leftOperand;
    std::size_t sharedTwos = 0;
    while (((leftOperand.limb(0) | rightOperand.limb(0)) & 1U) == 0)
    {
        leftOperand = shift_right(leftOperand, 1);
        rightOperand = shift_right(rightOperand, 1);
        ++sharedTwos;
    }
    while ((leftOperand.limb(0) & 1U) == 0)
        leftOperand = shift_right(leftOperand, 1);
    while (!rightOperand.is_zero())
    {
        while ((rightOperand.limb(0) & 1U) == 0)
            rightOperand = shift_right(rightOperand, 1);
        if (rightOperand < leftOperand)
        {
            WideUnsigned<L> const smaller = rightOperand;
            rightOperand = leftOperand;
            leftOperand = smaller;
        }
        rightOperand = *sub_checked_or_none(rightOperand, leftOperand);
    }
    // The common factor of two was taken out of both, so it fits back in.
    return *shift_left_checked_or_none(leftOperand, sharedTwos);
}
```

  `lcm_checked_or_none(leftOperand, rightOperand)`: zero when either is zero, else
  `mul_checked_or_none(divmod(leftOperand, gcd(leftOperand, rightOperand)).quotient, rightOperand)`.
  `pow10<L>(std::size_t exponent)`: start at `from_u64(1)`, `mul_small_checked_or_none(..., 10U)` `exponent` times,
  stopping empty on overflow. `WideRatio<L>` exactly as the contract states it, each member documented
  ("the denominator is not zero; the fraction need not be in lowest terms; zero is never negative").
  `WideSigned<L> { bool negative = false; WideUnsigned<L> magnitude; }` and its three overloads:

```cpp
/// @p leftOperand plus @p rightOperand, signs included; nothing on overflow.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> add_checked_or_none(WideSigned<L> const& leftOperand,
                                                                 WideSigned<L> const& rightOperand) noexcept
{
    if (leftOperand.negative == rightOperand.negative)
    {
        std::optional<WideUnsigned<L>> const together = add_checked_or_none(leftOperand.magnitude, rightOperand.magnitude);
        if (!together)
            return std::nullopt;
        return WideSigned<L> { leftOperand.negative && !together->is_zero(), *together };
    }
    bool const leftLarger = !(leftOperand.magnitude < rightOperand.magnitude);
    WideUnsigned<L> const apart = leftLarger ? *sub_checked_or_none(leftOperand.magnitude, rightOperand.magnitude)
                                             : *sub_checked_or_none(rightOperand.magnitude, leftOperand.magnitude);
    return WideSigned<L> { (leftLarger ? leftOperand.negative : rightOperand.negative) && !apart.is_zero(), apart };
}
```

  `sub_checked_or_none(WideSigned)`: `add_checked_or_none(leftOperand, WideSigned<L> { !rightOperand.negative
  && !rightOperand.magnitude.is_zero(), rightOperand.magnitude })`. `mul_checked_or_none(WideSigned)`: the
  magnitudes' `mul_checked_or_none`, negative when the signs differ and the product is not zero.

  `checked_int.hpp` is not touched: its comment at `:113-116` keeps the `checked_` prefix for `std::expected`,
  and these follow its own `std::optional` helpers' `*_checked_or_none` names.

- [ ] **Step 4: FILE_SET.** In the root `CMakeLists.txt`, after line 50 (`detail/type_list.hpp`), add
  `        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/wide_int.hpp"`.

- [ ] **Step 5: Run and pass.** `cl.ps1 -Filter "wide unsigned|hygiene"` then `gcc14.sh "wide unsigned|hygiene"`:
  9 `wide unsigned:` cases pass on both, every `hygiene.*` passes. Then the full suite on both (no filter):
  1808 + 9 pass. g++-14 must be clean under `-Wconversion -Wshadow`; fix a warning at its source, never with a
  pragma.

- [ ] **Step 6: Mutation checks.** (a) Delete the `carriedOut ||` term in `divmod` and rebuild: the `atTop` case
  must fail (it is the only case that reaches it). (b) In `mul_checked_or_none`, drop the loop that refuses a
  multiplier limb landing past the top: the `2^96 times 2^64` cases fail. (c) In `divmod_small`, start
  `carried` at the dividend's top limb instead of 0: the short-division case fails. Restore each with a plain
  write and rebuild green.

- [ ] **Step 7: Commit.**
  `git add include/formula-cpp/detail/wide_int.hpp test/wide_int_tests.cpp test/CMakeLists.txt CMakeLists.txt`
  `git commit -m "feat(detail): fixed-width unsigned integers for exact arithmetic past 64 bits" -m "A value the 64-bit Rational cannot hold -- a least-squares slope through many readings -- still has an exact decimal it rounds to, and finding it needs integers wider than 64 bits. WideUnsigned holds 32-bit limbs and forms every product in std::uint64_t, since cl has no 128-bit integer; every operation that can leave the width answers an empty std::optional instead of wrapping, named *_checked_or_none as the 64-bit helpers beside it are, and no shift is by 32 or more. Short division by a 32-bit divisor goes limb by limb, and a product skips its left operand's zero limbs, which keeps both cheap in a constant evaluation. Signed values and fractions are a sign beside magnitudes. Internal to the library." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 2: Exact rounding of a wide fraction (`detail/wide_rounding.hpp`)

**Files:**
- Create: `include/formula-cpp/detail/wide_rounding.hpp`
- Create: `test/wide_rounding_tests.cpp`
- Modify: `CMakeLists.txt` FILE_SET (add `detail/wide_rounding.hpp` after `detail/wide_int.hpp`)
- Modify: `test/CMakeLists.txt` (add `wide_rounding_tests.cpp` after `wide_int_tests.cpp`)

**Interfaces:**
- Consumes (Task 1): `WideUnsigned`, `WideSigned`, `WideRatio`, `mul_checked_or_none`, `sub_checked_or_none`,
  `add_small_checked_or_none`, `divmod`, `gcd`, `pow10<L>`.
- Produces (namespace `formula::detail`, all `constexpr noexcept`):
  - `template <std::size_t L> WideRatio<L> reduced(WideRatio<L> const& unreduced)` -- lowest terms, zero as 0/1.
  - `template <std::size_t L> requires(L >= 4) WideRatio<L> wide_from_rational(Rational exact)`
  - `template <std::size_t L> std::optional<WideSigned<L>> scaled_to_denominator(Rational exact, WideUnsigned<L> const& commonDenominator)`
  - `template <std::size_t L> std::expected<Rational, ArithmeticError> round_wide_ratio(WideRatio<L> const& unrounded, DecimalPlaces places, RoundingMode roundingMode)`
  - `template <std::size_t L> std::expected<Rational, ArithmeticError> narrow_wide_ratio(WideRatio<L> const& unreduced)`
  - `template <std::size_t L> std::expected<Rational, ArithmeticError> rounded_in_unit(WideRatio<L> const& coherentValue, Unit const& roundedIn, DecimalPlaces places, RoundingMode roundingMode)`
  - `template <std::size_t L> std::expected<Rational, ArithmeticError> decide_rounding(WideRatio<L> const& lowerBound, WideRatio<L> const& upperBound, DecimalPlaces places, RoundingMode roundingMode)`

- [ ] **Step 1: Write the failing tests** -- `test/wide_rounding_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/rounding_node.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using W4 = formula::detail::WideUnsigned<4>;
using R4 = formula::detail::WideRatio<4>;
using formula::detail::decide_rounding;
using formula::detail::narrow_wide_ratio;
using formula::detail::round_wide_ratio;
using formula::detail::rounded_in_unit;
using formula::detail::wide_from_rational;

constexpr std::array everyMode { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven,
                                 RoundingMode::Ceiling,          RoundingMode::Floor,          RoundingMode::TowardZero,
                                 RoundingMode::AwayFromZero };
// Signs, halves, zero, and denominators with and without factors of 2 and 5,
// up to 4 * 10^18: the grid the agreement with checked_round is judged on.
constexpr std::array<std::int64_t, 17> gridNumerators { -2001, -1000, -999, -501, -500, -499, -125, -1, 0,
                                                        1,     125,   499,  500,  501,  999,  1000, 2001 };
constexpr std::array<std::int64_t, 11> gridDenominators { 1,    2,    3,    7,    8, 40, 125, 1000,
                                                          1024, 999'999'999'989, 4'000'000'000'000'000'000 };

[[nodiscard]] constexpr R4 from(Rational exact) noexcept
{
    return wide_from_rational<4>(exact);
}
} // namespace

TEST_CASE("wide rounding: a wide fraction rounds as checked_round rounds the same Rational", "[wide-rounding]")
{
    // Wherever checked_round answers, round_wide_ratio gives the same, in
    // every mode and at every place it accepts. Counted: 43729 cases, the
    // number a probe of this algorithm measured on cl 19.51.
    int checkedCases = 0;
    int agreedCases = 0;
    auto const judge = [&](Rational exact) {
        for (int places = -18; places <= 18; ++places)
            for (RoundingMode const each: everyMode)
            {
                std::expected<Rational, formula::ArithmeticError> const expected =
                    formula::checked_round(exact, DecimalPlaces { places }, each);
                if (!expected.has_value())
                    continue;
                ++checkedCases;
                std::expected<Rational, formula::ArithmeticError> const wide =
                    round_wide_ratio(from(exact), DecimalPlaces { places }, each);
                if (wide.has_value() && *wide == *expected)
                    ++agreedCases;
            }
    };
    for (std::int64_t const numeratorValue: gridNumerators)
        for (std::int64_t const denominatorValue: gridDenominators)
            judge(Rational { numeratorValue, denominatorValue });
    for (Rational const extreme: { Rational { formula::detail::IntMin }, Rational { formula::detail::IntMax },
                                   Rational { formula::detail::IntMin, 3 }, Rational { formula::detail::IntMax, 2 } })
        judge(extreme);
    CHECK(checkedCases == 43729);
    CHECK(agreedCases == checkedCases);
}

TEST_CASE("wide rounding: ties follow the mode and the sign", "[wide-rounding]")
{
    // 19/2 = 9.5 and -9.5 at 0 dp: every mode tells a tie apart.
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfEven) == Rational { 10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero) == Rational { 10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfTowardZero) == Rational { 9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfEven) == Rational { -10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfTowardZero) == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::Floor) == Rational { -10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::Ceiling) == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::TowardZero) == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::AwayFromZero) == Rational { -10 });
    // 19/28 at 4 dp, the fitted slope: 0.678571...
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 28 }), DecimalPlaces { 4 }, RoundingMode::HalfEven) == Rational { 3393, 5000 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 28 }), DecimalPlaces { 4 }, RoundingMode::Floor) == Rational { 1357, 2000 });
    // Negative places: 125 to tens.
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 125 }), DecimalPlaces { -1 }, RoundingMode::HalfEven) == Rational { 120 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 125 }), DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero) == Rational { 130 });
}

TEST_CASE("wide rounding: a fraction wider than 64 bits rounds exactly", "[wide-rounding]")
{
    // (10^30 + 1) / 10^30 at 6 dp: 1, or one step up under Ceiling.
    constexpr W4 tenToThirty = *formula::detail::pow10<4>(30);
    constexpr R4 justAboveOne { false, *formula::detail::add_small_checked_or_none(tenToThirty, 1U), tenToThirty };
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::HalfEven) == Rational { 1 });
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::Floor) == Rational { 1 });
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::Ceiling) == Rational { 1'000'001, 1'000'000 });
    // 2^70 / 2^60, unreduced: 1024.
    constexpr R4 wideQuotient { false, *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 70),
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 60) };
    STATIC_REQUIRE(*round_wide_ratio(wideQuotient, DecimalPlaces { 0 }, RoundingMode::HalfEven) == Rational { 1024 });
    // 6/4, unreduced: 1.5 exactly at 1 dp.
    STATIC_REQUIRE(*round_wide_ratio(R4 { false, W4::from_u64(6), W4::from_u64(4) }, DecimalPlaces { 1 }, RoundingMode::HalfEven)
                   == Rational { 3, 2 });
    // 3 * 2^100 over 2^102 is 3/4. Scaled by 10^18 unreduced it needs 162
    // bits; reduced first it fits: the case that pins the reduction.
    constexpr R4 farFromLowest { false, *formula::detail::shift_left_checked_or_none(W4::from_u64(3), 100),
                                 *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 102) };
    STATIC_REQUIRE(*round_wide_ratio(farFromLowest, DecimalPlaces { 18 }, RoundingMode::HalfEven) == Rational { 3, 4 });
    // 3 * 2^118 over 2^120, 3/4 m again: times the millimetre's 1000 unreduced
    // it needs 130 bits, so rounded_in_unit must reduce before it converts.
    constexpr R4 wideInMetres { false, *formula::detail::shift_left_checked_or_none(W4::from_u64(3), 118),
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 120) };
    STATIC_REQUIRE(*rounded_in_unit(wideInMetres, unit::Millimetre, DecimalPlaces { 12 }, RoundingMode::HalfEven) == Rational { 3, 4 });
}

TEST_CASE("wide rounding: the kept integer must fit Rational and the places must be in range", "[wide-rounding]")
{
    constexpr W4 twoToSixtyThree = W4::from_u64(std::uint64_t { 1 } << 63);
    // -2^63 is IntMin; +2^63 is one past IntMax.
    STATIC_REQUIRE(*round_wide_ratio(R4 { true, twoToSixtyThree, W4::from_u64(1) }, DecimalPlaces { 0 }, RoundingMode::HalfEven)
                   == Rational { formula::detail::IntMin });
    STATIC_REQUIRE(round_wide_ratio(R4 { false, twoToSixtyThree, W4::from_u64(1) }, DecimalPlaces { 0 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(round_wide_ratio(R4 { false, *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 70),
                                         W4::from_u64(1) },
                                    DecimalPlaces { -18 }, RoundingMode::HalfEven)
                       .error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(round_wide_ratio(from(Rational { 1, 3 }), DecimalPlaces { 19 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(round_wide_ratio(from(Rational { 1, 3 }), DecimalPlaces { -19 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(round_wide_ratio(R4 { false, W4::from_u64(1), W4 {} }, DecimalPlaces { 0 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::DivisionByZero);
    // Zero rounds to zero, never to a negative zero.
    STATIC_REQUIRE(*round_wide_ratio(R4 { true, W4 {}, W4::from_u64(7) }, DecimalPlaces { 2 }, RoundingMode::Floor) == Rational { 0 });
}

TEST_CASE("wide rounding: narrow_wide_ratio is the exact value or Overflow", "[wide-rounding]")
{
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(6), W4::from_u64(4) }) == Rational { 3, 2 });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { true, W4::from_u64(std::uint64_t { 1 } << 63), W4::from_u64(1) })
                   == Rational { formula::detail::IntMin });
    STATIC_REQUIRE(narrow_wide_ratio(R4 { false, W4::from_u64(std::uint64_t { 1 } << 63), W4::from_u64(1) }).error()
                   == formula::ArithmeticError::Overflow);
    // 2^64 / (3 * 2^64): reduced first, 1/3.
    constexpr W4 twoToSixtyFour = W4::from_limbs({ 0U, 0U, 1U, 0U });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, twoToSixtyFour,
                                           *formula::detail::mul_small_checked_or_none(twoToSixtyFour, 3U) })
                   == Rational { 1, 3 });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { true, W4 {}, W4::from_u64(5) }) == Rational { 0 });
}

TEST_CASE("wide rounding: rounded_in_unit rounds in its unit as a rounding node does", "[wide-rounding]")
{
    // 19/28000 m/s is 285/7 mm/min: 40.71 at 2 dp, 40.72 under Ceiling, back in m/s.
    STATIC_REQUIRE(*rounded_in_unit(from(Rational { 19, 28'000 }), unit::MillimetrePerMinute, DecimalPlaces { 2 }, RoundingMode::HalfEven)
                   == Rational { 1357, 2'000'000 });
    STATIC_REQUIRE(*rounded_in_unit(from(Rational { 19, 28'000 }), unit::MillimetrePerMinute, DecimalPlaces { 2 }, RoundingMode::Ceiling)
                   == Rational { 509, 750'000 });
    // An offset or a zero magnitude is refused, as checked_convert refuses a zero one.
    STATIC_REQUIRE(rounded_in_unit(from(Rational { 300 }), unit::Celsius, DecimalPlaces { 1 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::DomainError);
    constexpr formula::Unit noScale { .dimension = formula::dim::Length, .magnitudeNumerator = 0 };
    STATIC_REQUIRE(rounded_in_unit(from(Rational { 3 }), noScale, DecimalPlaces { 1 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::DomainError);

    // Wherever RepRounding<Rational>::round_in answers, the same result, over
    // units whose factor is and is not a power of ten. Counted: 64582 cases.
    std::array<formula::Unit, 5> const units { unit::Gram, unit::Millimetre, unit::MillimetrePerMinute, unit::Minute, unit::Percent };
    int checkedCases = 0;
    int agreedCases = 0;
    for (formula::Unit const& roundedIn: units)
        for (std::int64_t const numeratorValue: gridNumerators)
            for (std::int64_t const denominatorValue: gridDenominators)
            {
                Rational const coherentValue { numeratorValue, denominatorValue };
                for (int places = -3; places <= 6; ++places)
                    for (RoundingMode const each: everyMode)
                    {
                        auto const expected = formula::RepRounding<Rational>::round_in(coherentValue, roundedIn, DecimalPlaces { places }, each);
                        if (!expected.has_value())
                            continue;
                        ++checkedCases;
                        auto const wide = rounded_in_unit(from(coherentValue), roundedIn, DecimalPlaces { places }, each);
                        if (wide.has_value() && *wide == *expected)
                            ++agreedCases;
                    }
            }
    CHECK(checkedCases == 64582);
    CHECK(agreedCases == checkedCases);
}

TEST_CASE("wide rounding: decide_rounding answers only when both bounds round alike", "[wide-rounding]")
{
    struct Expected
    {
        RoundingMode mode;
        std::optional<Rational> answer; // empty: Overflow
    };
    auto const bounds = [](std::int64_t lowerNumerator, std::int64_t upperNumerator) {
        return std::array { from(Rational { lowerNumerator, 100'000'000 }), from(Rational { upperNumerator, 100'000'000 }) };
    };
    auto const judge = [](std::array<R4, 2> const& enclosure, std::array<Expected, 7> const& table) {
        for (Expected const& row: table)
        {
            auto const decided = decide_rounding(enclosure[0], enclosure[1], DecimalPlaces { 4 }, row.mode);
            INFO("mode " << static_cast<int>(row.mode));
            if (row.answer.has_value())
            {
                REQUIRE(decided.has_value());
                CHECK(*decided == *row.answer);
            }
            else
            {
                REQUIRE(!decided.has_value());
                CHECK(decided.error() == formula::ArithmeticError::Overflow);
            }
        }
    };
    Rational const low { 7071, 5000 };      // 1.4142
    Rational const high { 14143, 10000 };   // 1.4143
    Rational const minusLow { -7071, 5000 };
    Rational const minusHigh { -14143, 10000 };
    // [1.41421356, 1.41421357]: tight, every mode decides.
    judge(bounds(141421356, 141421357),
          { Expected { RoundingMode::HalfAwayFromZero, low }, Expected { RoundingMode::HalfTowardZero, low },
            Expected { RoundingMode::HalfEven, low }, Expected { RoundingMode::Ceiling, high }, Expected { RoundingMode::Floor, low },
            Expected { RoundingMode::TowardZero, low }, Expected { RoundingMode::AwayFromZero, high } });
    // [1.41424999, 1.41425001] straddles the half: the half modes cannot decide.
    judge(bounds(141424999, 141425001),
          { Expected { RoundingMode::HalfAwayFromZero, std::nullopt }, Expected { RoundingMode::HalfTowardZero, std::nullopt },
            Expected { RoundingMode::HalfEven, std::nullopt }, Expected { RoundingMode::Ceiling, high },
            Expected { RoundingMode::Floor, low }, Expected { RoundingMode::TowardZero, low },
            Expected { RoundingMode::AwayFromZero, high } });
    // [1.41419999, 1.41420001] straddles a grid point: the directed modes cannot.
    judge(bounds(141419999, 141420001),
          { Expected { RoundingMode::HalfAwayFromZero, low }, Expected { RoundingMode::HalfTowardZero, low },
            Expected { RoundingMode::HalfEven, low }, Expected { RoundingMode::Ceiling, std::nullopt },
            Expected { RoundingMode::Floor, std::nullopt }, Expected { RoundingMode::TowardZero, std::nullopt },
            Expected { RoundingMode::AwayFromZero, std::nullopt } });
    // The negative enclosure of the half: the sign decides the directed modes.
    judge(bounds(-141425001, -141424999),
          { Expected { RoundingMode::HalfAwayFromZero, std::nullopt }, Expected { RoundingMode::HalfTowardZero, std::nullopt },
            Expected { RoundingMode::HalfEven, std::nullopt }, Expected { RoundingMode::Ceiling, minusLow },
            Expected { RoundingMode::Floor, minusHigh }, Expected { RoundingMode::TowardZero, minusLow },
            Expected { RoundingMode::AwayFromZero, minusHigh } });
}

TEST_CASE("wide rounding: a Rational scaled to a common denominator keeps its sign", "[wide-rounding]")
{
    constexpr W4 twelve = W4::from_u64(12);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { -3, 4 }, twelve)->negative);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { -3, 4 }, twelve)->magnitude == W4::from_u64(9));
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { 5, 6 }, twelve)->magnitude == W4::from_u64(10));
    STATIC_REQUIRE(!formula::detail::scaled_to_denominator(Rational { 0 }, twelve)->negative);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { 0 }, twelve)->magnitude.is_zero());
}
```

  (Each expected value above is from the probe or checked by hand: 19/28 = 0.678571...; 285/7 = 40.714285...
  mm/min, so 40.71 is `4071/6000000` m/s = `1357/2000000`, 40.72 = `509/750000`; the four enclosure tables were
  computed in Python `fractions`.)

- [ ] **Step 2: Add the file to `test/CMakeLists.txt`** after `wide_int_tests.cpp`; run
  `cl.ps1 -Filter "wide rounding"`. Expected: `BUILD FAILED`, `C1083` for `formula-cpp/detail/wide_rounding.hpp`.

- [ ] **Step 3: Implement `include/formula-cpp/detail/wide_rounding.hpp`.**

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The decimal a wide fraction rounds to, exactly: the rounding `rounded_output`
/// states for a value `Rational` cannot hold (`opaque.hpp`), and the rule for a
/// value known only between two bounds.
///
/// `round_wide_ratio` is built to agree with `checked_round`: for a value
/// `checked_round` accepts, it gives the same result in all seven modes at every
/// place from -18 to 18 (`test/wide_rounding_tests.cpp` checks 43729 such
/// cases). It also answers some values `checked_round` refuses -- where a
/// numerator times 10^places leaves 64 bits but the rounded result fits -- and
/// refuses, with `Overflow`, a result that does not fit `Rational`, as
/// `checked_round` does.
///
/// Every fraction is reduced before it is scaled, so an operation may hand over
/// one far from lowest terms: the width only has to hold the value, not the way
/// it was reached.
///
/// Integer arithmetic only (`detail/wide_int.hpp`): no floating point, no
/// intrinsic, no 128-bit integer type.

#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace formula::detail
{
```

  `reduced`, `wide_from_rational`, `scaled_to_denominator`:

```cpp
    /// @p unreduced in lowest terms, its sign kept; zero as zero over one, never
    /// negative. @pre the denominator is not zero.
    template <std::size_t L>
    [[nodiscard]] constexpr WideRatio<L> reduced(WideRatio<L> const& unreduced) noexcept
    {
        if (unreduced.numerator.is_zero())
            return WideRatio<L> { false, WideUnsigned<L> {}, WideUnsigned<L>::from_u64(1) };
        WideUnsigned<L> const common = gcd(unreduced.numerator, unreduced.denominator);
        return WideRatio<L> { unreduced.negative, divmod(unreduced.numerator, common).quotient,
                              divmod(unreduced.denominator, common).quotient };
    }

    /// @p exact as a wide fraction. Four limbs at least, so that the product of
    /// its numerator and a 64-bit factor always fits.
    template <std::size_t L>
        requires(L >= 4)
    [[nodiscard]] constexpr WideRatio<L> wide_from_rational(Rational exact) noexcept
    {
        return WideRatio<L> { exact.numerator() < 0, WideUnsigned<L>::from_u64(magnitude(exact.numerator())),
                              WideUnsigned<L>::from_u64(static_cast<std::uint64_t>(exact.denominator())) };
    }

    /// @p exact times @p commonDenominator, an integer; nothing when it does not
    /// fit. @pre `exact.denominator()` divides @p commonDenominator.
    template <std::size_t L>
    [[nodiscard]] constexpr std::optional<WideSigned<L>> scaled_to_denominator(Rational exact,
                                                                              WideUnsigned<L> const& commonDenominator) noexcept
    {
        WideUnsigned<L> const cofactor =
            divmod(commonDenominator, WideUnsigned<L>::from_u64(static_cast<std::uint64_t>(exact.denominator()))).quotient;
        std::optional<WideUnsigned<L>> const scaledMagnitude =
            mul_checked_or_none(WideUnsigned<L>::from_u64(magnitude(exact.numerator())), cofactor);
        if (!scaledMagnitude)
            return std::nullopt;
        return WideSigned<L> { exact.numerator() < 0, *scaledMagnitude };
    }
```

  `round_wide_ratio` -- rounds the **magnitude**, then applies the sign; the directed modes read the sign
  (Floor rounds a negative magnitude up, Ceiling a positive one), exactly the pairs `checked_round_to_int`
  (`rounding.hpp:98-148`) reaches by flooring the signed value:

```cpp
    /// The decimal @p unrounded rounds to at @p places under @p roundingMode, as
    /// `Rational::from_decimal(kept, -places)`. `Overflow` for places outside
    /// -18 to 18, or a kept integer that does not fit `Rational::Int`, as
    /// `checked_round` reports them; `DivisionByZero` for a zero denominator.
    template <std::size_t L>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> round_wide_ratio(WideRatio<L> const& unrounded,
                                                                                      DecimalPlaces places,
                                                                                      RoundingMode roundingMode) noexcept
    {
        if (places.value > 18 || places.value < -18)
            return std::unexpected { ArithmeticError::Overflow };
        if (unrounded.denominator.is_zero())
            return std::unexpected { ArithmeticError::DivisionByZero };
        WideRatio<L> const lowest = reduced(unrounded);
        auto const placesMagnitude = static_cast<std::size_t>(places.value < 0 ? -places.value : places.value);
        std::optional<WideUnsigned<L>> const powerOfTen = pow10<L>(placesMagnitude);
        if (!powerOfTen)
            return std::unexpected { ArithmeticError::Overflow };
        // v * 10^places, as a numerator over a denominator: a positive places
        // scales the numerator, a negative one the denominator.
        std::optional<WideUnsigned<L>> const scaledNumerator = places.value >= 0
            ? mul_checked_or_none(lowest.numerator, *powerOfTen)
            : std::optional<WideUnsigned<L>> { lowest.numerator };
        std::optional<WideUnsigned<L>> const scaledDenominator = places.value < 0
            ? mul_checked_or_none(lowest.denominator, *powerOfTen)
            : std::optional<WideUnsigned<L>> { lowest.denominator };
        if (!scaledNumerator || !scaledDenominator)
            return std::unexpected { ArithmeticError::Overflow };

        WideDivision<L> const split = divmod(*scaledNumerator, *scaledDenominator);
        bool awayFromZero = false; // one step up in magnitude
        if (!split.remainder.is_zero())
        {
            // Which side of the half: remainder against denominator - remainder, never doubled.
            std::strong_ordering const side = split.remainder <=> *sub_checked_or_none(*scaledDenominator, split.remainder);
            switch (roundingMode)
            {
                case RoundingMode::TowardZero: awayFromZero = false; break;
                case RoundingMode::AwayFromZero: awayFromZero = true; break;
                case RoundingMode::Floor: awayFromZero = lowest.negative; break;
                case RoundingMode::Ceiling: awayFromZero = !lowest.negative; break;
                case RoundingMode::HalfAwayFromZero: awayFromZero = side != std::strong_ordering::less; break;
                case RoundingMode::HalfTowardZero: awayFromZero = side == std::strong_ordering::greater; break;
                case RoundingMode::HalfEven:
                    awayFromZero = side == std::strong_ordering::greater
                                   || (side == std::strong_ordering::equal && (split.quotient.limb(0) & 1U) != 0);
                    break;
            }
        }
        std::optional<WideUnsigned<L>> const kept =
            awayFromZero ? add_small_checked_or_none(split.quotient, 1U) : split.quotient;
        std::optional<std::uint64_t> const keptMagnitude = kept ? kept->to_u64() : std::nullopt;
        constexpr std::uint64_t positiveLimit = static_cast<std::uint64_t>(IntMax);
        if (!keptMagnitude || *keptMagnitude > (lowest.negative ? positiveLimit + 1U : positiveLimit))
            return std::unexpected { ArithmeticError::Overflow };
        // Well defined since C++20: conversion to a signed type is modular.
        auto const mantissa = lowest.negative ? static_cast<Rational::Int>(0U - *keptMagnitude)
                                              : static_cast<Rational::Int>(*keptMagnitude);
        return Rational::from_decimal(mantissa, -places.value);
    }
```

  `narrow_wide_ratio(unreduced)`: `DivisionByZero` for a zero denominator; `reduced`; `to_u64` both;
  `Overflow` when either is empty, the denominator exceeds `IntMax`, or the numerator exceeds `IntMax`
  (`IntMax + 1` when negative); then `Rational::make(signedNumerator, static_cast<Rational::Int>(denominator))`,
  the signed numerator formed as in `round_wide_ratio`.

  `rounded_in_unit`:

```cpp
    /// @p coherentValue, a value in the coherent unit of @p roundedIn's
    /// dimension, rounded to @p places of @p roundedIn under @p roundingMode, and
    /// answered in the coherent unit again: what `RepRounding<Rational>::round_in`
    /// does for a `Rational` (`rounding_node.hpp`), exactly, for a wider value.
    /// The conversion into @p roundedIn is a multiplication by the unit's
    /// magnitude, inverted; the conversion back is `checked_convert`'s.
    ///
    /// A unit with an offset is `DomainError` here: `rounded_output` refuses one
    /// at compile time, and this is the run-time backstop. A zero magnitude
    /// numerator is `DomainError` and a zero denominator `DivisionByZero`, as
    /// `checked_convert` answers them.
    template <std::size_t L>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_in_unit(WideRatio<L> const& coherentValue,
                                                                                     Unit const& roundedIn,
                                                                                     DecimalPlaces places,
                                                                                     RoundingMode roundingMode) noexcept
    {
        if (roundedIn.magnitudeNumerator == 0 || roundedIn.offsetNumerator != 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (roundedIn.magnitudeDenominator == 0 || coherentValue.denominator.is_zero())
            return std::unexpected { ArithmeticError::DivisionByZero };
        WideRatio<L> const lowest = reduced(coherentValue);
        bool const flipped = (roundedIn.magnitudeNumerator < 0) != (roundedIn.magnitudeDenominator < 0);
        std::optional<WideUnsigned<L>> const inUnitNumerator =
            mul_checked_or_none(lowest.numerator, WideUnsigned<L>::from_u64(magnitude(roundedIn.magnitudeDenominator)));
        std::optional<WideUnsigned<L>> const inUnitDenominator =
            mul_checked_or_none(lowest.denominator, WideUnsigned<L>::from_u64(magnitude(roundedIn.magnitudeNumerator)));
        if (!inUnitNumerator || !inUnitDenominator)
            return std::unexpected { ArithmeticError::Overflow };
        std::expected<Rational, ArithmeticError> const roundedThere = round_wide_ratio(
            WideRatio<L> { lowest.negative != flipped && !inUnitNumerator->is_zero(), *inUnitNumerator, *inUnitDenominator },
            places,
            roundingMode);
        if (!roundedThere)
            return roundedThere;
        return checked_convert(*roundedThere, roundedIn, Unit { .dimension = roundedIn.dimension });
    }
```

  `decide_rounding(lowerBound, upperBound, places, roundingMode)`: round each with `round_wide_ratio`; return the
  first error if either fails; `Overflow` when the two answers differ; otherwise the shared answer. Its comment:
  "Rounding is monotone in every mode, so a value between two bounds that round alike rounds as they do; bounds
  that round differently straddle a boundary the working precision cannot place the value on, and the answer is
  `Overflow`, never a guess. Nothing else is decided here: which bounds, and how tight, is the caller's."

- [ ] **Step 4: FILE_SET.** Root `CMakeLists.txt`, after the `detail/wide_int.hpp` line:
  `        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/wide_rounding.hpp"`.

- [ ] **Step 5: Run and pass.** `cl.ps1 -Filter "wide rounding|hygiene"` and `gcc14.sh "wide rounding|hygiene"`:
  8 cases pass; the two counts read 43729 and 64582. Then the full suite on both.

- [ ] **Step 6: Mutation checks.** (a) Swap `lowest.negative` and `!lowest.negative` in the Floor/Ceiling cases:
  "ties follow the mode and the sign" and the grid fail. (b) Replace `reduced(unrounded)` by `unrounded` in
  `round_wide_ratio`: the `farFromLowest` case fails to compile as a constant expression (it is `Overflow`).
  (c) Replace `reduced(coherentValue)` by `coherentValue` in `rounded_in_unit` alone: the `wideInMetres` case
  fails (3 * 2^118 times 1000 needs 130 bits). (d) Remove the
  `+ 1U` for a negative value in the limit: the `IntMin` case fails. Restore each with a plain write.

- [ ] **Step 7: Commit.**
  `git add include/formula-cpp/detail/wide_rounding.hpp test/wide_rounding_tests.cpp test/CMakeLists.txt CMakeLists.txt`
  `git commit -m "feat(detail): round a wide fraction exactly, as checked_round rounds a Rational" -m "The decimal a value rounds to can be decided in integers even when the value itself needs more than 64 bits. round_wide_ratio reduces the fraction, scales it by a power of ten and decides the last digit by comparing the remainder with its complement, in every mode, agreeing with checked_round wherever that answers. rounded_in_unit rounds in a declared unit as a rounding node does, and decide_rounding answers for a value known between two bounds only when both round alike -- Overflow otherwise, never a guess. Internal to the library." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 3: `rounded_output` -- the node, its refusals and its evaluation

**Files:**
- Modify: `include/formula-cpp/opaque.hpp` -- file comment `:4-43`; includes `:45-69`; `OpaqueFailure` `:88-108`
  (add `OpaqueValues` after it); `OpaqueCallInfo` `:130-145`; after `OpaqueComputeNoexcept` `:493-501`;
  `RequireOpaqueOutputChosen` `:798-808`; after `opaque_output` `:837-853`; `opaque_compute` and
  `evaluate_opaque_from` `:980-1022`; `evaluate_call` `:1044-1060`; after `opaque_sound_for` `:1062-1072`; after
  `checked_evaluate_si` `:1075-1106`.
- Modify: `include/formula-cpp/sink.hpp:106-110` (the opaque hooks' paragraph)
- Create: `test/rounded_output_tests.cpp`; Modify: `test/CMakeLists.txt` (source list, after `wide_rounding_tests.cpp`)
- Create: `test/negative/rounded_output_unit_dimension_mismatch.cpp`, `rounded_output_offset_unit_dimension_mismatch.cpp`,
  `rounded_output_unit_has_offset.cpp`, `rounded_output_unknown_output.cpp`, `rounded_output_refused_call.cpp`,
  `rounded_output_forged_position.cpp`, `rounded_output_rep_double.cpp`; register them after
  `least_squares_in_double` (`test/CMakeLists.txt:2203-2205`)
- Modify: `CHANGELOG.md` (`### Added`, `### Changed`)

**Interfaces:**
- Consumes (Task 2): `detail::rounded_in_unit`, `detail::WideRatio`; `RepRounding<Rep>` (`rounding_node.hpp`).
- Produces (namespace `formula` unless marked):
  - `enum class OpaqueValues : std::uint8_t { Exact, RoundedWhereUsed };`
  - `OpaqueCallInfo::values` (`OpaqueValues`, default `Exact`), after `dimensions`.
  - `template <std::size_t I, typename Call, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin = detail::NamedOpaqueOutput> struct RoundedOpaqueOutputNode: NodeBase`
    with `Call call;` and `static constexpr` `unit`, `places`, `mode`, `dimension` (the output's), `index`,
    `output`, `detail::RefusedFlag refused`.
  - `template <detail::FixedString Name, Unit U, DecimalPlaces Places, RoundingMode Mode, OpaqueOperation Op, typename... Inputs> constexpr auto rounded_output(OpaqueCall<Op, Inputs...> call) noexcept`
  - `checked_evaluate_si<Rep>(RoundedOpaqueOutputNode<...> const&, Env const&, Sink = {})`
  - `detail::declares_compute_exact<Op, Inputs...>` (bool); `detail::evaluate_rounded_call<I>(call, environment, sink)`
    -> `std::expected<std::optional<Answer>, OpaqueCallFailure>` with `Answer` = `detail::WideRatio<Op::exact_limbs>`
    or `Rational`; `detail::unrounded(node)` -> `OpaqueOutputNode<I, Call, Origin>`;
    `detail::evaluate_opaque_inputs<Answer, Rep, At>(call, environment, sink, finish, done...)`.

- [ ] **Step 1: Write the failing tests** -- `test/rounded_output_tests.cpp` (this task's part; Tasks 4 and 5 add to it):

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/rounding_node.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented readings in grams, and what an operation makes of them.
struct Reading: formula::Quantity<Reading, "r", "an invented reading", unit::Gram>
{
};
struct Span: formula::Quantity<Span, "r_sp", "the span of the readings", unit::Gram>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", unit::One>
{
};
// Invented dimensionless draws, and the sum of their reciprocals.
struct Draw: formula::Quantity<Draw, "z", "an invented dimensionless draw", unit::One>
{
};
struct Reciprocals: formula::Quantity<Reciprocals, "z_r", "the sum of the draws' reciprocals", unit::One>
{
};
// Two invented gains and their product.
struct Gain: formula::Quantity<Gain, "g_1", "an invented gain", unit::One>
{
};
struct Boost: formula::Quantity<Boost, "g_2", "another invented gain", unit::One>
{
};
struct Amplified: formula::Quantity<Amplified, "g_12", "the two gains' product", unit::One>
{
};

// The lowest reading and the span of a series: compute alone, so a rounded
// output of it takes compute<Rational> and rounds that exactly.
struct ReadingSpan
{
    static constexpr std::string_view name = "reading span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "lowest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const apart = formula::RepTraits<Rep>::subtract(most, least);
        if (!apart.has_value())
            return std::unexpected { apart.error() };
        return std::array { least, *apart };
    }
};

// The sum of the draws' reciprocals, with the exact hook: over ten invented
// primes its exact denominator is their product, 75 bits, which no Rational
// holds and four 32-bit limbs do. Counts which route ran.
struct ReciprocalSum
{
    static constexpr std::string_view name = "reciprocal sum";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "total" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        if (!(declared[0] == formula::dim::Scalar))
            return std::nullopt;
        return std::array { formula::dim::Scalar };
    }

    static inline int computeCalls = 0;
    static inline int exactCalls = 0;

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> draws) noexcept
    {
        if !consteval
        {
            ++computeCalls;
        }
        using Traits = formula::RepTraits<Rep>;
        std::expected<Rep, formula::ArithmeticError> const zero = Traits::from(formula::Rational { 0 });
        std::expected<Rep, formula::ArithmeticError> const one = Traits::from(formula::Rational { 1 });
        if (!zero.has_value() || !one.has_value())
            return std::unexpected { formula::ArithmeticError::Overflow };
        Rep running = *zero;
        for (Rep const& each: draws)
        {
            if (!(*zero < each))
                return std::unexpected { formula::ArithmeticError::DomainError };
            std::expected<Rep, formula::ArithmeticError> const reciprocal = Traits::divide(*one, each);
            if (!reciprocal.has_value())
                return std::unexpected { reciprocal.error() };
            std::expected<Rep, formula::ArithmeticError> const added = Traits::add(running, *reciprocal);
            if (!added.has_value())
                return std::unexpected { added.error() };
            running = *added;
        }
        return std::array { running };
    }

    static constexpr std::size_t exact_limbs = 4;

    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(std::span<formula::Rational const> draws) noexcept
    {
        if !consteval
        {
            ++exactCalls;
        }
        using Wide = formula::detail::WideUnsigned<exact_limbs>;
        formula::detail::WideRatio<exact_limbs> running { false, Wide {}, Wide::from_u64(1) };
        for (formula::Rational const& each: draws)
        {
            if (each.sign() <= 0)
                return std::unexpected { formula::ArithmeticError::DomainError };
            // running + 1 / (p / q) = (running.numerator * p + running.denominator * q) / (running.denominator * p)
            Wide const top = Wide::from_u64(static_cast<std::uint64_t>(each.numerator()));
            Wide const bottom = Wide::from_u64(static_cast<std::uint64_t>(each.denominator()));
            std::optional<Wide> const kept = formula::detail::mul_checked_or_none(running.numerator, top);
            std::optional<Wide> const added = formula::detail::mul_checked_or_none(running.denominator, bottom);
            std::optional<Wide> const summed =
                kept && added ? formula::detail::add_checked_or_none(*kept, *added) : std::nullopt;
            std::optional<Wide> const under = formula::detail::mul_checked_or_none(running.denominator, top);
            if (!summed || !under)
                return std::unexpected { formula::ArithmeticError::Overflow };
            running = formula::detail::WideRatio<exact_limbs> { false, *summed, *under };
        }
        return std::array { running };
    }
};

// The same operation with a hook that is not noexcept, and one too narrow:
// neither is taken, and both fall back to compute<Rational>.
struct LooseReciprocalSum: ReciprocalSum
{
    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(std::span<formula::Rational const> draws)
    {
        return ReciprocalSum::compute_exact(draws);
    }
};
struct NarrowReciprocalSum: ReciprocalSum
{
    static constexpr std::size_t exact_limbs = 2;
};

// The product of two gains, with the exact hook: 2^40 times 2^40 is 2^80,
// which the hook holds and no Rational does.
struct WideProduct
{
    static constexpr std::string_view name = "wide product";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "product" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] * declared[1] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep multiplicand, Rep multiplier) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const product = formula::RepTraits<Rep>::multiply(multiplicand, multiplier);
        if (!product.has_value())
            return std::unexpected { product.error() };
        return std::array { *product };
    }

    static constexpr std::size_t exact_limbs = 4;

    static constexpr std::expected<std::array<formula::detail::WideRatio<exact_limbs>, 1>, formula::ArithmeticError>
    compute_exact(formula::Rational multiplicand, formula::Rational multiplier) noexcept
    {
        auto const leftWide = formula::detail::wide_from_rational<exact_limbs>(multiplicand);
        auto const rightWide = formula::detail::wide_from_rational<exact_limbs>(multiplier);
        auto const numeratorProduct = formula::detail::mul_checked_or_none(leftWide.numerator, rightWide.numerator);
        auto const denominatorProduct = formula::detail::mul_checked_or_none(leftWide.denominator, rightWide.denominator);
        if (!numeratorProduct || !denominatorProduct)
            return std::unexpected { formula::ArithmeticError::Overflow };
        return std::array { formula::detail::WideRatio<exact_limbs> {
            leftWide.negative != rightWide.negative && !numeratorProduct->is_zero(), *numeratorProduct, *denominatorProduct } };
    }
};

constexpr formula::Citation spanClause { .title = "Span of readings", .reference = "Example Standard 7", .section = "2.3" };
constexpr formula::Citation reciprocalClause { .title = "Reciprocal sum", .reference = "Example Standard 7", .section = "2.4" };
constexpr formula::Citation productClause { .title = "Product of gains", .reference = "Example Standard 7", .section = "2.5" };

constexpr auto spanCall = formula::opaque<ReadingSpan>(spanClause, formula::series<Reading, 4>);

// 127.3, 103.26, 191.07 and 139.4 g: span 87.81 g. With 191.11 g in place of
// 191.07 g the span is 87.85 g, a tie at 1 dp.
constexpr auto readings = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(1273, 10) },
                                                                                 formula::Measured<Reading> { rat(10326, 100) },
                                                                                 formula::Measured<Reading> { rat(19107, 100) },
                                                                                 formula::Measured<Reading> { rat(1394, 10) }));
constexpr auto tiedReadings = formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(1273, 10) },
                                                                                     formula::Measured<Reading> { rat(10326, 100) },
                                                                                     formula::Measured<Reading> { rat(19111, 100) },
                                                                                     formula::Measured<Reading> { rat(1394, 10) }));

// Five and ten invented primes. The five's reciprocals sum to
// 2101205901/58386114749, which Rational holds; the ten's to a fraction over
// a 75-bit denominator, which it does not.
constexpr auto fiveDraws = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                               formula::Measured<Draw> { rat(127) },
                                                                               formula::Measured<Draw> { rat(139) },
                                                                               formula::Measured<Draw> { rat(163) },
                                                                               formula::Measured<Draw> { rat(197) }));
constexpr auto tenDraws = formula::environment(formula::measured_series<Draw>(
    formula::Measured<Draw> { rat(103) }, formula::Measured<Draw> { rat(127) }, formula::Measured<Draw> { rat(139) },
    formula::Measured<Draw> { rat(163) }, formula::Measured<Draw> { rat(197) }, formula::Measured<Draw> { rat(211) },
    formula::Measured<Draw> { rat(227) }, formula::Measured<Draw> { rat(229) }, formula::Measured<Draw> { rat(233) },
    formula::Measured<Draw> { rat(239) }));
constexpr auto fiveCall = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 5>);
constexpr auto tenCall = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 10>);

template <formula::RoundingMode Mode>
void check_routes_agree_on_five_draws()
{
    auto const fused = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, Mode>(fiveCall), fiveDraws);
    auto const afterwards = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded<unit::One, formula::DecimalPlaces { 6 }, Mode>(formula::opaque_output<"total">(fiveCall)), fiveDraws);
    REQUIRE(fused.has_value());
    REQUIRE(afterwards.has_value());
    CHECK(**fused == **afterwards);
}

/// What a sink that hears the opaque hooks was told of one call.
struct ToldCall
{
    std::size_t valuesHeld;
    formula::OpaqueValues values;
    bool answered;
};

/// A sink of the test's own: it records what each opaque call told it.
struct HearsCalls
{
    std::vector<ToldCall>* told;

    template <formula::Node N>
    void entered(N const&) noexcept
    {
    }

    template <formula::Node N, typename V>
    void produced(N const&, V const&) noexcept
    {
    }

    void opaque_entered(formula::OpaqueCallInfo const&) noexcept {}

    template <std::size_t M>
    void opaque_produced(formula::OpaqueCallInfo const& callInfo, formula::OpaqueEvaluated<formula::Rational, M> const& evaluated)
    {
        told->push_back(ToldCall { M, callInfo.values, evaluated.has_value() && evaluated->has_value() });
    }
};
} // namespace

TEST_CASE("rounded output: a node of its output's dimension that states its rounding", "[rounded-output]")
{
    constexpr auto roundedSpan =
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall);
    using RoundedSpan = decltype(roundedSpan);
    STATIC_REQUIRE(formula::Node<RoundedSpan>);
    STATIC_REQUIRE(RoundedSpan::dimension == formula::dim::Mass);
    STATIC_REQUIRE(RoundedSpan::unit == unit::Gram);
    STATIC_REQUIRE(RoundedSpan::places == formula::DecimalPlaces { 1 });
    STATIC_REQUIRE(RoundedSpan::mode == formula::RoundingMode::HalfEven);
    STATIC_REQUIRE(RoundedSpan::index == 1);
    STATIC_REQUIRE(RoundedSpan::output == "span");
    STATIC_REQUIRE(!RoundedSpan::refused);
}

TEST_CASE("rounded output: rounds in its own unit and not in the coherent one", "[rounded-output]")
{
    // 87.81 g to 1 dp of g is 87.8 g, 0.0878 kg. Rounded in kilograms it would
    // be 0.1 kg: the unit is part of the rounding.
    constexpr auto inGrams = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        readings);
    STATIC_REQUIRE(inGrams->measurement().value() == rat(439, 5));
    constexpr auto inKilograms = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"span", unit::Kilogram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        readings);
    STATIC_REQUIRE(**inKilograms == rat(1, 10));
    // Ceiling moves off 87.81 where HalfEven does not.
    constexpr auto upwards = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::Ceiling>(spanCall),
        readings);
    STATIC_REQUIRE(upwards->measurement().value() == rat(879, 10));
}

TEST_CASE("rounded output: a tie is broken by the mode as checked_round breaks it", "[rounded-output]")
{
    // 87.85 g: 87.8 under HalfEven and HalfTowardZero, 87.9 under HalfAwayFromZero.
    constexpr auto even = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall),
        tiedReadings);
    constexpr auto away = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(spanCall),
        tiedReadings);
    constexpr auto toward = formula::checked_evaluate<Span>(
        formula::rounded_output<"span", unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfTowardZero>(spanCall),
        tiedReadings);
    STATIC_REQUIRE(even->measurement().value() == rat(439, 5));
    STATIC_REQUIRE(away->measurement().value() == rat(879, 10));
    STATIC_REQUIRE(toward->measurement().value() == rat(439, 5));
}

TEST_CASE("rounded output: the exact hook answers where the exact route overflows", "[rounded-output]")
{
    STATIC_REQUIRE(formula::detail::declares_compute_exact<ReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    // Ten reciprocals: the exact total needs a 75-bit denominator.
    auto const exactRoute = formula::checked_evaluate<Reciprocals>(formula::opaque_output<"total">(tenCall), tenDraws);
    REQUIRE(!exactRoute.has_value());
    CHECK(exactRoute.error() == formula::ArithmeticError::Overflow);

    ReciprocalSum::computeCalls = 0;
    ReciprocalSum::exactCalls = 0;
    auto const rounded = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(tenCall), tenDraws);
    REQUIRE(rounded.has_value());
    CHECK(rounded->measurement().value() == rat(2319, 40000)); // 0.057975
    CHECK(ReciprocalSum::exactCalls == 1);
    CHECK(ReciprocalSum::computeCalls == 0);
    auto const upwards = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Ceiling>(tenCall), tenDraws);
    CHECK(upwards->measurement().value() == rat(7247, 125000)); // 0.057976
}

TEST_CASE("rounded output: where both routes answer it is the exact output rounded in every mode", "[rounded-output]")
{
    // 2101205901/58386114749 = 0.0359881...: 0.035988, 0.035989 upwards.
    constexpr auto fused = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), fiveDraws);
    STATIC_REQUIRE(fused->measurement().value() == rat(8997, 250000));
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfAwayFromZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfTowardZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::HalfEven>();
    check_routes_agree_on_five_draws<formula::RoundingMode::Ceiling>();
    check_routes_agree_on_five_draws<formula::RoundingMode::Floor>();
    check_routes_agree_on_five_draws<formula::RoundingMode::TowardZero>();
    check_routes_agree_on_five_draws<formula::RoundingMode::AwayFromZero>();
}

TEST_CASE("rounded output: absent when an input is and the hook is never called", "[rounded-output]")
{
    auto const gap = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                         formula::Measured<Draw> { rat(127) },
                                                                         formula::Measured<Draw>::absent(),
                                                                         formula::Measured<Draw> { rat(163) },
                                                                         formula::Measured<Draw> { rat(197) }));
    ReciprocalSum::exactCalls = 0;
    auto const outcome = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), gap);
    REQUIRE(outcome.has_value());
    CHECK(outcome->is_empty());
    CHECK(ReciprocalSum::exactCalls == 0);
}

TEST_CASE("rounded output: an input's failure is relayed with its site and the hook is never called", "[rounded-output]")
{
    // The draws divided by a zero divisor: the division fails at element 0.
    constexpr auto divided = formula::opaque<ReciprocalSum>(reciprocalClause, formula::series<Draw, 5> / formula::var<Divisor>);
    auto const byZero = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                            formula::Measured<Draw> { rat(127) },
                                                                            formula::Measured<Draw> { rat(139) },
                                                                            formula::Measured<Draw> { rat(163) },
                                                                            formula::Measured<Draw> { rat(197) }),
                                             formula::Measured<Divisor> { rat(0) });
    ReciprocalSum::exactCalls = 0;
    auto const called = formula::detail::evaluate_rounded_call<0>(divided, byZero, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::DivisionByZero);
    CHECK(called.error().origin == formula::OpaqueFailure::Propagated);
    CHECK(called.error().element == std::optional<std::size_t> { 0 });
    CHECK(ReciprocalSum::exactCalls == 0);
    auto const outcome = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(divided), byZero);
    REQUIRE(!outcome.has_value());
    CHECK(outcome.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("rounded output: the hook's own failure is the operation's", "[rounded-output]")
{
    auto const withZero = formula::environment(formula::measured_series<Draw>(formula::Measured<Draw> { rat(103) },
                                                                              formula::Measured<Draw> { rat(0) },
                                                                              formula::Measured<Draw> { rat(139) },
                                                                              formula::Measured<Draw> { rat(163) },
                                                                              formula::Measured<Draw> { rat(197) }));
    auto const called = formula::detail::evaluate_rounded_call<0>(fiveCall, withZero, formula::NullSink {});
    REQUIRE(!called.has_value());
    CHECK(called.error().error == formula::ArithmeticError::DomainError);
    CHECK(called.error().origin == formula::OpaqueFailure::Own);
}

TEST_CASE("rounded output: a rounding that fails after the call answered is the output's alone", "[rounded-output]")
{
    constexpr auto productCall = formula::opaque<WideProduct>(productClause, formula::var<Gain>, formula::var<Boost>);
    constexpr std::int64_t twoToForty = std::int64_t { 1 } << 40;
    auto const huge = formula::environment(formula::Measured<Gain> { rat(twoToForty) }, formula::Measured<Boost> { rat(twoToForty) });
    // The exact route cannot hold 2^80 at all.
    auto const plain = formula::checked_evaluate<Amplified>(formula::opaque_output<"product">(productCall), huge);
    REQUIRE(!plain.has_value());
    CHECK(plain.error() == formula::ArithmeticError::Overflow);
    // The hook holds it -- the call answers -- but no Rational holds 2^80
    // rounded to units either: the output fails, not the call.
    auto const called = formula::detail::evaluate_rounded_call<0>(productCall, huge, formula::NullSink {});
    CHECK(called.has_value());
    auto const rounded = formula::checked_evaluate<Amplified>(
        formula::rounded_output<"product", unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(productCall), huge);
    REQUIRE(!rounded.has_value());
    CHECK(rounded.error() == formula::ArithmeticError::Overflow);
    // The control: 2^40 times 1/2^30 is 1024.
    auto const modest = formula::environment(formula::Measured<Gain> { rat(twoToForty) },
                                             formula::Measured<Boost> { rat(1, std::int64_t { 1 } << 30) });
    auto const answered = formula::checked_evaluate<Amplified>(
        formula::rounded_output<"product", unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(productCall), modest);
    REQUIRE(answered.has_value());
    CHECK(answered->measurement().value() == rat(1024));
}

TEST_CASE("rounded output: a sink hearing the call is told it holds no values", "[rounded-output]")
{
    std::vector<ToldCall> told;
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall), fiveDraws,
        HearsCalls { &told });
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"total">(fiveCall), fiveDraws, HearsCalls { &told });
    REQUIRE(told.size() == 2);
    CHECK(told[0].valuesHeld == 0);
    CHECK(told[0].values == formula::OpaqueValues::RoundedWhereUsed);
    CHECK(told[0].answered);
    CHECK(told[1].valuesHeld == 1);
    CHECK(told[1].values == formula::OpaqueValues::Exact);
    CHECK(told[1].answered);
}

TEST_CASE("rounded output: a hook that is not noexcept or too narrow is not taken", "[rounded-output]")
{
    STATIC_REQUIRE(!formula::detail::declares_compute_exact<LooseReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    STATIC_REQUIRE(!formula::detail::declares_compute_exact<NarrowReciprocalSum, formula::SeriesVarNode<Draw, 10>>);
    // Each falls back to compute<Rational>, and so to its Overflow over ten
    // draws, where ReciprocalSum's own hook answers.
    auto const loose = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(
            formula::opaque<LooseReciprocalSum>(reciprocalClause, formula::series<Draw, 10>)),
        tenDraws);
    auto const narrow = formula::checked_evaluate<Reciprocals>(
        formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(
            formula::opaque<NarrowReciprocalSum>(reciprocalClause, formula::series<Draw, 10>)),
        tenDraws);
    REQUIRE(!loose.has_value());
    CHECK(loose.error() == formula::ArithmeticError::Overflow);
    REQUIRE(!narrow.has_value());
    CHECK(narrow.error() == formula::ArithmeticError::Overflow);
}
```

  (`formula::series<Q, N>` is `SeriesVarNode<Q, N>`, `series.hpp:65`, `:92`.)

- [ ] **Step 2: Add `rounded_output_tests.cpp` to `test/CMakeLists.txt`** after `wide_rounding_tests.cpp` and run
  `cl.ps1 -Filter "rounded output"`. Expected: `BUILD FAILED`, `C2039: 'rounded_output': is not a member of
  'formula'` (and `OpaqueValues`).

- [ ] **Step 3: `OpaqueValues` and `OpaqueCallInfo::values`.** Includes (`opaque.hpp:45-57`): add
  `#include <formula-cpp/detail/wide_rounding.hpp>`, `#include <formula-cpp/rounding.hpp>`,
  `#include <formula-cpp/rounding_node.hpp>` and `#include <formula-cpp/unit.hpp>` in alphabetical place. After
  `OpaqueFailure` (`:108`):

```cpp
/// Whether an opaque call holds its outputs' values, as a sink is told of it
/// (`OpaqueCallInfo::values`) and a trace records it (`OpaqueStepData::values`).
enum class OpaqueValues : std::uint8_t
{
    /// Every output is a `Rational`, exactly: `opaque_output`'s route. The zero
    /// value, so a call described before this existed reads as it did.
    Exact,
    /// The call was evaluated for an output rounded where it is used
    /// (`rounded_output`): no output exists as a `Rational` until it is
    /// rounded, so the call holds no values, and each output used states its
    /// own rounding.
    RoundedWhereUsed,
};
```

  In `OpaqueCallInfo` after `dimensions` (`:144`):

```cpp
    /// Whether the evaluation the sink is told of holds the outputs' values. A
    /// call evaluated for a `rounded_output` says `RoundedWhereUsed`, and its
    /// `opaque_produced` is handed an `OpaqueEvaluated<Rational, 0>`: whether
    /// the call answered, was absent or failed, and no value.
    OpaqueValues values = OpaqueValues::Exact;
```

- [ ] **Step 4: Detecting the exact hook.** After `OpaqueComputeNoexcept` (`:493-501`):

```cpp
    /// Whether @p Op declares the exact hook a `rounded_output` prefers, for
    /// arguments @p Tuple: `static constexpr std::size_t exact_limbs`, at least
    /// 4, and a `noexcept` `compute_exact` taking what `compute<Rational>`
    /// takes and answering `std::expected<std::array<WideRatio<exact_limbs>,
    /// M>, ArithmeticError>`, each output exactly. An internal hook, not a
    /// customisation point: an operation that declares it any other way is
    /// evaluated through `compute<Rational>`, as if it had none.
    template <typename Op, typename Tuple>
    struct OpaqueExactCallable;

    template <typename Op, typename... Args>
    struct OpaqueExactCallable<Op, std::tuple<Args...>>
    {
        static constexpr bool value = requires(Args... arguments) {
            { Op::exact_limbs } -> std::convertible_to<std::size_t>;
            requires Op::exact_limbs >= 4;
            {
                Op::compute_exact(arguments...)
            } -> std::same_as<std::expected<std::array<WideRatio<Op::exact_limbs>, Op::outputs.size()>, ArithmeticError>>;
            requires noexcept(Op::compute_exact(arguments...));
        };
    };

    /// Whether a call of @p Op on @p Inputs is evaluated for a `rounded_output`
    /// through `compute_exact`.
    template <typename Op, typename... Inputs>
    inline constexpr bool declares_compute_exact = OpaqueExactCallable<Op, OpaqueArgumentTuple<Rational, Inputs...>>::value;
```

- [ ] **Step 5: The refusals.** In the `detail` block after `RequireOpaqueOutputChosen` (`:798-808`):

```cpp
    /// Fails to compile when the unit a `rounded_output` is stated in does not
    /// measure the dimension of the output it rounds. Silent over a refused
    /// call or an output of no declared name: @p Output is refused already.
    template <typename Output, Unit U>
    struct RequireRoundedOutputUnitMatches
    {
        static_assert(refused_already<Output>() || U.dimension == Output::dimension,
                      "formula: this rounded_output names a unit that does not measure the dimension of the output it "
                      "rounds; the output and the unit appear in this diagnostic as the template arguments of "
                      "RequireRoundedOutputUnitMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when that unit has an offset, as degrees Celsius has:
    /// an operation's output -- a fitted coefficient, a spread -- is a
    /// difference or a ratio, and every reader converts the node's value
    /// through the unit, offset included. Asked only when @p DimensionMatches,
    /// so that a unit wrong in both ways draws the dimension's one message, as
    /// `RequireRootUnitWithoutOffset` is gated (`rounded_root.hpp`).
    template <Unit U, bool DimensionMatches>
    struct RequireRoundedOutputUnitWithoutOffset
    {
        static_assert(!DimensionMatches || U.offsetNumerator == 0,
                      "formula: this rounded_output names a unit with an offset, such as degrees Celsius; a fitted "
                      "coefficient is a difference or a ratio, which an offset unit would misstate; name the unit "
                      "without its offset");

        static constexpr bool value = true;
    };
```

- [ ] **Step 6: The node, the factory and `unrounded`.** After `opaque_output` (`:853`):

```cpp
/// Output @p I of the opaque call @p Call, rounded to @p Places decimal places
/// of @p U under @p Mode -- the decimal the operation's true output rounds to,
/// exact, even where that output is a fraction too wide for `Rational`: the
/// fused counterpart of `opaque_output`, as `rounded_sqrt` is of `rounded<>`
/// around a root. `rounded<>(opaque_output<>(...))` keeps meaning what it
/// says, an exact output rounded afterwards, which fails where the exact
/// output does.
///
/// Its dimension is the output's, and it rounds in `U`, never in the coherent
/// unit. @p Origin is `opaque_output`'s (`detail::UnnamedOpaqueOutput`); leave
/// it to its default. No `{}` initialiser on `call`, deliberately (defect
/// class 4).
template <std::size_t I, typename Call, Unit U, DecimalPlaces Places, RoundingMode Mode,
          typename Origin = detail::NamedOpaqueOutput>
struct RoundedOpaqueOutputNode: NodeBase
{
    static_assert(detail::RequireOpaqueOutputPosition<I, Call, Origin>::value);
    static_assert(detail::RequireRoundedOutputUnitMatches<OpaqueOutputNode<I, Call, Origin>, U>::value);
    static_assert(detail::RequireRoundedOutputUnitWithoutOffset<
                  U,
                  !OpaqueOutputNode<I, Call, Origin>::refused && U.dimension == OpaqueOutputNode<I, Call, Origin>::dimension>::value);

    /// The call whose output this is. Evaluating this node evaluates it whole.
    Call call;

    /// The unit the rounding happens in, and the step's value is stated in.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to go; the three half modes differ only on a tie, which only
    /// an output that is exactly rational can reach.
    static constexpr RoundingMode mode = Mode;
    /// The dimension the operation declares for this output.
    static constexpr Dimension dimension = OpaqueOutputNode<I, Call, Origin>::dimension;
    /// The output's position among the operation's outputs, zero-based.
    static constexpr std::size_t index = I;
    /// The output's name, as the operation declares it.
    static constexpr std::string_view output = OpaqueOutputNode<I, Call, Origin>::output;
    /// Whether the call was refused, or this position names no output
    /// (`OpaqueOutputNode::refused`): then every check over this node is
    /// silent, and it is never evaluated.
    static constexpr detail::RefusedFlag refused = OpaqueOutputNode<I, Call, Origin>::refused;
};

/// The output named @p Name of @p call, rounded to @p Places decimal places of
/// @p U under @p Mode, exactly:
/// `rounded_output<"slope", millimetrePerSecond, DecimalPlaces { 4 }, RoundingMode::HalfEven>(fit)`.
/// An output the operation does not declare is refused once, in
/// `opaque_output`'s words.
template <detail::FixedString Name, Unit U, DecimalPlaces Places, RoundingMode Mode, OpaqueOperation Op, typename... Inputs>
[[nodiscard]] constexpr auto rounded_output(OpaqueCall<Op, Inputs...> call) noexcept
{
    using Call = OpaqueCall<Op, Inputs...>;
    constexpr std::size_t namedAt = detail::output_index<Op, Name>();
    static_assert(std::conditional_t<detail::opaque_operation_well_formed<Op>,
                                     detail::RequireOpaqueOutputNamed<Op, Name>,
                                     std::true_type>::value);
    if constexpr (namedAt < Op::outputs.size())
        return RoundedOpaqueOutputNode<namedAt, Call, U, Places, Mode> { {}, call };
    else
        return RoundedOpaqueOutputNode<detail::unknownOutput, Call, U, Places, Mode, detail::UnnamedOpaqueOutput> { {}, call };
}

namespace detail
{
    /// The output @p node rounds, as `opaque_output` builds it: for the walks
    /// that treat the two alike -- `document`, an overlay's rewrite, `render`.
    template <std::size_t I, typename Call, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin>
    [[nodiscard]] constexpr OpaqueOutputNode<I, Call, Origin> unrounded(
        RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin> const& node) noexcept
    {
        return OpaqueOutputNode<I, Call, Origin> { {}, node.call };
    }
} // namespace detail
```

- [ ] **Step 7: One input walk for both routes.** Replace `opaque_compute` and `evaluate_opaque_from`
  (`:980-1022`) with:

```cpp
    /// Calls `compute` on every input, each wholly present: the outputs, or
    /// the operation's own error.
    template <typename Rep, typename Op, typename... Evaluated>
    [[nodiscard]] constexpr std::expected<std::array<Rep, Op::outputs.size()>, ArithmeticError> opaque_compute(
        Evaluated const&... evaluatedInputs) noexcept
    {
        auto const held = std::tuple { opaque_held(evaluatedInputs)... };
        auto const arguments =
            std::apply([](auto const&... each) noexcept { return std::tuple_cat(opaque_arguments<Rep>(each)...); }, held);
        return std::apply([](auto const&... each) noexcept { return Op::template compute<Rep>(each...); }, arguments);
    }

    /// Evaluates the call's inputs from position @p At on, each once and in
    /// order, carrying the ones already evaluated; stops at the first that
    /// fails, and hands the inputs to @p finish only when every one is wholly
    /// present. @p finish answers `std::expected<Answer, ArithmeticError>`,
    /// and its error is the operation's own.
    template <typename Answer, typename Rep, std::size_t At, typename Op, typename... Inputs, typename Env, typename Sink,
              typename Finish, typename... Done>
    [[nodiscard]] constexpr std::expected<std::optional<Answer>, OpaqueCallFailure> evaluate_opaque_inputs(
        OpaqueCall<Op, Inputs...> const& call, Env const& environment, Sink sink, Finish const& finish,
        Done const&... evaluatedSoFar) noexcept
    {
        if constexpr (At == sizeof...(Inputs))
        {
            // Absence is decided here, after every input has been asked.
            if (!(opaque_input_present(evaluatedSoFar) && ...))
                return std::optional<Answer> {};
            std::expected<Answer, ArithmeticError> const answered = finish(evaluatedSoFar...);
            if (!answered.has_value())
                return std::unexpected { OpaqueCallFailure { answered.error(), OpaqueFailure::Own, std::nullopt } };
            return std::optional<Answer> { *answered };
        }
        else
        {
            auto const evaluatedInput = evaluate_opaque_input<Rep>(std::get<At>(call.inputs), environment, sink);
            if (!evaluatedInput.has_value())
            {
                OpaqueCallFailure stopped = opaque_input_failure(evaluatedInput);
                stopped.notEvaluated = sizeof...(Inputs) - At - 1;
                return std::unexpected { stopped };
            }
            return evaluate_opaque_inputs<Answer, Rep, At + 1>(call, environment, sink, finish, evaluatedSoFar..., *evaluatedInput);
        }
    }
```

  In `evaluate_call` (`:1056`) the evaluation becomes:

```cpp
        OpaqueEvaluated<Rep, outputCount> const evaluated = evaluate_opaque_inputs<std::array<Rep, outputCount>, Rep, 0>(
            call, environment, sink, [](auto const&... held) noexcept { return opaque_compute<Rep, Op>(held...); });
```

  The exact route's behaviour is unchanged: `test/opaque_tests.cpp` and `test/least_squares_tests.cpp` pass
  untouched.

- [ ] **Step 8: The rounded call and the node's evaluation.** After `opaque_sound_for` (`:1072`, inside `detail`):

```cpp
    /// What a call evaluated for a `rounded_output` answers before it is
    /// rounded: output @p I exactly, wide when the operation declares the hook
    /// (`declares_compute_exact`), a `Rational` when it does not.
    template <typename Op, bool Exact>
    struct RoundedOpaqueAnswerOf
    {
        using type = Rational;
    };

    template <typename Op>
    struct RoundedOpaqueAnswerOf<Op, true>
    {
        using type = WideRatio<Op::exact_limbs>;
    };

    /// Output @p I of @p Op on every input, each wholly present: through
    /// `compute_exact` when @p Exact, through `compute<Rational>` otherwise.
    template <typename Op, std::size_t I, bool Exact, typename... Evaluated>
    [[nodiscard]] constexpr std::expected<typename RoundedOpaqueAnswerOf<Op, Exact>::type, ArithmeticError>
    opaque_rounding_answer(Evaluated const&... evaluatedInputs) noexcept
    {
        auto const held = std::tuple { opaque_held(evaluatedInputs)... };
        auto const arguments =
            std::apply([](auto const&... each) noexcept { return std::tuple_cat(opaque_arguments<Rational>(each)...); }, held);
        auto const computed = std::apply(
            [](auto const&... each) noexcept {
                if constexpr (Exact)
                    return Op::compute_exact(each...);
                else
                    return Op::template compute<Rational>(each...);
            },
            arguments);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        return (*computed)[I];
    }

    /// What a sink is told of a call evaluated for a rounded output: whether it
    /// answered, was absent or failed -- and no value, since none exists yet.
    template <typename Answer>
    [[nodiscard]] constexpr OpaqueEvaluated<Rational, 0> without_values(
        std::expected<std::optional<Answer>, OpaqueCallFailure> const& answered) noexcept
    {
        if (!answered.has_value())
            return std::unexpected { answered.error() };
        if (!answered->has_value())
            return std::optional<std::array<Rational, 0>> {};
        return std::optional<std::array<Rational, 0>> { std::array<Rational, 0> {} };
    }

    /// Evaluates the call @p call for its output @p I, rounded where it is
    /// used: every input, in `Rational`, as `evaluate_call` does, then
    /// `compute_exact` or `compute<Rational>`, keeping output @p I alone. A sink
    /// that asks is told of the call as `evaluate_call` tells it, with
    /// `OpaqueValues::RoundedWhereUsed` and an evaluation of no values.
    template <std::size_t I, typename Op, typename... Inputs, typename Env, typename Sink>
    [[nodiscard]] constexpr auto evaluate_rounded_call(OpaqueCall<Op, Inputs...> const& call, Env const& environment,
                                                       Sink sink) noexcept
    {
        constexpr bool exact = declares_compute_exact<Op, Inputs...>;
        using Answer = typename RoundedOpaqueAnswerOf<Op, exact>::type;
        OpaqueCallInfo callInfo = opaque_call_info(call);
        callInfo.values = OpaqueValues::RoundedWhereUsed;
        if constexpr (HearsOpaque<Sink, Rational, 0>)
            sink.opaque_entered(callInfo);
        std::expected<std::optional<Answer>, OpaqueCallFailure> const answered = evaluate_opaque_inputs<Answer, Rational, 0>(
            call, environment, sink, [](auto const&... held) noexcept { return opaque_rounding_answer<Op, I, exact>(held...); });
        if constexpr (HearsOpaque<Sink, Rational, 0>)
            sink.opaque_produced(callInfo, without_values(answered));
        return answered;
    }

    /// @p computedExactly rounded in @p U: `rounded_in_unit` for a wide value,
    /// `RepRounding<Rational>::round_in` -- a rounding node's own -- for a
    /// `Rational`.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, std::size_t L>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_opaque_value(WideRatio<L> const& computedExactly) noexcept
    {
        return rounded_in_unit(computedExactly, U, Places, Mode);
    }

    template <Unit U, DecimalPlaces Places, RoundingMode Mode>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_opaque_value(Rational computedExactly) noexcept
    {
        return RepRounding<Rational>::round_in(computedExactly, U, Places, Mode);
    }
```

  `exact` is a `constexpr` local read only in the lambda's template argument, which is not an odr-use; if a
  compiler still asks for a capture, make it a template parameter of a named helper instead of capturing.

  After `checked_evaluate_si` for `OpaqueOutputNode` (`:1106`):

```cpp
/// Evaluates a rounded opaque output: the whole call, then output @p I rounded
/// in `U`. Under `Rational`, exactly, through the operation's exact hook when it
/// has one (`detail::evaluate_rounded_call`); a failure of the call, the
/// operation's own or an input's, is this output's, and so is one of the
/// rounding itself. Under any other representation, that representation's own
/// output and its own rounding (`RepRounding<Rep>::round_in`) -- which for
/// `double` refuses to compile, as it does for every rounding node.
template <typename Rep = Rational, std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode,
          typename Origin, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(
    RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node,
    Env const& environment,
    Sink sink = {}) noexcept
{
    if constexpr (RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>::refused)
        return std::unexpected { ArithmeticError::DomainError };
    else if constexpr (!detail::opaque_sound_for<Rep, Op, Inputs...>())
        return std::unexpected { ArithmeticError::DomainError };
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
            if constexpr (std::is_same_v<Rep, Rational>)
            {
                auto const called = detail::evaluate_rounded_call<I>(node.call, environment, sink);
                if (!called.has_value())
                    return std::unexpected { called.error().error };
                if (!called->has_value())
                    return detail::nothing<Rep>();
                std::expected<Rational, ArithmeticError> const rounded = detail::rounded_opaque_value<U, Places, Mode>(**called);
                if (!rounded.has_value())
                    return std::unexpected { rounded.error() };
                return detail::present<Rep>(*rounded);
            }
            else
            {
                OpaqueEvaluated<Rep, Op::outputs.size()> const called = detail::evaluate_call<Rep>(node.call, environment, sink);
                if (!called.has_value())
                    return std::unexpected { called.error().error };
                if (!called->has_value())
                    return detail::nothing<Rep>();
                std::expected<Rep, ArithmeticError> const rounded = RepRounding<Rep>::round_in((**called)[I], U, Places, Mode);
                if (!rounded.has_value())
                    return std::unexpected { rounded.error() };
                return detail::present<Rep>(*rounded);
            }
        }();
        sink.produced(node, evaluated);
        return evaluated;
    }
}
```

- [ ] **Step 9: The file comment and `sink.hpp`.** Append to `opaque.hpp`'s file comment (after `:43`):

```cpp
///
/// **An output the exact layer cannot hold is reported at a declared
/// precision.** `rounded_output<"slope", U, Places, Mode>(call)` is the
/// decimal the operation's true output rounds to in `U` -- exact, even where
/// the output itself is a fraction too wide for `Rational`, as a fit through
/// many readings is. An operation may state its outputs exactly in wider
/// integers for this (`compute_exact`, detected by
/// `detail::declares_compute_exact`; internal, not a customisation point
/// yet); one that does not is evaluated through `compute<Rational>` and
/// rounded exactly. The call is then told to a sink with
/// `OpaqueValues::RoundedWhereUsed`, and its trace names its outputs without
/// values. `opaque_output` is unchanged: the exact output, or `Overflow`.
```

  In `sink.hpp:106-110`, after "... `opaque_produced(info, result)` after `compute`, asked for together
  (`detail::HearsOpaque`); `info` is an `OpaqueCallInfo`, plain data." add: "A call evaluated for a
  `rounded_output` says `info.values == OpaqueValues::RoundedWhereUsed`, and its `result` is an
  `OpaqueEvaluated<Rational, 0>`: whether it answered, was absent or failed, and no value -- none exists until
  the output is rounded. A sink whose `opaque_produced` takes one fixed number of outputs does not hear such a
  call."

- [ ] **Step 10: Run and pass.** `cl.ps1 -Filter "rounded output|opaque|least squares|fit"` and
  `gcc14.sh "rounded output|opaque|least squares|fit"`: the 11 new cases pass, every existing opaque and
  least-squares case passes untouched. Then the full suite on both.

- [ ] **Step 11: The negative tests.** Each of the seven files opens with this preamble, verbatim, after its own
  header comment:

```cpp
#include <formula-cpp/formula.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace
{
struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};
struct Span: formula::Quantity<Span, "r_sp", "the span of the readings", formula::unit::Gram>
{
};

// The span of a series of readings, in the readings' dimension.
struct ReadingSpan
{
    static constexpr std::string_view name = "reading span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "span" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> readings) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const apart = formula::RepTraits<Rep>::subtract(readings[1], readings[0]);
        if (!apart.has_value())
            return std::unexpected { apart.error() };
        return std::array { *apart };
    }
};

inline constexpr auto spanCall =
    formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" }, formula::series<Reading, 2>);
inline constexpr auto twoReadings =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 1273, 10 } },
                                                           formula::Measured<Reading> { formula::Rational { 1394, 10 } }));
} // namespace
```

  Each file's header is `// SPDX-License-Identifier: Apache-2.0`, then `// EXPECT: <text>`, `// REJECT: <text>`
  lines, a comment saying what the case is and why only one message may fire, and `// This must not compile.`
  Their `main()`s:

  - `rounded_output_unit_dimension_mismatch.cpp` -- a span of masses "to 1 dp of s":
    `return formula::checked_evaluate<Span>(formula::rounded_output<"span", formula::unit::Second, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall), twoReadings).has_value() ? 0 : 1;`
  - `rounded_output_offset_unit_dimension_mismatch.cpp` -- the same in `formula::unit::Celsius` (wrong in both
    ways: the dimension's message alone).
  - `rounded_output_unit_has_offset.cpp` -- declares, in its namespace, `struct Probe: formula::Quantity<Probe,
    "T_p", "an invented probe temperature", formula::unit::Kelvin> {};` and
    `inline constexpr auto probeSpan = formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" }, formula::series<Probe, 2>);`,
    and evaluates `formula::checked_evaluate_si<formula::Rational>(formula::rounded_output<"span", formula::unit::Celsius, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(probeSpan), formula::environment(formula::measured_series<Probe>(formula::Measured<Probe> { formula::Rational { 293 } }, formula::Measured<Probe> { formula::Rational { 297 } })))`.
  - `rounded_output_unknown_output.cpp` -- `rounded_output<"spna", formula::unit::Gram, ...>(spanCall)`, evaluated
    for `Span`.
  - `rounded_output_refused_call.cpp` -- `formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" },
    formula::var<Reading>)` (a single value where a series is declared), its `rounded_output<"span",
    formula::unit::Gram, ...>` evaluated for `Span` against `formula::environment(formula::Measured<Reading> {
    formula::Rational { 1273, 10 } })`.
  - `rounded_output_forged_position.cpp` -- built by hand at a position the operation does not declare:
    `constexpr auto forged = formula::RoundedOpaqueOutputNode<5, std::remove_cvref_t<decltype(spanCall)>, formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven> { {}, spanCall };`
    (add `#include <type_traits>`), evaluated for `Span`.
  - `rounded_output_rep_double.cpp` -- `formula::checked_evaluate_si<double>(formula::rounded_output<"span", formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(spanCall), twoReadings)`.

  Register them in `test/CMakeLists.txt` after line 2205, **first with the expected text of each misspelt
  (`"formula: this rounded_output WRONG"`)**, run `cl.ps1 -Filter "negative.rounded_output"` and see seven
  failures, then with the real texts:

```cmake
# rounded_output (`opaque.hpp`): each refusal once, gated on the ones before
# it; every REJECT names what the next check down, or the result's, would add.
formula_add_negative_test(rounded_output_unit_dimension_mismatch
    "formula: this rounded_output names a unit that does not measure the dimension of the output it rounds; the output and the unit appear in this diagnostic as the template arguments of RequireRoundedOutputUnitMatches"
    EXPECT_COUNT 1
    REJECT "formula: this rounded_output names a unit with an offset" "this result quantity does not measure the dimension")
formula_add_negative_test(rounded_output_offset_unit_dimension_mismatch
    "formula: this rounded_output names a unit that does not measure the dimension of the output it rounds"
    EXPECT_COUNT 1
    REJECT "formula: this rounded_output names a unit with an offset")
formula_add_negative_test(rounded_output_unit_has_offset
    "formula: this rounded_output names a unit with an offset, such as degrees Celsius; a fitted coefficient is a difference or a ratio, which an offset unit would misstate; name the unit without its offset"
    EXPECT_COUNT 1
    REJECT "formula: this rounded_output names a unit that does not measure")
formula_add_negative_test(rounded_output_unknown_output
    "formula: this opaque operation has no output of that name; the operation and the name asked for appear in this diagnostic as the template arguments of RequireOpaqueOutputNamed"
    EXPECT_COUNT 1
    REJECT "formula: this rounded_output names a unit" "this result quantity does not measure the dimension")
formula_add_negative_test(rounded_output_refused_call
    "formula: this opaque call passes an input of another shape than the operation declares"
    REJECT "formula: this rounded_output names a unit" "this result quantity does not measure the dimension")
formula_add_negative_test(rounded_output_forged_position
    "formula: this opaque output's position names no output of its operation"
    EXPECT_COUNT 1
    REJECT "formula: this rounded_output names a unit")
# `RepRounding<double>`'s own refusal, as for every rounding node.
formula_add_negative_test(rounded_output_rep_double
    "formula: a rounding node cannot be evaluated with Rep = double"
    REJECT "formula: this rounded_output names a unit")
```

  Run `cl.ps1 -Filter "negative.rounded_output"` and `gcc14.sh "negative.rounded_output"`: seven pass on each
  (g++ also checks the counts). **Deletion checks**, one at a time, each restored with a plain write: delete the
  `static_assert` of `RequireRoundedOutputUnitMatches` -> the two dimension cases compile (fail); of
  `RequireRoundedOutputUnitWithoutOffset` -> `unit_has_offset` fails; the factory's `RequireOpaqueOutputNamed`
  `static_assert` in `rounded_output` -> `unknown_output` fails; `RequireOpaqueShapes`' `static_assert`
  (`opaque.hpp:519`) -> `refused_call` fails; the node's `RequireOpaqueOutputPosition` `static_assert` and
  `OpaqueOutputNode`'s (`:819`) -> `forged_position` fails; `RepRounding<double>`'s (`rounding_node.hpp:190`) ->
  `rep_double` fails. If g++ counts two copies of an EXPECT text, find the second message (defect class 2) --
  do not drop the count.

- [ ] **Step 12: CHANGELOG.** Under `## [Unreleased]` / `### Added`:

```markdown
- `rounded_output<"name", U, Places, Mode>(call)`: one output of an opaque call, rounded to `Places` decimal
  places of the unit `U` under `Mode`, exactly -- the decimal the operation's true output rounds to, even where
  that output is a fraction too wide for `Rational`. It rounds in `U`, never in the coherent unit, and its
  dimension is the output's. It is refused, in the library's words, for an output the operation does not
  declare, for a unit that does not measure the output's dimension and for a unit with an offset, and it does
  not compile under `Rep = double`, as no rounding node does. `rounded<>(opaque_output<>(...))` is unchanged:
  the exact output rounded afterwards, or `Overflow` where the exact output overflows.
- `OpaqueValues`, and `OpaqueCallInfo::values`: a sink hearing an opaque call is told `Exact`, as before, or
  `RoundedWhereUsed` for a call evaluated for a `rounded_output`, whose `opaque_produced` is then handed an
  `OpaqueEvaluated<Rational, 0>` -- whether the call answered, was absent or failed, and no value.
```

  Under `### Changed`: "- `OpaqueCallInfo` gains `values` after `dimensions`, defaulted to `OpaqueValues::Exact`:
  code that builds one with designated initialisers is unaffected; a structured binding over one now has five
  members, not four."

- [ ] **Step 13: Commit.**
  `git add include/formula-cpp/opaque.hpp include/formula-cpp/sink.hpp test/rounded_output_tests.cpp test/negative/rounded_output_*.cpp test/CMakeLists.txt CHANGELOG.md`
  `git commit -m "feat(opaque): rounded_output, an opaque output at a declared precision" -m "An opaque operation's output can be a fraction too wide for Rational -- a line fitted through many readings -- and the exact route can only say Overflow. rounded_output states the unit, places and mode the output is reported at and answers the decimal the true output rounds to, exactly: through an operation's own exact hook in wide integers when it declares one, through compute<Rational> otherwise. rounded<>(opaque_output<>()) keeps its meaning. A sink hearing the call is told it holds no values, since none exists until the output is rounded. Refused in the library's words for an unknown output, a unit of another dimension and a unit with an offset, and under double." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 4: `rounded_output` on the page, under overlays and in the walks

**Files:**
- Modify: `include/formula-cpp/render.hpp` -- after the `OpaqueOutputNode` overload (`:1834-1862`)
- Modify: `include/formula-cpp/document.hpp` -- declaration after `:541-542`, definition after `:1186-1210`
- Modify: `include/formula-cpp/precision.hpp` -- after `LevelChildren<OpaqueOutputNode...>` (`:666-672`)
- Modify: `include/formula-cpp/overlay.hpp` -- after `ConstantRewrite<Sub, OpaqueOutputNode...>` (`:1947-1994`) and
  after `SubstitutedIn<OpaqueOutputNode...>` (`:2503-2508`)
- Modify: `test/least_squares_tests.cpp` (a unit, render, page and overlay cases), `test/rounded_output_tests.cpp`
  (a second operation, the calculation walk and the page)
- Modify: `CHANGELOG.md` (extend the `rounded_output` entry)

**Interfaces:**
- Consumes (Task 3): `RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin>`, `rounded_output`,
  `detail::unrounded(node)`.
- Produces: `render_node<D>(RoundedOpaqueOutputNode const&, V const&)`; `detail::collect(Walk<V>&,
  RoundedOpaqueOutputNode const&)`; `detail::LevelChildren<RoundedOpaqueOutputNode<...>>` (children: the call's
  inputs); `detail::ConstantRewrite<Sub, RoundedOpaqueOutputNode<...>>` and `detail::SubstitutedIn<...>`.

- [ ] **Step 1: Write the failing tests.** In `test/least_squares_tests.cpp`, in the anonymous namespace after
  `Offset` (`:43-45`):

```cpp
// A unit the tests round the fitted rate in, declared here as a method would
// declare it: a millimetre a second, with four decimals, so that the padded
// style writes 0.6786 as it is.
constexpr formula::Unit MillimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr auto roundedSlope =
    formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit);
```

  (`roundedSlope` goes after `fit`, `:59-61`.) At the end of the file:

```cpp
TEST_CASE("rounded output: a rounded slope renders as the rounding it states in every dialect", "[least-squares][render]")
{
    CHECK(formula::render(roundedSlope) == "round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)");
    CHECK(formula::render<formula::Dialect::Markdown>(roundedSlope)
          == "round(linear least squares(`t(i)`, `L(i)`).slope, to 4 dp of mm/s)");
    CHECK(formula::render<formula::Dialect::LaTeX>(roundedSlope)
          == "\\operatorname{round}_{4\\,\\mathrm{mm/s}}(\\text{linear least squares}({t}_{i}, {L}_{i})_{\\text{slope}})");
    // The inputs follow the vocabulary; the operation's name and the unit do not.
    constexpr auto renamed = formula::vocabulary(formula::renames<Elapsed>("t_e"));
    CHECK(formula::render(roundedSlope, renamed) == "round(linear least squares(t_e(i), L(i)).slope, to 4 dp of mm/s)");
}

TEST_CASE("rounded output: a page lists the fit once however its outputs are used", "[least-squares][document]")
{
    constexpr auto both = formula::opaque_output<"intercept">(fit) + roundedSlope * formula::constant<unit::Second>(rat(1));
    formula::Documentation const page = formula::document(both);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "linear least squares");
    CHECK(page.opaqueOperations[0].outputs == std::vector<std::string_view> { "intercept", "slope" });
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].section == "5.1");
    CHECK(page.formula.find("round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)") != std::string::npos);
    REQUIRE(page.symbols.size() == 2); // t and L, each once
}
```

  After the overlay cases (after `:470`):

```cpp
namespace
{
// The scaled fit's slope, rounded where it is used: the scale is read inside
// the call only.
constexpr auto roundedInsideMethod = formula::method(
    formula::variants(formula::variant<Fitted>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            scaledFit))),
    TenthOfMillimetrePerMinute {},
    formula::constraints());
} // namespace

TEST_CASE("rounded output: a constant read inside a rounded output's call is fixed for the whole call",
          "[least-squares][overlay]")
{
    // 19/28 * 1.03 = 0.69892857... mm/s, 0.6989 at 4 dp; 41.934 mm/min, 41.9
    // after the method's rule. The environment has no scale: a use left
    // reading it would not compile.
    constexpr auto fixed = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex)), roundedInsideMethod);
    STATIC_REQUIRE(formula::detail::ConstantRewriteOf<
                   formula::ConstantOverride<Scale>,
                   std::remove_cvref_t<decltype(formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 },
                                                                        formula::RoundingMode::HalfEven>(scaledFit))>>::known);
    auto const outcome = formula::evaluate_method<Fitted>(fixed, fitPoints);
    REQUIRE(outcome.has_value());
    CHECK(outcome->value() == rat(419, 600'000));
}
```

  In `test/rounded_output_tests.cpp`, add `#include <formula-cpp/calculation.hpp>`, `#include
  <formula-cpp/document.hpp>` and `#include <formula-cpp/precision.hpp>`, and in the anonymous namespace:

```cpp
struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};

// One value over another: compute alone, over two single values, so that a
// calculation -- which holds single values -- can define a quantity by it.
struct RatioOfTwo
{
    static constexpr std::string_view name = "ratio of two";
    static constexpr std::array shapes { formula::InputShape::Single, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "ratio" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        return std::array { declared[0] / declared[1] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(Rep dividend, Rep divisor) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const quotientValue = formula::RepTraits<Rep>::divide(dividend, divisor);
        if (!quotientValue.has_value())
            return std::unexpected { quotientValue.error() };
        return std::array { *quotientValue };
    }
};

constexpr formula::Citation shareClause { .title = "Share of two factors", .reference = "Example Standard 7", .section = "3.2" };
constexpr auto roundedShare = formula::rounded_output<"ratio", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
    formula::opaque<RatioOfTwo>(shareClause, formula::var<Factor>, formula::var<Other>));
constexpr auto shareCalculation = formula::calculation(formula::define<Share>(roundedShare));
```

  and the cases:

```cpp
TEST_CASE("rounded output: the walks see the call's inputs", "[rounded-output][precision][calculation]")
{
    using RoundedShare = std::remove_cvref_t<decltype(roundedShare)>;
    STATIC_REQUIRE(formula::detail::LevelChildren<RoundedShare>::seen);
    STATIC_REQUIRE(std::is_same_v<typename formula::detail::LevelChildren<RoundedShare>::type,
                                  std::tuple<formula::VarNode<Factor>, formula::VarNode<Other>>>);
    // A calculation defines a quantity by it and reads what the call reads.
    STATIC_REQUIRE(formula::inputs_of(shareCalculation) == std::array<std::string_view, 2> { "k", "k_o" });
}

TEST_CASE("rounded output: a page lists a call once whether its output is rounded or not", "[rounded-output][document]")
{
    constexpr auto both = formula::opaque_output<"total">(fiveCall)
                          - formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall);
    formula::Documentation const page = formula::document(both);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "reciprocal sum");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.formula == "reciprocal sum(z(i)).total - round(reciprocal sum(z(i)).total, to 6 dp)");
}
```

  (Add `#include <string>`, `<tuple>` and `<type_traits>` to that file's includes; `formula::VarNode` is
  `expression.hpp`'s variable node, `formula::var<Q>` its value.)

- [ ] **Step 2: Run and see them fail.** `cl.ps1 -Filter "rounded output"`. Expected: `BUILD FAILED` -- the render
  and document calls reach the primaries that refuse an unknown node kind (for `document`, its own "formula: ..."
  refusal; for `LevelChildren`, `RequireLevelChildrenFor`), and `render` finds no `render_node`.

- [ ] **Step 3: `render_node`.** After `render.hpp:1862`:

```cpp
/// A rounded opaque output renders as what it states, the output rounded:
/// `round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)`, and in
/// LaTeX `RoundNode`'s subscripted `\operatorname{round}` around the output.
/// That it is one exact operation rather than a rounding of an exact output is
/// how it is evaluated, not what it states -- `rounded_sqrt`'s reasoning,
/// above. The mode is left out, for the reason `RoundNode`'s overload gives,
/// and the trace states it.
template <Dialect D, std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode,
          typename Origin, Vocabulary V>
[[nodiscard]] std::string render_node(RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node,
                                      V const& vocabulary)
{
    std::string const inner = render<D>(detail::unrounded(node), vocabulary);
    constexpr Unit declaredUnit = U;
    std::string const unitSymbol { view(declaredUnit.symbolText) };
    std::string const placesText = std::to_string(Places.value);
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + detail::unit_clause("\\,", detail::latex_unit(unitSymbol)) + "}(" + inner
               + ")";
    else
        return "round(" + inner + ", to " + placesText + " dp" + detail::unit_clause(" of ", unitSymbol) + ")";
}
```

- [ ] **Step 4: `document`.** Declare after `document.hpp:542`:

```cpp
    template <Vocabulary V, std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode,
              typename Origin>
    void collect(Walk<V>& walk, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node);
```

  and define after `:1210`:

```cpp
    /// A rounded opaque output lists its call as an output of that call does
    /// -- once per call, whichever outputs are used and whether they are
    /// rounded -- and reads what the call's inputs read. The precision is in
    /// the formula's text already.
    template <Vocabulary V, std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode,
              typename Origin>
    void collect(Walk<V>& walk, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node)
    {
        collect(walk, unrounded(node));
    }
```

- [ ] **Step 5: `LevelChildren`.** After `precision.hpp:672`:

```cpp
    /// A rounded opaque output's children are its call's inputs, as an
    /// unrounded output's are.
    template <std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin>
    struct LevelChildren<RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>>: LevelParent<Inputs...>
    {
    };
```

- [ ] **Step 6: `ConstantRewrite` and `SubstitutedIn`.** After `overlay.hpp:1994`:

```cpp
    /// A rounded opaque output, rewritten as the output it rounds is -- through
    /// its call's inputs, with the call's citation, and never when the call
    /// was refused -- and rounded as before.
    template <typename Sub, std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode,
              typename Origin>
    struct ConstantRewrite<Sub, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>>
    {
        /// How the output it rounds is rewritten.
        using Output = ConstantRewrite<Sub, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>>;
        /// Whether every input is a kind this header knows, and the call was not refused.
        static constexpr bool known = Output::known;
        /// Whether any input uses `Q`.
        static constexpr bool mentions = Output::mentions;
        /// The same rounding, of the output of the rewritten call.
        using type = RoundedOpaqueOutputNode<I, decltype(Output::type::call), U, Places, Mode, Origin>;

        /// The node, over the rewritten call.
        [[nodiscard]] static constexpr type apply(
            RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& original,
            Sub const& overriding) noexcept
        {
            return type { {}, Output::apply(unrounded(original), overriding).call };
        }
    };
```

  After `:2508`:

```cpp
    template <std::size_t I, typename Op, typename... Inputs, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin>
    struct SubstitutedIn<RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>>
    {
        /// Whatever any of the call's inputs substitutes.
        using type = SubstitutedInAll<Inputs...>;
    };
```

- [ ] **Step 7: Run and pass.** `cl.ps1 -Filter "rounded output|least squares|opaque|vocabulary"` and the same on
  `gcc14.sh`: 6 new cases pass; then the full suite on both, `hygiene.vocabulary-reach` included (the new
  `render_node` passes the vocabulary on; there is no one-argument `render<...>(x)` call). Doxygen on
  `render.hpp`, `document.hpp`, `precision.hpp`, `overlay.hpp`.

- [ ] **Step 8: Mutation checks.** (a) In `ConstantRewrite`, return `original` unchanged from `apply` (and
  `type` = the original node): the overlay case no longer compiles (the scale stays read from an environment
  without one) -- confirm, restore. (b) Render the mode into the plain text: the render case fails. Restore with a
  plain write.

- [ ] **Step 9: CHANGELOG.** Append to the `rounded_output` entry of Task 3: "It renders as the rounding it states,
  `round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)`, a page lists its call once however the call's
  outputs are used, an overlay's constant reaches inside its call as it does inside `opaque_output`'s, and a
  calculation may define a quantity by one."

- [ ] **Step 10: Commit.**
  `git add include/formula-cpp/render.hpp include/formula-cpp/document.hpp include/formula-cpp/precision.hpp include/formula-cpp/overlay.hpp test/least_squares_tests.cpp test/rounded_output_tests.cpp CHANGELOG.md`
  `git commit -m "feat(opaque): render, document, rewrite and walk a rounded opaque output" -m "A rounded output is read everywhere an opaque output is: it renders as the rounding it states, the page lists its call once whichever outputs are used and however, an overlay's constant or derived quantity reaches the call's inputs, and the precision, calculation, rejection and retry walks see those inputs. Each registry treats it as the output it rounds, so the two cannot drift apart." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 5: `rounded_output` in the trace

**Files:**
- Modify: `include/formula-cpp/trace.hpp` -- `StepKind` (`OpaqueOperation`'s comment `:431-455`; append after
  `AttemptInput` `:486`); `Step` field comments `:828-830`, `:873-874`, `:914-917`; `OpaqueStepData`
  `:1390-1412`; `StepKindOf` after `:1913-1917`; `produced`'s comments `:2994-3004`, `:3096-3098` and the push
  `:3249-3251`; `opaque_produced` `:3942-4006`
- Modify: `include/formula-cpp/trace_render.hpp` -- `step_expression` after `:1120-1121`; `OpaqueLine` comment
  `:2402-2423`; `opaque_call_line` `:2543-2588`; a new line function after `opaque_output_line` `:2717-2739`;
  `opaque_line_of` `:2777`; `escaped_step_line` `:2861-2864`
- Modify: `test/least_squares_tests.cpp`, `test/rounded_output_tests.cpp`, `test/consumer_globals_tests.cpp`
  (after `:794`), `test/consumer_globals_run_tests.cpp:16`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes (Tasks 3-4): the node, `OpaqueValues`, `OpaqueCallInfo::values`, the `M == 0` evaluation.
- Produces: `StepKind::RoundedOpaqueOutput`; `OpaqueStepData::values` (`OpaqueValues`) and
  `OpaqueStepData::answered` (`bool`); `detail::rounded_opaque_output_line(ShownStep const&, OpaqueLine const&,
  NumberStyle)`; the call line `<name>(<inputs>) = <out>, <out>: rounded where used [inside not shown] [...]`
  and the output line `round(<out> of #k, to P dp of U) = <value> [<mode>]`.

- [ ] **Step 1: Write the failing tests.** In `test/least_squares_tests.cpp`:

```cpp
TEST_CASE("rounded output: the fit's trace names its outputs without values and states the rounding",
          "[least-squares][trace]")
{
    auto const explained = formula::explain<Rate>(roundedSlope, fitPoints);
    CHECK(explained.outcome.measurement().value() == rat(10179, 250)); // 0.6786 mm/s is 40.716 mm/min
    std::string const fractions = formula::render_trace(explained.trace, { .maxSteps = 30 });
    CHECK(fractions
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm\n"
             "3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm\n"
             "4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]\n");
    std::string const decimals =
        formula::render_trace(explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::exact_decimal() });
    CHECK(decimals
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 10.2 mm; 10.9 mm; 12.1 mm; 14.3 mm\n"
             "3. curve(#1, #2) = 1 s: 10.2 mm; 2 s: 10.9 mm; 4 s: 12.1 mm; 7 s: 14.3 mm\n"
             "4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. round(slope of #4, to 4 dp of mm/s) = 0.6786 mm/s [nearest, ties to even]\n");
    // The rounded decimal is the step's exact value: no style marks it.
    std::string const approximate = formula::render_trace(
        explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) });
    CHECK(approximate == decimals);

    // What the trace holds: the call's row names both outputs, holds no value,
    // and says the call answered; the output's step holds the rounding.
    formula::OpaqueStepData<> const* const callRow = formula::opaque_data(explained.trace, 3);
    REQUIRE(callRow != nullptr);
    CHECK(callRow->values == formula::OpaqueValues::RoundedWhereUsed);
    CHECK(callRow->answered);
    REQUIRE(callRow->outputs.size() == 2);
    CHECK(callRow->outputs[1].name == "slope");
    CHECK(!callRow->outputs[0].value.has_value());
    CHECK(!callRow->outputs[1].value.has_value());
    formula::Step<> const& rounding = explained.trace.steps[4];
    CHECK(rounding.kind == formula::StepKind::RoundedOpaqueOutput);
    CHECK(rounding.granularity == 4);
    CHECK(rounding.mode == formula::RoundingMode::HalfEven);
    CHECK(rounding.unit == MillimetrePerSecond);
    CHECK(rounding.value == rat(3393, 5'000'000));
    REQUIRE(formula::opaque_output_data(explained.trace, 4) != nullptr);
    CHECK(formula::opaque_output_data(explained.trace, 4)->outputIndex == 1);

    // The exact route's row is as it was: every value, Exact.
    auto const exactRoute = formula::explain<Rate>(formula::opaque_output<"slope">(fit), fitPoints);
    CHECK(formula::opaque_data(exactRoute.trace, 3)->values == formula::OpaqueValues::Exact);
    CHECK(formula::opaque_data(exactRoute.trace, 3)->outputs[1].value == rat(19, 28'000));
}

TEST_CASE("rounded output: a padded style pads the rounded value to its unit's decimals", "[least-squares][trace]")
{
    // 19/2 mm at 0 dp is a tie: 10 mm under HalfEven; the millimetre declares one decimal.
    constexpr auto roundedIntercept =
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(fit);
    auto const explained = formula::explain<Offset>(roundedIntercept, fitPoints);
    CHECK(explained.outcome.measurement().value() == rat(10));
    std::string const fractions = formula::render_trace(explained.trace, { .maxSteps = 30 });
    CHECK(fractions.find("5. round(intercept of #4, to 0 dp of mm) = 10 mm [nearest, ties to even]\n") != std::string::npos);
    std::string const padded = formula::render_trace(
        explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded) });
    CHECK(padded.find("5. round(intercept of #4, to 0 dp of mm) = 10.0 mm [nearest, ties to even]\n") != std::string::npos);
}

TEST_CASE("rounded output: a failure reads as the call's own or as carried up from it", "[least-squares][trace]")
{
    // One point: the fit's own DomainError.
    constexpr auto onePoint =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(3) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(103, 10) }));
    constexpr auto single = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 1>, formula::series<Length, 1>), { .reference = "Example Standard 12" });
    formula::Trace<> own {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(single),
        onePoint, formula::RecordingSink { own });
    std::string const ownText = formula::render_trace(own, { .maxSteps = 30 });
    CHECK(ownText.find("4. linear least squares(#3) = argument outside the domain of the operation [inside not shown] "
                       "[the operation itself failed, not any input] [Example Standard 12]\n")
          != std::string::npos);
    CHECK(ownText.find("5. round(slope of #4, to 4 dp of mm/s) = argument outside the domain of the operation "
                       "[the operation itself failed, not any input]\n")
          != std::string::npos);

    // Repeated points: the curve's failure, relayed by the call.
    constexpr auto repeated =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) },
                                                              formula::Measured<Length> { rat(109, 10) },
                                                              formula::Measured<Length> { rat(121, 10) },
                                                              formula::Measured<Length> { rat(143, 10) }));
    formula::Trace<> relayed {};
    (void) formula::detail::dispatch<formula::Rational>(roundedSlope, repeated, formula::RecordingSink { relayed });
    CHECK(formula::render_trace(relayed, { .maxSteps = 30 })
              .find("5. round(slope of #4, to 4 dp of mm/s) = argument outside the domain of the operation [carried up from #4]\n")
          != std::string::npos);

    // An absent length: the call and the output are absent, and say so.
    constexpr auto gap =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(1) },
                                                               formula::Measured<Elapsed> { rat(2) },
                                                               formula::Measured<Elapsed> { rat(4) },
                                                               formula::Measured<Elapsed> { rat(7) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) },
                                                              formula::Measured<Length>::absent(),
                                                              formula::Measured<Length> { rat(121, 10) },
                                                              formula::Measured<Length> { rat(143, 10) }));
    auto const absent = formula::explain<Rate>(roundedSlope, gap);
    CHECK(absent.outcome.is_empty());
    std::string const absentText = formula::render_trace(absent.trace, { .maxSteps = 30 });
    CHECK(absentText.find("4. linear least squares(#3) = (not measured) [inside not shown] [Rate of change, Example Standard 12, 5.1]\n")
          != std::string::npos);
    CHECK(absentText.find("5. round(slope of #4, to 4 dp of mm/s) = (not measured) [nearest, ties to even]\n") != std::string::npos);
}
```

  In `test/rounded_output_tests.cpp` (add `#include <formula-cpp/trace.hpp>` and `<formula-cpp/trace_render.hpp>`):

```cpp
TEST_CASE("rounded output: one call used rounded and plain runs twice and says which is which", "[rounded-output][trace]")
{
    constexpr auto both = formula::opaque_output<"total">(fiveCall)
                          - formula::rounded_output<"total", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfEven>(fiveCall);
    auto const explained = formula::explain<Reciprocals>(both, fiveDraws);
    // 2101205901/58386114749 - 8997/250000.
    CHECK(explained.outcome.measurement().value() == rat(1600853247, 14596528687250000));
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 40 })
          == "1. z = 103; 127; 139; 163; 197\n"
             "2. reciprocal sum(#1) = total = 2101205901/58386114749 [inside not shown] [Reciprocal sum, Example Standard 7, 2.4]\n"
             "3. total of #2 = 2101205901/58386114749\n"
             "4. z = 103; 127; 139; 163; 197\n"
             "5. reciprocal sum(#4) = total: rounded where used [inside not shown] [Reciprocal sum, Example Standard 7, 2.4]\n"
             "6. round(total of #5, to 6 dp) = 8997/250000 [nearest, ties to even]\n"
             "7. #3 - #6 = 1600853247/14596528687250000\n");
}

TEST_CASE("rounded output: a rounding that fails after the call answered is the output's line alone", "[rounded-output][trace]")
{
    constexpr auto productCall = formula::opaque<WideProduct>(productClause, formula::var<Gain>, formula::var<Boost>);
    constexpr std::int64_t twoToForty = std::int64_t { 1 } << 40;
    auto const huge = formula::environment(formula::Measured<Gain> { rat(twoToForty) }, formula::Measured<Boost> { rat(twoToForty) });
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"product", unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(productCall), huge,
        formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 20 })
          == "1. g_1 = 1099511627776\n"
             "2. g_2 = 1099511627776\n"
             "3. wide product(#1, #2) = product: rounded where used [inside not shown] [Product of gains, Example Standard 7, 2.5]\n"
             "4. round(product of #3, to 0 dp) = overflow in exact arithmetic [nearest, ties to even]\n");
    REQUIRE(formula::opaque_data(recorded, 2) != nullptr);
    CHECK(formula::opaque_data(recorded, 2)->answered);
    CHECK(formula::opaque_data(recorded, 2)->failure == formula::OpaqueFailure::None);
}

TEST_CASE("rounded output: a worksheet's derivation shows the rounding", "[rounded-output][calculation][trace]")
{
    auto sheet = formula::worksheet(shareCalculation, formula::environment(formula::Measured<Factor> { rat(2) },
                                                                           formula::Measured<Other> { rat(3) }));
    auto const explained = formula::explain_worksheet<Share>(sheet);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->measurement().value() == rat(6667, 10000));
    CHECK(formula::render_derivation(explained, { .maxSteps = 20 })
          == "s = round(ratio of two(k, k_o).ratio, to 4 dp) = 6667/10000\n"
             "  1. k = 2\n"
             "  2. k_o = 3\n"
             "  3. ratio of two(#1, #2) = ratio: rounded where used [inside not shown] [Share of two factors, Example Standard 7, 3.2]\n"
             "  4. round(ratio of #3, to 4 dp) = 6667/10000 [nearest, ties to even]\n"
             "inputs\n"
             "  k = 2\n"
             "  k_o = 3\n");
    CHECK(formula::render_derivation(explained, { .maxSteps = 20, .numbers = formula::NumberStyle::exact_decimal() })
              .find("  4. round(ratio of #3, to 4 dp) = 0.6667 [nearest, ties to even]\n")
          != std::string::npos);
}

TEST_CASE("rounded output: a step built by hand without its row still says the inside is not shown", "[rounded-output][trace]")
{
    formula::Step<> bare {};
    bare.kind = formula::StepKind::RoundedOpaqueOutput;
    bare.granularity = 4;
    formula::Trace<> recorded {};
    recorded.steps.push_back(bare);
    CHECK(formula::render_trace(recorded, { .maxSteps = 5 })
          == "1. round(an opaque output, to 4 dp) = (not measured) [nearest, ties away from zero] [inside not shown]\n");
}
```

  (The derivation's shape -- header `symbol = definition = value`, numbered steps indented two spaces, then
  `inputs` -- is the one `test/calculation_trace_tests.cpp:737-755` pins; `worksheet` and `explain_worksheet` come
  from `calculation.hpp`, `render_derivation` from `trace_render.hpp`.)

- [ ] **Step 2: Run and see them fail.** `cl.ps1 -Filter "rounded output"`: `BUILD FAILED` -- `StepKind` has no
  `RoundedOpaqueOutput`, `OpaqueStepData` no `values`, and `RecordingSink::produced` finds no `StepKindOf` for the
  node ("use of undefined type 'formula::detail::StepKindOf<...>'", C2027).

- [ ] **Step 3: `StepKind`, `StepKindOf`, the field comments.** Append after `AttemptInput,` (`trace.hpp:486`):

```cpp
    /// One output of an opaque call, rounded where it is used
    /// (`RoundedOpaqueOutputNode`): a single-value step whose operand is the
    /// call's step and whose value is the rounded decimal, exact -- the
    /// unrounded output is never a value. The unit rounded in is `Step::unit`,
    /// the places `Step::granularity` and the mode `Step::mode`, as for
    /// `Round`; which output, in `Trace::opaqueOutputSteps`. The call's row
    /// says `OpaqueValues::RoundedWhereUsed` and holds no values.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `RoundedOpaqueOutputNode`
    /// and its factory `rounded_output`, so nothing in namespace `formula` is
    /// spelt `RoundedOpaqueOutput`.
    RoundedOpaqueOutput,
```

  (The `-Wshadow` sentence is true only once `gcc14.sh` has built it clean -- Step 8; if g++ reports a clash,
  rename and say so.) In `OpaqueOperation`'s comment (`:431-436`), after "each output's name, dimension and
  value": "-- no value when the call was evaluated for a rounded output, `OpaqueStepData::values` --". In the
  `Step` field comments add `RoundedOpaqueOutput` to the kinds listed for `granularity` (`:828`), `mode`
  (`:873`) and `unit` (`:916`). After `StepKindOf<OpaqueOutputNode...>` (`:1913-1917`):

```cpp
    template <std::size_t I, typename Call, Unit U, DecimalPlaces Places, RoundingMode Mode, typename Origin>
    struct StepKindOf<RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin>>
    {
        static constexpr StepKind value = StepKind::RoundedOpaqueOutput;
    };
```

- [ ] **Step 4: `OpaqueStepData` and the recorder.** After `inputsNotEvaluated` (`:1411`):

```cpp
    /// Whether the call held its outputs' values: `RoundedWhereUsed` when it
    /// was evaluated for a `rounded_output`, and then no output has a value.
    OpaqueValues values {};
    /// Whether the call answered -- every input present and the operation
    /// successful. On the rounded route it is the only record that the call
    /// was not absent, since no output holds a value there.
    bool answered {};
```

  In `opaque_produced` (`:3953`), replace the output loop (`:3978-3990`) and add the two fields:

```cpp
        OpaqueStepData<Rep> callRow {};
        callRow.operationName = callInfo.name;
        callRow.values = callInfo.values;
        callRow.answered = result.has_value() && result->has_value();
        // On the rounded route the evaluation holds no values (`M` is 0): every
        // output the operation declares is named, and none has a value.
        std::size_t const outputsNamed = callInfo.values == OpaqueValues::RoundedWhereUsed ? callInfo.outputs.size() : M;
        for (std::size_t outputAt = 0;
             outputAt < outputsNamed && outputAt < callInfo.outputs.size() && outputAt < callInfo.dimensions.size();
             ++outputAt)
        {
            OpaqueOutputValue<Rep> recordedOutput {};
            recordedOutput.name = callInfo.outputs[outputAt];
            recordedOutput.dimension = callInfo.dimensions[outputAt];
            recordedOutput.unit = detail::opaque_output_unit(_trace->steps, callStep.operands, recordedOutput.dimension);
            if constexpr (M > 0)
                if (outputAt < M && callRow.answered)
                    recordedOutput.value = (**result)[outputAt];
            callRow.outputs.push_back(recordedOutput);
        }
```

  In `produced`, the push at `:3249` becomes
  `if constexpr (detail::StepKindOf<N>::value == StepKind::OpaqueOutput || detail::StepKindOf<N>::value == StepKind::RoundedOpaqueOutput)`.
  The comments at `:2994-3004` and `:3096-3098` list `RoundedOpaqueOutputNode` beside `RoundedRootNode` (it
  declares `unit`, `places` and `mode`, read by the same `requires`).

- [ ] **Step 5: The renderer.** In `step_expression` after `:1121` (the `switch` has no default, so without this
  case g++ `-Wswitch` fails the build):

```cpp
            // `render()`'s spelling, as for a rounded root; the output's name is
            // in the call's row, which `rounded_opaque_output_line` reads.
            case StepKind::RoundedOpaqueOutput:
                return "round("
                       + (shownStep.operands.empty() ? std::string { "an opaque output" } : "output of " + sole_operand(shownStep))
                       + ", to " + std::to_string(shownStep.granularity) + " dp"
                       + unit_clause(" of ", unit_symbol_text(shownStep.unit)) + ")";
```

  `OpaqueLine`'s comment: "an `OpaqueOutput` step" becomes "an `OpaqueOutput` or `RoundedOpaqueOutput` step" (both
  places). In `opaque_call_line`, between the error branch and the absent branch (`:2559-2562`):

```cpp
        else if (callRow != nullptr && callRow->values == OpaqueValues::RoundedWhereUsed)
        {
            // No output holds a value: the names, each spending one unit of
            // the budget as a value would, and how they are reported.
            if (!callRow->answered || callRow->outputs.empty())
                lineText += NotMeasuredText;
            else
            {
                std::size_t const outputCount = callRow->outputs.size();
                std::size_t const listed = budget < outputCount ? budget : outputCount;
                budget -= listed;
                for (std::size_t at = 0; at < listed; ++at)
                    lineText += (at > 0 ? ", " : "") + escaped_author_text(callRow->outputs[at].name);
                if (listed < outputCount)
                    lineText += std::string { listed > 0 ? ", " : "" } + "... " + std::to_string(outputCount - listed) + " more";
                lineText += ": rounded where used";
            }
        }
```

  and extend its comment: "A call evaluated for a rounded output names its outputs without values --
  `linear least squares(#3) = intercept, slope: rounded where used` -- since none exists until an output is
  rounded; an absent one, `(not measured)`." After `opaque_output_line` (`:2739`):

```cpp
    /// A rounded opaque output's line, without its number: `round(slope of #4,
    /// to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]`, the output
    /// named from its call's row, as `opaque_output_line` names one.
    ///
    /// The value is the rounded decimal the step holds, exact, so no style
    /// writes `≈` before it; the mode follows in brackets, as a rounding's
    /// does. A failure the call carried reads as the call's: `[the operation
    /// itself failed, not any input]`, or `[carried up from #k]` naming the
    /// call's step, in place of the mode. A failure of the rounding itself,
    /// after the call answered, keeps the mode, as a failed rounding's line
    /// does. Ends `[inside not shown]` unless its operand is the call's own
    /// step (`OpaqueLine::overCall`), as an output's line does.
    [[nodiscard]] inline std::string rounded_opaque_output_line(ShownStep const& recorded,
                                                                OpaqueLine const& opaqueLine,
                                                                NumberStyle numberStyle)
    {
        std::string outputText =
            recorded.operands.empty() ? std::string { "an opaque output" } : "output of " + sole_operand(recorded);
        if (opaqueLine.call != nullptr && opaqueLine.outputIndex.has_value()
            && *opaqueLine.outputIndex < opaqueLine.call->outputs.size())
            outputText = escaped_author_text(opaqueLine.call->outputs[*opaqueLine.outputIndex].name) + " of "
                         + sole_operand(recorded);
        std::string lineText = "round(" + outputText + ", to " + std::to_string(recorded.granularity) + " dp"
                               + unit_clause(" of ", unit_symbol_text(recorded.unit)) + ") = ";
        bool const callFailed = opaqueLine.call != nullptr && opaqueLine.call->failure != OpaqueFailure::None;
        if (recorded.error.has_value() && callFailed)
            lineText += std::string { describe(*recorded.error) }
                        + opaque_failure_suffix(recorded,
                                                opaqueLine.call->failure,
                                                recorded.operands.empty() ? std::nullopt
                                                                          : std::optional<std::size_t> { recorded.operands.front() });
        else if (recorded.error.has_value())
            lineText += std::string { describe(*recorded.error) } + rounding_mode_suffix(recorded.mode);
        else
            lineText += opaque_value_text(recorded.dimension, recorded.unit, recorded.value, numberStyle)
                        + rounding_mode_suffix(recorded.mode);
        return lineText + (opaqueLine.overCall ? "" : " [inside not shown]");
    }
```

  `opaque_line_of` (`:2777`): `if (recorded.kind != StepKind::OpaqueOutput && recorded.kind != StepKind::RoundedOpaqueOutput)`.
  `escaped_step_line` after `:2864`:
  `if (recorded.kind == StepKind::RoundedOpaqueOutput) return rounded_opaque_output_line(recorded, opaqueLine, valueStyle);`
  (`opaque_failure_suffix`'s `Propagated` text reads the output step's own `failedElement`, which the recorder
  never sets for this kind, so it says `[carried up from #4]` with no element.)

- [ ] **Step 6: The consumer-globals probe.** In `test/consumer_globals_tests.cpp` after the `edgeSpan` checks
  (`:794`), so that cl's C4459 guard instantiates the rounded route's evaluation, trace, render and page:

```cpp
    // The span rounded where it is used, 36 mm to 1 dp of mm: evaluated,
    // traced, rendered and documented.
    auto const roundedSpan =
        formula::rounded_output<"span", unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(
            formula::opaque<EdgeSpan>({ .reference = "Example Standard 3" }, formula::series<EdgeX, 2>));
    auto const roundedSpanValue = formula::checked_evaluate<EdgeX>(roundedSpan, spanEdges);
    auto const explainedRoundedSpan = formula::explain<EdgeX>(roundedSpan, spanEdges, north);
    probe.checks.push_back(
        roundedSpanValue.has_value() && roundedSpanValue->measurement().value() == formula::Rational { 36 }
        && formula::render_trace(explainedRoundedSpan.trace, { .maxSteps = 20 }).find("span: rounded where used") != std::string::npos
        && formula::render(roundedSpan, north) == "round(edge span(x_m(i)).span, to 1 dp of mm)"
        && formula::document(roundedSpan, north).opaqueOperations.size() == 1);
```

  and `test/consumer_globals_run_tests.cpp:16`: `REQUIRE(probe.checks.size() == 81);`. Extend the file comment's
  list (`:45-47`): "... a consumer's opaque operation's output, evaluated exactly and in double, and rounded where
  it is used, traced, rendered and documented; ...".

- [ ] **Step 7: CHANGELOG.** `### Added`:

```markdown
- A trace of a `rounded_output`: its call's line names the outputs without values, since none exists until an
  output is rounded -- `linear least squares(#3) = intercept, slope: rounded where used [inside not shown]
  [...]` -- and the output's own line states the rounding: `round(slope of #4, to 4 dp of mm/s) = 3393/5000
  mm/s [nearest, ties to even]`. The rounded decimal is the step's exact value, so no number style marks it
  `≈`. A failure the call carried reads as the call's on the output's line too. `OpaqueStepData` records
  `values` and `answered`.
```

  `### Changed`:

```markdown
- `StepKind` gains `RoundedOpaqueOutput`, after `AttemptInput`: a `switch` over `StepKind` that names every
  enumerator now warns of the one it does not.
- `OpaqueStepData` gains `values` and `answered`, after `inputsNotEvaluated`: a structured binding over one now
  has seven members, not five.
```

- [ ] **Step 8: Run and pass.** `cl.ps1 -Filter "rounded output|least squares|opaque|trace|consumer"` and the same
  on `gcc14.sh` (g++ is the one that reports `-Wswitch` and `-Wshadow` here: the build must be clean). 8 new cases;
  every existing trace line passes untouched. Full suite on both. Doxygen on `trace.hpp` and `trace_render.hpp`.

- [ ] **Step 9: Mutation checks.** (a) Drop the `RoundedWhereUsed` branch of `opaque_call_line`: the fit's
  fraction trace fails (the call reads `(not measured)`). (b) Append the mode on a carried failure too: the
  one-point case fails. (c) Record values on the rounded route anyway (drop `outputsNamed`): the `callRow`
  checks fail. Restore each with a plain write.

- [ ] **Step 10: Commit.**
  `git add include/formula-cpp/trace.hpp include/formula-cpp/trace_render.hpp test/least_squares_tests.cpp test/rounded_output_tests.cpp test/consumer_globals_tests.cpp test/consumer_globals_run_tests.cpp CHANGELOG.md`
  `git commit -m "feat(trace): trace a rounded opaque output without writing the unrounded value" -m "A value the exact layer cannot hold must not appear in a trace, even as a fraction of a call's outputs. A call evaluated for a rounded output names its outputs without values, and the output's own step holds the rounded decimal -- exact, so no number style marks it approximate -- with its unit, places and mode, as a rounding step does. A failure the call carried reads as the call's on the output's line. The new step kind is appended to StepKind." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 6: `LinearLeastSquares::compute_exact`, and where the rounded fit stops

**Files:**
- Modify: `include/formula-cpp/least_squares.hpp` -- file comment `:16-37`; after `compute` (`:177`, inside the
  struct); include `<formula-cpp/detail/wide_int.hpp>` and `<formula-cpp/detail/wide_rounding.hpp>`
- Modify: `test/least_squares_tests.cpp`
- Modify: `test/overflow_census_tests.cpp` -- helpers after `fit_node_overflows` (`:404-424`); the least-squares
  case `:711-764`
- Modify: `docs/numeric-headroom.md` -- the least-squares prose `:296-306`, `:317-324`; "What this does not decide"
  `:344-352`; the table between `<!-- census:least-squares -->` markers (regenerated, never typed)
- Modify: `test/consumer_globals_tests.cpp` (after the `fitSlope` check, `:802`), `test/consumer_globals_run_tests.cpp:16`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes (Tasks 1-5): `WideUnsigned`, `WideSigned` and their `*_checked_or_none`, `lcm_checked_or_none`,
  `scaled_to_denominator`, `WideRatio`, `rounded_in_unit`, `declares_compute_exact`, `rounded_output`.
- Produces: `LinearLeastSquares::exact_limbs` (`8`) and
  `static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 2>, ArithmeticError> compute_exact(std::span<Rational const> domainPoints, std::span<Rational const> pointValues) noexcept`
  -- intercept then slope, in coherent units, exact, not reduced.

- [ ] **Step 1: Write the failing tests** in `test/least_squares_tests.cpp`. Every one runs at run time (8 limbs:
  see Global Constraints). Add, in the anonymous namespace, a helper:

```cpp
template <formula::detail::FixedString Name, formula::Unit U, formula::DecimalPlaces Places, formula::RoundingMode Mode,
          typename Call, typename Env>
void check_rounded_route(Call const& call, Env const& inputs)
{
    auto const fused = formula::checked_evaluate_si<formula::Rational>(formula::rounded_output<Name, U, Places, Mode>(call), inputs);
    auto const afterwards =
        formula::checked_evaluate_si<formula::Rational>(formula::rounded<U, Places, Mode>(formula::opaque_output<Name>(call)), inputs);
    REQUIRE(afterwards.has_value());
    REQUIRE(fused.has_value());
    CHECK(**fused == **afterwards);
}

template <formula::detail::FixedString Name, formula::Unit U, formula::DecimalPlaces Places, typename Call, typename Env>
void check_rounded_route_in_every_mode(Call const& call, Env const& inputs)
{
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfAwayFromZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfTowardZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfEven>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::Ceiling>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::Floor>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::TowardZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::AwayFromZero>(call, inputs);
}

/// The intercept of @p call over @p inputs, rounded to 4 dp of mm under @p Mode, in metres.
template <formula::RoundingMode Mode, typename Call, typename Env>
[[nodiscard]] formula::Rational rounded_intercept(Call const& call, Env const& inputs)
{
    return **formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }, Mode>(call), inputs);
}
```

  (`distinct_denominators<N>()` exists at `:212-226`; move that anonymous-namespace block above the new cases if
  they come first in the file.) The cases:

```cpp
TEST_CASE("rounded output: compute_exact states the fit exactly", "[least-squares]")
{
    // The hook is taken. A hook that is not noexcept, has fewer than 4 limbs or
    // returns another type is skipped silently for compute<Rational>, so this
    // is the only thing that notices one.
    STATIC_REQUIRE(formula::detail::declares_compute_exact<
                   formula::LinearLeastSquares,
                   std::remove_cvref_t<decltype(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>))>>);
    auto const exact = formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { inOrderTimes },
                                                                  std::span<formula::Rational const> { inOrderLengths });
    REQUIRE(exact.has_value());
    CHECK(*formula::detail::narrow_wide_ratio((*exact)[0]) == rat(19, 2'000));  // 9.5 mm in metres
    CHECK(*formula::detail::narrow_wide_ratio((*exact)[1]) == rat(19, 28'000)); // 19/28 mm/s in m/s
    // The same refusals as compute: one point, all equal, spans of different lengths.
    std::array<formula::Rational, 4> const sameTime { rat(3), rat(3), rat(3), rat(3) };
    CHECK(formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { sameTime },
                                                     std::span<formula::Rational const> { inOrderLengths })
              .error()
          == formula::ArithmeticError::DomainError);
    CHECK(formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { inOrderTimes }.first(3),
                                                     std::span<formula::Rational const> { inOrderLengths })
              .error()
          == formula::ArithmeticError::DomainError);
}

TEST_CASE("rounded output: wherever the exact fit answers the rounded fit is it rounded in every mode", "[least-squares]")
{
    // The fixture: the slope at 4 dp of mm/s and at 2 dp of mm/min (a factor
    // that is no power of ten), the intercept at 0 dp of mm (a tie) and to
    // tens of mm.
    check_rounded_route_in_every_mode<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"slope", unit::MillimetrePerMinute, formula::DecimalPlaces { 2 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { 0 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { -1 }>(fit, fitPoints);
    // Five distinct denominators, where the exact route still answers.
    constexpr auto five = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 5>, formula::series<Length, 5>), { .reference = "Example Standard 12" });
    check_rounded_route_in_every_mode<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }>(five, distinct_denominators<5>());
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }>(five, distinct_denominators<5>());
    // The pinned values, so that both routes agreeing on a wrong number cannot pass.
    auto const slope = formula::checked_evaluate<Rate>(roundedSlope, fitPoints);
    CHECK(slope->measurement().value() == rat(10179, 250)); // 0.6786 mm/s in mm/min
    auto const toTens = formula::checked_evaluate<Offset>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { -1 }, formula::RoundingMode::Floor>(fit), fitPoints);
    CHECK(toTens->measurement().value() == rat(0)); // 9.5 mm down to tens
}

TEST_CASE("rounded output: the rounded fit answers where the exact fit overflows", "[least-squares]")
{
    constexpr auto fifteen = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 15>, formula::series<Length, 15>), { .reference = "Example Standard 12" });
    auto const exactRoute = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fifteen), distinct_denominators<15>());
    REQUIRE(!exactRoute.has_value());
    CHECK(exactRoute.error() == formula::ArithmeticError::Overflow);
    // 1.93724895... mm/s: 1.9372 at 4 dp, 116.232 mm/min; 1.9373 upwards.
    auto const even = formula::checked_evaluate<Rate>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fifteen),
        distinct_denominators<15>());
    REQUIRE(even.has_value());
    CHECK(even->measurement().value() == rat(14529, 125));
    auto const upwards = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::Ceiling>(fifteen),
        distinct_denominators<15>());
    CHECK(**upwards == rat(19373, 10'000'000));
    // A negative intercept, -0.017688... mm: the sign decides the directed modes.
    CHECK(rounded_intercept<formula::RoundingMode::HalfEven>(fifteen, distinct_denominators<15>()) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::Floor>(fifteen, distinct_denominators<15>()) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::AwayFromZero>(fifteen, distinct_denominators<15>()) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::Ceiling>(fifteen, distinct_denominators<15>()) == rat(-11, 625'000));
    CHECK(rounded_intercept<formula::RoundingMode::TowardZero>(fifteen, distinct_denominators<15>()) == rat(-11, 625'000));

    // Under the approximate style other lines carry the marker; the rounded line does not.
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fifteen),
        distinct_denominators<15>(), formula::RecordingSink { recorded });
    std::string const approximate = formula::render_trace(
        recorded, { .maxSteps = 100, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) });
    CHECK(approximate.find("\xe2\x89\x88") != std::string::npos);
    CHECK(approximate.find("5. round(slope of #4, to 4 dp of mm/s) = 1.9372 mm/s [nearest, ties to even]\n") != std::string::npos);
}

TEST_CASE("rounded output: a rounded fit that outgrows 256 bits is the operation's Overflow", "[least-squares]")
{
    // Measured with this algorithm: 57 points on distinct denominators fit,
    // 58 do not, in compute_exact itself.
    constexpr auto fiftySeven = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 57>, formula::series<Length, 57>), { .reference = "Example Standard 12" });
    constexpr auto fiftyEight = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 58>, formula::series<Length, 58>), { .reference = "Example Standard 12" });
    CHECK(formula::checked_evaluate_si<formula::Rational>(
              formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fiftySeven),
              distinct_denominators<57>())
              .has_value());
    formula::Trace<> recorded {};
    auto const overflowing = formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fiftyEight),
        distinct_denominators<58>(), formula::RecordingSink { recorded });
    REQUIRE(!overflowing.has_value());
    CHECK(overflowing.error() == formula::ArithmeticError::Overflow);
    REQUIRE(formula::opaque_data(recorded, 3) != nullptr);
    CHECK(formula::opaque_data(recorded, 3)->failure == formula::OpaqueFailure::Own);
    CHECK(formula::render_trace(recorded, { .maxSteps = 400 })
              .find("5. round(slope of #4, to 4 dp of mm/s) = overflow in exact arithmetic [the operation itself failed, not any input]\n")
          != std::string::npos);
}
```

- [ ] **Step 2: Run and see them fail.** `cl.ps1 -Filter "rounded output"`: `BUILD FAILED`, `C2039:
  'compute_exact': is not a member of 'formula::LinearLeastSquares'`.

- [ ] **Step 3: Implement `compute_exact`** inside `LinearLeastSquares`, after `compute`:

```cpp
    /// The width `compute_exact` works in: eight limbs, 256 bits.
    static constexpr std::size_t exact_limbs = 8;

    /// The fit, exactly, for a `rounded_output`: intercept then slope, in
    /// coherent units, as fractions of `exact_limbs`-limb integers, not
    /// reduced. The same closed form as `compute`, over integer sums: each
    /// series is first brought to one common denominator -- `X = x Dx`,
    /// `Y = y Dy` -- and then, with `n` points,
    ///
    ///     slope     = (n Sxy - Sx Sy) Dx / ((n Sxx - Sx^2) Dy)
    ///     intercept = (Sy Sxx - Sx Sxy)   / ((n Sxx - Sx^2) Dy)
    ///
    /// where `Sx` is the sum of `X`, `Sxy` of `X Y`, and so on. Nothing is
    /// rounded here; `rounded_output` rounds the one output it is asked for.
    ///
    /// The same refusals as `compute`, decided the same way: spans of
    /// different lengths, and fewer than two distinct points, are the fit's own
    /// `DomainError`. A common denominator, a sum or a product that leaves 256
    /// bits is `Overflow` -- on readings with a different denominator on every
    /// point from 58 points (`docs/numeric-headroom.md`), never a wrong line.
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 2>, ArithmeticError> compute_exact(
        std::span<Rational const> domainPoints, std::span<Rational const> pointValues) noexcept
    {
        using Wide = detail::WideUnsigned<exact_limbs>;
        using Signed = detail::WideSigned<exact_limbs>;
        if (domainPoints.size() != pointValues.size())
            return std::unexpected { ArithmeticError::DomainError };
        bool anotherPoint = false;
        for (Rational const& each: domainPoints)
            if (!(each == domainPoints[0]))
                anotherPoint = true;
        if (!anotherPoint)
            return std::unexpected { ArithmeticError::DomainError };

        // One common denominator for the points, and one for the values.
        std::optional<Wide> pointScale = Wide::from_u64(1);
        std::optional<Wide> valueScale = Wide::from_u64(1);
        for (std::size_t at = 0; at < domainPoints.size(); ++at)
        {
            if (pointScale)
                pointScale = detail::lcm_checked_or_none(
                    *pointScale, Wide::from_u64(static_cast<std::uint64_t>(domainPoints[at].denominator())));
            if (valueScale)
                valueScale = detail::lcm_checked_or_none(
                    *valueScale, Wide::from_u64(static_cast<std::uint64_t>(pointValues[at].denominator())));
        }
        if (!pointScale || !valueScale)
            return std::unexpected { ArithmeticError::Overflow };

        // The four integer sums: of X, of Y, of X^2 and of X Y.
        Signed sumOfPoints {};
        Signed sumOfValues {};
        Signed sumOfSquares {};
        Signed sumOfProducts {};
        for (std::size_t at = 0; at < domainPoints.size(); ++at)
        {
            std::optional<Signed> const scaledPoint = detail::scaled_to_denominator(domainPoints[at], *pointScale);
            std::optional<Signed> const scaledValue = detail::scaled_to_denominator(pointValues[at], *valueScale);
            if (!scaledPoint || !scaledValue)
                return std::unexpected { ArithmeticError::Overflow };
            std::optional<Signed> const squareTerm = detail::mul_checked_or_none(*scaledPoint, *scaledPoint);
            std::optional<Signed> const productTerm = detail::mul_checked_or_none(*scaledPoint, *scaledValue);
            std::optional<Signed> const withPoint = detail::add_checked_or_none(sumOfPoints, *scaledPoint);
            std::optional<Signed> const withValue = detail::add_checked_or_none(sumOfValues, *scaledValue);
            std::optional<Signed> const withSquare =
                squareTerm ? detail::add_checked_or_none(sumOfSquares, *squareTerm) : std::nullopt;
            std::optional<Signed> const withProduct =
                productTerm ? detail::add_checked_or_none(sumOfProducts, *productTerm) : std::nullopt;
            if (!withPoint || !withValue || !withSquare || !withProduct)
                return std::unexpected { ArithmeticError::Overflow };
            sumOfPoints = *withPoint;
            sumOfValues = *withValue;
            sumOfSquares = *withSquare;
            sumOfProducts = *withProduct;
        }

        // n Sxx - Sx^2, n Sxy - Sx Sy and Sy Sxx - Sx Sxy.
        Signed const pointCount { false, Wide::from_u64(domainPoints.size()) };
        std::optional<Signed> const countedSquares = detail::mul_checked_or_none(pointCount, sumOfSquares);
        std::optional<Signed> const squaredSum = detail::mul_checked_or_none(sumOfPoints, sumOfPoints);
        std::optional<Signed> const countedProducts = detail::mul_checked_or_none(pointCount, sumOfProducts);
        std::optional<Signed> const crossSum = detail::mul_checked_or_none(sumOfPoints, sumOfValues);
        std::optional<Signed> const valuesBySquares = detail::mul_checked_or_none(sumOfValues, sumOfSquares);
        std::optional<Signed> const pointsByProducts = detail::mul_checked_or_none(sumOfPoints, sumOfProducts);
        if (!countedSquares || !squaredSum || !countedProducts || !crossSum || !valuesBySquares || !pointsByProducts)
            return std::unexpected { ArithmeticError::Overflow };
        std::optional<Signed> const pointSpread = detail::sub_checked_or_none(*countedSquares, *squaredSum);
        std::optional<Signed> const riseTerm = detail::sub_checked_or_none(*countedProducts, *crossSum);
        std::optional<Signed> const interceptTerm = detail::sub_checked_or_none(*valuesBySquares, *pointsByProducts);
        if (!pointSpread || !riseTerm || !interceptTerm)
            return std::unexpected { ArithmeticError::Overflow };
        // A backstop only: distinct points were checked above.
        if (pointSpread->negative || pointSpread->magnitude.is_zero())
            return std::unexpected { ArithmeticError::DomainError };

        std::optional<Wide> const slopeNumerator = detail::mul_checked_or_none(riseTerm->magnitude, *pointScale);
        std::optional<Wide> const sharedDenominator = detail::mul_checked_or_none(pointSpread->magnitude, *valueScale);
        if (!slopeNumerator || !sharedDenominator)
            return std::unexpected { ArithmeticError::Overflow };
        return std::array { detail::WideRatio<exact_limbs> { interceptTerm->negative, interceptTerm->magnitude, *sharedDenominator },
                            detail::WideRatio<exact_limbs> { riseTerm->negative, *slopeNumerator, *sharedDenominator } };
    }
```

  **This is the algorithm the probe measured** (first failure at 58 points in both distinct fixtures, 0 of 127 at 3
  dp). Do not reduce, reorder the products or cancel `Dx` against `Dy` without re-measuring: the pinned sizes
  below would move. `compute_exact` is a non-template member of a non-template struct, so cl checks its names in
  every translation unit that includes the header: none of them may be a consumer global (the list's `points`,
  `slope`, `intercept`, `spread`, `level`, `sum`, `factor`, `count`, `next` are avoided on purpose).

  The file comment (`:16-37`): after the census sentence ("... the first at 34.") add: "**Rounded where it is
  used, it answers further.** `rounded_output<"slope", U, Places, Mode>(fit)` runs `compute_exact`, the same
  closed form over 256-bit integer sums, and reports the slope at the precision the method declares: on those
  readings at every size from 2 to 128 points. A different denominator on every point outgrows even that from 58
  points, and the answer is `Overflow`." And in the `double` paragraph (`:29-37`), after "...the citation goes
  nowhere.": "Where the exact fit overflows, `rounded_output` is the traced answer, not `double`."

- [ ] **Step 4: The census rows.** In `test/overflow_census_tests.cpp` after `fit_node_overflows` (`:424`):

```cpp
/// The coherent unit of a force over a time, N/s: the rounded fit's unit here.
inline constexpr formula::Unit newtonPerSecond { .dimension = formula::dim::Force / formula::dim::Time,
                                                 .symbolText = formula::symbol("N/s"),
                                                 .decimals = 4 };

/// The slope over the first @p count points of @p shape, rounded to 4 dp of
/// N/s the way `rounded_output` rounds it -- `LinearLeastSquares::compute_exact`,
/// then `detail::rounded_in_unit` -- and whether it overflowed.
template <typename Shape>
[[nodiscard]] bool rounded_fit_overflows(Shape shape, std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> forces;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        forces.push_back(point.y);
    }
    auto const fitted = formula::LinearLeastSquares::compute_exact(std::span<Rational const> { times },
                                                                   std::span<Rational const> { forces });
    if (!fitted.has_value())
        return fitted.error() == formula::ArithmeticError::Overflow;
    auto const slope = formula::detail::rounded_in_unit((*fitted)[1], newtonPerSecond, formula::DecimalPlaces { 4 },
                                                        formula::RoundingMode::HalfEven);
    return !slope.has_value() && slope.error() == formula::ArithmeticError::Overflow;
}

/// `scan_fit` for the rounded route.
template <typename Shape>
[[nodiscard]] FitScan scan_rounded_fit(Shape shape)
{
    FitScan found;
    for (std::size_t count = 2; count <= 128; ++count)
    {
        bool overflowed = false;
        Used const used = census_of([&] { overflowed = rounded_fit_overflows(shape, count); });
        if (overflowed)
            found.overflowing.push_back(count);
        else
            found.leastHeadroom = std::min(found.leastHeadroom, used.headroom());
    }
    return found;
}

/// The first @p N points of @p shape through the node --
/// `rounded_output<"slope", N/s, 4 dp>` of `linear_least_squares` over a curve
/// -- and whether it overflowed.
template <std::size_t N, typename Shape>
[[nodiscard]] bool rounded_fit_node_overflows(Shape shape)
{
    std::array<formula::Measured<FitTime>, N> times;
    std::array<formula::Measured<FitForce>, N> forces;
    for (std::size_t at = 0; at < N; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times[at] = formula::Measured<FitTime> { point.x };
        forces[at] = formula::Measured<FitForce> { point.y };
    }
    auto const inputs =
        formula::environment(formula::MeasuredSeries<FitTime, N> { times }, formula::MeasuredSeries<FitForce, N> { forces });
    constexpr auto fit = formula::linear_least_squares(
        formula::curve(formula::series<FitTime, N>, formula::series<FitForce, N>), { .reference = "Example Standard 12" });
    auto const slope = formula::checked_evaluate_si<Rational>(
        formula::rounded_output<"slope", newtonPerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit), inputs);
    return !slope.has_value() && slope.error() == formula::ArithmeticError::Overflow;
}
```

  In the least-squares case, after `FitScan const distinct = scan_fit(distinct_denominators_point);` (`:745`):

```cpp
    FitScan const roundedThree = scan_rounded_fit(three_decimals_point);
    FitScan const roundedDistinct = scan_rounded_fit(distinct_denominators_point);
```

  after the third `emit` row (`:750`):

```cpp
    emit("least-squares", roundedThree.row("the slope rounded to 4 dp by rounded_output: readings at 3 dp near 2410 N (realistic)"));
    emit("least-squares", roundedDistinct.row("the slope rounded to 4 dp by rounded_output: a different denominator on every point (stress control)"));
```

  and at the end of the case:

```cpp
    // The rounded route, measured with this algorithm: the realistic readings
    // never outgrow 256 bits; a different denominator on every point does,
    // in compute_exact, from 58 points. The node agrees on both sides of 58.
    CHECK(roundedThree.overflowing.empty());
    REQUIRE(!roundedDistinct.overflowing.empty());
    CHECK(roundedDistinct.overflowing.front() == 58);
    CHECK(roundedDistinct.overflowing.size() == 71);
    CHECK(!rounded_fit_node_overflows<128>(three_decimals_point));
    CHECK(!rounded_fit_node_overflows<57>(distinct_denominators_point));
    CHECK(rounded_fit_node_overflows<58>(distinct_denominators_point));
```

  If the scan gives other sizes, the implementation is not the algorithm above: find where it differs; never move
  the pin to fit it.

- [ ] **Step 5: The consumer-globals probe.** After the `fitSlope` check (`test/consumer_globals_tests.cpp:802`):

```cpp
    // The same slope, 73/110, rounded where it is used through the fit's exact
    // hook: 0.664 at 3 dp.
    auto const roundedFitSlope = formula::checked_evaluate<Factor>(
        formula::rounded_output<"slope", unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven>(edgeFit), specimen);
    probe.checks.push_back(roundedFitSlope.has_value() && roundedFitSlope->measurement().value() == formula::Rational { 83, 125 });
```

  and `consumer_globals_run_tests.cpp:16` -> `82`; the file comment's "a least-squares fit" becomes "a least-squares
  fit, exact and rounded where it is used".

- [ ] **Step 6: Run, then regenerate the page on cl.** `cl.ps1 -Filter "rounded output|least squares|consumer"`, then
  `cl.ps1 -Target formula-cpp-census-page -NoTest` (rewrites the `least-squares` table in
  `docs/numeric-headroom.md`), then `cl.ps1 -Filter "census|docs.numeric-headroom"`. The table gains two rows
  reading `0 of 127 | none | <h>` and `71 of 127 | 58 points | <h>`, `<h>` whatever the census measured. Then
  `gcc14.sh "rounded output|least squares|consumer|census|docs.numeric-headroom"` and the full suite on both.

- [ ] **Step 7: The page's prose** (`docs/numeric-headroom.md`). After "... is checked against it at 33 and 34
  points." (`:305-306`) add: "The last two rows fit the same shapes the way `rounded_output` does: the slope
  reported to 4 decimal places of N/s, computed by `LinearLeastSquares::compute_exact` in 256-bit integers and
  rounded exactly; the node is checked against that at 57, 58 and 128 points." Replace "It has no traced fallback
  in `double`: ..." (`:320-324`) by: "Where it overflows, a method that states the precision it reports the slope
  at gets that instead, from `rounded_output`: exact, traced and documented, at every size here for readings at 3
  decimal places, and `Overflow` from 58 points on a different denominator for every point, where even 256 bits
  are outgrown. There is no traced fallback in `double`: a curve evaluates only in `Rational`, so
  `checked_evaluate_si<double>` over a fit is refused. `LinearLeastSquares::compute<double>` can be called
  directly, on numbers already in coherent units, but nothing it returns is checked, traced, rendered or
  documented." In "What this does not decide" (`:344-352`), after its first sentence: "Beside them, a formula can
  declare the precision a value is reported at, and `rounded_output` computes that decimal in wider integers
  ([Displaying numbers](display.md#values-the-exact-layer-cannot-hold)); that answers for the one output
  reported, not for `Rational` itself."

  (The link's anchor exists once Task 7 has added the section; `mkdocs build --strict`, run by the controller at
  the end, checks it.)

- [ ] **Step 8: CHANGELOG.** `### Added`: "- `LinearLeastSquares::compute_exact`: the fit in 256-bit integers,
  so that `rounded_output<"slope", ...>(linear_least_squares(...))` answers where the exact route overflows -- on
  readings at 3 decimal places of a few thousand newtons at every size from 2 to 128 points, where
  `opaque_output<"slope">` overflows from 34. A different denominator on every point outgrows it from 58 points,
  and the answer is `Overflow` (`docs/numeric-headroom.md`)."

- [ ] **Step 9: Mutation checks.** (a) Swap the intercept's and slope's order in the returned array: the fixture
  case fails. (b) Drop `* Dx` from the slope (use `riseTerm->magnitude`): the equivalence cases fail. (c) Return
  `DomainError` for a negative intercept term: the 15-point intercept cases fail. (d) Drop `noexcept` from
  `compute_exact`: the hook is then skipped silently, and the `declares_compute_exact` `STATIC_REQUIRE` must
  fail the build. Restore each with a plain write.

- [ ] **Step 10: Commit.**
  `git add include/formula-cpp/least_squares.hpp test/least_squares_tests.cpp test/overflow_census_tests.cpp docs/numeric-headroom.md test/consumer_globals_tests.cpp test/consumer_globals_run_tests.cpp CHANGELOG.md`
  `git commit -m "feat(least_squares): the fit in wide integers, so that a rounded slope answers where the exact one overflows" -m "A least-squares slope through 34 readings at three decimals already needs more than 64 bits as an exact fraction, and the exact route can only say Overflow. compute_exact is the same closed form over integer sums on a common denominator, in 256 bits, and rounded_output rounds the one coefficient a method reports: on those readings it answers at every size to 128 points, and agrees with the exact route in every mode wherever that answers. The census page pins where it stops: 58 points on a different denominator for every point." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

### Task 7: Documentation -- the display rule, the guides and the example

**Files:**
- Modify: `docs/display.md` (the rule `:7-9`; a section after "What no style rounds", before `:240`)
- Modify: `docs/opaque-and-retry.md` (the least-squares section `:168-223`)
- Modify: `examples/opaque_and_retry.cpp` (comment `:12-14`; after `slope` `:118`; printing `:276-299`),
  `examples/CMakeLists.txt:274` (pass regex)
- Modify: `docs/expressions.md` (after `:343-352`)
- Modify: comments in `include/formula-cpp/trace_render.hpp` (`TraceRenderOptions::numbers` `:104-126`;
  `render_trace` `:3074-3079`), `include/formula-cpp/trace.hpp` (`explain` `:4251-4257`),
  `include/formula-cpp/evaluate.hpp` (`:14-23`)
- Modify: `docs/numeric-headroom.md` (regenerated on cl: the example's census twin changes)
- Modify: `CHANGELOG.md`

**Interfaces:** Consumes everything above; produces no code interface.

- [ ] **Step 1: The example.** In `examples/opaque_and_retry.cpp`, item 3 of the header comment (`:12-14`) becomes:

```cpp
//   3. A least-squares line is such an operation: exact in Rational, refused
//      for a degenerate set of points, and Overflow -- never a wrong line --
//      when its exact sums leave Rational's range. Rounded where it is used,
//      to the precision the method reports it at, it answers there too.
```

  After `constexpr auto slope = formula::opaque_output<"slope">(fit);` (`:118`):

```cpp
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr auto roundedSlope =
    formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit);
```

  In section 3's printing, the `fifteen distinct denominators: %s\n\n` `printf` (`:295`) ends in one `\n`, and after
  its `check` (`:297`):

```cpp
    std::printf("%s\n", formula::render(roundedSlope).c_str());
    auto const roundedRate = formula::explain<Rate>(roundedSlope, points);
    std::printf("%s\n", formula::render_trace(roundedRate.trace, { .maxSteps = 20 }).c_str());
    check(roundedRate.outcome.measurement().value() == formula::Rational { 10179, 250 }, "0.6786 mm/s is 40.716 mm/min");

    auto const roundedWide = formula::checked_evaluate<Rate>(
        formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fifteen),
        distinctDenominators());
    check(roundedWide.has_value() && roundedWide->measurement().value() == formula::Rational { 14529, 125 },
          "rounded where used, fifteen distinct denominators answer: 1.9372 mm/s");
    if (roundedWide.has_value())
    {
        formula::NumberText const wideSlope = formula::number_text(roundedWide->measurement(), formula::NumberStyle::exact_decimal());
        std::printf("fifteen distinct denominators, rounded where used: %.*s\n\n", static_cast<int>(wideSlope.view().size()),
                    wideSlope.view().data());
    }
```

  The program then prints, after the overflow line (this is what Step 2's guide block quotes):

```text
fifteen distinct denominators: overflow in exact arithmetic
round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm
4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] [Rate of change, Example Standard 12, 5.1]
5. round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]

fifteen distinct denominators, rounded where used: 116.232 mm/min
```

  Copy the guide's blocks from the program's real output, not from this plan. In `examples/CMakeLists.txt:274`,
  after `fifteen distinct denominators: overflow in exact arithmetic` in the regex, insert (no literal `;`):
  `.*round\(linear least squares\(t\(i\), L\(i\)\)\.slope, to 4 dp of mm/s\).*4\. linear least squares\(#3\) = intercept, slope: rounded where used \[inside not shown\].*5\. round\(slope of #4, to 4 dp of mm/s\) = 3393/5000 mm/s \[nearest, ties to even\].*fifteen distinct denominators, rounded where used: 116\.232 mm/min`,
  and extend the comment above it (`:266-273`) with "the slope rounded where it is used, and on fifteen distinct
  denominators".

- [ ] **Step 2: `docs/opaque-and-retry.md`.** After the "fifteen distinct denominators" block (`:207-209`), a new
  subsection -- the code block is the example's source, the text blocks its output:

````markdown
### Rounded where it is used

A method that reports the slope at a stated precision -- "to 0.0001 mm/s" -- does not need the exact fraction:
it needs the decimal that fraction rounds to. `rounded_output` states that precision, as `rounded<>` does, and
the library computes the decimal exactly, even where the fraction itself is too wide for `Rational`:

```cpp
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr auto roundedSlope =
    formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit);
```

```text
round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm
4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] [Rate of change, Example Standard 12, 5.1]
5. round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]
```

- **The true slope is written nowhere.** The call's line names its outputs without values -- none exists until
  one is rounded -- and the output's own line states its rounding. 3393/5000 mm/s is 0.6786 mm/s exactly, the
  correct rounding of 19/28 mm/s, and the step's value; no number style marks it approximate.
- **It answers where the exact fit overflows.** `linear_least_squares` computes the fit for it in 256-bit
  integers, and the fifteen distinct denominators that overflow above give a slope:

```text
fifteen distinct denominators, rounded where used: 116.232 mm/min
```

- **It still refuses rather than guess.** A different denominator on every point outgrows 256 bits from 58
  points, and the answer is `Overflow` ([numeric headroom](numeric-headroom.md#least-squares-realistic-and-one-stress-control)).
  A unit that does not measure the output, or one with an offset, is refused at compile time, and so is
  `Rep = double`.
- `rounded<...>(opaque_output<"slope">(fit))` keeps its meaning: the exact slope, rounded afterwards, which
  overflows where the exact slope does. Each output used runs the whole call, rounded or not.
````

  In the paragraph "**When a fit overflows depends on the data ...**" (`:211-223`), replace "**A fit that overflows
  has no traced fallback in `double`.**" and the rest of that paragraph by: "**A fit that overflows has a traced
  answer only at a declared precision** (`rounded_output`, above), **and none in `double`.** A curve evaluates
  only in `Rational`, so `checked_evaluate_si<double>` over a fit is refused where it is written.
  `LinearLeastSquares::compute<double>` can be called directly, on numbers the caller has put in coherent units,
  but it returns bare numbers: nothing checks their dimensions, and nothing reaches the trace or the page."

- [ ] **Step 3: `docs/display.md`.** The rule (`:7-9`) becomes: "... and the one rule that governs all of them:
  **a decimal is written only where it is the exact value, and a rounded one only where you ask for it -- and a
  value the exact layer cannot hold is written only as the rounding its formula declares.**" After the "What no
  style rounds" section (before `## Decimals in a rendered formula and its documentation`, `:240`):

```markdown
### Values the exact layer cannot hold

A square root is irrational almost everywhere, and a line fitted through fifty readings at four decimals is a
fraction whose numerator and denominator need hundreds of bits. The library does not approximate them. A
formula that needs one **declares the precision it is reported at** -- a unit, decimal places and a rounding
mode, as `rounded<>` does -- and the library computes the decimal the true value rounds to: `rounded_sqrt` for a
root, `rounded_output` for an output of an [opaque operation](opaque-and-retry.md#rounded-where-it-is-used).

- **The rounded decimal is the step's value, and it is exact.** It is the correct rounding of the true value,
  found with integer arithmetic alone -- no floating point -- so the same inputs give the same digits wherever
  the library is built.
- **Its line says it was rounded, and carries no `≈`.** `≈` marks a style's rounding of a value a step holds;
  this rounding is the formula's, stated in the expression, its mode in the brackets:
  `round(slope of #4, to 4 dp of mm/s) = 0.6786 mm/s [nearest, ties to even]`.
- **The true value is written nowhere.** An opaque call evaluated for a rounded output names its outputs
  without values -- `linear least squares(#3) = intercept, slope: rounded where used` -- and each output's own
  line states its rounding.
- **Without a declared precision, these values are refused, never approximated.** `sqrt(2)` is `Inexact`, and
  `opaque_output<"slope">(fit)` is the exact slope or `Overflow`.
- **Only a rational value can tie**, and its tie is broken by the mode as `checked_round` breaks it.
- **A rounding that cannot be decided fails** with `Overflow` -- never a guess. So does a fraction that
  outgrows even the wide integers it is computed in.
- **Floating point never reaches a trace.** `checked_evaluate_si<double>` answers approximately, on purpose,
  and its trace has no printable form.
```

  The section has no fenced block, so `docs.display-output` and `docs.display-snippets` are unaffected.

- [ ] **Step 4: `docs/expressions.md`.** After the paragraph ending "strictly between 1.41421356 and 1.41421357."
  (`:352`):

```markdown
Where the method states the precision the root is reported at, the exact layer gives that instead:
`rounded_sqrt<unit, places, mode>(x)` is the decimal the true root rounds to, exact, and its trace line carries
no `≈`. An output of an opaque operation that is too wide for `Rational` -- a least-squares slope through many
readings -- has the same form, `rounded_output` ([Displaying numbers](display.md#values-the-exact-layer-cannot-hold)).
The `double` route stays what it is: approximate, untraced, and for exploring.
```

- [ ] **Step 5: The header comments.** `trace_render.hpp`, `TraceRenderOptions::numbers` (after "... a value in a
  unit nobody declared is never padded;" sentence group, `:108-116`), add: "A value a formula rounded itself --
  `rounded`, `rounded_sqrt`, `rounded_output` -- is the step's exact value, a decimal, so it reads without `≈`
  in every style, its mode in brackets after it." `render_trace`'s comment (`:3074-3079`), after "Evaluate in
  `double` by all means; print the exact trace.": "A value the exact layer cannot hold is no reason to trace in
  `double` either: a formula declares the precision it is reported at (`rounded_sqrt`, `rounded_output`), and the
  trace shows that exact decimal." `trace.hpp`, `explain` (`:4251-4257`), after "... to trace a `double`
  computation.": "For a value `Rational` cannot hold, declare the precision it is reported at instead
  (`rounded_sqrt`, `rounded_output`): that trace is exact and renders." `evaluate.hpp` (`:16-19`), the first
  bullet becomes: "`checked_evaluate_si<Rep>` is the representation-agnostic core. It answers in the coherent unit
  and in whatever `Rep` the caller asked for -- exact `Rational` by default, `double` when a formula needs values
  exact rationals cannot hold and no audit trail is wanted. A formula that states the precision such a value is
  reported at (`rounded_sqrt`, `rounded_output`) is evaluated exactly by both entry points instead."

- [ ] **Step 6: Build, check, regenerate.** `cl.ps1 -Filter "example|docs|census|hygiene"`, then
  `cl.ps1 -Target formula-cpp-census-page -NoTest` (the example's census twin changed its row), then
  `cl.ps1 -Filter "docs.numeric-headroom|census"`; `gcc14.sh "example|docs|census|hygiene"`; the full suite on
  both. Doxygen on the four headers. `mkdocs build --strict` is the controller's at the end; check the three new
  anchors by eye: `display.md#values-the-exact-layer-cannot-hold`, `opaque-and-retry.md#rounded-where-it-is-used`,
  `numeric-headroom.md#least-squares-realistic-and-one-stress-control`.

- [ ] **Step 7: CHANGELOG.** `### Added`: "- The guides explain values the exact layer cannot hold:
  `docs/display.md` gains *Values the exact layer cannot hold* -- such a value is written only as the rounding its
  formula declares, exact and without `≈`, and refused without one -- `docs/opaque-and-retry.md` a section on
  `rounded_output`, with the fit's trace and the fifteen-point fit it answers, and `docs/numeric-headroom.md` the
  rounded fit's census rows."

- [ ] **Step 8: Commit.**
  `git add docs/display.md docs/opaque-and-retry.md docs/expressions.md docs/numeric-headroom.md examples/opaque_and_retry.cpp examples/CMakeLists.txt include/formula-cpp/trace_render.hpp include/formula-cpp/trace.hpp include/formula-cpp/evaluate.hpp CHANGELOG.md`
  `git commit -m "docs: values the exact layer cannot hold, and rounded_output in the guides" -m "The display rule gains its last clause: a value the exact layer cannot hold is written only as the rounding its formula declares. The display guide says what that means for a trace line -- exact, no approximation marker, the true value written nowhere -- the opaque-operations guide shows the rounded slope's trace and the fit it answers where the exact one overflows, and the expressions guide and the entry points' comments point there instead of at double." -m "Signed-off-by: Christian Parpart <c.parpart@lastrada.net>"`

---

## Self-Review

**Spec coverage** (spec section -> task):
- §1 Problem, §2 The rule -- the node (Task 3), the trace (Task 5), the page's rule (Task 7); integer arithmetic
  only (Global Constraints, Tasks 1-2); the unrounded forms unchanged (Task 3 Step 7 keeps `opaque_output`'s route
  and its tests untouched); an undecidable rounding fails (Task 2 `decide_rounding`); `double` refused (Task 3
  negative `rounded_output_rep_double`).
- §3 Wide unsigned integers -- Task 1 (every operation of the table, plus `WideSigned`, `lcm_checked_or_none`,
  and the controller's `divmod_small` and zero-limb skip in `mul_checked_or_none`; every `std::optional` one
  named `*_checked_or_none`, and its coexistence with `checked_int.hpp`'s `Int` family pinned in the carries
  case).
- §4 Exact rounding, `rounded_in_unit`, agreement with `checked_round`, enclosures -- Task 2 (43,729 and 64,582
  counted cases; the four enclosure tables).
- §5 `rounded_output`, its refusals and its evaluation (inputs as today, `compute_exact` detected, fallback,
  `Own` errors) -- Task 3.
- §6 Trace: `StepKind::RoundedOpaqueOutput`, `OpaqueValues` on `OpaqueStepData` and `OpaqueCallInfo`, the call
  and output lines, no `≈`, fractions `3393/5000 mm/s`, the failure line -- Tasks 3 (`OpaqueCallInfo`) and 5
  (the rest; the spec's exact failure line is pinned in Task 6's 58-point case, its example's `Own` form in
  Task 5's one-point case).
- §7 Page, rendering, overlays, walks -- Task 4.
- §8 Display rules and the comments listed -- Task 7 (`least_squares.hpp`'s in Task 6).
- §9 Acceptance on the existing fit -- Task 6 (equivalence in every mode, 128 points at 3 dp, the stress row's
  `Overflow`, census rows).
- §10 Tests -- distributed as listed; §11 out of scope honoured; §12 risks: the constant-evaluation budget
  measured (Global Constraints), `StepKind` appended with a `### Changed` entry (Task 5), double rounding visible
  in the formula text and trace (Task 5's lines).

**Placeholder scan:** no TBD or "similar to"; every code step shows its code; the census's `least headroom` figures
are generated by the census program into the page (the page is never typed), which is why no number is given
for them.

**Type consistency:** `RoundedOpaqueOutputNode<I, Call, U, Places, Mode, Origin>` in Tasks 3-5;
`detail::unrounded(node)` in Tasks 3-4; `OpaqueValues::{Exact, RoundedWhereUsed}` in Tasks 3 and 5;
`OpaqueStepData::values`/`answered` in Task 5; `detail::rounded_in_unit(WideRatio<L> const&, Unit const&,
DecimalPlaces, RoundingMode)` in Tasks 2, 3, 6; `LinearLeastSquares::exact_limbs == 8` in Task 6 and the probe;
`MillimetrePerSecond` (tests) and `millimetrePerSecond` (example) are separate declarations in separate files.

**Review Focus placement:** 1 -> Task 2 (ties, enclosures) and Task 6 (15-point intercept); 2 -> Task 2 (unit
sweep) and Task 6 (mm/min, -1 dp); 3 -> Task 2 (`IntMin`, 2^63, 2^70), Task 3 and Task 5 (`WideProduct`);
4 -> Task 4 (page) and Task 5 (two runs in one trace); 5 -> Task 3 (`LooseReciprocalSum`, `NarrowReciprocalSum`).

