// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Overflow-detecting integer primitives, all constexpr and free of compiler
/// intrinsics. MSVC has no __builtin_*_overflow, and its <intrin.h> equivalents
/// are not constexpr, so the checks are written in portable C++ and used on
/// every compiler. Optimisers recognise these idioms.
///
/// **The overflow census.** This repository's own census programs are compiled
/// with `FORMULA_OVERFLOW_CENSUS` defined. The macro is internal to them: it
/// is not part of the library's contract, no consumer's build defines it,
/// and it must be defined in all of a program's translation units or in
/// none -- one of each gives the arithmetic below two definitions, which the
/// linker would silently merge. cl and clang-cl refuse such a program at
/// link time (`#pragma detect_mismatch`, below: LNK2038); an ELF linker has
/// no such check.
/// Then every integer these primitives form at run time -- and every
/// numerator and denominator `Rational::make` is handed -- is reported to
/// `census_record`, which the census program defines
/// (`support/census_tally.cpp`), so that it can say how many of the 63 bits
/// real formulas use (`docs/numeric-headroom.md`). A constant evaluation
/// reports nothing. Without the macro -- every build but the census's --
/// `FORMULA_CENSUS_NOTE` expands to nothing, its arguments are never
/// evaluated, and none of the census's names exist: no call, no symbol, no
/// cost.

#include <cstdint>
#include <optional>

namespace formula::detail
{

using Int = std::int64_t;

inline constexpr Int IntMax = 9223372036854775807LL;
inline constexpr Int IntMin = -IntMax - 1;

/// Absolute value as an unsigned quantity. Exists because `-IntMin` overflows
/// but `magnitude(IntMin)` is an ordinary number.
[[nodiscard]] constexpr std::uint64_t magnitude(Int operandValue) noexcept
{
    return operandValue < 0 ? ~static_cast<std::uint64_t>(operandValue) + 1U : static_cast<std::uint64_t>(operandValue);
}

#if defined(_MSC_VER)
    #if defined(FORMULA_OVERFLOW_CENSUS)
        #pragma detect_mismatch("formula_overflow_census", "on")
    #else
        #pragma detect_mismatch("formula_overflow_census", "off")
    #endif
#endif

#if defined(FORMULA_OVERFLOW_CENSUS)
/// What an integer the overflow census is told of was: a numerator or a
/// denominator handed to `Rational::make`, any other signed intermediate, or
/// an unsigned one (`rounded_sqrt`'s, which has 64 bits to use).
enum class CensusRole : std::uint8_t
{
    Numerator,
    Denominator,
    Intermediate,
    Unsigned,
};

/// Told the magnitude of an integer formed at run time. Declared here and
/// defined only by the census program, never by the library.
void census_record(CensusRole role, std::uint64_t magnitudeSeen) noexcept;

/// Tells the overflow census of @p magnitudeSeen, unless this is a constant
/// evaluation.
constexpr void census_note(CensusRole role, std::uint64_t magnitudeSeen) noexcept
{
    if !consteval
    {
        census_record(role, magnitudeSeen);
    }
}

    /// Tells the overflow census that an integer of @p magnitudeSeen was formed
    /// in @p role (a `CensusRole` enumerator's name). See the file comment.
    #define FORMULA_CENSUS_NOTE(role, magnitudeSeen) \
        ::formula::detail::census_note(::formula::detail::CensusRole::role, (magnitudeSeen))
#else
    /// Nothing: this is not a census build. The arguments are not evaluated.
    #define FORMULA_CENSUS_NOTE(role, magnitudeSeen) static_cast<void>(0)
#endif

/// True when `leftOperand + rightOperand` is not representable.
[[nodiscard]] constexpr bool add_overflows(Int leftOperand, Int rightOperand) noexcept
{
    return (rightOperand > 0 && leftOperand > IntMax - rightOperand)
           || (rightOperand < 0 && leftOperand < IntMin - rightOperand);
}

/// True when `leftOperand - rightOperand` is not representable.
[[nodiscard]] constexpr bool sub_overflows(Int leftOperand, Int rightOperand) noexcept
{
    return (rightOperand < 0 && leftOperand > IntMax + rightOperand)
           || (rightOperand > 0 && leftOperand < IntMin + rightOperand);
}

/// True when `leftOperand * rightOperand` is not representable.
[[nodiscard]] constexpr bool mul_overflows(Int leftOperand, Int rightOperand) noexcept
{
    if (leftOperand == 0 || rightOperand == 0)
        return false;
    if (leftOperand > 0)
        return rightOperand > 0 ? leftOperand > IntMax / rightOperand : rightOperand < IntMin / leftOperand;
    return rightOperand > 0 ? leftOperand < IntMin / rightOperand : leftOperand < IntMax / rightOperand;
}

// Named *_or_none, not checked_*: `checked_` is reserved elsewhere in this
// library for a function returning `std::expected<T, ArithmeticError>`. These
// return `std::optional<Int>` -- a different contract that must not share the
// prefix meant to promise the first one.

[[nodiscard]] constexpr std::optional<Int> add_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (add_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand + rightOperand));
    return leftOperand + rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> sub_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (sub_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand - rightOperand));
    return leftOperand - rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> mul_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (mul_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand * rightOperand));
    return leftOperand * rightOperand;
}

/// Greatest common divisor. `gcd(0, n) == n` and `gcd(0, 0) == 0`.
/// Unsigned so that `magnitude(IntMin)` is a legal argument.
[[nodiscard]] constexpr std::uint64_t gcd(std::uint64_t leftOperand, std::uint64_t rightOperand) noexcept
{
    while (rightOperand != 0)
    {
        std::uint64_t const remainder = leftOperand % rightOperand;
        leftOperand = rightOperand;
        rightOperand = remainder;
    }
    return leftOperand;
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
/// @pre `divisor > 0`.
[[nodiscard]] constexpr DivMod floor_divmod(Int dividend, Int divisor) noexcept
{
    Int truncated = dividend / divisor;
    Int remainder = dividend % divisor;
    if (remainder < 0)
    {
        // Safe: a non-zero remainder means |quotient| is strictly below
        // |numerator| / denominator, so the decrement cannot reach IntMin, and
        // remainder is greater than -denominator.
        --truncated;
        remainder += divisor;
    }
    return { truncated, remainder };
}

/// `10^exponent` for `0 <= exponent <= 18`; `nullopt` otherwise. 10^18 is the
/// largest power of ten representable in Int.
[[nodiscard]] constexpr std::optional<Int> pow10(int exponent) noexcept
{
    if (exponent < 0 || exponent > 18)
        return std::nullopt;
    Int power = 1;
    for (int multiplied = 0; multiplied < exponent; ++multiplied)
        power *= 10;
    return power;
}

/// Number of decimal digits in `|value|`. Zero has one digit.
[[nodiscard]] constexpr int decimal_digits(Int operandValue) noexcept
{
    std::uint64_t remaining = magnitude(operandValue);
    int digitCount = 1;
    while (remaining >= 10U)
    {
        remaining /= 10U;
        ++digitCount;
    }
    return digitCount;
}

/// `operandValue * 10^exponent`, or `nullopt` on overflow or an out-of-range exponent.
[[nodiscard]] constexpr std::optional<Int> mul_pow10(Int operandValue, int exponent) noexcept
{
    std::optional<Int> const powerOfTen = pow10(exponent);
    if (!powerOfTen)
        return std::nullopt;
    return mul_checked_or_none(operandValue, *powerOfTen);
}

} // namespace formula::detail
