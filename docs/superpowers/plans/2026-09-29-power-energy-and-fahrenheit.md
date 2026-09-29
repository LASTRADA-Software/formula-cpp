# Power, Energy and Fahrenheit Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ship the units an energy calculation needs — watt, kilowatt, watt-hour, kilowatt-hour — with the
dimension `dim::Power`, and a second affine temperature unit, degrees Fahrenheit. They are what the
electricity-bill example of the calculation engine needs, and none of them exists today
(`include/formula-cpp/unit.hpp:195-530` ships 51 units, no Watt, no kWh, no Fahrenheit; `dim::Energy`
exists at `dimension.hpp:295-296`, `dim::Power` does not).

**Architecture:** Nothing new in the mechanism. Units are structural aggregates used as template
arguments (`unit.hpp:150-171`: `dimension`, `magnitudeNumerator/Denominator`, `offsetNumerator/Denominator`,
`symbolText` via `symbol("...")`, `decimals`, `bounds`); `value_in_SI = value * magnitude + offset`
(`unit.hpp:142-149`). Every offset rule in the library keys on `offsetNumerator != 0`
(`trace.hpp:2022-2025` `borrowable`, `:2595` `unit_quotient`, `rounded_root.hpp:82`, `precision.hpp:277`),
so a second affine unit needs no code change outside `unit.hpp`.

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

## Values (verified arithmetic)

| Name | Where | Dimension | Magnitude | Offset | Symbol | Decimals (why) |
|---|---|---|---|---|---|---|
| `dim::Power` | `dimension.hpp`, right after `Energy` (:295) | `Energy / Time` (L² M T⁻³) | — | — | — | — |
| `unit::Watt` | `unit.hpp`, a power/energy block after `Kilojoule` (:422-425) | `dim::Power` | 1 | 0 | `"W"` | 1 (as `Joule`) |
| `unit::Kilowatt` | same block | `dim::Power` | 1000 | 0 | `"kW"` | 3 (last digit = one watt) |
| `unit::WattHour` | same block | `dim::Energy` | 3600 | 0 | `"Wh"` | 1 |
| `unit::KilowattHour` | same block | `dim::Energy` | 3600000 | 0 | `"kWh"` | 3 (last digit = one Wh) |
| `unit::Fahrenheit` | right after `Celsius` (:365-369) | `dim::Temperature` | 5/9 | 45967/180 | `"\xc2\xb0" "F"` | 1 (as `Celsius`) |

Fahrenheit: K = °F·5/9 + 459.67·5/9 and 459.67·5/9 = 45967/100·5/9 = 45967/180. Checks: 32 °F → 3200/180 +
45967/180 = 49167/180 = 273.15 K; 212 °F → 373.15 K; −40 °F → 41967/180 = 233.15 K = −40 °C; 98.6 °F →
55827/180 = 310.15 K = 37 °C; 37 °C → 493/5 °F. All intermediates ≤ ~10⁶. °C→°F multiplies by 9/5 (stays a
terminating decimal); °F→°C divides by 9 (100 °F = 340/9 °C).

`dim::Power`'s Doxygen comment says it is unrelated to the function `formula::power()` (`dimension.hpp:235`).
Megawatt and megawatt-hour are **not** added (on demand only).

---

## Task 1: `dim::Power`, the five units, and their tests

**Files:** `include/formula-cpp/dimension.hpp`, `include/formula-cpp/unit.hpp`, `test/unit_tests.cpp`,
`test/dimension_tests.cpp`, `test/evaluate_tests.cpp`, `test/trace_render_tests.cpp` (or the file that
already pins Celsius series lines), `CHANGELOG.md`.

- [ ] `dimension.hpp`: add `dim::Power` with the table's definition and a Doxygen comment.
- [ ] `unit.hpp`: add the five units with the table's values and Doxygen comments. Reword the comment at
  `unit.hpp:364` ("The affine unit …") to say `Celsius` is one of two affine units and `Fahrenheit` the other.
- [ ] `test/unit_tests.cpp`:
  - the shipped-unit table (`:186-238`): five new rows (`Fahrenheit` as `5, 9`), count 51 → 56 at `:239`;
  - `:273`: `size(table) - 2`, comment "Celsius and Fahrenheit are the affine ones";
  - the symbol list (`:282-332`): five new symbols, count 56 at `:333`;
  - the round-trip pairs (`:562-585`): add `{Kilowatt, Watt}`, `{KilowattHour, Joule}`, `{WattHour, Kilojoule}`,
    `{Fahrenheit, Celsius}`;
  - compile-time conversions: `converted(1, 1, KilowattHour, Joule) == 3600000`;
    `converted(-40, 1, Fahrenheit, Celsius) == -40`; 32 → 0; 212 → 100; 493/5 °F → 37 °C and 37 °C → 493/5 °F;
    `converted(0, 1, Fahrenheit, Kelvin) == 45967/180`; the point, not the difference:
    `converted(1, 1, Fahrenheit, Kelvin) == 46067/180`; `!SameDimension<Kilowatt.dimension, KilowattHour.dimension>`.
    (Use the helper names the file already uses; line numbers are from master `ccb7796`.)
- [ ] `test/dimension_tests.cpp`: `dim::Power` in the exponent-vector checks (`:178-189`), the zero-base
  loop (`:193-195`), the pairwise `named[]` sweep (`:239-243`) and the identity sweep `all[]` (`:252-256`);
  plus `CHECK(dim::Power * dim::Time == dim::Energy)`.
- [ ] `test/evaluate_tests.cpp`: a heater of 3/2 kW for 4 h, evaluated into a quantity declared in
  `KilowattHour`, is exactly `Rational { 6 }`; 98.6 °F evaluated into a °C quantity is exactly 37.
- [ ] Trace pins (fraction default, existing behaviour made explicit):
  - kW × h: `"1. P = 3/2 kW\n2. t = 4 h\n3. #1 * #2 = 21600000\n"` — the product line reads in the unlabelled
    coherent unit (joules), which is today's documented behaviour; the test's comment says so.
  - A Fahrenheit twin of the existing Celsius series case (search `trace_render_tests.cpp` near `:2290-2325`
    for the Celsius one), readings 50 °F and 51 °F: `sum(#1) = 51017/90`, `sample_range(#1) = 5/9`,
    `sample_mean(#1) = 101/2 °F` (a mean reads in °F; a sum and a range read in unlabelled kelvin).
  - If a pinned string differs from what the build prints, **stop and report** — do not adjust the
    expectation to the output without understanding why.
- [ ] Optional negative test `unit_power_is_not_energy` (adding a `Kilowatt` quantity to a `KilowattHour`
  quantity) expecting `formula: the two sides of this addition or subtraction measure different dimensions`
  — add it only if no existing negative test already pins that message for two distinct physical
  dimensions; say which in the report.
- [ ] `CHANGELOG.md` `[Unreleased]` → `### Added`: `dim::Power`, `unit::Watt`, `unit::Kilowatt`,
  `unit::WattHour`, `unit::KilowattHour`, `unit::Fahrenheit`.
- [ ] Commit: `feat(unit): watt, kilowatt, watt-hour, kilowatt-hour and degrees Fahrenheit`.

## Task 2: Documentation and the dimensions example

**Files:** `docs/dimensions.md`, `docs/expressions.md`, `examples/dimensions_and_units.cpp`,
`docs/numeric-headroom.md` (only if regenerated).

- [ ] `docs/dimensions.md`: add `Power` where the named dimensions are listed (`:53`); "fifty-one" →
  "fifty-six" units, naming watt, kilowatt-hour and degrees Fahrenheit (`:128-135`); the affine section
  (`:174-200`) gains Fahrenheit rows (a point converts affinely; a difference does not; 100 °F is 340/9 °C,
  a fraction — say plainly why). **Fix the already-stale line citations** at `:156` (`unit_tests.cpp:234-235`)
  and `:181` (`:243,244,250`): cite the assertion text instead of line numbers, so they cannot go stale again.
- [ ] `docs/expressions.md:272-273`: "An affine unit, of which this library ships one" → two, with one
  sentence on Fahrenheit.
- [ ] `examples/dimensions_and_units.cpp` (around `:99-111`): print and self-check `-40 degF = -40 degC` and
  `1 kWh = 3600000 J` (ASCII `degF`/`degC` only if the example already avoids non-ASCII output — match it);
  update the matching output block in `dimensions.md` from a real run.
- [ ] If the example's census row changes, regenerate `docs/numeric-headroom.md` with the
  `formula-cpp-census-page` target **on cl** (`CMakeLists.txt:207`); otherwise `docs.numeric-headroom` fails.
  Say in the report whether it changed.
- [ ] Commit: `docs(dimensions): power, energy and Fahrenheit units`.
