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

// Named *_or_none, not checked_*: `checked_` is reserved elsewhere in this
// library for a function returning `std::expected<T, ArithmeticError>`. These
// return `std::optional<Int>` -- a different contract that must not share the
// prefix meant to promise the first one.

[[nodiscard]] constexpr std::optional<Int> add_checked_or_none(Int lhs, Int rhs) noexcept
{
    if (add_overflows(lhs, rhs))
        return std::nullopt;
    return lhs + rhs;
}

[[nodiscard]] constexpr std::optional<Int> sub_checked_or_none(Int lhs, Int rhs) noexcept
{
    if (sub_overflows(lhs, rhs))
        return std::nullopt;
    return lhs - rhs;
}

[[nodiscard]] constexpr std::optional<Int> mul_checked_or_none(Int lhs, Int rhs) noexcept
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
    return mul_checked_or_none(value, *factor);
}

} // namespace formula::detail
