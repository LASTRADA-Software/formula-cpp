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
    return leftOperand + rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> sub_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (sub_overflows(leftOperand, rightOperand))
        return std::nullopt;
    return leftOperand - rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> mul_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (mul_overflows(leftOperand, rightOperand))
        return std::nullopt;
    return leftOperand * rightOperand;
}

/// Absolute value as an unsigned quantity. Exists because `-IntMin` overflows
/// but `magnitude(IntMin)` is an ordinary number.
[[nodiscard]] constexpr std::uint64_t magnitude(Int operandValue) noexcept
{
    return operandValue < 0 ? ~static_cast<std::uint64_t>(operandValue) + 1U : static_cast<std::uint64_t>(operandValue);
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
