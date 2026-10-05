# A 128-bit `Rational` over `formula::Int128` — design

**Status:** draft, ready for review · **Date:** 2026-10-03 · **Owner:** Christian Parpart

Resolves issue #1 ("Numeric headroom: 64-bit Rational overflows on realistic lab statistics"). It lands in one pull
request with the trace-units change (`2026-10-03-trace-units-design.md`). The two designs share no code.

## 1. Problem

`Rational` stores a 64-bit numerator and denominator (`rational.hpp:54`, `:282-283`). Every operation is exact and
refuses overflow as `ArithmeticError::Overflow`, so nothing returns a wrong number. But realistic laboratory data
runs out of range, and `docs/numeric-headroom.md` measures where:

- the sample variance of six masses near 40 g at 6 decimal places overflows on 423 of 1000 samples;
- rejection by 7/4 standard deviations overflows on 897 of 1000 at 6 dp;
- a cylinder's strength 4F/(πd²) at 89.3 kN overflows at 17 of 63 diameters between 101 and 163 mm;
- a least-squares line through readings at 3 decimals overflows from 34 points.

The page's own rule, "any realistic case under 8 bits of headroom recommends wider arithmetic", is met.

## 2. Options weighed

An exact model of `checked_add` and `checked_mul`, written in Python over the census's own samples, reproduces
today's figures exactly: 423 of 1000 variance overflows, 17 of 63 cylinder diameters. Run against the alternatives:

| Representation | variance, 6 dp | rejection, 6 dp | cylinder | variance, 10 dp |
|---|---|---|---|---|
| today: 64-bit | 423/1000 overflow | ~890/1000 | 17/63 | — |
| 128-bit intermediates, 64-bit storage | at most 49 rescued | — | 0 rescued | — |
| n/d × 10^e (a decimal exponent) | fits, 10 bits spare | fits, 10 bits | fits, 12 bits | overflows from 8 dp |
| **`Rational` over a 128-bit integer** | **fits, 62 of 127 bits spare** | **fits, 58** | **fits, 63** | **fits, 35** |

**Intermediates alone cannot help a product.** `checked_mul` cross-reduces before it multiplies
(`rational.hpp:355-380`), so its product is already in lowest terms. When that product overflows 64 bits, the exact
result does not fit 64 bits either. Only `checked_add` can overflow before it reduces.

**Declared-unit storage was rejected.** Nodes would compute and record in a declared or scaled unit instead of SI.
That changes the evaluator's core rule (every leaf converted to SI), the meaning of `Step::value` and the renderer's
conversion, and a variance node has no declared unit to work in.

**An opt-in wide `Rep` was rejected.** The traced and audited path hard-codes `Rational`: `checked_evaluate`,
`Outcome`, `explain`, `Step<Rational>`, 32 `is_same_v<Rep, Rational>` gates and the rejection loop. An opt-in type
would leave every one of those at 64 bits.

**Decision:** a `formula::Int128` integer, native where the compiler has one and constexpr software where it does
not. `Rational` is built on it, and there is no 64-bit `Rational` beside it.

## 3. `formula::Int128` — `include/formula-cpp/int128.hpp`

A signed 128-bit two's-complement integer: one class, with one API on every compiler.

### Storage and arithmetic

**One layout everywhere:** two `std::uint64_t` words in two's complement. A second, native layout would double the
class for no gain: a conversion in and out of the compiler's own integer optimises away.

| Compiler | Multiplication, division, overflow check | Why |
|---|---|---|
| GCC, Clang, AppleClang: `__SIZEOF_INT128__` defined, `_MSC_VER` not | `unsigned __int128`, through `__extension__ typedef` | Hardware arithmetic. `__extension__` keeps `-Wpedantic` quiet under `CMAKE_CXX_EXTENSIONS OFF` |
| cl, and clang-cl: any `_MSC_VER` | portable `constexpr` code on the two words | cl has no 128-bit integer. clang-cl accepts `__int128`, but dividing one calls compiler-rt's `__divti3`, which the MSVC linker does not supply |

- **The native type is never handed to the standard library.** In strict mode, libstdc++ does not treat `__int128` as
  an integer type: no `std::is_integral_v`, `std::numeric_limits`, `std::make_unsigned` or `std::format`.
- **The portable operations are `constexpr` functions on two words, in `detail::`.** Every compiler builds and tests
  them. No intrinsics are used, so constant evaluation and run time take the same code.
- **A 64-bit fast path** applies to division, remainder and gcd: when both operands fit 64 bits, the 64-bit operation
  runs. Nearly every value a formula forms is small, so this keeps run time close to today's. It also keeps cl's
  constexpr step budget safe for the suite's roughly 2730 `STATIC_REQUIRE`s, and avoids a call to `__udivti3` on the
  native path.
- **Determinism is unaffected.** Integer results are identical on every path, so the rule that the same inputs give
  the same digits on every compiler still holds. This updates the declared-precision design's "no intrinsics, no
  `__int128`" rule (`2026-09-29-declared-precision-design.md:40-41`), which existed to protect exactly that property.

### Surface

| Group | Members |
|---|---|
| Construction | `constexpr` default (zero); implicit `constexpr` from every built-in integer type except `bool`, sign-extending signed and zero-extending unsigned values |
| Arithmetic | `+ - * / %`, unary `-`, and their compound forms; `<<`, `>>` |
| Comparison | `==`, `<=>` (`std::strong_ordering`) |
| Conversion | **none to a built-in integer type, implicit or explicit**, so code written for a 64-bit `Rational` cannot cut a 128-bit value in half; `to_int64()` and `to_uint64()` return `std::optional`; `fits_int64()`; `to_double()`, rounded to nearest, ties to even |
| Standard library | a `std::numeric_limits<formula::Int128>` specialisation; `std::formatter<formula::Int128, char>`, decimal only. No `std::hash`: `Rational` has none |

- **Plain operators behave like a built-in signed integer:** overflow, division by zero and an out-of-range shift are
  preconditions.
- **Checked forms live in `detail/checked_int.hpp`,** beside the 64-bit ones, as `Int128` overloads of
  `add_checked_or_none`, `sub_checked_or_none` and `mul_checked_or_none`. The sum and the difference are formed on
  the two words' bit patterns with the portable routines, on every compiler, and an overflow is detected from the
  signs: calling `Int128`'s own `+` or `-` first would break their precondition that the exact result fits. Only
  the product uses the compiler's checked builtin, `__builtin_mul_overflow` on the unsigned magnitudes, where the
  compiler has `__int128`, and partial products in software elsewhere; it is then checked against the signed
  range.
- **`detail::UInt128`**, an unsigned 128-bit type, carries magnitudes. A numerator equal to the minimum, -2^127, keeps
  working, as `std::uint64_t` magnitudes let -2^63 work today. It also carries `gcd` (binary, using `std::countr_zero`
  on the words) and the integer square root.

### Tests — `test/int128_tests.cpp`

- **Every operation** at 0, ±1, the 64-bit boundaries (±2^63, 2^64) and the 128-bit boundaries (±(2^127 − 1), -2^127).
- **Compile time and run time:** `STATIC_REQUIRE` and `CHECK` on the same cases.
- **Software against native:** the software path is cross-checked against the native one on GCC and Clang, over
  fixed tables and a seeded generator. On cl and clang-cl, where there is no native type, it is checked against a
  64-bit oracle for products of 32-bit halves, which `test/checked_int_tests.cpp:99-122` already builds.
- **Checked operations** report overflow exactly at the boundary, never one before or after it.
- **Formatting and conversions:** `std::format`, `to_double`, `numeric_limits`.

## 4. `Rational` over `Int128`

- **Type and layout.** `Rational::Int` becomes `formula::Int128`, so `numerator()` and `denominator()` return it.
  The bounds become 2^127 − 1 and −2^127, and `sizeof(Rational)` goes from 16 to 32 bytes.
- **Algorithms.** They are unchanged: `make`'s reduction (now over `UInt128` magnitudes), cross-reduced `checked_mul`,
  `checked_add`, the continued-fraction `<=>`, `checked_negate` refusing the minimum, and `checked_pow`.
- **Constructors.** The integer constructor accepts every built-in integer type except `bool`; the unsigned 64-bit
  `static_assert` no longer applies, because `std::uint64_t` fits `Int128`. A constructor from `Int128` is added.
- **Unchanged documented limits.** Rounding places stay −18…18, and so do `from_decimal`'s exponent range and the
  `_r` literal's digit and exponent caps. Only the integer width changes. Widening those limits is a separate decision.
  The 64-bit `detail::pow10` stays for them; a 128-bit `pow10`, up to 10^38, serves the code that spells a 128-bit
  integer.
- **`detail::Int` keeps meaning `std::int64_t`** for every use that is not `Rational`'s integer.

**Compatibility rule.** Every computation that answers today gives the same answer afterwards; some that are
refused with `Overflow` today now answer. A kernel that assumed 64-bit inputs either widens or refuses a wider input
with `Overflow`, never a different number.

### Readers of `numerator()` / `denominator()`

There are about 159 uses in 22 files. Each one is moved to `Int128` arithmetic, or narrowed through `to_int64()`
with an `Overflow` refusal where its algorithm is genuinely 64-bit.

| File | Change |
|---|---|
| `rounding.hpp` | decimal scaling and `at_least_pow10` |
| `number_text.hpp` | spelling a 128-bit integer in decimal |
| `detail/wide_rounding.hpp` | `wide_from_rational` splits 128 bits into four 32-bit limbs; `narrow_wide_ratio` narrows to 128 bits, so more exact fits answer |
| `rounded_root.hpp` | the integer square root moves from `std::uint64_t` to `UInt128` |
| `function.hpp`, `critical_value.hpp`, `detail/transcendental.hpp`, `rounded_transcendental.hpp`, `lookup.hpp`, `band.hpp`, `trace.hpp`, `detail/least_squares_kernel.hpp` | the remaining readers |

`RepTraits<double>::from` converts through `Int128::to_double()`, and `Rational::to_double()` does the same.

## 5. The census and the headroom page

- **Headroom is `127 − bits`.**
    - The hooks (`FORMULA_CENSUS_NOTE`, `census_record`) take a 128-bit magnitude.
    - `support/census_tally` and `support/census_report` keep and print 128-bit figures ("headroom N of 127").
    - The regex at `cmake/CheckCensusPage.cmake:77` and those at `CMakeLists.txt:209,213` follow.
- **Pins are rebuilt.**
    - The stress controls in `test/overflow_census_tests.cpp` move to the 128-bit boundaries, e.g. (2^126 − 1) + 2^126
      = 2^127 − 1 with headroom 0, and (2^127 − 1) + 1 being `Overflow`.
    - The cylinder list, the resolution survey and the least-squares and regression pins take their new values.
    - `tools/census/exact_sizes.py` sizes against 127 bits, and `census.exact-sizes` and its self-check pin the new
      output.
- **The page is regenerated** with `formula-cpp-census-page`. Its prose is rewritten:
    - *The answer* becomes "enough, for every realistic case measured", with the figures.
    - The 8-bit rule stays, as the rule a future regression would be judged by.
    - The section that left the choice open is replaced by what was chosen and why: the table in §2, in prose.
- **`examples/opaque_and_retry`** shows a least-squares fit refusing with `Overflow` on distinct denominators (pinned
  at `examples/CMakeLists.txt:280`). It moves to data that still outgrows 128 bits, so it keeps teaching the refusal.

## 6. Tests and documentation

**Tests:**

- **Flipped.** `test/rational_tests.cpp:314-347` names itself "the one to flip" when wide intermediates arrive.
- **Moved to the 128-bit bounds.** Tests that pin `Overflow` at 64-bit bounds, in `rational_tests`, `checked_int_tests`
  and the overflow stress tests. Each keeps pinning the refusal at the new limit, not merely stops failing.
- **New cases.** The issue's named sample (40.053270, 39.475922, 39.025798, 40.615904, 39.418416, 40.131659 g) now
  has a variance and a rejection by 7/4 standard deviations. The cylinder strength answers at 139 mm.
- **Unchanged.** Negative tests that pin the literal's caps, since those limits do not change.

**Documentation:**

- Every guide that says 64-bit says 128-bit: `docs/numbers.md:119`, `docs/calculations.md:987`, `docs/display.md:253`, `docs/statistics.md:357`, and the README where it states the width.
- The `Rational` and `Int128` doc comments; Doxygen builds clean.
- CHANGELOG `[Unreleased]`:
    - *Added:* `formula::Int128`.
    - *Changed:* `Rational::Int` is `formula::Int128` (a source change for code that stores `numerator()` in an
      `std::int64_t`), values are twice the size, and `Overflow` arrives much later.

## 7. Verification

**Per task:**

- MSVC `cl-debug`: the full build and every non-negative test.
- The task's negative tests on `cl-debug` and `clangcl-debug`.

**The `Int128` task** also runs `gcc-release` and `clang-debug` under WSL. The native path exists only there.

**At the end**, all eight presets, Doxygen and mkdocs, and the census page:

- no overflow in the variance or the rejection at 6 dp;
- the cylinder fits at every diameter;
- every realistic row keeps at least 8 bits.

## 8. Out of scope

- **Wider documented limits:** rounding places, `from_decimal` exponents and `_r` digits beyond 18.
- **Faster wide kernels.** `detail/wide_int.hpp`'s one-bit-at-a-time division stays as it is.
- **A 64-bit `Rational` for memory-constrained use.**
