# Unit Text, Bounds, Per-Quantity Decimals and Run-Time Parsing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close every issue open on 2026-10-05 (#22, #23, #24, #25, #26, #27) in one pull request.

**Architecture:** Two groups of independent changes, then a merge and the finish.

- **Units (Lane A, Tasks 1–4).**
    - A unit symbol holds 31 bytes; `symbol()` is `consteval`, and run-time text goes through `checked_symbol`.
    - A unit carries an ASCII key; a unit whose symbol is not ASCII must declare one, refused at compile time.
    - Bounds have an optional end each, and a value is checked against limits held at run time with `checked_within`.
    - A quantity can declare its own decimal places, and `same_unit` compares what a unit is.
- **Numbers (Lane B, Tasks 5–6).**
    - Decimal text is parsed at run time, by the same parser `_r` uses, now with a 128-bit mantissa;
      `from_decimal` reaches 10^±38.
    - `checked_transform` and `checked_combine` take callbacks that return `std::expected`.
- **Final tasks (Tasks 7–9).** Merge Lane B; read the guides through once; the finish.

**Tech Stack:** C++23, header-only; Catch2 3.6 (`STATIC_REQUIRE`), the `test/negative/` harness, the CTest docs checks.

**Spec:** `docs/superpowers/specs/2026-10-05-unit-text-bounds-parsing-design.md`. Read it before any task. Each task
names the spec section it implements.

**Order and lanes:**

- **Lane A, units:** Tasks 1 → 2 → 3 → 4, in `D:\formula-cpp` on branch `feature/unit-text-bounds-parsing`.
    - Task 2 uses Task 1's `SymbolError` and `checked_symbol`; Task 4's `same_unit` compares Task 2's key.
- **Lane B, numbers:** Tasks 5 → 6, each in its own agent-owned worktree.
    - Their commits go on the branch `feature/unit-text-bounds-parsing-numbers`. The controller creates it at the
      commit that adds this plan, and after each Lane B task moves it to that task's head (`git branch -f`).
    - Each Lane B task begins with `git reset --hard feature/unit-text-bounds-parsing-numbers` in its fresh worktree.
- **Shared files.** The lanes run at the same time. They share `CHANGELOG.md` and `include/formula-cpp/measured.hpp`
  (Lane A adds the `checked_within` overloads after `checked_within_bounds`; Lane B adds `checked_transform` and
  `checked_combine` after `combine`), so Task 7's merge is mechanical.
- **After both lanes:** Task 7 merges Lane B into `feature/unit-text-bounds-parsing`, then Tasks 8 → 9 in
  `D:\formula-cpp`.

**Line anchors** were read at `931e81c`. Find each place by the name quoted beside its anchor, never by the number
alone.

---

## Global Constraints

These bind every task.

- **C++23, header-only.** Nothing beyond the standard library in `include/`.
- **Worktrees:**
    - Lane A and Tasks 7–9 work in `D:\formula-cpp`.
    - A Lane B task works only in its own agent-owned worktree.
    - No task touches another lane's tree.
- **Gates run from PowerShell.** Bash's pipe mis-encodes `°` and fails `docs.*-output`.
    - `$S = C:\Users\c.parpart\AppData\Local\Temp\claude\D--formula-cpp\81d1061b-b25f-4c83-9f67-664a67264017\scratchpad`
    - `$T` = the task's tree.
- **Verify, the per-task gate.** Every command must print `ALL OK`:
    1. `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Exclude "^negative\."`: the full cl-debug build, then every test
       except the negative ones.
    2. `pwsh -NoProfile -File $S\neg.ps1 -Tree $T -Filter "<regex>"`, for a task that adds or touches negative tests.
       It runs the negatives matching `<regex>` on cl-debug **and** clangcl-debug.
- **Quick loop:** `pwsh -NoProfile -File $S\cl.ps1 -Tree $T -Filter "<ctest -R regex>"`.
- **Run builds and tests in the foreground.**
    - Never wait on a background monitor of your own.
    - Never redirect a build to `/dev/null`: a `STATIC_REQUIRE` failure is a build error.
    - Prove from ctest's count that a filter selected something before trusting it.
    - Catch2 splits test filters on commas.
- **Baseline at `931e81c`:** cl-debug passes 1582 of 1582 non-negative tests; 567 negative tests are registered.
  Each task reports its total and the difference from the previous task in its lane, and that difference must equal
  the number of `TEST_CASE`s and negative tests it added.
- **Invariants (CONTRIBUTING.md), each enforced by a `hygiene.*` test:**
    - an SPDX header on every file, and no `NOLINT`;
    - core public headers include no `<string>`, `<vector>`, `<format>`, `<iostream>` or `<print>`
      (`hygiene.headers`);
    - every public `static_assert` message begins `formula: `;
    - a new test source is added to `test/CMakeLists.txt` beside its neighbours.
- **Consumer globals.** `test/consumer_globals_tests.cpp` declares a long list of `int` globals (line ~145). No new
  parameter or local may reuse one: cl C4459 and g++ `-Wshadow` turn a reuse into a consumer's build error. Names on
  the list this plan is tempted by: `value`, `text`, `symbol`, `view`, `low`, `high`, `lo`, `hi`, `bound`, `bounds`,
  `left`, `right`, `lhs`, `rhs`, `unit`, `mode`, `offset`, `digits`, `digit`, `numerator`, `denominator`, `sign`,
  `scale`, `result`, `error`, `input`, `minimum`, `maximum`, `lower`, `upper`, `character`, `chars`, `length`,
  `precision`, `e`, `i`, `n`. The spec's signatures name parameters informally; use the names this plan gives
  (`spelling`, `magnitude`, `lowEnd`, `highEnd`, `leftUnit`, `rightUnit`, `measured`, `transformed`). Member names
  are not affected.
- **Behaviour rule.**
    - Every computation that answers today gives the same answer afterwards, and every trace text, rendered formula
      and number text is unchanged (the ASCII key is never displayed).
    - The only refusals added are the ones the spec names: `symbol()` given text that is not a constant expression,
      a unit whose symbol is not ASCII and that declares no key, and a `checked_transform`/`checked_combine` callback
      that does not return `std::expected<Rational, ArithmeticError>`.
    - Some `Overflow` refusals now answer: `from_decimal` at exponents from 19 to 38 in magnitude, and `_r` literals
      wider than 64 bits. A refusal is never turned into a different number.
- **No internal labels in public text.**
    - Code, docs, commit messages and the PR never name tasks, lanes, plans, phases, spikes or reviewers, and never
      cite local progress notes.
    - Every sentence must make sense to a reader who never saw this plan.
    - Issue numbers may appear only in the PR body.
- **No third-party standard content.** Fixture values are plainly invented.
- **Style.**
    - Do not run clang-format on existing files; match the surrounding style by hand.
    - Every new public entity and member gets a Doxygen `///` comment.
- **Printing** is `std::print` / `std::println` only. Never `printf`, `puts` or iostream.
- **Error handling.** Check every `std::expected` result before use. Never unwrap unchecked, and never switch to a
  throwing form to shorten code.
- **Newest GCC only** (g++-14). No workaround for an older compiler.
- **Commits.** Conventional style (`feat(unit): …`, `fix: …`, `test: …`, `docs: …`). Every message ends with:

  ```
  Signed-off-by: Christian Parpart <c.parpart@lastrada.net>
  ```

  Commit with `git commit -F <file>`; a here-string passed to `-F -` does not work in PowerShell.
- **CHANGELOG.md:**
    - Entries go under `## [Unreleased]` (it exists, empty, at the top), in `### Added` / `### Changed` / `### Fixed`
      subsections.
    - A breaking change says so in its first words: `**Breaking:** …`.

## Review Focus

The five inputs this spec implies but no obvious test reaches, most likely to bite first. Each one's test is in the
task named.

1. **A symbol of exactly 31 bytes that ends in a multi-byte UTF-8 character**, built by `symbol()` and by
   `checked_symbol`. It fits and is kept whole; 32 bytes is refused, never cut inside the character. Task 1.
2. **A unit whose symbol is ASCII but whose declared key is not** (`asciiText = "µm"` on a unit spelled `um`). It is
   refused like a missing key: the key is what a serialiser trusts. Task 2.
3. **A trace quotient where only the divisor has a key** (`kg` over `µm`): the quotient's key is `kg/um`, built from
   `view_ascii` of both, and the display symbol stays `kg/µm`. Task 2.
4. **A one-sided limit at exactly its end**, and an empty limit pair: `checked_within(0, 0, nullopt)` is
   `WithinBounds`; `checked_within(x, nullopt, nullopt)` is `NotChecked`, never `WithinBounds`. Task 3.
5. **Text the parser must not half-accept:**
    - `"1e"`, `"1e+"`, `"."`, `"+"`, `"-"`, `"1.2.3"` and `"1'000"` (accepted by `_r`, refused at run time);
    - a spelling with a thousand leading fractional zeros before a digit (`Overflow`, not a hang or a wrong value);
    - `"-0.0"` (zero, places 1).
   Task 5.

## Execution

- **Briefs and reports:**
    - The controller writes each task's brief to
      `D:\formula-cpp\.superpowers\sdd\2026-10-05-unit-text-bounds-parsing\task-N-brief.md` (`.superpowers/` is
      git-ignored).
    - The implementer writes its report beside it, as `task-N-report.md`.
    - A Lane B implementer that cannot write outside its worktree writes the report to `.superpowers/task-N-report.md`
      inside its worktree and says so in its reply.
    - A report states: the commits; the test total and its delta; each rule in *Global Constraints* the task touched,
      and how it was kept; anything it could not do.
- **Agents.** Every task is implemented by `sdd-implementer` and reviewed by `sdd-reviewer`. A fix round goes back to
  `sdd-implementer`. Task 7's merge is run by the controller; an `sdd-implementer` resolves any conflict that is not
  mechanical.
- **Negative tests**, where a task adds one:
    1. Add `test/negative/<name>.cpp`, plus `formula_add_negative_test(<name> "<expected>" …)` in
       `test/CMakeLists.txt`, beside its neighbours.
    2. Register it first with a deliberately wrong expected text, and watch it fail.
    3. Register the right text, and watch it pass, on cl-debug and clangcl-debug.
    4. Delete the guard it pins, confirm the case compiles, then restore the guard.

---

## Lane A — units (#27, #22, #25, #26)

Lane A works in `D:\formula-cpp` on branch `feature/unit-text-bounds-parsing`, Tasks 1 → 2 → 3 → 4. Throughout the
lane, `$T = D:\formula-cpp`.

---

### Task 1: Symbol capacity and building a symbol at run time (#27)

Spec §2.1.

**Files:**
- Modify: `include/formula-cpp/dimension.hpp` (`SymbolCapacity` :189, `formula_unit_symbol_too_long` :220,
  `symbol()` :230)
- Modify: `test/unit_tests.cpp` (the symbol tests near :129)
- Modify: `test/number_text_tests.cpp` (`Widest` :54)
- Modify: `test/negative/unit_symbol_too_long.cpp`, `test/negative/base_dimension_name_too_long.cpp`
- Create: `test/negative/unit_symbol_from_runtime_text.cpp`
- Modify: `test/CMakeLists.txt` (negatives near :329 and :369)
- Modify: `docs/dimensions.md` (capacity at :428 and :500–508)
- Modify: `CHANGELOG.md`

**Interfaces:**
- Produces (used by Task 2):
    - `inline constexpr std::size_t SymbolCapacity = 32;`
    - `enum class SymbolError : std::uint8_t { TooLong, EmbeddedNull, NotAscii };`
    - `[[nodiscard]] constexpr std::string_view describe(SymbolError symbolError) noexcept;`
    - `[[nodiscard]] constexpr std::expected<Symbol, SymbolError> checked_symbol(std::string_view spelling) noexcept;`
    - `[[nodiscard]] consteval Symbol symbol(char const* spelling) noexcept;`

- [ ] **Step 1: Write the failing tests** in `test/unit_tests.cpp`, beside the existing symbol tests. Use the file's
  existing `using` declarations; add `#include <expected>` and `<string_view>` if absent.

```cpp
TEST_CASE("checked_symbol: run-time text that fits is kept byte for byte", "[unit][symbol]")
{
    // µmol/(L·min·kg): 18 bytes of UTF-8.
    constexpr std::string_view compound = "\xc2\xb5mol/(L\xc2\xb7min\xc2\xb7kg)";
    STATIC_REQUIRE(compound.size() == 18);
    constexpr std::expected<formula::Symbol, formula::SymbolError> built = formula::checked_symbol(compound);
    STATIC_REQUIRE(built.has_value());
    STATIC_REQUIRE(formula::view(*built) == compound);

    // 31 bytes, the most that fits, ending in a two-byte character: kept whole.
    constexpr std::string_view widest = "abcdefghijklmnopqrstuvwxyz012\xc2\xb5";
    STATIC_REQUIRE(widest.size() == formula::SymbolCapacity - 1);
    constexpr std::expected<formula::Symbol, formula::SymbolError> widestBuilt = formula::checked_symbol(widest);
    STATIC_REQUIRE(widestBuilt.has_value());
    STATIC_REQUIRE(formula::view(*widestBuilt) == widest);

    constexpr std::expected<formula::Symbol, formula::SymbolError> empty = formula::checked_symbol("");
    STATIC_REQUIRE(empty.has_value());
    STATIC_REQUIRE(formula::view(*empty).empty());

    // The same at run time, from text the compiler cannot see.
    std::string const fromCatalogue { compound };
    std::expected<formula::Symbol, formula::SymbolError> const atRunTime = formula::checked_symbol(fromCatalogue);
    REQUIRE(atRunTime.has_value());
    REQUIRE(formula::view(*atRunTime) == compound);
}

TEST_CASE("checked_symbol: text that does not fit, or holds a NUL, is refused", "[unit][symbol]")
{
    constexpr std::string_view tooLong = "abcdefghijklmnopqrstuvwxyz0123\xc2\xb5"; // 32 bytes
    STATIC_REQUIRE(tooLong.size() == formula::SymbolCapacity);
    STATIC_REQUIRE(formula::checked_symbol(tooLong).error() == formula::SymbolError::TooLong);

    constexpr std::string_view withNull { "mg\0L", 4 };
    STATIC_REQUIRE(formula::checked_symbol(withNull).error() == formula::SymbolError::EmbeddedNull);
}

TEST_CASE("describe(SymbolError) names each refusal", "[unit][symbol]")
{
    STATIC_REQUIRE(formula::describe(formula::SymbolError::TooLong)
                   == "the symbol does not fit SymbolCapacity bytes, terminator included");
    STATIC_REQUIRE(formula::describe(formula::SymbolError::EmbeddedNull) == "the symbol contains a NUL byte");
    STATIC_REQUIRE(formula::describe(formula::SymbolError::NotAscii)
                   == "the symbol holds a byte outside printable ASCII");
}

TEST_CASE("symbol(): a compound UTF-8 laboratory unit fits", "[unit][symbol]")
{
    constexpr formula::Symbol compound = formula::symbol("\xc2\xb5mol/(L\xc2\xb7min\xc2\xb7kg)");
    STATIC_REQUIRE(formula::view(compound).size() == 18);
}
```

- [ ] **Step 2: Run them and watch them fail.** Quick loop with `-Filter "symbol"`. Expected: a build error naming
  `checked_symbol` / `SymbolError`.

- [ ] **Step 3: Implement in `dimension.hpp`.**
    - `SymbolCapacity` becomes 32. Its comment: enough for compound laboratory units such as `µmol/(L·min·kg)`
      (18 bytes); a literal that does not fit is a compile error (`symbol()`), run-time text is refused by
      `checked_symbol`, never truncated.
    - `symbol()` becomes `consteval`. Its comment says a run-time spelling goes through `checked_symbol`.
    - `formula_unit_symbol_too_long`'s comment: its body is reached only during constant evaluation, where reaching
      a non-`constexpr` function is the refusal; `symbol()` is `consteval`, so no run-time call exists. Keep the body.
    - Add, after `view(Symbol&&)`:

```cpp
/// Why `checked_symbol` or `checked_ascii_symbol` refused a spelling.
enum class SymbolError : std::uint8_t
{
    /// The spelling does not fit `SymbolCapacity` bytes, terminator included.
    TooLong,
    /// The spelling contains a NUL byte, which would end the stored symbol early.
    EmbeddedNull,
    /// The spelling holds a byte outside printable ASCII (0x20 to 0x7E); only `checked_ascii_symbol` asks.
    NotAscii,
};

/// @p symbolError in prose, for an error message.
[[nodiscard]] constexpr std::string_view describe(SymbolError symbolError) noexcept
{
    switch (symbolError)
    {
        case SymbolError::TooLong: return "the symbol does not fit SymbolCapacity bytes, terminator included";
        case SymbolError::EmbeddedNull: return "the symbol contains a NUL byte";
        case SymbolError::NotAscii: return "the symbol holds a byte outside printable ASCII";
    }
    return "unknown symbol error";
}

/// Builds a Symbol from run-time text -- a catalogue row, a configuration file -- byte for byte. Refuses text that
/// does not fit or that holds a NUL; never truncates, never aborts. UTF-8 is not validated: a symbol is bytes.
[[nodiscard]] constexpr std::expected<Symbol, SymbolError> checked_symbol(std::string_view spelling) noexcept
{
    if (spelling.size() + 1 > SymbolCapacity)
        return std::unexpected { SymbolError::TooLong };
    Symbol built {};
    for (std::size_t characterIndex = 0; characterIndex < spelling.size(); ++characterIndex)
    {
        if (spelling[characterIndex] == '\0')
            return std::unexpected { SymbolError::EmbeddedNull };
        built.characters[characterIndex] = spelling[characterIndex];
    }
    return built;
}
```

  Add `#include <expected>` if `dimension.hpp` lacks it.

- [ ] **Step 4: Move the tests that pin the old capacity.**
    - `test/number_text_tests.cpp` `Widest`: fill all 32 bytes (`'a'` … `'z'`, `'0'` … `'5'`), so it still tests a
      symbol with no terminator at the full capacity. Adjust any expected text in that file that spells the 16
      letters.
    - `test/unit_tests.cpp` :129, :135, :367 use `SymbolCapacity` by name; check each still states what it claims.
    - `test/negative/unit_symbol_too_long.cpp`: the literal becomes `"0123456789abcdef0123456789abcdef"` (32 bytes),
      and the comment says 32 bytes, one more than the 31 usable.
    - `test/negative/base_dimension_name_too_long.cpp`: the name becomes `"AcmeLoyaltyProgrammePointsLedger"`
      (32 bytes); fix its comment the same way.
    - New `test/negative/unit_symbol_from_runtime_text.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// symbol() is consteval: text that only exists at run time cannot reach it, so a symbol too long for
// SymbolCapacity can never abort a running program. Run-time text goes through checked_symbol. This must not compile.
#include <formula-cpp/unit.hpp>

int main(int argumentCount, char** arguments)
{
    formula::Symbol const fromRunTime = formula::symbol(arguments[argumentCount - 1]);
    return fromRunTime.characters[0];
}
```

  Register it as `formula_add_negative_test(unit_symbol_from_runtime_text "<text>")`, where `<text>` is a phrase
  both cl and clang-cl print for a non-constant call of a `consteval` function. Find it by building the case on both
  (cl: C7595 "call to immediate function is not a constant expression"; clang-cl: "call to consteval function …
  is not a constant expression"); `is not a constant expression` is the expected common part. Record both
  diagnostics in the report.

- [ ] **Step 5: Docs and CHANGELOG.**
    - `docs/dimensions.md`: the capacity is 32 bytes including the terminator, 31 usable, at both places; add one
      paragraph: a symbol from run-time text is built with `checked_symbol`, which returns `SymbolError::TooLong` or
      `EmbeddedNull` instead of aborting, and `symbol()` accepts only a constant expression.
    - CHANGELOG `### Changed`: `**Breaking:** \`symbol()\` is \`consteval\`: …` and the capacity entry; `### Added`:
      `checked_symbol` and `SymbolError`.

- [ ] **Step 6: Verify.** Gate 1, and gate 2 with `-Filter "unit_symbol|base_dimension_name"`. Expected delta: +4
  tests, +1 negative.

- [ ] **Step 7: Commit** `feat(unit): symbols of 31 bytes, built at run time without aborting`.

---

### Task 2: A stable ASCII key per unit (#22)

Spec §2.2.

**Files:**
- Modify: `include/formula-cpp/unit.hpp` (`Unit` :67, the four built-ins `PerMille` :152, `Micrometre` :202,
  `Celsius` :313, `Fahrenheit` :325; after `RequireNamedScaledScalar` :569)
- Modify: every header asserting `detail::RequireNamedScaledScalar` (24 sites, listed in Step 4)
- Modify: `include/formula-cpp/quantity.hpp` (`RequireDescribedUnitNamesItsScale` :227, `RequireDescribed` :249)
- Modify: `include/formula-cpp/trace.hpp` (`unit_quotient` :2853)
- Modify: `test/unit_tests.cpp`, and the test that covers `unit_quotient` (find it with `git grep unit_quotient test`)
- Create: `test/negative/unit_without_ascii_key_{measured,constant,snap,var}.cpp`
- Modify: `test/CMakeLists.txt`, `README.md`, `docs/dimensions.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: `checked_symbol`, `SymbolError` (Task 1).
- Produces (used by Task 4):
    - `Symbol Unit::asciiText {};`, directly after `symbolText`
    - `[[nodiscard]] constexpr std::string_view view_ascii(Unit const& unitValue) noexcept;`
      and `std::string_view view_ascii(Unit&&) = delete;`
    - `[[nodiscard]] constexpr bool has_ascii_key(Unit const& unitValue) noexcept;`
    - `[[nodiscard]] constexpr std::expected<Symbol, SymbolError> checked_ascii_symbol(std::string_view spelling) noexcept;`
    - `template <Unit U> struct detail::RequireAsciiKey;` with `static constexpr bool value = true;`

- [ ] **Step 1: Write the failing tests** in `test/unit_tests.cpp`:

```cpp
TEST_CASE("view_ascii: a unit's key, or its symbol when that is ASCII", "[unit][ascii]")
{
    STATIC_REQUIRE(formula::view_ascii(formula::unit::Millimetre) == "mm");
    STATIC_REQUIRE(formula::view_ascii(formula::unit::PerMille) == "permille");
    STATIC_REQUIRE(formula::view_ascii(formula::unit::Micrometre) == "um");
    STATIC_REQUIRE(formula::view_ascii(formula::unit::Celsius) == "degC");
    STATIC_REQUIRE(formula::view_ascii(formula::unit::Fahrenheit) == "degF");
    STATIC_REQUIRE(formula::view_ascii(formula::coherent(formula::dim::Mass)).empty());
    // The display symbol is unchanged.
    STATIC_REQUIRE(formula::view(formula::unit::Micrometre.symbolText) == "\xc2\xb5m");
}

TEST_CASE("has_ascii_key: false for a non-ASCII symbol without a key, and for a non-ASCII key", "[unit][ascii]")
{
    constexpr formula::Unit MicrogramPerLitreUnkeyed { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                       .magnitudeNumerator = 1,
                                                       .magnitudeDenominator = 1'000'000,
                                                       .symbolText = formula::symbol("\xc2\xb5g/L") };
    STATIC_REQUIRE(!formula::has_ascii_key(MicrogramPerLitreUnkeyed));

    constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1'000'000,
                                                .symbolText = formula::symbol("\xc2\xb5g/L"),
                                                .asciiText = formula::symbol("ug/L") };
    STATIC_REQUIRE(formula::has_ascii_key(MicrogramPerLitre));
    STATIC_REQUIRE(formula::view_ascii(MicrogramPerLitre) == "ug/L");

    // An ASCII symbol with a key that is not ASCII is refused like a missing key: the key is what a serialiser trusts.
    constexpr formula::Unit BadKey { .dimension = formula::dim::Length,
                                     .magnitudeNumerator = 1,
                                     .magnitudeDenominator = 1'000'000,
                                     .symbolText = formula::symbol("um"),
                                     .asciiText = formula::symbol("\xc2\xb5m") };
    STATIC_REQUIRE(!formula::has_ascii_key(BadKey));
    STATIC_REQUIRE(formula::has_ascii_key(formula::unit::Millimetre));
}

TEST_CASE("checked_ascii_symbol: printable ASCII only", "[unit][ascii]")
{
    STATIC_REQUIRE(formula::view(*formula::checked_ascii_symbol("ug/L")) == "ug/L");
    STATIC_REQUIRE(formula::checked_ascii_symbol("\xc2\xb5g/L").error() == formula::SymbolError::NotAscii);
    STATIC_REQUIRE(formula::checked_ascii_symbol("tab\there").error() == formula::SymbolError::NotAscii);
    STATIC_REQUIRE(formula::checked_ascii_symbol("abcdefghijklmnopqrstuvwxyz012345").error()
                   == formula::SymbolError::TooLong);
    STATIC_REQUIRE(formula::checked_ascii_symbol(std::string_view { "u\0g", 3 }).error()
                   == formula::SymbolError::EmbeddedNull);
}
```

  Where a `STATIC_REQUIRE` dereferences an `expected`, the dereference is checked by the same expression's
  constant evaluation (an empty `expected` fails to compile); keep it, but do not copy that idiom into run-time code.
  Adjust unit names (`Millimetre`, the volume dimension spelling) to the header's real names.

  The quotient test, beside the existing `unit_quotient` tests:

```cpp
TEST_CASE("unit_quotient: a quotient of keyed units carries a quotient key", "[trace][unit][ascii]")
{
    std::optional<formula::Unit> const perSecond = formula::detail::unit_quotient(formula::unit::Micrometre,
                                                                                   formula::unit::Second);
    REQUIRE(perSecond.has_value());
    REQUIRE(formula::view(perSecond->symbolText) == "\xc2\xb5m/s");
    REQUIRE(formula::view_ascii(*perSecond) == "um/s");

    std::optional<formula::Unit> const kilogramPerMicrometre =
        formula::detail::unit_quotient(formula::unit::Kilogram, formula::unit::Micrometre);
    REQUIRE(kilogramPerMicrometre.has_value());
    REQUIRE(formula::view(kilogramPerMicrometre->symbolText) == "kg/\xc2\xb5m");
    REQUIRE(formula::view_ascii(*kilogramPerMicrometre) == "kg/um");

    // Neither operand has a key of its own: the quotient has none either, and view_ascii reads its symbol.
    std::optional<formula::Unit> const metrePerSecond = formula::detail::unit_quotient(formula::unit::Metre,
                                                                                        formula::unit::Second);
    REQUIRE(metrePerSecond.has_value());
    REQUIRE(formula::view(metrePerSecond->asciiText).empty());
    REQUIRE(formula::view_ascii(*metrePerSecond) == "m/s");
}
```

- [ ] **Step 2: Run them and watch them fail** (quick loop, `-Filter "ascii"`).

- [ ] **Step 3: Implement in `unit.hpp`.**
    - `Unit` gains, after `symbolText`:

```cpp
    /// A stable ASCII key for serialising the unit -- a JSON annotation, a database column, a client's choice -- that
    /// stays the same when `symbolText` is restyled. Empty when the symbol is its own key; required when the symbol
    /// is not printable ASCII (see `RequireAsciiKey`). Never displayed: read it with `view_ascii`.
    Symbol asciiText {};
```

    - After `checked_symbol` is visible (top of `unit.hpp`, after the includes, in `namespace formula`):

```cpp
namespace detail
{
    /// Whether every byte of @p spelling is printable ASCII, 0x20 to 0x7E. The empty text is.
    [[nodiscard]] constexpr bool is_printable_ascii(std::string_view spelling) noexcept
    {
        for (char const byteAt: spelling)
            if (static_cast<unsigned char>(byteAt) < 0x20U || static_cast<unsigned char>(byteAt) > 0x7EU)
                return false;
        return true;
    }
} // namespace detail

/// Builds an ASCII key from run-time text: `checked_symbol`'s refusals, and `SymbolError::NotAscii` for any byte
/// outside printable ASCII.
[[nodiscard]] constexpr std::expected<Symbol, SymbolError> checked_ascii_symbol(std::string_view spelling) noexcept
{
    // checked_symbol first: a NUL is below 0x20, and must be reported as EmbeddedNull, not NotAscii.
    std::expected<Symbol, SymbolError> const built = checked_symbol(spelling);
    if (!built)
        return std::unexpected { built.error() };
    if (!detail::is_printable_ascii(spelling))
        return std::unexpected { SymbolError::NotAscii };
    return *built;
}
```

    - After `struct Unit`:

```cpp
/// The unit's serialising key: `asciiText` when one is declared, otherwise `symbolText`. Stable across restyling of
/// the display symbol. A unit that a template takes is guaranteed printable ASCII here (`RequireAsciiKey`); for a unit
/// built at run time, ask `has_ascii_key` first.
[[nodiscard]] constexpr std::string_view view_ascii(Unit const& unitValue) noexcept
{
    std::string_view const declaredKey = view(unitValue.asciiText);
    return declaredKey.empty() ? view(unitValue.symbolText) : declaredKey;
}

/// Deleted: the view would point into a destroyed temporary, as for `view(Symbol&&)`.
std::string_view view_ascii(Unit&&) = delete;

/// Whether @p unitValue has a printable-ASCII key: its declared `asciiText`, or, when none is declared, its symbol.
[[nodiscard]] constexpr bool has_ascii_key(Unit const& unitValue) noexcept
{
    return detail::is_printable_ascii(view_ascii(unitValue));
}
```

    - In `namespace detail`, after `RequireNamedScaledScalar`:

```cpp
    /// Fails to compile when @p U's symbol is not printable ASCII and it declares no ASCII key (`has_ascii_key`).
    /// Asserted wherever `RequireNamedScaledScalar` is, so every unit a quantity, constant, rounding or table uses
    /// can be serialised by `view_ascii`. Write `::value`, as there.
    template <Unit U>
    struct RequireAsciiKey
    {
        static_assert(has_ascii_key(U),
                      "formula: a unit whose symbol is not ASCII must declare an ASCII key (asciiText), for "
                      "example .asciiText = formula::symbol(\"ug/L\") for ug/L written with a micro sign; the unit "
                      "appears in this diagnostic as the template argument of RequireAsciiKey");

        /// Always `true` once reached -- the `static_assert` above already failed compilation otherwise.
        static constexpr bool value = true;
    };
```

    - The four built-ins gain `.asciiText = symbol("permille")`, `symbol("um")`, `symbol("degC")`,
      `symbol("degF")`, written directly after `.symbolText` (designated initialisers must keep member order).

- [ ] **Step 4: Assert the check beside every `RequireNamedScaledScalar`.** After each of these lines add the same
  line with `RequireAsciiKey`, same argument (`git grep -n "RequireNamedScaledScalar<" include` must list exactly
  these, and afterwards `RequireAsciiKey<` the same 23 plus quantity.hpp's):
  `binning.hpp:146`, `conformity.hpp:349`, `critical_value.hpp:382`, `curve.hpp:111`, `escape.hpp:90`,
  `expression.hpp:53`, `expression.hpp:79`, `lookup.hpp:769`, `lookup.hpp:770`, `lookup.hpp:1177`,
  `lookup.hpp:1812`, `lookup.hpp:1813`, `method.hpp:1085`, `observations.hpp:61`, `opaque.hpp:1030`,
  `overlay.hpp:699`, `rounded_root.hpp:284`, `rounding_node.hpp:52`, `rounding_node.hpp:79`, `series.hpp:73`,
  `series.hpp:183`, `series.hpp:500`, `snap.hpp:172`.

  In `quantity.hpp`, add beside `RequireDescribedUnitNamesItsScale`:

```cpp
    /// `RequireAsciiKey` of a described type's unit, asked only once the type is described, as above.
    template <typename T, bool IsDescribed = Described<T>>
    struct RequireDescribedUnitHasAsciiKey: std::true_type
    {
    };

    template <typename T>
    struct RequireDescribedUnitHasAsciiKey<T, true>: RequireAsciiKey<Describe<T>::unit>
    {
    };
```

  and `static_assert(detail::RequireDescribedUnitHasAsciiKey<T>::value);` after the scale check in
  `RequireDescribed`; extend its comment ("… or declares it in a unit whose symbol is not ASCII and that has no ASCII
  key").

- [ ] **Step 5: The quotient key in `unit_quotient` (`trace.hpp`).** When either operand's `asciiText` is not empty,
  build the quotient's `asciiText` from `view_ascii(over)`, `/`, `view_ascii(under)`, bracketing the divisor when
  `compound_unit_symbol(view_ascii(under))` holds. Refuse (`std::nullopt`) when either key contains `/` or the key
  does not fit `SymbolCapacity`, exactly as the symbol is refused. Write both through one local helper lambda or
  function that appends `over`, `/`, an optionally bracketed `under` into a `Symbol`, so the symbol and the key use
  one piece of code; the existing capacity check moves into it.

- [ ] **Step 6: Every unit in the repository passes.** Build everything (gate 1). Any test, example or guide unit
  with a non-ASCII symbol and no key now fails to compile with the new message: give it a key (`"ug/L"`, `"degC"`,
  and so on), never weaken the check. List each one you keyed in the report.

- [ ] **Step 7: Negative tests.** Copy each `test/negative/scaled_scalar_unit_without_symbol_{measured,constant,snap,var}.cpp`
  to `unit_without_ascii_key_{…}.cpp`, replacing the scaled scalar unit with:

```cpp
inline constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                   .magnitudeNumerator = 1,
                                                   .magnitudeDenominator = 1'000'000,
                                                   .symbolText = formula::symbol("\xc2\xb5g/L") };
```

  (adjusting the dimension spelling to the header's), and each header comment to say a non-ASCII unit without a key
  must not compile. Register each with `"formula: a unit whose symbol is not ASCII must declare an ASCII key"` and the
  same `EXPECT_COUNT`/`REJECT` options the copied case uses.

- [ ] **Step 8: Docs and CHANGELOG.**
    - `README.md`, in the units part: one short paragraph: serialise a unit with `formula::view_ascii(unit)`, its
      stable ASCII key, not with its display symbol; a unit whose symbol is not ASCII declares the key as
      `.asciiText`.
    - `docs/dimensions.md`: `asciiText` in the `Unit` fields table (:124–135); a short section on the key,
      `view_ascii`, `has_ascii_key`, `checked_ascii_symbol`, the refusal, and the four built-in keys.
    - CHANGELOG `### Added`: the key and its functions; `### Changed`: `**Breaking:** a unit whose symbol is not
      ASCII must declare an ASCII key …`.

- [ ] **Step 9: Verify.** Gate 1; gate 2 with `-Filter "unit_without_ascii_key|scaled_scalar_unit_without_symbol"`.
  Expected delta: +4 tests, +4 negatives.

- [ ] **Step 10: Commit** `feat(unit): a stable ASCII key per unit`.

---

### Task 3: One-sided bounds, and bounds known at run time (#25)

Spec §2.3.

**Files:**
- Modify: `include/formula-cpp/unit.hpp` (`Bounds` :28, `bounds()` :46, `checked_within_bounds` :701)
- Modify: `include/formula-cpp/measured.hpp` (after the `Measured` `checked_within_bounds` :229, and after
  `within_bounds` :257)
- Modify: `test/unit_tests.cpp` (:801–802, :936–988), `test/measured_tests.cpp`
- Modify: `docs/dimensions.md` (:291–340), `CHANGELOG.md`

**Interfaces:**
- Produces:
    - `Bounds { bool lowPresent = false; bool highPresent = false; std::int64_t lowNumerator = 0, lowDenominator = 1, highNumerator = 0, highDenominator = 1; }`
    - `constexpr Bounds bounds(std::int64_t lowNumerator, std::int64_t lowDenominator, std::int64_t highNumerator, std::int64_t highDenominator) noexcept;` (unchanged)
    - `constexpr Bounds at_least(std::int64_t lowNumerator, std::int64_t lowDenominator) noexcept;`
    - `constexpr Bounds at_most(std::int64_t highNumerator, std::int64_t highDenominator) noexcept;`
    - `constexpr std::expected<BoundsCheck, ArithmeticError> checked_within(Rational magnitude, std::optional<Rational> lowEnd, std::optional<Rational> highEnd) noexcept;`
    - `constexpr BoundsCheck within(Rational magnitude, std::optional<Rational> lowEnd, std::optional<Rational> highEnd);` (throws)
    - `template <Described Q> constexpr std::expected<BoundsCheck, ArithmeticError> checked_within(Measured<Q> measured, std::optional<Rational> lowEnd, std::optional<Rational> highEnd) noexcept;`
    - `template <Described Q> constexpr BoundsCheck within(Measured<Q> measured, std::optional<Rational> lowEnd, std::optional<Rational> highEnd);` (throws)

- [ ] **Step 1: Write the failing tests** in `test/unit_tests.cpp`:

```cpp
TEST_CASE("checked_within: either end, both, or neither", "[unit][bounds]")
{
    using formula::BoundsCheck;
    using formula::Rational;
    constexpr std::optional<Rational> none {};
    STATIC_REQUIRE(*formula::checked_within(Rational { 5 }, Rational { 0 }, none) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { -1 }, Rational { 0 }, none) == BoundsCheck::BelowMinimum);
    STATIC_REQUIRE(*formula::checked_within(Rational { 0 }, Rational { 0 }, none) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 21 }, none, Rational { 20 }) == BoundsCheck::AboveMaximum);
    STATIC_REQUIRE(*formula::checked_within(Rational { 20 }, none, Rational { 20 }) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 7 }, Rational { 0 }, Rational { 20 }) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within(Rational { 7 }, none, none) == BoundsCheck::NotChecked);
    STATIC_REQUIRE(formula::checked_within(Rational { 7 }, Rational { 20 }, Rational { 0 }).error()
                   == formula::ArithmeticError::DomainError);
    REQUIRE(formula::within(Rational { 7 }, none, Rational { 5 }) == BoundsCheck::AboveMaximum);
    REQUIRE_THROWS_AS(formula::within(Rational { 7 }, Rational { 9 }, Rational { 5 }), formula::ArithmeticException);
}

TEST_CASE("checked_within_bounds: a unit declared with at_least or at_most", "[unit][bounds]")
{
    using formula::BoundsCheck;
    using formula::Rational;
    constexpr formula::Unit NonNegative { .dimension = formula::dim::Scalar,
                                          .symbolText = formula::symbol("x"),
                                          .bounds = formula::at_least(0, 1) };
    constexpr formula::Unit AtMostTwenty { .dimension = formula::dim::Scalar,
                                           .symbolText = formula::symbol("y"),
                                           .bounds = formula::at_most(20, 1) };
    STATIC_REQUIRE(NonNegative.bounds.lowPresent && !NonNegative.bounds.highPresent);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { 1'000'000 }, NonNegative) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { -1, 2 }, NonNegative) == BoundsCheck::BelowMinimum);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { -1'000'000 }, AtMostTwenty) == BoundsCheck::WithinBounds);
    STATIC_REQUIRE(*formula::checked_within_bounds(Rational { 41, 2 }, AtMostTwenty) == BoundsCheck::AboveMaximum);
}
```

  In `test/measured_tests.cpp`, using the file's existing measured quantity:

```cpp
TEST_CASE("checked_within on a measurement: absent is NotMeasured", "[measured][bounds]")
{
    // <Q> is the file's existing scalar or mass quantity.
    constexpr formula::Measured<Q> absentReading {};
    STATIC_REQUIRE(*formula::checked_within(absentReading, formula::Rational { 0 }, std::nullopt)
                   == formula::BoundsCheck::NotMeasured);
    constexpr formula::Measured<Q> present { formula::Rational { 3 } };
    STATIC_REQUIRE(*formula::checked_within(present, formula::Rational { 0 }, std::nullopt)
                   == formula::BoundsCheck::WithinBounds);
    REQUIRE(formula::within(present, std::nullopt, formula::Rational { 2 }) == formula::BoundsCheck::AboveMaximum);
}
```

- [ ] **Step 2: Run them and watch them fail** (quick loop, `-Filter "bounds|within"`).

- [ ] **Step 3: Implement in `unit.hpp`.**
    - `Bounds`: replace `present` with

```cpp
    /// Whether a minimum was declared. `false` for a unit with no lower limit.
    bool lowPresent = false;
    /// Whether a maximum was declared. `false` for a unit with no upper limit.
    bool highPresent = false;
```

      and update the struct comment: either end, both, or neither.
    - `bounds()` returns `{ true, true, … }`; add `at_least` (`{ true, false, lowNumerator, lowDenominator, 0, 1 }`) and
      `at_most` (`{ false, true, 0, 1, highNumerator, highDenominator }`), each with a `///` line.
    - Before `checked_within_bounds`, add `checked_within` (needs `<optional>`):

```cpp
/// Checks @p magnitude against limits held at run time -- a specification row, a catalogue entry -- either of which
/// may be absent. Both ends are inclusive. With no end there is nothing to check (`NotChecked`, never
/// `WithinBounds`); a lower end above the upper one is a malformed pair of limits, refused as `DomainError`.
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within(
    Rational magnitude, std::optional<Rational> lowEnd, std::optional<Rational> highEnd) noexcept
{
    if (!lowEnd && !highEnd)
        return BoundsCheck::NotChecked;
    if (lowEnd && highEnd && *lowEnd > *highEnd)
        return std::unexpected { ArithmeticError::DomainError };
    if (lowEnd && magnitude < *lowEnd)
        return BoundsCheck::BelowMinimum;
    if (highEnd && magnitude > *highEnd)
        return BoundsCheck::AboveMaximum;
    return BoundsCheck::WithinBounds;
}

/// @throws ArithmeticException when the checked form would report an error.
[[nodiscard]] constexpr BoundsCheck within(Rational magnitude, std::optional<Rational> lowEnd,
                                           std::optional<Rational> highEnd)
{
    return detail::or_throw(checked_within(magnitude, lowEnd, highEnd));
}
```

    - `checked_within_bounds(Rational, Unit)` builds each declared end with `Rational::make` (an error returns as
      today), leaves an undeclared one `std::nullopt`, and returns `checked_within(magnitude, lowEnd, highEnd)`. Keep
      its comment about a malformed unit, moved to say where the refusal now happens.

- [ ] **Step 4: `measured.hpp`.** After the `Measured` `checked_within_bounds`:

```cpp
/// Checks a measurement against limits held at run time, as `checked_within` on a `Rational`; an absent
/// measurement is `BoundsCheck::NotMeasured`.
template <Described Q>
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within(
    Measured<Q> measured, std::optional<Rational> lowEnd, std::optional<Rational> highEnd) noexcept
{
    if (measured.is_absent())
        return BoundsCheck::NotMeasured;
    return checked_within(measured.value(), lowEnd, highEnd);
}
```

  and after `within_bounds`, the throwing `within(Measured<Q>, …)` built on `detail::or_throw`, with the same comment
  shape as `within_bounds`.

- [ ] **Step 5: Move the old readers.** `test/unit_tests.cpp:801–802` read `lowPresent`/`highPresent`; the positional
  `Bounds` at :976 and :986 become designated (`.bounds = { .lowPresent = true, .highPresent = true, .lowNumerator = 100, … }`)
  keeping each test's meaning (the `false` one has neither end). `git grep -n "\.present\b"` must find nothing in
  `include`, `test`, `examples`, `tools` or `docs` (outside `docs/superpowers`).

- [ ] **Step 6: Docs and CHANGELOG.** `docs/dimensions.md` bounds section: per-end presence, `at_least`/`at_most`, and
  `checked_within` for limits read at run time (a one-sided example: a lower limit from a catalogue row). CHANGELOG
  `### Added`: `at_least`, `at_most`, `checked_within`, `within`; `### Changed`: `**Breaking:** \`Bounds::present\` is
  replaced by \`lowPresent\` and \`highPresent\` …`.

- [ ] **Step 7: Verify.** Gate 1. Expected delta: +3 tests.

- [ ] **Step 8: Commit** `feat(unit): one-sided bounds and limits checked at run time`.

---

### Task 4: Declared decimals per quantity, and `same_unit` (#26)

Spec §2.4.

**Files:**
- Modify: `include/formula-cpp/quantity.hpp` (`Quantity` :96, `quantity_base_of` :123)
- Modify: `include/formula-cpp/unit.hpp` (a `with_decimals` helper and `same_unit`)
- Modify: `include/formula-cpp/trace.hpp` (`same_scale_and_symbol` :2193)
- Modify: tests: `test/quantity_tests.cpp` (or the file holding `Quantity` tests), `test/measured_tests.cpp`,
  `test/number_text_tests.cpp`, the `std::format` test file, and one trace test file
- Modify: `docs/quantities.md`, `docs/dimensions.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: `Unit::asciiText`, `view_ascii` (Task 2).
- Produces:
    - `template <typename Tag, detail::FixedString Symbol, detail::FixedString Description, Unit U, DecimalPlaces Places = declared_decimals(U)> struct Quantity;`
    - `[[nodiscard]] constexpr bool same_unit(Unit const& leftUnit, Unit const& rightUnit) noexcept;`
    - `[[nodiscard]] constexpr bool detail::same_scale(Unit const& leftUnit, Unit const& rightUnit) noexcept;`
    - `[[nodiscard]] constexpr Unit detail::with_decimals(Unit declared, DecimalPlaces places) noexcept;`

- [ ] **Step 1: Write the failing tests.** In the `Quantity` test file:

```cpp
inline constexpr formula::Unit Milliampere { .dimension = formula::dim::Current,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000,
                                             .symbolText = formula::symbol("mA"),
                                             .decimals = 0 };
struct FineCurrent: formula::Quantity<FineCurrent, "I_f", "a current read to a tenth of a milliampere", Milliampere,
                                      formula::DecimalPlaces { 1 }>
{
};
struct CoarseCurrent: formula::Quantity<CoarseCurrent, "I_c", "a current read to whole milliamperes", Milliampere>
{
};

TEST_CASE("Quantity: its own decimal places, in the unit it shares", "[quantity][decimals]")
{
    STATIC_REQUIRE(formula::Describe<FineCurrent>::unit.decimals == 1);
    STATIC_REQUIRE(formula::Describe<CoarseCurrent>::unit.decimals == 0);
    STATIC_REQUIRE(formula::same_unit(formula::Describe<FineCurrent>::unit, Milliampere));
    STATIC_REQUIRE(!(formula::Describe<FineCurrent>::unit == Milliampere));
    STATIC_REQUIRE(formula::Describe<CoarseCurrent>::unit == Milliampere);
}

TEST_CASE("same_unit: what a unit is, not its decimals or bounds", "[unit][same_unit]")
{
    constexpr formula::Unit Base { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                   .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm") };
    constexpr formula::Unit OtherDecimals { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                            .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                            .decimals = 1, .bounds = formula::at_least(0, 1) };
    STATIC_REQUIRE(formula::same_unit(Base, OtherDecimals));
    STATIC_REQUIRE(!formula::same_unit(Base, formula::unit::Metre));                 // magnitude
    STATIC_REQUIRE(!formula::same_unit(formula::unit::Kelvin, formula::unit::Celsius)); // offset
    constexpr formula::Unit OtherSymbol { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 1000, .symbolText = formula::symbol("MM") };
    STATIC_REQUIRE(!formula::same_unit(Base, OtherSymbol));
    constexpr formula::Unit OtherKey { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                       .asciiText = formula::symbol("millimetre") };
    STATIC_REQUIRE(!formula::same_unit(Base, OtherKey));
    constexpr formula::Unit SameKeySpelledOut { .dimension = formula::dim::Length, .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1000, .symbolText = formula::symbol("mm"),
                                                .asciiText = formula::symbol("mm") };
    STATIC_REQUIRE(formula::same_unit(Base, SameKeySpelledOut)); // the same key, declared or read off the symbol
    constexpr formula::Unit Area { .dimension = formula::dim::Length * formula::dim::Length,
                                   .magnitudeNumerator = 1, .magnitudeDenominator = 1000,
                                   .symbolText = formula::symbol("mm") };
    STATIC_REQUIRE(!formula::same_unit(Base, Area)); // dimension
}
```

  Spell `dim::Current`, `Kelvin` and the dimension product as the headers do. `same_unit` compares the keys with
  `view_ascii` (a key declared equal to the symbol is the same key as none declared): rule this in the report.

  In `test/measured_tests.cpp`: `checked_round_to_declared(Measured<FineCurrent>{ Rational { 1234, 100 } }, RoundingMode::HalfAwayFromZero)`
  is `123/10`, and the same for `CoarseCurrent` is `12`. In `test/number_text_tests.cpp` and the `std::format` tests:
  `Measured<FineCurrent>{ 12.34_r }` writes `12.3 mA`, `CoarseCurrent` writes `12 mA` (use each file's existing call
  shape). In one trace test file (the one testing `Measured` inputs to a formula): a formula `FineCurrent * 2`
  traced with input `12.34` shows its input at one place, matching how the file pins a 1-place unit's trace.

- [ ] **Step 2: Run them and watch them fail** (quick loop, `-Filter "decimals|same_unit"`). Expected: "too many
  template arguments for class template 'Quantity'".

- [ ] **Step 3: Implement.**
    - `unit.hpp`, in `namespace detail`:

```cpp
    /// @p declared with its display precision replaced by @p places: how a quantity that declares its own places
    /// holds its unit.
    [[nodiscard]] constexpr Unit with_decimals(Unit declared, DecimalPlaces places) noexcept
    {
        declared.decimals = places.value;
        return declared;
    }

    /// Whether two units put values on one scale: the same dimension, factor and offset, compared as declared.
    [[nodiscard]] constexpr bool same_scale(Unit const& leftUnit, Unit const& rightUnit) noexcept
    {
        return leftUnit.dimension == rightUnit.dimension
               && leftUnit.magnitudeNumerator == rightUnit.magnitudeNumerator
               && leftUnit.magnitudeDenominator == rightUnit.magnitudeDenominator
               && leftUnit.offsetNumerator == rightUnit.offsetNumerator
               && leftUnit.offsetDenominator == rightUnit.offsetDenominator;
    }
```

      and public, after `has_ascii_key`:

```cpp
/// Whether two units are the same unit: the same scale (dimension, factor, offset), the same display symbol and the
/// same key (`view_ascii`). Their declared decimals and bounds may differ -- a quantity read to a tenth of a
/// milliampere and one read to whole milliamperes are both in milliamperes -- which is what `==`, comparing every
/// member, does not answer. Use this, or `view_ascii`, to key a table by unit.
[[nodiscard]] constexpr bool same_unit(Unit const& leftUnit, Unit const& rightUnit) noexcept
{
    return detail::same_scale(leftUnit, rightUnit) && view(leftUnit.symbolText) == view(rightUnit.symbolText)
           && view_ascii(leftUnit) == view_ascii(rightUnit);
}
```

    - `trace.hpp` `same_scale_and_symbol` becomes `return same_scale(leftUnit, rightUnit) && view(leftUnit.symbolText) == view(rightUnit.symbolText);`
      and its comment adds that it ignores the ASCII key, which is never displayed.
    - `quantity.hpp`: `Quantity` gains `DecimalPlaces Places = declared_decimals(U)` and
      `static constexpr Unit unit = detail::with_decimals(U, Places);` with a comment: `U`, with its declared places
      replaced when the quantity declares its own; `quantity_base_of` gains the parameter in both its parameter and
      return types. Document the parameter in `Quantity`'s comment, with the milliampere example.

- [ ] **Step 4: Docs and CHANGELOG.** `docs/quantities.md`: the fifth parameter, with the milliampere example and the
  note that `same_unit` answers whether two quantities share a unit. `docs/dimensions.md`: `same_unit` beside `==`.
  CHANGELOG `### Added`: both.

- [ ] **Step 5: Verify.** Gate 1. Expected delta: +2 tests, plus each test case the step-1 additions to the measured,
  number text, format and trace files created (state how many).

- [ ] **Step 6: Commit** `feat(quantity): declared decimals per quantity, and same_unit`.

---

## Lane B — numbers (#24, #23)

Lane B tasks run in agent-owned worktrees on `feature/unit-text-bounds-parsing-numbers`. Each begins with
`git reset --hard feature/unit-text-bounds-parsing-numbers`. `$T` is the worktree.

---

### Task 5: Decimal text parsed at run time (#24)

Spec §3.1.

**Files:**
- Modify: `include/formula-cpp/detail/checked_int.hpp` (`pow10` :233, the 128-bit `mul_pow10` :386)
- Modify: `include/formula-cpp/rational.hpp` (`from_decimal` :128, `rational_from_spelling` :556, `operator""_r` :633)
- Create: `test/decimal_parsing_tests.cpp`; register it in `test/CMakeLists.txt` beside `rational_literal_tests.cpp`
- Modify: `test/rational_literal_tests.cpp`, `test/rational_tests.cpp`
- Modify: `test/negative/rational_literal_out_of_range.cpp`, `rational_literal_too_many_places.cpp`,
  `rational_literal_exponent_out_of_range.cpp`
- Modify: `docs/numbers.md` (:24–31, :94–128), `CHANGELOG.md`

**Interfaces:**
- Produces:
    - `struct ParsedDecimal { Rational value; std::int32_t places; constexpr bool operator==(ParsedDecimal const&) const noexcept = default; };`
    - `[[nodiscard]] constexpr std::expected<ParsedDecimal, ArithmeticError> parse_decimal_text(std::string_view spelling) noexcept;`
    - `[[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> Rational::from_decimal_text(std::string_view spelling) noexcept;`
    - `[[nodiscard]] constexpr std::optional<Int128> detail::pow10_wide(int exponent) noexcept;` (0 to 38)

- [ ] **Step 1: Write the failing tests** in `test/decimal_parsing_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <limits>
#include <string>
#include <string_view>

using formula::ArithmeticError;
using formula::ParsedDecimal;
using formula::Rational;

namespace
{
    constexpr bool parses_as(std::string_view spelling, Rational expectedValue, std::int32_t expectedPlaces)
    {
        std::expected<ParsedDecimal, ArithmeticError> const parsed = formula::parse_decimal_text(spelling);
        return parsed.has_value() && parsed->value == expectedValue && parsed->places == expectedPlaces;
    }

    constexpr bool refused_as(std::string_view spelling, ArithmeticError expectedError)
    {
        std::expected<ParsedDecimal, ArithmeticError> const parsed = formula::parse_decimal_text(spelling);
        return !parsed.has_value() && parsed.error() == expectedError;
    }
} // namespace

TEST_CASE("parse_decimal_text: the value and the places as typed", "[rational][text]")
{
    STATIC_REQUIRE(parses_as("2.400", Rational { 12, 5 }, 3));
    STATIC_REQUIRE(parses_as("2.4", Rational { 12, 5 }, 1));
    STATIC_REQUIRE(parses_as("7", Rational { 7 }, 0));
    STATIC_REQUIRE(parses_as("1.5e3", Rational { 1500 }, 0));
    STATIC_REQUIRE(parses_as("15e-1", Rational { 3, 2 }, 1));
    STATIC_REQUIRE(parses_as("1e-3", Rational { 1, 1000 }, 3));
    STATIC_REQUIRE(parses_as("2.5E+2", Rational { 250 }, 0));
    STATIC_REQUIRE(parses_as("-27.3", Rational { -273, 10 }, 1));
    STATIC_REQUIRE(parses_as("+0.0", Rational {}, 1));
    STATIC_REQUIRE(parses_as("-0", Rational {}, 0));
    STATIC_REQUIRE(parses_as("-0.0", Rational {}, 1));
    STATIC_REQUIRE(parses_as(".5", Rational { 1, 2 }, 1));
    STATIC_REQUIRE(parses_as("5.", Rational { 5 }, 0));
    STATIC_REQUIRE(parses_as("007", Rational { 7 }, 0));
    STATIC_REQUIRE(parses_as("0e99999", Rational {}, 0));
}

TEST_CASE("parse_decimal_text: text that is not a decimal is DomainError", "[rational][text]")
{
    for (std::string_view const spelling: { "", " 1", "1 ", "1,5", "1'000", "1_000", "1.2.3", "0x1F", "inf", "nan",
                                            "1e", "1e+", "e5", "+", "-", ".", "+-1", "1e2.5", "--1" })
    {
        CAPTURE(spelling);
        REQUIRE(refused_as(spelling, ArithmeticError::DomainError));
    }
}

TEST_CASE("parse_decimal_text: a value no Rational holds is Overflow", "[rational][text]")
{
    STATIC_REQUIRE(parses_as("170141183460469231731687303715884105727",
                             Rational { std::numeric_limits<formula::Int128>::max() }, 0));
    STATIC_REQUIRE(refused_as("170141183460469231731687303715884105728", ArithmeticError::Overflow));
    STATIC_REQUIRE(parses_as("1e38", *Rational::from_decimal(1, 38), 0));
    STATIC_REQUIRE(refused_as("1e39", ArithmeticError::Overflow));
    STATIC_REQUIRE(refused_as("1e-39", ArithmeticError::Overflow));
    STATIC_REQUIRE(refused_as("1e1001", ArithmeticError::Overflow));
    // A thousand fractional zeros before a digit: refused, not a wrong value and not a hang.
    std::string const deepFraction = "0." + std::string(1000, '0') + "1";
    REQUIRE(refused_as(deepFraction, ArithmeticError::Overflow));
    // Trailing fractional zeros cost nothing, however many.
    std::string const manyTrailingZeros = "1." + std::string(1000, '0');
    std::expected<ParsedDecimal, ArithmeticError> const trailing = formula::parse_decimal_text(manyTrailingZeros);
    REQUIRE(trailing.has_value());
    REQUIRE(trailing->value == Rational { 1 });
    REQUIRE(trailing->places == 1000);
}

TEST_CASE("Rational::from_decimal_text: the value parse_decimal_text reads", "[rational][text]")
{
    STATIC_REQUIRE(*Rational::from_decimal_text("2.400") == Rational { 12, 5 });
    STATIC_REQUIRE(Rational::from_decimal_text("2,4").error() == ArithmeticError::DomainError);
    std::string const fromCsv = "0.0213";
    std::expected<Rational, ArithmeticError> const atRunTime = Rational::from_decimal_text(fromCsv);
    REQUIRE(atRunTime.has_value());
    REQUIRE(*atRunTime == Rational { 213, 10'000 });
}
```

  Check every expected value above against the spec's syntax before relying on it; `"1e2.5"` is refused because an
  exponent is digits only.

  In `test/rational_tests.cpp`:

```cpp
TEST_CASE("from_decimal: exponents up to 38 either way", "[rational]")
{
    STATIC_REQUIRE(*Rational::from_decimal(1, -19) == Rational::make(1, *formula::detail::pow10_wide(19)).value());
    STATIC_REQUIRE(Rational::from_decimal(1, -38).has_value());
    STATIC_REQUIRE(Rational::from_decimal(1, -39).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(Rational::from_decimal(1, 38).has_value());
    STATIC_REQUIRE(Rational::from_decimal(1, 39).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(*Rational::from_decimal(10, -39) == *Rational::from_decimal(1, -38)); // trailing zeros fold first
}
```

  In `test/rational_literal_tests.cpp`:

```cpp
TEST_CASE("_r: a literal wider than 64 bits", "[rational][literal]")
{
    STATIC_REQUIRE(12'345'678'901'234'567'890_r == 1'234'567'890_r * 10'000'000'000_r + 1'234'567'890_r);
    STATIC_REQUIRE(0.000'000'000'000'000'000'1_r == *Rational::from_decimal(1, -19));
}
```

- [ ] **Step 2: Run them and watch them fail** (quick loop, `-Filter "text|from_decimal|literal"`).

- [ ] **Step 3: `checked_int.hpp`.** Add, in the 128-bit section, before the 128-bit `mul_pow10`:

```cpp
/// `10^exponent` on 128 bits, for `0 <= exponent <= 38`; `nullopt` otherwise. 10^38 is the largest power of ten
/// `Int128` holds.
[[nodiscard]] constexpr std::optional<Int128> pow10_wide(int exponent) noexcept
{
    if (exponent < 0 || exponent > 38)
        return std::nullopt;
    Int128 power { 1 };
    for (int multiplied = 0; multiplied < exponent; ++multiplied)
        power = power * Int128 { 10 };
    return power;
}
```

  and the 128-bit `mul_pow10` uses `pow10_wide` (exponents 0 to 38); update its comment. It has one caller,
  `from_decimal`.

- [ ] **Step 4: `from_decimal`.** Fold the mantissa's trailing zeros into a negative exponent first
  (`while (exponent < 0 && mantissa % 10 == 0) { mantissa /= 10; ++exponent; }`, after the zero check), then the
  negative branch uses `detail::pow10_wide(-exponent)`. Its comment states the range: 10^-38 to 10^38.

- [ ] **Step 5: The shared parser.** In `rational.hpp`, after the class and before `namespace detail` holding the
  literal sentinels: `ParsedDecimal` (public, `///` on each member: `value` is the exact value, `places` the digits
  after the point minus the exponent, never below 0), then in `namespace detail`:

```cpp
    /// Which spelling `parse_decimal` reads: run-time text, or a `_r` literal's spelling.
    enum class DecimalSyntax : std::uint8_t
    {
        /// An optional sign, digits with at most one point, an optional exponent; leading zeros are fine.
        Text,
        /// What C++ hands a literal operator: no sign, digit separators (`'`), and no leading zero that C++ reads
        /// as octal, hexadecimal or binary.
        Literal,
    };

    /// The exact value of a decimal spelling, and its places as typed. `DomainError` for a spelling that is not a
    /// decimal in @p syntax, `Overflow` for a value no `Rational` holds. The mantissa accumulates in 128 bits;
    /// fractional zeros are deferred, so trailing ones cost nothing.
    [[nodiscard]] constexpr std::expected<ParsedDecimal, ArithmeticError> parse_decimal(std::string_view spelling,
                                                                                    DecimalSyntax syntax) noexcept
    {
        bool const literal = syntax == DecimalSyntax::Literal;
        std::size_t at = 0;
        bool negative = false;
        if (!literal && at < spelling.size() && (spelling[at] == '+' || spelling[at] == '-'))
        {
            negative = spelling[at] == '-';
            ++at;
        }
        if (literal && spelling.size() > 1 && spelling[0] == '0'
            && spelling.find_first_of(".eE") == std::string_view::npos)
            return std::unexpected { ArithmeticError::DomainError }; // 017, 0x1F, 0b101
        Rational::Int mantissa { 0 };
        std::int64_t decimalScale = 0;   // decimal exponent the digits carry
        std::int64_t pendingZeros = 0;   // fractional zeros not yet multiplied in
        std::int64_t fractionDigits = 0; // digits after the point, as typed
        bool inFraction = false;
        bool sawDigit = false;
        for (; at < spelling.size() && spelling[at] != 'e' && spelling[at] != 'E'; ++at)
        {
            char const symbolAt = spelling[at];
            if (literal && symbolAt == '\'')
                continue;
            if (symbolAt == '.')
            {
                if (inFraction)
                    return std::unexpected { ArithmeticError::DomainError };
                inFraction = true;
                continue;
            }
            if (symbolAt < '0' || symbolAt > '9')
                return std::unexpected { ArithmeticError::DomainError };
            sawDigit = true;
            int const digitValue = symbolAt - '0';
            if (inFraction)
                ++fractionDigits;
            if (inFraction && digitValue == 0)
            {
                ++pendingZeros;
                continue;
            }
            for (std::int64_t zero = 0; zero <= pendingZeros; ++zero) // the zeros, then this digit's place
            {
                std::optional<Rational::Int> const shifted = mul_checked_or_none(mantissa, Rational::Int { 10 });
                if (!shifted)
                    return std::unexpected { ArithmeticError::Overflow };
                std::optional<Rational::Int> const placed =
                    add_checked_or_none(*shifted, Rational::Int { zero == pendingZeros ? digitValue : 0 });
                if (!placed)
                    return std::unexpected { ArithmeticError::Overflow };
                mantissa = *placed;
            }
            if (inFraction)
                decimalScale -= pendingZeros + 1;
            pendingZeros = 0;
        }
        if (!sawDigit)
            return std::unexpected { ArithmeticError::DomainError };
        std::int64_t exponentValue = 0;
        if (at < spelling.size()) // at an e or E
        {
            ++at;
            bool exponentNegative = false;
            if (at < spelling.size() && (spelling[at] == '+' || spelling[at] == '-'))
            {
                exponentNegative = spelling[at] == '-';
                ++at;
            }
            bool sawExponentDigit = false;
            for (; at < spelling.size(); ++at)
            {
                if (literal && spelling[at] == '\'')
                    continue;
                if (spelling[at] < '0' || spelling[at] > '9')
                    return std::unexpected { ArithmeticError::DomainError };
                sawExponentDigit = true;
                if (exponentValue <= 1'000) // bounded while read: anything above is refused below
                    exponentValue = exponentValue * 10 + (spelling[at] - '0');
            }
            if (!sawExponentDigit)
                return std::unexpected { ArithmeticError::DomainError };
            if (exponentNegative)
                exponentValue = -exponentValue;
        }
        std::int64_t const placesTyped = fractionDigits - exponentValue;
        if (placesTyped > std::numeric_limits<std::int32_t>::max())
            return std::unexpected { ArithmeticError::Overflow };
        std::int32_t const places = placesTyped < 0 ? 0 : static_cast<std::int32_t>(placesTyped);
        if (mantissa == 0)
            return ParsedDecimal { Rational {}, places };
        if (exponentValue > 1'000 || exponentValue < -1'000) // as `_r` has always refused
            return std::unexpected { ArithmeticError::Overflow };
        std::int64_t const totalScale = decimalScale + exponentValue;
        if (totalScale > 1'000 || totalScale < -1'000) // keeps the narrowing to int safe; from_decimal decides the rest
            return std::unexpected { ArithmeticError::Overflow };
        std::expected<Rational, ArithmeticError> const made =
            Rational::from_decimal(mantissa, static_cast<int>(totalScale));
        if (!made)
            return std::unexpected { made.error() };
        return ParsedDecimal { negative ? -*made : *made, places };
    }
```

  The mantissa is never negative here, so `-*made` cannot overflow. A zero value answers before the exponent is
  judged, so `0e99999` is 0; in literal mode that turns the old refusal of `0e1001_r` into 0, which the behaviour rule
  allows. Construct a `Rational` from an `Int128` with `Rational(Int)` (:72).

  Then the public functions, after `namespace detail` closes:

```cpp
/// Parses decimal text that arrives at run time -- a CSV import, a form field, a configuration value -- into its
/// exact value and the places it was typed to: `"2.400"` is 12/5 at 3 places, `"2.4"` 12/5 at 1. An optional sign,
/// digits with at most one point, an optional exponent (`e` or `E`, an optional sign, digits). `DomainError` for
/// anything else -- whitespace, a decimal comma, separators, `inf`, `nan` -- and `Overflow` for a value no `Rational`
/// holds. The same parser reads `_r` literals.
[[nodiscard]] constexpr std::expected<ParsedDecimal, ArithmeticError> parse_decimal_text(std::string_view spelling) noexcept
{
    return detail::parse_decimal(spelling, detail::DecimalSyntax::Text);
}
```

  `Rational::from_decimal_text` is declared in the class with a `///` comment ("the value `parse_decimal_text` reads")
  and defined after `parse_decimal_text`:

```cpp
constexpr std::expected<Rational, ArithmeticError> Rational::from_decimal_text(std::string_view spelling) noexcept
{
    std::expected<ParsedDecimal, ArithmeticError> const parsed = parse_decimal_text(spelling);
    if (!parsed)
        return std::unexpected { parsed.error() };
    return parsed->value;
}
```

- [ ] **Step 6: `_r` on the shared parser.** `rational_from_spelling` becomes:

```cpp
    consteval Rational rational_from_spelling(char const* spelling)
    {
        std::expected<ParsedDecimal, ArithmeticError> const parsed =
            parse_decimal(std::string_view { spelling }, DecimalSyntax::Literal);
        if (!parsed && parsed.error() == ArithmeticError::DomainError)
            formula_rational_literal_not_a_decimal();
        if (!parsed)
            formula_rational_literal_out_of_range();
        return parsed->value;
    }
```

  Update `operator""_r`'s comment: a spelling no `Rational` holds (more than 128 bits, or an exponent beyond 10^±38
  once trailing zeros fold) fails to compile.

- [ ] **Step 7: Move the negative tests that now compile.**
    - `rational_literal_out_of_range.cpp`: `170'141'183'460'469'231'731'687'303'715'884'105'728_r` (2^127, one past
      `Int128`'s maximum); comment says so.
    - `rational_literal_too_many_places.cpp`: `0.000'000'000'000'000'000'000'000'000'000'000'000'001_r` (10^-39);
      comment: a denominator above `Int128`'s range.
    - `rational_literal_exponent_out_of_range.cpp`: `1e39_r`; comment: 10^39 is an integer no `Rational` holds
      (`Int128` stops below 1.8 × 10^38).
    - Each existing `_r` test and every other `rational_literal_*` negative must pass unchanged.

- [ ] **Step 8: Docs and CHANGELOG.** `docs/numbers.md`: the literal limits become 128 bits and 10^±38 (the table
  at :119–122 and its prose); a new section *Decimal text at run time* with `parse_decimal_text`, `from_decimal_text`,
  the syntax, the errors, and places as typed, using `std::println` in any sample. CHANGELOG `### Added`: the parser;
  `### Changed`: `_r` and `from_decimal` take 128-bit values and exponents to ±38 (not breaking: only refusals turn
  into answers).

- [ ] **Step 9: Verify.** Gate 1; gate 2 with `-Filter "rational_literal"`. Expected delta: +6 tests, +0 negatives.

- [ ] **Step 10: Commit** `feat(rational): parse decimal text at run time, on the parser _r uses`.

---

### Task 6: `checked_transform` and `checked_combine` (#23)

Spec §3.2.

**Files:**
- Modify: `include/formula-cpp/measured.hpp` (after `combine` :165)
- Modify: `test/measured_tests.cpp`
- Create: `test/negative/checked_transform_plain_callback.cpp`, `test/negative/checked_combine_plain_callback.cpp`
- Modify: `test/CMakeLists.txt`, `docs/quantities.md` (:251–273), `CHANGELOG.md`

**Interfaces:**
- Produces:
    - `template <Described Q, typename F> constexpr std::expected<Measured<Q>, ArithmeticError> checked_transform(Measured<Q> measured, F function) noexcept(std::is_nothrow_invocable_v<F&, Rational>);`
    - `template <Described Result, Described Q, Described R, typename F> constexpr std::expected<Measured<Result>, ArithmeticError> checked_combine(Measured<Q> lhs, Measured<R> rhs, F function) noexcept(std::is_nothrow_invocable_v<F&, Rational, Rational>);`

  (`lhs` and `rhs` are the names `combine` already uses; keep them for symmetry.)

- [ ] **Step 1: Write the failing tests** in `test/measured_tests.cpp`, with the file's existing quantities (`Q`, `R`,
  and a third for the combined result):

```cpp
TEST_CASE("checked_transform: present, absent, and an error from the callback", "[measured][checked]")
{
    using formula::ArithmeticError;
    using formula::Rational;
    constexpr auto timesTen = [](Rational reading) noexcept { return formula::checked_mul(reading, Rational { 10 }); };
    constexpr std::expected<formula::Measured<Q>, ArithmeticError> diluted =
        formula::checked_transform(formula::Measured<Q> { Rational { 3, 2 } }, timesTen);
    STATIC_REQUIRE(diluted.has_value());
    STATIC_REQUIRE(diluted->value() == Rational { 15 });

    int calls = 0;
    auto const counting = [&calls](Rational reading) {
        ++calls;
        return std::expected<Rational, ArithmeticError> { reading };
    };
    std::expected<formula::Measured<Q>, ArithmeticError> const absent =
        formula::checked_transform(formula::Measured<Q> {}, counting);
    REQUIRE(absent.has_value());
    REQUIRE(absent->is_absent());
    REQUIRE(calls == 0);

    constexpr auto overflowing = [](Rational) noexcept {
        return std::expected<Rational, ArithmeticError> { std::unexpected { ArithmeticError::Overflow } };
    };
    constexpr std::expected<formula::Measured<Q>, ArithmeticError> refused =
        formula::checked_transform(formula::Measured<Q> { Rational { 1 } }, overflowing);
    STATIC_REQUIRE(refused.error() == ArithmeticError::Overflow);

    STATIC_REQUIRE(noexcept(formula::checked_transform(formula::Measured<Q> {}, timesTen)));
    STATIC_REQUIRE(!noexcept(formula::checked_transform(formula::Measured<Q> {}, counting)));
}

TEST_CASE("checked_combine: absent if either is absent; an error propagates", "[measured][checked]")
{
    using formula::ArithmeticError;
    using formula::Rational;
    constexpr auto multiply = [](Rational lhsReading, Rational rhsReading) noexcept {
        return formula::checked_mul(lhsReading, rhsReading);
    };
    constexpr std::expected<formula::Measured<Combined>, ArithmeticError> product =
        formula::checked_combine<Combined>(formula::Measured<Q> { Rational { 2 } }, formula::Measured<R> { Rational { 3 } }, multiply);
    STATIC_REQUIRE(product.has_value());
    STATIC_REQUIRE(product->value() == Rational { 6 });
    constexpr std::expected<formula::Measured<Combined>, ArithmeticError> oneAbsent =
        formula::checked_combine<Combined>(formula::Measured<Q> {}, formula::Measured<R> { Rational { 3 } }, multiply);
    STATIC_REQUIRE(oneAbsent.has_value());
    STATIC_REQUIRE(oneAbsent->is_absent());
    constexpr auto dividing = [](Rational lhsReading, Rational rhsReading) noexcept {
        return formula::checked_div(lhsReading, rhsReading);
    };
    constexpr std::expected<formula::Measured<Combined>, ArithmeticError> byZero =
        formula::checked_combine<Combined>(formula::Measured<Q> { Rational { 2 } }, formula::Measured<R> { Rational {} }, dividing);
    STATIC_REQUIRE(byZero.error() == ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(noexcept(formula::checked_combine<Combined>(formula::Measured<Q> {}, formula::Measured<R> {}, multiply)));
}
```

  Use the real name of the checked division in `rational.hpp` (it sits after `checked_mul`, :398–407).

- [ ] **Step 2: Run them and watch them fail** (quick loop, `-Filter "checked_transform|checked_combine"`).

- [ ] **Step 3: Implement** in `measured.hpp`, after `combine` (add `<type_traits>`):

```cpp
namespace detail
{
    /// Whether @p F, called with @p Arguments, returns exactly `std::expected<Rational, ArithmeticError>`.
    template <typename F, typename... Arguments>
    inline constexpr bool returns_checked_rational =
        std::is_same_v<std::invoke_result_t<F&, Arguments...>, std::expected<Rational, ArithmeticError>>;
} // namespace detail

/// `transform` for a callback that can fail: @p function returns `std::expected<Rational, ArithmeticError>` --
/// `checked_mul` and its siblings, say -- and its error is returned unchanged. An absent measurement stays absent
/// without calling @p function. `noexcept` when @p function is, so one line of arithmetic on a measurement can be
/// written under a no-throw rule.
template <Described Q, typename F>
[[nodiscard]] constexpr std::expected<Measured<Q>, ArithmeticError> checked_transform(Measured<Q> measured, F function)
    noexcept(std::is_nothrow_invocable_v<F&, Rational>)
{
    static_assert(detail::returns_checked_rational<F, Rational>,
                  "formula: a checked_transform callback must return std::expected<Rational, ArithmeticError>; use "
                  "transform for a callback that returns a Rational");
    if (measured.is_absent())
        return Measured<Q> {};
    std::expected<Rational, ArithmeticError> const transformed = function(measured.value());
    if (!transformed)
        return std::unexpected { transformed.error() };
    return Measured<Q> { *transformed };
}

/// `combine` for a callback that can fail, as `checked_transform` is for `transform`. Absent if either measurement
/// is absent, without calling @p function.
template <Described Result, Described Q, Described R, typename F>
[[nodiscard]] constexpr std::expected<Measured<Result>, ArithmeticError> checked_combine(Measured<Q> lhs,
                                                                                         Measured<R> rhs,
                                                                                         F function)
    noexcept(std::is_nothrow_invocable_v<F&, Rational, Rational>)
{
    static_assert(detail::returns_checked_rational<F, Rational, Rational>,
                  "formula: a checked_combine callback must return std::expected<Rational, ArithmeticError>; use "
                  "combine for a callback that returns a Rational");
    if (lhs.is_absent() || rhs.is_absent())
        return Measured<Result> {};
    std::expected<Rational, ArithmeticError> const combined = function(lhs.value(), rhs.value());
    if (!combined)
        return std::unexpected { combined.error() };
    return Measured<Result> { *combined };
}
```

  `measured.value()` throws only for an absent value, which is returned before it; if a compiler warns that a
  `noexcept` function calls a throwing one, read the value through the member that does not throw instead (look at
  `Measured`'s accessors, :98–108) rather than suppressing the warning.

- [ ] **Step 4: Negative tests.** `checked_transform_plain_callback.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// checked_transform's callback reports failure through std::expected; one returning a bare Rational belongs to
// transform. This must not compile.
#include <formula-cpp/measured.hpp>
// … the smallest quantity declaration the other measured negatives use …

int main()
{
    auto const plain = [](formula::Rational reading) { return reading; };
    auto const refused = formula::checked_transform(formula::Measured<Reading> { formula::Rational { 1 } }, plain);
    return refused.has_value() ? 0 : 1;
}
```

  and `checked_combine_plain_callback.cpp` likewise for `checked_combine`. Register them with
  `"formula: a checked_transform callback must return std::expected"` and
  `"formula: a checked_combine callback must return std::expected"`.

- [ ] **Step 5: Docs and CHANGELOG.** `docs/quantities.md` after `transform`/`combine`: the checked forms, with the
  dilution example (`checked_transform(reading, [](Rational r) noexcept { return checked_mul(r, Rational { 10 }); })`
  — rename `r` in prose samples if they are compiled by a docs test that includes consumer globals). CHANGELOG
  `### Added`.

- [ ] **Step 6: Verify.** Gate 1; gate 2 with `-Filter "checked_(transform|combine)"`. Expected delta: +2 tests,
  +2 negatives.

- [ ] **Step 7: Commit** `feat(measured): checked_transform and checked_combine`.

---

## Final tasks

### Task 7: Merge Lane B

Run by the controller in `D:\formula-cpp`.

- [ ] **Step 1:** `git merge --no-ff feature/unit-text-bounds-parsing-numbers -m "Merge decimal text parsing and checked measured arithmetic"`
  (message file with the sign-off).
- [ ] **Step 2:** Resolve `CHANGELOG.md` (keep every entry; within each subsection, breaking entries first) and
  `measured.hpp` (both additions kept, each where its task put it). An `sdd-implementer` resolves anything else.
- [ ] **Step 3: Verify.** Gate 1; gate 2 with `-Filter "."` (every negative on both compilers). The total must equal
  the Lane A total plus the Lane B deltas.

### Task 8: The guides, read through once

- [ ] **Step 1:** Read `docs/dimensions.md`, `docs/quantities.md`, `docs/numbers.md` and the README's units part
  end to end. Fix anything that contradicts what the branch now does (the old capacity, `Bounds::present`, the
  64-bit literal limits, a sentence saying units are keyed outside the library), and make the new sections agree in
  terms with each other (`ASCII key`, `checked_*`).
- [ ] **Step 2:** `git grep -n "SymbolCapacity = 16\|15 usable\|bounds.present\|64-bit mantissa"` finds nothing
  outside `docs/superpowers/` and released CHANGELOG sections.
- [ ] **Step 3: Verify.** Gate 1 (the docs output tests run there). Commit `docs: …` if anything changed.

### Task 9: Finish

The controller runs this task. Fix rounds go to `sdd-implementer`; the whole-branch review and each re-review go to
`sdd-reviewer`.

- [ ] **Step 1: Whole-branch review** of `931e81c..HEAD` against the spec; fix every finding, re-review.
- [ ] **Step 2: All eight presets.** `pwsh -NoProfile -File $S\windows-matrix.ps1 -Tree D:\formula-cpp` and
  `wsl bash /mnt/c/Users/c.parpart/AppData/Local/Temp/claude/D--formula-cpp/81d1061b-b25f-4c83-9f67-664a67264017/scratchpad/posix-matrix.sh --tree /mnt/d/formula-cpp`
  (from PowerShell). Every preset green.
- [ ] **Step 3: Docs builds.** Doxygen with warnings as errors, and `mkdocs build --strict`
  (`$S\docs-pages.sh`).
- [ ] **Step 4: Pull request.** Push the branch; open a draft PR titled
  `Unit keys and capacity, one-sided bounds, per-quantity decimals, decimal text parsing, checked measured arithmetic`
  whose body describes the changes in user terms, lists the breaking ones, and ends with `Closes #22` … `Closes #27`
  (one per line).
- [ ] **Step 5: CI.** Wait for every job; fix any failure on the branch. When green, `gh pr ready`. The owner merges.
