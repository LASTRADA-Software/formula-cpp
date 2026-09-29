# Named Base Dimensions (Money) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Let an application declare base dimensions the SI does not have — money first — so that euros can
no longer be added to a bare ratio, euros and yen never convert into each other, and kWh × EUR/kWh is EUR.
Today `Dimension` (`include/formula-cpp/dimension.hpp:189-208`) holds exactly the seven SI exponents, and
`examples/composition.cpp:39-48` has to declare `Euro` as `dim::Scalar`, with a comment admitting the
dimension system "will not stop you adding euros to a bare ratio".

**Architecture:** `Dimension` keeps its seven SI exponents and gains a fixed-capacity, name-sorted list of up
to four *named bases*, each a name (`Symbol`, the 16-byte structural string `Unit` already uses) and a rational
exponent. `base_dimension("EUR")` makes one. `*`, `/`, `power` and `nth_root` merge the lists canonically
(sorted, packed, zeroed tail, zero exponents removed), so structural equality stays mathematical equality and
two spellings of one dimension are one template argument. Each currency is its own base: EUR and JPY never
convert (an exchange rate is data — a quantity in JPY/EUR), a euro cent (magnitude 1/100 of the EUR base)
converts to EUR. The library ships **no currencies**: `formula::unit` is generic physics only, the currency list
and its minor-unit decimals come from a third-party standard that `hygiene.no-real-standards` forbids naming,
and decimals are application policy.

**Tech Stack:** C++23, header-only; Catch2 via CPM; `STATIC_REQUIRE`; the `test/negative/` harness.

**Known costs, accepted:** `Dimension` 56 → ~152 bytes, `Unit` 152 → ~248, `Step<Rational>` 1008 → ~1296
(its `dimension`, `unit` and `sourceUnit` each grow) — measure; `record_trace_tests.cpp:346` pins
`sizeof(Step<Rational>) == 1008` and is updated **in the same commit** as the `Dimension` change, with the
reason in its comment. Symbols naming a unit grow by roughly 300–400 characters on cl. Measure build time
before and after (task 8).

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

## Exact spellings (`dimension.hpp`)

```cpp
inline constexpr std::size_t NamedBaseCapacity = 4;

struct NamedBase
{
    Symbol name {};        // the base's name; also the symbol of its coherent unit ("EUR")
    Exponent exponent {};
    [[nodiscard]] constexpr bool operator==(NamedBase const&) const noexcept = default;
};

struct Dimension
{
    Exponent length {}; /* ... the seven SI exponents, unchanged ... */ Exponent luminosity {};
    NamedBase namedBases[NamedBaseCapacity] {};   // last, so Dimension{ .length = ... } still compiles
    [[nodiscard]] constexpr bool operator==(Dimension const&) const noexcept = default;
};

[[nodiscard]] consteval Dimension base_dimension(char const* baseName) noexcept;

namespace detail {
    struct MergedDimension { Dimension dimension {}; bool fits = true; };
    [[nodiscard]] constexpr int compare_base_names(Symbol const&, Symbol const&) noexcept;
    [[nodiscard]] constexpr MergedDimension merged_dimension(Dimension const& leftOperand,
                                                             Dimension const& rightOperand,
                                                             bool dividing) noexcept;
}
```
(`merged_dimension`, not `combined…`: `detail::combined_dimension` already exists at `expression.hpp:214`.)

**Canonical form:** used entries at the front; each has a non-zero exponent; names strictly ascending,
compared byte by byte as `unsigned char` over all 16 bytes (bytes after the terminator are zero); every slot
after the last used one equals `NamedBase {}`. Filling `.namedBases` by hand bypasses it — document that with
the same warning `Exponent` carries (`dimension.hpp:25-28`).

**Merge (O(N+N)):** combine the seven SI exponents with `+` (or `-` when dividing) as today; walk both sorted
lists with two cursors (each stops at the first zero-exponent slot): smaller left name → emit left; smaller
right name → emit right (negated when dividing); equal names → emit `l ± r` unless it is zero (cancellation).
Emitting when all four slots are used sets `fits = false` and stops. Cancellation happens before the count, so
`(EUR/USD) * (USD/JPY)` fits. Exponent overflow still goes through `detail::reduced`'s existing guard.

**Operations:** `operator*` / `operator/` call `merged_dimension`; if it does not fit they call a new guard
`detail::formula_dimension_has_too_many_named_bases()` — a deliberately non-`constexpr` function whose name is
the error, like the existing guards (a compile-time evaluation fails naming it; reached at run time it aborts —
so no run-time path may reach it, see task 3). `power(d, 0)` → `Dimension {}`; otherwise scale each used entry.
`nth_root` divides each used entry (degree 0 still hits the existing zero-denominator guard). `is_dimensionless`
(`== Dimension {}`) is correct unchanged.

**`base_dimension` validation** (one guard each): empty → `formula_base_dimension_name_must_not_be_empty`;
16 bytes or longer → `formula_base_dimension_name_too_long`; not an ASCII letter followed by ASCII letters or
digits → `formula_base_dimension_name_must_be_a_letter_then_letters_or_digits` (the coherent-unit text uses
space, `/`, `^`, parentheses; Markdown treats `_`, `*`, `[` as markup); one of `m kg s A K mol cd` →
`formula_base_dimension_name_is_an_si_base_unit_symbol` (a base named "m" would read as metres). `consteval`
makes a bad name always a compile error; if cl mishandles `consteval` returning `Dimension`, use `constexpr` —
the guard names still appear.

**Identity is the name, byte for byte.** Two libraries declaring `base_dimension("EUR")` get the same dimension
— for a currency code that is what you want. The documented convention makes it work: **the coherent unit of a
base is one of it** (the unit named after the base has magnitude 1; a cent 1/100). For generic words the docs
advise distinctive names ("AcmeCredit", not "credit"). No `dim::Money`: one money dimension could not tell EUR
from JPY. Never write the name of the currency-code standard; say "three-letter currency codes".

## Task 1: Probe (throwaway, not committed)

- [ ] On cl, clang-cl, clang++ and g++-14: a `Dimension`-like struct holding `NamedBase namedBases[4]` (each a
  16-byte `char` array + exponent) works as a template argument, links identically across two translation
  units (mirror `test/unit_cross_tu.hpp`), `consteval` returning it works, the guard-function names appear in
  each compiler's diagnostic, and measure `sizeof`. Fallbacks if anything fails: parallel arrays `Symbol
  namedBaseNames[4]; Exponent namedBaseExponents[4];` (same size and meaning); `constexpr` for `consteval`.
  Write findings into the report; probe files live only in the scratchpad.

## Task 2: Move `Symbol` into `dimension.hpp` (refactor)

- [ ] Move `SymbolCapacity`, `Symbol`, `detail::formula_unit_symbol_too_long`, `symbol()`, `view(Symbol const&)`
  and the deleted `view(Symbol&&)` **verbatim** from `unit.hpp:27-111` into `dimension.hpp` (before `:183`);
  `unit.hpp` includes `dimension.hpp`, so every spelling keeps compiling. Fix `band.hpp:23`'s stale
  `unit.hpp:114` reference (cite `Bounds` by name). Full suite green untouched.
- [ ] Commit: `refactor(dimension): the fixed-size symbol type moves next to Dimension`.

## Task 3: Named bases in `Dimension`

**Files:** `dimension.hpp`, `test/dimension_tests.cpp`, `test/dimension_cross_tu.hpp` + `_b.cpp`,
`test/record_trace_tests.cpp` (size pin), negative cases, `test/CMakeLists.txt`, `CHANGELOG.md`.
- [ ] Everything under "Exact spellings", with Doxygen on every public entity (Doxyfile fails on undocumented
  ones). Update the file comment (`:4-11`: "seven SI base dimensions and up to four named ones"; add
  `<cstddef>`, `<string_view>`). Reword the `dim::` comment (`:259-260`) "Named dimensions" → "Dimension
  constants". `RequireSameDimension`'s message (`:321-363`) gets ", then the named base dimensions by name"
  appended after "...luminosity" (negative tests match only the prefix; no doc quotes the tail — check).
- [ ] Compile-time tests (`EUR = base_dimension("EUR")`, `JPY = base_dimension("JPY")`): EUR == EUR; EUR is
  not JPY, not Scalar, not dimensionless; `Tagged<EUR * dim::Energy / dim::Energy>` is the same type as
  `Tagged<EUR>`; `EUR * JPY` and `JPY * EUR` equal and the same type; `(EUR / JPY) * JPY == EUR`; `EUR / EUR` is
  Scalar's type; `dim::Energy * (EUR / dim::Energy) == EUR`; `power(EUR, 0) == dim::Scalar`;
  `nth_root(power(EUR, 2), 2) == EUR`; `nth_root(EUR, 2).namedBases[0].exponent == exponent(1, 2)`;
  `(JPY * EUR).namedBases[0].name == symbol("EUR")`, slot [1] JPY, slot [2] `NamedBase {}`; four distinct bases
  fit; `detail::merged_dimension(four, fifth, false).fits` is false; four bases, divide one out, multiply a fifth
  in: fits. Runtime: extend the "stay at exactly zero" loop (`:193-201`) to check `namedBases[0] == NamedBase {}`;
  add EUR, JPY and EUR/Energy to the pairwise sweep (`:234`) and identity sweep (`:250`).
- [ ] Cross-TU: `consume_tariff(Tagged<EUR / dim::Energy>)` defined in one file with a different spelling
  (`dim::Scalar / dim::Energy * EUR`), plus a JPY*EUR vs EUR*JPY pair.
- [ ] Negative (register each wrong-first; deletion check): `dimension_named_base_mismatch` →
  `formula: these two dimensions are not the same`; `dimension_too_many_named_bases` and
  `expression_too_many_named_bases` (five quantities multiplied) → `formula_dimension_has_too_many_named_bases`;
  `base_dimension_name_empty`, `_too_long`, `_not_identifier` (`"EUR/kWh"`), `_si_symbol` (`"kg"`) → the guard
  names. (They expect a function name, not a `formula: ` message: a `static_assert` cannot see a function
  argument; the harness already matches guard names, `test/CMakeLists.txt:267-287`.)
- [ ] Update the `sizeof(Step<Rational>)` pin to the measured value, comment says why, same commit.
- [ ] CHANGELOG: Added `base_dimension`, `NamedBase`, `NamedBaseCapacity`; Changed `Dimension::namedBases`,
  the sizes of `Dimension`, `Unit` and `Step`, `Symbol` now declared in `dimension.hpp`, the
  `RequireSameDimension` wording, structured bindings over `Dimension` no longer have seven members, and a
  currency previously declared as `dim::Scalar` should move to a named base.
- [ ] Commit: `feat(dimension): base dimensions an application declares, such as money`.

## Task 4: The trace spells named bases and never aborts on them

**Files:** `trace.hpp` (`unit_quotient`, `:2585-2637`), `trace_render.hpp` (`coherent_unit_text`,
`:2104-2145`), `evaluate.hpp` (`coherent()` comment, `:37-40`), `unit.hpp` comments (`:4-12`, `:142-161`: the
coherent unit is "SI, or one of each named base"), tests (`opaque_tests.cpp` near `:1041`, trace tests).
- [ ] `unit_quotient` uses `detail::merged_dimension(over.dimension, under.dimension, true)` and returns
  `std::nullopt` when it does not fit — removes the run-time abort.
- [ ] `coherent_unit_text` spells named bases first, each through `escaped_author_text` (`:130`): `EUR`,
  `EUR s^2/(m^2 kg)`, `1/JPY`, `EUR/JPY`, `EUR^(1/2)` — pin each; plus an escaped hand-built name `a]b`.
- [ ] Test: `unit_quotient` over two units carrying 3 + 3 distinct bases returns `nullopt` and does not abort.
- [ ] Places that gate on `== dim::Scalar` now treat a currency correctly with no change (critical_value.hpp:187
  and :379 refuse a count in EUR; trace.hpp:2132-2135 a series of prices scaled by a ratio reads in EUR;
  trace.hpp:2595/:2655 a currency unit can be borrowed for display; trace_render.hpp:2158 an opaque output shows
  `EUR`) — add one test for the first two.
- [ ] Commit: `feat(trace): named base dimensions in coherent units`.

## Task 5: Money, end to end

**Files:** `test/unit_tests.cpp` (append), `test/measured_tests.cpp`, `test/evaluate_tests.cpp` or
`expression_tests.cpp`, `test/unit_cross_tu.hpp` + `_b.cpp`, four negative cases.
- [ ] Test-local units: `Euro` (EUR, magnitude 1, 2 decimals), `EuroCent` (1/100, 0 decimals), `Yen` (JPY, 0
  decimals), `KilowattHour` (3600000 J — or the shipped one if it exists on this branch),
  `EuroPerKilowattHour` (EUR / Energy, magnitude 1/3600000, 4 decimals).
- [ ] `converted(250, 1, Euro, EuroCent) == 25000` and back; `checked_convert(1, Euro, Yen)` →
  `DomainError` at compile time and at run time; `RequireSameUnitDimension<Euro, EuroCent>::value`; Yen rounds
  to whole yen; `checked_convert_to` EUR → JPY → `DomainError`, cents → EUR exact; 150 kWh × 3/10 EUR/kWh is
  exactly 45 EUR; 10 EUR + 250 ct is 25/2 EUR; 100 EUR × 16235/100 JPY/EUR is 16235 JPY (an exchange rate is
  data, supplied as a quantity); compile-time dimension of `var<Energy> * var<Tariff>` is EUR.
- [ ] Negative: `money_plus_number` (`var<Price> + formula::Rational { 1, 2 }`) and `money_plus_other_currency`
  → `formula: the two sides of this addition or subtraction measure different dimensions`;
  `unit_currency_mismatch` → `formula: these two units measure different dimensions`;
  `evaluate_result_other_currency` → `formula: this result quantity does not measure the dimension this
  expression computes`. Use operands whose SI parts are identical, so only the named base explains the
  failure; `EXPECT_COUNT 1`.
- [ ] Commit: `test(unit): money as a dimension, end to end`.

## Task 6: Examples

- [ ] `examples/composition.cpp`: declare `Euro` with `.dimension = formula::base_dimension("EUR")` (`:46`);
  rewrite the comments at `:19-20` and `:39-45` (drop the "will not stop you adding euros to a bare ratio"
  caveat); add `static_assert(!formula::SameDimension<Euro.dimension, formula::dim::Scalar>)` with a comment
  pointing at `test/negative/money_plus_number.cpp`. Arithmetic and output unchanged (so
  `docs/expressions.md:394-400` stays valid).
- [ ] `examples/dimensions_and_units.cpp`: `print_exponent` takes `std::string_view`; `print_dimension`
  (`:37-50`) also prints named bases (`EUR^1`); a new section "a base dimension the SI does not have: money"
  (tariff dimension, tariff × energy is EUR, 250 EUR = 25000 ct, EUR → JPY refused with `DomainError`), added to
  the summary checks (`:155-157`).
- [ ] Regenerate `docs/numeric-headroom.md` on cl only if a census row changed (say which).
- [ ] Commit: `docs(examples): money as its own dimension`.

## Task 7: Documentation

- [ ] `docs/dimensions.md`: update `:18-25` and `:27-38` (the order text); rewrite `:74-76` (the "eighth base
  quantity" hypothetical has now happened, with no call site changed); the Units table (`:110-117`); remove the
  internal "phase 4" label at `:119`; fix the stale citations at `:155-157` and `:181` (cite assertion text, not
  line numbers); a new section **"Base dimensions the SI does not have"**: why (money), `base_dimension`, identity
  by name and the magnitude-one convention, one base per currency with the exchange rate as a JPY/EUR quantity,
  the name rules, capacity and cancellation, how the name appears in a coherent unit; Limits (`:248-284`) gains
  the capacity, name limits and guard names. Output blocks from a real run.
- [ ] `docs/expressions.md`: after `:108`, one paragraph on money being refused, pointing at the two negative
  tests; one sentence near `:373` that `c_u` is in a dimension of its own. `docs/series.md:102`: the coherent
  unit also covers a named base. README (`:235`, `:263`) and `docs/index.md` (`:63`, `:101`): mention base
  dimensions such as money.
- [ ] Run every `hygiene.*`, `gallery.is-current` (must stay green: nothing in `document()` renders a
  `Dimension`), `docs.numeric-headroom`, Doxygen. Commit: `docs(dimensions): base dimensions the SI does not have`.

## Task 8: Measure and report

- [ ] `sizeof(Dimension)`, `sizeof(Unit)`, `sizeof(Step<Rational>)` before/after; `formula-cpp-tests` build time
  on cl-debug and clangcl-debug, three runs each, before (master) and after; object-file size delta of one
  test TU. Write them into the report (the controller puts them in the pull request).
