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
        case RoundingMode::HalfAwayFromZero:
            return "nearest, ties away from zero";
        case RoundingMode::HalfTowardZero:
            return "nearest, ties toward zero";
        case RoundingMode::HalfEven:
            return "nearest, ties to even";
        case RoundingMode::Ceiling:
            return "toward positive infinity";
        case RoundingMode::Floor:
            return "toward negative infinity";
        case RoundingMode::TowardZero:
            return "toward zero";
        case RoundingMode::AwayFromZero:
            return "away from zero";
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
[[nodiscard]] constexpr std::expected<Rational::Int, ArithmeticError> checked_round_to_int(Rational value,
                                                                                           RoundingMode mode) noexcept
{
    auto const split = detail::floor_divmod(value.numerator(), value.denominator());
    if (split.remainder == 0)
        return split.quotient;

    auto const toCeiling = [&]() -> std::expected<Rational::Int, ArithmeticError> {
        std::optional<Rational::Int> const raised = detail::add_checked_or_none(split.quotient, Rational::Int { 1 });
        if (!raised)
            return std::unexpected { ArithmeticError::Overflow };
        return *raised;
    };

    bool const positive = value.numerator() > 0;

    switch (mode)
    {
        case RoundingMode::Floor:
            return split.quotient;
        case RoundingMode::Ceiling:
            return toCeiling();
        case RoundingMode::TowardZero:
            return positive ? std::expected<Rational::Int, ArithmeticError> { split.quotient } : toCeiling();
        case RoundingMode::AwayFromZero:
            return positive ? toCeiling() : std::expected<Rational::Int, ArithmeticError> { split.quotient };
        default:
            break;
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
            return split.quotient % 2 == 0 ? std::expected<Rational::Int, ArithmeticError> { split.quotient } : toCeiling();
        default:
            break;
    }

    return std::unexpected { ArithmeticError::DomainError };
}

[[nodiscard]] constexpr Rational::Int round_to_int(Rational value, RoundingMode mode)
{
    return detail::or_throw(checked_round_to_int(value, mode));
}

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_integer(Rational value,
                                                                                          RoundingMode mode) noexcept
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
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_multiple(Rational value,
                                                                                           Rational step,
                                                                                           RoundingMode mode) noexcept
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

namespace detail
{
    /// Whether `|numerator| / denominator >= 10^exponent`, exactly and without
    /// ever constructing 10^exponent as a Rational -- which is impossible at the
    /// extremes of the representable range.
    [[nodiscard]] constexpr bool at_least_pow10(std::uint64_t numerator, std::uint64_t denominator, int exponent) noexcept
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

[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round(Rational value,
                                                                               DecimalPlaces places,
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
/// Two separate limits apply, and both fail as a reported Overflow, never as a
/// wrong number:
///
/// - `from_double_exact` needs the double's exact binary value to be
///   representable, which fails outright for a full-mantissa value below
///   `2^-10`, about 0,00098. Measured: `0.0009765625` converts, `0.0001` does not.
/// - Rounding to a POSITIVE number of places `N` scales by `10^N`, cancelling
///   common factors of two against the denominator first, so what must fit in
///   `Int` is `|numerator| * (10^N / gcd(10^N, denominator))` -- for a binary
///   denominator, `|numerator| * 5^N`. There the limit is set by the
///   **numerator's** magnitude, not the denominator's and not the value's size:
///   `1 / 2^60` rounds at all 18 places, while `8106479329266893 / 2^54`
///   manages 4. A `double`'s mantissa is always about 53 bits whatever its
///   exponent, so every value from this function caps out at 4 decimal places.
///   A value built with `from_decimal` has a tiny numerator and is unaffected
///   at positive places.
/// - Rounding to a NEGATIVE number of places -- to whole tens or hundreds --
///   uses an integer step, which multiplies the **denominator** instead. There
///   the denominator is the constraint, and the two cases invert: `1/10^18` is
///   refused at every negative place while `1/3` handles all of them.
///
/// Prefer `from_decimal` for an exact decimal; use this function only for a
/// genuinely measured `double`, and only at modest decimal precision.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rational_from_double(double value,
                                                                                      DecimalPlaces places,
                                                                                      RoundingMode mode) noexcept
{
    std::expected<Rational, ArithmeticError> const exactValue = Rational::from_double_exact(value);
    if (!exactValue)
        return exactValue;
    return checked_round(*exactValue, places, mode);
}

} // namespace formula
