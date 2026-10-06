# Unit text, bounds, per-quantity decimals and run-time parsing — design

**Status:** draft, ready for review · **Date:** 2026-10-05 · **Owner:** Christian Parpart

Resolves issues #22, #23, #24, #25, #26 and #27: every issue open on 2026-10-05. All six came from porting a
laboratory application (concentrations in mg/L, µg/L and ng/L, dilution factors, currents in A and mA) onto 0.3.0.
They land together, on the branch `feature/unit-text-bounds-parsing` from `master` at `6ef3b19`, in one pull request
whose body closes all six.

0.4.0 is released, so these changes would ship in 0.5.0. Several break the public API, which a minor release before
1.0 may do; each such change is listed in §6.

## 1. Delivery

Parameter names in this document's signatures are descriptive; the plan gives the names the code uses, since a
test translation unit declares globals such as `value`, `text`, `low` and `high` that a parameter must not shadow.

| Group | Issues | Main files |
|---|---|---|
| Lane A: units | #27, #22, #25, #26 | `dimension.hpp`, `unit.hpp`, `quantity.hpp`, the unit checks, `trace.hpp` (`unit_quotient`), `measured.hpp` (bounds overloads) |
| Lane B: numbers | #24, then #23 | `rational.hpp`, `detail/checked_int.hpp`, `measured.hpp` (`checked_transform`, `checked_combine`) |
| Final tasks | — | merge Lane B; `docs/dimensions.md`, `docs/quantities.md`, `docs/numbers.md`, `README.md`; finish |

- Lane A and Lane B run in parallel, each in its own worktree. Lane B merges into the branch before the final tasks.
  Both lanes add to `measured.hpp` and to `CHANGELOG.md`; the merge task resolves those, keeping breaking entries
  first.
- Lane A runs in the order above: #22 needs #27's `SymbolError` and wider `Symbol`, and #26's `same_unit` compares
  #22's ASCII key.
- Each task updates the guide text it changes and adds its CHANGELOG entry under `## [Unreleased]`. A breaking
  entry says so in its first words. The final docs task covers what no single task owns: the README note, and a
  read-through of the three guides for consistency.
- No version bump: the release is a separate step.

## 2. Lane A: units

### 2.1 #27: symbol capacity, and building a symbol at run time

**Problem.**

- `SymbolCapacity` is 16 bytes including the terminator (`dimension.hpp:189`). Compound laboratory units are longer
  in UTF-8: `µmol/(L·min·kg)` is 18 bytes.
- `symbol(char const*)` (`dimension.hpp:230`) is `constexpr`. A symbol that does not fit is a compile error in a
  constant expression, but at run time it reaches `detail::formula_unit_symbol_too_long()`, which calls
  `std::abort()`. A unit built from run-time text (a catalogue, a configuration file) can terminate the process.

**Design.**

- `SymbolCapacity` becomes 32: 31 usable bytes.
  - `NumberTextCapacity` (128) still holds `LongestNumberText`, which grows from 97 to 113; its `static_assert`
    stays.
  - `Unit` grows by 16 bytes per `Symbol` it holds, and `Dimension` by 64 (four named-base symbols). No test pins
    these sizes.
  - `unit_quotient` (`trace.hpp`) already checks the capacity, so derived quotient symbols simply get more room.
- `symbol(char const*)` becomes `consteval`. A literal that does not fit stays a compile error, with the existing
  sentinel's name in the diagnostic. A call with a pointer that is not a constant expression no longer compiles, so
  no run-time path reaches `std::abort()`. The sentinel itself is unchanged: it is not `constexpr`, so reaching it
  during constant evaluation is the refusal, and a `consteval` caller can no longer reach it at run time. Its
  comment says so.
- `base_dimension` is already `consteval` and keeps its own length check.
- New `enum class SymbolError : std::uint8_t { TooLong, EmbeddedNull, NotAscii }`, with
  `describe(SymbolError) -> std::string_view`, in `dimension.hpp` beside `Symbol`. It is not an `ArithmeticError`:
  a symbol that does not fit is not an arithmetic failure.
- New `constexpr std::expected<Symbol, SymbolError> checked_symbol(std::string_view text) noexcept`:
  - `TooLong` when `text.size() + 1 > SymbolCapacity`;
  - `EmbeddedNull` when `text` contains `'\0'`, which would cut the stored symbol short without notice;
  - otherwise the symbol, byte for byte. UTF-8 is not validated: a symbol is bytes, as today.

**Tests.**

- `checked_symbol`: an 18-byte UTF-8 symbol fits; 31 bytes fit; 32 bytes give `TooLong`; an embedded NUL gives
  `EmbeddedNull`; the empty text gives the empty symbol. Each also checked in a `static_assert`.
- `describe(SymbolError)` for every enumerator.
- `symbol("µmol/(L·min·kg)")` compiles.
- Negative tests: `unit_symbol_too_long` moves to a 32-byte literal; a new one passes `symbol()` a run-time pointer
  and expects the `consteval` refusal. `base_dimension_name_too_long` moves to a 32-byte name.
- `test/number_text_tests.cpp`'s widest-symbol case is rebuilt for the new capacity, so it still tests the widest
  symbol.
- `docs/dimensions.md` (the capacity, at its two places) and the CHANGELOG's old capacity note stay accurate: the
  guide is updated, the released CHANGELOG entry is not.

### 2.2 #22: a stable ASCII key per unit

**Problem.** `Unit` has one text field, `symbolText`. Code that serialises units (JSON-schema annotations, payload
tags, a database column naming a unit, a client's unit choice) needs a key that stays the same when the display
text is restyled (`µ` or `u`, `·`, superscripts). `µg/L` has no ASCII spelling.

**Decision (owner).** A unit whose symbol is not ASCII must declare an ASCII key; this is refused at compile time,
so the accessor returns a plain `std::string_view`.

**Design.**

- `Unit` gains `Symbol asciiText {}`, directly after `symbolText`. Every `Unit {…}` in the repository uses
  designated initialisers, so no initialiser breaks. `Unit` stays structural.
- **ASCII** here means printable ASCII, bytes `0x20` to `0x7E`. The empty text is ASCII.
- New `constexpr std::string_view view_ascii(Unit const& unitValue) noexcept`: `asciiText` when it is not empty,
  otherwise `symbolText`. `view_ascii(Unit&&)` is deleted, as `view(Symbol&&)` is.
- New `constexpr bool has_ascii_key(Unit const& unitValue) noexcept`: whether `view_ascii` is ASCII, and
  `asciiText`, when set, is ASCII.
- New `constexpr std::expected<Symbol, SymbolError> checked_ascii_symbol(std::string_view text) noexcept`:
  `checked_symbol`'s checks, plus `NotAscii` for any byte outside `0x20`-`0x7E`. A `Unit` built at run time takes its
  key from it.
- New check `detail::RequireAsciiKey<Unit U>`, failing when `!has_ascii_key(U)`, with the message
  `formula: a unit whose symbol is not ASCII must declare an ASCII key (asciiText)`. It is asserted beside
  `detail::RequireNamedScaledScalar` at every place that asserts that check (24 today: a quantity's description,
  constants, rounding, table keys and results, and the rest), gated the same way, so one mistake gives one message.
- The built-in units with non-ASCII symbols get keys:

  | Unit | Symbol | `asciiText` |
  |---|---|---|
  | `PerMille` | `‰` | `permille` |
  | `Micrometre` | `µm` | `um` |
  | `DegreeCelsius` | `°C` | `degC` |
  | `DegreeFahrenheit` | `°F` | `degF` |

  The plan uses the names the header gives these units.
- `unit_quotient` (`trace.hpp`) builds the derived unit's key: when either operand has an `asciiText`, the quotient's
  `asciiText` is `view_ascii(over)`, `/`, and `view_ascii(under)`, bracketed by the same rule as the symbol. If the
  key does not fit, there is no quotient unit (`std::nullopt`), as when the symbol does not fit.
- The key is for serialising only. `render()`, traces, `number_text` and `std::format` keep writing `symbolText`.
- `README.md` gains a short note under units: serialise a unit with `view_ascii`, not its display symbol.

**Tests.**

- `view_ascii` of a plain ASCII unit returns its symbol; of each of the four built-ins, its key; of a coherent unit,
  the empty text.
- `has_ascii_key` is false for a unit with a non-ASCII symbol and no key, and for a non-ASCII key.
- `checked_ascii_symbol`: `NotAscii` for `µg/L`; `TooLong` and `EmbeddedNull` as `checked_symbol`.
- A trace quotient of a µm-based unit by a time carries the key `um/s`.
- Negative tests: a quantity, a constant and a table key in a unit with symbol `µg/L` and no key, each expecting
  `must declare an ASCII key`.
- Every unit in the repository's tests, examples and guides passes the check, or gets a key.

### 2.3 #25: one-sided bounds, and bounds known at run time

**Problem.** `Bounds` has one `bool present` for both ends (`unit.hpp:28`), and `checked_within_bounds` reads
bounds only from a `Unit`. Specification limits read at run time can have a lower end, an upper end, or both. Today
a one-sided limit needs a sentinel, and checking any run-time limit needs a copied `Unit`, which then no longer
compares equal to the original.

**Design.**

- `Bounds` replaces `bool present` with `bool lowPresent = false` and `bool highPresent = false`, ahead of the four
  `int64` fields, which keep their names and defaults. `operator==` stays defaulted.
- Factories:
  - `bounds(lowNumerator, lowDenominator, highNumerator, highDenominator)`: both ends, as today.
  - New `at_least(numerator, denominator)`: the lower end only.
  - New `at_most(numerator, denominator)`: the upper end only.
- New `constexpr std::expected<BoundsCheck, ArithmeticError> checked_within(Rational value,
  std::optional<Rational> low, std::optional<Rational> high) noexcept`:
  - no end given: `NotChecked`;
  - both ends given and `low > high`: `DomainError`;
  - below `low`: `BelowMinimum`; above `high`: `AboveMaximum`; otherwise `WithinBounds`. Both ends are inclusive.
- `checked_within_bounds(Rational, Unit)` builds the ends its `Bounds` declares (with `Rational::make`, so a zero
  denominator still gives `DivisionByZero`) and calls `checked_within`. There is one comparison rule.
- In `measured.hpp`:
  - `checked_within(Measured<Q>, std::optional<Rational> low, std::optional<Rational> high)`: `NotMeasured` for an
    absent value, otherwise as above.
  - A throwing `within(...)` for each, built on `detail::or_throw`, as `within_bounds` is.
- Every reader of `Bounds::present` (in headers, tests, examples and guides) moves to the per-end flags.

**Tests.**

- `checked_within`: lower only, upper only, both, neither; a value equal to each end; `low > high`; a `Measured`
  absent value.
- `checked_within_bounds` on a unit declared with `at_least` and with `at_most`; the existing two-sided cases
  unchanged; a zero denominator still `DivisionByZero`.
- The positional `Bounds` initialisers in `test/unit_tests.cpp` move to designated or factory form.

### 2.4 #26: declared decimals per quantity, and `same_unit`

**Problem.** A `Quantity` takes its declared decimals only from its `Unit`. Two quantities in one unit cannot
declare different precision without a second `Unit`, which then compares unequal to the first, because
`Unit::operator==` compares `decimals` and `bounds` too.

**Decision (owner).** `Unit::operator==` keeps comparing every member. A new `same_unit` compares what a unit is.

**Design.**

- `Quantity` gains a defaulted fifth parameter:
  `template <typename Tag, detail::FixedString Symbol, detail::FixedString Description, Unit U,
  DecimalPlaces Places = declared_decimals(U)> struct Quantity`.
  - Its `unit` member is `U` with `decimals` set to `Places.value`. Every reader of a quantity's declared places
    therefore honours it unchanged: `checked_round_to_declared`, `number_text`, `std::format`, traces and
    `render()`.
  - Its `Describe` specialisation exposes that `unit`, as today. A `Describe<Q>` the application writes is
    unaffected.
  - The unit checks (§2.2's, #14's and the rest) apply to `U`, as today.
- New `constexpr bool same_unit(Unit const& leftUnit, Unit const& rightUnit) noexcept`: equal dimension, magnitude,
  offset, `symbolText`, and key (`view_ascii`, so a key declared equal to the symbol is the same key as none
  declared); `decimals` and `bounds` are ignored.
- `detail::same_scale_and_symbol` (`trace.hpp`) compares what a unit shows, so it keeps ignoring `asciiText`. It
  and `same_unit` share one helper for the dimension, magnitude and offset comparison; the plan names it.

**Tests.**

- A quantity in mA with `DecimalPlaces { 1 }`: `checked_round_to_declared`, `number_text` and `std::format` use one
  place; `same_unit(Q::unit, mA)` is true; `Q::unit == mA` is false. A quantity without the parameter is unchanged.
- `same_unit` is false when dimension, magnitude, offset, symbol or key differ, and true when only decimals or
  bounds differ.
- A trace of a formula over the 1-place quantity shows its values at one place.

## 3. Lane B: numbers

### 3.1 #24: decimal text parsed at run time

**Problem.** The only text-to-`Rational` path is the `consteval` `operator""_r`. Values that arrive as text at run
time (CSV or instrument imports, form fields, configuration) have no library parser. Applications that enforce a
declared precision also need the places as typed: `"2.400"` is not `"2.4"`.

**Design.**

- New in `rational.hpp`:
  - `struct ParsedDecimal { Rational value; std::int32_t places; }`, with defaulted `operator==`.
  - `constexpr std::expected<ParsedDecimal, ArithmeticError> parse_decimal_text(std::string_view text) noexcept`.
  - `static constexpr std::expected<Rational, ArithmeticError> Rational::from_decimal_text(std::string_view text)
    noexcept`: `parse_decimal_text`'s value.
- **Syntax:**
  - an optional `+` or `-`;
  - digits with at most one `.`, and at least one digit in all (`.5` and `5.` are accepted);
  - an optional exponent: `e` or `E`, an optional `+` or `-`, and at least one digit;
  - leading zeros are accepted (`007`).
- **Refused with `DomainError`:** the empty text; whitespace anywhere (the caller trims); a decimal comma; any
  separator, including `'`; a second `.`; hexadecimal; `inf` and `nan`; an exponent with no digits; anything else.
- **Refused with `Overflow`:** a value no `Rational` holds.
- **Places as typed:** the digits after the `.`, minus the exponent, and never below 0. `"2.400"` gives 3, `"2.4"`
  gives 1, `"1.5e3"` gives 0, `"15e-1"` gives 1, `"7"` gives 0.
- `"-0"` and `"+0.0"` give 0 (with places 0 and 1).
- **Range:** the mantissa accumulates in `Int128`. Trailing fractional zeros are deferred and cost nothing unless a
  non-zero digit follows, as in `_r`. An exponent with very many digits is bounded while it is read, so reading it
  cannot overflow: a non-zero value with such an exponent is `Overflow`, a zero value is 0.
- **One parser.** The parsing core becomes a `constexpr` detail function returning `std::expected`, with a literal
  mode for `_r`. The literal mode accepts `'` separators, refuses a leading `0` not followed by `.`, `e` or `E` (an
  octal, hexadecimal or binary prefix), and takes no sign. `operator""_r` stays `consteval` and turns each error
  into its existing compile-time sentinel, so its refusals keep their messages.
- **Wider literals.** The literal mode's mantissa also widens from 64 to 128 bits, so `_r` accepts every literal
  `Int128` holds. `docs/numbers.md`'s limits are updated.

**A finding, fixed here: `from_decimal` and exponents beyond 18.** `Rational::from_decimal(mantissa, exponent)`
scales by `detail::pow10` (and the 128-bit `mul_pow10` built on it), which reach only 10^18, so `from_decimal(1, -19)`
and `from_decimal(1, 19)` are `Overflow` although both values fit a `Rational`. Both move to a 128-bit power of ten,
reaching 10^38, and a negative exponent first folds the mantissa's trailing zeros, so `from_decimal(10, -39)` is
1/10^38. The negative test
`rational_literal_exponent_out_of_range` keeps a literal that is still refused, and its comment gives the true
reason.

**Tests.**

- Every accepted form above, each value and places checked, also in `static_assert`s.
- Every refused form, each with its error.
- The widest mantissa `Int128` holds parses; one digit more is `Overflow`.
- `from_decimal(1, -19)` and `from_decimal(1, -38)` answer; `from_decimal(1, -39)` is `Overflow`.
- `_r`: the existing tests pass unchanged; a 20-digit literal now compiles; every existing negative test still fails
  with its message.

### 3.2 #23: `checked_transform` and `checked_combine`

**Problem.** The callbacks of `transform` and `combine` (`measured.hpp:137`, `:165`) return a bare `Rational`. A
caller that must not throw can only use the throwing operators or replace an overflow with a made-up value.

**Design.** New in `measured.hpp`, beside `checked_convert_to`:

```cpp
template <Described Q, typename F>
[[nodiscard]] constexpr std::expected<Measured<Q>, ArithmeticError>
checked_transform(Measured<Q> value, F function) noexcept(std::is_nothrow_invocable_v<F&, Rational>);

template <Described Result, Described Q, Described R, typename F>
[[nodiscard]] constexpr std::expected<Measured<Result>, ArithmeticError>
checked_combine(Measured<Q> lhs, Measured<R> rhs, F function)
    noexcept(std::is_nothrow_invocable_v<F&, Rational, Rational>);
```

- `F` must return exactly `std::expected<Rational, ArithmeticError>`. Anything else is refused by a library
  `static_assert` naming the plain-value alternative: `formula: a checked_transform callback must return
  std::expected<Rational, ArithmeticError>; use transform for a callback that returns a Rational` (and likewise for
  `checked_combine`).
- An absent input gives an absent result without calling `F`. An error from `F` is returned unchanged.

**Tests.** Present and absent inputs; an error from `F` propagating; `checked_mul` as the callback; the `noexcept`
specification following `F`'s; a negative test for a callback returning `Rational`, expecting `must return
std::expected`.

## 4. Final tasks

- **Merge Lane B** into the branch; resolve `measured.hpp` and `CHANGELOG.md`.
- **Docs:** `docs/dimensions.md` (capacity, `checked_symbol`, ASCII keys, per-end bounds, `checked_within`,
  `same_unit`), `docs/quantities.md` (`Places`, `checked_transform`, `checked_combine`), `docs/numbers.md` (run-time
  parsing, the 128-bit literal mantissa), `README.md` (the serialising note). Code samples in the guides compile
  where the repository already compiles them.
- **Finish:** as §5.

## 5. Verification

- **Per task:** `cl-debug`, plus the negative tests on `cl` and `clangcl-debug`. Each task is reviewed and fixed
  until no finding is open, Minor findings included.
- **Finish:**
  - a whole-branch review, its fixes, and a re-review;
  - all eight presets green locally (`cl` and `clang-cl` debug and release; g++-14 and clang++-20 under WSL,
    including UndefinedBehaviorSanitizer);
  - Doxygen (warnings fail) and `mkdocs build --strict`;
  - CI green on the pushed head;
  - the pull request, whose body closes the six issues, marked ready for review. The owner merges.

## 6. Breaking changes

Each gets a CHANGELOG entry under *Changed* that says so in its first words.

- `symbol()` is `consteval`: run-time text must use `checked_symbol`.
- A unit whose symbol is not ASCII must declare `asciiText` where a template takes it.
- `Bounds::present` is replaced by `lowPresent` and `highPresent`.
- `Unit`, `Symbol` and `Dimension` are larger (the capacity and the new member).

## 7. Out of scope

- Changing `Unit::operator==`.
- Writing ASCII keys into `render()`, traces or number text.
- Validating UTF-8 in symbols.
- Widening `Bounds` or `Unit` magnitude fields beyond 64 bits.
- A per-quantity bounds parameter: #25's run-time limits are answered by `checked_within`.
- A version bump or release.
