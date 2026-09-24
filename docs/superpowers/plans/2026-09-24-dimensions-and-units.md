# Dimensions and Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `formula::Dimension` — a structural exponent vector over the seven SI base dimensions with **rational** exponents, usable as a non-type template parameter — and `formula::Unit`, carrying a dimension, an exact conversion magnitude, an optional affine offset, a display symbol, a default decimal precision and optional validity bounds.

**Architecture:** Two new headers. `dimension.hpp` defines a structural `Exponent` and the `Dimension` vector with its algebra, plus the named `dim::` constants and the mismatch-diagnostic helper. `unit.hpp` defines the `Unit` descriptor, the named `unit::` constants, and exact conversion by multiply-then-divide. Both types are **structural**, so both can be template arguments — which spec §8's quantity declaration requires of `Unit` in phase 4.

**Tech Stack:** C++23, header-only, no third-party dependencies. Catch2 v3 for runtime tests, `static_assert` for compile-time tests, the negative-compile harness for diagnostics.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md` — implements **§7 (Dimensions)** and the unit half of **§8**, i.e. spec phase 3 of §17: "Dimensions (rational exponents), units, exact conversion, decimals + bounds".

## Global Constraints

Copied from the spec. Every task's requirements implicitly include this section.

- **Open source, no company IP.** Apache-2.0; `// SPDX-License-Identifier: Apache-2.0` (or `#` for CMake) on the first line of every new source and build file.
- **No norm content.** No DIN/EN/ISO citations, no transcribed standard text, no real threshold values. Examples use generic physics only.
- **No third-party dependencies in the core.**
- **Compilers:** MSVC `cl`, `clang-cl`, `clang++`, and GCC. Local gate is `cl` + `clang-cl`; `clang++`/`gcc` are verified by CI.
- **C++23 everywhere.**
- Public headers must not include `<string>`, `<vector>`, `<format>` or `<iostream>`. `<string_view>` is permitted.
- The literal text `NOLINT` must not appear in tracked source.
- **Naming:** types `CamelCase`, functions `lower_case`, global constants `CamelCase`, private members `_leadingUnderscore`.
- **Formatting:** `.clang-format` at the repo root — column limit 125, Allman braces, `Cpp11BracedListStyle: false`.
- **Catch2 `TEST_CASE` names must contain balanced square brackets.** Enforced by `ctest -R hygiene.testnames`; an unbalanced `[` or `]` silently drops that test and every test after it.
- **Every new header is registered in `FILE_SET HEADERS`** in the top-level `CMakeLists.txt` as it is created — never deferred.

## Design Rulings — all measured by a spike before this plan was written

The spike lives in the scratchpad (`dim_probe.hpp`, `unit_probe.hpp`, and four translation units) and was compiled and **linked** on `cl`, `clang-cl` and `clang++`. Do not re-litigate these; they are facts, not preferences.

**Ruling A — `Exponent` is a new structural type, NOT `formula::Rational`.**
A class type used as a non-type template parameter must be *structural*: every non-static data member public, recursively. `formula::Rational` keeps `_numerator`/`_denominator` private, so it cannot appear in an NTTP. Measured, clang says exactly that:

```
error: type 'formula::Rational' of non-type template parameter is not a structural type
note: 'formula::Rational' is not a structural type because it has a
      non-static data member that is not public
```

Making `Rational`'s members public would fix that and break something worse: phase 2's reviewers confirmed that `Rational::operator==` compares componentwise **only because** no code path can build a non-canonical value. Public members destroy that guarantee. So dimensions get their own small structural exponent type, and `Rational` stays encapsulated.

**Ruling B — `Dimension` with rational exponents works as an NTTP on all three compilers, including across translation units.**
Phase 1 verified this for *integer* exponents. Rational exponents nest a structural type inside the NTTP, which is a new risk, so it was re-measured. Verified: a computed `dim::area * dim::length` and a literal `dim::volume` are the **same type**, a function defined in one TU taking `Tagged<nth_root(dim::area, 2)>` links against a call spelled `Tagged<dim::length>` in another, and `sqrt` and `p^(2/3)` both come out exactly (`1/2`, `2/3`, `-2/3`).

**Ruling C — exponents must be canonical, or one dimension splits into several types.**
`exponent(2, 4)` and `exponent(1, 2)` must produce identical objects, otherwise `Tagged<...>` instantiates twice for the same physical dimension and the two do not interoperate. Verified by `static_assert` on type identity. Canonicalisation therefore happens in the factory, not on demand.

**Ruling D — `Unit` carries its symbol as a fixed-capacity `char` array, not a `std::string_view`.**
Spec §8 passes `formula::unit::litre` *by value* as a template argument, so `Unit` must be structural too. `std::string_view` has private members and is not. A fixed array is structural and — unlike a `FixedString<N>` template — keeps `Unit` a **single non-template type**, so every unit has the same type and they can be compared, stored and passed uniformly. Capacity 16 bytes, enough for UTF-8 symbols like `m³` (4 bytes) and `°C` (3 bytes). A `std::string_view` accessor reads it back at the point of use; only the *storage* has to be structural.
Measured: `Unit` as an NTTP works on all three compilers and links across TUs, and two units differing only in symbol stay distinct types — which matters, because otherwise a relabelled unit would silently collapse into another.

**Ruling E — conversion is stored as separate numerator/denominator integers, not a `Rational`.**
Same structural constraint as Ruling A. `Unit` holds `magnitudeNumerator`/`magnitudeDenominator` and `offsetNumerator`/`offsetDenominator` as `std::int64_t`. Conversion *computes* through `formula::Rational` (phase 2) so it inherits exact arithmetic and overflow reporting; only the stored descriptor is raw pairs.
Measured exactly, both directions: `450 l` → `450/1000 m³` → `450 l`, and `100 °C` → `37315/100 K` → `100 °C`.

**Ruling F — dimension mismatch is a `static_assert` inside a named helper template.**
Spec §13 measured this: asserting the condition inline prints only type *names*, while a named helper prints the exponent **values**, so the reader sees `volume vs mass` rather than two opaque template-ids. The helper's message is also our own text, which is what makes the negative-compile harness able to assert the *reason*.

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/dimension.hpp` | **Create.** `Exponent` + factory and arithmetic; `Dimension`; `*`, `/`, `power`, `nth_root`; `dim::` constants; `RequireSameDimension`. |
| `include/formula-cpp/unit.hpp` | **Create.** `Symbol`, `Bounds`, `Unit`; `unit::` constants; `convert`, `checked_convert`; bounds and decimals accessors. |
| `include/formula-cpp/formula.hpp` | **Modify.** Add both headers to the umbrella. |
| `CMakeLists.txt` | **Modify.** Register both headers in `FILE_SET HEADERS`. |
| `test/dimension_tests.cpp` | **Create.** Exponent canonicalisation, dimension algebra, rational exponents, NTTP identity. |
| `test/dimension_cross_tu.hpp`, `test/dimension_cross_tu_b.cpp` | **Create.** The cross-TU linkage test — a function defined in one TU, called with an equal-but-differently-spelled dimension in another. |
| `test/unit_tests.cpp` | **Create.** Unit identity, exact conversion both directions, affine offsets, bounds, decimals. |
| `test/negative/dimension_mismatch.cpp` | **Create.** Must-not-compile: adding a volume to a mass. |
| `test/negative/unit_dimension_mismatch.cpp` | **Create.** Must-not-compile: converting between units of different dimension. |
| `test/CMakeLists.txt` | **Modify.** Register the new sources and the two negative cases. |
| `examples/dimensions_and_units.cpp` | **Create.** Generic-physics example. |
| `examples/CMakeLists.txt` | **Modify.** Register it. |
| `docs/dimensions.md` | **Create.** Human-readable page. |
| `mkdocs.yml` | **Modify.** Add to nav. |

## Task Overview

| Task | Deliverable |
|---|---|
| 1 | `Exponent` — structural rational exponent, canonical by construction |
| 2 | `Dimension` — the vector, its algebra, `dim::` constants, NTTP and cross-TU proof |
| 3 | `RequireSameDimension` and the mismatch diagnostic, with negative-compile tests |
| 4 | `Unit` — the descriptor, `Symbol`, `Bounds`, `unit::` constants |
| 5 | Exact conversion, including affine offsets, through `Rational` |
| 6 | Bounds checking and declared decimals |
| 7 | Umbrella, example, documentation, packaging verification |

---

### Task 1: `Exponent` — a structural rational exponent

**Files:**
- Create: `include/formula-cpp/dimension.hpp`
- Create: `test/dimension_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `dimension_tests.cpp` to `add_executable(formula-cpp-tests ...)`)
- Modify: `CMakeLists.txt` (register `dimension.hpp` in `FILE_SET HEADERS`)

**Interfaces produced**, all in `namespace formula`:
- `struct Exponent { std::int32_t numerator = 0; std::int32_t denominator = 1; ... };`
- `constexpr Exponent exponent(std::int32_t numerator, std::int32_t denominator = 1) noexcept`
- `constexpr Exponent operator+(Exponent, Exponent) noexcept`, `operator-` (binary and unary)
- `constexpr Exponent operator*(Exponent, std::int32_t) noexcept`, `operator/(Exponent, std::int32_t) noexcept`
- `constexpr bool is_zero(Exponent) noexcept`, `constexpr bool is_integer(Exponent) noexcept`

**Why this type exists rather than reusing `Rational`:** see Ruling A. Do not "simplify" it by
substituting `Rational` — that is a measured impossibility, not a preference.

- [ ] **Step 1: Write the failing test**

Create `test/dimension_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/dimension.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using formula::Exponent;
using formula::exponent;

// ---- canonical by construction ----
//
// This is load-bearing, not tidiness: an Exponent is part of a Dimension, and a
// Dimension is a non-type template parameter. Two equal-but-uncanonical
// exponents would make one physical dimension into two distinct,
// non-interoperating types.

static_assert(exponent(2, 4) == exponent(1, 2));
static_assert(exponent(-1, -2) == exponent(1, 2));
static_assert(exponent(1, -2) == exponent(-1, 2));
static_assert(exponent(0, 5) == exponent(0, 1));
static_assert(exponent(6, 3) == exponent(2, 1));
static_assert(exponent(5).denominator == 1);

static_assert(exponent(2, 4).numerator == 1);
static_assert(exponent(2, 4).denominator == 2);
static_assert(exponent(-6, 4).numerator == -3);
static_assert(exponent(-6, 4).denominator == 2);

// The denominator is always positive, so the sign lives in one place only.
static_assert(exponent(1, -2).denominator > 0);
static_assert(exponent(-1, -2).denominator > 0);

// ---- arithmetic ----

static_assert(exponent(1, 2) + exponent(1, 2) == exponent(1));
static_assert(exponent(1, 3) + exponent(1, 6) == exponent(1, 2));
static_assert(exponent(1) - exponent(1, 3) == exponent(2, 3));
static_assert(-exponent(2, 3) == exponent(-2, 3));
static_assert(exponent(1, 2) * 4 == exponent(2));
static_assert(exponent(2) / 4 == exponent(1, 2));
static_assert(exponent(1, 3) * 3 == exponent(1));

// The case integer exponents cannot express at all.
static_assert(exponent(1) / 2 == exponent(1, 2));
static_assert(exponent(2) / 3 == exponent(2, 3));
static_assert((exponent(1) / 3) * 2 == exponent(2, 3));

static_assert(formula::is_zero(exponent(0)));
static_assert(!formula::is_zero(exponent(1, 2)));
static_assert(formula::is_integer(exponent(3)));
static_assert(!formula::is_integer(exponent(1, 2)));
static_assert(formula::is_integer(exponent(6, 3)));

TEST_CASE("exponents canonicalise on construction", "[dimension]")
{
    CHECK(exponent(2, 4) == exponent(1, 2));
    CHECK(exponent(10, 5) == exponent(2));
    CHECK(exponent(0, 7).denominator == 1);

    // Equal values must be bit-identical, because that is what makes two
    // spellings of one dimension the same template argument.
    Exponent const a = exponent(3, 9);
    Exponent const b = exponent(1, 3);
    CHECK(a.numerator == b.numerator);
    CHECK(a.denominator == b.denominator);
}

TEST_CASE("exponent arithmetic stays canonical", "[dimension]")
{
    for (std::int32_t numerator = -6; numerator <= 6; ++numerator)
    {
        for (std::int32_t denominator = 1; denominator <= 6; ++denominator)
        {
            Exponent const e = exponent(numerator, denominator);
            INFO(e.numerator << "/" << e.denominator);
            CHECK(e.denominator > 0);

            Exponent const doubled = e + e;
            CHECK(doubled.denominator > 0);
            CHECK(doubled == e * 2);

            CHECK(e - e == exponent(0));
            CHECK((e * 3) / 3 == e);
            CHECK(-(-e) == e);
        }
    }
}
```

- [ ] **Step 2: Add the test to the build and run it to verify it fails**

Add `dimension_tests.cpp` to `add_executable(formula-cpp-tests ...)` in `test/CMakeLists.txt`, then:

```
cmake --build --preset cl-debug
```

Expected: **compile error** — `formula-cpp/dimension.hpp` not found. That is the RED signal;
there is no failing-assertion stage because nothing compiles before the header exists.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/dimension.hpp`. Task 2 appends to the same file.

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Dimensional analysis: a structural exponent vector over the seven SI base
/// dimensions, usable as a non-type template parameter so that a dimension is
/// part of a type rather than a runtime tag.

#include <cstdint>

namespace formula
{

/// A rational exponent on one base dimension.
///
/// Deliberately a separate type from `Rational`, and deliberately all-public. A
/// class used as a non-type template parameter must be *structural*: every
/// non-static data member public, recursively. `Rational` keeps its members
/// private -- which is exactly what lets its `operator==` compare componentwise,
/// since no code path can build a non-canonical value -- so it cannot be used
/// here. This type pays for its public members by canonicalising in its factory.
///
/// Always in lowest terms with a positive denominator. Build one with
/// `exponent(...)`. Aggregate initialisation bypasses that and is a mistake:
/// `Exponent { 2, 4 }` and `Exponent { 1, 2 }` are the same number but
/// different objects, and as template arguments they name different types.
struct Exponent
{
    std::int32_t numerator = 0;
    std::int32_t denominator = 1;

    [[nodiscard]] constexpr bool operator==(Exponent const&) const noexcept = default;
};

namespace detail
{
    [[nodiscard]] constexpr std::int64_t exponent_gcd(std::int64_t lhs, std::int64_t rhs) noexcept
    {
        if (lhs < 0)
            lhs = -lhs;
        if (rhs < 0)
            rhs = -rhs;
        while (rhs != 0)
        {
            std::int64_t const remainder = lhs % rhs;
            lhs = rhs;
            rhs = remainder;
        }
        return lhs == 0 ? 1 : lhs;
    }

    /// Reduces and narrows back to the stored width. Intermediates are 64-bit so
    /// that ordinary combinations cannot overflow on the way; the reduced result
    /// always fits, because dimension exponents are physically small. A value
    /// that does not fit is a programming error, and in a constant expression --
    /// which is where dimensions are built -- the narrowing is diagnosed rather
    /// than silently wrapping.
    [[nodiscard]] constexpr Exponent reduced(std::int64_t numerator, std::int64_t denominator) noexcept
    {
        if (denominator < 0)
        {
            numerator = -numerator;
            denominator = -denominator;
        }
        std::int64_t const common = exponent_gcd(numerator, denominator);
        return { static_cast<std::int32_t>(numerator / common), static_cast<std::int32_t>(denominator / common) };
    }
} // namespace detail

/// The only sanctioned way to make an `Exponent`: canonicalises.
[[nodiscard]] constexpr Exponent exponent(std::int32_t numerator, std::int32_t denominator = 1) noexcept
{
    return detail::reduced(numerator, denominator);
}

[[nodiscard]] constexpr Exponent operator+(Exponent lhs, Exponent rhs) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(lhs.numerator) * rhs.denominator
                               + static_cast<std::int64_t>(rhs.numerator) * lhs.denominator,
                           static_cast<std::int64_t>(lhs.denominator) * rhs.denominator);
}

[[nodiscard]] constexpr Exponent operator-(Exponent lhs, Exponent rhs) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(lhs.numerator) * rhs.denominator
                               - static_cast<std::int64_t>(rhs.numerator) * lhs.denominator,
                           static_cast<std::int64_t>(lhs.denominator) * rhs.denominator);
}

[[nodiscard]] constexpr Exponent operator-(Exponent value) noexcept
{
    return detail::reduced(-static_cast<std::int64_t>(value.numerator), value.denominator);
}

/// Raising a dimension to an integer power scales its exponents.
[[nodiscard]] constexpr Exponent operator*(Exponent value, std::int32_t factor) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(value.numerator) * factor, value.denominator);
}

/// Taking an nth root divides them -- the operation integer exponents cannot express.
[[nodiscard]] constexpr Exponent operator/(Exponent value, std::int32_t divisor) noexcept
{
    return detail::reduced(value.numerator, static_cast<std::int64_t>(value.denominator) * divisor);
}

[[nodiscard]] constexpr bool is_zero(Exponent value) noexcept
{
    return value.numerator == 0;
}

[[nodiscard]] constexpr bool is_integer(Exponent value) noexcept
{
    return value.denominator == 1;
}

} // namespace formula
```

Register the header in the top-level `CMakeLists.txt`:

```cmake
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/dimension.hpp"
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS on `cl-debug`, then repeat on `clangcl-debug`.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/dimension.hpp test/dimension_tests.cpp test/CMakeLists.txt CMakeLists.txt
git commit -m "feat(dimensions): add a structural rational exponent"
```

---

### Task 2: `Dimension` — the vector, its algebra, and the NTTP proof

**Files:**
- Modify: `include/formula-cpp/dimension.hpp` (append after `Exponent`)
- Modify: `test/dimension_tests.cpp` (append)
- Create: `test/dimension_cross_tu.hpp`
- Create: `test/dimension_cross_tu_b.cpp`
- Modify: `test/CMakeLists.txt` (add `dimension_cross_tu_b.cpp` to the test executable)

**Interfaces produced**, in `namespace formula`:
- `struct Dimension { Exponent length, mass, time, current, temperature, amount, luminosity; ... };`
- `constexpr Dimension operator*(Dimension, Dimension) noexcept`, `operator/`
- `constexpr Dimension power(Dimension, std::int32_t) noexcept`
- `constexpr Dimension nth_root(Dimension, std::int32_t) noexcept`
- `constexpr bool is_dimensionless(Dimension) noexcept`
- In `namespace formula::dim`: `Scalar`, `Length`, `Mass`, `Time`, `Current`, `Temperature`, `Amount`, `Luminosity`, `Area`, `Volume`, `Density`, `Velocity`, `Acceleration`, `Force`, `Pressure`, `Energy`, `Frequency`

**Naming note:** the spec writes these as `dim::volume`. Project convention is `CamelCase` for
global constants, so they are `dim::Volume`. Keep the namespace `dim` lowercase.

**Measured and settled (Ruling B):** this works as an NTTP on all three compilers and links across
translation units. The cross-TU test below is what keeps it that way.

- [ ] **Step 1: Write the failing tests**

Append to `test/dimension_tests.cpp`:

```cpp
// ---- the dimension vector ----

using formula::Dimension;
using formula::nth_root;
using formula::power;

namespace dim = formula::dim;

static_assert(dim::Volume == Dimension { .length = exponent(3) });
static_assert(dim::Area == dim::Length * dim::Length);
static_assert(dim::Volume == dim::Area * dim::Length);
static_assert(dim::Area / dim::Length == dim::Length);
static_assert(dim::Volume / dim::Volume == dim::Scalar);
static_assert(dim::Density == dim::Mass / dim::Volume);
static_assert(formula::is_dimensionless(dim::Scalar));
static_assert(!formula::is_dimensionless(dim::Length));

static_assert(power(dim::Length, 3) == dim::Volume);
static_assert(power(dim::Length, 0) == dim::Scalar);
static_assert(power(dim::Length, -1) == dim::Scalar / dim::Length);

// Rational exponents: what this whole design is for.
static_assert(nth_root(dim::Area, 2) == dim::Length);
static_assert(nth_root(dim::Volume, 3) == dim::Length);

constexpr Dimension HalfLength = nth_root(dim::Length, 2);
static_assert(HalfLength.length == exponent(1, 2));
static_assert(HalfLength != dim::Length);
static_assert(HalfLength * HalfLength == dim::Length);

// Pressure to the two-thirds, the spec's stated driver for rational exponents.
constexpr Dimension PressureTwoThirds = power(nth_root(dim::Pressure, 3), 2);
static_assert(PressureTwoThirds.mass == exponent(2, 3));
static_assert(PressureTwoThirds.length == exponent(-2, 3));
static_assert(PressureTwoThirds.time == exponent(-4, 3));
static_assert(power(PressureTwoThirds, 3) == power(dim::Pressure, 2));

// ---- the property that makes a Dimension usable as a template argument ----

template <Dimension D>
struct Tagged
{
    int value {};
};

static_assert(std::is_same_v<Tagged<dim::Area * dim::Length>, Tagged<dim::Volume>>,
              "a computed dimension and a literal one must be the SAME type");
static_assert(std::is_same_v<Tagged<dim::Volume / dim::Volume>, Tagged<dim::Scalar>>,
              "a ratio of like dimensions must be the scalar type");
static_assert(!std::is_same_v<Tagged<dim::Length>, Tagged<dim::Mass>>,
              "different dimensions must be different types");
static_assert(std::is_same_v<Tagged<Dimension { .length = exponent(2, 4) }>,
                             Tagged<Dimension { .length = exponent(1, 2) }>>,
              "uncanonical exponents would split one dimension into two types");

TEST_CASE("dimension algebra composes the way physics does", "[dimension]")
{
    CHECK(dim::Velocity == dim::Length / dim::Time);
    CHECK(dim::Acceleration == dim::Velocity / dim::Time);
    CHECK(dim::Force == dim::Mass * dim::Acceleration);
    CHECK(dim::Pressure == dim::Force / dim::Area);
    CHECK(dim::Energy == dim::Force * dim::Length);
    CHECK(dim::Frequency == dim::Scalar / dim::Time);
}

TEST_CASE("multiplying by a dimension and dividing by it again is an identity", "[dimension]")
{
    Dimension const all[] = { dim::Scalar,  dim::Length, dim::Mass,   dim::Time,     dim::Area,
                              dim::Volume,  dim::Density, dim::Force, dim::Pressure, dim::Energy };
    for (Dimension const& a: all)
    {
        for (Dimension const& b: all)
        {
            CHECK((a * b) / b == a);
            CHECK(power(nth_root(a, 2), 2) == a);
        }
    }
}
```

Create `test/dimension_cross_tu.hpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Cross-translation-unit identity for a Dimension used as a non-type template
/// parameter. Phase 1 established that the equivalent trick with
/// `decltype([]{})` gives each TU its OWN type and fails at link time with a
/// message that never names the cause. This test exists so that a future change
/// to Dimension cannot reintroduce that failure silently: the functions below
/// are DEFINED in dimension_cross_tu_b.cpp and CALLED from dimension_tests.cpp
/// with an equal but differently spelled dimension. If the two spellings are not
/// the same type, this does not link.

#include <formula-cpp/dimension.hpp>

namespace formula_test
{

template <formula::Dimension D>
struct Tagged
{
    int value {};
};

int consume_volume(Tagged<formula::dim::Volume> tagged);
int consume_root_of_area(Tagged<formula::nth_root(formula::dim::Area, 2)> tagged);

} // namespace formula_test
```

Create `test/dimension_cross_tu_b.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include "dimension_cross_tu.hpp"

namespace formula_test
{

int consume_volume(Tagged<formula::dim::Volume> tagged)
{
    return tagged.value + 1;
}

// Spelled as a COMPUTED dimension here, called as a literal one in the test.
int consume_root_of_area(Tagged<formula::nth_root(formula::dim::Area, 2)> tagged)
{
    return tagged.value + 2;
}

} // namespace formula_test
```

Append to `test/dimension_tests.cpp`:

```cpp
#include "dimension_cross_tu.hpp"

TEST_CASE("a dimension template argument has the same identity in every translation unit",
          "[dimension]")
{
    // Defined in dimension_cross_tu_b.cpp. Called here with a dimension spelled
    // differently but equal -- so this linking at all is the assertion.
    CHECK(formula_test::consume_volume(formula_test::Tagged<dim::Area * dim::Length> { 10 }) == 11);
    CHECK(formula_test::consume_root_of_area(formula_test::Tagged<dim::Length> { 20 }) == 22);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build --preset cl-debug
```

Expected: **compile error** — no `formula::Dimension`, no `formula::dim::Volume`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/dimension.hpp`, inside `namespace formula`:

```cpp
/// An exponent vector over the seven SI base dimensions.
///
/// Structural, so it can be a non-type template parameter -- which is the point:
/// a dimension belongs to a *type*, checked when the program is compiled, not to
/// a value checked when it runs. Verified on MSVC, clang-cl and clang++,
/// including that two translation units agree on the mangling.
struct Dimension
{
    Exponent length {};
    Exponent mass {};
    Exponent time {};
    Exponent current {};
    Exponent temperature {};
    Exponent amount {};
    Exponent luminosity {};

    [[nodiscard]] constexpr bool operator==(Dimension const&) const noexcept = default;
};

/// Multiplying quantities adds their dimensions' exponents.
[[nodiscard]] constexpr Dimension operator*(Dimension lhs, Dimension rhs) noexcept
{
    return { lhs.length + rhs.length,
             lhs.mass + rhs.mass,
             lhs.time + rhs.time,
             lhs.current + rhs.current,
             lhs.temperature + rhs.temperature,
             lhs.amount + rhs.amount,
             lhs.luminosity + rhs.luminosity };
}

/// Dividing subtracts them.
[[nodiscard]] constexpr Dimension operator/(Dimension lhs, Dimension rhs) noexcept
{
    return { lhs.length - rhs.length,
             lhs.mass - rhs.mass,
             lhs.time - rhs.time,
             lhs.current - rhs.current,
             lhs.temperature - rhs.temperature,
             lhs.amount - rhs.amount,
             lhs.luminosity - rhs.luminosity };
}

[[nodiscard]] constexpr Dimension power(Dimension value, std::int32_t exponentOfPower) noexcept
{
    return { value.length * exponentOfPower,      value.mass * exponentOfPower,
             value.time * exponentOfPower,        value.current * exponentOfPower,
             value.temperature * exponentOfPower, value.amount * exponentOfPower,
             value.luminosity * exponentOfPower };
}

/// The nth root. Integer exponents cannot express the result at all -- the
/// square root of an area is a length, but the square root of a length is
/// length to the one half, and norm formulas do take such roots.
[[nodiscard]] constexpr Dimension nth_root(Dimension value, std::int32_t degree) noexcept
{
    return { value.length / degree,      value.mass / degree,   value.time / degree, value.current / degree,
             value.temperature / degree, value.amount / degree, value.luminosity / degree };
}

[[nodiscard]] constexpr bool is_dimensionless(Dimension value) noexcept
{
    return value == Dimension {};
}

/// Named dimensions. Users compose these rather than spelling exponents, which
/// keeps the representation swappable.
namespace dim
{
    inline constexpr Dimension Scalar {};
    inline constexpr Dimension Length { .length = exponent(1) };
    inline constexpr Dimension Mass { .mass = exponent(1) };
    inline constexpr Dimension Time { .time = exponent(1) };
    inline constexpr Dimension Current { .current = exponent(1) };
    inline constexpr Dimension Temperature { .temperature = exponent(1) };
    inline constexpr Dimension Amount { .amount = exponent(1) };
    inline constexpr Dimension Luminosity { .luminosity = exponent(1) };

    inline constexpr Dimension Area = Length * Length;
    inline constexpr Dimension Volume = Area * Length;
    inline constexpr Dimension Density = Mass / Volume;
    inline constexpr Dimension Velocity = Length / Time;
    inline constexpr Dimension Acceleration = Velocity / Time;
    inline constexpr Dimension Force = Mass * Acceleration;
    inline constexpr Dimension Pressure = Force / Area;
    inline constexpr Dimension Energy = Force * Length;
    inline constexpr Dimension Frequency = Scalar / Time;
} // namespace dim
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS. **The cross-TU case passing means it linked**, which is the whole assertion.

- [ ] **Step 5: Prove the cross-TU test can actually fail**

A link-time test that would pass even when broken is worthless. Temporarily change
`dimension_cross_tu_b.cpp`'s `consume_root_of_area` parameter to
`Tagged<formula::dim::Mass>`, rebuild, and confirm you get an **unresolved external symbol** for
the call in `dimension_tests.cpp`. Restore it and confirm the build is green again. Report both
outcomes.

- [ ] **Step 6: Verify on clang-cl**

```
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS on both compilers.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/dimension.hpp test/dimension_tests.cpp test/dimension_cross_tu.hpp test/dimension_cross_tu_b.cpp test/CMakeLists.txt
git commit -m "feat(dimensions): add the SI dimension vector and its algebra"
```

---

### Task 3: The mismatch diagnostic

**Files:**
- Modify: `include/formula-cpp/dimension.hpp` (append)
- Modify: `test/dimension_tests.cpp` (append)
- Create: `test/negative/dimension_mismatch.cpp`
- Modify: `test/CMakeLists.txt` (register the negative case)

**Interfaces produced**, in `namespace formula`:
- `template <Dimension Left, Dimension Right> struct RequireSameDimension`
- `template <Dimension Left, Dimension Right> inline constexpr bool SameDimension`

**Why a named helper rather than an inline `static_assert` (Ruling F):** spec §13 measured the
difference. Asserting the condition at the use site prints only the operand *type names*; routing
it through a named template makes both compilers print the **exponent values**, so the reader sees
`{3,0,0,...}` against `{0,1,0,...}` — volume against mass — instead of two opaque template-ids.
The helper's message is also our own text, which is what lets the negative-compile harness assert
the *reason* a compile failed rather than merely that it did.

- [ ] **Step 1: Write the failing test**

Append to `test/dimension_tests.cpp`:

```cpp
// ---- the mismatch helper ----

static_assert(formula::SameDimension<dim::Volume, dim::Area * dim::Length>);
static_assert(formula::SameDimension<dim::Scalar, dim::Volume / dim::Volume>);
static_assert(!formula::SameDimension<dim::Volume, dim::Mass>);
static_assert(!formula::SameDimension<dim::Length, nth_root(dim::Length, 2)>);

// Instantiating the helper on matching dimensions must be fine.
static_assert(formula::RequireSameDimension<dim::Volume, dim::Area * dim::Length>::value);

TEST_CASE("the same-dimension predicate agrees with equality", "[dimension]")
{
    CHECK(formula::SameDimension<dim::Force, dim::Mass * dim::Acceleration>);
    CHECK(formula::SameDimension<dim::Energy, dim::Force * dim::Length>);
    CHECK_FALSE(formula::SameDimension<dim::Energy, dim::Force>);
}
```

Create `test/negative/dimension_mismatch.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Adding a volume to a mass is meaningless, and the library must say so at
// compile time, in its own words, naming the two dimensions. This must not
// compile.
#include <formula-cpp/dimension.hpp>

namespace dim = formula::dim;

using Checked = formula::RequireSameDimension<dim::Volume, dim::Mass>;

int main()
{
    return Checked::value ? 1 : 0;
}
```

Register it in `test/CMakeLists.txt` beside the existing negative cases:

```cmake
formula_add_negative_test(dimension_mismatch
    "formula: these two dimensions are not the same")
```

- [ ] **Step 2: Run it to verify the RED signal**

```
cmake --build --preset cl-debug
```

Expected: **compile error** — no `formula::RequireSameDimension`, no `formula::SameDimension`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/dimension.hpp`, inside `namespace formula`:

```cpp
/// True when two dimensions are identical.
template <Dimension Left, Dimension Right>
inline constexpr bool SameDimension = (Left == Right);

/// Fails to compile, loudly and legibly, when two dimensions differ.
///
/// The indirection through a named template is deliberate and was measured: an
/// inline `static_assert(Left == Right, ...)` at the point of use prints only
/// the operand type names, while instantiating a template *on the values* makes
/// every supported compiler print the exponent vectors themselves --
///
///     RequireSameDimension<Dimension{...length 3...}, Dimension{...mass 1...}>
///
/// -- so the reader sees volume against mass rather than two opaque template
/// ids. The wording below is ours, which is what allows the negative-compile
/// test harness to assert why a compile failed rather than only that it did.
template <Dimension Left, Dimension Right>
struct RequireSameDimension
{
    static_assert(Left == Right,
                  "formula: these two dimensions are not the same; the template arguments printed "
                  "above are the offending exponent vectors, in the order length, mass, time, "
                  "current, temperature, amount, luminosity");

    static constexpr bool value = true;
};
```

- [ ] **Step 4: Run the tests to verify they pass**

```
cmake --build --preset cl-debug
ctest --preset cl-debug --output-on-failure
```

Expected: PASS, including `negative.dimension_mismatch`.

- [ ] **Step 5: Prove the negative test asserts the REASON, not merely a failure**

Temporarily change the expected text in `test/CMakeLists.txt` to something that appears in no
diagnostic. Run `ctest -R negative.dimension_mismatch`; it must **FAIL**. Restore the correct
text; it must **PASS**. Report both outcomes — a negative test that passes for the wrong reason is
worse than none.

- [ ] **Step 6: Capture the diagnostic quality, which is the point of Ruling F**

Build the negative target directly and paste the compiler's message into your report, for both
`cl` and `clang-cl`:

```
cmake --build --preset cl-debug --target negative-dimension_mismatch
```

Confirm in your report that the exponent **values** appear, not just type names. If they do not on
some compiler, say so plainly — that is a finding about the design, not a failure of the task.

- [ ] **Step 7: Commit**

```bash
git add include/formula-cpp/dimension.hpp test/dimension_tests.cpp test/negative/dimension_mismatch.cpp test/CMakeLists.txt
git commit -m "feat(dimensions): add the mismatch diagnostic helper"
```

---

### Task 4: `Unit` — the descriptor

**Files:**
- Create: `include/formula-cpp/unit.hpp`
- Create: `test/unit_tests.cpp`
- Modify: `test/CMakeLists.txt` (add `unit_tests.cpp`)
- Modify: `CMakeLists.txt` (register `unit.hpp` in `FILE_SET HEADERS`)

**Interfaces produced**, in `namespace formula`:
- `inline constexpr std::size_t SymbolCapacity = 16;`
- `struct Symbol { char characters[SymbolCapacity]; ... };`
- `constexpr Symbol symbol(char const* text) noexcept`
- `constexpr std::string_view view(Symbol const&) noexcept`
- `struct Bounds { bool present; std::int64_t lowNumerator, lowDenominator, highNumerator, highDenominator; ... };`
- `constexpr Bounds bounds(std::int64_t lowNumerator, std::int64_t lowDenominator, std::int64_t highNumerator, std::int64_t highDenominator) noexcept`
- `struct Unit { Dimension dimension; std::int64_t magnitudeNumerator, magnitudeDenominator, offsetNumerator, offsetDenominator; Symbol symbolText; std::int32_t decimals; Bounds bounds; ... };`
- In `namespace formula::unit`: `Metre`, `Millimetre`, `Centimetre`, `Kilometre`, `SquareMetre`, `CubicMetre`, `Litre`, `Millilitre`, `Kilogram`, `Gram`, `Tonne`, `Second`, `Minute`, `Hour`, `Kelvin`, `Celsius`, `Pascal`, `Megapascal`, `Percent`, `One`

**Everything here is structural (Ruling D), and must stay so** — `Unit` is passed by value as a
template argument in spec §8's quantity declaration, which phase 4 implements. Do not introduce a
`std::string_view`, a `Rational`, or any private member into `Unit`, `Symbol` or `Bounds`.
Measured: the symbol participates in unit identity, so two units differing only in spelling remain
distinct types, which is what stops a relabelled unit from silently collapsing into another.

- [ ] **Step 1: Write the failing test**

Create `test/unit_tests.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/unit.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <type_traits>

using formula::Symbol;
using formula::Unit;

namespace dim = formula::dim;
namespace unit = formula::unit;

// ---- the symbol ----

static_assert(formula::view(formula::symbol("l")) == std::string_view { "l" });
static_assert(formula::view(formula::symbol("mm")) == std::string_view { "mm" });
static_assert(formula::view(formula::symbol("")).empty());
static_assert(formula::symbol("l") == formula::symbol("l"));
static_assert(!(formula::symbol("l") == formula::symbol("L")));

// Over-long input is truncated rather than overrunning; the terminator survives.
static_assert(formula::view(formula::symbol("0123456789abcdefghij")).size() < formula::SymbolCapacity);

// ---- units carry their dimension ----

static_assert(unit::Metre.dimension == dim::Length);
static_assert(unit::Litre.dimension == dim::Volume);
static_assert(unit::CubicMetre.dimension == dim::Volume);
static_assert(unit::Kilogram.dimension == dim::Mass);
static_assert(unit::Celsius.dimension == dim::Temperature);
static_assert(unit::Pascal.dimension == dim::Pressure);
static_assert(unit::One.dimension == dim::Scalar);
static_assert(unit::Percent.dimension == dim::Scalar);

// ---- the coherent SI units have magnitude 1 and no offset ----

static_assert(unit::Metre.magnitudeNumerator == 1 && unit::Metre.magnitudeDenominator == 1);
static_assert(unit::Kilogram.magnitudeNumerator == 1 && unit::Kilogram.magnitudeDenominator == 1);
static_assert(unit::Kelvin.offsetNumerator == 0);

// ---- scaled units state an EXACT ratio, never a rounded factor ----

static_assert(unit::Millimetre.magnitudeNumerator == 1 && unit::Millimetre.magnitudeDenominator == 1000);
static_assert(unit::Litre.magnitudeNumerator == 1 && unit::Litre.magnitudeDenominator == 1000);
static_assert(unit::Tonne.magnitudeNumerator == 1000 && unit::Tonne.magnitudeDenominator == 1);
static_assert(unit::Minute.magnitudeNumerator == 60 && unit::Minute.magnitudeDenominator == 1);
static_assert(unit::Hour.magnitudeNumerator == 3600 && unit::Hour.magnitudeDenominator == 1);
static_assert(unit::Percent.magnitudeNumerator == 1 && unit::Percent.magnitudeDenominator == 100);

// The affine one, which is why an offset field exists at all: 0 degC is 273,15 K.
static_assert(unit::Celsius.offsetNumerator == 27315 && unit::Celsius.offsetDenominator == 100);

// ---- a Unit is a template argument, which is what phase 4 needs ----

template <Unit U>
struct Measured
{
    double value {};
};

static_assert(std::is_same_v<Measured<unit::Litre>, Measured<unit::Litre>>);
static_assert(!std::is_same_v<Measured<unit::Litre>, Measured<unit::CubicMetre>>);
static_assert(!std::is_same_v<Measured<unit::Metre>, Measured<unit::Millimetre>>);

// Two units alike in everything but spelling must stay distinct, or a relabelled
// unit would silently collapse into another.
inline constexpr Unit LitreSpelledL { .dimension = dim::Volume,
                                      .magnitudeNumerator = 1,
                                      .magnitudeDenominator = 1000,
                                      .symbolText = formula::symbol("L"),
                                      .decimals = 1 };
static_assert(!std::is_same_v<Measured<unit::Litre>, Measured<LitreSpelledL>>);

TEST_CASE("units report a readable symbol", "[unit]")
{
    CHECK(formula::view(unit::Litre.symbolText) == std::string_view { "l" });
    CHECK(formula::view(unit::Kilogram.symbolText) == std::string_view { "kg" });
    CHECK(formula::view(unit::Celsius.symbolText) == std::string_view { "\xc2\xb0" "C" });
    CHECK(formula::view(unit::Percent.symbolText) == std::string_view { "%" });
}

TEST_CASE("every named unit carries a plausible declared precision", "[unit]")
{
    Unit const all[] = { unit::Metre,  unit::Millimetre, unit::Litre,  unit::CubicMetre, unit::Kilogram,
                         unit::Gram,   unit::Second,     unit::Kelvin, unit::Celsius,    unit::Pascal,
                         unit::Percent, unit::One };
    for (Unit const& u: all)
    {
        INFO(formula::view(u.symbolText));
        CHECK(u.decimals >= 0);
        CHECK(u.decimals <= 18);
        CHECK(u.magnitudeDenominator > 0);
        CHECK(u.offsetDenominator > 0);
        CHECK(u.magnitudeNumerator != 0);
    }
}
```

- [ ] **Step 2: Run it to verify the RED signal**

Add `unit_tests.cpp` to the test executable, then build. Expected: **compile error**,
`formula-cpp/unit.hpp` not found.

- [ ] **Step 3: Write the header**

Create `include/formula-cpp/unit.hpp`. Tasks 5 and 6 append to it.

```cpp
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Units: a dimension, an exact conversion to the coherent SI unit, a display
/// symbol, a declared decimal precision and optional validity bounds.
///
/// Every type here is *structural*, so a unit can be a non-type template
/// parameter -- a quantity's declaration names its unit as a template argument.
/// That rules out `std::string_view` for the symbol and `Rational` for the
/// magnitude, both of which keep private members. Storage is therefore plain
/// public fields; the convenient types appear at the point of use.

#include <formula-cpp/dimension.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace formula
{

/// Bytes available for a unit symbol, including the terminator. Enough for the
/// UTF-8 spellings that occur in practice: `m3`, `°C` (3 bytes), `µm` (3).
inline constexpr std::size_t SymbolCapacity = 16;

/// A fixed-capacity symbol. An array of a structural type is structural, which a
/// `std::string_view` is not -- and unlike a `FixedString<N>` template this keeps
/// `Unit` a single non-template type, so every unit has the same type.
struct Symbol
{
    char characters[SymbolCapacity] {};

    [[nodiscard]] constexpr bool operator==(Symbol const&) const noexcept = default;
};

/// Builds a Symbol, truncating rather than overrunning if the text is too long.
[[nodiscard]] constexpr Symbol symbol(char const* text) noexcept
{
    Symbol result {};
    std::size_t index = 0;
    while (text[index] != '\0' && index + 1 < SymbolCapacity)
    {
        result.characters[index] = text[index];
        ++index;
    }
    return result;
}

/// Reads a Symbol back as a view. The storage has to be structural; this does not.
[[nodiscard]] constexpr std::string_view view(Symbol const& value) noexcept
{
    return std::string_view { value.characters };
}

/// Optional validity range, in the unit's own scale, as exact rationals.
struct Bounds
{
    bool present = false;
    std::int64_t lowNumerator = 0;
    std::int64_t lowDenominator = 1;
    std::int64_t highNumerator = 0;
    std::int64_t highDenominator = 1;

    [[nodiscard]] constexpr bool operator==(Bounds const&) const noexcept = default;
};

[[nodiscard]] constexpr Bounds bounds(std::int64_t lowNumerator,
                                      std::int64_t lowDenominator,
                                      std::int64_t highNumerator,
                                      std::int64_t highDenominator) noexcept
{
    return { true, lowNumerator, lowDenominator, highNumerator, highDenominator };
}

/// A unit of measurement.
///
/// The conversion to the coherent SI unit is affine and exact:
///
///     value_in_SI = value * (magnitudeNumerator / magnitudeDenominator)
///                         + (offsetNumerator / offsetDenominator)
///
/// stated as integer pairs so the whole descriptor stays structural, and applied
/// by multiply-then-divide so that 30 MPa is exactly 30000000 Pa and converts
/// back to exactly 30.
struct Unit
{
    Dimension dimension {};
    std::int64_t magnitudeNumerator = 1;
    std::int64_t magnitudeDenominator = 1;
    std::int64_t offsetNumerator = 0;
    std::int64_t offsetDenominator = 1;
    Symbol symbolText {};
    std::int32_t decimals = 3;
    Bounds bounds {};

    [[nodiscard]] constexpr bool operator==(Unit const&) const noexcept = default;
};

/// Named units. The `decimals` values are ordinary engineering defaults, not
/// requirements from any standard; a caller that needs a different precision
/// states it at the point of use.
namespace unit
{
    inline constexpr Unit One { .dimension = dim::Scalar, .symbolText = symbol(""), .decimals = 3 };
    inline constexpr Unit Percent { .dimension = dim::Scalar,
                                    .magnitudeNumerator = 1,
                                    .magnitudeDenominator = 100,
                                    .symbolText = symbol("%"),
                                    .decimals = 1 };

    inline constexpr Unit Metre { .dimension = dim::Length, .symbolText = symbol("m"), .decimals = 3 };
    inline constexpr Unit Centimetre { .dimension = dim::Length,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 100,
                                       .symbolText = symbol("cm"),
                                       .decimals = 1 };
    inline constexpr Unit Millimetre { .dimension = dim::Length,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000,
                                       .symbolText = symbol("mm"),
                                       .decimals = 1 };
    inline constexpr Unit Kilometre { .dimension = dim::Length,
                                      .magnitudeNumerator = 1000,
                                      .symbolText = symbol("km"),
                                      .decimals = 3 };

    inline constexpr Unit SquareMetre { .dimension = dim::Area, .symbolText = symbol("m2"), .decimals = 4 };
    inline constexpr Unit CubicMetre { .dimension = dim::Volume, .symbolText = symbol("m3"), .decimals = 4 };
    inline constexpr Unit Litre { .dimension = dim::Volume,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 1000,
                                  .symbolText = symbol("l"),
                                  .decimals = 1 };
    inline constexpr Unit Millilitre { .dimension = dim::Volume,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000000,
                                       .symbolText = symbol("ml"),
                                       .decimals = 1 };

    inline constexpr Unit Kilogram { .dimension = dim::Mass, .symbolText = symbol("kg"), .decimals = 3 };
    inline constexpr Unit Gram { .dimension = dim::Mass,
                                 .magnitudeNumerator = 1,
                                 .magnitudeDenominator = 1000,
                                 .symbolText = symbol("g"),
                                 .decimals = 1 };
    inline constexpr Unit Tonne { .dimension = dim::Mass,
                                  .magnitudeNumerator = 1000,
                                  .symbolText = symbol("t"),
                                  .decimals = 3 };

    inline constexpr Unit Second { .dimension = dim::Time, .symbolText = symbol("s"), .decimals = 2 };
    inline constexpr Unit Minute { .dimension = dim::Time,
                                   .magnitudeNumerator = 60,
                                   .symbolText = symbol("min"),
                                   .decimals = 2 };
    inline constexpr Unit Hour { .dimension = dim::Time,
                                 .magnitudeNumerator = 3600,
                                 .symbolText = symbol("h"),
                                 .decimals = 2 };

    inline constexpr Unit Kelvin { .dimension = dim::Temperature, .symbolText = symbol("K"), .decimals = 2 };
    /// The affine unit, and the reason `Unit` carries an offset at all.
    inline constexpr Unit Celsius { .dimension = dim::Temperature,
                                    .offsetNumerator = 27315,
                                    .offsetDenominator = 100,
                                    .symbolText = symbol("\xc2\xb0" "C"),
                                    .decimals = 1 };

    inline constexpr Unit Pascal { .dimension = dim::Pressure, .symbolText = symbol("Pa"), .decimals = 0 };
    inline constexpr Unit Megapascal { .dimension = dim::Pressure,
                                       .magnitudeNumerator = 1000000,
                                       .symbolText = symbol("MPa"),
                                       .decimals = 1 };
} // namespace unit

} // namespace formula
```

Register `unit.hpp` in the top-level `CMakeLists.txt` `FILE_SET HEADERS` list.

- [ ] **Step 4: Run the tests to verify they pass, on both compilers**

```
cmake --build --preset cl-debug   && ctest --preset cl-debug --output-on-failure
cmake --build --preset clangcl-debug && ctest --preset clangcl-debug --output-on-failure
```

Expected: PASS on both.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/unit.hpp test/unit_tests.cpp test/CMakeLists.txt CMakeLists.txt
git commit -m "feat(units): add the unit descriptor and the named units"
```

---

### Task 5: Exact conversion

**Files:**
- Modify: `include/formula-cpp/unit.hpp` (append)
- Modify: `test/unit_tests.cpp` (append)
- Create: `test/negative/unit_dimension_mismatch.cpp`
- Modify: `test/CMakeLists.txt` (register the negative case)

**Interfaces produced**, in `namespace formula`:
- `constexpr std::expected<Rational, ArithmeticError> checked_convert(Rational value, Unit from, Unit to) noexcept`
- `constexpr Rational convert(Rational value, Unit from, Unit to)` — throws on failure
- `template <Unit From, Unit To> struct RequireSameUnitDimension`

**The conversion, stated once so it is not re-derived:**

```
value_in_SI = value * (magnitudeNumerator / magnitudeDenominator)
                    + (offsetNumerator / offsetDenominator)

result      = (value_in_SI - toOffset) / toMagnitude
```

Compute it **through `Rational`** so it inherits exact arithmetic and overflow reporting — spec §6's
rule is that conversion applies exact integer factors by multiply-then-divide, never a precomputed
floating-point factor. Only the stored descriptor is raw integer pairs (Ruling E).

**Already measured in the spike, so these are expectations, not hopes:** `450 l` → `9/20 m³` → back
to exactly `450 l`; `0 °C` → exactly `27315/100 K`; `100 °C` → `37315/100 K` → back to exactly `100 °C`.

- [ ] **Step 1: Write the failing test**

Append to `test/unit_tests.cpp`:

```cpp
// ---- exact conversion ----

using formula::ArithmeticError;
using formula::Rational;

namespace
{
/// Converts in a constant expression, asserting success.
consteval Rational converted(std::int64_t numerator, std::int64_t denominator, Unit from, Unit to)
{
    return formula::convert(*Rational::make(numerator, denominator), from, to);
}
} // namespace

// A volume the spec itself uses: 450 litres is exactly 9/20 of a cubic metre.
static_assert(converted(450, 1, unit::Litre, unit::CubicMetre) == *Rational::make(9, 20));
// And back again, with nothing lost.
static_assert(converted(9, 20, unit::CubicMetre, unit::Litre) == *Rational::make(450, 1));

// Scaling within one dimension.
static_assert(converted(1, 1, unit::Metre, unit::Millimetre) == *Rational::make(1000, 1));
static_assert(converted(1000, 1, unit::Millimetre, unit::Metre) == *Rational::make(1, 1));
static_assert(converted(1, 1, unit::Kilometre, unit::Metre) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::Tonne, unit::Kilogram) == *Rational::make(1000, 1));
static_assert(converted(1, 1, unit::Hour, unit::Second) == *Rational::make(3600, 1));

// 30 MPa is exactly 30000000 Pa, and converts back to exactly 30 -- the spec's
// own worked example of why a precomputed floating-point factor is not enough.
static_assert(converted(30, 1, unit::Megapascal, unit::Pascal) == *Rational::make(30000000, 1));
static_assert(converted(30000000, 1, unit::Pascal, unit::Megapascal) == *Rational::make(30, 1));

// A percentage is a scalar with a magnitude, not a special case.
static_assert(converted(50, 1, unit::Percent, unit::One) == *Rational::make(1, 2));
static_assert(converted(1, 2, unit::One, unit::Percent) == *Rational::make(50, 1));

// ---- the affine case, which is why Unit carries an offset ----

static_assert(converted(0, 1, unit::Celsius, unit::Kelvin) == *Rational::make(27315, 100));
static_assert(converted(100, 1, unit::Celsius, unit::Kelvin) == *Rational::make(37315, 100));
static_assert(converted(27315, 100, unit::Kelvin, unit::Celsius) == *Rational::make(0, 1));
static_assert(converted(37315, 100, unit::Kelvin, unit::Celsius) == *Rational::make(100, 1));

// A DIFFERENCE of temperatures is not affine, but this API converts POINTS, so
// the offset always applies. Pinned so nobody "fixes" it later by dropping it.
static_assert(converted(1, 1, unit::Celsius, unit::Kelvin) == *Rational::make(27415, 100));

// Converting to the same unit is the identity, offset and all.
static_assert(converted(37, 1, unit::Celsius, unit::Celsius) == *Rational::make(37, 1));
static_assert(converted(5, 2, unit::Litre, unit::Litre) == *Rational::make(5, 2));

TEST_CASE("conversion round-trips exactly, in both directions", "[unit]")
{
    struct Pair
    {
        Unit from;
        Unit to;
    };
    Pair const pairs[] = { { unit::Litre, unit::CubicMetre }, { unit::Millimetre, unit::Metre },
                           { unit::Metre, unit::Kilometre },  { unit::Gram, unit::Kilogram },
                           { unit::Minute, unit::Hour },      { unit::Celsius, unit::Kelvin },
                           { unit::Megapascal, unit::Pascal }, { unit::Percent, unit::One } };

    for (Pair const& pair: pairs)
    {
        for (std::int64_t numerator: { -7, -1, 0, 1, 3, 450, 30000 })
        {
            auto const original = Rational::make(numerator, 4);
            REQUIRE(original.has_value());

            auto const there = formula::checked_convert(*original, pair.from, pair.to);
            if (!there)
                continue; // a genuinely unrepresentable intermediate is allowed
            auto const back = formula::checked_convert(*there, pair.to, pair.from);
            REQUIRE(back.has_value());

            INFO(formula::view(pair.from.symbolText) << " -> " << formula::view(pair.to.symbolText));
            CHECK(*back == *original);
        }
    }
}

TEST_CASE("conversion reports failure rather than producing a wrong number", "[unit]")
{
    // A unit whose magnitude would overflow the intermediate.
    Unit const absurd { .dimension = dim::Length,
                        .magnitudeNumerator = 9223372036854775807LL,
                        .magnitudeDenominator = 1,
                        .symbolText = formula::symbol("huge"),
                        .decimals = 0 };

    auto const result = formula::checked_convert(*Rational::make(9223372036854775807LL, 1), absurd, unit::Metre);
    CHECK_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);
}
```

Create `test/negative/unit_dimension_mismatch.cpp`:

```cpp
// SPDX-License-Identifier: Apache-2.0
// Converting a volume into a mass is meaningless. The library must refuse it at
// compile time, in its own words. This must not compile.
#include <formula-cpp/unit.hpp>

namespace unit = formula::unit;

using Checked = formula::RequireSameUnitDimension<unit::Litre, unit::Kilogram>;

int main()
{
    return Checked::value ? 1 : 0;
}
```

Register it:

```cmake
formula_add_negative_test(unit_dimension_mismatch
    "formula: these two units measure different dimensions")
```

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — no `formula::convert`, no `formula::checked_convert`, no
`formula::RequireSameUnitDimension`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/unit.hpp`. It now needs `rational.hpp`, so add
`#include <formula-cpp/rational.hpp>` to the header's include block.

```cpp
/// Fails to compile when two units measure different dimensions.
///
/// Same shape and same reason as `RequireSameDimension`: instantiating a named
/// template on the values makes the compiler print the offending dimensions,
/// and the wording is ours so the negative-compile harness can assert the
/// reason rather than merely the failure.
template <Unit From, Unit To>
struct RequireSameUnitDimension
{
    static_assert(From.dimension == To.dimension,
                  "formula: these two units measure different dimensions, so no conversion between "
                  "them exists; the template arguments printed above name the offending units");

    static constexpr bool value = true;
};

/// Converts @p value from @p from into @p to, exactly.
///
/// Applies integer factors by multiply-then-divide rather than a precomputed
/// floating-point factor, so 30 MPa is exactly 30000000 Pa and converts back to
/// exactly 30. The offset makes the conversion affine, which is what degrees
/// Celsius need; for units without one it is zero and drops out.
///
/// Converts a POINT on the scale, not a difference: 1 degC becomes 274,15 K, not
/// 1 K. A difference-preserving conversion is a different operation and is not
/// this one.
///
/// @return the converted value, or an error if the dimensions differ or an
///         intermediate is not representable. Never a wrong number.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_convert(Rational value,
                                                                                 Unit from,
                                                                                 Unit to) noexcept
{
    if (!(from.dimension == to.dimension))
        return std::unexpected { ArithmeticError::DomainError };

    std::expected<Rational, ArithmeticError> const fromMagnitude =
        Rational::make(from.magnitudeNumerator, from.magnitudeDenominator);
    std::expected<Rational, ArithmeticError> const fromOffset =
        Rational::make(from.offsetNumerator, from.offsetDenominator);
    std::expected<Rational, ArithmeticError> const toMagnitude =
        Rational::make(to.magnitudeNumerator, to.magnitudeDenominator);
    std::expected<Rational, ArithmeticError> const toOffset =
        Rational::make(to.offsetNumerator, to.offsetDenominator);
    if (!fromMagnitude)
        return fromMagnitude;
    if (!fromOffset)
        return fromOffset;
    if (!toMagnitude)
        return toMagnitude;
    if (!toOffset)
        return toOffset;

    std::expected<Rational, ArithmeticError> const scaled = checked_mul(value, *fromMagnitude);
    if (!scaled)
        return scaled;
    std::expected<Rational, ArithmeticError> const inSi = checked_add(*scaled, *fromOffset);
    if (!inSi)
        return inSi;
    std::expected<Rational, ArithmeticError> const shifted = checked_sub(*inSi, *toOffset);
    if (!shifted)
        return shifted;
    return checked_div(*shifted, *toMagnitude);
}

/// @throws ArithmeticException when the conversion cannot be represented.
[[nodiscard]] constexpr Rational convert(Rational value, Unit from, Unit to)
{
    return detail::or_throw(checked_convert(value, from, to));
}
```

- [ ] **Step 4: Run the tests to verify they pass, on both compilers**

Expected: PASS on `cl-debug` and `clangcl-debug`, including `negative.unit_dimension_mismatch`.

- [ ] **Step 5: Prove the negative test asserts the reason**

Same procedure as Task 3 Step 5: wrong expected text must FAIL, correct text must PASS. Report both.

- [ ] **Step 6: Commit**

```bash
git add include/formula-cpp/unit.hpp test/unit_tests.cpp test/negative/unit_dimension_mismatch.cpp test/CMakeLists.txt
git commit -m "feat(units): add exact unit conversion"
```

---

### Task 6: Bounds and declared decimals

**Files:**
- Modify: `include/formula-cpp/unit.hpp` (append)
- Modify: `test/unit_tests.cpp` (append)

**Interfaces produced**, in `namespace formula`:
- `enum class BoundsCheck : std::uint8_t { WithinBounds, BelowMinimum, AboveMaximum, NotChecked };`
- `constexpr std::string_view describe(BoundsCheck) noexcept`
- `constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(Rational value, Unit unitOfValue) noexcept`
- `constexpr DecimalPlaces declared_decimals(Unit) noexcept`
- `constexpr std::expected<Rational, ArithmeticError> round_to_declared(Rational value, Unit unitOfValue, RoundingMode mode) noexcept`

`unit.hpp` now also needs `#include <formula-cpp/rounding.hpp>`.

**Why `NotChecked` rather than "true":** a unit with no declared bounds has not been checked, which
is different from having been checked and passed. Collapsing the two is how an unbounded unit comes
to look validated in a report.

- [ ] **Step 1: Write the failing test**

Append to `test/unit_tests.cpp`:

```cpp
// ---- bounds ----

using formula::BoundsCheck;

namespace
{
/// A unit with bounds, for the tests: 0 to 100, in its own scale.
inline constexpr Unit BoundedPercent { .dimension = dim::Scalar,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 100,
                                       .symbolText = formula::symbol("%"),
                                       .decimals = 1,
                                       .bounds = formula::bounds(0, 1, 100, 1) };
} // namespace

static_assert(!unit::Litre.bounds.present);
static_assert(BoundedPercent.bounds.present);

static_assert(*formula::checked_within_bounds(*Rational::make(50, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(0, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(100, 1), BoundedPercent)
              == BoundsCheck::WithinBounds);
static_assert(*formula::checked_within_bounds(*Rational::make(-1, 1), BoundedPercent)
              == BoundsCheck::BelowMinimum);
static_assert(*formula::checked_within_bounds(*Rational::make(101, 1), BoundedPercent)
              == BoundsCheck::AboveMaximum);

// An unbounded unit is NOT "within bounds" -- it was never checked, and saying
// otherwise is how an unvalidated value comes to look validated.
static_assert(*formula::checked_within_bounds(*Rational::make(1000000, 1), unit::Litre)
              == BoundsCheck::NotChecked);

static_assert(!formula::describe(BoundsCheck::WithinBounds).empty());
static_assert(formula::describe(BoundsCheck::NotChecked) != formula::describe(BoundsCheck::WithinBounds));

// ---- declared decimals ----

static_assert(formula::declared_decimals(unit::Litre).value == 1);
static_assert(formula::declared_decimals(unit::CubicMetre).value == 4);
static_assert(formula::declared_decimals(unit::Pascal).value == 0);

static_assert(*formula::round_to_declared(*Rational::make(1234, 1000), unit::Litre,
                                          formula::RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(12, -1));
static_assert(*formula::round_to_declared(*Rational::make(15, 10), unit::Pascal,
                                          formula::RoundingMode::HalfAwayFromZero)
              == *Rational::make(2, 1));

TEST_CASE("bounds distinguish not-checked from checked-and-passed", "[unit]")
{
    CHECK(*formula::checked_within_bounds(*Rational::make(1, 2), BoundedPercent) == BoundsCheck::WithinBounds);
    CHECK(*formula::checked_within_bounds(*Rational::make(1, 2), unit::One) == BoundsCheck::NotChecked);

    // Every outcome has its own distinct wording.
    BoundsCheck const all[] = { BoundsCheck::WithinBounds, BoundsCheck::BelowMinimum, BoundsCheck::AboveMaximum,
                                BoundsCheck::NotChecked };
    for (std::size_t i = 0; i < std::size(all); ++i)
    {
        CHECK_FALSE(formula::describe(all[i]).empty());
        for (std::size_t j = i + 1; j < std::size(all); ++j)
            CHECK(formula::describe(all[i]) != formula::describe(all[j]));
    }
}

TEST_CASE("rounding to a unit's declared precision uses that unit's decimals", "[unit]")
{
    Rational const value = *Rational::make(123456, 1000); // 123,456

    CHECK(*formula::round_to_declared(value, unit::Litre, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(1235, -1));
    CHECK(*formula::round_to_declared(value, unit::Pascal, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::make(123, 1));
    CHECK(*formula::round_to_declared(value, unit::CubicMetre, formula::RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(123456, -3));
}
```

- [ ] **Step 2: Run to verify the RED signal**

Expected: **compile error** — no `formula::BoundsCheck`, no `formula::declared_decimals`.

- [ ] **Step 3: Write the implementation**

Append to `include/formula-cpp/unit.hpp`:

```cpp
/// The outcome of checking a value against its unit's declared bounds.
enum class BoundsCheck : std::uint8_t
{
    /// Bounds were declared and the value lies within them, inclusive.
    WithinBounds,
    /// Below the declared minimum.
    BelowMinimum,
    /// Above the declared maximum.
    AboveMaximum,
    /// The unit declares no bounds, so nothing was checked. Deliberately NOT
    /// the same as WithinBounds: a value that was never checked must not be
    /// reported as one that was checked and passed.
    NotChecked,
};

[[nodiscard]] constexpr std::string_view describe(BoundsCheck outcome) noexcept
{
    switch (outcome)
    {
        case BoundsCheck::WithinBounds: return "within the declared bounds";
        case BoundsCheck::BelowMinimum: return "below the declared minimum";
        case BoundsCheck::AboveMaximum: return "above the declared maximum";
        case BoundsCheck::NotChecked: return "no bounds declared for this unit";
    }
    return "unknown bounds outcome";
}

/// Checks @p value, expressed in @p unitOfValue, against that unit's bounds.
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(
    Rational value, Unit unitOfValue) noexcept
{
    if (!unitOfValue.bounds.present)
        return BoundsCheck::NotChecked;

    std::expected<Rational, ArithmeticError> const low =
        Rational::make(unitOfValue.bounds.lowNumerator, unitOfValue.bounds.lowDenominator);
    std::expected<Rational, ArithmeticError> const high =
        Rational::make(unitOfValue.bounds.highNumerator, unitOfValue.bounds.highDenominator);
    if (!low)
        return std::unexpected { low.error() };
    if (!high)
        return std::unexpected { high.error() };

    if (value < *low)
        return BoundsCheck::BelowMinimum;
    if (value > *high)
        return BoundsCheck::AboveMaximum;
    return BoundsCheck::WithinBounds;
}

/// The unit's declared display precision, as the rounding layer's own type.
[[nodiscard]] constexpr DecimalPlaces declared_decimals(Unit unitOfValue) noexcept
{
    return DecimalPlaces { unitOfValue.decimals };
}

/// Rounds @p value to the precision its unit declares.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> round_to_declared(Rational value,
                                                                                   Unit unitOfValue,
                                                                                   RoundingMode mode) noexcept
{
    return checked_round(value, declared_decimals(unitOfValue), mode);
}
```

- [ ] **Step 4: Run the tests on both compilers**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/formula-cpp/unit.hpp test/unit_tests.cpp
git commit -m "feat(units): add bounds checking and declared precision"
```

---

### Task 7: Umbrella, example, documentation, packaging

**Files:**
- Modify: `include/formula-cpp/formula.hpp`
- Create: `examples/dimensions_and_units.cpp`
- Modify: `examples/CMakeLists.txt`
- Create: `docs/dimensions.md`
- Modify: `mkdocs.yml`, `README.md`

- [ ] **Step 1: Add both headers to the umbrella**

Add `#include <formula-cpp/dimension.hpp>` and `#include <formula-cpp/unit.hpp>` to
`include/formula-cpp/formula.hpp`, keeping the list alphabetical.

- [ ] **Step 2: Write the example**

Create `examples/dimensions_and_units.cpp`. It must include `<formula-cpp/formula.hpp>`, not the
individual headers, so that the umbrella is what gets exercised. Generic physics only — no
standard cited, no real threshold values.

Cover, in this order, with the computed values printed:
1. Composing dimensions from named constants: area from length, volume from area, density from mass and volume.
2. A dimension that only rational exponents can express: the square root of a length.
3. Exact conversion that round-trips: 450 l to m³ and back.
4. The affine case: 100 °C to K and back.
5. A unit's declared precision applied to a computed value.
6. A bounds check reporting `NotChecked` for an unbounded unit, and a real verdict for a bounded one.

Register it in `examples/CMakeLists.txt` with a `PASS_REGULAR_EXPRESSION` on a line that only
appears when the numbers are right.

- [ ] **Step 3: Run the example and paste its real output into the report**

Do not write the expected output from reasoning; run it and record what it prints.

- [ ] **Step 4: Write the documentation page**

Create `docs/dimensions.md`. Required sections:
1. **Why dimensions are types** — a dimension mismatch is a compile error, not a runtime check, and the diagnostic names both exponent vectors.
2. **Composing dimensions** — the `dim::` constants and the algebra; never spell exponents by hand.
3. **Rational exponents** — why integers are not enough, with the square-root and two-thirds-power examples.
4. **Units** — what a `Unit` carries and why each field is there.
5. **Exact conversion** — multiply-then-divide, the 30 MPa example, and the affine `°C` case including the point-versus-difference caveat.
6. **Declared precision and bounds** — and why `NotChecked` is not `WithinBounds`.
7. **Limits** — `Exponent` is `std::int32_t`; `SymbolCapacity` is 16 bytes; conversion inherits the rounding limits documented in `docs/numbers.md`, and a link to them.

**Every numeric claim in this page must be produced by running code, not by reasoning about it.**
Phase 2 shipped six wrong claims in one prose section by doing the opposite. Put the snippet in the
page only after you have compiled and run it.

- [ ] **Step 5: Add the page to `mkdocs.yml` nav and one accurate line to `README.md`**

The README must claim only what this branch ships: dimensional analysis and units. No expression
trees, no traceability.

- [ ] **Step 6: Run the full four-preset matrix**

```
cmake --build --preset cl-debug        && ctest --preset cl-debug --output-on-failure
cmake --build --preset cl-release      && ctest --preset cl-release --output-on-failure
cmake --build --preset clangcl-debug   && ctest --preset clangcl-debug --output-on-failure
cmake --build --preset clangcl-release && ctest --preset clangcl-release --output-on-failure
```

Pass counts must be identical across all four.

- [ ] **Step 7: Verify the installed package**

Configure with `-DFORMULA_BUILD_TESTS=OFF -DFORMULA_INSTALL=ON`, install to a staging directory,
confirm **all ten** headers land there, and compile a small consumer against `stage/include` only —
not the source tree. That last step is what catches a header that builds in-tree but is missing
from the install manifest; this repository has shipped that defect once already.

- [ ] **Step 8: Commit**

```bash
git add include/formula-cpp/formula.hpp examples/dimensions_and_units.cpp examples/CMakeLists.txt docs/dimensions.md mkdocs.yml README.md
git commit -m "docs(dimensions): document and demonstrate dimensions and units"
```

---

## Self-Review

**1. Spec coverage.**

| Spec requirement | Task |
|---|---|
| §7 `Dimension` as a structural exponent vector, NTTP-capable | 2 |
| §7 rational exponents, driven by roots and fractional powers | 1, 2 |
| §7 users compose named constants, never spell exponents | 2 (`dim::`) |
| §7 mismatch is a `static_assert` in a named helper, printing the vectors | 3 |
| §8 unit carries dimension, exact magnitude, affine offset, symbol, default precision, bounds | 4, 6 |
| §6 conversion by exact integer factors, multiply-then-divide | 5 |
| §17 phase 3 "decimals + bounds" | 6 |

Deliberately **not** here: `Quantity`, `Describe<T>` and the CRTP declaration are the rest of §8 and
belong to spec phase 4; this plan delivers the `Unit` they will name. Verified that `Unit` can be a
template argument, which is what phase 4 requires of it.

**2. Placeholder scan.** No "TBD", no "add error handling", no "similar to Task N". Every code step
carries its code. Task 7's example and documentation are specified by required content rather than
verbatim text, because their numbers must come from running the code — which is a requirement, not
a placeholder.

**3. Type consistency.**
- `Exponent` is defined once (Task 1) and used by `Dimension` (Task 2) and `Unit` (Task 4).
- `dim::` constants are `CamelCase` per project convention, deviating from the spec's lowercase
  prose spelling; noted in Task 2 so the deviation is deliberate and visible.
- `RequireSameDimension` (Task 3) and `RequireSameUnitDimension` (Task 5) are distinct templates
  with distinct messages; both negative tests assert their own text.
- `checked_convert` returns `std::expected<Rational, ArithmeticError>`, matching the phase 2
  convention that `checked_` means exactly that.
- `declared_decimals` returns `DecimalPlaces`, the rounding layer's type, not a bare `int`.
- `unit.hpp` includes `dimension.hpp`, `rational.hpp` and `rounding.hpp`; `dimension.hpp` includes
  only `<cstdint>`. No cycle.

**4. Ordering hazards.** `Dimension` must be defined before `Unit` uses it, and `Rational` before
`checked_convert`. Task order respects both. `RequireSameUnitDimension` is declared before
`checked_convert` in Task 5's listing, and the header has no forward declarations.
