# Logarithms and Exponentials Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a formula take the natural logarithm, the decimal logarithm and the exponential of a
dimensionless expression — `ln(x)`, `log10(x)`, `exp(x)` — shown on the page and in the trace as the
functions they are, exact where the value is rational and `Inexact` elsewhere, and add the rounded forms
`rounded_ln<P, M>(x)`, `rounded_log10<P, M>(x)`, `rounded_exp<P, M>(x)`, whose value is the correctly
rounded decimal of the irrational value, proved with integer arithmetic.

**Architecture:**
- Plain nodes in `function.hpp`: one `TranscendentalNode<Transcendental F, Operand>` over an enum, as
  `RootNode` covers `sqrt`, `cbrt` and `root<N>`; `RepFunctions<Rep>` gains `natural_log`, `decimal_log`
  and `exponential`; the operand must be dimensionless (`detail::RequireDimensionlessArgument`).
- An integer kernel, `detail/transcendental.hpp`, on the foundation's `detail::WideUnsigned<12>`: 128
  fraction bits, every operation truncating non-negative values, so each result is an enclosure
  `[lower, upper]`; `detail::decide_rounding` (foundation) rounds it or answers `Overflow`.
- Rounded nodes in a new `rounded_transcendental.hpp`: special points (the only values that can tie) go
  to `checked_round`, everything else to the kernel; `Rep = double` is refused by `RepRounding<double>`.
- Every registry a node kind needs: `LevelChildren`, `ConstantRewrite`, `SubstitutedIn`, `document`'s
  `collect`, `render_node`, `StepKindOf`, `step_expression`, the trace's mode suffix.

**Tech Stack:** C++23, header-only; Catch2 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness;
Python 3.13's `decimal` module (libmpdec) for the reference digits, computed once while planning.

**Spec:** `docs/superpowers/specs/2026-09-29-logarithms-and-exponentials-design.md` (issue #3).
**Foundation spec:** `docs/superpowers/specs/2026-09-29-declared-precision-design.md`. **This plan runs
after the foundation plan** and consumes, unchanged, `detail/wide_int.hpp` (`WideUnsigned<L>`,
`add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none`, `mul_small_checked_or_none`, `add_small_checked_or_none`,
`shift_left_checked_or_none`, `shift_right`, `divmod`, `pow10<L>`, `WideRatio<L>`) and
`detail/wide_rounding.hpp` (`round_wide_ratio`, `decide_rounding`), and the foundation plan's
`divmod_small` (a short division by a `std::uint32_t`) and zero-limb-skipping `mul_checked_or_none`, which it adds
at this plan's request (Finding 4).

## Global Constraints

These bind every task. Several exist because the alternative failed in an earlier phase.

- **C++23, header-only**, no dependency beyond the standard library in shipped headers.
- **Per-task verification: MSVC `cl-debug` and g++-14 only** (the owner's rule). All eight presets,
  Doxygen 1.9.8 and `mkdocs build --strict` run once at the end, by the controller.
  - **CL** — Windows, from any directory:
    `pwsh -NoProfile -File C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\6031eb20-da56-4aa2-bb6a-de8c11d53bfc\scratchpad\cl.ps1 [-Filter <ctest -R regex>] [-Target <target>] [-NoTest]`
    (VS dev shell, `cl-debug` in `out/build/cl-debug`, build, `ctest -j 12`; prints `ALL OK`,
    `BUILD FAILED` or `TESTS FAILED`, non-zero on failure).
  - **GCC14** — g++-14 under WSL (`--preset gcc-release -B out/build/gcc14-release
    -DCMAKE_CXX_COMPILER=g++-14`, the preset with `-Wshadow -Wconversion -Wpedantic -Werror`):
    `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/6031eb20-da56-4aa2-bb6a-de8c11d53bfc/scratchpad/gcc14.sh ["<ctest -R regex>"]`.
    Never `wsl bash -lc`. Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is a build error.
  - Every task ends with **both** full runs (no filter) green. **Counts in this plan are deltas**: each
    task's *Counts* line says how many ctest cases it adds (one per `TEST_CASE`, one per negative) and by
    how much it raises the consumer-globals probe (`REQUIRE(probe.checks.size() == N)` in
    `test/consumer_globals_run_tests.cpp`). Record the full-run count before the task and after it; the
    difference is the task's delta, exactly. The whole plan: **+47 ctest cases, probe +3 checks**.
- **Line anchors** below are for commit `c30f261` (before the foundation's commits, which shift some).
  Every anchor is given with the text it points at; find that text, not the number.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:** SPDX header on every file; no
  `NOLINT`; core public headers include no `<string>`, `<vector>`, `<format>` or `<iostream>`; every public
  `static_assert` message begins `formula: ` and is stable; never identify a type with `decltype([]{})`;
  every new header goes into the install `FILE_SET` in the root `CMakeLists.txt`
  (`hygiene.installed-headers`, detail headers included) and every new public header into
  `test/consumer_globals_tests.cpp`'s includes (`hygiene.consumer-globals`).
- **Defect classes.** Read `D:\formula-cpp\.superpowers\sdd\2026-09-25-methods-and-overlays\defect-classes.md`
  before starting. Your report says, for each of its nine classes, what you checked and how.
- **No third-party standard content.** Cite only invented `Example Standard N:YYYY` references. Never
  name a real standards body with a number anywhere (tests, comments, commit messages, docs) —
  `hygiene.no-real-standards` scans every tracked file for `ISO`, `IEC`, `IEEE`, `EN`, `DIN`, `BS`, `NF`,
  `SN`, `UNI`, `TL`, `TP`, `ASTM` and others followed by a digit. Fixture numbers are plainly invented.
- **Negative tests** (`test/negative/<name>.cpp` + `formula_add_negative_test(<name> "<expected>" ...)` in
  `test/CMakeLists.txt`; ctest name `negative.<name>`) assert that the build fails **with this library's
  own text**. Register each with a deliberately wrong expected string first (prefix `WRONG `), run, watch
  `TESTS FAILED`; then the right one, watch `ALL OK`. **Deletion check** for each: delete the guard it
  pins, confirm the case then compiles (or, for a `REJECT`, that the rejected text now appears), run,
  watch it fail; restore **with a plain write** (a timestamp-preserving copy leaves ninja trusting stale
  outputs — `ninja: no work to do` after an edit means something is wrong). `EXPECT_COUNT 1` where a
  second message could plausibly fire (cl ignores the count; g++ enforces it).
- **No `{}` default member initialiser on any member that holds an expression or a node** (defect class
  4). The operand member of both new nodes is `Operand operand;`.
- **Names.** Under g++ `-Wshadow` and cl C4459 no parameter or local may hide one of the globals in
  `test/consumer_globals_tests.cpp:125-144` — among them, for this work: `e`, `x`, `y`, `z`, `a`, `b`, `k`,
  `n`, `m`, `r`, `s`, `t`, `v`, `sum`, `total`, `low`, `high`, `lower`, `upper`, `bound`, `bounds`, `digit`,
  `digits`, `scaled`, `quotient`, `numerator`, `denominator`, `factor`, `value`, `result`, `mode`,
  `precision`, `sign`, `step`, `count`, `ratio`, `rate`, `root`, `range`, `start`, `input`, `output`,
  `arg`, `args`, `epsilon`, `extent`, `offset`, `scale`, `width`, `data`. Read the whole list. Every new
  public name (`Transcendental`, `TranscendentalNode`, `RoundedTranscendentalNode`, `ln`, `log10`, `exp`,
  `rounded_ln`, `rounded_log10`, `rounded_exp`, `natural_log`, `decimal_log`, `exponential`) is absent
  from it. Avoid locals named after functions in namespace `formula` too (`power`, `exponent`, `round`,
  `rounded`): g++ does not warn about those, but a reader stumbles.
- **The trace recorder reads node members by name** (`trace.hpp:3084-3100`: `keyUnit`, `exponent`,
  `degree`, `places`, `digits`, `mode`; `unit` at `:3026-3027`). The plain node declares **none** of them;
  the rounded node declares `places` and `mode` and **no** `unit`.
- **Inside namespace `formula`, call `std::log`, `std::log10` and `std::exp` qualified.** An unqualified
  `exp(double)` there finds `formula::exp`, whose `Node` constraint refuses a `double` — measured while
  planning: `C2672 'formula::exp': no matching overloaded function found` on cl 19.51.36257 and
  `no matching function for call to 'exp(double&)'` on g++ 14.2.
- **Documentation is read by humans.** Every ```` ```cpp ```` block in a guide is consecutive lines of the
  source it names; a quoted compiler diagnostic comes from a real compile (`hygiene.documented-diagnostics`,
  `hygiene.documented-diagnostic-text`). No internal labels in any public text or commit message: no plan,
  task, phase, lane, reviewer or tracker names. Doxygen comment on every public entity. **Do not touch
  `expression.hpp`**: the guides quote its line numbers.
- **Do not run clang-format** on existing files. Match the surrounding style by hand.
- **Catch2 splits test filters on commas.** No new `TEST_CASE` name contains a comma. Prove a `-Filter`
  selected something (the ctest summary names the count) before trusting its result.
- **Commits.** Conventional subject, a body that explains why, and the last line exactly
  `Signed-off-by: Christian Parpart <c.parpart@lastrada.net>`. One commit per task; never `--no-verify`,
  never amend or rewrite another task's commit; every commit builds and passes on its own.
- **You are the only writer in your worktree.** Do not dispatch subagents. Do not touch any other
  worktree or `D:\formula-cpp` itself. Scratch probes live in a scratch directory outside the worktree and
  are never committed. End your turn after the task's report; do not start the next task.
- **CHANGELOG.md** gets its entry under `## [Unreleased]` (`### Added` at `CHANGELOG.md:9`, `### Changed` at
  `:178`) in the task that changes public behaviour.

---

## Findings the design depends on (all verified while planning)

1. **Where a node kind is registered** — the list `AbsoluteValueNode` and `RoundedRootNode` occupy:
   `LevelChildren` (`precision.hpp:487-490` for `RootNode`, `:506-510` `RoundedRootNode`), `ConstantRewrite`
   (`overlay.hpp:1616-1620`, `:1645-1666`), `SubstitutedIn` (`overlay.hpp:2345-2348`, `:2360-2363`; the
   primary at `:2291-2296` silently answers "none"), `document.hpp` `collect` declaration (`:415-416`,
   `:430-431`) and definition (`:767-771`, `:842-848`), `render_node` (`render.hpp:1189-1209`,
   `:1293-1315`), `StepKindOf` (`trace.hpp:1708-1712`, `:1732-1736`), `step_expression`
   (`trace_render.hpp:941-944`, `:1063-1067`; a `switch` with **no `default`**, ends `:1138`), the mode
   suffix (`trace_render.hpp:2935-2937`). The calculation, rejection and retry walks read `LevelChildren`
   (`calculation.hpp:306-322`). Headers including `rounded_root.hpp`: `document.hpp:27`, `formula.hpp:48`,
   `overlay.hpp:154`, `precision.hpp:78`, `render.hpp:56`, `trace.hpp:33`.
2. **Precedence.** `render.hpp`'s `PrecedenceOf` primary answers `Atom`; a call form needs no entry, so
   `pow<2>(ln(x))` renders `ln(x)^2` (`PowerNode` renders its base with `Precedence::Atom`, `:1183`).
3. **Name clashes with `<cmath>` (measured, scratch probe with the factories' exact shape over the real
   `function.hpp`).** Under `using namespace std;`, under `using namespace std; using namespace formula;`
   at namespace and global scope, with `exp(v)`, `log10(v)`, `exp(2)`, `log10(100)` on numbers,
   `exp(log10(ln(var<Ratio>)))` on nodes, and locals named `ln`, `exp`, `log10`: no ambiguity, no warning on
   cl 19.51.36257 `/W4 /WX /permissive-` or g++ 14.2 `-Wall -Wextra -Wconversion -Wpedantic -Wshadow
   -Werror`. Six scoped enumerators `NaturalLogarithm` … `RoundedExponential` beside `Transcendental`'s own
   `NaturalLogarithm`, `DecimalLogarithm`, `Exponential` and the six factories: clean on both.
4. **cl's constexpr step budget (measured).** The effective default on cl 19.51.36257 is about
   **1 049 000** steps, not the documented 100 000: a 349 436-iteration loop compiles at the default and
   needs `/constexpr:steps` ≈ 1 049 814. A stand-in `WideUnsigned<12>` (schoolbook multiply, binary
   shift-subtract `divmod`, as the foundation spec describes) with this plan's kernel, bisecting
   `/constexpr:steps` per kernel call:
   | Variant | ln 3 | ln, z → 1/3 (worst) | log10 2 | log10 7 | exp 43 | exp 1 |
   |---|---|---|---|---|---|---|
   | binary `divmod` for the scaled quotients and k, short division through the public interface | 282k | 338k | 138k | 312k | 344k | 393k |
   | 64-bit long division + binary search, limb-level short division | 48k | 68k | 4.9k | 59k | 28k | 44k |
   | same, short division through the public interface only | — | 159k | — | 137k | — | 104k |
   | limb-level short division, multiply skipping zero limbs | 22k | 31k | 3.0k | 27k | 14k | 21k |
   | same, 8-limb words (no log10 at this width) | 15k | 21k | — | — | 9.8k | 15k |
   clang-cl 22.1.3 (row 2): 62k–95k per call, compiling at its default. So the kernel **never calls
   `divmod`** and uses the limb-level `divmod_small` the foundation plan adds for it. The spec's 25 000 target was a quarter of a
   100 000 default; against the measured default this plan sets **at most 100 000 cl steps per kernel call
   at `KernelLimbs = 12`** (Task 4 Step 2), about a tenth of the effective default, and Task 4 Step 7
   measures and reports each call. A whole rounding — the enclosure and the foundation's `decide_rounding`,
   on stand-ins built from both plans' code — measured 240 600–284 500 cl steps (297 950–355 300 on clang-cl
   22.1.3, default 1 048 576), so the kernel's checks run at run time but one (Task 4 Step 7).
5. **Reference digits.** Python 3.13 `decimal` (libmpdec) at 150 significant digits; its `ln`, `log10` and
   `exp` are correctly rounded. A Python model of this plan's kernel, operation for operation (written
   while planning, not committed), enclosed the reference on 12 020 inputs (4 000
   random ln/log10 pairs over 1…2^63, 4 000 exp arguments over −43…44, edges): at most 40 atanh terms,
   31 Taylor terms, ln enclosures ≤ 190 units of 2^-128 wide, exp deficit ≤ 59 units against a slack of
   512. It decided all 2 457 (row, places, mode) cases of Task 4's table exactly as the reference does.
   A C++ transcription (the stand-in above) produced the same bits as the model for ln 3, ln at z → 1/3,
   log10 7, exp 43 and exp 1.
6. **Special points.** `log10(10^15)` at −1 dp is 20, 10, 20 under HalfAwayFromZero, HalfTowardZero,
   HalfEven; `log10(10^5)` at −1 dp is 10, 0, 0; `log10(10^-15)` at −1 dp is −20, −10, −20.
7. **`measured_tests.cpp` holds no per-node-kind absence table** (its cases are about `Measured` itself),
   so absence is pinned beside each node's other value tests (`function_tests.cpp`,
   `rounded_transcendental_tests.cpp`).

## Review Focus

Five input classes a task could miss, each with its test in the owning task:

1. **Percent operands** are read in the coherent unit: `ln` of 5 % is ln 0.05 = −2.9957…, never ln 5 =
   1.6094…; `log10` of 1000 % is exactly 1, never 3. — Task 1 "function: a percentage is read in the
   coherent unit under a logarithm"; Task 5 "rounded_transcendental: a percentage is read in the coherent
   unit" (−29957/10000).
2. **ln near 1**: a value within 10^-19 of 0, positive and negative, whose sign must come from a < b and
   never from a rounding; and a value 1.25·10^-37 below a tie at 18 places, which the kernel cannot decide
   and must report as `Overflow`. — Task 4 table rows (2^63−1)/(2^63−2), 1000001/1000000, 999999/1000000;
   Task 4 "transcendental kernel: an enclosure that straddles a tie is Overflow and never a guess"; Task 5
   the same through the node.
3. **exp of a large negative argument under Ceiling and AwayFromZero** is one unit of the last kept place
   (10^-p, or 10^|p| at negative places), never 0; under the other five modes 0. — Task 5
   "rounded_transcendental: a tiny exponential is zero or one unit by mode" (exp −50 at 3 dp and at −2 dp,
   and exp −43 through the kernel at 18 dp).
4. **log10 of an exact power of ten written as a fraction**: 1/100 → −2, 1/10^18 → −18, 10^18 → 18
   exactly; 20, 1001/1000 and 1000/3 are not powers of ten; the ties at negative places. — Task 1
   "function: ln log10 and exp are exact where their value is rational" and "…any other value is Inexact
   in Rational"; Task 5 "rounded_transcendental: a special point ties and the mode decides it".
5. **Absence versus error ordering**: an absent operand stays absent (never `DomainError`); an operand's
   own failure (`DivisionByZero`) reaches the node unchanged; `DomainError` is reported before a places
   `Overflow`; a places `Overflow` before the tiny-exponential rule. — Task 1 "function: an absent argument
   leaves a logarithm or an exponential absent" and "…the argument's own failure reaches a logarithm
   unchanged"; Task 5 "rounded_transcendental: absence and failures come first and in order".

---

## Task 1: Plain nodes — `ln`, `log10`, `exp`, exact where rational

**Counts:** +15 ctest cases (+9 `function_tests`, +6 negatives); consumer-globals probe +2 checks.

**Files:**
- Modify `include/formula-cpp/function.hpp` (includes at `:13-19`; after `PiNode`, `:81-86`; factories
  after `inline constexpr PiNode pi {};`, `:116-117`; `RepFunctions<Rational>` `:135-164`;
  `RepFunctions<double>` `:167-193`; evaluators before the closing `} // namespace formula`, `:259`).
- Modify `test/function_tests.cpp` (fixtures in the anonymous namespace `:7-29`; new cases at the end).
- Create six `test/negative/transcendental_*.cpp`; modify `test/CMakeLists.txt` (after
  `function_zero_root_degree`, `:710-711`).
- Modify `test/consumer_globals_tests.cpp` (after the rounded-root probe, `:510-515`; header comment
  `:18-21`) and `test/consumer_globals_run_tests.cpp:16` (the check count).
- Modify `CHANGELOG.md`.

**Interfaces:**
- Consumes: `detail::refused_already`, `detail::RefusedFlag` (`expression.hpp:125-169`),
  `is_dimensionless` (`dimension.hpp:610`), `detail::dispatch/report_failure/nothing/present` (as the
  `RootNode` evaluator, `function.hpp:222-245`).
- Produces (namespace `formula`):
  ```cpp
  enum class Transcendental : std::uint8_t { NaturalLogarithm, DecimalLogarithm, Exponential };
  template <Transcendental F, Node Operand> struct TranscendentalNode;   // operand, function, dimension, refused
  template <Node Operand> constexpr auto ln(Operand operand) noexcept;
  template <Node Operand> constexpr auto log10(Operand operand) noexcept;
  template <Node Operand> constexpr auto exp(Operand operand) noexcept;
  // RepFunctions<Rational> and RepFunctions<double> gain:
  static std::expected<Rep, ArithmeticError> natural_log(Rep) noexcept;    // constexpr for Rational
  static std::expected<Rep, ArithmeticError> decimal_log(Rep) noexcept;
  static std::expected<Rep, ArithmeticError> exponential(Rep) noexcept;
  template <typename Rep = Rational, Transcendental F, Node Operand, typename Env, typename Sink = NullSink>
  constexpr Evaluated<Rep> checked_evaluate_si(TranscendentalNode<F, Operand> const&, Env const&, Sink = {}) noexcept;
  ```
  and in `formula::detail`: `RequireDimensionlessArgument<Operand>`,
  `constexpr std::string_view transcendental_name(Transcendental) noexcept` (`"ln"`, `"log10"`, `"exp"`,
  the one spelling `render.hpp` and `trace_render.hpp` read in Tasks 3 and 6),
  `constexpr std::optional<int> power_of_ten_exponent(Rational positive) noexcept`,
  `template <Transcendental F, typename Rep> constexpr std::expected<Rep, ArithmeticError> transcendental_of(Rep) noexcept`.

- [ ] **Step 1: Write the failing tests.** In `test/function_tests.cpp` add to the includes `<cstdint>`,
  `<limits>` and `<optional>`, and to the anonymous namespace (after `Edge`, `:18-20`):

```cpp
struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", formula::unit::One>
{
};
struct Share: formula::Quantity<Share, "p", "an invented share", formula::unit::Percent>
{
};

/// @p node evaluated exactly, with the ratio at @p ratioValue.
template <typename N>
[[nodiscard]] constexpr formula::Evaluated<formula::Rational> exactlyAt(N const& node, formula::Rational ratioValue)
{
    return formula::checked_evaluate_si<formula::Rational>(
        node, formula::environment(formula::Measured<Ratio> { ratioValue }));
}

/// @p node evaluated in double, with the ratio at @p ratioValue.
template <typename N>
[[nodiscard]] formula::Evaluated<double> approximatelyAt(N const& node, formula::Rational ratioValue)
{
    return formula::checked_evaluate_si<double>(node, formula::environment(formula::Measured<Ratio> { ratioValue }));
}

/// The error @p evaluated failed with, or nothing when it did not fail.
template <typename Rep>
[[nodiscard]] constexpr std::optional<formula::ArithmeticError> failureOf(formula::Evaluated<Rep> const& evaluated)
{
    return evaluated.has_value() ? std::nullopt : std::optional<formula::ArithmeticError> { evaluated.error() };
}

/// Whether `formula::ln` accepts an argument of type @p T.
template <typename T>
concept LogarithmAccepts = requires(T argument) { formula::ln(argument); };
```

  and at the end of the file:

```cpp
TEST_CASE("function: a logarithm or an exponential is a dimensionless node over a dimensionless argument",
          "[function]")
{
    constexpr auto logarithm = formula::ln(var<Ratio>);
    STATIC_REQUIRE(formula::Node<decltype(logarithm)>);
    STATIC_REQUIRE(decltype(logarithm)::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(logarithm)::function == formula::Transcendental::NaturalLogarithm);
    STATIC_REQUIRE(decltype(formula::log10(var<Ratio>))::function == formula::Transcendental::DecimalLogarithm);
    STATIC_REQUIRE(decltype(formula::exp(var<Ratio>))::function == formula::Transcendental::Exponential);
    // A ratio of two areas is a bare number, and so is a percentage.
    STATIC_REQUIRE(decltype(formula::ln(var<Area> / var<Area>))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::exp(var<Share>))::dimension == formula::dim::Scalar);
    // A formula node only: a bare number is not one, so ln(2) never means a number function.
    STATIC_REQUIRE(LogarithmAccepts<decltype(var<Ratio>)>);
    STATIC_REQUIRE_FALSE(LogarithmAccepts<formula::Rational>);
    STATIC_REQUIRE_FALSE(LogarithmAccepts<double>);
}

TEST_CASE("function: ln log10 and exp are exact where their value is rational", "[function]")
{
    STATIC_REQUIRE(**exactlyAt(formula::ln(var<Ratio>), rat(1)) == rat(0));
    STATIC_REQUIRE(**exactlyAt(formula::exp(var<Ratio>), rat(0)) == rat(1));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1)) == rat(0));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1000)) == rat(3));
    // A power of ten written as one over a power of ten.
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1, 100)) == rat(-2));
    // The largest powers of ten a Rational holds, either way up.
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1'000'000'000'000'000'000)) == rat(18));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1, 1'000'000'000'000'000'000)) == rat(-18));
}

TEST_CASE("function: a logarithm or an exponential of any other value is Inexact in Rational", "[function]")
{
    constexpr auto Inexact = formula::ArithmeticError::Inexact;
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(2))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(2))) == Inexact);
    // Not powers of ten: 20 ends in a zero, 1001/1000 has a power of ten below the line, 1000/3 above it.
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(20))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(1001, 1000))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(1000, 3))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(1))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(-1))) == Inexact);
    // However large: exp 1000 is irrational before it is too large, and says so.
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(1000))) == Inexact);
}

TEST_CASE("function: the logarithm of zero or a negative value is a domain error in both representations",
          "[function]")
{
    constexpr auto DomainError = formula::ArithmeticError::DomainError;
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(0))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(-1))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(0))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(-1))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::ln(var<Ratio>), rat(0))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::ln(var<Ratio>), rat(-1))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::log10(var<Ratio>), rat(0))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::log10(var<Ratio>), rat(-1))) == DomainError);
    // The double guard is !(v > 0.0), so NaN -- which no Rational converts to -- is refused as well.
    auto const ofNaN = formula::RepFunctions<double>::natural_log(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(ofNaN.has_value());
    CHECK(ofNaN.error() == DomainError);
    auto const decimalOfNaN = formula::RepFunctions<double>::decimal_log(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(decimalOfNaN.has_value());
    CHECK(decimalOfNaN.error() == DomainError);
}

TEST_CASE("function: the double representation answers where the exact one refuses", "[function]")
{
    auto const naturalOfTwo = approximatelyAt(formula::ln(var<Ratio>), rat(2));
    REQUIRE(naturalOfTwo.has_value());
    REQUIRE(naturalOfTwo->has_value());
    CHECK(**naturalOfTwo > 0.69314718);
    CHECK(**naturalOfTwo < 0.69314719);
    auto const decimalOfTwo = approximatelyAt(formula::log10(var<Ratio>), rat(2));
    REQUIRE(decimalOfTwo.has_value());
    REQUIRE(decimalOfTwo->has_value());
    CHECK(**decimalOfTwo > 0.30102999);
    CHECK(**decimalOfTwo < 0.30103000);
    auto const exponentialOfOne = approximatelyAt(formula::exp(var<Ratio>), rat(1));
    REQUIRE(exponentialOfOne.has_value());
    REQUIRE(exponentialOfOne->has_value());
    CHECK(**exponentialOfOne > 2.71828182);
    CHECK(**exponentialOfOne < 2.71828183);
}

TEST_CASE("function: exp too large for double is +inf and passes as a power's does", "[function]")
{
    // RepFunctions<double>::raise lets inf through, and RepTraits<double> says why: the caller asked
    // for double. exp does the same.
    auto const huge = approximatelyAt(formula::exp(var<Ratio>), rat(1000));
    REQUIRE(huge.has_value());
    REQUIRE(huge->has_value());
    CHECK(**huge == std::numeric_limits<double>::infinity());
}

TEST_CASE("function: an absent argument leaves a logarithm or an exponential absent", "[function]")
{
    // Absent, never a domain error: an argument nobody measured is not zero.
    constexpr auto nothingMeasured = formula::environment(formula::Measured<Ratio>::absent());
    STATIC_REQUIRE(!formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Ratio>), nothingMeasured)->has_value());
    STATIC_REQUIRE(!formula::checked_evaluate_si<formula::Rational>(formula::log10(var<Ratio>), nothingMeasured)->has_value());
    STATIC_REQUIRE(!formula::checked_evaluate_si<formula::Rational>(formula::exp(var<Ratio>), nothingMeasured)->has_value());
    auto const approximate = formula::checked_evaluate_si<double>(formula::ln(var<Ratio>), nothingMeasured);
    REQUIRE(approximate.has_value());
    CHECK_FALSE(approximate->has_value());
}

TEST_CASE("function: the argument's own failure reaches a logarithm unchanged", "[function]")
{
    // r / q with q = 0 fails with DivisionByZero. The logarithm reports that, not DomainError, which a
    // node that looked at a default value in place of the failure would report.
    constexpr auto inputs =
        formula::environment(formula::Measured<Ratio> { rat(1) }, formula::Measured<Divisor> { rat(0) });
    constexpr auto DivisionByZero = formula::ArithmeticError::DivisionByZero;
    STATIC_REQUIRE(failureOf(formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Ratio> / var<Divisor>), inputs))
                   == DivisionByZero);
    STATIC_REQUIRE(failureOf(formula::checked_evaluate_si<formula::Rational>(formula::exp(var<Ratio> / var<Divisor>), inputs))
                   == DivisionByZero);
    CHECK(failureOf(formula::checked_evaluate_si<double>(formula::log10(var<Ratio> / var<Divisor>), inputs))
          == DivisionByZero);
}

TEST_CASE("function: a percentage is read in the coherent unit under a logarithm", "[function]")
{
    // 1000 % is the number 10, so its decimal logarithm is 1 -- not 3, which reading it in percent gives.
    constexpr auto tenfold = formula::environment(formula::Measured<Share> { rat(1000) });
    STATIC_REQUIRE(**formula::checked_evaluate_si<formula::Rational>(formula::log10(var<Share>), tenfold) == rat(1));
    // 5 % is 0.05: ln 0.05 = -2.9957..., where ln 5 would be 1.6094....
    constexpr auto fivePercent = formula::environment(formula::Measured<Share> { rat(5) });
    auto const approximate = formula::checked_evaluate_si<double>(formula::ln(var<Share>), fivePercent);
    REQUIRE(approximate.has_value());
    REQUIRE(approximate->has_value());
    CHECK(**approximate > -2.9957323);
    CHECK(**approximate < -2.9957322);
    // Exactly, ln 0.05 is irrational.
    STATIC_REQUIRE(failureOf(formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Share>), fivePercent))
                   == formula::ArithmeticError::Inexact);
}
```

- [ ] **Step 2: Run and watch it fail.** `CL -Filter "function: "` → `BUILD FAILED` (cl: `'ln': is not a
  member of 'formula'` or `'Transcendental': is not a member of 'formula'`).

- [ ] **Step 3: Implement** in `include/formula-cpp/function.hpp`. Extend the file comment (`:4-11`) with a
  paragraph: logarithms and the exponential are nodes for the page's and the trace's sake, act on no
  dimension because they accept none, and are exact only where their value is rational. Add
  `#include <cstdint>`, `<expected>`, `<optional>`, `<string_view>`. After `PiNode` (`:81-86`):

```cpp
/// Which function a `TranscendentalNode` takes of its argument.
enum class Transcendental : std::uint8_t
{
    /// The natural logarithm, `ln`: to base e.
    NaturalLogarithm,
    /// The decimal logarithm, `log10`: to base 10.
    DecimalLogarithm,
    /// The exponential, `exp`: e raised to the argument.
    Exponential,
};

namespace detail
{
    /// Fails to compile when the argument of a logarithm or an exponential is not dimensionless: of a
    /// quantity, ln(2 m) would be ln 2 + ln(m), a number that changes with the unit the quantity is read
    /// in.
    ///
    /// Asked only of an operand not refused already (`refused_already`), so that an operand refused for
    /// another reason draws that refusal alone. Here, beside `RequirePositiveRootDegree`, and not in
    /// `expression.hpp`, whose line numbers the guides quote.
    template <typename Operand>
    struct RequireDimensionlessArgument
    {
        static_assert(refused_already<Operand>() || is_dimensionless(Operand::dimension),
                      "formula: the argument of this logarithm or exponential is not dimensionless; ln, log10 "
                      "and exp take a bare number, and of a quantity they would change with the unit it is read "
                      "in -- divide it by a reference value of its own dimension, or read it with "
                      "numeric_value_of; the argument appears in this diagnostic as the template argument of "
                      "RequireDimensionlessArgument");

        static constexpr bool value = true;
    };

    /// How a formula writes @p function -- `ln`, `log10`, `exp` -- in plain text, in Markdown and in a
    /// trace: the one spelling `render.hpp` and `trace_render.hpp` both read, so that a derivation names
    /// the function its formula names.
    [[nodiscard]] constexpr std::string_view transcendental_name(Transcendental function) noexcept
    {
        switch (function)
        {
            case Transcendental::NaturalLogarithm:
                return "ln";
            case Transcendental::DecimalLogarithm:
                return "log10";
            case Transcendental::Exponential:
                return "exp";
        }
        return "unknown function";
    }

    /// k when @p positive is exactly 10^k, and nothing otherwise. Read off the reduced fraction: a power
    /// of ten is a power of ten over 1, or 1 over a power of ten, so k runs from -18 to 18, the powers of
    /// ten `Rational::Int` holds. @pre @p positive is above zero.
    [[nodiscard]] constexpr std::optional<int> power_of_ten_exponent(Rational positive) noexcept
    {
        bool const whole = positive.denominator() == 1;
        if (!whole && positive.numerator() != 1)
            return std::nullopt;
        Rational::Int remaining = whole ? positive.numerator() : positive.denominator();
        int tens = 0;
        while (remaining % 10 == 0)
        {
            remaining /= 10;
            ++tens;
        }
        if (remaining != 1)
            return std::nullopt;
        return whole ? tens : -tens;
    }
} // namespace detail

/// The natural logarithm, the decimal logarithm or the exponential -- @p F -- of @p Operand, a
/// dimensionless expression. The result is dimensionless too.
template <Transcendental F, Node Operand>
struct TranscendentalNode: NodeBase
{
    static_assert(detail::RequireDimensionlessArgument<Operand>::value);

    /// The argument: a bare number.
    ///
    /// Named `operand`, as `ConstantRewriteOperand` (`overlay.hpp`) reads it, and deliberately no `{}`
    /// default member initialiser -- see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// Which function this is. Named `function`: the trace recorder reads a node's `exponent`, `degree`,
    /// `places`, `digits`, `mode`, `unit` and `keyUnit` by name, and this node declares none of them.
    static constexpr Transcendental function = F;
    /// A bare number, as the argument is.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether its operand was refused -- see `detail::refused_already`. Its own check is not counted:
    /// `ln(L) + m`, over a length and a mass, is two mistakes, and the sum says so too.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};
```

  After `inline constexpr PiNode pi {};` (`:116-117`):

```cpp
/// The natural logarithm of `operand`, a dimensionless expression:
/// `ln(var<Count> / var<InitialCount>)`. Takes a formula node only -- a bare `Rational` is not one.
template <Node Operand>
[[nodiscard]] constexpr auto ln(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::NaturalLogarithm, Operand> { {}, operand };
}

/// The decimal logarithm of `operand`, a dimensionless expression.
template <Node Operand>
[[nodiscard]] constexpr auto log10(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::DecimalLogarithm, Operand> { {}, operand };
}

/// The exponential of `operand`, a dimensionless expression: e raised to it.
template <Node Operand>
[[nodiscard]] constexpr auto exp(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::Exponential, Operand> { {}, operand };
}
```

  In `RepFunctions<Rational>`, after `pi_value` (`:159-163`), and extend the struct's comment to name them:

```cpp
    /// The natural logarithm of `argument`, exactly: 0 at 1. Every other positive rational has an
    /// irrational logarithm, which is `Inexact`; zero and below have none, which is `DomainError`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> natural_log(Rational argument) noexcept
    {
        if (argument.sign() <= 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (argument == Rational { 1 })
            return Rational {};
        return std::unexpected { ArithmeticError::Inexact };
    }

    /// The decimal logarithm of `argument`, exactly: k at 10^k, for k from -18 to 18. Every other positive
    /// rational has an irrational one, which is `Inexact`; zero and below have none, which is
    /// `DomainError`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> decimal_log(Rational argument) noexcept
    {
        if (argument.sign() <= 0)
            return std::unexpected { ArithmeticError::DomainError };
        std::optional<int> const tens = detail::power_of_ten_exponent(argument);
        if (tens.has_value())
            return Rational { *tens };
        return std::unexpected { ArithmeticError::Inexact };
    }

    /// e raised to `argument`, exactly: 1 at 0. At every other rational it is irrational, however large,
    /// which is `Inexact`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> exponential(Rational argument) noexcept
    {
        if (argument.is_zero())
            return Rational { 1 };
        return std::unexpected { ArithmeticError::Inexact };
    }
```

  In `RepFunctions<double>`, after `pi_value` (`:188-192`):

```cpp
    /// The natural logarithm, via `std::log`. `DomainError` for zero, a negative value and NaN, all three
    /// caught by `!(argument > 0.0)` before the standard library is called.
    [[nodiscard]] static std::expected<double, ArithmeticError> natural_log(double argument) noexcept
    {
        if (!(argument > 0.0))
            return std::unexpected { ArithmeticError::DomainError };
        return std::log(argument);
    }

    /// The decimal logarithm, via `std::log10` -- qualified, as every call here is: inside namespace
    /// `formula` an unqualified `log10` or `exp` finds `formula::log10` or `formula::exp`, which take a
    /// formula node, and looks no further. `DomainError` as for `natural_log`.
    [[nodiscard]] static std::expected<double, ArithmeticError> decimal_log(double argument) noexcept
    {
        if (!(argument > 0.0))
            return std::unexpected { ArithmeticError::DomainError };
        return std::log10(argument);
    }

    /// The exponential, via `std::exp`. A result too large for `double` is `+inf`, and passes, as `raise`'s
    /// does: the caller asked for `double`.
    [[nodiscard]] static std::expected<double, ArithmeticError> exponential(double argument) noexcept
    {
        return std::exp(argument);
    }
```

  Before `} // namespace formula` (`:259`):

```cpp
namespace detail
{
    /// @p F of @p argument through `RepFunctions<Rep>`: the one place the three functions are told
    /// apart, for the plain node and, under a representation other than `Rational`, the rounded one.
    template <Transcendental F, typename Rep>
    [[nodiscard]] constexpr std::expected<Rep, ArithmeticError> transcendental_of(Rep argument) noexcept
    {
        if constexpr (F == Transcendental::NaturalLogarithm)
            return RepFunctions<Rep>::natural_log(argument);
        else if constexpr (F == Transcendental::DecimalLogarithm)
            return RepFunctions<Rep>::decimal_log(argument);
        else
            return RepFunctions<Rep>::exponential(argument);
    }
} // namespace detail

/// Evaluates the argument, then takes `F` of it via `RepFunctions<Rep>`. An absent argument leaves the
/// node absent and a failed one fails it, before the function is asked anything.
template <typename Rep = Rational, Transcendental F, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(TranscendentalNode<F, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedOperand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!evaluatedOperand.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedOperand.error());
    }
    if (!evaluatedOperand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const taken = detail::transcendental_of<F, Rep>(**evaluatedOperand);
    Evaluated<Rep> const evaluated =
        taken.has_value() ? detail::present<Rep>(*taken) : Evaluated<Rep> { std::unexpected { taken.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}
```

- [ ] **Step 4: Run and pass.** `CL -Filter "function: "` → `ALL OK` (the summary names at least the 9 new
  cases); then `GCC14 "function: "`.

- [ ] **Step 5: Negative tests.** Six files; each begins `// SPDX-License-Identifier: Apache-2.0`, then
  `// EXPECT: <text>`, a comment saying what mistake it is and why it draws one message, `// This must not
  compile.`, and `#include <formula-cpp/formula.hpp>`. A shared fixture in each:

```cpp
namespace
{
struct Height: formula::Quantity<Height, "h", "an invented height", formula::unit::Metre>
{
};
} // namespace
```

  1. `transcendental_ln_dimensioned.cpp` — `int main() { constexpr auto logarithm = formula::ln(formula::var<Height>); return decltype(logarithm)::dimension == formula::dim::Scalar ? 0 : 1; }`
  2. `transcendental_log10_dimensioned.cpp` — the same with `formula::log10`.
  3. `transcendental_exp_dimensioned.cpp` — the same with `formula::exp`.
  4. `transcendental_no_factory_dimensioned.cpp` — the aggregate built without a factory, so only a check
     in the class body can see it:
     ```cpp
     inline constexpr auto height = formula::var<Height>;
     inline constexpr formula::TranscendentalNode<formula::Transcendental::Exponential, decltype(height)> broken { {}, height };
     int main() { return decltype(broken)::dimension == formula::dim::Scalar ? 0 : 1; }
     ```
  5. `transcendental_refused_operand.cpp` — a logarithm over an interpolation refused already, whose
     stand-in dimension is a length: one message, the interpolation's. Fixture adds
     `struct Opening: formula::Quantity<Opening, "d", "an invented screen opening", formula::unit::Metre> {};`;
     ```cpp
     inline constexpr auto misread = formula::interpolate_at(
         formula::curve(formula::series<Opening, 3>, formula::series<Height, 3>),
         formula::constant<formula::unit::Percent>(formula::Rational { 50 }));
     inline constexpr auto logarithm = formula::ln(misread);
     int main() { return decltype(logarithm)::dimension == formula::dim::Scalar ? 0 : 1; }
     ```
  6. `transcendental_then_addition.cpp` — `formula::ln(var<Height>) + var<Mass>` with
     `struct Mass: formula::Quantity<Mass, "m", "an invented mass", formula::unit::Kilogram> {};`: two
     mistakes, and the addition's is still reported (the node's `refused` carries only its operand's).

  Registration, after `function_zero_root_degree` (`test/CMakeLists.txt:710-711`):

```cmake
# A logarithm or an exponential of a quantity: refused once, through each
# factory and around them, and silent over an operand refused already.
formula_add_negative_test(transcendental_ln_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1)
formula_add_negative_test(transcendental_log10_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1)
formula_add_negative_test(transcendental_exp_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1)
formula_add_negative_test(transcendental_no_factory_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1)
formula_add_negative_test(transcendental_refused_operand
    "formula: this curve is read at a point that does not measure the dimension of its domain"
    REJECT "formula: the argument of this logarithm or exponential is not dimensionless")
# Two mistakes, two messages: the logarithm's own refusal does not hide the
# sum's, since fixing either leaves the other.
formula_add_negative_test(transcendental_then_addition
    "formula: the two sides of this addition or subtraction measure different dimensions")
```

  Protocol per case: wrong text first (`WRONG ` prefix) → `CL -Filter "negative\.transcendental_"` →
  `TESTS FAILED`; right text → `ALL OK`; then `GCC14 "negative\.transcendental_"` (enforces
  `EXPECT_COUNT`). Deletion checks: (1–4) delete `static_assert(detail::RequireDimensionlessArgument<Operand>::value);`
  from `TranscendentalNode` → cases 1–4 compile → fail; restore by plain write. (5) delete
  `refused_already<Operand>() || ` from `RequireDimensionlessArgument` → the rejected text appears → fails;
  restore. (6) change the node's `refused` to `true` → the addition's message disappears → fails;
  restore.

- [ ] **Step 6: Consumer globals.** In `test/consumer_globals_tests.cpp`, after the rounded-root check
  (`:510-515`):

```cpp
    // Logarithms and exponentials where each is exact -- exp(ln 1) is 1 and log10 1000 is 3 -- and the
    // double route where none is: ln 2.
    auto const logarithmic = formula::evaluate<Factor>(
        formula::exp(formula::ln(var<Factor>)) * formula::log10(var<Factor> * formula::Rational { 1000 }), specimen);
    auto const approximateLogarithm =
        formula::checked_evaluate_si<double>(formula::ln(var<Factor> * formula::Rational { 2 }), specimen);
    probe.checks.push_back(logarithmic.is_value() && logarithmic.measurement().value() == formula::Rational { 3 });
    probe.checks.push_back(approximateLogarithm.has_value() && approximateLogarithm->has_value()
                           && **approximateLogarithm > 0.69 && **approximateLogarithm < 0.70);
```

  Raise the count in `test/consumer_globals_run_tests.cpp:16` by 2, and add to the header comment's list
  (`:18-21`) "a logarithm, a decimal logarithm and an exponential, exactly and in double".

- [ ] **Step 7: CHANGELOG** under `## [Unreleased]`:
  - `### Added`: "- `ln(x)`, `log10(x)` and `exp(x)` (`function.hpp`) take the natural logarithm, the
    decimal logarithm and the exponential of a formula. The argument must be dimensionless -- a quantity
    divided by a reference value of its own dimension, or a number read with `numeric_value_of` -- and a
    dimensioned one does not compile. A percentage is dimensionless and read as a fraction, so `log10` of
    1000 % is 1. Evaluated exactly, each answers where its value is rational -- ln 1 = 0, exp 0 = 1,
    log10 10^k = k for k from -18 to 18 -- and is `ArithmeticError::Inexact` elsewhere; the logarithm of
    zero or of a negative value is `DomainError`. `checked_evaluate_si<double>` answers with `std::log`,
    `std::log10` and `std::exp`. `RepFunctions` gains `natural_log`, `decimal_log` and `exponential`,
    which a representation of a consumer's own needs only to evaluate these."
  - `### Changed`: "- An unqualified call of `ln`, `log10` or `exp` whose argument is a formula node now
    finds the library's function by argument-dependent lookup; one whose argument is a number still finds
    only the standard library's, which the library's refuses. A consumer's own function of one of these
    names that accepts a formula node now makes such a call ambiguous."

- [ ] **Step 8: Full verification.** `CL` and `GCC14`, no filter: all green (`hygiene.*` included).

- [ ] **Step 9: Commit.**
  ```
  feat(function): natural and decimal logarithms and the exponential of a dimensionless formula

  ln, log10 and exp are nodes, so a formula that takes them is still a formula: dimension-checked
  where it is written, with a refusal that names the argument and the two ways to make it a bare
  number. Exactly evaluated they answer only where the value is rational, and are Inexact
  elsewhere, as sqrt(2) is; in double they call the standard library.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 2: Walks — overlays, precision limits and calculations see inside the plain node

**Counts:** +3 ctest cases (+1 `calculation_tests`, +2 negatives; the overlay assertions join an existing
case); probe unchanged.

**Files:**
- Modify `include/formula-cpp/precision.hpp` (after `LevelChildren<RootNode<Degree, Operand>>`,
  `:486-490`).
- Modify `include/formula-cpp/overlay.hpp` (after `ConstantRewrite<Sub, RootNode<Degree, Operand>>`,
  `:1616-1620`; after `SubstitutedIn<RootNode<Degree, Operand>>`, `:2345-2348`).
- Modify `test/overlay_tests.cpp` ("an overlay fixes a constant inside every node kind", `:368-450`, after
  the `abs` line `:419`), `test/calculation_tests.cpp` (after "documented() reads what it documents",
  `:200-204`).
- Create `test/negative/overlay_substitution_inside_logarithm_across_overlays.cpp` and
  `test/negative/precision_level_in_level_through_logarithm.cpp`; modify `test/CMakeLists.txt` (beside
  `overlay_substitution_inside_opaque_across_overlays`, `:2277-2279`, and `precision_level_in_own_level`,
  `:799-801`).

**Interfaces:**
- Consumes: Task 1's `TranscendentalNode<F, Operand>` (member `operand`); `LevelParent`,
  `ConstantRewriteOperand`, `ConstantRewriteOf`, `SubstitutedInOperand`.
- Produces: `detail::LevelChildren<TranscendentalNode<F, Operand>>`,
  `detail::ConstantRewrite<Sub, TranscendentalNode<F, Operand>>`,
  `detail::SubstitutedIn<TranscendentalNode<F, Operand>>`.

- [ ] **Step 1: Write the failing tests.** In `test/overlay_tests.cpp`, after the `abs` assertion (`:419`):

```cpp
    // A logarithm and an exponential, at the fixed 4 where each is exact: ln(4/4) is 0, log10(4 * 25)
    // is 2 and exp(4 - 4) is 1. A rewrite that stopped at the function would leave r unread, and the
    // environment holds nothing.
    STATIC_REQUIRE(withRatioFixedAtFour(f::ln(r / f::number(Rational { 4 }))) == Rational { 0 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::log10(r * f::number(Rational { 25 }))) == Rational { 2 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::exp(r - f::number(Rational { 4 }))) == Rational { 1 });
```

  In `test/calculation_tests.cpp`, after "documented() reads what it documents":

```cpp
TEST_CASE("a logarithm and an exponential read what their argument reads", "[calculation]")
{
    STATIC_REQUIRE(
        std::is_same_v<CalculationReadsOf<decltype(formula::ln(var<Factor> / var<Other>))>, QuantityList<Factor, Other>>);
    STATIC_REQUIRE(
        std::is_same_v<CalculationReadsOf<decltype(formula::exp(var<Other> - var<Factor>))>, QuantityList<Other, Factor>>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::define<Share>(formula::log10(var<Factor>)))::reads, QuantityList<Factor>>);
}
```

  `test/negative/overlay_substitution_inside_logarithm_across_overlays.cpp`, modelled on
  `overlay_substitution_inside_opaque_across_overlays.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it
// REJECT: cannot see inside
//
// The first overlay fixes the ratio, which the method reads only inside a
// logarithm. The second replaces the other variant with one that reads the
// ratio plainly. The whole-method rule must see the substitution inside the
// logarithm (`SubstitutedIn` for `TranscendentalNode`): without it, one
// method would read the ratio at 103/100 in one variant and from the
// environment in the other.
//
// This must not compile.
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <tuple>

namespace
{
struct Logged
{
};
struct Plain
{
};

struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};

constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };

inline constexpr auto m = formula::method(
    formula::variants(formula::variant<Logged>(formula::ln(formula::var<Ratio>)),
                      formula::variant<Plain>(formula::constant<formula::unit::One>(formula::Rational { 1 }))),
    formula::rounding_rule<formula::unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto first =
        formula::apply(formula::overlay(formula::with_constant<Ratio>(formula::Rational { 103, 100 }, annex)), m);
    constexpr auto second =
        formula::apply(formula::overlay(formula::replace_variant<Plain>(formula::var<Ratio>, annex)), first);
    return std::tuple_size_v<decltype(second.variantSet.cases)> == 0 ? 1 : 0;
}
```

  `test/negative/precision_level_in_level_through_logarithm.cpp`, modelled on
  `precision_level_in_own_level.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// A level written in terms of the level, the placeholder hidden inside an
// exponential: the level checks must see inside it (`LevelChildren` for
// `TranscendentalNode`). This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};
} // namespace

int main()
{
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::var<Ratio> * formula::exp(formula::precision_level<Ratio> - formula::precision_level<Ratio>),
        formula::Rational { 1, 50 } * formula::precision_level<Ratio>);
    auto const outcome = formula::checked_evaluate<Ratio>(
        broken, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }));
    return outcome.has_value() ? 0 : 1;
}
```

  Registrations: after `overlay_substitution_inside_opaque_across_overlays` (`:2277-2279`)

```cmake
formula_add_negative_test(overlay_substitution_inside_logarithm_across_overlays
    "formula: this method reads a quantity both where an overlay fixed or derived it"
    REJECT "cannot see inside")
```

  and after `precision_level_in_own_level` (`:799-801`)

```cmake
formula_add_negative_test(precision_level_in_level_through_logarithm
    "formula: this precision_limit's level expression reads precision_level"
    REJECT "precision_level is meaningful only inside" "provides no value for this quantity"
           "has no detail::LevelChildren specialisation")
```

  (Wrong text first for both, as the protocol says.)

- [ ] **Step 2: Run and watch it fail.** `CL -Filter "overlay|calculation|negative\.overlay_substitution_inside_logarithm|negative\.precision_level_in_level_through"`
  → `BUILD FAILED`: `overlay_tests.cpp` refuses with the overlay's own message ("… cannot see inside …",
  `RequireOverlaySeesNode`), and `calculation_tests.cpp` with `RequireLevelChildrenFor`'s ("this library
  node kind has no detail::LevelChildren specialisation").

- [ ] **Step 3: Implement.** `precision.hpp`, after the `RootNode` entry:

```cpp
    template <Transcendental F, Node Operand>
    struct LevelChildren<TranscendentalNode<F, Operand>>: LevelParent<Operand>
    {
    };
```

  `overlay.hpp`, after the `RootNode` `ConstantRewrite`:

```cpp
    template <typename Sub, Transcendental F, Node Operand>
    struct ConstantRewrite<Sub, TranscendentalNode<F, Operand>>:
        ConstantRewriteOperand<Sub, Operand, TranscendentalNode<F, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };
```

  and after the `RootNode` `SubstitutedIn`:

```cpp
    /// Needed for correctness, not only for completeness: the primary answers "none", so without it a
    /// substitution inside a logarithm or an exponential would be invisible to the whole-method rule.
    template <Transcendental F, Node Operand>
    struct SubstitutedIn<TranscendentalNode<F, Operand>>: SubstitutedInOperand<Operand>
    {
    };
```

- [ ] **Step 4: Run and pass.** The Step 2 command → `ALL OK` (wrong texts replaced by the right ones by
  now); then `GCC14 "overlay|calculation|negative\.overlay_substitution_inside_logarithm|negative\.precision_level_in_level_through"`.

- [ ] **Step 5: Deletion checks.** Delete the `SubstitutedIn` entry → the substitution case compiles →
  `negative.overlay_substitution_inside_logarithm_across_overlays` fails. Delete the `LevelChildren`
  entry → the level case draws `RequireLevelChildrenFor`'s message instead (its `REJECT` fires) → fails,
  and `calculation_tests.cpp` stops compiling. Delete the `ConstantRewrite` entry → `overlay_tests.cpp`
  stops compiling with the overlay's refusal. Restore each by plain write; rerun the Step 2 command green.

- [ ] **Step 6: Full verification.** `CL` and `GCC14`, no filter.

- [ ] **Step 7: Commit.**
  ```
  feat(function): overlays, precision limits and calculations see inside a logarithm

  A jurisdiction's fixed constant reaches the argument of ln, log10 and exp; the whole-method rule
  sees a substitution made there; a precision_level hidden in one is still found; and a definition
  lists what its logarithm reads. The substitution entry is needed for correctness: its primary
  template answers that nothing was substituted.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 3: The plain node on the page and in the trace

**Counts:** +5 ctest cases (+2 `render_tests`, +1 each `document_tests`, `trace_tests`,
`trace_render_tests`; the every-node-kind and consumer-globals edits extend existing cases); probe unchanged.

**Files:**
- Modify `include/formula-cpp/render.hpp` (a `detail` block and a `render_node` after the `RootNode`
  overload, `:1189-1209`).
- Modify `include/formula-cpp/document.hpp` (declaration after `:415-416`, definition after `:767-771`).
- Modify `include/formula-cpp/trace.hpp` (three enumerators appended to `StepKind` — after the last one,
  `AttemptInput` at `:486` before the foundation, `RoundedOpaqueOutput` after it; `StepKindOf` after
  `:1708-1712`).
- Modify `include/formula-cpp/trace_render.hpp` (a helper before `step_expression`, `:897-903`; cases after
  `case StepKind::Root:`, `:941-944`).
- Modify `test/render_tests.cpp`, `test/document_tests.cpp`, `test/trace_tests.cpp`,
  `test/trace_render_tests.cpp`, `test/vocabulary_tests.cpp`, `test/consumer_globals_tests.cpp`,
  `CHANGELOG.md`.

**Interfaces:**
- Consumes: `TranscendentalNode`, `detail::transcendental_name` (Task 1).
- Produces: `StepKind::NaturalLogarithm`, `StepKind::DecimalLogarithm`, `StepKind::Exponential`;
  `render_node(TranscendentalNode<F, Operand> const&, V const&)`; `detail::collect(Walk<V>&,
  TranscendentalNode<F, Operand> const&)`; in `render.hpp`'s `detail`:
  `constexpr std::string_view transcendental_latex_name(Transcendental) noexcept` and
  `template <Dialect D> std::string transcendental_text(Transcendental, std::string const& argumentText)`
  (Task 6 reuses it); in `trace_render.hpp`'s `detail`:
  `std::string transcendental_call(Transcendental, std::string const& operandText)`.

Readings, per the spec (§6): Plain `ln(x)`, `log10(x)`, `exp(x)`; Markdown ``ln(`x`)``; LaTeX
`\ln\left(x\right)`, `\log_{10}\left(x\right)`, `\exp\left(x\right)` — `\exp`, not `e^{x}`: `PowerNode`
renders its base as an atom, so `pow<2>(exp(x))` would become `e^{x}^{2}`, which LaTeX refuses, and `e` is
a quantity symbol of its own in many methods. Trace: `2. ln(#1) = no exact rational result exists`.

- [ ] **Step 1: Write the failing tests.**

  `test/render_tests.cpp` — two new cases after "render: pi renders per dialect" (`:171-175`):

```cpp
TEST_CASE("render: a logarithm and an exponential read as calls in every dialect", "[render]")
{
    CHECK(formula::render(formula::ln(var<Determinations>)) == "ln(n_d)");
    CHECK(formula::render(formula::log10(var<Determinations>)) == "log10(n_d)");
    CHECK(formula::render(formula::exp(var<Determinations>)) == "exp(n_d)");
    CHECK(formula::render<Dialect::Markdown>(formula::ln(var<Determinations>)) == "ln(`n_d`)");
    CHECK(formula::render<Dialect::Markdown>(formula::log10(var<Determinations>)) == "log10(`n_d`)");
    CHECK(formula::render<Dialect::Markdown>(formula::exp(var<Determinations>)) == "exp(`n_d`)");
    CHECK(formula::render<Dialect::LaTeX>(formula::ln(var<Determinations>)) == "\\ln\\left(n_d\\right)");
    CHECK(formula::render<Dialect::LaTeX>(formula::log10(var<Determinations>)) == "\\log_{10}\\left(n_d\\right)");
    CHECK(formula::render<Dialect::LaTeX>(formula::exp(var<Determinations>)) == "\\exp\\left(n_d\\right)");
}

TEST_CASE("render: a logarithm groups its own argument and is an atom to what holds it", "[render]")
{
    // The call's parentheses group a compound argument...
    CHECK(formula::render(formula::ln(var<WaterVolume> / var<CementVolume>)) == "ln(V_w / V_c)");
    CHECK(formula::render<Dialect::LaTeX>(formula::ln(var<WaterVolume> / var<CementVolume>))
          == "\\ln\\left(\\frac{V_w}{V_c}\\right)");
    // ...and the call is an atom: a power's base, a negation's operand, a difference's right side and a
    // quotient's numerator, with no bracket of its own.
    CHECK(formula::render(formula::pow<2>(formula::ln(var<Determinations>))) == "ln(n_d)^2");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(formula::ln(var<Determinations>)))
          == "\\ln\\left(n_d\\right)^{2}");
    CHECK(formula::render<Dialect::LaTeX>(formula::pow<2>(formula::exp(var<Determinations>)))
          == "\\exp\\left(n_d\\right)^{2}");
    CHECK(formula::render(-formula::exp(var<Determinations>)) == "-exp(n_d)");
    CHECK(formula::render(rat(1) - formula::log10(var<Determinations>)) == "1 - log10(n_d)");
    CHECK(formula::render<Dialect::LaTeX>(formula::exp(var<Determinations>) / rat(2))
          == "\\frac{\\exp\\left(n_d\\right)}{2}");
}
```

  and in "render: the Markdown dialect covers every node kind…" (`:200-226`), after the `AbsoluteValueNode`
  line:

```cpp
    CHECK(formula::render<Dialect::Markdown>(formula::log10(var<Determinations>)) == "log10(`n_d`)"); // TranscendentalNode
```

  and in "render: Markdown output never contains text a CommonMark parser reinterprets…", after the
  `AbsoluteValueNode` line (`:1576`):

```cpp
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::ln(var<Determinations>)));            // TranscendentalNode
    isInertInMarkdown(formula::render<Dialect::Markdown>(formula::exp(var<Diameter> / var<Diameter>)));
```

  `test/document_tests.cpp` — after the rounded square root case (`:278-289`):

```cpp
TEST_CASE("document: a logarithm lists what its argument reads and states itself as a call", "[document]")
{
    formula::Documentation const documentation = formula::document(formula::ln(var<WaterVolume> / var<CementVolume>));
    CHECK(documentation.formula == "ln(V_w / V_c)");
    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_c" });
    CHECK(formula::document<formula::Dialect::LaTeX>(formula::exp(var<WaterVolume> / var<CementVolume>)).formula
          == "\\exp\\left(\\frac{V_w}{V_c}\\right)");
}
```

  `test/trace_tests.cpp` — add `struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", unit::One> {};`
  to the anonymous namespace, and after "a RoundSignificant step records…" (`:402-424`):

```cpp
TEST_CASE("a logarithm step records its own kind and the error an irrational value is", "[trace]")
{
    auto const traced = [](auto const& node) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(
            node, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }), sink);
        return trace;
    };
    formula::Trace<> const natural = traced(formula::ln(var<Ratio>));
    REQUIRE(natural.steps.size() == 2);
    CHECK(natural.steps[1].kind == formula::StepKind::NaturalLogarithm);
    CHECK(natural.steps[1].error == formula::ArithmeticError::Inexact);
    CHECK_FALSE(natural.steps[1].value.has_value());
    CHECK(natural.steps[1].unit == formula::coherent(formula::dim::Scalar));
    REQUIRE(natural.steps[1].operands.size() == 1);
    CHECK(natural.steps[1].operands[0] == 0);
    CHECK(traced(formula::log10(var<Ratio>)).steps[1].kind == formula::StepKind::DecimalLogarithm);
    CHECK(traced(formula::exp(var<Ratio>)).steps[1].kind == formula::StepKind::Exponential);
}
```

  `test/trace_render_tests.cpp` — add
  `struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", unit::One> {};` and
  `struct Share: formula::Quantity<Share, "p", "an invented share", unit::Percent> {};` to the first anonymous
  namespace (`:23-80`), and after "a derivation renders a RoundedRoot step…" (`:361-382`):

```cpp
TEST_CASE("a derivation writes a logarithm or an exponential as a call on its argument's step", "[trace-render]")
{
    auto const traceOf = [](auto const& node, auto const& inputs) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, inputs, sink);
        return formula::render_trace(trace, { .maxSteps = 10 });
    };
    auto const ratioAt = [](formula::Rational ratioValue) {
        return formula::environment(formula::Measured<Ratio> { ratioValue });
    };
    CHECK(traceOf(formula::log10(var<Ratio>), ratioAt(formula::Rational { 1000 })) == "1. r = 1000\n2. log10(#1) = 3\n");
    CHECK(traceOf(formula::exp(formula::ln(var<Ratio> / var<Ratio>)), ratioAt(formula::Rational { 7 }))
          == "1. r = 7\n2. r = 7\n3. #1 / #2 = 1\n4. ln(#3) = 0\n5. exp(#4) = 1\n");
    // No exact value: the step says so, and so does every step it reaches. The irrational number
    // appears nowhere.
    CHECK(traceOf(formula::exp(formula::ln(var<Ratio>)), ratioAt(formula::Rational { 2 }))
          == "1. r = 2\n2. ln(#1) = no exact rational result exists\n3. exp(#2) = no exact rational result exists\n");
    CHECK(traceOf(formula::ln(var<Ratio>), ratioAt(formula::Rational { 0 }))
          == "1. r = 0\n2. ln(#1) = argument outside the domain of the operation\n");
    // A percentage is read as the number it is: 1000 % is 10.
    CHECK(traceOf(formula::log10(var<Share>), formula::environment(formula::Measured<Share> { formula::Rational { 1000 } }))
          == "1. p = 1000 %\n2. log10(#1) = 1\n");
}
```

  `test/vocabulary_tests.cpp` — in `everyNodeKind()` (`:679-713`), append to the last product, after
  `formula::precision_limit<formula::PrecisionKind::Repeatability>(r, formula::precision_level<EveryDerived>)`:

```cpp
               // A logarithm and an exponential, each at a point where it is exact, so the product keeps
               // its value: ln(r / r) = ln 1 = 0, exp 0 = 1 and log10 10 = 1.
               * formula::exp(formula::ln(r / r)) * formula::log10(formula::constant<unit::One>(rat(10)))
```

  and extend both pinned cube strings — "every node kind renders in the vocabulary, in every dialect"
  (`:850-853`) and "every node kind writes its numbers in the style asked for…" (`:1034-1040`) — so their
  last piece reads
  `"* critical(2, at 2, 3) * abs(E / R) * r(level; level = E / R) * exp(ln(E / R / (E / R))) * log10(10)");`.
  In "every node kind traces in the vocabulary" (`:1065-1090`) add, after the critical-value `find`:

```cpp
    // The logarithm and the exponential read the ratio over itself, and are calls on its step.
    CHECK(cube.find("ln(#") != std::string::npos);
```

  then run once, and replace that line with two exact `find`s of the lines the run prints: the three
  consecutive lines `"<j>. #<a> / #<b> = 1\n<j+1>. ln(#<j>) = 0\n<j+2>. exp(#<j+1>) = 1\n"` and the line
  `"<k>. log10(#<k-1>) = 1\n"` (whose operand is the constant 10's step). A reviewer checks the shapes.

  `test/consumer_globals_tests.cpp` — in `everything` (`:273-293`), append to the product inside
  `rounded<unit::Megapascal, …>(…)`, after the `rounded_to_digits<…>(numeric_value_of<…>(var<Factor>))`
  factor: `* formula::exp(formula::ln(var<Factor>)) * formula::log10(var<Factor> * formula::Rational { 10 })`
  (Factor is 1 in `specimen`, `elsewhere` and `boundRecords`, so the product keeps its value), and add to
  both lists of kinds (header comment `:18-21`, `everything`'s comment `:269-272`) "a logarithm, a decimal
  logarithm and an exponential".

- [ ] **Step 2: Run and watch it fail.** `CL -Filter "render|document|trace|vocabulary|consumer"` →
  `BUILD FAILED` (`'NaturalLogarithm': is not a member of 'formula::StepKind'`, and no `render_node` for
  `TranscendentalNode`).

- [ ] **Step 3: Implement.**

  `render.hpp`, after the `RootNode` overload:

```cpp
namespace detail
{
    /// How LaTeX writes @p function: `\ln`, `\log_{10}`, `\exp`. `\exp` and not `e^{...}`: a power
    /// renders its base as an atom, so `pow<2>(exp(x))` would read `e^{x}^{2}`, which LaTeX refuses; and
    /// `e` is a quantity's symbol in many methods.
    [[nodiscard]] constexpr std::string_view transcendental_latex_name(Transcendental function) noexcept
    {
        switch (function)
        {
            case Transcendental::NaturalLogarithm:
                return "\\ln";
            case Transcendental::DecimalLogarithm:
                return "\\log_{10}";
            case Transcendental::Exponential:
                return "\\exp";
        }
        return "\\operatorname{unknown}";
    }

    /// @p function called on @p argumentText, in dialect @p D: `ln(x)`, and in LaTeX `\ln\left(x\right)`.
    /// Shared by the plain node and the rounded one (`rounded_transcendental.hpp`).
    template <Dialect D>
    [[nodiscard]] std::string transcendental_text(Transcendental function, std::string const& argumentText)
    {
        if constexpr (D == Dialect::LaTeX)
            return std::string { transcendental_latex_name(function) } + "\\left(" + argumentText + "\\right)";
        else
            return std::string { transcendental_name(function) } + "(" + argumentText + ")";
    }
} // namespace detail

/// A logarithm or an exponential renders as a call on its argument -- `ln(x)`, `log10(x)`, `exp(x)`, and in
/// LaTeX `\ln\left(x\right)`, `\log_{10}\left(x\right)`, `\exp\left(x\right)` -- in the vocabulary's
/// symbols. Like `sqrt(...)`, the parentheses it always produces group its own argument, so it needs no
/// `PrecedenceOf` entry: the primary's `Atom` is right, and `pow<2>(ln(x))` reads `ln(x)^2`.
template <Dialect D, Transcendental F, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(TranscendentalNode<F, Operand> const& node, V const& vocabulary)
{
    return detail::transcendental_text<D>(F, render<D>(node.operand, vocabulary));
}
```

  `document.hpp`: declaration after `:415-416`
  `template <Vocabulary V, Transcendental F, Node Operand> void collect(Walk<V>& walk, TranscendentalNode<F, Operand> const& node);`
  and definition after `:767-771`:

```cpp
    /// A logarithm or an exponential reads what its argument reads.
    template <Vocabulary V, Transcendental F, Node Operand>
    void collect(Walk<V>& walk, TranscendentalNode<F, Operand> const& node)
    {
        collect(walk, node.operand);
    }
```

  `trace.hpp`, appended after the enum's last enumerator:

```cpp
    /// A `TranscendentalNode` taking the natural logarithm of its one operand (`ln`). Its value is exact
    /// -- 0 at 1 -- or its error is `Inexact`: an irrational logarithm is never a step's value.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `TranscendentalNode`, its own enumerator of this name
    /// is scoped in `Transcendental`, and the factory is `ln`, so nothing in namespace `formula` is spelt
    /// `NaturalLogarithm`.
    NaturalLogarithm,
    /// A `TranscendentalNode` taking the decimal logarithm of its one operand (`log10`): exact at 10^k.
    /// Checked on GCC under `-Wshadow`, as `NaturalLogarithm` is.
    DecimalLogarithm,
    /// A `TranscendentalNode` taking the exponential of its one operand (`exp`): exact at 0. Checked on
    /// GCC under `-Wshadow`, as `NaturalLogarithm` is.
    Exponential,
```

  and after the `RootNode` `StepKindOf`:

```cpp
    template <Transcendental F, Node Operand>
    struct StepKindOf<TranscendentalNode<F, Operand>>
    {
        static constexpr StepKind value = F == Transcendental::NaturalLogarithm   ? StepKind::NaturalLogarithm
                                          : F == Transcendental::DecimalLogarithm ? StepKind::DecimalLogarithm
                                                                                  : StepKind::Exponential;
    };
```

  `trace_render.hpp`, before `step_expression` (after `lineage_expression`, `:880-896`):

```cpp
    /// `ln(#1)`, `log10(#1)`, `exp(#1)`: @p function called on @p operandText, in `render()`'s words
    /// (`transcendental_name`), so that a derivation names the function its formula names.
    [[nodiscard]] inline std::string transcendental_call(Transcendental function, std::string const& operandText)
    {
        return std::string { transcendental_name(function) } + "(" + operandText + ")";
    }
```

  and in `step_expression`, after `case StepKind::Root:` (`:941-944`):

```cpp
            case StepKind::NaturalLogarithm:
                return transcendental_call(Transcendental::NaturalLogarithm, sole_operand(shownStep));
            case StepKind::DecimalLogarithm:
                return transcendental_call(Transcendental::DecimalLogarithm, sole_operand(shownStep));
            case StepKind::Exponential:
                return transcendental_call(Transcendental::Exponential, sole_operand(shownStep));
```

- [ ] **Step 4: Run and pass.** `CL -Filter "render|document|trace|vocabulary|consumer"`, then pin the
  vocabulary trace lines as Step 1 says, rerun → `ALL OK`; `GCC14 "render|document|trace|vocabulary|consumer"`
  (this is the `-Wshadow` check the enumerators' comments claim).

- [ ] **Step 5: CHANGELOG.** Extend the Task 1 `### Added` entry: "They render as `ln(x)`, `log10(x)` and
  `exp(x)`, in LaTeX as `\ln\left(x\right)`, `\log_{10}\left(x\right)` and `\exp\left(x\right)`, and a
  trace writes each as a step of its own, `ln(#1) = …`." `### Changed`: "- `StepKind` gains
  `NaturalLogarithm`, `DecimalLogarithm` and `Exponential`, appended; a consumer's `switch` over
  `StepKind` that names every enumerator warns until it handles them."

- [ ] **Step 6: Full verification.** `CL` and `GCC14`, no filter (`hygiene.vocabulary-reach` included: the
  new overload renders its operand with the vocabulary it was given).

- [ ] **Step 7: Commit.**
  ```
  feat(render): logarithms and exponentials on the page and in the trace

  A logarithm or an exponential reads as the call it is in every dialect -- \exp in LaTeX, since a
  power renders its base as an atom and e^{x}^{2} is no formula -- and a derivation writes it as a
  step on its argument's, whose value is exact or whose error says there is none. Three StepKind
  enumerators are appended.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 4: The kernel — enclosures of ln, log10 and exp in integers

**Counts:** +6 ctest cases (`transcendental_tests`); probe unchanged.

**Files:**
- Create `include/formula-cpp/detail/transcendental.hpp`, `test/transcendental_tests.cpp`.
- Modify root `CMakeLists.txt` (`FILE_SET`, detail headers `:44-50`), `test/CMakeLists.txt` (sources,
  after `rounded_root_tests.cpp`, `:53`).
- `detail/wide_int.hpp` is **not** modified: the foundation ships every primitive the kernel uses.

**Interfaces:**
- Consumes (foundation, `detail/wide_int.hpp`): `WideUnsigned<12>` (`from_u64`, `is_zero`, `limb`,
  `<=>`), `add_checked_or_none`, `sub_checked_or_none`, `mul_checked_or_none` (skips the zero limbs of its
  left operand), `mul_small_checked_or_none`, `add_small_checked_or_none`, `shift_left_checked_or_none`,
  `shift_right`, `pow10<12>`, `WideRatio<12>` (`negative`, `numerator`, `denominator`), and
  `template <std::size_t L> struct WideSmallDivision { WideUnsigned<L> quotient; std::uint32_t remainder; };`
  with `template <std::size_t L> constexpr WideSmallDivision<L> divmod_small(WideUnsigned<L> const& dividend,
  std::uint32_t divisor) noexcept` (divisor != 0). From `detail/wide_rounding.hpp`:
  `decide_rounding(lowerBound, upperBound, places, roundingMode)` (tests here; Task 5's node).
  `detail::magnitude` (`checked_int.hpp:41-44`).
- Produces (namespace `formula::detail`):
  ```cpp
  inline constexpr std::size_t KernelLimbs = 12;          using KernelWord = WideUnsigned<KernelLimbs>;
  inline constexpr std::size_t KernelFractionBits = 128;  inline constexpr KernelWord KernelOne;   // 2^128
  inline constexpr KernelWord Ln2Lower, Ln2Upper, Log10eLower, Log10eUpper;
  inline constexpr std::uint32_t AtanhSlack = 64, ExponentialSlack = 512, AtanhTermLimit = 42, TaylorTermLimit = 40;
  struct Enclosure { WideRatio<KernelLimbs> lower; WideRatio<KernelLimbs> upper; };
  struct ScaledQuotient { KernelWord below; bool exact; };
  constexpr ScaledQuotient scaled_quotient(std::uint64_t dividend, std::uint64_t divisor) noexcept;        // floor(dividend 2^128 / divisor)
  constexpr std::optional<KernelWord> atanh_series_lower(KernelWord const& fixedArgument) noexcept;
  constexpr std::optional<KernelWord> exponential_series_lower(KernelWord const& fixedArgument) noexcept;
  constexpr std::optional<Enclosure> natural_log_enclosure(Rational positive) noexcept;   // positive > 0, != 1
  constexpr std::optional<Enclosure> decimal_log_enclosure(Rational positive) noexcept;   // positive > 0, not 10^k
  constexpr std::optional<Enclosure> exponential_enclosure(Rational argument) noexcept;   // != 0, -43 <= argument <= 44
  ```
  An empty `optional` means a bound derived below did not hold; the caller (Task 5) reports it as
  `Overflow`. No test input reaches it.

### The algorithm and its error bound

This text goes, as it stands, into the header's file comment (Step 5), with the measured step counts
(Step 7) appended.

- **Fixed point.** A value v is an integer V in `WideUnsigned<12>` (384 bits) with 128 fraction bits,
  V = floor(v 2^128). Every operation truncates a non-negative value, so a lower bound stays one; each
  upper bound is the lower bound plus a slack derived here. No `<cmath>`, no floating point, no
  intrinsics, no 128-bit type, **and no call of the general `divmod`** (one costs about 130 000 cl steps
  over 384 bits): the two scaled quotients are 64-bit long divisions, k is a binary search, and the
  series' divisors are below 2^32 (`divmod_small`).
- **ln(a/b)**, a, b > 0, a != b. For a < b, ln(b/a) is taken and negated: the sign comes from a < b and
  never from a rounding. So let a > b. B = b 2^k with B <= a < 2B, so k <= 62 and a + B < 2^64. With
  z = (a - B)/(a + B), 0 <= z < 1/3, ln(a/b) = k ln 2 + 2 atanh(z), atanh(z) = sum_{i>=0} z^(2i+1)/(2i+1).
  Z = floor(z 2^128); Z2 = floor(Z^2 / 2^128); P_0 = Z, P_{i+1} = floor(P_i Z2 / 2^128);
  S = sum floor(P_i / (2i+1)) until P_i = 0. Then S <= atanh(z) 2^128. Deficits: Z2 is below z^2 2^128
  by less than 2z + 1 < 2; the deficit d_i of P_i against z^(2i+1) 2^128 obeys d_0 < 1 and
  d_{i+1} < 2 z^(2i+1) + z^2 d_i + 1, so d_i < 2 throughout; each term is short by less than
  1 + d_i/(2i+1); P_i is 0 by i = 41 (z^83 2^128 < 1), after which the tail is below 2.25/(2i+1). So
  atanh(z) 2^128 - S < 42 + 2 (1 + 1/3 + ... + 1/83) + 1 < 48 <= `AtanhSlack` = 64. With
  L = floor(ln 2 2^128): lower = k L + 2S, upper = k (L + 1) + 2 (S + 64) — at most k + 128 <= 190 units
  of 2^-128 apart, under 2^-120, absolute.
- **log10** = ln log10(e). With M = floor(log10(e) 2^128): lower = floor(lower_ln M / 2^128),
  upper = floor(upper_ln (M + 1) / 2^128) + 1, under 190 · 0.44 + 44 + 2 < 130 units apart, since
  |ln(a/b)| <= ln(2^63) < 44. The widest product, upper_ln (M + 1), is below 2^261.
- **exp(x)**, x = a/b != 0, -43 <= x <= 44 (Task 5 answers outside). X = floor(|x| 2^128), exact or one
  below. For x > 0, k is the largest integer in [0, 63] with k (L + 1) <= X (64 ln 2 > 44 bounds it), and
  R = X - k (L + 1) <= r 2^128 for r = x - k ln 2; for x < 0, m is the smallest in [1, 63] with
  m L >= X' (X' = X + 1 when X is inexact; 63 ln 2 > 43 bounds it), R = m L - X' <= r 2^128 for
  r = m ln 2 - |x|, and exp(x) = 2^-m exp(r). Either way 0 <= R, r < ln 2 + 2^-121, and r 2^128 - R <= 64
  (one unit for X, one per multiple of ln 2's unit). E = sum T_j, T_0 = 2^128,
  T_j = floor(floor(T_{j-1} R / 2^128) / j), until T_j = 0 (within 40 terms for r < 0.7), so
  E <= exp(R 2^-128) 2^128 <= exp(r) 2^128. Each T_j is short by e_j < e_{j-1} r / j + 1 < 2, and the tail
  after the last term is below 3, so exp(R 2^-128) 2^128 - E < 2 · 40 + 3; the 64 units of r add less than
  2 · 1.0001 · 64 < 129. So exp(r) 2^128 < E + 212 <= E + `ExponentialSlack` = 512:
  lower = E 2^k / 2^128, upper = (E + 512) 2^k / 2^128 (for x < 0, denominator 2^(128+m)), under 2^-119
  relative. The widest numerator, (E + 512) 2^63, is below 2^193; `decide_rounding`'s scaling by up to
  10^18 keeps it below 2^253.
- **Undecided.** When the two ends round differently, `decide_rounding` answers `Overflow`: the rounding
  needs more bits than the kernel holds. The rounded decimal exists, so `Inexact` would be wrong, and a new
  error enumerator would break `describe()` and consumers' switches.

- [ ] **Step 1: Measure cl's default constexpr step budget** in a scratch directory outside the worktree.
  `budget.cpp`:

```cpp
constexpr unsigned spin(unsigned n) noexcept
{
    unsigned total = 0;
    for (unsigned at = 0; at < n; ++at)
        total += at;
    return total;
}
static_assert(spin(SPIN) != 1U);
int main() { return 0; }
```

  In a VS dev shell, `cl /nologo /std:c++latest /permissive- /c /Zs /DSPIN=<n> budget.cpp` compiles for
  n = 340000 and fails for n = 360000 on cl 19.51.36257 (measured while planning; the largest n is about
  349 436, which `/constexpr:steps1049814` also admits). Record your cl version and the two results in
  the task report. If they differ from these, stop and tell the controller before Step 7.

- [ ] **Step 2: Confirm the foundation's primitives and fix the target.** Read `detail/wide_int.hpp`:
  `divmod_small` has the signature under *Interfaces* and walks the limbs once, and `mul_checked_or_none`
  skips a zero limb of its left operand. Both are the foundation's, added for this kernel; if either is
  missing or different, stop and tell the controller (do not add them here). **The per-call target**,
  from the planning measurements at `KernelLimbs = 12` with both primitives (Finding 4: 3 000–31 000 cl
  steps per enclosure; 62 000–95 000 on clang-cl 22.1.3 without the zero-limb skip): **one kernel call at
  most 100 000 steps on cl 19.51**, about a tenth of the effective default of about 1 049 000, with
  `KernelLimbs = 12` kept. A whole rounding adds `decide_rounding`, about a quarter of a million steps
  more (whole roundings measured 240 600–284 500 while planning), so the kernel's checks run at run time
  but one (Step 7, *Compile time and run time*). Step 7
  measures each call and reports the figure; only a call above 100 000 stops the task.

- [ ] **Step 3: Write the failing kernel tests.** `test/transcendental_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// The integer kernel behind the rounded logarithms and exponential (detail/transcendental.hpp),
// against an independent reference. Each reference row is the value's first 40 significant digits,
// truncated, so that |value| lies in [D, D + 1] * 10^-scale. The digits were computed once with
// Python 3.13's decimal module (libmpdec), whose ln, log10 and exp are correctly rounded, at 150
// significant digits:
//
//     from decimal import Decimal, getcontext, ROUND_FLOOR
//     getcontext().prec = 150
//     v = Decimal(a).ln() - Decimal(b).ln()          # log10: .log10(); exp: (Decimal(a) / Decimal(b)).exp()
//     scale = 40 - (abs(v).adjusted() + 1)
//     D = int((abs(v) * Decimal(10) ** scale).to_integral_value(rounding=ROUND_FLOOR))
//
// The published values the stored constants are checked against: ln 2 =
// 0.6931471805599453094172321214581765680755..., log10(e) = 0.4342944819032518276511289189166050822943....
//
// Every check that runs the kernel runs at run time, but one: a whole rounding costs a quarter of a
// compiler's default constant-evaluation budget, too near it to pin many. The last case keeps one at
// compile time, on purpose. The stored constants, which run no kernel, are checked at compile time.
#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace
{
namespace detail = formula::detail;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using formula::Transcendental;
using Word = detail::KernelWord;
using Ratio = detail::WideRatio<detail::KernelLimbs>;

constexpr std::array<RoundingMode, 7> everyMode { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero,
                                                  RoundingMode::HalfEven,         RoundingMode::Ceiling,
                                                  RoundingMode::Floor,            RoundingMode::TowardZero,
                                                  RoundingMode::AwayFromZero };

/// The kernel's enclosure of @p function at @p argument.
[[nodiscard]] constexpr std::optional<detail::Enclosure> enclosure_of(Transcendental function, Rational argument)
{
    switch (function)
    {
        case Transcendental::NaturalLogarithm:
            return detail::natural_log_enclosure(argument);
        case Transcendental::DecimalLogarithm:
            return detail::decimal_log_enclosure(argument);
        case Transcendental::Exponential:
            return detail::exponential_enclosure(argument);
    }
    return std::nullopt;
}

/// @p function at @p argument rounded to @p places under @p roundingMode by the kernel's enclosure.
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError>
    kernel_rounding(Transcendental function, Rational argument, int places, RoundingMode roundingMode)
{
    std::optional<detail::Enclosure> const enclosure = enclosure_of(function, argument);
    if (!enclosure.has_value())
        return std::unexpected { formula::ArithmeticError::Overflow };
    return detail::decide_rounding(enclosure->lower, enclosure->upper, DecimalPlaces { places }, roundingMode);
}

/// The decimal digits @p decimalDigits as a word.
[[nodiscard]] constexpr Word word_of(std::string_view decimalDigits)
{
    Word parsed {};
    for (char const each: decimalDigits)
        parsed = *detail::add_small_checked_or_none(*detail::mul_small_checked_or_none(parsed, 10U), static_cast<std::uint32_t>(each - '0'));
    return parsed;
}

/// Whether @p left <= @p right, for two signed ratios with positive denominators.
[[nodiscard]] constexpr bool at_most(Ratio const& left, Ratio const& right)
{
    bool const leftNegative = left.negative && !left.numerator.is_zero();
    bool const rightNegative = right.negative && !right.numerator.is_zero();
    if (leftNegative != rightNegative)
        return leftNegative;
    Word const leftCross = *detail::mul_checked_or_none(left.numerator, right.denominator);
    Word const rightCross = *detail::mul_checked_or_none(right.numerator, left.denominator);
    return leftNegative ? rightCross <= leftCross : leftCross <= rightCross;
}

/// Whether @p stored is floor(v * 2^128) for the value v whose first 40 digits are @p published:
/// (D - 1) * 2^128 >= stored * 10^40 and (D + 1) * 2^128 <= (stored + 1) * 10^40. That interval is
/// 2 * 2^128 / 10^40 < 0.07 units wide, so it pins the floor.
[[nodiscard]] constexpr bool pinned_by_published_digits(Word const& stored, std::string_view published)
{
    Word const digitsValue = word_of(published);
    Word const unit = *detail::shift_left_checked_or_none(Word::from_u64(1), 128);
    Word const tenTo40 = *detail::pow10<detail::KernelLimbs>(40);
    return *detail::mul_checked_or_none(stored, tenTo40) <= *detail::mul_checked_or_none(*detail::sub_checked_or_none(digitsValue, Word::from_u64(1)), unit)
           && *detail::mul_checked_or_none(*detail::add_small_checked_or_none(digitsValue, 1U), unit)
                  <= *detail::mul_checked_or_none(*detail::add_small_checked_or_none(stored, 1U), tenTo40);
}

/// One row of the reference table: |value| lies in [digits, digits + 1] * 10^-scale.
struct Reference
{
    Transcendental function;
    std::int64_t numerator;
    std::int64_t denominator;
    bool negative;
    std::string_view digits;
    std::size_t scale;
};

// clang-format off
constexpr std::array<Reference, 39> references { {
    { Transcendental::NaturalLogarithm, 2, 1, false, "6931471805599453094172321214581765680755", 40 },
    { Transcendental::NaturalLogarithm, 1, 2, true, "6931471805599453094172321214581765680755", 40 },
    { Transcendental::NaturalLogarithm, 3, 1, false, "1098612288668109691395245236922525704647", 39 },
    { Transcendental::NaturalLogarithm, 10, 1, false, "2302585092994045684017991454684364207601", 39 },
    { Transcendental::NaturalLogarithm, 7, 5, false, "3364722366212129305045934102169920901114", 40 },
    { Transcendental::NaturalLogarithm, 4611686018427387904, 1, false, "4297512519471660918386839153040694722068", 38 },
    { Transcendental::NaturalLogarithm, 4611686018427387903, 2305843009213693952, false, "6931471805599453092003916869610756812504", 40 },
    { Transcendental::NaturalLogarithm, 9223372036854775807, 9223372036854775806, false, "1084202172485504434183776953493284837650", 58 },
    { Transcendental::NaturalLogarithm, 9223372036854775807, 1, false, "4366827237527655449317720343461657334534", 38 },
    { Transcendental::NaturalLogarithm, 1, 9223372036854775807, true, "4366827237527655449317720343461657334534", 38 },
    { Transcendental::NaturalLogarithm, 1000001, 1000000, false, "9999995000003333330833335333331666668095", 46 },
    { Transcendental::NaturalLogarithm, 999999, 1000000, true, "1000000500000333333583333533333500000142", 45 },
    { Transcendental::NaturalLogarithm, 22, 7, false, "1145132304303002548373822955980126138260", 39 },
    { Transcendental::NaturalLogarithm, 1, 20, true, "2995732273553990993435223576142540775676", 39 },
    { Transcendental::NaturalLogarithm, 355, 113, false, "1144729970763075227348082372123200064255", 39 },
    { Transcendental::NaturalLogarithm, 1000000000000000000, 1, false, "4144653167389282231232384618431855573681", 38 },
    { Transcendental::DecimalLogarithm, 2, 1, false, "3010299956639811952137388947244930267681", 40 },
    { Transcendental::DecimalLogarithm, 1, 2, true, "3010299956639811952137388947244930267681", 40 },
    { Transcendental::DecimalLogarithm, 3, 1, false, "4771212547196624372950279032551153092001", 40 },
    { Transcendental::DecimalLogarithm, 7, 1, false, "8450980400142568307122162585926361934835", 40 },
    { Transcendental::DecimalLogarithm, 20, 1, false, "1301029995663981195213738894724493026768", 39 },
    { Transcendental::DecimalLogarithm, 1, 3, true, "4771212547196624372950279032551153092001", 40 },
    { Transcendental::DecimalLogarithm, 999999999999999999, 1, false, "1799999999999999999956570551809674817213", 38 },
    { Transcendental::DecimalLogarithm, 4611686018427387904, 1, false, "1866385973116683410325181147291856765962", 38 },
    { Transcendental::DecimalLogarithm, 5, 1, false, "6989700043360188047862611052755069732318", 40 },
    { Transcendental::DecimalLogarithm, 1001, 1000, false, "4340774793186406689213877779888660200037", 43 },
    { Transcendental::Exponential, 1, 1, false, "2718281828459045235360287471352662497757", 39 },
    { Transcendental::Exponential, -1, 1, false, "3678794411714423215955237701614608674458", 40 },
    { Transcendental::Exponential, 43, 1, false, "4727839468229346561474457562744280370819", 21 },
    { Transcendental::Exponential, -43, 1, false, "2115131037591080486631401007022651470196", 58 },
    { Transcendental::Exponential, 1, 2, false, "1648721270700128146848650787814163571653", 39 },
    { Transcendental::Exponential, -1, 2, false, "6065306597126334236037995349911804534419", 40 },
    { Transcendental::Exponential, 10, 1, false, "2202646579480671651695790064528424436635", 35 },
    { Transcendental::Exponential, -10, 1, false, "4539992976248485153559151556055061023791", 44 },
    { Transcendental::Exponential, 1, 1000, false, "1001000500166708341668055753993058311563", 39 },
    { Transcendental::Exponential, 437, 10, false, "9520699529632624602822133077880255204738", 21 },
    { Transcendental::Exponential, 1, 4611686018427387904, false, "1000000000000000000216840434497100886825", 39 },
    { Transcendental::Exponential, -1, 4611686018427387904, false, "9999999999999999997831595655028991132220", 40 },
    { Transcendental::Exponential, 44, 1, false, "1285160011435930827580929963214309925780", 20 },
} };
// clang-format on

/// The reference's two ends for @p row.
[[nodiscard]] constexpr std::array<Ratio, 2> reference_bounds(Reference const& row)
{
    Word const below = word_of(row.digits);
    Word const above = *detail::add_small_checked_or_none(below, 1U);
    Word const tenToScale = *detail::pow10<detail::KernelLimbs>(row.scale);
    if (row.negative)
        return { Ratio { true, above, tenToScale }, Ratio { true, below, tenToScale } };
    return { Ratio { false, below, tenToScale }, Ratio { false, above, tenToScale } };
}
} // namespace

TEST_CASE("transcendental kernel: the stored ln 2 and log10(e) are the published values", "[transcendental]")
{
    STATIC_REQUIRE(pinned_by_published_digits(detail::Ln2Lower, "6931471805599453094172321214581765680755"));
    STATIC_REQUIRE(pinned_by_published_digits(detail::Log10eLower, "4342944819032518276511289189166050822943"));
    STATIC_REQUIRE(detail::Ln2Upper == *detail::add_small_checked_or_none(detail::Ln2Lower, 1U));
    STATIC_REQUIRE(detail::Log10eUpper == *detail::add_small_checked_or_none(detail::Log10eLower, 1U));
}

TEST_CASE("transcendental kernel: the kernel re-derives its stored constants from its own series", "[transcendental]")
{
    // At run time: the kernel's series, like every check that runs it but one (see the last case).
    // ln 2 = 2 atanh(1/3), since (1 + 1/3) / (1 - 1/3) = 2: the series' enclosure of it meets the stored one.
    std::optional<Word> const atanhThird = detail::atanh_series_lower(detail::scaled_quotient(1, 3).below);
    REQUIRE(atanhThird.has_value());
    Word const ln2Lower = *detail::add_checked_or_none(*atanhThird, *atanhThird);
    Word const ln2Upper = *detail::add_checked_or_none(ln2Lower, Word::from_u64(2 * detail::AtanhSlack));
    CHECK(ln2Lower <= detail::Ln2Upper);
    CHECK(detail::Ln2Lower <= ln2Upper);
    // log10(e) = 1 / ln 10, and ln 10 = 3 ln 2 + 2 atanh(1/9), since (1 + 1/9) / (1 - 1/9) = 10/8. The
    // stored M meets [2^256 / upper, 2^256 / lower] over the whole enclosure of ln 10 * 2^128.
    std::optional<Word> const atanhNinth = detail::atanh_series_lower(detail::scaled_quotient(1, 9).below);
    REQUIRE(atanhNinth.has_value());
    Word const ln10Lower = *detail::add_checked_or_none(*detail::mul_small_checked_or_none(detail::Ln2Lower, 3U),
                                                        *detail::add_checked_or_none(*atanhNinth, *atanhNinth));
    Word const ln10Upper = *detail::add_checked_or_none(
        *detail::mul_small_checked_or_none(detail::Ln2Upper, 3U),
        *detail::mul_small_checked_or_none(*detail::add_small_checked_or_none(*atanhNinth, detail::AtanhSlack), 2U));
    Word const twoTo256 = *detail::shift_left_checked_or_none(Word::from_u64(1), 256);
    CHECK(*detail::mul_checked_or_none(detail::Log10eLower, ln10Lower) <= twoTo256);
    CHECK(twoTo256 <= *detail::mul_checked_or_none(detail::Log10eUpper, ln10Upper));
}

TEST_CASE("transcendental kernel: every reference value is enclosed and rounds as the reference does in every mode",
          "[transcendental]")
{
    constexpr std::array<int, 9> placesTried { -2, -1, 0, 1, 2, 4, 9, 17, 18 };
    std::size_t compared = 0;
    for (Reference const& row: references)
    {
        INFO("row " << row.numerator << "/" << row.denominator);
        std::optional<detail::Enclosure> const enclosure = enclosure_of(row.function, Rational { row.numerator, row.denominator });
        REQUIRE(enclosure.has_value());
        std::array<Ratio, 2> const referenceEnds = reference_bounds(row);
        // The value lies in both intervals, so they meet.
        CHECK(at_most(enclosure->lower, referenceEnds[1]));
        CHECK(at_most(referenceEnds[0], enclosure->upper));
        for (int const places: placesTried)
            for (RoundingMode const roundingMode: everyMode)
            {
                INFO("places " << places << ", mode " << formula::describe(roundingMode));
                CHECK(detail::decide_rounding(enclosure->lower, enclosure->upper, DecimalPlaces { places }, roundingMode)
                      == detail::decide_rounding(referenceEnds[0], referenceEnds[1], DecimalPlaces { places }, roundingMode));
                ++compared;
            }
    }
    // 39 rows, 9 places, 7 modes: a loop over nothing fails here.
    REQUIRE(compared == 2457);
    // Three of them written out, so that a reader sees the digits.
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, Rational { 2 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(693'147'180'559'945'309, -18));
    CHECK(kernel_rounding(Transcendental::DecimalLogarithm, Rational { 2 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(301'029'995'663'981'195, -18));
    CHECK(kernel_rounding(Transcendental::Exponential, Rational { 1 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(2'718'281'828'459'045'235, -18));
}

TEST_CASE("transcendental kernel: an enclosure is at most 2^-118 wide", "[transcendental]")
{
    // Absolute for the logarithms, whose ends share the denominator 2^128: at most 2^10 units apart.
    // Relative for the exponential: upper - lower at most lower / 2^118.
    Word const unit = *detail::shift_left_checked_or_none(Word::from_u64(1), 128);
    for (Reference const& row: references)
    {
        std::optional<detail::Enclosure> const enclosure = enclosure_of(row.function, Rational { row.numerator, row.denominator });
        REQUIRE(enclosure.has_value());
        Ratio const& nearer = enclosure->lower.negative ? enclosure->upper : enclosure->lower;
        Ratio const& farther = enclosure->lower.negative ? enclosure->lower : enclosure->upper;
        REQUIRE(nearer.denominator == farther.denominator);
        Word const width = *detail::sub_checked_or_none(farther.numerator, nearer.numerator);
        if (row.function == Transcendental::Exponential)
            CHECK(*detail::shift_left_checked_or_none(width, 118) <= nearer.numerator);
        else
        {
            CHECK(nearer.denominator == unit);
            CHECK(width <= Word::from_u64(1024));
        }
    }
}

TEST_CASE("transcendental kernel: an enclosure that straddles a tie is Overflow and never a guess", "[transcendental]")
{
    // ln(1 + 1/(2 * 10^18)) = 5 * 10^-19 - 1.25 * 10^-37 + ...: 1.25 * 10^-37 below the tie between 0 and
    // 10^-18 at 18 places, nearer than the enclosure is wide (128 units of 2^-128, about 3.8 * 10^-37).
    // The half modes cannot be decided and say Overflow; the directed modes can, and answer. The true
    // rounding in the half modes is 0: Overflow here means more bits were needed, never a wrong number.
    constexpr Rational nearTie { 2'000'000'000'000'000'001, 2'000'000'000'000'000'000 };
    for (RoundingMode const roundingMode: { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven })
        CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, roundingMode)
              == std::expected<Rational, formula::ArithmeticError> { std::unexpected { formula::ArithmeticError::Overflow } });
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::Floor) == Rational {});
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::TowardZero) == Rational {});
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::Ceiling) == Rational::from_decimal(1, -18));
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::AwayFromZero) == Rational::from_decimal(1, -18));
}

TEST_CASE("transcendental kernel: the kernel answers at compile time", "[transcendental]")
{
    // The one deliberate compile-time check of the kernel. A whole rounding -- the enclosure and
    // decide_rounding -- in one constant evaluation, the cheapest measured while planning: about
    // 240 600 steps on cl 19.51 and 298 000 on clang-cl 22.1.3, against defaults of about 1 049 000
    // and 1 048 576. Every other check that runs the kernel runs at run time.
    STATIC_REQUIRE(kernel_rounding(Transcendental::DecimalLogarithm, Rational { 2 }, 3, RoundingMode::HalfEven)
                   == Rational { 301, 1000 });
}
```

  Add `transcendental_tests.cpp` to `formula-cpp-tests` after `rounded_root_tests.cpp`
  (`test/CMakeLists.txt:53`). If the foundation named `decide_rounding`'s result or `WideRatio`'s members
  differently, adapt the calls, not the assertions.

- [ ] **Step 4: Run and watch it fail.** `CL -Filter "transcendental kernel"` → `BUILD FAILED`
  (`cannot open include file: 'formula-cpp/detail/transcendental.hpp'`).

- [ ] **Step 5: Implement `include/formula-cpp/detail/transcendental.hpp`.** File comment: one paragraph on
  what the kernel is for (the rounded forms, `rounded_transcendental.hpp`), then *The algorithm and its
  error bound* above, verbatim, then (Step 7) the measured step counts. Code:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/rational.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace formula::detail
{
    /// The kernel's width, 384 bits: room for its widest product (under 2^261) and for
    /// `decide_rounding`'s scaling by up to 10^18 of every end it is handed (under 2^253).
    inline constexpr std::size_t KernelLimbs = 12;
    /// A value in the kernel's fixed point.
    using KernelWord = WideUnsigned<KernelLimbs>;
    /// How many of a fixed-point value's bits are fraction.
    inline constexpr std::size_t KernelFractionBits = 128;
    /// How far, in units of 2^-128, the atanh series' lower bound can fall short -- see the file comment.
    inline constexpr std::uint32_t AtanhSlack = 64;
    /// How far the exponential's lower bound can fall short of exp(r), r's own width included.
    inline constexpr std::uint32_t ExponentialSlack = 512;
    /// More terms than the atanh series takes for any z below 1/3; reaching it is refused.
    inline constexpr std::uint32_t AtanhTermLimit = 42;
    /// More terms than the exponential's series takes for any r below 0.7; reaching it is refused.
    inline constexpr std::uint32_t TaylorTermLimit = 40;

    /// Two ends between which a value certainly lies: lower <= value <= upper.
    struct Enclosure
    {
        /// The lower end.
        WideRatio<KernelLimbs> lower;
        /// The upper end.
        WideRatio<KernelLimbs> upper;
    };

    /// @p highHalf * 2^64 + @p lowHalf as a kernel word. 128 bits in 384: neither step can overflow.
    [[nodiscard]] constexpr KernelWord kernel_word(std::uint64_t highHalf, std::uint64_t lowHalf) noexcept
    {
        return *add_checked_or_none(*shift_left_checked_or_none(KernelWord::from_u64(highHalf), 64), KernelWord::from_u64(lowHalf));
    }

    /// One in the kernel's fixed point, 2^128.
    inline constexpr KernelWord KernelOne = *shift_left_checked_or_none(KernelWord::from_u64(1), KernelFractionBits);
    /// floor(ln 2 * 2^128): ln 2 lies in [Ln2Lower, Ln2Upper] * 2^-128. Checked against its published
    /// digits, and re-derived by the kernel's own series, in `transcendental_tests.cpp`.
    inline constexpr KernelWord Ln2Lower = kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6AFULL);
    /// Ln2Lower + 1.
    inline constexpr KernelWord Ln2Upper = kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6B0ULL);
    /// floor(log10(e) * 2^128), checked as `Ln2Lower` is.
    inline constexpr KernelWord Log10eLower = kernel_word(0x6F2D'EC54'9B94'38CAULL, 0x9AAD'D557'D699'EE19ULL);
    /// Log10eLower + 1.
    inline constexpr KernelWord Log10eUpper = kernel_word(0x6F2D'EC54'9B94'38CAULL, 0x9AAD'D557'D699'EE1AULL);

    /// floor(dividend * 2^128 / divisor), and whether that is exact.
    struct ScaledQuotient
    {
        /// The quotient, rounded down.
        KernelWord below;
        /// Whether nothing was rounded away.
        bool exact;
    };

    /// @p dividend * 2^128 / @p divisor, by long division in 64-bit words: the whole part, then the 128
    /// fraction bits one at a time. The running remainder stays below the divisor; doubled, it leaves 64
    /// bits only when it is then above the divisor, and the subtraction, taken modulo 2^64, is then exact.
    /// @pre divisor != 0.
    [[nodiscard]] constexpr ScaledQuotient scaled_quotient(std::uint64_t dividend, std::uint64_t divisor) noexcept
    {
        std::uint64_t remaining = dividend % divisor;
        std::uint64_t highBits = 0;
        std::uint64_t lowBits = 0;
        for (std::size_t bit = 0; bit < KernelFractionBits; ++bit)
        {
            bool const carriedOut = (remaining >> 63) != 0;
            remaining <<= 1;
            highBits = (highBits << 1) | (lowBits >> 63);
            lowBits <<= 1;
            if (carriedOut || remaining >= divisor)
            {
                remaining -= divisor;
                lowBits |= 1;
            }
        }
        // The whole part is below 2^64 and the fraction below 2^128: the sum fits.
        KernelWord const whole = *shift_left_checked_or_none(KernelWord::from_u64(dividend / divisor), KernelFractionBits);
        return { *add_checked_or_none(whole, kernel_word(highBits, lowBits)), remaining == 0 };
    }

    /// A lower bound of atanh(z) * 2^128 for z = @p fixedArgument * 2^-128 below 1/3 -- see the file
    /// comment; nothing when a bound failed or the series had not ended by `AtanhTermLimit`.
    [[nodiscard]] constexpr std::optional<KernelWord> atanh_series_lower(KernelWord const& fixedArgument) noexcept
    {
        std::optional<KernelWord> const squaredProduct = mul_checked_or_none(fixedArgument, fixedArgument);
        if (!squaredProduct)
            return std::nullopt;
        KernelWord const squared = shift_right(*squaredProduct, KernelFractionBits);
        KernelWord oddPower = fixedArgument;
        KernelWord partialSum {};
        for (std::uint32_t term = 0; !oddPower.is_zero(); ++term)
        {
            if (term == AtanhTermLimit)
                return std::nullopt;
            std::optional<KernelWord> const added = add_checked_or_none(partialSum, divmod_small(oddPower, 2 * term + 1).quotient);
            std::optional<KernelWord> const product = mul_checked_or_none(oddPower, squared);
            if (!added || !product)
                return std::nullopt;
            partialSum = *added;
            oddPower = shift_right(*product, KernelFractionBits);
        }
        return partialSum;
    }

    /// A lower bound of exp(r) * 2^128 for r = @p fixedArgument * 2^-128 below 0.7 -- see the file
    /// comment; nothing when a bound failed or the series had not ended by `TaylorTermLimit`.
    [[nodiscard]] constexpr std::optional<KernelWord> exponential_series_lower(KernelWord const& fixedArgument) noexcept
    {
        KernelWord term = KernelOne;
        KernelWord partialSum = KernelOne;
        for (std::uint32_t order = 1;; ++order)
        {
            std::optional<KernelWord> const product = mul_checked_or_none(term, fixedArgument);
            if (!product)
                return std::nullopt;
            term = divmod_small(shift_right(*product, KernelFractionBits), order).quotient;
            if (term.is_zero())
                return partialSum;
            if (order == TaylorTermLimit)
                return std::nullopt;
            std::optional<KernelWord> const added = add_checked_or_none(partialSum, term);
            if (!added)
                return std::nullopt;
            partialSum = *added;
        }
    }

    /// |ln(a/b)| enclosed in units of 2^-128, and whether ln(a/b) is negative (a < b).
    struct LogarithmMagnitude
    {
        /// The lower end of |ln(a/b)| * 2^128.
        KernelWord lower;
        /// The upper end.
        KernelWord upper;
        /// Whether the logarithm is below zero.
        bool negative;
    };

    /// The enclosure of |ln(@p positive)| -- see the file comment. @pre @p positive > 0 and != 1.
    [[nodiscard]] constexpr std::optional<LogarithmMagnitude> natural_log_magnitude(Rational positive) noexcept
    {
        auto larger = static_cast<std::uint64_t>(positive.numerator());
        auto smaller = static_cast<std::uint64_t>(positive.denominator());
        bool const negative = larger < smaller;
        if (negative)
            std::swap(larger, smaller);
        // B = smaller * 2^doublings <= larger < 2B. Both are below 2^63, so the shift stays in 64 bits.
        int doublings = static_cast<int>(std::bit_width(larger)) - static_cast<int>(std::bit_width(smaller));
        if ((smaller << doublings) > larger)
            --doublings;
        std::uint64_t const base = smaller << doublings;
        // z = (a - B) / (a + B), and a + B < 2^64.
        std::optional<KernelWord> const series = atanh_series_lower(scaled_quotient(larger - base, larger + base).below);
        if (!series)
            return std::nullopt;
        auto const multiples = static_cast<std::uint32_t>(doublings);
        std::optional<KernelWord> const twiceSeries = add_checked_or_none(*series, *series);
        std::optional<KernelWord> const ln2Below = mul_small_checked_or_none(Ln2Lower, multiples);
        std::optional<KernelWord> const ln2Above = mul_small_checked_or_none(Ln2Upper, multiples);
        std::optional<KernelWord> const slackedSeries = add_small_checked_or_none(*series, AtanhSlack);
        std::optional<KernelWord> const twiceSlacked = slackedSeries ? add_checked_or_none(*slackedSeries, *slackedSeries) : std::nullopt;
        if (!twiceSeries || !ln2Below || !ln2Above || !twiceSlacked)
            return std::nullopt;
        std::optional<KernelWord> const lowerEnd = add_checked_or_none(*ln2Below, *twiceSeries);
        std::optional<KernelWord> const upperEnd = add_checked_or_none(*ln2Above, *twiceSlacked);
        if (!lowerEnd || !upperEnd)
            return std::nullopt;
        return LogarithmMagnitude { *lowerEnd, *upperEnd, negative };
    }

    /// An enclosure from the magnitudes of its ends, over 2^128, and the sign.
    [[nodiscard]] constexpr Enclosure signed_enclosure(KernelWord const& nearer, KernelWord const& farther, bool negative) noexcept
    {
        if (negative)
            return { { true, farther, KernelOne }, { true, nearer, KernelOne } };
        return { { false, nearer, KernelOne }, { false, farther, KernelOne } };
    }

    /// ln(@p positive), enclosed. @pre @p positive > 0 and != 1.
    [[nodiscard]] constexpr std::optional<Enclosure> natural_log_enclosure(Rational positive) noexcept
    {
        std::optional<LogarithmMagnitude> const natural = natural_log_magnitude(positive);
        if (!natural)
            return std::nullopt;
        return signed_enclosure(natural->lower, natural->upper, natural->negative);
    }

    /// log10(@p positive) = ln(@p positive) * log10(e), enclosed. @pre @p positive > 0, not a power of ten.
    [[nodiscard]] constexpr std::optional<Enclosure> decimal_log_enclosure(Rational positive) noexcept
    {
        std::optional<LogarithmMagnitude> const natural = natural_log_magnitude(positive);
        if (!natural)
            return std::nullopt;
        std::optional<KernelWord> const lowerProduct = mul_checked_or_none(natural->lower, Log10eLower);
        std::optional<KernelWord> const upperProduct = mul_checked_or_none(natural->upper, Log10eUpper);
        if (!lowerProduct || !upperProduct)
            return std::nullopt;
        std::optional<KernelWord> const farther = add_small_checked_or_none(shift_right(*upperProduct, KernelFractionBits), 1U);
        if (!farther)
            return std::nullopt;
        return signed_enclosure(shift_right(*lowerProduct, KernelFractionBits), *farther, natural->negative);
    }

    /// exp(@p argument), enclosed -- see the file comment. @pre @p argument != 0 and -43 <= @p argument <= 44.
    [[nodiscard]] constexpr std::optional<Enclosure> exponential_enclosure(Rational argument) noexcept
    {
        bool const negative = argument.sign() < 0;
        ScaledQuotient const scaled =
            scaled_quotient(magnitude(argument.numerator()), static_cast<std::uint64_t>(argument.denominator()));
        std::optional<KernelWord> remainderBelow;
        std::uint32_t shifts = 0;
        if (!negative)
        {
            // The largest k in [0, 63] with k (L + 1) <= X: 64 ln 2 > 44 >= x.
            std::uint32_t below = 0;
            std::uint32_t above = 64;
            while (below + 1 < above)
            {
                std::uint32_t const middle = (below + above) / 2;
                std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper, middle);
                if (!multiple)
                    return std::nullopt;
                if (*multiple <= scaled.below)
                    below = middle;
                else
                    above = middle;
            }
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper, below);
            remainderBelow = multiple ? sub_checked_or_none(scaled.below, *multiple) : std::nullopt;
            shifts = below;
        }
        else
        {
            // The smallest m in [1, 63] with m L >= X' (X rounded up): 63 ln 2 > 43 >= |x|.
            std::optional<KernelWord> const roundedUp = scaled.exact ? std::optional<KernelWord> { scaled.below }
                                                                     : add_small_checked_or_none(scaled.below, 1U);
            if (!roundedUp)
                return std::nullopt;
            std::uint32_t below = 0;
            std::uint32_t above = 63;
            while (below + 1 < above)
            {
                std::uint32_t const middle = (below + above) / 2;
                std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower, middle);
                if (!multiple)
                    return std::nullopt;
                if (*multiple >= *roundedUp)
                    above = middle;
                else
                    below = middle;
            }
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower, above);
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
            return Enclosure { { false, *nearer, KernelOne }, { false, *farther, KernelOne } };
        }
        std::optional<KernelWord> const halved = shift_left_checked_or_none(KernelOne, shifts);
        if (!halved)
            return std::nullopt;
        return Enclosure { { false, *series, *halved }, { false, *slacked, *halved } };
    }
} // namespace formula::detail
```

  Names checked against the consumer-globals list: `remaining`, `highBits`, `lowBits`, `carriedOut`,
  `whole`, `squared`, `oddPower`, `partialSum`, `added`, `product`, `order`, `larger`, `smaller`,
  `doublings`, `base`, `series`, `multiples`, `below`, `above`, `middle`, `multiple`, `shifts`, `nearer`,
  `farther`, `halved`, `slacked`, `roundedUp`, `remainderBelow` — none is on it. Add the header to the
  `FILE_SET` (root `CMakeLists.txt:44-50`, with the other detail headers).

- [ ] **Step 6: Run and pass.** `CL -Filter "transcendental kernel"` → `ALL OK` (6 cases); then
  `GCC14 "transcendental kernel"`. A mismatch in the table test prints the row, places and mode: compare
  against Finding 5 before touching a slack (the slacks come from the derivation, not from the table).

- [ ] **Step 7: Measure one kernel call per case on cl.** In the scratch directory, `probe.cpp`:

```cpp
#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
constexpr bool probe()
{
    using formula::Rational;
#if PROBE_CASE == 1
    return formula::detail::natural_log_enclosure(Rational { 3 }).has_value();
#elif PROBE_CASE == 2
    return formula::detail::natural_log_enclosure(Rational { (1LL << 62) - 1, 1LL << 61 }).has_value();
#elif PROBE_CASE == 3
    return formula::detail::decimal_log_enclosure(Rational { 7 }).has_value();
#elif PROBE_CASE == 4
    return formula::detail::exponential_enclosure(Rational { 1 }).has_value();
#elif PROBE_CASE == 5
    return formula::detail::exponential_enclosure(Rational { -43 }).has_value();
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

  For each case, bisect the smallest n for which
  `cl /nologo /std:c++latest /permissive- /c /Zs /I<worktree>\include /DPROBE_CASE=<c> /constexpr:steps<n> probe.cpp`
  compiles. Record the six counts and the cl version in the header comment ("One enclosure costs, on cl
  19.51.36257, between … and … constant-evaluation steps, and a whole rounding about …; cl's default
  budget measured about 1 049 000") and in the report. **Expected** from stand-ins built from this plan's
  and the foundation plan's code: cases 1–5, 3 000–31 000 (ln with z near 1/3 the most); case 6, about
  240 600 (298 000 on clang-cl 22.1.3), nearly all of it `decide_rounding`: per end, a gcd and three
  binary `divmod`s over 384 bits. Whole roundings measured while planning ran from 240 600 (log10 2 at 3
  places) to 284 500 (exp 1 at 18 places) on cl. **The target (Step 2) is at most 100 000 per kernel
  call**: report every figure; an enclosure above 100 000 (and only such a case) stops the task, reported
  to the controller with the counts.

  **Compile time and run time.** Every check in this plan that runs the kernel, or `decide_rounding` on
  its enclosure, is a run-time `CHECK`, except the one deliberate compile-time smoke test, "transcendental
  kernel: the kernel answers at compile time" (case 6). The special points and the early exits, which
  never reach the kernel, stay `STATIC_REQUIRE`, and so do the stored constants' checks (about 15 300 cl
  steps each, no kernel). The smoke test is measured by every per-task build on cl-debug and g++-14 (it
  compiles, or the build fails); if it exceeds any toolchain's default at the final all-preset run, the
  controller moves it to run time and takes the compile-time claim out of the documentation.

- [ ] **Step 8: Full verification.** `CL` and `GCC14`, no filter.

- [ ] **Step 9: Commit.**
  ```
  feat(transcendental): an integer kernel enclosing logarithms and exponentials

  ln, log10 and exp of a rational computed in 384-bit fixed point with 128 fraction bits, every
  step truncating, so each answer is an enclosure whose width the header derives: under 2^-120
  absolute for the logarithms and 2^-119 relative for the exponential. No floating point: the same
  inputs give the same bits on every compiler, at compile time and at run time. The stored ln 2 and
  log10(e) are checked against their published digits and re-derived from the kernel's own series,
  and 39 inputs against a 40-digit reference decide as the reference does in every mode.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 5: Rounded nodes — `rounded_ln`, `rounded_log10`, `rounded_exp`

**Counts:** +14 ctest cases (+10 `rounded_transcendental_tests`, +1 `calculation_tests`, +3 negatives; the
overlay assertions join an existing case); consumer-globals probe +1 check.

**Files:**
- Create `include/formula-cpp/rounded_transcendental.hpp`, `test/rounded_transcendental_tests.cpp`, three
  `test/negative/rounded_transcendental_*.cpp`.
- Modify `include/formula-cpp/precision.hpp` (include beside `rounded_root.hpp`, `:78`; `LevelChildren`
  after the `RoundedRootNode` entry, `:506-510`), `include/formula-cpp/overlay.hpp` (include `:154`;
  `ConstantRewrite` after `:1645-1666`; `SubstitutedIn` after `:2360-2363`),
  `include/formula-cpp/formula.hpp` (after `rounded_root.hpp`, `:48`), root `CMakeLists.txt` (`FILE_SET`,
  after `rounded_root.hpp`, `:88`), `test/CMakeLists.txt` (sources; negatives after `rounded_sqrt_rep_double`,
  `:938-940`), `test/consumer_globals_tests.cpp` (include after `rounded_root.hpp`, `:187`; a probe check),
  `test/consumer_globals_run_tests.cpp:16`, `test/overlay_tests.cpp`, `test/calculation_tests.cpp`,
  `CHANGELOG.md`.

**Interfaces:**
- Consumes: `Transcendental`, `detail::RequireDimensionlessArgument`, `RepFunctions<Rational>`,
  `detail::transcendental_of` (Task 1); the kernel's `*_enclosure` (Task 4); `decide_rounding`
  (foundation); `checked_round` (`rounding.hpp:255`); `RepRounding<Rep>::round_in` (`rounding_node.hpp:126-196`).
- Produces (namespace `formula`):
  ```cpp
  template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
  struct RoundedTranscendentalNode;   // operand; static function, places, mode, dimension = dim::Scalar, refused; no unit
  template <DecimalPlaces Places, RoundingMode Mode, Node Operand> constexpr auto rounded_ln(Operand) noexcept;
  template <DecimalPlaces Places, RoundingMode Mode, Node Operand> constexpr auto rounded_log10(Operand) noexcept;
  template <DecimalPlaces Places, RoundingMode Mode, Node Operand> constexpr auto rounded_exp(Operand) noexcept;
  template <typename Rep = Rational, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand,
            typename Env, typename Sink = NullSink>
  constexpr Evaluated<Rep> checked_evaluate_si(RoundedTranscendentalNode<F, Places, Mode, Operand> const&, Env const&, Sink = {}) noexcept;
  ```
  and in `formula::detail`: `constexpr std::expected<Rational, ArithmeticError> rounded_transcendental(Transcendental,
  Rational argument, DecimalPlaces, RoundingMode) noexcept`; the walks' three entries.

**The decision, in order** (`detail::rounded_transcendental`): (1) a logarithm of zero or below →
`DomainError`; (2) places outside −18…18 → `Overflow`, as `checked_round`; (3) a special point (ln 1,
log10 10^k, exp 0 — the only values that can tie) → `checked_round` of the exact value; (4) exp of x > 44
→ `Overflow` — exp(44) > 1.28·10^19, and every rounding of it, down to whole 10^18s under `Floor`, stays
above `Rational::Int`'s 9.22·10^18; (5) exp of x < −43 → below exp(−43) < 2.12·10^-19 < 10^-18/4, a quarter
of the last kept unit at any places this accepts: 0 in the nearest, toward-zero and `Floor` modes, one
unit 10^-places under `Ceiling` and `AwayFromZero` (exp is positive); (6) the kernel's enclosure →
`decide_rounding`, whose `Overflow` covers a kept integer too large for `Rational::Int` and an undecided
rounding alike; an empty enclosure (a bound that did not hold) → `Overflow`.

- [ ] **Step 1: Write the failing tests.** `test/rounded_transcendental_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// Every expected value below was computed while planning with Python 3.13's decimal module at 150
// significant digits, and agrees with the kernel's own reference table (transcendental_tests.cpp).
//
// A value that reaches the kernel is checked at run time: one rounding through it costs about a quarter
// of a compiler's default constant-evaluation budget. The special points and the early exits, which
// never reach it, are checked at compile time.
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <expected>
#include <optional>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using formula::var;

struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", unit::One>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", unit::One>
{
};
struct Share: formula::Quantity<Share, "p", "an invented share", unit::Percent>
{
};

/// @p node evaluated exactly with the ratio at @p ratioValue: its value, or its error.
template <typename N>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> at(N const& node, Rational ratioValue)
{
    formula::Evaluated<Rational> const evaluated =
        formula::checked_evaluate_si<Rational>(node, formula::environment(formula::Measured<Ratio> { ratioValue }));
    if (!evaluated.has_value())
        return std::unexpected { evaluated.error() };
    if (!evaluated->has_value())
        return std::unexpected { formula::ArithmeticError::NotFinite };   // absent: no fixture expects it
    return **evaluated;
}

template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> lnAt(Rational ratioValue)
{
    return at(formula::rounded_ln<Places, Mode>(var<Ratio>), ratioValue);
}
template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> log10At(Rational ratioValue)
{
    return at(formula::rounded_log10<Places, Mode>(var<Ratio>), ratioValue);
}
template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> expAt(Rational ratioValue)
{
    return at(formula::rounded_exp<Places, Mode>(var<Ratio>), ratioValue);
}

constexpr std::expected<Rational, formula::ArithmeticError> overflow { std::unexpected { formula::ArithmeticError::Overflow } };
constexpr std::expected<Rational, formula::ArithmeticError> domainError { std::unexpected { formula::ArithmeticError::DomainError } };
} // namespace

TEST_CASE("rounded_transcendental: ln 2 to 4 dp in every mode and of 1/2 with the directions paired the other way",
          "[rounded_transcendental]")
{
    // ln 2 = 0.693147...: the nearest and downward modes keep 0.6931, the upward ones 0.6932.
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfTowardZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 2 }) == Rational { 1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::AwayFromZero>(Rational { 2 }) == Rational { 1733, 2500 });
    // ln 1/2 = -0.693147...: Ceiling and TowardZero keep -0.6931, Floor and AwayFromZero -0.6932.
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 1, 2 }) == Rational { -1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::AwayFromZero>(Rational { 1, 2 }) == Rational { -1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    // A pure number, whatever the argument was.
    STATIC_REQUIRE(decltype(formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>))::dimension
                   == formula::dim::Scalar);
}

TEST_CASE("rounded_transcendental: log10 2 to 3 dp and exp 1 to 4 dp and exp -1 to 3 dp", "[rounded_transcendental]")
{
    // log10 2 = 0.30102...: 0.301, and 0.302 upward.
    CHECK(log10At<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { 2 }) == Rational { 301, 1000 });
    CHECK(log10At<DecimalPlaces { 3 }, RoundingMode::Ceiling>(Rational { 2 }) == Rational { 151, 500 });
    // exp 1 = 2.718281...: the nearest modes round up to 2.7183, where Floor and TowardZero keep 2.7182.
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(Rational { 1 }) == Rational { 27183, 10000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1 }) == Rational { 27183, 10000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 1 }) == Rational { 13591, 5000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 1 }) == Rational { 13591, 5000 });
    // exp -1 = 0.367879...: 0.368 = 46/125, and 0.367 downward.
    CHECK(expAt<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { -1 }) == Rational { 46, 125 });
    CHECK(expAt<DecimalPlaces { 3 }, RoundingMode::Floor>(Rational { -1 }) == Rational { 367, 1000 });
}

TEST_CASE("rounded_transcendental: a special point ties and the mode decides it", "[rounded_transcendental]")
{
    // log10 10^15 = 15, exactly halfway between 10 and 20 at -1 dp.
    constexpr Rational tenTo15 { 1'000'000'000'000'000 };
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(tenTo15) == Rational { 20 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(tenTo15) == Rational { 10 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfEven>(tenTo15) == Rational { 20 });
    // log10 10^5 = 5, halfway between 0 and 10: even is 0 this time.
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 100'000 }) == Rational { 10 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(Rational { 100'000 }) == Rational {});
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfEven>(Rational { 100'000 }) == Rational {});
    // log10 10^-15 = -15, written as one over a power of ten.
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 1, 1'000'000'000'000'000 })
                   == Rational { -20 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(Rational { 1, 1'000'000'000'000'000 })
                   == Rational { -10 });
    // ln 1 and exp 0 are exact too.
    STATIC_REQUIRE(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 1 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { -1 }, RoundingMode::HalfEven>(Rational {}) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { -1 }, RoundingMode::Ceiling>(Rational {}) == Rational { 10 });
}

TEST_CASE("rounded_transcendental: an exponential too large to hold is Overflow", "[rounded_transcendental]")
{
    // exp 50 = 5.18 * 10^21: past the early bound.
    STATIC_REQUIRE(expAt<DecimalPlaces { 6 }, RoundingMode::HalfAwayFromZero>(Rational { 50 }) == overflow);
    // exp 43.7 = 9.52 * 10^18: through the kernel. Floor to whole 10^18s keeps 9 * 10^18, which fits;
    // the nearest modes give 10^19, which does not.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 437, 10 }) == Rational { 9'000'000'000'000'000'000 });
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::HalfAwayFromZero>(Rational { 437, 10 }) == overflow);
    // exp 44 = 1.29 * 10^19 fits at no places.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 44 }) == overflow);
    // exp 43 = 4727839468229346561.47...: whole, it fits.
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 43 }) == Rational { 4'727'839'468'229'346'561 });
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::Ceiling>(Rational { 43 }) == Rational { 4'727'839'468'229'346'562 });
}

TEST_CASE("rounded_transcendental: a tiny exponential is zero or one unit by mode", "[rounded_transcendental]")
{
    // exp -50 = 1.9 * 10^-22, below a quarter of any last kept unit: 0, except one unit upward.
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::Floor>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::TowardZero>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::Ceiling>(Rational { -50 }) == Rational { 1, 1000 });
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::AwayFromZero>(Rational { -50 }) == Rational { 1, 1000 });
    STATIC_REQUIRE(expAt<DecimalPlaces { -2 }, RoundingMode::Ceiling>(Rational { -50 }) == Rational { 100 });
    STATIC_REQUIRE(expAt<DecimalPlaces { -2 }, RoundingMode::HalfEven>(Rational { -50 }) == Rational {});
    // exp -43 = 2.1 * 10^-19, through the kernel at its lowest power of two, 2^-63.
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(Rational { -43 }) == Rational {});
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { -43 }) == Rational { 1, 1'000'000'000'000'000'000 });
}

TEST_CASE("rounded_transcendental: absence and failures come first and in order", "[rounded_transcendental]")
{
    constexpr auto nothingMeasured = formula::environment(formula::Measured<Ratio>::absent());
    STATIC_REQUIRE(!formula::checked_evaluate_si<Rational>(
                        formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>), nothingMeasured)
                        ->has_value());
    STATIC_REQUIRE(!formula::checked_evaluate_si<Rational>(
                        formula::rounded_exp<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>), nothingMeasured)
                        ->has_value());
    // The argument's own failure, unchanged.
    constexpr auto dividedByZero = formula::environment(formula::Measured<Ratio> { Rational { 1 } },
                                                        formula::Measured<Divisor> { Rational {} });
    STATIC_REQUIRE(formula::checked_evaluate_si<Rational>(
                       formula::rounded_log10<DecimalPlaces { 2 }, RoundingMode::HalfEven>(var<Ratio> / var<Divisor>), dividedByZero)
                       .error()
                   == formula::ArithmeticError::DivisionByZero);
    // A logarithm of zero or below; before places out of range.
    STATIC_REQUIRE(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational {}) == domainError);
    STATIC_REQUIRE(log10At<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { -1 }) == domainError);
    STATIC_REQUIRE(lnAt<DecimalPlaces { 19 }, RoundingMode::HalfEven>(Rational { -1 }) == domainError);
    STATIC_REQUIRE(lnAt<DecimalPlaces { 19 }, RoundingMode::HalfEven>(Rational { 2 }) == overflow);
    STATIC_REQUIRE(lnAt<DecimalPlaces { -19 }, RoundingMode::HalfEven>(Rational { 1 }) == overflow);
    // Places out of range before the tiny rule.
    STATIC_REQUIRE(expAt<DecimalPlaces { 19 }, RoundingMode::Ceiling>(Rational { -50 }) == overflow);
}

TEST_CASE("rounded_transcendental: a percentage is read in the coherent unit", "[rounded_transcendental]")
{
    // 5 % is 0.05: ln 0.05 = -2.99573..., -2.9957; ln 5 would give 1.6094. Through the kernel, so at run time.
    constexpr auto fivePercent = formula::environment(formula::Measured<Share> { Rational { 5 } });
    CHECK(**formula::checked_evaluate_si<Rational>(
              formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(var<Share>), fivePercent)
          == Rational { -29957, 10000 });
    // 1000 % is 10, a special point: exactly 1, at compile time.
    constexpr auto tenfold = formula::environment(formula::Measured<Share> { Rational { 1000 } });
    STATIC_REQUIRE(**formula::checked_evaluate_si<Rational>(
                       formula::rounded_log10<DecimalPlaces { 0 }, RoundingMode::HalfEven>(var<Share>), tenfold)
                   == Rational { 1 });
}

TEST_CASE("rounded_transcendental: a value a double cannot tell from a boundary rounds right", "[rounded_transcendental]")
{
    // log10(10^18 - 1) = 17.99999999999999999956...: 4.3 * 10^-19 below 18. The doubles next to 18 are
    // 3.6 * 10^-15 apart, so a double computation can answer no better than 18, and 18.00 under Floor.
    CHECK(log10At<DecimalPlaces { 2 }, RoundingMode::Floor>(Rational { 999'999'999'999'999'999 }) == Rational { 1799, 100 });
    CHECK(log10At<DecimalPlaces { 17 }, RoundingMode::Floor>(Rational { 999'999'999'999'999'999 })
          == Rational::from_decimal(1'799'999'999'999'999'999, -17));
    CHECK(log10At<DecimalPlaces { 17 }, RoundingMode::HalfEven>(Rational { 999'999'999'999'999'999 }) == Rational { 18 });
}

TEST_CASE("rounded_transcendental: a rounding the kernel cannot decide is Overflow", "[rounded_transcendental]")
{
    // 1.25 * 10^-37 below a tie at 18 places: the half modes cannot be decided (the true rounding is 0).
    constexpr Rational nearTie { 2'000'000'000'000'000'001, 2'000'000'000'000'000'000 };
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(nearTie) == overflow);
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(nearTie) == Rational {});
}

TEST_CASE("rounded_transcendental: rounded of a plain logarithm stays an exact logarithm that fails", "[rounded_transcendental]")
{
    // rounded<>(ln(x)) does not become rounded_ln: the plain node fails before the rounding sees a value.
    STATIC_REQUIRE(at(formula::rounded<unit::One, DecimalPlaces { 4 }, RoundingMode::HalfEven>(formula::ln(var<Ratio>)), Rational { 2 })
                   == std::expected<Rational, formula::ArithmeticError> { std::unexpected { formula::ArithmeticError::Inexact } });
}
```

  Add to `test/overlay_tests.cpp`, after the Task 2 lines:

```cpp
    // The rounded forms: ln 4 = 1.386294... is 1.386 at 3 dp, log10 4 = 0.60205... is 0.602 -- both
    // through the logarithm kernel, so checked at run time -- and exp(4 - 4) is exactly 1.
    CHECK(withRatioFixedAtFour(f::rounded_ln<f::DecimalPlaces { 3 }, f::RoundingMode::HalfAwayFromZero>(r))
          == Rational { 693, 500 });
    CHECK(withRatioFixedAtFour(f::rounded_log10<f::DecimalPlaces { 3 }, f::RoundingMode::HalfAwayFromZero>(r))
          == Rational { 301, 500 });
    STATIC_REQUIRE(withRatioFixedAtFour(f::rounded_exp<f::DecimalPlaces { 3 }, f::RoundingMode::HalfAwayFromZero>(
                       r - f::number(Rational { 4 })))
                   == Rational { 1 });
```

  Add to `test/calculation_tests.cpp`, after Task 2's case:

```cpp
TEST_CASE("a worksheet calculates a rounded logarithm from the values it reads", "[calculation][worksheet]")
{
    constexpr auto logShare = formula::calculation(formula::define<Share>(
        formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor> / var<Other>)));
    STATIC_REQUIRE(std::is_same_v<decltype(formula::define<Share>(
                                      formula::rounded_exp<formula::DecimalPlaces { 2 }, formula::RoundingMode::Floor>(var<Other>)))::reads,
                                  QuantityList<Other>>);
    auto sheet = formula::worksheet(
        logShare, formula::environment(formula::Measured<Factor> { rat(2) }, formula::Measured<Other> { rat(1) }));
    CHECK(sheet.calculate<Share>().measurement() == formula::Measured<Share> { rat(6931, 10000) });
}
```

  Add `rounded_transcendental_tests.cpp` to the test sources (after `transcendental_tests.cpp`).

- [ ] **Step 2: Run and watch it fail.** `CL -Filter "rounded_transcendental|overlay|calculation"` →
  `BUILD FAILED` (`'rounded_ln': is not a member of 'formula'`).

- [ ] **Step 3: Implement `include/formula-cpp/rounded_transcendental.hpp`.** File comment (after the SPDX
  line and `#pragma once`):

```cpp
/// @file
/// A logarithm or an exponential rounded exactly to declared decimal places.
///
/// Under `Rational`, `ln(x)`, `log10(x)` and `exp(x)` answer only where the value is rational -- ln 1,
/// log10 10^k, exp 0 -- and are `ArithmeticError::Inexact` elsewhere, as `sqrt(2)` is. A method that
/// reports a logarithm "to 0.0001" states an exact number all the same: the decimal the true, irrational
/// value rounds to. This header computes that number with integer arithmetic
/// (`detail/transcendental.hpp`), never holding an approximation of the value.
///
/// `rounded_ln<Places, Mode>(x)` is one fused node rather than a `RoundNode` around a
/// `TranscendentalNode`, for the reasons `rounded_root.hpp` gives for `rounded_sqrt`: its trace step shows
/// the argument and the rounded result, both exact, and `rounded<...>(ln(x))` keeps meaning an exact
/// logarithm, which fails where there is none.
///
/// There is no unit: the argument and the result are pure numbers, rounded as they are.
```

  Includes: `dimension.hpp`, `error.hpp`, `evaluate.hpp`, `expression.hpp`, `function.hpp`, `rational.hpp`,
  `rounding.hpp`, `rounding_node.hpp`, `sink.hpp`, `unit.hpp`, `detail/transcendental.hpp`,
  `detail/wide_rounding.hpp`; `<expected>`, `<optional>`, `<type_traits>`. Code:

```cpp
namespace formula
{

namespace detail
{
    /// @p function of @p argument where it is rational, through `RepFunctions<Rational>`: its value at a
    /// special point, and `Inexact` elsewhere (or `DomainError`, which the caller has already answered).
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> exact_transcendental(Transcendental function,
                                                                                         Rational argument) noexcept
    {
        switch (function)
        {
            case Transcendental::NaturalLogarithm:
                return RepFunctions<Rational>::natural_log(argument);
            case Transcendental::DecimalLogarithm:
                return RepFunctions<Rational>::decimal_log(argument);
            case Transcendental::Exponential:
                return RepFunctions<Rational>::exponential(argument);
        }
        return std::unexpected { ArithmeticError::DomainError };
    }

    /// @p function of @p argument, rounded to @p places decimal places under @p roundingMode -- the correctly
    /// rounded decimal of the true value, rational or not. The decision, in order: a logarithm of zero or
    /// below is `DomainError`; places outside -18...18 are `Overflow`, as for `checked_round`; a special
    /// point -- the only values that can tie -- goes to `checked_round`; the exponential of more than 44
    /// is `Overflow` (every rounding of exp 44 exceeds `Rational::Int`), and of less than -43 is below a
    /// quarter of the last kept unit at any places accepted, so 0, or one unit under `Ceiling` and
    /// `AwayFromZero`; everything else is the kernel's enclosure, rounded by `decide_rounding`, which
    /// answers `Overflow` when the kept integer does not fit and when the two ends round differently.
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_transcendental(Transcendental function,
                                                                                           Rational argument,
                                                                                           DecimalPlaces places,
                                                                                           RoundingMode roundingMode) noexcept
    {
        if (function != Transcendental::Exponential && argument.sign() <= 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (places.value > 18 || places.value < -18)
            return std::unexpected { ArithmeticError::Overflow };

        std::expected<Rational, ArithmeticError> const exact = exact_transcendental(function, argument);
        if (exact.has_value())
            return checked_round(*exact, places, roundingMode);
        if (exact.error() != ArithmeticError::Inexact)
            return std::unexpected { exact.error() };

        std::optional<Enclosure> enclosure;
        switch (function)
        {
            case Transcendental::NaturalLogarithm:
                enclosure = natural_log_enclosure(argument);
                break;
            case Transcendental::DecimalLogarithm:
                enclosure = decimal_log_enclosure(argument);
                break;
            case Transcendental::Exponential:
                if (argument > Rational { 44 })
                    return std::unexpected { ArithmeticError::Overflow };
                if (argument < Rational { -43 })
                {
                    if (roundingMode == RoundingMode::Ceiling || roundingMode == RoundingMode::AwayFromZero)
                        return Rational::from_decimal(1, -places.value);
                    return Rational {};
                }
                enclosure = exponential_enclosure(argument);
                break;
        }
        if (!enclosure.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        return decide_rounding(enclosure->lower, enclosure->upper, places, roundingMode);
    }
} // namespace detail

/// @p F -- ln, log10 or exp -- of @p Operand, a dimensionless expression, rounded to @p Places decimal
/// places under @p Mode: exactly, whether the value is rational or not.
template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
struct RoundedTranscendentalNode: NodeBase
{
    static_assert(detail::RequireDimensionlessArgument<Operand>::value);

    /// The argument: a bare number.
    ///
    /// Named `operand`, as `ConstantRewriteOperand` (`overlay.hpp`) reads it, and deliberately no `{}`
    /// default member initialiser -- see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// Which function this is.
    static constexpr Transcendental function = F;
    /// How many decimal places to keep; the trace reads it by name, as it reads `RoundNode`'s.
    static constexpr DecimalPlaces places = Places;
    /// Which way to go. The three half modes differ only on a tie, and only a special point -- ln 1,
    /// log10 10^k, exp 0 -- can tie. No `unit`: the value is a pure number, and the trace shows it in the
    /// coherent unit.
    static constexpr RoundingMode mode = Mode;
    /// A pure number.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

/// The natural logarithm of `operand`, a dimensionless expression, rounded exactly to `Places` decimal
/// places: `rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Count> / var<InitialCount>)`.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_ln(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::NaturalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// The decimal logarithm of `operand`, rounded exactly to `Places` decimal places.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_log10(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::DecimalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// The exponential of `operand`, rounded exactly to `Places` decimal places.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_exp(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::Exponential, Places, Mode, Operand> { {}, operand };
}

/// Evaluates the argument, then rounds `F` of it. Under `Rational` this is
/// `detail::rounded_transcendental`, exact. Under any other representation it is that representation's
/// function (`RepFunctions<Rep>`) followed by its own rounding (`RepRounding<Rep>::round_in`), which for
/// `double` refuses to compile, as it does for every rounding node.
template <typename Rep = Rational,
          Transcendental F,
          DecimalPlaces Places,
          RoundingMode Mode,
          Node Operand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundedTranscendentalNode<F, Places, Mode, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedOperand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!evaluatedOperand.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedOperand.error());
    }
    if (!evaluatedOperand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> roundedValue = std::unexpected { ArithmeticError::DomainError };
    if constexpr (std::is_same_v<Rep, Rational>)
        roundedValue = detail::rounded_transcendental(F, **evaluatedOperand, Places, Mode);
    else
    {
        std::expected<Rep, ArithmeticError> const unrounded = detail::transcendental_of<F, Rep>(**evaluatedOperand);
        roundedValue = unrounded.has_value() ? RepRounding<Rep>::round_in(*unrounded, unit::One, Places, Mode)
                                             : std::expected<Rep, ArithmeticError> { std::unexpected { unrounded.error() } };
    }
    Evaluated<Rep> const evaluated = roundedValue.has_value() ? detail::present<Rep>(*roundedValue)
                                                              : Evaluated<Rep> { std::unexpected { roundedValue.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

} // namespace formula
```

  Walks: `precision.hpp` (and `#include <formula-cpp/rounded_transcendental.hpp>` beside `rounded_root.hpp`):

```cpp
    template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct LevelChildren<RoundedTranscendentalNode<F, Places, Mode, Operand>>: LevelParent<Operand>
    {
    };
```

  `overlay.hpp` (and the include):

```cpp
    template <typename Sub, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Sub, RoundedTranscendentalNode<F, Places, Mode, Operand>>:
        ConstantRewriteOperand<Sub,
                               Operand,
                               RoundedTranscendentalNode<F, Places, Mode, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };
```

```cpp
    template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct SubstitutedIn<RoundedTranscendentalNode<F, Places, Mode, Operand>>: SubstitutedInOperand<Operand>
    {
    };
```

  Wiring: `#include <formula-cpp/rounded_transcendental.hpp>` in `formula.hpp` after `rounded_root.hpp`;
  `"${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/rounded_transcendental.hpp"` in the `FILE_SET` after
  `rounded_root.hpp`; the same include in `test/consumer_globals_tests.cpp` after `rounded_root.hpp`
  (`:187`).

- [ ] **Step 4: Run and pass.** `CL -Filter "rounded_transcendental|overlay|calculation"` → `ALL OK`; then
  `GCC14 "rounded_transcendental|overlay|calculation"`.

- [ ] **Step 5: Negative tests** (header comments as in Task 1; fixture `Height` in metres and
  `struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One> {};`):
  1. `rounded_transcendental_dimensioned.cpp` — `formula::rounded_ln<formula::DecimalPlaces { 2 },
     formula::RoundingMode::HalfEven>(formula::var<Height>)`, its `dimension` read in `main`.
  2. `rounded_transcendental_no_factory_dimensioned.cpp` —
     `inline constexpr formula::RoundedTranscendentalNode<formula::Transcendental::DecimalLogarithm,
     formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven, decltype(height)> broken { {}, height };`
  3. `rounded_transcendental_rep_double.cpp` — a correct node evaluated in `double`:
     ```cpp
     int main()
     {
         constexpr auto logarithm =
             formula::rounded_exp<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(formula::var<Ratio>);
         auto const evaluated = formula::checked_evaluate_si<double>(
             logarithm, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }));
         return evaluated.has_value() ? 0 : 1;
     }
     ```

  Registration after `rounded_sqrt_rep_double` (`test/CMakeLists.txt:938-940`):

```cmake
# A rounded logarithm or exponential: the plain node's dimension check, once,
# through the factory and around it; and RepRounding<double>'s own refusal
# under double, as for every rounding node.
formula_add_negative_test(rounded_transcendental_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1
    REJECT "formula: this rounding node names a unit")
formula_add_negative_test(rounded_transcendental_no_factory_dimensioned
    "formula: the argument of this logarithm or exponential is not dimensionless" EXPECT_COUNT 1)
formula_add_negative_test(rounded_transcendental_rep_double
    "formula: a rounding node cannot be evaluated with Rep = double"
    REJECT "formula: the argument of this logarithm or exponential is not dimensionless")
```

  Protocol as in Task 1 (`-Filter "negative\.rounded_transcendental_"`). Deletion checks: (1, 2) delete the
  class-body `static_assert` → compile → fail; (3) replace the `round_in` call in the non-`Rational`
  branch with `unrounded` → compiles → fail. Restore by plain write.

- [ ] **Step 6: Consumer globals.** After Task 1's probe checks:

```cpp
    // The rounded forms through the kernel: ln 2 to 4 places is 0.6931, log10 2 to 3 is 0.301 and exp 1
    // to 4 is 2.7183, 3.7124 together.
    auto const roundedLogarithms = formula::evaluate<Factor>(
        formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor> * formula::Rational { 2 })
            + formula::rounded_log10<formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(
                var<Factor> * formula::Rational { 2 })
            + formula::rounded_exp<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor>),
        specimen);
    probe.checks.push_back(roundedLogarithms.is_value()
                           && roundedLogarithms.measurement().value() == formula::Rational { 37124, 10000 });
```

  Raise `test/consumer_globals_run_tests.cpp:16` by 1; add "each rounded exactly to declared places" to the
  header comment's list. (The kernel's functions are non-template, so including the header already puts
  their locals under cl's C4459 guard.)

- [ ] **Step 7: CHANGELOG** `### Added`: "- `rounded_ln<Places, Mode>(x)`, `rounded_log10<Places, Mode>(x)` and
  `rounded_exp<Places, Mode>(x)`, in the new header `rounded_transcendental.hpp` (included by
  `formula.hpp`), answer the logarithm or exponential of a dimensionless expression rounded to `Places`
  decimal places under `Mode`: the decimal the true value rounds to, computed with integer arithmetic, so
  the same inputs give the same digits on every compiler, at compile time and at run time. Only ln 1,
  log10 10^k and exp 0 can tie, and the mode breaks the tie as `checked_round` does. `Overflow` answers a
  result too large for a `Rational` at the declared places, places outside -18 to 18, and the rare rounding
  the computation cannot decide: one whose value lies within the computation's width -- under 2^-118,
  relative for the exponential -- of a rounding boundary.
  Evaluated with `Rep = double`, each is refused at compile time, as `rounded_sqrt` is.
  `rounded<...>(ln(x))` still means an exact logarithm, which fails where there is none."

- [ ] **Step 8: Full verification.** `CL` and `GCC14`, no filter.

- [ ] **Step 9: Commit.**
  ```
  feat(rounded_transcendental): logarithms and exponentials rounded exactly to declared places

  A method that reports ln, log10 or exp to so many decimals states an exact number: the decimal the
  irrational value rounds to. rounded_ln, rounded_log10 and rounded_exp compute it -- the special
  points through checked_round, which alone can see a tie, everything else through the integer
  kernel's enclosure -- and answer Overflow where the result does not fit or the rounding cannot be
  decided, never a guess. Overlays, precision limits and calculations see inside them.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 6: The rounded nodes on the page and in the trace

**Counts:** +4 ctest cases (+1 each `render_tests`, `document_tests`, `trace_tests`, `trace_render_tests`);
probe unchanged.

**Files:**
- Modify `include/formula-cpp/render.hpp` (include beside `rounded_root.hpp`, `:56`; `render_node` after
  the `RoundedRootNode` overload, `:1293-1315`), `include/formula-cpp/document.hpp` (include `:27`;
  declaration after `:430-431`; definition after `:842-848`), `include/formula-cpp/trace.hpp` (include
  `:33`; three enumerators after Task 3's; `StepKindOf` after `:1732-1736`),
  `include/formula-cpp/trace_render.hpp` (cases after `case StepKind::RoundedRoot:`, `:1063-1067`; the mode
  suffix, `:2935-2937`).
- Modify `test/render_tests.cpp`, `test/document_tests.cpp`, `test/trace_tests.cpp`,
  `test/trace_render_tests.cpp`, `test/vocabulary_tests.cpp`, `test/consumer_globals_tests.cpp`,
  `CHANGELOG.md`.

**Interfaces:**
- Consumes: `RoundedTranscendentalNode` (Task 5); `detail::transcendental_text<D>` (Task 3);
  `detail::transcendental_call` (Task 3).
- Produces: `StepKind::RoundedNaturalLogarithm`, `StepKind::RoundedDecimalLogarithm`,
  `StepKind::RoundedExponential`; `render_node(RoundedTranscendentalNode<F, Places, Mode, Operand> const&,
  V const&)`; `detail::collect(Walk<V>&, RoundedTranscendentalNode<…> const&)`;
  `detail::rounded_transcendental_expression(Transcendental, ShownStep const&)` in `trace_render.hpp`.

Readings (spec §6): Plain `round(ln(x), to 4 dp)`; Markdown ``round(ln(`x`), to 4 dp)``; LaTeX
`\operatorname{round}_{4}(\ln\left(x\right))`; the mode is not part of the formula text, as for
`rounded<>`. Trace: one fused step, `2. round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties away from zero]`
— the irrational value never appears; no `of <unit>` clause, since the node has no unit.

- [ ] **Step 1: Write the failing tests.** `test/render_tests.cpp`, after "render: a rounded square root
  reads as a rounding of a root, in every dialect" (`:245-265`):

```cpp
TEST_CASE("render: a rounded logarithm or exponential reads as a rounding of the call in every dialect", "[render]")
{
    constexpr auto logged = formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(var<Determinations>);
    CHECK(formula::render(logged) == "round(ln(n_d), to 4 dp)");
    CHECK(formula::render<Dialect::Markdown>(logged) == "round(ln(`n_d`), to 4 dp)");
    CHECK(formula::render<Dialect::LaTeX>(logged) == "\\operatorname{round}_{4}(\\ln\\left(n_d\\right))");
    constexpr auto decimal =
        formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::Floor>(var<WaterVolume> / var<CementVolume>);
    CHECK(formula::render(decimal) == "round(log10(V_w / V_c), to 2 dp)");
    CHECK(formula::render<Dialect::LaTeX>(decimal) == "\\operatorname{round}_{2}(\\log_{10}\\left(\\frac{V_w}{V_c}\\right))");
    constexpr auto grown =
        formula::rounded_exp<formula::DecimalPlaces { -1 }, formula::RoundingMode::Ceiling>(var<Determinations>);
    CHECK(formula::render(grown) == "round(exp(n_d), to -1 dp)");
    // The mode is the trace's, as for every rounding: two nodes differing only in it render alike.
    CHECK(formula::render(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(var<Determinations>))
          == formula::render(logged));
    // A call: an atom to what holds it.
    CHECK(formula::render(logged * rat(2)) == "round(ln(n_d), to 4 dp) * 2");
}
```

  In the Markdown every-kind test (`:200-226`):

```cpp
    CHECK(formula::render<Dialect::Markdown>(formula::rounded_exp<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
              var<Determinations>))
          == "round(exp(`n_d`), to 2 dp)"); // RoundedTranscendentalNode
```

  and in the inertness test, after Task 3's lines:

```cpp
    isInertInMarkdown(formula::render<Dialect::Markdown>(
        formula::rounded_log10<formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven>(var<Determinations>))); // RoundedTranscendentalNode
```

  `test/document_tests.cpp`, after Task 3's case:

```cpp
TEST_CASE("document: a rounded logarithm lists what its argument reads", "[document]")
{
    formula::Documentation const documentation = formula::document(
        formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(var<WaterVolume> / var<CementVolume>));
    CHECK(documentation.formula == "round(log10(V_w / V_c), to 2 dp)");
    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_c" });
}
```

  `test/trace_tests.cpp`, after Task 3's case:

```cpp
TEST_CASE("a rounded logarithm step records its places and mode and no unit of its own", "[trace]")
{
    constexpr auto node = formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>);
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(
        node, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }), sink);
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::RoundedNaturalLogarithm);
    CHECK(trace.steps[1].granularity == 4);
    CHECK(trace.steps[1].mode == formula::RoundingMode::HalfAwayFromZero);
    CHECK(trace.steps[1].unit == formula::coherent(formula::dim::Scalar));
    CHECK(trace.steps[1].value == formula::Rational { 6931, 10000 });
    REQUIRE(trace.steps[1].operands.size() == 1);
    CHECK(trace.steps[1].operands[0] == 0);
}
```

  `test/trace_render_tests.cpp`, after Task 3's case:

```cpp
TEST_CASE("a derivation writes a rounded logarithm or exponential as one step in its places and mode", "[trace-render]")
{
    auto const traceOf = [](auto const& node, formula::Rational ratioValue, formula::NumberStyle numberStyle) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(
            node, formula::environment(formula::Measured<Ratio> { ratioValue }), sink);
        return formula::render_trace(trace, { .maxSteps = 10, .numbers = numberStyle });
    };
    auto const fractions = formula::NumberStyle::fraction();
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 2 }, fractions)
          == "1. r = 2\n2. round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties away from zero]\n");
    CHECK(traceOf(formula::rounded_log10<formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven>(var<Ratio>),
                  formula::Rational { 2 }, fractions)
          == "1. r = 2\n2. round(log10(#1), to 3 dp) = 301/1000 [nearest, ties to even]\n");
    CHECK(traceOf(formula::rounded_exp<formula::DecimalPlaces { 3 }, formula::RoundingMode::Floor>(var<Ratio>),
                  formula::Rational { -1 }, fractions)
          == "1. r = -1\n2. round(exp(#1), to 3 dp) = 367/1000 [toward negative infinity]\n");
    // A failure reads like any step's, and still names the mode.
    CHECK(traceOf(formula::rounded_exp<formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 50 }, fractions)
          == "1. r = 50\n2. round(exp(#1), to 6 dp) = overflow in exact arithmetic [nearest, ties away from zero]\n");
    // In exact decimals the rounded value is a decimal like any other, with no approximation mark: it is exact.
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 2 }, formula::NumberStyle::exact_decimal())
          == "1. r = 2\n2. round(ln(#1), to 4 dp) = 0.6931 [nearest, ties away from zero]\n");
    // The display guide quotes this line: ln 0.05 = -2.99573..., negative, so the sign is written.
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(var<Ratio>),
                  formula::Rational { 1, 20 }, formula::NumberStyle::exact_decimal())
          == "1. r = 0.05\n2. round(ln(#1), to 4 dp) = -2.9957 [nearest, ties to even]\n");
}
```

  `test/vocabulary_tests.cpp` — in `everyNodeKind()`, after Task 3's factors:

```cpp
               // The rounded forms, each over the overlay's fixed factor, so the rewrite must reach inside:
               // round(ln(x_n / x_n)) = 0, whose exponential is 1, and round(log10(x_n / x_n * 10)) = 1.
               * formula::rounded_exp<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                   formula::rounded_ln<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                       var<EveryFixed> / var<EveryFixed>))
               * formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                   var<EveryFixed> / var<EveryFixed> * formula::constant<unit::One>(rat(10)))
```

  and extend both pinned cube strings to end
  `"... * exp(ln(E / R / (E / R))) * log10(10) * round(exp(round(ln(x_n / x_n), to 2 dp)), to 2 dp) "`
  `"* round(log10(x_n / x_n * 10), to 2 dp)");`. In the trace test pin, as in Task 3, the three rounded
  lines the run prints: `"<i>. round(ln(#<i-1>), to 2 dp) = 0 [nearest, ties away from zero]\n<i+1>. round(exp(#<i>), to 2 dp) = 1 [nearest, ties away from zero]\n"`
  and `"<j>. round(log10(#<j-1>), to 2 dp) = 1 [nearest, ties away from zero]\n"`, each reading `x_n` where
  its argument reads it (the fixed-factor line with its overlay citation).

  `test/consumer_globals_tests.cpp` — append to `everything`'s product, after Task 3's factors:
  `* formula::rounded_exp<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(formula::rounded_ln<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor>)) * formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor> * formula::Rational { 10 })`
  (1 and 1 at Factor = 1), so every surface the probe runs on `everything` — `explain`, `render_trace`,
  `render`, `document`, a definition — instantiates the rounded nodes under the consumer's globals; extend
  the comments' lists with "rounded to declared places".

- [ ] **Step 2: Run and watch it fail.** `CL -Filter "render|document|trace|vocabulary|consumer"` →
  `BUILD FAILED` (no `render_node` for `RoundedTranscendentalNode`; `'RoundedNaturalLogarithm': is not a
  member of 'formula::StepKind'`).

- [ ] **Step 3: Implement.** `render.hpp` (and the include):

```cpp
/// A rounded logarithm or exponential renders as what it computes, a rounding of the call:
/// `round(ln(x), to 4 dp)`, and in LaTeX `\operatorname{round}_{4}(\ln\left(x\right))`. No unit clause:
/// the node rounds a pure number. See `RoundNode`'s overload for why the mode is left out and why the
/// places are a comma-separated second argument; the call's own parentheses group it, so the primary
/// `PrecedenceOf`'s `Atom` is right.
template <Dialect D, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(RoundedTranscendentalNode<F, Places, Mode, Operand> const& node, V const& vocabulary)
{
    std::string const call = detail::transcendental_text<D>(F, render<D>(node.operand, vocabulary));
    std::string const placesText = std::to_string(Places.value);
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + "}(" + call + ")";
    else
        return "round(" + call + ", to " + placesText + " dp)";
}
```

  `document.hpp` (and the include): declaration
  `template <Vocabulary V, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand> void collect(Walk<V>& walk, RoundedTranscendentalNode<F, Places, Mode, Operand> const& node);`
  and definition

```cpp
    /// A rounded logarithm or exponential reads what its argument reads, as a rounding node reads what
    /// its operand does.
    template <Vocabulary V, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundedTranscendentalNode<F, Places, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
    }
```

  `trace.hpp` (and the include), appended after Task 3's enumerators:

```cpp
    /// A `RoundedTranscendentalNode` over the natural logarithm: `ln` of its one operand, rounded to a
    /// number of decimal places -- `Step::granularity` and `Step::mode`, as for `Round`; no unit of its own,
    /// so `Step::unit` is the coherent one. One step and not a `NaturalLogarithm` beneath a `Round`: the
    /// logarithm is irrational almost everywhere, so that step would have to show a number the evaluator
    /// never had. The operand's value is exact, and so is this step's.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `RoundedTranscendentalNode` and the factory
    /// `rounded_ln`, so nothing in namespace `formula` is spelt `RoundedNaturalLogarithm`.
    RoundedNaturalLogarithm,
    /// The same for the decimal logarithm (`rounded_log10`). Checked on GCC under `-Wshadow`, as
    /// `RoundedNaturalLogarithm` is.
    RoundedDecimalLogarithm,
    /// The same for the exponential (`rounded_exp`). Checked on GCC under `-Wshadow`, as
    /// `RoundedNaturalLogarithm` is.
    RoundedExponential,
```

```cpp
    template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct StepKindOf<RoundedTranscendentalNode<F, Places, Mode, Operand>>
    {
        static constexpr StepKind value = F == Transcendental::NaturalLogarithm   ? StepKind::RoundedNaturalLogarithm
                                          : F == Transcendental::DecimalLogarithm ? StepKind::RoundedDecimalLogarithm
                                                                                  : StepKind::RoundedExponential;
    };
```

  Update the comment beside `requires { N::mode; }` (`trace.hpp:3096-3098`), which says `RoundNode`,
  `RoundSignificantNode` and `RoundedRootNode` are the only kinds declaring one, to name
  `RoundedTranscendentalNode` too.

  `trace_render.hpp`, after `transcendental_call`:

```cpp
    /// `round(ln(#1), to 4 dp)`: `render()`'s spelling, one step with the function inside it, because the
    /// unrounded value was never a value. No unit clause: the node rounds a pure number.
    [[nodiscard]] inline std::string rounded_transcendental_expression(Transcendental function, ShownStep const& shownStep)
    {
        return "round(" + transcendental_call(function, sole_operand(shownStep)) + ", to "
               + std::to_string(shownStep.granularity) + " dp)";
    }
```

  in `step_expression`, after `case StepKind::RoundedRoot:`:

```cpp
            case StepKind::RoundedNaturalLogarithm:
                return rounded_transcendental_expression(Transcendental::NaturalLogarithm, shownStep);
            case StepKind::RoundedDecimalLogarithm:
                return rounded_transcendental_expression(Transcendental::DecimalLogarithm, shownStep);
            case StepKind::RoundedExponential:
                return rounded_transcendental_expression(Transcendental::Exponential, shownStep);
```

  and extend the mode suffix's condition (`:2935-2937`):

```cpp
        else if (recorded.kind == StepKind::Round || recorded.kind == StepKind::RoundSignificant
                 || recorded.kind == StepKind::RoundedRoot || recorded.kind == StepKind::RoundedNaturalLogarithm
                 || recorded.kind == StepKind::RoundedDecimalLogarithm || recorded.kind == StepKind::RoundedExponential)
            annotation = rounding_mode_suffix(recorded.mode);
```

- [ ] **Step 4: Run and pass.** `CL -Filter "render|document|trace|vocabulary|consumer"`, pin the vocabulary
  lines, rerun → `ALL OK`; `GCC14 "render|document|trace|vocabulary|consumer"`.

- [ ] **Step 5: CHANGELOG.** Extend Task 5's `### Added` entry: "They render as `round(ln(x), to 4 dp)`, in
  LaTeX `\operatorname{round}_{4}(\ln\left(x\right))`, and a trace writes each as one step whose value is
  the rounded decimal and whose bracket names the mode: `round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties
  away from zero]`." Extend Task 3's `### Changed` entry to "…gains `NaturalLogarithm`, `DecimalLogarithm`,
  `Exponential`, `RoundedNaturalLogarithm`, `RoundedDecimalLogarithm` and `RoundedExponential`…".

- [ ] **Step 6: Full verification.** `CL` and `GCC14`, no filter.

- [ ] **Step 7: Commit.**
  ```
  feat(trace_render): a rounded logarithm or exponential as one step, in every dialect

  The page states the rounding and the call, round(ln(x), to 4 dp); the trace writes one step whose
  value is the rounded decimal and whose bracket names the mode, so the irrational value appears
  nowhere. Three more StepKind enumerators are appended.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

## Task 7: The guide, the gallery, the census page and the CHANGELOG

**Counts:** +0 ctest cases (the docs checks already exist and must stay green); probe unchanged.

**Files:** `docs/display.md` (the foundation's *Values the exact layer cannot hold*),
`docs/expressions.md` (a section between "Powers, roots and pi", `:302-370`, and "Composing a
formula from other formulas", `:372`), `tools/gallery/main.cpp` (a formula after `massSpread`, `:368-375`,
and `write_formula` after `write_formula(out, massSpread);`, `:669`), `docs/gallery.md` (regenerated),
`docs/numeric-headroom.md` (one sentence after the paragraph beginning "The hooks sit in", `:95-104`),
`CHANGELOG.md`.

- [ ] **Step 1: Capture the diagnostic.** `GCC14 "negative\.transcendental_ln_dimensioned"` passes; to see
  the compiler's text run, in WSL, `ctest --test-dir out/build/gcc14-release -R negative.transcendental_ln_dimensioned -V`
  and copy the line containing `static assertion failed: formula: the argument of this logarithm`,
  **from `static assertion failed:` to the end of the line** (no file prefix, so only
  `hygiene.documented-diagnostic-text`, which checks the words, applies).

- [ ] **Step 2: Write the section** in `docs/expressions.md`. Its content, in this order (prose may be
  tightened, never a claim added that no test pins):

  - `## Logarithms and exponentials` — `formula::ln(operand)`, `formula::log10(operand)` and
    `formula::exp(operand)` take the natural logarithm, the decimal logarithm and the exponential of a
    formula. Nodes, as a power and a root are: the page shows `ln(x)`, a trace names the step, an overlay's
    constant reaches inside. Unlike a power they do nothing to a dimension, because they accept none.
  - `### The argument is a bare number` — ln of 2 m would be ln 2 + ln(m), a number that changes with the
    unit the length is read in, so a dimensioned argument does not compile; the captured line in a
    ```` ```text ```` block, introduced as "g++ 14 reports:". Two ways to a bare number: divide by a
    reference value of the same dimension, `formula::ln(var<Count> / var<InitialCount>)`; or, where a
    method states its formula over a bare number, read it in a named unit with `numeric_value_of`
    ([the traced escape hatch](rounding-and-conditionals.md#the-traced-escape-hatch-numeric_value_of)). A
    percentage is dimensionless and read in the coherent unit, as every value is: then quote, verbatim,
    the three lines of `test/function_tests.cpp`'s "function: a percentage is read in the coherent unit
    under a logarithm" from the `// 1000 % is the number 10` comment to its `STATIC_REQUIRE`, naming the
    test after the block, as the "Powers, roots and pi" section names its source.
  - `### Where the value is exact` — the table:

    | Function | Exact at | Elsewhere, in `Rational` |
    |---|---|---|
    | `ln(x)` | x = 1: 0 | `Inexact` |
    | `log10(x)` | x = 10^k, k from −18 to 18: k — `1000` and `1/1000` alike | `Inexact` |
    | `exp(x)` | x = 0: 1 | `Inexact`, however large |

    `Inexact` is the exact layer refusing to approximate, as it refuses `sqrt(2)`.
    `checked_evaluate_si<double>` answers with `std::log`, `std::log10` and `std::exp` — as a bounded fact
    pinned in "function: the double representation answers where the exact one refuses": ln 2 lies strictly
    between 0.69314718 and 0.69314719.
  - `### Declaring a precision` — `rounded_ln`, `rounded_log10`, `rounded_exp`, whose value is the decimal
    the true value rounds to, computed with integer arithmetic (link
    [values the exact layer cannot hold](display.md#values-the-exact-layer-cannot-hold)); quote, verbatim,
    the first three `CHECK` lines of "rounded_transcendental: ln 2 to 4 dp in every mode and of 1/2
    with the directions paired the other way" (run-time checks, as every check through the kernel is) and
    name the test; say nothing there about compile time. Choosing places: the method's own; at
    most 18, and the result must fit a `Rational` there — at 18 places a value below about 9.2, so
    `log10` of a count near 10^18 at 17. Only ln 1, log10 10^k and exp 0 can tie, and the mode breaks the
    tie as `rounded<>` does (log10 10^15 at −1 dp is 20, 10 or 20 under HalfAwayFromZero, HalfTowardZero,
    HalfEven). A rounding the computation cannot decide — a value within its width (under 2^-118, relative
    for `exp`) of a boundary — is `Overflow`, never a guess. `rounded<...>(ln(x))` is not `rounded_ln`: the
    plain logarithm fails before the rounding sees a value, as `rounded<...>(sqrt(x))` does.
  - `### What goes wrong` — the table:

    | Case | `ln`, `log10` | `exp` | `rounded_ln`, `rounded_log10`, `rounded_exp` |
    |---|---|---|---|
    | argument absent | absent | absent | absent |
    | argument failed | its error | its error | its error |
    | argument zero or below | `DomainError` | — | `DomainError` (the logarithms) |
    | not a point where the value is rational | `Inexact` | `Inexact` | the rounded decimal |
    | result too large at the declared places, or places outside −18…18 | — | — | `Overflow` |
    | rounding not decidable | — | — | `Overflow` |
    | `checked_evaluate_si<double>` | `std::log`, `std::log10`; `DomainError` for zero, below, NaN | `std::exp`; too large is `+inf` | does not compile, as `rounded_sqrt` |

  - `### How they read` — the table of spec §6 (Plain, Markdown, LaTeX for ln, log10, exp, rounded), with
    the reason for `\exp` over `e^{x}` in one sentence, and "`pow<2>(ln(x))` reads `ln(x)^2`".
  - `### A real power` — `pow(base, exponent)` with an exponent computed from data is not offered. A
    constant rational exponent is already `pow<P>(root<Q>(x))`, dimension-checked and exact wherever the
    answer is rational. A data exponent is `exp(y · ln x)`: exactly
    `rounded_exp<...>(y * rounded_ln<...>(x))`, both roundings declared and visible. A dimensioned base under
    a run-time exponent would have no compile-time dimension. A real power, if added, would be named
    `power`, not `pow`.

- [ ] **Step 2b: `docs/display.md`.** The foundation's section *Values the exact layer cannot hold* names
  no logarithm, since none existed (the foundation plan's Finding 10 leaves these sentences to this plan).
  Four edits, each in that section's own voice:
  - First paragraph: "A square root is irrational almost everywhere" becomes "A square root, a logarithm
    or an exponential is irrational almost everywhere".
  - Its list of declaring nodes, "`rounded_sqrt` for a root, `rounded_output` for an output of ...",
    becomes "`rounded_sqrt` for a root, `rounded_ln`, `rounded_log10` and `rounded_exp` for a
    [logarithm or an exponential](expressions.md#declaring-a-precision), `rounded_output` for an output of ...".
  - The bullet "**Its line says it was rounded, and carries no `≈`.**": after its slope example, before
    the full stop, add ", and the natural logarithm of a ratio of 0.05 reads
    `round(ln(#1), to 4 dp) = -2.9957 [nearest, ties to even]`". That line is Task 6's last
    `trace_render_tests` check, character for character; name no other.
  - The bullet "**Without a declared precision, these values are refused, never approximated.**": after
    "`sqrt(2)` is `Inexact`" insert ", and so is `ln(2)`: `ln(x)` answers only where the logarithm is
    rational -- ln 1, and `log10` of a power of ten -- and is `Inexact` elsewhere, as `exp(x)` is
    everywhere but 0." and start the slope's clause as its own sentence: "`opaque_output<"slope">(fit)` is
    the exact slope or `Overflow`."

  Read the section through once after: its bullet "**Only a rational value can tie**" now covers ln 1,
  log10 10^k and exp 0 without change, and nothing in it may claim more than Task 5's and Task 6's tests
  pin. The section still has no fenced block, so `docs.display-output` and `docs.display-snippets` are
  unaffected; `CL -Filter "docs.display"` and `GCC14 "docs.display"` show it.

- [ ] **Step 3: The gallery entry.** In `tools/gallery/main.cpp`, after `massSpread` (`:368-375`):

```cpp
using InitialCount = formula::Quantity<struct InitialCountTag, "N_0", "count before treatment", unit::One>;
using SurvivingCount = formula::Quantity<struct SurvivingCountTag, "N", "count after treatment", unit::One>;

constexpr auto logReduction = formula::documented(
    formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        var<InitialCount> / var<SurvivingCount>),
    { .title = "Logarithmic reduction",
      .reference = "Example Standard 8:2023",
      .section = "6.1",
      .text = "The decimal logarithm of the count before over the count after, rounded exactly to 0.01: "
              "the decimal the true logarithm rounds to, never a rounded floating-point one." });
```

  and `write_formula(out, logReduction);` after `write_formula(out, massSpread);`. Build with `GCC14`, then
  regenerate in WSL (LF line endings):
  `out/build/gcc14-release/tools/gallery/formula-cpp-gallery docs/gallery.md` from the worktree root.
  The new section reads `round(log10(N_0 / N), to 2 dp)` and
  `\operatorname{round}_{2}(\log_{10}\left(\frac{N_0}{N}\right))`. `git diff docs/gallery.md` shows only
  the new section. `CL -Filter "gallery|census|docs"` → `ALL OK` (`gallery.is-current`, `census.gallery`
  and `docs.numeric-headroom` included: the entry is rendered, never evaluated, so no census row moves —
  if one does, regenerate with the `formula-cpp-census-page` target on cl and say so in the report).

- [ ] **Step 4: The census page.** Read `round_wide_ratio` in `detail/wide_rounding.hpp` and confirm it builds
  its result with `Rational::from_decimal`. Then add, after the paragraph beginning "The hooks sit in"
  (`docs/numeric-headroom.md:95-104`): "The logarithm and exponential kernel (`detail/transcendental.hpp`)
  carries no hooks: it computes in the wide words of `detail/wide_int.hpp`, outside the census, and only
  the rounded decimal it answers is counted, made by `Rational::from_decimal` as every other rounding's
  is." If the foundation already wrote a sentence about the wide words, extend that one instead of adding a
  second; if `round_wide_ratio` does not use `from_decimal`, name what it does use.

- [ ] **Step 5: CHANGELOG.** Read the whole `## [Unreleased]` section: the entries of Tasks 1, 3, 5 and 6
  read as one account; add "- A section, *Logarithms and
  exponentials*, in *Expressions and evaluation* (`docs/expressions.md`), and a gallery entry, a
  logarithmic reduction rounded exactly to 0.01." to `### Added`, and extend the foundation's entry for
  *Values the exact layer cannot hold* (in the same `### Added`) so that it names the logarithm and the
  exponential beside the root: one clause, not a second entry.

- [ ] **Step 6: Full verification.** `CL` and `GCC14`, no filter; then `git grep -nE "(^|[^A-Za-z0-9_])(ISO|IEC|IEEE|EN|DIN|BS|NF|SN|UNI|TL|TP|ASTM)[ ./_-]*[A-Z]?[ ./_-]*[0-9]"`
  over the files this plan touched shows nothing new.

- [ ] **Step 7: Commit.**
  ```
  docs(expressions): logarithms and exponentials, with a gallery entry

  How to write ln, log10 and exp of a bare number, where each is exact, how to declare the precision
  an irrational value is reported at, what goes wrong and how it reads on the page; the display guide
  names the logarithm and the exponential among the values the exact layer cannot hold, with the trace
  line of a rounded logarithm; the census page says the kernel's wide words are outside its tally; and
  the gallery shows a logarithmic reduction rounded exactly to 0.01.

  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

---

## Self-Review

**Spec coverage.**

| Spec section | Where |
|---|---|
| §1 Problem, §2 two forms; `rounded<>(ln(x))` stays failing; real power out of scope | Tasks 1, 5 (test "rounded of a plain logarithm stays an exact logarithm that fails"), 7 (*A real power*) |
| §3 plain node, factories on `Node` only, `RepFunctions` members, the case table, log10 detection, dimension rule and message, percent, `refused` pass-through | Task 1 (all; negatives 1–6; `LogarithmAccepts`; NaN guard; `+inf`) |
| §4 rounded nodes, no unit, special points to `checked_round`, `double` refused, shared dimension check | Task 5 |
| §5 kernel: fixed point, ln reduction and series, log10, exp early exits and reduction, constants, error bound, undecided → `Overflow`, places range, step budget | Task 4 (algorithm text, code, six tests, Steps 1 and 7); early exits and places in Task 5 |
| §6 rendering table, call form, `\exp`, shared name helper, six step kinds, trace lines | Tasks 1 (`transcendental_name`), 3, 6 |
| §7 registries | Tasks 2 (plain walks), 3 (plain text), 5 (rounded walks, `formula.hpp`, `FILE_SET`, consumer-globals include), 6 (rounded text, mode suffix) |
| §8 tests | `function_tests` (T1), `transcendental_tests` (T4), `rounded_transcendental_tests` (T5), negatives (T1, T2, T5), every-node-kind suites (T3, T6: render, vocabulary, trace, trace_render, document; T2, T5: overlay, calculation; T1, T3, T5, T6: consumer_globals). `measured_tests` holds no per-node absence table (Finding 7): absence is pinned in T1 and T5. |
| §9 documentation | Task 7; CHANGELOG in Tasks 1, 3, 5, 6, 7 |
| §10 risks | Finding 4 and Task 4 Step 7 (budget); Tasks 3, 6 (`-Wshadow` via GCC14); `-Wconversion` casts in the kernel; shifts bounded (`doublings` ≤ 62, `carried << 32` with `carried` < 2^32); names list in Task 4 Step 5 |

**Placeholder scan.** Two pins are filled in from a run by design, each with its required shape: the
vocabulary trace lines (Tasks 3, 6), whose step numbers depend on the every-kind formula, and the measured
step counts (Task 4 Step 7). Nothing else is left to the implementer; the wide-word primitives are the
foundation's, confirmed (not written) in Task 4 Step 2.

**Type consistency.** `Transcendental`, `TranscendentalNode<F, Operand>`, `RoundedTranscendentalNode<F,
Places, Mode, Operand>`, `detail::transcendental_name`, `detail::transcendental_text<D>`,
`detail::transcendental_call`, `detail::rounded_transcendental`, `detail::Enclosure`, `KernelWord`,
`KernelLimbs`, `Ln2Lower/Upper`, `Log10eLower/Upper`, `AtanhSlack`, `ExponentialSlack` are spelt the same
in every task that uses them; the kernel's enclosures are `WideRatio<12>` wherever `decide_rounding` meets
them.

**Review Focus placement.** 1 → T1, T5; 2 → T4 (table, straddle), T5; 3 → T5; 4 → T1, T5; 5 → T1, T5.

**Defect classes, as they bear on this plan.** (1) Every compiler claim above names its compiler and
version and was measured; the header comments' "Checked on GCC under -Wshadow" become true in the GCC14 run
of Tasks 3 and 6. (2) The dimension check is gated on `refused_already` (negative 5 REJECTs the second
message); the rounded node's double refusal REJECTs the dimension message. (3) Both nodes are aggregates
whose trace-read facts (`function`, `places`, `mode`) are type-level; a hand-built aggregate is checked in
the class body (negatives 4 and T5-2). (4) No `{}` on `operand`. (5) Every value fixture separates the modes
a plausible wrong implementation would confuse (ln 2 and ln 1/2 for the directed pairs, exp 1 for the
nearest modes rounding up, the ties for the half modes, 5 % against ln 5). (6) Not applicable: no rule
judged per step. (7) Every negative case has a deletion check. (8) The tools named in Global Constraints.
(9) Citations are `Example Standard 8:2023` and `12:2021 NA`; fixtures are invented ratios and counts.

---

## Open questions for the controller

1. **The per-call target** — resolved: at most 100 000 cl steps per kernel call at `KernelLimbs = 12`
   (Finding 4, Task 4 Step 2), about a tenth of cl 19.51.36257's effective default of about 1 049 000; the
   stand-in measured 3 000–31 000. Task 4 Step 7 reports each call and stops only above 100 000.
2. **`divmod_small` and the zero-limb skip in `wide_int.hpp`** — resolved: the foundation plan now ships
   both (its Finding 11); Task 4 Step 2 only confirms them.
3. **`decide_rounding` at `L = 12`** — resolved: the foundation's `round_wide_ratio` reduces its argument, then
   multiplies the numerator (or, for negative places, the denominator) by `pow10<L>(|places|)` within the same
   width and divides once (declared-precision plan, Task 2). Numerators up to 2^193 times 10^18 < 2^60 stay
   below 2^253 < 2^384. Measured while planning, on stand-ins built from this plan's kernel code and the
   foundation plan's `wide_int.hpp` and `wide_rounding.hpp` code: 13 roundings (ln 2, log10 2 and exp 1 at 18
   places among them) give the reference's answers at run time on cl 19.51, and a whole rounding costs
   240 600–284 500 cl steps (297 950–355 300 on clang-cl 22.1.3), nearly all of it `decide_rounding`'s gcd
   and binary `divmod`s. A spurious `Overflow` in the 18-place table means the word is too narrow: stop and
   report, do not add slack.
4. **`StepKind` order** — resolved: only the declared-precision plan (`RoundedOpaqueOutput`) and this plan (six
   kinds) append enumerators, in that order; the regression plan appends none.
5. **Compile-time checks through the kernel** — resolved, for consistency across the three plans and for
   portability (clang's default `-fconstexpr-steps` is 1 048 576 as well): every check in Tasks 4 and 5 that
   reaches the kernel, or `decide_rounding` on its enclosure, is a run-time `CHECK`; the special points and
   early exits stay `STATIC_REQUIRE`. Exactly one compile-time check reaches the kernel, "transcendental
   kernel: the kernel answers at compile time" — log10 2 at 3 places, the cheapest whole rounding measured
   (about 240 600 cl steps) — so the claim "at compile time and at run time" stays true. Each per-task build
   on cl-debug and g++-14 compiles it; if it exceeds any toolchain's default at the final all-preset run,
   the controller moves it to run time and takes the compile-time claim out of the documentation. Task 7
   quotes `CHECK` lines.

