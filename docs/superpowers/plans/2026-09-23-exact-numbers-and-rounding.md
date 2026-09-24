# Exact Numbers and Rounding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `formula::Rational` — an exact rational number over `std::int64_t` — together with the rounding vocabulary the norms actually specify (decimal places, significant digits, round-to-multiple, seven rounding modes), all `constexpr` and dependency-free.

**Architecture:** Four new headers in a strict dependency chain: `detail/checked_int.hpp` (overflow-detecting integer primitives) → `error.hpp` (the failure vocabulary) → `rational.hpp` (the number) → `rounding.hpp` (the rounding operations). Every operation that can fail has a `checked_*` form returning `std::expected`; the operators are defined in terms of those and throw. No operation ever saturates, truncates or produces a silently wrong number.

**Tech Stack:** C++23, header-only, no third-party dependencies. Catch2 v3 for runtime tests, `static_assert` for compile-time tests, the phase-1 `formula_add_negative_test` harness for must-not-compile tests.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — this plan implements **§6 (Numbers)** and the rounding half of **§16.3 Tier A #2**, i.e. spec phase 2 of §17.

## Global Constraints

Copied verbatim from the spec. Every task's requirements implicitly include this section.

- **Open source, no company IP.** Apache-2.0. Every new source and build file starts with `// SPDX-License-Identifier: Apache-2.0` (or `# SPDX-License-Identifier: Apache-2.0` for CMake).
- **No norm content in this repository.** No standard identifiers, clause or table numbers, transcribed text, equations or threshold values — not even as a bare citation. Public examples use **generic physics only** with fictional `Example Standard` citations.
- **No third-party dependencies in the core**, and no serialization library in particular.
- **No macros for traceability.**
- **Compilers:** MSVC `cl`, `clang-cl`, `clang++`. GCC welcome if free.
- **Must compile as C++23 everywhere.**
- **Public headers must not include `<string>`, `<vector>`, `<format>` or `<iostream>`.** Enforced by `ctest -R hygiene.headers`. `<string_view>` is permitted.
- **Naming:** types `CamelCase`, functions `lower_case`, global constants `CamelCase`, private data members `_leadingUnderscore`. Enforced by `.clang-tidy`.
- **Formatting:** `.clang-format` at the repo root, column limit 125, Allman braces, `Cpp11BracedListStyle: false` (so `{ a, b }` carries inner spaces).
- **Every file that can fail a guard must pass it:** `hygiene.spdx`, `hygiene.nolint`, `hygiene.headers`, `hygiene.version`. They glob, so new files are picked up with no CMake edit.

## Design Rulings

These resolve ambiguities in the spec before implementation starts. Each is binding for this plan.

**Ruling A — `Rational` carries no decimal-place tag.**
Spec §6 asks for "an exact rational with an explicit decimal-place tag and rounding mode". We split that in two: `Rational` is a pure exact number carrying **no** precision member, and `DecimalPlaces` and `RoundingMode` are separate value types consumed by explicit rounding operations.
*Why:* §6's own rationale is that rounding is *specified behaviour, not presentation*. A tag riding inside the number forces every arithmetic operator to invent a precision-propagation policy — which is exactly the payload-shape tagging §4 tells us to shed. Spec §17 phase 3 puts "decimals + bounds" on the **unit/quantity** layer and phase 8 makes rounding a **tree node**; neither needs the number to carry a tag.
*Migration consequence (spec §15):* downstream code adopting this type gets its declared precision from the quantity layer, not from the number. Recorded here so the migration does not discover it late.
*Cost if wrong:* a `DecimalPlaces` member and its propagation rules can be added later without changing any call site that does not read it.

**Ruling B — failure is reported, never absorbed.**
Every fallible operation has a `checked_*` free function returning `std::expected<T, ArithmeticError>`. The operators (`+ - * /`, compound assignment) are thin wrappers that throw `formula::ArithmeticException` when the checked form fails.
*Why:* an arithmetic layer that saturates on overflow and merely logs it produces a wrong certificate in silence, and §4 requires us to shed the logger in any case. Throwing keeps operator syntax usable (the library's headline feature is ordinary operators) and makes a failure inside a constant expression a **compile error**. Spec phase 5's sum-type result consumes the `checked_*` layer directly and never touches the throwing layer.
*Cost if wrong:* the throwing wrappers are ~6 lines each and can be removed or made configurable without touching the checked core.

**Ruling C — comparison is overflow-free, so `operator<=>` is total and `noexcept`.**
`Rational` comparison uses the continued-fraction (Euclidean) algorithm, which performs no multiplication. Cross-multiplication would overflow for operands that are individually representable.
*Cost if wrong:* none identified; the algorithm is exact for all representable operands.

**Ruling D — no floating-point conversion is implicit.**
A constructor template constrained to floating-point types exists solely to `static_assert` with our own message. Users convert explicitly via `Rational::from_decimal` (exact, preferred) or `rational_from_double` (rounded, in `rounding.hpp`).
*Why:* `Rational r = 0.45;` reading as `8106479329266893/2^54` is the single most likely silent-wrongness bug in this layer.

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/detail/checked_int.hpp` | **Create.** Overflow predicates and checked operations on `std::int64_t`; `gcd` over magnitudes; `floor_divmod`; `pow10`; `decimal_digits`. No dependency on any other formula header. |
| `include/formula-cpp/error.hpp` | **Create.** `ArithmeticError`, `describe(ArithmeticError)`, `ArithmeticException`, `detail::or_throw`. |
| `include/formula-cpp/rational.hpp` | **Create.** `Rational`: canonicalisation, accessors, conversions, ordering, arithmetic (checked + throwing), `abs`, `pow`. |
| `include/formula-cpp/rounding.hpp` | **Create.** `RoundingMode`, `DecimalPlaces`, `SignificantDigits`, `round_to_int`, `round_to_multiple`, `round`, `round_to_integer`, `decimal_exponent`, `rational_from_double`, and the `checked_*` form of each. |
| `include/formula-cpp/formula.hpp` | **Modify.** Add the four includes to the umbrella. |
| `test/checked_int_tests.cpp` | **Create.** Runtime + `static_assert` coverage of the integer primitives. |
| `test/rational_tests.cpp` | **Create.** Runtime + `static_assert` coverage of `Rational`. |
| `test/rounding_tests.cpp` | **Create.** The seven-modes × sign table, decimal places, significant digits, multiples. |
| `test/negative/rational_from_floating_point.cpp` | **Create.** Must-not-compile: implicit construction from `double`. |
| `test/CMakeLists.txt` | **Modify.** Add the three test sources and the negative case. |
| `examples/exact_numbers.cpp` | **Create.** Generic-physics example: exact unit conversion and a specified rounding rule. |
| `examples/CMakeLists.txt` | **Modify.** Register the example. |
| `docs/numbers.md` | **Create.** Human-readable explanation of why exact, how to construct, and the rounding-mode table. |
| `mkdocs.yml` | **Modify.** Add the page to the nav. |

## Task Overview

| Task | Deliverable |
|---|---|
| 1 | `detail/checked_int.hpp` — integer primitives |
| 2 | `error.hpp` — the failure vocabulary |
| 3 | `rational.hpp` part 1 — the type, canonicalisation, conversions, ordering |
| 4 | `rational.hpp` part 2 — arithmetic, `pow`, `abs`, the negative compile test |
| 5 | `rounding.hpp` part 1 — modes, scale types, round-to-integer and round-to-multiple |
| 6 | `rounding.hpp` part 2 — decimal places, significant digits, `rational_from_double` |
| 7 | Umbrella wiring, example, documentation, full-matrix verification |

---

### Task 1: Checked integer primitives

**Files:**
- Create: `include/formula-cpp/detail/checked_int.hpp`
- Create: `test/checked_int_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `checked_int_tests.cpp` to `add_executable(formula-cpp-tests ...)`)

**Interfaces:**
- Consumes: nothing.
- Produces, all in `namespace formula::detail`:
  - `using Int = std::int64_t;`
  - `inline constexpr Int IntMax`, `inline constexpr Int IntMin`
  - `constexpr bool add_overflows(Int, Int) noexcept`
  - `constexpr bool sub_overflows(Int, Int) noexcept`
  - `constexpr bool mul_overflows(Int, Int) noexcept`
  - `constexpr std::optional<Int> checked_add(Int, Int) noexcept`
  - `constexpr std::optional<Int> checked_sub(Int, Int) noexcept`
  - `constexpr std::optional<Int> checked_mul(Int, Int) noexcept`
  - `constexpr std::uint64_t magnitude(Int) noexcept`
  - `constexpr std::uint64_t gcd(std::uint64_t, std::uint64_t) noexcept`
  - `struct DivMod { Int quotient; Int remainder; };`
  - `constexpr DivMod floor_divmod(Int numerator, Int denominator) noexcept` — precondition `denominator > 0`; returns `0 <= remainder < denominator`
  - `constexpr std::optional<Int> pow10(int exponent) noexcept` — `nullopt` outside `0..18`
  - `constexpr int decimal_digits(Int value) noexcept` — digit count of `|value|`; `0` has 1 digit
  - `constexpr std::optional<Int> mul_pow10(Int value, int exponent) noexcept` — `value * 10^exponent`, `nullopt` on overflow or on an exponent outside `0..18`

- [ ] **Step 1: Write the failing test**

Create `test/checked_int_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/checked_int.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iterator>

using formula::detail::checked_add;
using formula::detail::checked_mul;
using formula::detail::checked_sub;
using formula::detail::decimal_digits;
using formula::detail::floor_divmod;
using formula::detail::gcd;
using formula::detail::Int;
using formula::detail::IntMax;
using formula::detail::IntMin;
using formula::detail::magnitude;
using formula::detail::mul_pow10;
using formula::detail::pow10;

// ---- compile-time coverage: these must hold without running anything ----

static_assert(checked_add(Int { 2 }, Int { 3 }) == Int { 5 });
static_assert(!checked_add(IntMax, Int { 1 }).has_value());
static_assert(!checked_add(IntMin, Int { -1 }).has_value());
static_assert(checked_add(IntMax, Int { -1 }) == IntMax - 1);

static_assert(checked_sub(Int { 2 }, Int { 3 }) == Int { -1 });
static_assert(!checked_sub(IntMin, Int { 1 }).has_value());
static_assert(!checked_sub(IntMax, Int { -1 }).has_value());

static_assert(checked_mul(Int { 6 }, Int { 7 }) == Int { 42 });
static_assert(checked_mul(Int { 0 }, IntMin) == Int { 0 });
static_assert(!checked_mul(IntMin, Int { -1 }).has_value());
static_assert(checked_mul(IntMin, Int { 1 }) == IntMin);
static_assert(!checked_mul(IntMax, Int { 2 }).has_value());
static_assert(!checked_mul(Int { -3037000500 }, Int { 3037000500 }).has_value());
static_assert(checked_mul(Int { 3037000499 }, Int { 3037000499 }).has_value());

static_assert(magnitude(IntMin) == 9223372036854775808ULL);
static_assert(magnitude(Int { -5 }) == 5ULL);
static_assert(magnitude(Int { 5 }) == 5ULL);

static_assert(gcd(12ULL, 18ULL) == 6ULL);
static_assert(gcd(0ULL, 7ULL) == 7ULL);
static_assert(gcd(7ULL, 0ULL) == 7ULL);
static_assert(gcd(9223372036854775808ULL, 9223372036854775808ULL) == 9223372036854775808ULL);

static_assert(floor_divmod(Int { 7 }, Int { 2 }).quotient == Int { 3 });
static_assert(floor_divmod(Int { 7 }, Int { 2 }).remainder == Int { 1 });
static_assert(floor_divmod(Int { -7 }, Int { 2 }).quotient == Int { -4 });
static_assert(floor_divmod(Int { -7 }, Int { 2 }).remainder == Int { 1 });
static_assert(floor_divmod(Int { -6 }, Int { 2 }).quotient == Int { -3 });
static_assert(floor_divmod(Int { -6 }, Int { 2 }).remainder == Int { 0 });
static_assert(floor_divmod(IntMin, Int { 1 }).quotient == IntMin);
static_assert(floor_divmod(IntMin, Int { 1 }).remainder == Int { 0 });

static_assert(pow10(0) == Int { 1 });
static_assert(pow10(18) == Int { 1000000000000000000 });
static_assert(!pow10(19).has_value());
static_assert(!pow10(-1).has_value());

static_assert(decimal_digits(Int { 0 }) == 1);
static_assert(decimal_digits(Int { 9 }) == 1);
static_assert(decimal_digits(Int { 10 }) == 2);
static_assert(decimal_digits(Int { -999 }) == 3);
static_assert(decimal_digits(IntMax) == 19);
static_assert(decimal_digits(IntMin) == 19);

static_assert(mul_pow10(Int { 3 }, 2) == Int { 300 });
static_assert(!mul_pow10(IntMax, 1).has_value());
static_assert(mul_pow10(Int { -7 }, 3) == Int { -7000 });

// ---- runtime coverage: the same primitives, exercised through Catch2 ----

TEST_CASE("checked_add reports overflow rather than wrapping", "[checked_int]")
{
    CHECK(checked_add(IntMax - 1, Int { 1 }) == IntMax);
    CHECK_FALSE(checked_add(IntMax, Int { 1 }).has_value());
    CHECK_FALSE(checked_add(IntMin, Int { -1 }).has_value());
}

TEST_CASE("mul_overflows agrees with checked_mul on the boundary cases", "[checked_int]")
{
    Int const values[] = { IntMin, IntMin + 1, -3037000500, -2, -1, 0, 1, 2, 3037000499, IntMax - 1, IntMax };
    for (Int const lhs: values)
        for (Int const rhs: values)
            CHECK(formula::detail::mul_overflows(lhs, rhs) == !checked_mul(lhs, rhs).has_value());
}

TEST_CASE("floor_divmod always yields a remainder in [0, denominator)", "[checked_int]")
{
    for (Int numerator = -20; numerator <= 20; ++numerator)
    {
        for (Int denominator = 1; denominator <= 7; ++denominator)
        {
            auto const [quotient, remainder] = floor_divmod(numerator, denominator);
            CHECK(remainder >= 0);
            CHECK(remainder < denominator);
            CHECK(quotient * denominator + remainder == numerator);
        }
    }
}

TEST_CASE("decimal_digits counts the digits of the magnitude", "[checked_int]")
{
    CHECK(decimal_digits(Int { 1 }) == 1);
    CHECK(decimal_digits(Int { 100 }) == 3);
    CHECK(decimal_digits(Int { -100 }) == 3);
    CHECK(decimal_digits(Int { 999999999999999999 }) == 18);
}
```

- [ ] **Step 2: Add the test file to the build and run it to verify it fails**

In `test/CMakeLists.txt`, change

```cmake
add_executable(formula-cpp-tests runtime_tests.cpp compile_time_tests.cpp cross_tu_a.cpp)
```

to

```cmake
add_executable(formula-cpp-tests
    runtime_tests.cpp
    compile_time_tests.cpp
    cross_tu_a.cpp
    checked_int_tests.cpp)
```

Run:

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `cannot open include file: 'formula-cpp/detail/checked_int.hpp'` (MSVC) or `'formula-cpp/detail/checked_int.hpp' file not found` (clang). This is the RED signal: the header does not exist yet.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/detail/checked_int.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Overflow-detecting integer primitives, all constexpr and free of compiler
/// intrinsics. MSVC has no __builtin_*_overflow, and its <intrin.h> equivalents
/// are not constexpr, so the checks are written in portable C++ and used on
/// every compiler. Optimisers recognise these idioms.

#include <cstdint>
#include <optional>

namespace formula::detail
{

using Int = std::int64_t;

inline constexpr Int IntMax = 9223372036854775807LL;
inline constexpr Int IntMin = -IntMax - 1;

/// True when `lhs + rhs` is not representable.
[[nodiscard]] constexpr bool add_overflows(Int lhs, Int rhs) noexcept
{
    return (rhs > 0 && lhs > IntMax - rhs) || (rhs < 0 && lhs < IntMin - rhs);
}

/// True when `lhs - rhs` is not representable.
[[nodiscard]] constexpr bool sub_overflows(Int lhs, Int rhs) noexcept
{
    return (rhs < 0 && lhs > IntMax + rhs) || (rhs > 0 && lhs < IntMin + rhs);
}

/// True when `lhs * rhs` is not representable.
[[nodiscard]] constexpr bool mul_overflows(Int lhs, Int rhs) noexcept
{
    if (lhs == 0 || rhs == 0)
        return false;
    if (lhs > 0)
        return rhs > 0 ? lhs > IntMax / rhs : rhs < IntMin / lhs;
    return rhs > 0 ? lhs < IntMin / rhs : lhs < IntMax / rhs;
}

[[nodiscard]] constexpr std::optional<Int> checked_add(Int lhs, Int rhs) noexcept
{
    if (add_overflows(lhs, rhs))
        return std::nullopt;
    return lhs + rhs;
}

[[nodiscard]] constexpr std::optional<Int> checked_sub(Int lhs, Int rhs) noexcept
{
    if (sub_overflows(lhs, rhs))
        return std::nullopt;
    return lhs - rhs;
}

[[nodiscard]] constexpr std::optional<Int> checked_mul(Int lhs, Int rhs) noexcept
{
    if (mul_overflows(lhs, rhs))
        return std::nullopt;
    return lhs * rhs;
}

/// Absolute value as an unsigned quantity. Exists because `-IntMin` overflows
/// but `magnitude(IntMin)` is an ordinary number.
[[nodiscard]] constexpr std::uint64_t magnitude(Int value) noexcept
{
    return value < 0 ? ~static_cast<std::uint64_t>(value) + 1U : static_cast<std::uint64_t>(value);
}

/// Greatest common divisor. `gcd(0, n) == n` and `gcd(0, 0) == 0`.
/// Unsigned so that `magnitude(IntMin)` is a legal argument.
[[nodiscard]] constexpr std::uint64_t gcd(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
    while (rhs != 0)
    {
        std::uint64_t const remainder = lhs % rhs;
        lhs = rhs;
        rhs = remainder;
    }
    return lhs;
}

struct DivMod
{
    Int quotient {};
    Int remainder {};
};

/// Floored division: the remainder is always in `[0, denominator)`, unlike the
/// language's truncating `/` and `%`. Every rounding decision in this library is
/// expressed as "which side of `remainder / denominator` the value sits on",
/// which only reads cleanly with a non-negative remainder.
///
/// @pre `denominator > 0`.
[[nodiscard]] constexpr DivMod floor_divmod(Int numerator, Int denominator) noexcept
{
    Int quotient = numerator / denominator;
    Int remainder = numerator % denominator;
    if (remainder < 0)
    {
        // Safe: a non-zero remainder means |quotient| is strictly below
        // |numerator| / denominator, so the decrement cannot reach IntMin, and
        // remainder is greater than -denominator.
        --quotient;
        remainder += denominator;
    }
    return { quotient, remainder };
}

/// `10^exponent` for `0 <= exponent <= 18`; `nullopt` otherwise. 10^18 is the
/// largest power of ten representable in Int.
[[nodiscard]] constexpr std::optional<Int> pow10(int exponent) noexcept
{
    if (exponent < 0 || exponent > 18)
        return std::nullopt;
    Int result = 1;
    for (int step = 0; step < exponent; ++step)
        result *= 10;
    return result;
}

/// Number of decimal digits in `|value|`. Zero has one digit.
[[nodiscard]] constexpr int decimal_digits(Int value) noexcept
{
    std::uint64_t remaining = magnitude(value);
    int digits = 1;
    while (remaining >= 10U)
    {
        remaining /= 10U;
        ++digits;
    }
    return digits;
}

/// `value * 10^exponent`, or `nullopt` on overflow or an out-of-range exponent.
[[nodiscard]] constexpr std::optional<Int> mul_pow10(Int value, int exponent) noexcept
{
    std::optional<Int> const factor = pow10(exponent);
    if (!factor)
        return std::nullopt;
    return checked_mul(value, *factor);
}

} // namespace formula::detail
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS. The `static_assert` block compiling at all is the compile-time half of the evidence; the Catch2 cases are the runtime half.

- [ ] **Step 5: Verify the hygiene guards still pass**

```
ctest --preset cl-debug -R hygiene --output-on-failure
```

Expected: all four `hygiene.*` tests pass. `hygiene.spdx` reports a file count two higher than before, because it globs both `include/*.hpp` and `test/*.cpp`.

- [ ] **Step 6: Commit**

```bash
git add include/formula-cpp/detail/checked_int.hpp test/checked_int_tests.cpp test/CMakeLists.txt
git commit -m "feat(numbers): add overflow-detecting integer primitives"
```

---

### Task 2: The arithmetic failure vocabulary

**Files:**
- Create: `include/formula-cpp/error.hpp`
- Create: `test/error_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `error_tests.cpp` to the test executable)

**Interfaces:**
- Consumes: nothing.
- Produces, in `namespace formula`:
  - `enum class ArithmeticError : std::uint8_t { DivisionByZero, Overflow, NotFinite, DomainError };`
  - `constexpr std::string_view describe(ArithmeticError) noexcept`
  - `class ArithmeticException: public std::exception` with `constexpr explicit ArithmeticException(ArithmeticError)`, `code()`, `what()`
- Produces, in `namespace formula::detail`:
  - `template <typename T> constexpr T or_throw(std::expected<T, ArithmeticError> const&)` — returns the value, or throws `ArithmeticException`

Spec phase 5 turns an `ArithmeticError` into the sum type's `invalid(reason)` arm. Keep `describe` returning a lowercase noun phrase with no trailing punctuation so it composes into a longer sentence there.

- [ ] **Step 1: Write the failing test**

Create `test/error_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <iterator>
#include <string_view>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::describe;
using formula::detail::or_throw;

static_assert(describe(ArithmeticError::DivisionByZero) == std::string_view { "division by zero" });
static_assert(!describe(ArithmeticError::Overflow).empty());
static_assert(!describe(ArithmeticError::NotFinite).empty());
static_assert(!describe(ArithmeticError::DomainError).empty());

static_assert(or_throw(std::expected<int, ArithmeticError> { 42 }) == 42);

TEST_CASE("every error code has a distinct, non-empty description", "[error]")
{
    ArithmeticError const codes[] = { ArithmeticError::DivisionByZero,
                                      ArithmeticError::Overflow,
                                      ArithmeticError::NotFinite,
                                      ArithmeticError::DomainError };

    for (std::size_t i = 0; i < std::size(codes); ++i)
    {
        CHECK_FALSE(describe(codes[i]).empty());
        for (std::size_t j = i + 1; j < std::size(codes); ++j)
            CHECK(describe(codes[i]) != describe(codes[j]));
    }
}

TEST_CASE("or_throw passes a value through and throws on an error", "[error]")
{
    CHECK(or_throw(std::expected<int, ArithmeticError> { 7 }) == 7);

    auto const failed = std::expected<int, ArithmeticError> { std::unexpect, ArithmeticError::DivisionByZero };
    CHECK_THROWS_AS(or_throw(failed), ArithmeticException);
}

TEST_CASE("the exception carries the code and a readable message", "[error]")
{
    try
    {
        auto const failed = std::expected<int, ArithmeticError> { std::unexpect, ArithmeticError::Overflow };
        (void) or_throw(failed);
        FAIL("or_throw did not throw");
    }
    catch (ArithmeticException const& exception)
    {
        CHECK(exception.code() == ArithmeticError::Overflow);
        CHECK(std::string_view { exception.what() } == describe(ArithmeticError::Overflow));
    }
}
```

- [ ] **Step 2: Add the test file to the build and run it to verify it fails**

Add `error_tests.cpp` to `add_executable(formula-cpp-tests ...)` in `test/CMakeLists.txt`, then:

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `formula-cpp/error.hpp` not found.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/error.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The failure vocabulary of exact arithmetic.
///
/// Two layers, deliberately: every fallible operation has a `checked_` form
/// returning `std::expected`, and the operators are thin wrappers that throw.
/// Nothing saturates and nothing truncates -- a norm calculation that quietly
/// produces a near miss is worse than one that stops.
///
/// A throw inside a constant expression is a compile error, so misuse in a
/// constexpr context is caught at build time with no extra machinery.

#include <cstdint>
#include <exception>
#include <expected>
#include <string_view>

namespace formula
{

/// Why an exact arithmetic operation could not produce a result.
enum class ArithmeticError : std::uint8_t
{
    /// The divisor was zero.
    DivisionByZero,
    /// The exact result, or an intermediate term, is outside the representable range.
    Overflow,
    /// A floating-point input was NaN or infinite.
    NotFinite,
    /// An argument was outside the domain of the operation -- a non-positive
    /// step, say, or fewer than one significant digit.
    DomainError,
};

/// A lowercase noun phrase with no trailing punctuation, so callers can embed it
/// in a longer sentence. Spec phase 5 renders this into the `invalid` arm of the
/// evaluation result.
[[nodiscard]] constexpr std::string_view describe(ArithmeticError error) noexcept
{
    switch (error)
    {
        case ArithmeticError::DivisionByZero: return "division by zero";
        case ArithmeticError::Overflow: return "overflow in exact arithmetic";
        case ArithmeticError::NotFinite: return "value is not finite";
        case ArithmeticError::DomainError: return "argument outside the domain of the operation";
    }
    return "unknown arithmetic error";
}

/// Thrown by the operator layer when the corresponding `checked_` operation fails.
class ArithmeticException: public std::exception
{
  public:
    constexpr explicit ArithmeticException(ArithmeticError error) noexcept: _error { error } {}

    [[nodiscard]] constexpr ArithmeticError code() const noexcept { return _error; }

    [[nodiscard]] char const* what() const noexcept override { return describe(_error).data(); }

  private:
    ArithmeticError _error;
};

namespace detail
{

    /// Unwraps a checked result, throwing on failure. The operator layer is
    /// written entirely in terms of this.
    template <typename T>
    [[nodiscard]] constexpr T or_throw(std::expected<T, ArithmeticError> const& result)
    {
        if (!result)
            throw ArithmeticException { result.error() };
        return *result;
    }

} // namespace detail

} // namespace formula
```

Note for the implementer: `describe` returns a `std::string_view` built from a string literal, so `.data()` is NUL-terminated and `what()` is sound. Do not change `describe` to return a view over non-literal storage without revisiting `what()`.

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Verify the header guard still passes**

```
ctest --preset cl-debug -R hygiene.headers --output-on-failure
```

Expected: PASS. `<string_view>` is permitted, `<string>` is not — confirm the header includes the former and not the latter.

- [ ] **Step 6: Commit**

```bash
git add include/formula-cpp/error.hpp test/error_tests.cpp test/CMakeLists.txt
git commit -m "feat(numbers): add the arithmetic failure vocabulary"
```

---

### Task 3: `Rational` — the type, canonicalisation, conversions and ordering

**Files:**
- Create: `include/formula-cpp/rational.hpp`
- Create: `test/rational_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `rational_tests.cpp` to the test executable)

**Interfaces:**
- Consumes: everything from `formula::detail` in Task 1, and `ArithmeticError` / `or_throw` from Task 2.
- Produces, in `namespace formula`:
  - `class Rational` with `using Int = detail::Int;`
  - `constexpr Rational() noexcept` — zero
  - `constexpr Rational(Int whole) noexcept` — implicit, exact
  - `template <typename T> requires std::is_floating_point_v<T> constexpr Rational(T)` — `static_assert`s; see Ruling D
  - `constexpr Rational(Int numerator, Int denominator)` — throws on failure
  - `static constexpr std::expected<Rational, ArithmeticError> make(Int numerator, Int denominator) noexcept`
  - `static constexpr std::expected<Rational, ArithmeticError> from_decimal(Int mantissa, int exponent) noexcept`
  - `static constexpr std::expected<Rational, ArithmeticError> from_double_exact(double) noexcept`
  - `constexpr Int numerator() const noexcept`, `constexpr Int denominator() const noexcept`
  - `constexpr bool is_integer() const noexcept`, `constexpr bool is_zero() const noexcept`, `constexpr int sign() const noexcept`
  - `constexpr double to_double() const noexcept`
  - `constexpr std::strong_ordering operator<=>(Rational const&) const noexcept`
  - `constexpr bool operator==(Rational const&) const noexcept`

**Class invariants, which every constructor and every arithmetic result must restore:**
1. `denominator() > 0`
2. `gcd(magnitude(numerator()), magnitude(denominator())) == 1`
3. zero is exactly `0 / 1`

- [ ] **Step 1: Write the failing test**

Create `test/rational_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

#include <compare>
#include <limits>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Rational;

namespace
{
constexpr Rational::Int IntMax = formula::detail::IntMax;
constexpr Rational::Int IntMin = formula::detail::IntMin;

/// Builds a Rational in a constant expression, asserting success.
consteval Rational exact(Rational::Int numerator, Rational::Int denominator)
{
    return Rational { numerator, denominator };
}
} // namespace

// ---- invariants ----

static_assert(Rational {}.numerator() == 0);
static_assert(Rational {}.denominator() == 1);
static_assert(Rational { 7 }.numerator() == 7);
static_assert(Rational { 7 }.denominator() == 1);

static_assert(exact(2, 4).numerator() == 1);
static_assert(exact(2, 4).denominator() == 2);
static_assert(exact(-2, 4).numerator() == -1);
static_assert(exact(-2, 4).denominator() == 2);
static_assert(exact(2, -4).numerator() == -1);
static_assert(exact(2, -4).denominator() == 2);
static_assert(exact(-2, -4).numerator() == 1);
static_assert(exact(-2, -4).denominator() == 2);
static_assert(exact(0, -5).numerator() == 0);
static_assert(exact(0, -5).denominator() == 1);

static_assert(exact(IntMin, IntMin) == Rational { 1 });
static_assert(exact(IntMin, 1).numerator() == IntMin);
static_assert(exact(IntMin, 2).numerator() == IntMin / 2);
static_assert(exact(IntMin, 2).denominator() == 1);

static_assert(!Rational::make(1, 0).has_value());
static_assert(Rational::make(1, 0).error() == ArithmeticError::DivisionByZero);
static_assert(!Rational::make(IntMin, -1).has_value());
static_assert(Rational::make(IntMin, -1).error() == ArithmeticError::Overflow);

static_assert(Rational { 0 }.is_zero());
static_assert(Rational { 3 }.is_integer());
static_assert(!exact(1, 2).is_integer());
static_assert(Rational { 0 }.sign() == 0);
static_assert(Rational { -4 }.sign() == -1);
static_assert(Rational { 4 }.sign() == 1);

// ---- decimal construction: the exact, preferred route ----

static_assert(Rational::from_decimal(45, -2)->numerator() == 9);
static_assert(Rational::from_decimal(45, -2)->denominator() == 20);
static_assert(Rational::from_decimal(3, 0) == Rational { 3 });
static_assert(Rational::from_decimal(3, 2) == Rational { 300 });
static_assert(Rational::from_decimal(0, -5) == Rational { 0 });
static_assert(!Rational::from_decimal(1, -19).has_value());
static_assert(!Rational::from_decimal(IntMax, 1).has_value());

// ---- exact binary conversion ----

static_assert(Rational::from_double_exact(0.5) == exact(1, 2));
static_assert(Rational::from_double_exact(-0.25) == exact(-1, 4));
static_assert(Rational::from_double_exact(0.0) == Rational { 0 });
static_assert(Rational::from_double_exact(3.0) == Rational { 3 });
// 0.1 is not a dyadic rational, so the exact value is NOT 1/10.
static_assert(Rational::from_double_exact(0.1)->denominator() != 10);

// ---- ordering ----

static_assert(exact(1, 3) < exact(1, 2));
static_assert(exact(-1, 2) < exact(-1, 3));
static_assert(exact(2, 4) == exact(1, 2));
static_assert(exact(1, 2) <= exact(1, 2));
static_assert(Rational { IntMax } > Rational { IntMin });
// Cross-multiplication would overflow here; the continued-fraction comparison must not.
// n/(n-1) < (n-1)/(n-2) because n(n-2) = n^2-2n is one less than (n-1)^2.
static_assert(exact(IntMax, IntMax - 1) < exact(IntMax - 1, IntMax - 2));

TEST_CASE("construction canonicalises sign and common factors", "[rational]")
{
    Rational const value { 6, -8 };
    CHECK(value.numerator() == -3);
    CHECK(value.denominator() == 4);
}

TEST_CASE("a zero denominator is reported, not absorbed", "[rational]")
{
    auto const result = Rational::make(1, 0);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::DivisionByZero);
    CHECK_THROWS_AS(Rational(1, 0), ArithmeticException);
}

TEST_CASE("from_decimal is exact where from_double_exact is not", "[rational]")
{
    auto const exactDecimal = Rational::from_decimal(45, -2);
    REQUIRE(exactDecimal.has_value());
    CHECK(exactDecimal->numerator() == 9);
    CHECK(exactDecimal->denominator() == 20);

    auto const fromBinary = Rational::from_double_exact(0.45);
    REQUIRE(fromBinary.has_value());
    CHECK(fromBinary->denominator() != 20);
    CHECK(fromBinary->to_double() == 0.45);
}

TEST_CASE("from_double_exact rejects non-finite input", "[rational]")
{
    double const infinity = std::numeric_limits<double>::infinity();
    double const notANumber = std::numeric_limits<double>::quiet_NaN();

    CHECK(Rational::from_double_exact(infinity).error() == ArithmeticError::NotFinite);
    CHECK(Rational::from_double_exact(notANumber).error() == ArithmeticError::NotFinite);
}

TEST_CASE("ordering never overflows, whatever the operands", "[rational]")
{
    Rational const nearOne { IntMax, IntMax - 1 };
    Rational const alsoNearOne { IntMax - 1, IntMax - 2 };

    // (IntMax)/(IntMax-1) < (IntMax-1)/(IntMax-2): both exceed 1, and the one
    // with the smaller terms exceeds it by more.
    CHECK(nearOne < alsoNearOne);
    CHECK(nearOne != alsoNearOne);
    CHECK((nearOne <=> nearOne) == std::strong_ordering::equal);
}

TEST_CASE("to_double round-trips values that are exactly representable", "[rational]")
{
    CHECK(Rational(1, 2).to_double() == 0.5);
    CHECK(Rational(-3, 4).to_double() == -0.75);
    CHECK(Rational(0).to_double() == 0.0);
}
```

- [ ] **Step 2: Add the test file to the build and run it to verify it fails**

Add `rational_tests.cpp` to `add_executable(formula-cpp-tests ...)`, then:

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `formula-cpp/rational.hpp` not found.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/rational.hpp`. This task writes everything below; Task 4 appends the arithmetic to the same file.

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// An exact rational number over std::int64_t.
///
/// Norm rounding rules are specified behaviour, not presentation: "round the
/// result to 0,1 %" is part of the method. Binary floating point cannot express
/// that faithfully, and it makes exact round-tripping impossible -- 450 l stored
/// as 0,45 m3 and rendered back is not reliably 450.
///
/// This type carries no decimal-place tag. Declared precision belongs to the
/// unit and quantity layer; rounding is an explicit operation (rounding.hpp) and,
/// from spec phase 8 on, a node in the expression tree.

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/error.hpp>

#include <bit>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// Always false, but dependent on T, so a static_assert inside a template
    /// only fires when that template is actually instantiated.
    template <typename T>
    inline constexpr bool AlwaysFalse = false;

    [[nodiscard]] constexpr std::strong_ordering invert(std::strong_ordering order) noexcept
    {
        if (order == std::strong_ordering::less)
            return std::strong_ordering::greater;
        if (order == std::strong_ordering::greater)
            return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }
} // namespace detail

/// An exact rational number, always in lowest terms with a positive denominator.
class Rational
{
  public:
    using Int = detail::Int;

    /// Zero.
    constexpr Rational() noexcept = default;

    /// An integer is a rational, exactly and without narrowing.
    constexpr Rational(Int whole) noexcept: _numerator { whole } {}

    /// Deliberately unusable. A double is a binary fraction: `Rational r = 0.45;`
    /// would silently mean 8106479329266893 / 2^54, not 9 / 20.
    template <typename T>
        requires std::is_floating_point_v<T>
    constexpr Rational(T)
    {
        static_assert(detail::AlwaysFalse<T>,
                      "formula: a floating-point value is not an exact rational; use "
                      "Rational::from_decimal(mantissa, exponent) for an exact decimal, "
                      "rational_from_double(value, places, mode) to round one, or "
                      "Rational::from_double_exact(value) for the exact binary value");
    }

    /// @throws ArithmeticException on a zero denominator or on overflow.
    constexpr Rational(Int numerator, Int denominator): Rational { detail::or_throw(make(numerator, denominator)) } {}

    /// Canonicalising factory. The only place the class invariants are established.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> make(Int numerator,
                                                                                 Int denominator) noexcept
    {
        if (denominator == 0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        if (numerator == 0)
            return Rational {};

        // Reduce in the unsigned domain so that IntMin is an ordinary operand.
        std::uint64_t const numeratorMagnitude = detail::magnitude(numerator);
        std::uint64_t const denominatorMagnitude = detail::magnitude(denominator);
        std::uint64_t const common = detail::gcd(numeratorMagnitude, denominatorMagnitude);
        std::uint64_t const reducedNumerator = numeratorMagnitude / common;
        std::uint64_t const reducedDenominator = denominatorMagnitude / common;

        bool const negative = (numerator < 0) != (denominator < 0);

        constexpr std::uint64_t PositiveLimit = static_cast<std::uint64_t>(detail::IntMax);
        std::uint64_t const numeratorLimit = negative ? PositiveLimit + 1U : PositiveLimit;
        if (reducedDenominator > PositiveLimit || reducedNumerator > numeratorLimit)
            return std::unexpected { ArithmeticError::Overflow };

        Rational result {};
        // Well defined since C++20: conversion to a signed type is modular.
        result._numerator = negative ? static_cast<Int>(0U - reducedNumerator) : static_cast<Int>(reducedNumerator);
        result._denominator = static_cast<Int>(reducedDenominator);
        return result;
    }

    /// `mantissa * 10^exponent`, exactly. The preferred way to write a decimal:
    /// `from_decimal(45, -2)` is 9/20, not the nearest double to 0,45.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from_decimal(Int mantissa,
                                                                                         int exponent) noexcept
    {
        if (mantissa == 0)
            return Rational {};
        if (exponent >= 0)
        {
            std::optional<Int> const scaled = detail::mul_pow10(mantissa, exponent);
            if (!scaled)
                return std::unexpected { ArithmeticError::Overflow };
            return Rational { *scaled };
        }
        std::optional<Int> const denominator = detail::pow10(-exponent);
        if (!denominator)
            return std::unexpected { ArithmeticError::Overflow };
        return make(mantissa, *denominator);
    }

    /// The exact value of the double, which is a dyadic rational. Usually not
    /// what a norm means: 0,45 as a double is not 9/20. Prefer from_decimal, or
    /// rational_from_double when the input genuinely is a measured double.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from_double_exact(double value) noexcept
    {
        static_assert(std::numeric_limits<double>::is_iec559, "formula: from_double_exact assumes IEEE-754 doubles");

        std::uint64_t const bits = std::bit_cast<std::uint64_t>(value);
        bool const negative = (bits >> 63) != 0U;
        auto const rawExponent = static_cast<int>((bits >> 52) & 0x7FFU);
        std::uint64_t const rawMantissa = bits & 0xF'FFFF'FFFF'FFFFULL;

        if (rawExponent == 0x7FF)
            return std::unexpected { ArithmeticError::NotFinite };

        // Subnormals have no implicit leading bit and a fixed exponent of -1074.
        std::uint64_t const significand = rawExponent == 0 ? rawMantissa : rawMantissa | (1ULL << 52);
        if (significand == 0)
            return Rational {};
        int const exponent = (rawExponent == 0 ? 1 : rawExponent) - 1075;

        // Strip trailing zero bits so the exponent needed is as small as possible.
        std::uint64_t reduced = significand;
        int shifted = exponent;
        while ((reduced & 1U) == 0U)
        {
            reduced >>= 1U;
            ++shifted;
        }

        if (reduced > static_cast<std::uint64_t>(detail::IntMax))
            return std::unexpected { ArithmeticError::Overflow };
        auto numerator = static_cast<Int>(reduced);
        if (negative)
            numerator = -numerator;

        if (shifted >= 0)
        {
            if (shifted >= 63)
                return std::unexpected { ArithmeticError::Overflow };
            std::optional<Int> const scaled = detail::checked_mul(numerator, Int { 1 } << shifted);
            if (!scaled)
                return std::unexpected { ArithmeticError::Overflow };
            return Rational { *scaled };
        }

        if (-shifted >= 63)
            return std::unexpected { ArithmeticError::Overflow };
        return make(numerator, Int { 1 } << -shifted);
    }

    [[nodiscard]] constexpr Int numerator() const noexcept { return _numerator; }
    [[nodiscard]] constexpr Int denominator() const noexcept { return _denominator; }

    [[nodiscard]] constexpr bool is_integer() const noexcept { return _denominator == 1; }
    [[nodiscard]] constexpr bool is_zero() const noexcept { return _numerator == 0; }

    [[nodiscard]] constexpr int sign() const noexcept { return _numerator == 0 ? 0 : (_numerator < 0 ? -1 : 1); }

    /// Lossy by construction. Named so that every loss of exactness is visible
    /// at the call site.
    [[nodiscard]] constexpr double to_double() const noexcept
    {
        return static_cast<double>(_numerator) / static_cast<double>(_denominator);
    }

    /// Exact for every representable pair. Uses the continued-fraction
    /// (Euclidean) comparison, which performs no multiplication: cross
    /// multiplication would overflow for operands that are individually fine.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(Rational const& other) const noexcept
    {
        Int leftNumerator = _numerator;
        Int leftDenominator = _denominator;
        Int rightNumerator = other._numerator;
        Int rightDenominator = other._denominator;
        bool reversed = false;

        for (;;)
        {
            auto const left = detail::floor_divmod(leftNumerator, leftDenominator);
            auto const right = detail::floor_divmod(rightNumerator, rightDenominator);

            if (left.quotient != right.quotient)
            {
                std::strong_ordering const order = left.quotient <=> right.quotient;
                return reversed ? detail::invert(order) : order;
            }

            if (left.remainder == 0 || right.remainder == 0)
            {
                std::strong_ordering order = std::strong_ordering::equal;
                if (left.remainder == 0 && right.remainder != 0)
                    order = std::strong_ordering::less;
                else if (left.remainder != 0 && right.remainder == 0)
                    order = std::strong_ordering::greater;
                return reversed ? detail::invert(order) : order;
            }

            // Compare the reciprocals of the fractional parts, which flips the sense.
            leftNumerator = leftDenominator;
            leftDenominator = left.remainder;
            rightNumerator = rightDenominator;
            rightDenominator = right.remainder;
            reversed = !reversed;
        }
    }

    [[nodiscard]] constexpr bool operator==(Rational const& other) const noexcept
    {
        // Canonical form makes this a componentwise comparison.
        return _numerator == other._numerator && _denominator == other._denominator;
    }

  private:
    Int _numerator { 0 };
    Int _denominator { 1 };
};

} // namespace formula
```

Implementation notes the implementer must not lose:
- `make` is the **only** place the invariants are established. Every later operation ends in a `make` call.
- The comparison loop terminates because each iteration replaces a denominator with a strictly smaller remainder (Euclid).
- `operator==` may compare componentwise **only** because of invariant 2. If canonicalisation is ever relaxed, this breaks silently.

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Verify on clang-cl too, since `std::bit_cast` and `<=>` are the risky parts**

```
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS on both.

- [ ] **Step 6: Commit**

```bash
git add include/formula-cpp/rational.hpp test/rational_tests.cpp test/CMakeLists.txt
git commit -m "feat(numbers): add the exact Rational type"
```

---

### Task 4: `Rational` arithmetic, and the must-not-compile guard

**Files:**
- Modify: `include/formula-cpp/rational.hpp` (append the free functions after the class)
- Modify: `test/rational_tests.cpp` (append the arithmetic cases)
- Create: `test/negative/rational_from_floating_point.cpp`
- Modify: `test/CMakeLists.txt` (register the negative case)

**Interfaces:**
- Consumes: `Rational` from Task 3.
- Produces, in `namespace formula`:
  - `constexpr std::expected<Rational, ArithmeticError> checked_add(Rational, Rational) noexcept`
  - `constexpr std::expected<Rational, ArithmeticError> checked_sub(Rational, Rational) noexcept`
  - `constexpr std::expected<Rational, ArithmeticError> checked_mul(Rational, Rational) noexcept`
  - `constexpr std::expected<Rational, ArithmeticError> checked_div(Rational, Rational) noexcept`
  - `constexpr std::expected<Rational, ArithmeticError> checked_pow(Rational, int) noexcept`
  - `constexpr std::expected<Rational, ArithmeticError> reciprocal(Rational) noexcept`
  - `constexpr Rational operator+(Rational, Rational)`, and the same for `-`, `*`, `/`
  - `constexpr Rational& operator+=(Rational&, Rational)`, and the same for `-=`, `*=`, `/=`
  - `constexpr Rational operator-(Rational)` and `constexpr Rational operator+(Rational)`
  - `constexpr Rational abs(Rational)` — `@throws` only for `numerator() == IntMin`
  - `constexpr Rational pow(Rational, int)`

**Overflow strategy, which the implementer must follow rather than re-derive:** reduce
*before* multiplying. Two operands that are each in range can produce an intermediate product
that is not, even when the final canonical result is perfectly representable. Cross-reducing
first makes the overflow-reporting path rare instead of routine.

- [ ] **Step 1: Write the failing test**

Append to `test/rational_tests.cpp`:

```cpp
// ---- arithmetic ----

static_assert(exact(1, 2) + exact(1, 3) == exact(5, 6));
static_assert(exact(1, 2) - exact(1, 3) == exact(1, 6));
static_assert(exact(2, 3) * exact(3, 4) == exact(1, 2));
static_assert(exact(2, 3) / exact(4, 9) == exact(3, 2));
static_assert(exact(1, 2) + exact(1, 2) == Rational { 1 });
static_assert(exact(1, 2) - exact(1, 2) == Rational { 0 });
static_assert((exact(1, 2) - exact(1, 2)).denominator() == 1);

static_assert(-exact(1, 2) == exact(-1, 2));
static_assert(+exact(1, 2) == exact(1, 2));
static_assert(-Rational { 0 } == Rational { 0 });

static_assert(formula::abs(exact(-3, 4)) == exact(3, 4));
static_assert(formula::abs(exact(3, 4)) == exact(3, 4));
static_assert(formula::abs(Rational { 0 }) == Rational { 0 });

static_assert(formula::pow(exact(2, 3), 0) == Rational { 1 });
static_assert(formula::pow(exact(2, 3), 2) == exact(4, 9));
static_assert(formula::pow(exact(2, 3), -2) == exact(9, 4));
static_assert(formula::pow(Rational { 10 }, 18) == Rational { 1000000000000000000 });

static_assert(formula::reciprocal(exact(2, 3)) == exact(3, 2));
static_assert(formula::reciprocal(exact(-2, 3)) == exact(-3, 2));
static_assert(!formula::reciprocal(Rational { 0 }).has_value());

static_assert(!formula::checked_div(Rational { 1 }, Rational { 0 }).has_value());
static_assert(formula::checked_div(Rational { 1 }, Rational { 0 }).error() == ArithmeticError::DivisionByZero);
static_assert(!formula::checked_add(Rational { IntMax }, Rational { 1 }).has_value());
static_assert(formula::checked_add(Rational { IntMax }, Rational { 1 }).error() == ArithmeticError::Overflow);
static_assert(!formula::checked_pow(Rational { 10 }, 19).has_value());

// Cross-reduction must make this succeed: the naive product of the numerators
// would overflow, but the canonical result is simply 1.
static_assert(exact(IntMax, 3) * exact(3, IntMax) == Rational { 1 });

TEST_CASE("addition is exact where binary floating point is not", "[rational]")
{
    Rational const tenth = *Rational::from_decimal(1, -1);
    Rational sum {};
    for (int step = 0; step < 10; ++step)
        sum += tenth;

    CHECK(sum == Rational { 1 });
    CHECK(sum.numerator() == 1);
    CHECK(sum.denominator() == 1);
}

TEST_CASE("division by zero throws through the operator and reports through the checked form", "[rational]")
{
    CHECK_THROWS_AS(Rational(1) / Rational(0), ArithmeticException);

    auto const result = formula::checked_div(Rational { 1 }, Rational { 0 });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::DivisionByZero);
}

TEST_CASE("overflow is reported, never saturated", "[rational]")
{
    auto const result = formula::checked_mul(Rational { IntMax }, Rational { 2 });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);
    CHECK_THROWS_AS(Rational(IntMax) * Rational(2), ArithmeticException);
}

TEST_CASE("cross-reduction keeps representable results representable", "[rational]")
{
    Rational const left { IntMax, 3 };
    Rational const right { 3, IntMax };
    CHECK(left * right == Rational { 1 });

    Rational const half { 1, 2 };
    Rational const bigOdd { IntMax, 1 };
    CHECK((bigOdd * half).denominator() == 2);
}

TEST_CASE("compound assignment matches the binary operators", "[rational]")
{
    Rational value { 1, 2 };
    value += Rational { 1, 3 };
    CHECK(value == Rational(5, 6));
    value -= Rational { 1, 3 };
    CHECK(value == Rational(1, 2));
    value *= Rational { 4, 1 };
    CHECK(value == Rational(2, 1));
    value /= Rational { 4, 1 };
    CHECK(value == Rational(1, 2));
}

TEST_CASE("pow handles zero, positive and negative exponents", "[rational]")
{
    CHECK(formula::pow(Rational(5), 0) == Rational(1));
    CHECK(formula::pow(Rational(2), 10) == Rational(1024));
    CHECK(formula::pow(Rational(2), -3) == Rational(1, 8));
    CHECK_THROWS_AS(formula::pow(Rational(0), -1), ArithmeticException);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build --preset cl-debug
```

Expected: **compile error** — no `operator+` for `Rational`, no `formula::pow`, no `formula::checked_add`. This is the RED signal.

- [ ] **Step 3: Write the arithmetic**

Append to `include/formula-cpp/rational.hpp`, inside `namespace formula`, after the class:

```cpp
/// Reciprocal. Fails on zero, and on the one value whose reciprocal is not
/// representable.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> reciprocal(Rational value) noexcept
{
    if (value.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    return Rational::make(value.denominator(), value.numerator());
}

/// Sign flip. Fails only for the one numerator whose negation is not representable.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> negate(Rational value) noexcept
{
    if (value.numerator() == detail::IntMin)
        return std::unexpected { ArithmeticError::Overflow };
    // Already canonical: negating the numerator preserves both invariants.
    return Rational::make(-value.numerator(), value.denominator());
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_add(Rational lhs, Rational rhs) noexcept
{
    // Scale by the least common multiple rather than by the product: with
    // denominators 6 and 10 this uses 30, not 60 -- the difference between
    // fitting and overflowing once denominators get large.
    auto const common = static_cast<Rational::Int>(
        detail::gcd(static_cast<std::uint64_t>(lhs.denominator()), static_cast<std::uint64_t>(rhs.denominator())));
    Rational::Int const leftScale = lhs.denominator() / common;
    Rational::Int const rightScale = rhs.denominator() / common;

    std::optional<Rational::Int> const leftTerm = detail::checked_mul(lhs.numerator(), rightScale);
    std::optional<Rational::Int> const rightTerm = detail::checked_mul(rhs.numerator(), leftScale);
    if (!leftTerm || !rightTerm)
        return std::unexpected { ArithmeticError::Overflow };

    std::optional<Rational::Int> const numerator = detail::checked_add(*leftTerm, *rightTerm);
    std::optional<Rational::Int> const denominator = detail::checked_mul(leftScale, rhs.denominator());
    if (!numerator || !denominator)
        return std::unexpected { ArithmeticError::Overflow };

    return Rational::make(*numerator, *denominator);
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_sub(Rational lhs, Rational rhs) noexcept
{
    std::expected<Rational, ArithmeticError> const negated = negate(rhs);
    if (!negated)
        return negated;
    return checked_add(lhs, *negated);
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_mul(Rational lhs, Rational rhs) noexcept
{
    // Cross-reduce before multiplying: (IntMax/3) * (3/IntMax) is exactly 1, but
    // multiplying the numerators first would overflow.
    //
    // Each gcd divides a denominator, so it is positive and at most IntMax.
    // Signed division by it is therefore always safe -- the only signed division
    // that can overflow is IntMin / -1.
    auto const leftCross = static_cast<Rational::Int>(
        detail::gcd(detail::magnitude(lhs.numerator()), static_cast<std::uint64_t>(rhs.denominator())));
    auto const rightCross = static_cast<Rational::Int>(
        detail::gcd(detail::magnitude(rhs.numerator()), static_cast<std::uint64_t>(lhs.denominator())));

    Rational::Int const leftNumerator = lhs.numerator() / leftCross;
    Rational::Int const rightNumerator = rhs.numerator() / rightCross;
    Rational::Int const leftDenominator = lhs.denominator() / rightCross;
    Rational::Int const rightDenominator = rhs.denominator() / leftCross;

    std::optional<Rational::Int> const numerator = detail::checked_mul(leftNumerator, rightNumerator);
    std::optional<Rational::Int> const denominator = detail::checked_mul(leftDenominator, rightDenominator);
    if (!numerator || !denominator)
        return std::unexpected { ArithmeticError::Overflow };

    return Rational::make(*numerator, *denominator);
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_div(Rational lhs, Rational rhs) noexcept
{
    if (rhs.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    std::expected<Rational, ArithmeticError> const inverted = reciprocal(rhs);
    if (!inverted)
        return inverted;
    return checked_mul(lhs, *inverted);
}

/// Integer power. A negative exponent inverts, so `pow(0, -1)` is a division by zero.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_pow(Rational base, int exponent) noexcept
{
    if (exponent == 0)
        return Rational { 1 };

    bool const invertResult = exponent < 0;
    // Widen before negating: -INT_MIN would overflow int.
    long long remaining = invertResult ? -static_cast<long long>(exponent) : static_cast<long long>(exponent);

    Rational result { 1 };
    Rational factor = base;
    while (remaining > 0)
    {
        if ((remaining & 1) != 0)
        {
            std::expected<Rational, ArithmeticError> const next = checked_mul(result, factor);
            if (!next)
                return next;
            result = *next;
        }
        remaining >>= 1;
        if (remaining > 0)
        {
            std::expected<Rational, ArithmeticError> const squared = checked_mul(factor, factor);
            if (!squared)
                return squared;
            factor = *squared;
        }
    }

    if (!invertResult)
        return result;
    return reciprocal(result);
}

// ---- the operator layer: total-looking, but never silently wrong ----

[[nodiscard]] constexpr Rational operator+(Rational lhs, Rational rhs) { return detail::or_throw(checked_add(lhs, rhs)); }
[[nodiscard]] constexpr Rational operator-(Rational lhs, Rational rhs) { return detail::or_throw(checked_sub(lhs, rhs)); }
[[nodiscard]] constexpr Rational operator*(Rational lhs, Rational rhs) { return detail::or_throw(checked_mul(lhs, rhs)); }
[[nodiscard]] constexpr Rational operator/(Rational lhs, Rational rhs) { return detail::or_throw(checked_div(lhs, rhs)); }

constexpr Rational& operator+=(Rational& lhs, Rational rhs) { return lhs = lhs + rhs; }
constexpr Rational& operator-=(Rational& lhs, Rational rhs) { return lhs = lhs - rhs; }
constexpr Rational& operator*=(Rational& lhs, Rational rhs) { return lhs = lhs * rhs; }
constexpr Rational& operator/=(Rational& lhs, Rational rhs) { return lhs = lhs / rhs; }

[[nodiscard]] constexpr Rational operator+(Rational value) noexcept { return value; }
[[nodiscard]] constexpr Rational operator-(Rational value) { return detail::or_throw(negate(value)); }

[[nodiscard]] constexpr Rational abs(Rational value)
{
    return value.sign() < 0 ? -value : value;
}

[[nodiscard]] constexpr Rational pow(Rational base, int exponent)
{
    return detail::or_throw(checked_pow(base, exponent));
}
```

Ordering matters in a header with no forward declarations: `negate` must appear before
`checked_sub` and before `operator-(Rational)`, and `reciprocal` before `checked_div` and
`checked_pow`. The listing above is already in a valid order — keep it.

Two things that look like style but are not:
- `abs` is declared as throwing (no `noexcept`) because `abs` of the single value
  `IntMin / 1` is not representable. Silently returning a negative "absolute value" there is
  exactly the class of bug this library exists to prevent.
- The operators take `Rational` **by value**. It is two `int64_t`s; a reference would be larger
  than the object on every target we support.

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Add the must-not-compile guard**

Create `test/negative/rational_from_floating_point.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// A double is a binary fraction. Letting it convert implicitly would make
// `Rational r = 0.45;` mean 8106479329266893 / 2^54 rather than 9 / 20, and
// nothing downstream could tell. This must not compile.
#include <formula-cpp/rational.hpp>

formula::Rational const ratio = 0.45;

int main()
{
    return ratio.is_zero() ? 1 : 0;
}
```

Register it in `test/CMakeLists.txt`, next to the existing negative case:

```cmake
formula_add_negative_test(rational_from_floating_point
    "formula: a floating-point value is not an exact rational")
```

The expected text is a literal substring of the library's own `static_assert` message, matched
with `string(FIND)` — it is **not** a regular expression, so it needs no escaping.

- [ ] **Step 6: Run the negative test to verify it passes**

```
ctest --preset cl-debug -R negative --output-on-failure
```

Expected: both negative cases pass — the compile fails *and* the failure names our message.
Verify the test is meaningful by temporarily changing the expected text to something absent
(for example `"formula: this text is not in any diagnostic"`) and confirming the test then
**fails**. Restore the correct text afterwards.

- [ ] **Step 7: Verify on clang-cl**

```
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS, including both negative cases. MSVC wraps the message as
`error C2338: static assertion failed: '<message>'` and clang prints it directly; the chosen
substring appears verbatim in both.

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/rational.hpp test/rational_tests.cpp test/negative/rational_from_floating_point.cpp test/CMakeLists.txt
git commit -m "feat(numbers): add exact Rational arithmetic"
```

---

### Task 5: Rounding modes, and rounding to an integer or to a multiple

**Files:**
- Create: `include/formula-cpp/rounding.hpp`
- Create: `test/rounding_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `rounding_tests.cpp` to the test executable)

**Interfaces:**
- Consumes: `Rational` and its arithmetic from Tasks 3–4.
- Produces, in `namespace formula`:
  - `enum class RoundingMode : std::uint8_t { HalfAwayFromZero, HalfTowardZero, HalfEven, Ceiling, Floor, TowardZero, AwayFromZero };`
  - `constexpr std::string_view describe(RoundingMode) noexcept`
  - `struct DecimalPlaces { std::int32_t value {}; }` with defaulted `operator==`
  - `struct SignificantDigits { std::int32_t value {}; }` with defaulted `operator==`
  - `constexpr std::expected<Rational::Int, ArithmeticError> checked_round_to_int(Rational, RoundingMode) noexcept`
  - `constexpr Rational::Int round_to_int(Rational, RoundingMode)`
  - `constexpr std::expected<Rational, ArithmeticError> checked_round_to_integer(Rational, RoundingMode) noexcept`
  - `constexpr Rational round_to_integer(Rational, RoundingMode)`
  - `constexpr std::expected<Rational, ArithmeticError> checked_round_to_multiple(Rational value, Rational step, RoundingMode) noexcept`
  - `constexpr Rational round_to_multiple(Rational value, Rational step, RoundingMode)`

**Why seven modes.** Norm text distinguishes them and they are not interchangeable.
*Aufgerundet* on a non-negative mass means `Ceiling`; on a signed quantity `Ceiling` and
`AwayFromZero` differ, and picking the wrong one is a silent off-by-one-ulp-of-the-scale error.
`HalfEven` exists because it is the IEEE-754 decimal default and some methods inherit it.
Each mode must be tested on a positive value, a negative value and an exact tie.

- [ ] **Step 1: Write the failing test**

Create `test/rounding_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rounding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iterator> // std::size, used by the distinctness loops
#include <limits>   // std::numeric_limits, used by Task 6's cases

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Rational;
using formula::RoundingMode;

namespace
{
consteval Rational exact(Rational::Int numerator, Rational::Int denominator)
{
    return Rational { numerator, denominator };
}

constexpr Rational::Int round_int(Rational::Int numerator, Rational::Int denominator, RoundingMode mode)
{
    return formula::round_to_int(Rational { numerator, denominator }, mode);
}
} // namespace

// ---- the seven modes, on a positive value that is not a tie: 7/4 = 1.75 ----

static_assert(round_int(7, 4, RoundingMode::HalfAwayFromZero) == 2);
static_assert(round_int(7, 4, RoundingMode::HalfTowardZero) == 2);
static_assert(round_int(7, 4, RoundingMode::HalfEven) == 2);
static_assert(round_int(7, 4, RoundingMode::Ceiling) == 2);
static_assert(round_int(7, 4, RoundingMode::Floor) == 1);
static_assert(round_int(7, 4, RoundingMode::TowardZero) == 1);
static_assert(round_int(7, 4, RoundingMode::AwayFromZero) == 2);

// ---- the same on a negative value: -7/4 = -1.75 ----

static_assert(round_int(-7, 4, RoundingMode::HalfAwayFromZero) == -2);
static_assert(round_int(-7, 4, RoundingMode::HalfTowardZero) == -2);
static_assert(round_int(-7, 4, RoundingMode::HalfEven) == -2);
static_assert(round_int(-7, 4, RoundingMode::Ceiling) == -1);
static_assert(round_int(-7, 4, RoundingMode::Floor) == -2);
static_assert(round_int(-7, 4, RoundingMode::TowardZero) == -1);
static_assert(round_int(-7, 4, RoundingMode::AwayFromZero) == -2);

// ---- exact ties, where the half-* modes finally differ: 3/2 and 5/2 ----

static_assert(round_int(3, 2, RoundingMode::HalfAwayFromZero) == 2);
static_assert(round_int(3, 2, RoundingMode::HalfTowardZero) == 1);
static_assert(round_int(3, 2, RoundingMode::HalfEven) == 2);
static_assert(round_int(5, 2, RoundingMode::HalfAwayFromZero) == 3);
static_assert(round_int(5, 2, RoundingMode::HalfTowardZero) == 2);
static_assert(round_int(5, 2, RoundingMode::HalfEven) == 2);

static_assert(round_int(-3, 2, RoundingMode::HalfAwayFromZero) == -2);
static_assert(round_int(-3, 2, RoundingMode::HalfTowardZero) == -1);
static_assert(round_int(-3, 2, RoundingMode::HalfEven) == -2);
static_assert(round_int(-5, 2, RoundingMode::HalfAwayFromZero) == -3);
static_assert(round_int(-5, 2, RoundingMode::HalfTowardZero) == -2);
static_assert(round_int(-5, 2, RoundingMode::HalfEven) == -2);

// ---- an exact integer is unchanged under every mode ----

static_assert(round_int(4, 1, RoundingMode::Floor) == 4);
static_assert(round_int(4, 1, RoundingMode::Ceiling) == 4);
static_assert(round_int(-4, 1, RoundingMode::AwayFromZero) == -4);
static_assert(round_int(0, 1, RoundingMode::HalfEven) == 0);

// ---- rounding to a multiple ----

static_assert(formula::round_to_multiple(exact(7, 1), exact(5, 1), RoundingMode::HalfAwayFromZero) == exact(5, 1));
static_assert(formula::round_to_multiple(exact(8, 1), exact(5, 1), RoundingMode::HalfAwayFromZero) == exact(10, 1));
static_assert(formula::round_to_multiple(exact(7, 2), exact(1, 4), RoundingMode::Floor) == exact(7, 2));
static_assert(formula::round_to_multiple(exact(1, 3), exact(1, 4), RoundingMode::Ceiling) == exact(1, 2));
static_assert(formula::round_to_multiple(exact(1, 3), exact(1, 4), RoundingMode::Floor) == exact(1, 4));

static_assert(
    formula::checked_round_to_multiple(exact(1, 3), Rational { 0 }, RoundingMode::Floor).error()
    == ArithmeticError::DomainError);
static_assert(
    formula::checked_round_to_multiple(exact(1, 3), exact(-1, 4), RoundingMode::Floor).error()
    == ArithmeticError::DomainError);

TEST_CASE("every mode is exercised on positive, negative and tie inputs", "[rounding]")
{
    struct Case
    {
        RoundingMode mode;
        Rational::Int positive;   // 1.75
        Rational::Int negative;   // -1.75
        Rational::Int tieUp;      // 1.5
        Rational::Int tieDown;    // 2.5
    };

    Case const cases[] = {
        { RoundingMode::HalfAwayFromZero, 2, -2, 2, 3 }, { RoundingMode::HalfTowardZero, 2, -2, 1, 2 },
        { RoundingMode::HalfEven, 2, -2, 2, 2 },         { RoundingMode::Ceiling, 2, -1, 2, 3 },
        { RoundingMode::Floor, 1, -2, 1, 2 },            { RoundingMode::TowardZero, 1, -1, 1, 2 },
        { RoundingMode::AwayFromZero, 2, -2, 2, 3 },
    };

    for (Case const& testCase: cases)
    {
        INFO("mode = " << formula::describe(testCase.mode));
        CHECK(formula::round_to_int(Rational(7, 4), testCase.mode) == testCase.positive);
        CHECK(formula::round_to_int(Rational(-7, 4), testCase.mode) == testCase.negative);
        CHECK(formula::round_to_int(Rational(3, 2), testCase.mode) == testCase.tieUp);
        CHECK(formula::round_to_int(Rational(5, 2), testCase.mode) == testCase.tieDown);
    }
}

TEST_CASE("each mode has a distinct description", "[rounding]")
{
    RoundingMode const modes[] = { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero,
                                   RoundingMode::HalfEven,        RoundingMode::Ceiling,
                                   RoundingMode::Floor,           RoundingMode::TowardZero,
                                   RoundingMode::AwayFromZero };

    for (std::size_t i = 0; i < std::size(modes); ++i)
    {
        CHECK_FALSE(formula::describe(modes[i]).empty());
        for (std::size_t j = i + 1; j < std::size(modes); ++j)
            CHECK(formula::describe(modes[i]) != formula::describe(modes[j]));
    }
}

TEST_CASE("a non-positive step is a domain error, not a crash", "[rounding]")
{
    CHECK(formula::checked_round_to_multiple(Rational(1, 3), Rational(0), RoundingMode::Floor).error()
          == ArithmeticError::DomainError);
    CHECK_THROWS_AS(formula::round_to_multiple(Rational(1, 3), Rational(0), RoundingMode::Floor),
                    ArithmeticException);
}

TEST_CASE("round_to_integer returns a Rational, round_to_int returns an integer", "[rounding]")
{
    Rational const rounded = formula::round_to_integer(Rational(7, 4), RoundingMode::HalfAwayFromZero);
    CHECK(rounded == Rational(2));
    CHECK(rounded.is_integer());
    CHECK(formula::round_to_int(Rational(7, 4), RoundingMode::HalfAwayFromZero) == 2);
}
```

- [ ] **Step 2: Add the test file to the build and run it to verify it fails**

Add `rounding_tests.cpp` to `add_executable(formula-cpp-tests ...)`, then:

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `formula-cpp/rounding.hpp` not found.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/rounding.hpp`. Task 6 appends to the same file.

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Rounding as specified behaviour, not as formatting.
///
/// Norm methods state where rounding happens and which way it goes, and
/// intermediate and final rounding routinely differ within one method. A library
/// that rounds only on output produces wrong numbers, so rounding here is an
/// operation over exact values that yields another exact value. From spec phase
/// 8 on it is also a node in the expression tree, and the result carries into
/// the trace.

#include <formula-cpp/error.hpp>
#include <formula-cpp/rational.hpp>

#include <cstdint>
#include <expected>
#include <string_view>

namespace formula
{

/// How a value that falls between two representable results is resolved.
///
/// The four non-half modes are unconditional: they ignore how close the value is
/// and always move the same way. The three half modes move to the nearer result
/// and differ only on an exact tie.
enum class RoundingMode : std::uint8_t
{
    /// Nearest; ties move to the larger magnitude. 1,25 -> 1,3 and -1,25 -> -1,3.
    HalfAwayFromZero,
    /// Nearest; ties move to the smaller magnitude. 1,25 -> 1,2 and -1,25 -> -1,2.
    HalfTowardZero,
    /// Nearest; ties move to the neighbour whose last kept digit is even.
    /// 1,25 -> 1,2 and 1,35 -> 1,4. The IEEE-754 decimal default.
    HalfEven,
    /// Always toward positive infinity. 1,21 -> 1,3 and -1,21 -> -1,2.
    Ceiling,
    /// Always toward negative infinity. 1,29 -> 1,2 and -1,21 -> -1,3.
    Floor,
    /// Always toward zero; plain truncation. 1,29 -> 1,2 and -1,29 -> -1,2.
    TowardZero,
    /// Always away from zero. 1,21 -> 1,3 and -1,21 -> -1,3.
    /// On a non-negative quantity this coincides with Ceiling, which is what
    /// "rounded up" usually means in norm text; on a signed quantity it does not.
    AwayFromZero,
};

[[nodiscard]] constexpr std::string_view describe(RoundingMode mode) noexcept
{
    switch (mode)
    {
        case RoundingMode::HalfAwayFromZero: return "nearest, ties away from zero";
        case RoundingMode::HalfTowardZero: return "nearest, ties toward zero";
        case RoundingMode::HalfEven: return "nearest, ties to even";
        case RoundingMode::Ceiling: return "toward positive infinity";
        case RoundingMode::Floor: return "toward negative infinity";
        case RoundingMode::TowardZero: return "toward zero";
        case RoundingMode::AwayFromZero: return "away from zero";
    }
    return "unknown rounding mode";
}

/// A decimal scale. Negative values are meaningful: DecimalPlaces { -1 } rounds
/// to whole tens, which norms do ask for.
struct DecimalPlaces
{
    std::int32_t value {};
    [[nodiscard]] constexpr bool operator==(DecimalPlaces const&) const noexcept = default;
};

/// A count of significant digits. Must be at least 1.
struct SignificantDigits
{
    std::int32_t value {};
    [[nodiscard]] constexpr bool operator==(SignificantDigits const&) const noexcept = default;
};

/// Rounds to a whole number under `mode`.
///
/// The whole decision is expressed against a floored quotient, so the remainder
/// is always in `[0, denominator)` and "which side is it on" is a comparison of
/// `remainder` with `denominator - remainder` -- no doubling, so no overflow.
[[nodiscard]] constexpr std::expected<Rational::Int, ArithmeticError> checked_round_to_int(
    Rational value, RoundingMode mode) noexcept
{
    auto const split = detail::floor_divmod(value.numerator(), value.denominator());
    if (split.remainder == 0)
        return split.quotient;

    auto const toCeiling = [&]() -> std::expected<Rational::Int, ArithmeticError> {
        std::optional<Rational::Int> const raised = detail::checked_add(split.quotient, Rational::Int { 1 });
        if (!raised)
            return std::unexpected { ArithmeticError::Overflow };
        return *raised;
    };

    bool const positive = value.numerator() > 0;

    switch (mode)
    {
        case RoundingMode::Floor: return split.quotient;
        case RoundingMode::Ceiling: return toCeiling();
        case RoundingMode::TowardZero: return positive ? std::expected<Rational::Int, ArithmeticError> { split.quotient }
                                                       : toCeiling();
        case RoundingMode::AwayFromZero:
            return positive ? toCeiling() : std::expected<Rational::Int, ArithmeticError> { split.quotient };
        default: break;
    }

    // A half mode: compare the distance down with the distance up.
    Rational::Int const distanceUp = value.denominator() - split.remainder;
    if (split.remainder < distanceUp)
        return split.quotient;
    if (split.remainder > distanceUp)
        return toCeiling();

    switch (mode)
    {
        case RoundingMode::HalfAwayFromZero:
            return positive ? toCeiling() : std::expected<Rational::Int, ArithmeticError> { split.quotient };
        case RoundingMode::HalfTowardZero:
            return positive ? std::expected<Rational::Int, ArithmeticError> { split.quotient } : toCeiling();
        case RoundingMode::HalfEven:
            return split.quotient % 2 == 0 ? std::expected<Rational::Int, ArithmeticError> { split.quotient }
                                           : toCeiling();
        default: break;
    }

    return std::unexpected { ArithmeticError::DomainError };
}

[[nodiscard]] constexpr Rational::Int round_to_int(Rational value, RoundingMode mode)
{
    return detail::or_throw(checked_round_to_int(value, mode));
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_integer(
    Rational value, RoundingMode mode) noexcept
{
    std::expected<Rational::Int, ArithmeticError> const rounded = checked_round_to_int(value, mode);
    if (!rounded)
        return std::unexpected { rounded.error() };
    return Rational { *rounded };
}

[[nodiscard]] constexpr Rational round_to_integer(Rational value, RoundingMode mode)
{
    return detail::or_throw(checked_round_to_integer(value, mode));
}

/// Rounds to the nearest multiple of `step` under `mode`.
///
/// This is the primitive the decimal-place and significant-digit forms are built
/// on, and it is also what snapping a computed sieve size onto a standard sieve
/// series needs (spec phase 12).
///
/// @pre `step` is strictly positive; otherwise DomainError.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_multiple(
    Rational value, Rational step, RoundingMode mode) noexcept
{
    if (step.sign() <= 0)
        return std::unexpected { ArithmeticError::DomainError };

    std::expected<Rational, ArithmeticError> const quotient = checked_div(value, step);
    if (!quotient)
        return quotient;

    std::expected<Rational::Int, ArithmeticError> const steps = checked_round_to_int(*quotient, mode);
    if (!steps)
        return std::unexpected { steps.error() };

    return checked_mul(Rational { *steps }, step);
}

[[nodiscard]] constexpr Rational round_to_multiple(Rational value, Rational step, RoundingMode mode)
{
    return detail::or_throw(checked_round_to_multiple(value, step, mode));
}

} // namespace formula
```

Implementation note: the lambda `toCeiling` and the repeated
`std::expected<Rational::Int, ArithmeticError> { ... }` spellings exist because the two arms of
a conditional operator must have the same type. If the implementer finds a cleaner spelling
that keeps every arm explicit, that is welcome; what must not change is that **every** mode is
handled and the `default` arms remain, so adding an eighth mode later is a visible gap rather
than a silent fall-through.

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/rounding.hpp test/rounding_tests.cpp test/CMakeLists.txt
git commit -m "feat(numbers): add rounding modes and rounding to an integer or multiple"
```

---

### Task 6: Decimal places, significant digits, and rounded conversion from `double`

**Files:**
- Modify: `include/formula-cpp/rounding.hpp` (append)
- Modify: `test/rounding_tests.cpp` (append)

**Interfaces:**
- Consumes: everything from Task 5.
- Produces, in `namespace formula`:
  - `constexpr std::expected<int, ArithmeticError> checked_decimal_exponent(Rational) noexcept` — `floor(log10(|value|))`; DomainError on zero
  - `constexpr std::expected<Rational, ArithmeticError> checked_round(Rational, DecimalPlaces, RoundingMode) noexcept`
  - `constexpr Rational round(Rational, DecimalPlaces, RoundingMode)`
  - `constexpr std::expected<Rational, ArithmeticError> checked_round(Rational, SignificantDigits, RoundingMode) noexcept`
  - `constexpr Rational round(Rational, SignificantDigits, RoundingMode)`
  - `constexpr std::expected<Rational, ArithmeticError> rational_from_double(double, DecimalPlaces, RoundingMode) noexcept`

- [ ] **Step 1: Write the failing test**

Append to `test/rounding_tests.cpp`:

```cpp
// ---- decimal places ----

static_assert(formula::round(exact(1, 3), formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(33, -2));
static_assert(formula::round(exact(2, 3), formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(67, -2));
static_assert(formula::round(exact(2, 3), formula::DecimalPlaces { 2 }, RoundingMode::Floor)
              == *Rational::from_decimal(66, -2));
static_assert(formula::round(Rational { 1234 }, formula::DecimalPlaces { -2 }, RoundingMode::HalfAwayFromZero)
              == Rational { 1200 });
static_assert(formula::round(Rational { 1250 }, formula::DecimalPlaces { -2 }, RoundingMode::HalfEven)
              == Rational { 1200 });
static_assert(formula::round(Rational { 5 }, formula::DecimalPlaces { 0 }, RoundingMode::Floor) == Rational { 5 });

// ---- the decimal exponent, which significant digits is built on ----

static_assert(formula::checked_decimal_exponent(Rational { 1 }) == 0);
static_assert(formula::checked_decimal_exponent(Rational { 9 }) == 0);
static_assert(formula::checked_decimal_exponent(Rational { 10 }) == 1);
static_assert(formula::checked_decimal_exponent(Rational { 999 }) == 2);
static_assert(formula::checked_decimal_exponent(Rational { 1000 }) == 3);
static_assert(formula::checked_decimal_exponent(exact(1, 2)) == -1);
static_assert(formula::checked_decimal_exponent(*Rational::from_decimal(999, -3)) == -1);
static_assert(formula::checked_decimal_exponent(*Rational::from_decimal(1, -3)) == -3);
static_assert(formula::checked_decimal_exponent(Rational { -250 }) == 2);
static_assert(formula::checked_decimal_exponent(Rational { 0 }).error() == ArithmeticError::DomainError);

// ---- significant digits ----

static_assert(formula::round(Rational { 12345 }, formula::SignificantDigits { 3 }, RoundingMode::HalfAwayFromZero)
              == Rational { 12300 });
static_assert(formula::round(Rational { 12345 }, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero)
              == Rational { 12000 });
static_assert(formula::round(*Rational::from_decimal(123456, -6),
                             formula::SignificantDigits { 3 },
                             RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(123, -3));
static_assert(formula::round(Rational { 0 }, formula::SignificantDigits { 3 }, RoundingMode::Floor) == Rational { 0 });
static_assert(formula::round(Rational { -12345 }, formula::SignificantDigits { 3 }, RoundingMode::HalfAwayFromZero)
              == Rational { -12300 });
// Rounding across a power of ten still yields the requested digit count.
static_assert(formula::round(*Rational::from_decimal(999, -2),
                             formula::SignificantDigits { 2 },
                             RoundingMode::HalfAwayFromZero)
              == Rational { 10 });

static_assert(formula::checked_round(Rational { 5 }, formula::SignificantDigits { 0 }, RoundingMode::Floor).error()
              == ArithmeticError::DomainError);
static_assert(formula::checked_round(Rational { 5 }, formula::SignificantDigits { -1 }, RoundingMode::Floor).error()
              == ArithmeticError::DomainError);
static_assert(formula::checked_round(Rational { 5 }, formula::DecimalPlaces { 19 }, RoundingMode::Floor).error()
              == ArithmeticError::Overflow);

TEST_CASE("intermediate and final rounding compose, and the order matters", "[rounding]")
{
    // A method that rounds a mean to whole units before using it produces a
    // different answer from one that rounds only at the end. Both must be
    // expressible; neither is a formatting concern.
    Rational const mean = Rational(302, 3); // 100.666...

    Rational const roundedFirst =
        formula::round(mean, formula::DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero) * Rational(2);
    Rational const roundedLast =
        formula::round(mean * Rational(2), formula::DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero);

    CHECK(roundedFirst == Rational(202));
    CHECK(roundedLast == Rational(201));
    CHECK(roundedFirst != roundedLast);
}

TEST_CASE("rounding up is available separately from rounding to nearest", "[rounding]")
{
    Rational const value = *Rational::from_decimal(1001, -3); // 1.001

    CHECK(formula::round(value, formula::DecimalPlaces { 2 }, RoundingMode::Ceiling)
          == *Rational::from_decimal(101, -2));
    CHECK(formula::round(value, formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero)
          == Rational(1));
}

TEST_CASE("rational_from_double rounds a measured double onto a decimal scale", "[rounding]")
{
    auto const rounded = formula::rational_from_double(0.45, formula::DecimalPlaces { 2 },
                                                       RoundingMode::HalfAwayFromZero);
    REQUIRE(rounded.has_value());
    CHECK(rounded->numerator() == 9);
    CHECK(rounded->denominator() == 20);

    auto const third = formula::rational_from_double(1.0 / 3.0, formula::DecimalPlaces { 4 },
                                                     RoundingMode::HalfAwayFromZero);
    REQUIRE(third.has_value());
    CHECK(*third == *Rational::from_decimal(3333, -4));

    double const infinity = std::numeric_limits<double>::infinity();
    CHECK(formula::rational_from_double(infinity, formula::DecimalPlaces { 2 }, RoundingMode::Floor).error()
          == ArithmeticError::NotFinite);
}

TEST_CASE("significant digits and decimal places disagree, as they should", "[rounding]")
{
    Rational const value = *Rational::from_decimal(4567, -2); // 45.67

    CHECK(formula::round(value, formula::DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(457, -1));
    CHECK(formula::round(value, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero)
          == Rational(46));
}
```

`<limits>` and `<iterator>` are already in the include block from Task 5.

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build --preset cl-debug
```

Expected: **compile error** — no `formula::round`, no `formula::checked_decimal_exponent`, no
`formula::rational_from_double`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/rounding.hpp`, inside `namespace formula`:

```cpp
namespace detail
{
    /// Whether `|numerator| / denominator >= 10^exponent`, exactly and without
    /// ever constructing 10^exponent as a Rational -- which is impossible at the
    /// extremes of the representable range.
    [[nodiscard]] constexpr bool at_least_pow10(std::uint64_t numerator, std::uint64_t denominator,
                                                int exponent) noexcept
    {
        constexpr std::uint64_t Limit = static_cast<std::uint64_t>(IntMax);
        if (exponent >= 0)
        {
            if (exponent > 18)
                return false;
            std::uint64_t const factor = static_cast<std::uint64_t>(*pow10(exponent));
            // denominator * factor > Limit implies the scaled denominator already
            // exceeds any possible numerator, so the quotient is below 10^exponent.
            if (denominator > Limit / factor)
                return false;
            return numerator >= denominator * factor;
        }
        if (-exponent > 18)
            return true;
        std::uint64_t const factor = static_cast<std::uint64_t>(*pow10(-exponent));
        if (numerator > Limit / factor)
            return true;
        return numerator * factor >= denominator;
    }
} // namespace detail

/// `floor(log10(|value|))`: the exponent `e` with `10^e <= |value| < 10^(e+1)`.
///
/// @return DomainError for zero, which has no decimal exponent.
[[nodiscard]] constexpr std::expected<int, ArithmeticError> checked_decimal_exponent(Rational value) noexcept
{
    if (value.is_zero())
        return std::unexpected { ArithmeticError::DomainError };

    std::uint64_t const numerator = detail::magnitude(value.numerator());
    auto const denominator = static_cast<std::uint64_t>(value.denominator());

    // The digit counts bracket the answer to within one: with dn digits in the
    // numerator and dd in the denominator, floor(log10(n/d)) is either
    // dn - dd or dn - dd - 1.
    int exponent = detail::decimal_digits(value.numerator()) - detail::decimal_digits(value.denominator());
    if (!detail::at_least_pow10(numerator, denominator, exponent))
        --exponent;
    return exponent;
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round(Rational value, DecimalPlaces places,
                                                                               RoundingMode mode) noexcept
{
    // The step is 10^-places, which must itself be representable.
    if (places.value > 18 || places.value < -18)
        return std::unexpected { ArithmeticError::Overflow };

    std::expected<Rational, ArithmeticError> const step = Rational::from_decimal(1, -places.value);
    if (!step)
        return step;
    return checked_round_to_multiple(value, *step, mode);
}

[[nodiscard]] constexpr Rational round(Rational value, DecimalPlaces places, RoundingMode mode)
{
    return detail::or_throw(checked_round(value, places, mode));
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round(Rational value,
                                                                               SignificantDigits digits,
                                                                               RoundingMode mode) noexcept
{
    if (digits.value < 1)
        return std::unexpected { ArithmeticError::DomainError };
    if (value.is_zero())
        return value;

    std::expected<int, ArithmeticError> const exponent = checked_decimal_exponent(value);
    if (!exponent)
        return std::unexpected { exponent.error() };

    // Keeping `digits` digits of a value whose leading digit sits at 10^e means
    // rounding at the 10^(e - digits + 1) place.
    long long const places = static_cast<long long>(digits.value) - 1 - static_cast<long long>(*exponent);
    if (places > 18 || places < -18)
        return std::unexpected { ArithmeticError::Overflow };

    return checked_round(value, DecimalPlaces { static_cast<std::int32_t>(places) }, mode);
}

[[nodiscard]] constexpr Rational round(Rational value, SignificantDigits digits, RoundingMode mode)
{
    return detail::or_throw(checked_round(value, digits, mode));
}

/// Converts a measured `double` onto an exact decimal scale.
///
/// This is the honest conversion: a double carries no decimal precision of its
/// own, so the caller must say what scale the measurement is on. The exact binary
/// value is computed first and then rounded, so no decimal digit is invented.
///
/// Range: the exact binary value must itself be representable, which holds for
/// roughly |value| in [2^-62, 2^62]. Outside that, Overflow is reported rather
/// than a silently approximated result.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rational_from_double(double value,
                                                                                      DecimalPlaces places,
                                                                                      RoundingMode mode) noexcept
{
    std::expected<Rational, ArithmeticError> const exactValue = Rational::from_double_exact(value);
    if (!exactValue)
        return exactValue;
    return checked_round(*exactValue, places, mode);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Verify the decimal-exponent seed on both sides of every power of ten**

Add this loop to `test/rounding_tests.cpp` and confirm it passes — it is the check that the
"within one" argument behind `checked_decimal_exponent` actually holds:

```cpp
TEST_CASE("the decimal exponent is correct on both sides of every power of ten", "[rounding]")
{
    for (int exponent = -18; exponent <= 18; ++exponent)
    {
        auto const atPower = Rational::from_decimal(1, exponent);
        if (!atPower)
            continue;

        INFO("exponent = " << exponent);
        CHECK(formula::checked_decimal_exponent(*atPower) == exponent);

        // A hair below the power of ten must land one exponent lower. The
        // epsilon is not representable at the bottom of the range, so guard it
        // rather than dereferencing an empty expected.
        if (auto const epsilon = Rational::from_decimal(1, exponent - 3); epsilon)
        {
            auto const justBelow = formula::checked_sub(*atPower, *epsilon);
            if (justBelow && !justBelow->is_zero())
                CHECK(formula::checked_decimal_exponent(*justBelow) == exponent - 1);
        }

        if (auto const negated = formula::negate(*atPower); negated)
            CHECK(formula::checked_decimal_exponent(*negated) == exponent);
    }
}
```

- [ ] **Step 6: Verify on clang-cl**

```
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS on both compilers.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/rounding.hpp test/rounding_tests.cpp
git commit -m "feat(numbers): add decimal-place, significant-digit and double-to-rational rounding"
```

---

### Task 7: Wire into the umbrella, document, and verify the whole matrix

**Files:**
- Modify: `include/formula-cpp/formula.hpp`
- Create: `examples/exact_numbers.cpp`
- Modify: `examples/CMakeLists.txt`
- Create: `docs/numbers.md`
- Modify: `mkdocs.yml`
- Modify: `README.md`

**Interfaces:**
- Consumes: everything from Tasks 1–6.
- Produces: no new API. This task makes the API reachable, explained and verified.

- [ ] **Step 1: Add the new headers to the umbrella**

`include/formula-cpp/formula.hpp` currently reads:

```cpp
#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/evaluation.hpp>
#include <formula-cpp/version.hpp>
```

Make it:

```cpp
#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluation.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/version.hpp>
```

- [ ] **Step 2: Write the example**

Create `examples/exact_numbers.cpp`. Generic physics only — no norm content, no real standard
numbers, per the Global Constraints:

```cpp
// SPDX-License-Identifier: Apache-2.0
//
// Exact numbers and specified rounding.
//
// The scenario is deliberately generic: a flow rate from a measured volume and a
// measured duration, reported to a precision the method fixes. It shows the two
// things binary floating point cannot do -- exact unit conversion that round
// trips, and rounding that is part of the calculation rather than of the output.

#include <formula-cpp/formula.hpp>

#include <iostream>

int main()
{
    using formula::DecimalPlaces;
    using formula::Rational;
    using formula::RoundingMode;

    // 450 millilitres, written exactly. from_decimal(45, 1) is 450, not a double.
    Rational const volumeInMillilitres = *Rational::from_decimal(45, 1);

    // Convert to litres by an exact integer factor: multiply, then divide.
    // 450 ml -> 9/20 l, and back to 450 ml with nothing lost.
    Rational const volumeInLitres = volumeInMillilitres / Rational { 1000 };
    Rational const roundTripped = volumeInLitres * Rational { 1000 };

    std::cout << "volume = " << volumeInMillilitres.numerator() << " ml"
              << " = " << volumeInLitres.numerator() << '/' << volumeInLitres.denominator() << " l\n";
    std::cout << "round trip exact: " << (roundTripped == volumeInMillilitres ? "yes" : "no") << '\n';

    // 0,4 of a minute, exactly.
    Rational const durationInMinutes { 2, 5 };
    Rational const flowRate = volumeInLitres / durationInMinutes; // 9/8 l/min, i.e. 1,125

    // The method says: report to two decimal places, rounding half away from zero.
    // That rounding is part of the method, so it happens here, not at print time.
    Rational const reported = formula::round(flowRate, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero);

    std::cout << "flow rate = " << flowRate.numerator() << '/' << flowRate.denominator() << " l/min\n";
    std::cout << "reported  = " << reported.to_double() << " l/min\n";

    // Rounding up is a separate instruction from rounding to nearest, and the
    // two disagree on exactly the values where it matters.
    Rational const roundedUp = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::Ceiling);
    Rational const roundedNearest = formula::round(flowRate, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero);

    std::cout << "one decimal, ceiling = " << roundedUp.to_double() << '\n';
    std::cout << "one decimal, nearest = " << roundedNearest.to_double() << '\n';
    std::cout << "they differ: " << (roundedUp != roundedNearest ? "yes" : "no") << '\n';

    // Ten tenths are exactly one. In double arithmetic they are not.
    Rational sum {};
    for (int step = 0; step < 10; ++step)
        sum += *Rational::from_decimal(1, -1);

    std::cout << "ten tenths == one: " << (sum == Rational { 1 } ? "yes" : "no") << '\n';
    return 0;
}
```

Expected output, which the implementer must confirm by running it:

```
volume = 450 ml = 9/20 l
round trip exact: yes
flow rate = 9/8 l/min
reported  = 1.13 l/min
one decimal, ceiling = 1.2
one decimal, nearest = 1.1
they differ: yes
ten tenths == one: yes
```

Why these numbers: `(9/20) / (2/5) = 9/8 = 1,125`. At two decimals that is an exact tie, so
`HalfAwayFromZero` gives `1,13`. At one decimal it is not a tie — `1,125` sits nearer `1,1`
than `1,2` — so `Ceiling` and `HalfAwayFromZero` genuinely disagree, which is the point of
those four lines. A duration of `1/3` would give `27/20 = 1,35`, where the two modes happen to
agree and the example would demonstrate nothing. Do not change the duration without re-checking
that `they differ: yes` still holds.

- [ ] **Step 3: Register the example**

Append to `examples/CMakeLists.txt`:

```cmake
add_executable(formula-cpp-example-exact-numbers exact_numbers.cpp)
target_link_libraries(formula-cpp-example-exact-numbers PRIVATE formula-cpp::formula-cpp)
formula_apply_warnings(formula-cpp-example-exact-numbers)

add_test(NAME example.exact_numbers COMMAND formula-cpp-example-exact-numbers)
set_tests_properties(example.exact_numbers PROPERTIES PASS_REGULAR_EXPRESSION "ten tenths == one: yes")
```

- [ ] **Step 4: Run the example and confirm its output**

```
cmake --build --preset cl-debug
ctest --preset cl-debug -R example --output-on-failure
```

Expected: both examples pass. Run `formula-cpp-example-exact-numbers` directly and paste its
real output into the example's comment block if it differs from the block above.

- [ ] **Step 5: Write the documentation page**

Create `docs/numbers.md`. It must be readable by someone who has never seen the library, and
every code block in it must be code that actually compiles against the headers as written.

Required content, in this order:
1. **Why not `double`** — one short section. The 0,1 + 0,1 + ... example, and the
   450 ml / 0,45 l round trip. State plainly that rounding rules in standards are specified
   behaviour, so the number type has to be able to represent them exactly.
2. **Constructing a `Rational`** — the table below, spelled out with runnable lines.

   | You want | Write | You get |
   |---|---|---|
   | an integer | `Rational { 7 }` | 7/1 |
   | a fraction | `Rational { 3, 4 }` | 3/4 |
   | an exact decimal | `Rational::from_decimal(45, -2)` | 9/20 |
   | a whole number of tens | `Rational::from_decimal(45, 1)` | 450/1 |
   | the exact value of a `double` | `Rational::from_double_exact(0.45)` | a power-of-two denominator |
   | a measured `double` on a known scale | `rational_from_double(0.45, DecimalPlaces { 2 }, mode)` | 9/20 |

   Say explicitly that `Rational r = 0.45;` does not compile, and why.
3. **Exact or nothing** — the two-layer error model. `checked_add` returns
   `std::expected`; `operator+` throws. A failure inside a `constexpr` context is a compile
   error. Nothing saturates.
4. **Rounding** — the seven modes as a table with a worked column for `1.75`, `-1.75`, `1.5`
   and `2.5`, taken from the test. Then decimal places, significant digits and
   `round_to_multiple`, each with one line of code.
5. **Rounding is part of the calculation** — the intermediate-versus-final example from
   `rounding_tests.cpp`, showing two orders producing two different answers, both correct for
   their respective methods.
6. **Limits** — numerator and denominator are `std::int64_t`; `DecimalPlaces` is limited to
   ±18; `rational_from_double` covers roughly |value| in [2^-62, 2^62]. Overflow is always
   reported, never absorbed.

Do not cite any real standard in this page. Generic physics only.

- [ ] **Step 6: Add the page to the site navigation**

In `mkdocs.yml`, change

```yaml
nav:
  - Home: index.md
```

to

```yaml
nav:
  - Home: index.md
  - Numbers: numbers.md
```

- [ ] **Step 7: Update the README**

The README's feature list currently describes only the phase-1 scaffolding. Add one line under
the existing content stating that exact rational arithmetic and norm-style rounding are
available, and link to `docs/numbers.md`. Do not claim any capability that is not in the
headers as of this branch — no dimensions, no units, no expression trees, no traceability.
State what is verified, not what is planned.

- [ ] **Step 8: Run everything, on both local compilers, in both configurations**

```
cmake --build --preset cl-debug      && ctest --preset cl-debug --output-on-failure
cmake --build --preset cl-release    && ctest --preset cl-release --output-on-failure
cmake --build --preset clangcl-debug && ctest --preset clangcl-debug --output-on-failure
cmake --build --preset clangcl-release && ctest --preset clangcl-release --output-on-failure
```

Expected: every test passes in all four. Record the pass counts — they must be identical
across the four, and strictly greater than the phase-1 counts.

`clang++` and `gcc` are verified by CI, not locally; that split was established in phase 1 and
still holds.

- [ ] **Step 9: Confirm the install-and-consume package still works**

The new headers must actually be installed, not merely present in the source tree.

```
cmake --build --preset cl-release --target install
```

then run the packaging guard the same way `.github/workflows/package.yml` does. If the
`FILE_SET HEADERS` declaration in the top-level `CMakeLists.txt` lists headers individually
rather than by directory, **add the four new headers to it** — a header that compiles locally
but is missing from the install is the single most common packaging defect, and phase 1 built
this guard specifically to catch it.

- [ ] **Step 10: Measure what clang-tidy would say, without acting on it**

The lint CI job is deliberately not yet in place (phase 1 Ruling 16), and `NOLINT` is banned by
`ctest -R hygiene.nolint`. This step gathers real data for the job when it is written, instead
of guessing: run clang-tidy over the four new headers and **record the output in the ledger**.

```
clang-tidy --quiet -p out/build/clangcl-debug include/formula-cpp/rational.hpp include/formula-cpp/rounding.hpp include/formula-cpp/error.hpp include/formula-cpp/detail/checked_int.hpp -- -std=c++23 -Iinclude
```

Do **not** add suppressions, do **not** edit `.clang-tidy`, and do **not** restructure code to
please a check that does not run. Paste the finding counts by check name into the report. Some
checks (notably `bugprone-easily-swappable-parameters`) are expected to fire on a numerator /
denominator pair and are not defects.

- [ ] **Step 11: Commit**

```bash
git add include/formula-cpp/formula.hpp examples/exact_numbers.cpp examples/CMakeLists.txt docs/numbers.md mkdocs.yml README.md
git commit -m "docs(numbers): document and demonstrate exact numbers and rounding"
```

---

## Self-Review

**1. Spec coverage.**

| Spec requirement | Task |
|---|---|
| §6 "exact rational with an explicit decimal-place tag and rounding mode", free of serialization and logging dependencies | 3, 4 |
| §6 "`double` remains available as an opt-in representation" | 3 (`from_double_exact`), 6 (`rational_from_double`). The **representation-agnostic expression layer** is spec phase 5, not this plan. |
| §6 "unit conversion applies exact integer factors by multiply-then-divide" | 4 (the arithmetic that makes it exact), 7 (the example that demonstrates it). Unit *descriptors* are spec phase 3. |
| §16.3 Tier A #2 round-to-decimals | 6 |
| §16.3 Tier A #2 round-to-integer | 5 |
| §16.3 Tier A #2 round-up specifically | 5 (`Ceiling` and `AwayFromZero`, distinguished and separately tested) |
| §16.3 Tier A #2 significant figures | 6 |
| §16.3 Tier A #2 "intermediate and final rounding differ and both are specified" | 6 (the test that proves the two orders differ), 7 (documented) |
| §17 phase 2 "rounding modes incl. round-up and significant figures" | 5, 6 |

Deliberately **not** in this plan, and why: per-element rounding within a vector (§16.3 Tier A
#2, last sentence) needs the series type, which is spec phase 12. Rounding as a **tree node** is
spec phase 8. Both consume this layer unchanged.

**2. Placeholder scan.** No "TBD", no "add error handling", no "similar to Task N". Every code
step carries the code. The one place the plan asks the implementer to choose a value rather
than transcribe one — the example's duration in Task 7 Step 2 — states the constraint, gives a
worked alternative, and names the property the result must have.

**3. Type consistency.** Checked across tasks:
- `detail::Int` is defined once (Task 1) and re-exported as `Rational::Int` (Task 3); no second spelling.
- `checked_add` exists in **both** `formula::detail` (on `Int`, returning `std::optional`, Task 1)
  and `formula` (on `Rational`, returning `std::expected`, Task 4). This is deliberate — they are
  different operations at different layers — but it is the one name in this plan that could be
  misread. Every call site in the plan qualifies it.
- `describe` is overloaded for `ArithmeticError` (Task 2) and `RoundingMode` (Task 5). Both
  return `std::string_view`, both are found by ADL on `formula`.
- `round` is overloaded on `DecimalPlaces` and `SignificantDigits` (Task 6); the two are distinct
  struct types, not aliases of `int`, precisely so this overload is unambiguous.
- `negate` and `reciprocal` are declared before their first use in the header ordering given.

**4. Ordering hazards.** `rounding.hpp` includes `rational.hpp`, which includes
`detail/checked_int.hpp` and `error.hpp`. There is no cycle. `formula.hpp` includes all of them
alphabetically, which happens to also be a valid topological order; it does not rely on that,
since every header includes what it needs.
