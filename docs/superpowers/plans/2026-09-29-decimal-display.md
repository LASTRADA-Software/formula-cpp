# Decimal Display and `std::format` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Let a person read a value as a decimal — `0.6`, `18.8822`, `118.26 EUR`, `5.2 kW` — wherever the
library prints numbers, without ever passing an approximation off as exact, and give C++ developers
`std::format` support for `Rational` and `Measured<Q>` that is documented well enough to use without
reading the header. Today every surface prints rationals only (`3/5`): `render.hpp:338-343`
(`detail::number_text`), `:353-356` (`number_with_unit`), the trace via `value_in_declared_unit`
(`trace_render.hpp:1369-1390`); there is no `<format>` anywhere.

**Architecture:**
- A new **core** header `number_text.hpp` (in the umbrella; no `<string>`): a fixed-buffer `NumberText`,
  a `NumberStyle` (fraction / exact decimal / approximate decimal), and constexpr, allocation-free
  spelling functions. One exact rule: a decimal is shown only when it is the exact value; an approximation
  is opt-in, names its rounding mode, and always carries `≈`.
- `render_trace` gains `TraceRenderOptions::numbers` (default: fractions — **the default does not change**;
  flipping it was measured at ~1,040 pinned test literals and ~510 doc tokens and would silently change
  every consumer's archived audit text).
- `render()`/`document()` carry a style through a `detail::StyledVocabulary<V>` — the vocabulary is the one
  channel that already reaches every node, including consumers' own `render_node` overloads
  (`render.hpp:1915-1960`), so no signature a consumer overloads changes.
- A new **opt-in** header `format.hpp` with `std::formatter<Rational>` and `std::formatter<Measured<Q>>`.
- A guide `docs/display.md` with a matching example `examples/display.cpp`, including a full developer
  reference for the format specs.

**Tech Stack:** C++23, header-only; Catch2 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness.

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

## Findings the design depends on (from master `ccb7796`)

1. Number spellings today: formula text `detail::number_text(Rational)` (`render.hpp:338-343`),
   `number_with_unit` (`:353-356`); author-typed numbers `declared_number_text` (`:390-396`), `band_text`
   (`:401-406`), `limit_row_text` (`:2175-2188`); trace `value_in_declared_unit` (`trace_render.hpp:1369-1390`)
   plus direct `number_text` calls at `:1880`, `:1979-1980`. `document.hpp` prints no numbers itself.
2. `render_trace` takes the aggregate `TraceRenderOptions { StepLimit maxSteps; }` (`trace_render.hpp:79-90`);
   all 373 in-tree calls use `{ .maxSteps = N }`, so a second member with a default initialiser breaks none,
   and `{}` stays ill-formed (`StepLimit`'s deleted default constructor, `:63`).
3. Trace helpers receive no options today: `render_trace` (`:2703-2736`) → `step_line` (`:2667`) →
   `escaped_step_line` (`:2481`) on a `ShownStep` (`:219-225`). ~15 helpers print numbers:
   `closed_range_text` :566, `segment_text` :592, `lookup_miss_text` :638, `value_in_declared_unit` :1369,
   `failed_position_text` :1423, `series_step_line` :1455, `curve_point_text` :1501, `curve_step_line` :1538,
   `snap_suffix` :1646, `conformity_line` :1724, `rejection_value_text` :1867, `deviation_text` :1893,
   `rejection_line` :1951, `opaque_value_text` :2150, retry lines :2275 and :2335.
4. `Measured<Q>` is `template <Described Q>` (`measured.hpp:36-37`): a formatter partial specialisation
   deduces a type only — the libc++ problem the prototype hit with `auto` template parameters does not arise.
5. Display code must not feed the overflow census: use plain `std::uint64_t` arithmetic, never
   `FORMULA_CENSUS_NOTE` (`checked_int.hpp:82-86`) or `Rational::make` (`rational.hpp:99-100`). A new example
   gets a census twin automatically, so `docs/numeric-headroom.md` must be regenerated on cl when one is added.

## The API (exact spellings)

`include/formula-cpp/number_text.hpp` — includes only `rational.hpp`, `rounding.hpp`, `unit.hpp`,
`measured.hpp`, `error.hpp`, `<cstddef>`, `<cstdint>`, `<expected>`, `<optional>`, `<string_view>`;
everything `constexpr noexcept` except the throwing forms:

```cpp
namespace formula {
inline constexpr std::size_t NumberTextCapacity = 64;
/// The one spelling of "approximately": U+2248.
inline constexpr std::string_view ApproximationMarker = "\xe2\x89\x88";
/// The one spelling of absence, shared with the trace (trace_render.hpp:1373).
inline constexpr std::string_view NotMeasuredText = "(not measured)";

class NumberText {                       // a number spelled into a fixed buffer; usable at compile time
  public:
    [[nodiscard]] constexpr std::string_view view() const& noexcept;
    std::string_view view() const&& = delete;          // same guard as view(Symbol&&), unit.hpp:111
    [[nodiscard]] constexpr bool is_exact() const noexcept;   // false when rounding changed the value
    friend constexpr bool operator==(NumberText const&, std::string_view) noexcept;
  private:
    char _characters[NumberTextCapacity] {};
    std::size_t _length = 0;
    bool _exact = true;
};

enum class DecimalPadding : std::uint8_t { Trimmed, Padded };
enum class NumberNotation : std::uint8_t { Fraction, ExactDecimal, ApproximateDecimal };

class NumberStyle {                      // private members: an approximating style cannot exist without a mode
  public:
    constexpr NumberStyle() noexcept = default;         // == fraction()
    [[nodiscard]] static constexpr NumberStyle fraction() noexcept;
    [[nodiscard]] static constexpr NumberStyle exact_decimal(DecimalPadding = DecimalPadding::Trimmed) noexcept;
    [[nodiscard]] static constexpr NumberStyle approximate_decimal(RoundingMode approximationMode,
                                                                   DecimalPadding = DecimalPadding::Trimmed) noexcept;
    [[nodiscard]] constexpr NumberNotation notation() const noexcept;
    [[nodiscard]] constexpr RoundingMode approximation() const noexcept;
    [[nodiscard]] constexpr DecimalPadding padding() const noexcept;
    [[nodiscard]] constexpr NumberStyle exact_only() const noexcept;   // ApproximateDecimal -> ExactDecimal
    [[nodiscard]] constexpr bool operator==(NumberStyle const&) const noexcept = default;
};

[[nodiscard]] constexpr bool has_exact_decimal(Rational) noexcept;                   // 10^18 % den == 0
[[nodiscard]] constexpr std::optional<NumberText> exact_decimal(Rational) noexcept;
[[nodiscard]] constexpr NumberText fraction_text(Rational) noexcept;                  // "3/5", "4", "-1/3"
[[nodiscard]] constexpr std::expected<NumberText, ArithmeticError>
    checked_decimal_text(Rational, DecimalPlaces, RoundingMode, DecimalPadding) noexcept;
[[nodiscard]] constexpr NumberText decimal_text(Rational, DecimalPlaces, RoundingMode, DecimalPadding);  // throws
[[nodiscard]] constexpr std::expected<NumberText, ArithmeticError>
    checked_number_text(Rational, NumberStyle, Unit const& shownIn) noexcept;
[[nodiscard]] constexpr NumberText number_text(Rational, NumberStyle, Unit const& shownIn);
template <Described Q> [[nodiscard]] constexpr std::expected<NumberText, ArithmeticError>
    checked_number_text(Measured<Q> const&, NumberStyle) noexcept;              // "5.2 kW", "(not measured)"
template <Described Q> [[nodiscard]] constexpr NumberText number_text(Measured<Q> const&, NumberStyle);
}
```

Capacity 64: approximated ≤ 42 bytes (marker 3 + sign + 19 integer digits + `.` + 18 places); a fraction
≤ 40; plus `" "` and a ≤ 16-byte unit symbol (`unit.hpp:98-104`) ≤ 59.

**Algorithms** — all on `std::uint64_t mag = detail::magnitude(num)` (`checked_int.hpp:41-44`, handles
`IntMin`) and `den`:
- `exact_decimal`: if `10^18 % den != 0` → `nullopt`; else `whole = mag / den`, `rem = mag % den`,
  `frac = rem * (10^18 / den)` (no overflow: `rem < den` ⇒ `frac < 10^18`); print `whole`, `.`, `frac` as 18
  zero-padded digits, trim trailing zeros and a bare `.`. 18 places is the cap: 10^18 is the largest power
  of ten in `Int` (`checked_int.hpp:182-192`) and ±18 is `DecimalPlaces`' documented range
  (`rounding.hpp:260`); longer binary expansions (only from `from_double_exact`) stay fractions.
- `checked_decimal_text`, places 0…18: digit-by-digit long division that never forms `rem * 10` (add `rem`
  ten times, subtracting `den` whenever the running total reaches it; the total stays < 2·den < 2^64). Round
  on the final remainder by comparing `rem` with `den - rem` (no doubling), as `checked_round_to_int` does
  (`rounding.hpp:128-146`). Sign matters only for `Floor`/`Ceiling`; `HalfEven` checks the parity of the last
  kept digit (or of `whole` with no places); a carry propagates into `whole` (≤ 2^63, so `whole + 1` fits);
  a magnitude that rounds to zero prints no `-`; `is_exact` is `rem == 0`. `places > 18` → `Overflow`
  (`checked_round`'s own refusal, `rounding.hpp:260-261`); `places < 0` delegates to `checked_round` then
  `exact_decimal` (the only path touching census arithmetic — say so in its comment). Nothing saturates.
- `checked_number_text(Rational, style, unit)`: `Fraction` → `fraction_text`; `ExactDecimal` →
  `exact_decimal`, else `fraction_text`; `ApproximateDecimal` → `exact_decimal`, else marker +
  `checked_decimal_text(v, declared_decimals(unit) (unit.hpp:695), mode, padding)`. Padding pads an exact
  decimal up to the unit's decimals and never truncates. The `Measured` overload appends `" " + symbol` when
  the unit has one (`number_with_unit`'s rule) and returns `NotMeasuredText` when absent.

**The rule for a value whose decimal never ends:** `ExactDecimal` shows the exact fraction (`1/3`) — the
trace promises never to print "a number nobody can reproduce" (`trace_render.hpp:2697-2702`), and `0.6`
beside `1/3` is honest. `ApproximateDecimal` is opt-in, must name its mode, rounds at the unit's declared
decimals and always carries `≈` (not `~`: a pair of `~` is GFM strikethrough, `render.hpp:428-432`).
**Author-typed numbers** (formula constants, table bounds/rows, limit rows) and **both sides of a stated
comparison** (`rejection_line`'s `decided`, `:1976-1987`) are never approximated: spelled with
`style.exact_only()`, so `≈2.478 g > ≈2.478 g` can never appear beside a verdict.

**Unlabelled coherent values** in the trace (a product in joules) are never padded (their "declared
decimals" is `Unit`'s default of 3, which nobody declared); when approximated they use that 3 places. Say so
in the guide.

---

## Task 1: `number_text.hpp` and its tests

**Files:** new `include/formula-cpp/number_text.hpp`, `include/formula-cpp/formula.hpp`, root
`CMakeLists.txt` (FILE_SET), new `test/number_text_tests.cpp`, `test/CMakeLists.txt`,
`test/consumer_globals_tests.cpp` (+ `_run_tests.cpp` probe), `CHANGELOG.md`.

- [ ] Implement the API and algorithms above, with Doxygen on every public entity.
- [ ] `test/number_text_tests.cpp`, compile-time:
```cpp
STATIC_REQUIRE(*formula::exact_decimal(Rational { 3, 5 }) == "0.6");
STATIC_REQUIRE(*formula::exact_decimal(Rational { 94411, 5000 }) == "18.8822");
STATIC_REQUIRE(*formula::exact_decimal(Rational { -7, 4 }) == "-1.75");
STATIC_REQUIRE(*formula::exact_decimal(Rational { 1, 262144 }) == "0.000003814697265625"); // 2^18: 18 places
STATIC_REQUIRE(!formula::exact_decimal(Rational { 1, 524288 }).has_value());             // 2^19: 19 places
STATIC_REQUIRE(!formula::exact_decimal(Rational { 1, 3 }).has_value());
STATIC_REQUIRE(formula::fraction_text(Rational { -1, 3 }) == "-1/3");
STATIC_REQUIRE(formula::decimal_text(Rational { 23653, 200 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded) == "118.26");
STATIC_REQUIRE(formula::decimal_text(Rational { 23653, 200 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded) == "118.27");
STATIC_REQUIRE(formula::decimal_text(Rational { 4 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded) == "4.00");
STATIC_REQUIRE(formula::decimal_text(Rational { -1, 1000 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded) == "0.00");   // no "-0.00"
STATIC_REQUIRE(formula::decimal_text(Rational { -1, 1000 }, DecimalPlaces { 2 }, RoundingMode::Floor, DecimalPadding::Padded) == "-0.01");
STATIC_REQUIRE(formula::decimal_text(Rational { 9999, 1000 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded) == "10.00");
STATIC_REQUIRE(formula::number_text(Rational { 1, 3 }, NumberStyle::approximate_decimal(RoundingMode::HalfEven), unit::One) == "\xe2\x89\x88" "0.333");
```
  plus a `Measured<Q>` case in a test-local quantity declared in a unit with a symbol (e.g. `unit::Kilojoule`
  or whatever exists on this branch): `number_text(Measured<Q> { Rational { 26, 5 } }, exact_decimal()) ==
  "5.2 kJ"` (adapt the unit; the point is value + space + symbol), and absent → `"(not measured)"`.
  Runtime: `decimal_text(Rational { IntMax, 3 }, DecimalPlaces { 18 }, HalfEven, Padded) ==
  "3074457345618258602.333333333333333333"` while `checked_round` of the same value reports `Overflow`;
  `checked_decimal_text(…, DecimalPlaces { 19 }, …)` → `Overflow`; `DecimalPlaces { -1 }` on 125 → `120`
  (HalfEven) and `130` (HalfAwayFromZero). A **counted property sweep**: numerators −2000…2000, a spread of
  denominators (include 3, 7, 8, 40, 125, 1000, 1024), places 0…6, all 7 modes — wherever `checked_round`
  succeeds, `decimal_text` equals the padded `exact_decimal` of `*checked_round(…)`, and `is_exact()` equals
  `(rounded == v)`; assert the number of cases checked (anti-silent-loop pattern, `unit_tests.cpp:609-616`).
- [ ] Umbrella include, FILE_SET, consumer-globals include + a probe instantiation; CHANGELOG `### Added`.
- [ ] Commit: `feat(number_text): exact decimals, rounded decimals and fractions, spelled without allocation`.

## Task 2: Route today's spellings through the new primitives (no output change)

**Files:** `include/formula-cpp/render.hpp`, `include/formula-cpp/trace_render.hpp`.

- [ ] `render.hpp`'s fraction spelling uses `fraction_text`; the trace's `"(not measured)"` uses
  `NotMeasuredText`. **Output must be byte-identical**: the full suite, `gallery.is-current` and every
  `docs.*` test pass untouched. Commit: `refactor(render): spell fractions through number_text`.

## Task 3: Thread a `NumberStyle` through the trace renderer (no output change)

**Files:** `include/formula-cpp/trace_render.hpp`, `include/formula-cpp/render.hpp` (shared helpers).

- [ ] `render_trace` passes a style (for now always `NumberStyle::fraction()`) to `step_line`, then
  `EscapedStep`, stored on a new `ShownStep::numbers` member — initialise it **explicitly** in `EscapedStep`'s
  member initialiser (today `step { recorded, originRead, lineageCompared }`, `:239`; GCC's
  `-Wmissing-field-initializers` is part of `-Wextra`).
- [ ] Every leaf helper listed in finding 3, and `render.hpp`'s `limit_row_text`, `declared_number_text`,
  `band_text`, takes an explicit `NumberStyle numberStyle` parameter and spells values only through
  `checked_number_text`. Author-typed numbers and comparison operands use `numberStyle.exact_only()`.
- [ ] Output byte-identical; full suite green untouched. Commit:
  `refactor(trace_render): every number a trace line states goes through one style`.

## Task 4: `TraceRenderOptions::numbers`

**Files:** `include/formula-cpp/trace_render.hpp`, `test/trace_render_tests.cpp`, new
`test/negative/render_trace_numbers_without_step_limit.cpp`, `test/CMakeLists.txt`, `CHANGELOG.md`.

- [ ] Add to `TraceRenderOptions` (`:79-90`):
  `/// How every number a line states is spelled. Fractions unless a caller asks.`
  `NumberStyle numbers = NumberStyle::fraction();` and pass it where task 3 passed `fraction()`.
- [ ] A spelling failure prints `(not shown: <describe(error)>)` — the existing wording at `:1382-1383`.
- [ ] Extend the file comment (`:13-29`) with a third refusal: "It never shows an approximation as exact."
- [ ] Tests (exact strings):
  - the road gradient case (`:97-119`) under `exact_decimal()`:
    `"3. #1 / #2 = 0.6\n4. #3 = 0.6 [Road gradient, Example Standard 1:2020, 5.2]\n"` (adapt the citation
    text to what that test already declares);
  - m = 1 kg, V = 3 m3: `fraction` and `exact_decimal` give `"3. #1 / #2 = 1/3"`;
    `approximate_decimal(RoundingMode::HalfEven)` gives `"3. #1 / #2 = \xe2\x89\x88" "0.333"`; the padded
    variant gives `"1. m = 1.000 kg\n2. V = 3.0000 m3\n"` (check the units' declared decimals on this branch
    and derive the padding from them) and leaves the unlabelled step unpadded;
  - a rejection line under `approximate_decimal` still prints exact operands (the `exact_only` rule).
- [ ] Negative: `{ .numbers = … }` without `.maxSteps` → expected text `StepLimit` (as the existing case near
  `test/CMakeLists.txt:901` does) — proves the new member did not reopen the silent-zero hole.
- [ ] CHANGELOG. Commit: `feat(trace_render): decimals in traces, exact unless an approximation is asked for`.

## Task 5: `format.hpp`

**Files:** new `include/formula-cpp/format.hpp`, `cmake/CheckPublicHeaderIncludes.cmake`, root
`CMakeLists.txt`, `include/formula-cpp/formula.hpp` (comment only), new `test/format_tests.cpp`,
`test/CMakeLists.txt`, three `test/negative/format_*.cpp`, `test/consumer_globals_tests.cpp`, `CHANGELOG.md`.

- [ ] Not in the umbrella header. Add it to `exemptHeaders` (`CheckPublicHeaderIncludes.cmake:22-28`) with
  the allowance `"format.hpp=format"`; update that script's comment and `formula.hpp`'s list of opt-in headers.
  Write output with a plain loop so `<string>` is not included directly.
- [ ] `template <> struct std::formatter<formula::Rational, char>` and
  `template <formula::Described Q> struct std::formatter<formula::Measured<Q>, char>` (`char` only; symbols
  are UTF-8 bytes). Members: `constexpr auto parse(std::format_parse_context& parseContext)` and
  `template <typename FormatContext> auto format(T const& shown, FormatContext& formatContext) const`.
  Parameter names avoid the consumer-globals list (`ctx`, `fmt`, `format`, `text`, `value`, `mode`, `width`,
  `precision`, `digits`, `number`, `out`, …).
- [ ] Spec grammar (document it in the header's Doxygen verbatim):
```
spec  ::= [[fill] align] [width] [body]         fill: one UTF-8 code point; align: < > ^ (default >)
body  ::= ''                   exact decimal, else fraction      0.6   1/3    5.2 kW
        | '/'                  fraction                           3/5   1/3
        | '.' N Mode           rounded to N (0..18), padded       {:.2HalfEven} -> 118.26
        | '~' ['.' N] Mode     exact where exact, else ≈ rounded  {:~.3HalfEven} -> ≈0.333
Mode  ::= HalfAwayFromZero | HalfTowardZero | HalfEven | Ceiling | Floor | TowardZero | AwayFromZero
```
  Modes are the `RoundingMode` names verbatim (`rounding.hpp:29-48`); **no default mode**. `.N` is an explicit
  request to round (no marker); `~` adds the marker when inexact. For `Measured`, `~` without `.N` uses the
  unit's declared decimals; for `Rational` it requires `.N`. Width counts **code points**, not bytes. Nested
  `{}` dynamic width is refused.
- [ ] Refusals via the library's sentence-named non-constexpr guard functions (as
  `formula_unit_symbol_too_long`, `unit.hpp:62-65`): `detail::formula_number_format_needs_a_rounding_mode()`,
  `detail::formula_number_format_places_out_of_range()`, `detail::formula_number_format_spec_not_understood()`;
  each throws `std::format_error("formula: …")`. In a literal format string the constant evaluation fails and
  the compiler names the function; under `std::vformat` they throw. Expose the parser as
  `constexpr detail::parse_number_format(std::string_view)` for `STATIC_REQUIRE` tests.
- [ ] No 128-bit arithmetic, no floating-point formatting. Do not `STATIC_REQUIRE(std::formattable<…>)`
  (libstdc++ 14 support uncertain); test with `std::format` calls. Doxygen notes the library owns these
  specialisations (a consumer specialising them too breaks the one-definition rule).
- [ ] `test/format_tests.cpp`: `std::format("{}", Rational{3,5}) == "0.6"`; `"{}"` of 1/3 → `"1/3"`;
  `"{:/}"` of 3/5 → `"3/5"`; `"{:.2HalfAwayFromZero}"` of 23653/200 → `"118.27"`; `"{:~.3HalfEven}"` of 1/3 →
  `"≈0.333"` and of 3/5 → `"0.6"`; `"{:>8}"` of 3/5 → `"     0.6"`; `"{:*^7}"` → `"**0.6**"`; `Measured`
  → value + space + symbol, absent → `"(not measured)"`, `"{:.3HalfEven}"` pads; a `"{:>8}"` case over a
  two-byte symbol (e.g. `°C`) proves code-point width; `std::vformat("{:.2}", …)` throws `format_error` whose
  text starts `"formula: "`; `STATIC_REQUIRE`s on `detail::parse_number_format`.
- [ ] Negative: `format_places_without_mode.cpp` → `formula_number_format_needs_a_rounding_mode`;
  `format_places_out_of_range.cpp` → `formula_number_format_places_out_of_range`;
  `format_spec_not_understood.cpp` → `formula_number_format_spec_not_understood`. Verify on cl, clang-cl,
  clang++ and g++-14 that each diagnostic really names the function; if one compiler does not, say which
  and what it prints.
- [ ] CHANGELOG. Commit: `feat(format): std::format for Rational and Measured`.

## Task 6: Decimal constants in `render()` and `document()`

**Files:** `include/formula-cpp/vocabulary.hpp`, `render.hpp`, `document.hpp`,
`cmake/CheckVocabularyReach.cmake` (only if needed), `test/render_tests.cpp`, `test/document_tests.cpp`,
`test/vocabulary_tests.cpp`, `CHANGELOG.md`.

- [ ] `struct RenderOptions { NumberStyle numbers = NumberStyle::fraction(); };` and one generic overload each:
  `template <Dialect D, typename X, Vocabulary V> requires requires { render<D>(x, v); } std::string
  render(X const&, V const&, RenderOptions)` forwarding to `render<D>(x, detail::styled(vocabulary,
  options.numbers))`, and the same shape for `document<D>`.
- [ ] `detail::StyledVocabulary<V>` in `vocabulary.hpp` beside the closed list (`:368-378`):
  `isVocabulary<StyledVocabulary<V>> = isVocabulary<V>`; forwards `symbol<Q>()`; `styled(styled(v))`
  flattens; public `template <Vocabulary V> constexpr NumberStyle number_style_of(V const&) noexcept`
  returning `fraction()` for the plain vocabularies.
- [ ] Number sites read `number_style_of(vocabulary).exact_only()` with padding ignored (formula text states
  constants as typed, never approximated): `ConstantNode` (`:859-868`), `SeriesConstantNode` (`:1040-1063`),
  lookup corrections (`:1312`, `:1345`, `:1374-1377`), permitted and points lists (`:1396-1400`,
  `:1417-1420`), `limit_row_text` (`:2175-2188`).
- [ ] Delete `detail::number_text(Rational)` so no surface can print a `Rational` without choosing a style.
- [ ] `CheckVocabularyReach.cmake`'s whitelist accepts only the exact `return render<..>(node,
  DefaultVocabulary {});` line shape: do not add vocabulary-free options overloads.
- [ ] Tests: `render<Plain>(var<X> * constant<unit::One>(Rational::from_decimal(863, -3)), DefaultVocabulary{},
  { .numbers = NumberStyle::exact_decimal() }) == "x * 0.863"` (adapt names), plus Markdown and LaTeX forms; a
  1/3 constant stays `1/3` even under `approximate_decimal`; symbols still renamed under a scoped vocabulary;
  `document(…, RenderOptions)` spells its formula text with the style.
- [ ] CHANGELOG. Commit: `feat(render): decimal constants in rendered formulas and documentation`.

## Task 7: The guide, the example and the `std::format` reference

**Files:** new `docs/display.md`, new `examples/display.cpp`, `examples/CMakeLists.txt`, `mkdocs.yml`,
`docs/index.md`, `README.md`, `docs/numbers.md`, `docs/tracing.md`, `docs/numeric-headroom.md` (regenerated),
`CHANGELOG.md`.

- [ ] `examples/display.cpp`: a lab scenario built from units that exist on this branch (not an energy
  bill). It prints: one trace in all three styles (a value that is an exact decimal, one that is not, and the
  padded form), a formula with a decimal constant rendered with `RenderOptions`, `number_text` of a
  `Measured` (present and absent), and **every row of the `std::format` reference table** below, each
  self-checked; ends `all checks passed: yes`.
- [ ] `examples/CMakeLists.txt`: `formula_add_example(display display.cpp <regex>)` pinning `≈`, `1/3`, `0.6`
  and `all checks passed: yes`; `docs.display-output` and `docs.display-snippets` tests wired exactly like
  `docs.statistics-output`/`-snippets` (`examples/CMakeLists.txt:136-147`).
- [ ] `docs/display.md`, written for a C++ developer who has never seen the library:
  1. Why decimals are opt-in and why an approximation is never shown as exact (with the `0.6` beside `1/3`
     example).
  2. Decimals in traces: `TraceRenderOptions::numbers`, the three styles, padding, what "unlabelled coherent
     value" means, and the exact-only rule for author-typed numbers and comparisons.
  3. Decimals in rendered formulas and documentation: `RenderOptions`.
  4. `number_text()` / `decimal_text()`: no `<format>`, no allocation, usable in `static_assert`; when to prefer it.
  5. **Formatting with `std::format`** — a reference a developer can use without reading the header:
     - how to opt in (`#include <formula-cpp/format.hpp>`) and why it is not in the umbrella header;
     - which types format (`Rational`, `Measured<Q>`);
     - a **spec table**, one row per form: syntax, meaning, example call, real output (`{}`, `{:/}`,
       `{:.2HalfEven}`, `{:~.3HalfEven}`, `{:>8}`, `{:*^7}`, a `Measured` value, an absent one) and the
       grammar in one block;
     - the seven rounding-mode spellings, and why no mode is assumed (the same number rounds differently
       under different methods — link `numbers.md`);
     - when a value is shown exactly, as a fraction, or with `≈` — and that `≈` never appears unless asked;
     - fill/align/width count code points, so `°C`, `µm` and `≈` align the same on every toolchain;
     - what goes wrong and how it surfaces: a bad spec in a literal format string is a **compile error**
       naming the problem (quote the real diagnostic of one compiler, from a real compile), the same spec
       through `std::vformat` throws `std::format_error` whose message starts `formula: `;
     - that the library owns these `std::formatter` specialisations.
  Every ```` ```text ```` block is real output of `examples/display.cpp`; every ```` ```cpp ```` block is its
  source.
- [ ] Doxygen on each `std::formatter` specialisation carries the grammar and links this guide section.
- [ ] `mkdocs.yml` nav, `docs/index.md` table, README guide-table row
  (`| [Displaying numbers](docs/display.md) | Decimals in traces and rendered formulas, exact unless an
  approximation is asked for, and std::format for Rational and Measured |`) and a short `std::format` block in
  README's "See it work" (real output). Short prose cross-links in `numbers.md` and `tracing.md`.
- [ ] Regenerate `docs/numeric-headroom.md` on cl (the new example adds a census row).
- [ ] Commit: `docs(display): a guide to decimals, and a std::format reference`.
