// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Rounding as specified behaviour, not as formatting.
///
/// Norm methods state where rounding happens and which way it goes, and
/// intermediate and final rounding routinely differ within one method. A library
/// that rounds only on output produces wrong numbers, so rounding here is an
/// operation over exact values that yields another exact value. It is also a
/// node in the expression tree, and the result carries into the trace.

#include <formula-cpp/error.hpp>
#include <formula-cpp/rational.hpp>

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
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
    /// Nearest; ties move to the larger magnitude. 1.25 -> 1.3 and -1.25 -> -1.3.
    HalfAwayFromZero,
    /// Nearest; ties move to the smaller magnitude. 1.25 -> 1.2 and -1.25 -> -1.2.
    HalfTowardZero,
    /// Nearest; ties move to the neighbour whose last kept digit is even.
    /// 1.25 -> 1.2 and 1.35 -> 1.4. The IEEE-754 decimal default.
    HalfEven,
    /// Always toward positive infinity. 1.21 -> 1.3 and -1.21 -> -1.2.
    Ceiling,
    /// Always toward negative infinity. 1.29 -> 1.2 and -1.21 -> -1.3.
    Floor,
    /// Always toward zero; plain truncation. 1.29 -> 1.2 and -1.29 -> -1.2.
    TowardZero,
    /// Always away from zero. 1.21 -> 1.3 and -1.21 -> -1.3.
    /// On a non-negative quantity this coincides with Ceiling, which is what
    /// "rounded up" usually means in norm text; on a signed quantity it does not.
    AwayFromZero,
};

/// `roundingMode` in prose, for a trace or an error message.
[[nodiscard]] constexpr std::string_view describe(RoundingMode roundingMode) noexcept
{
    switch (roundingMode)
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

namespace detail
{
template <>
inline constexpr bool formats_by_describe<RoundingMode> = true;
} // namespace detail

/// A decimal scale. Negative values are meaningful: DecimalPlaces { -1 } rounds
/// to whole tens, which norms do ask for.
struct DecimalPlaces
{
    /// How many places past the decimal point to round to; negative rounds to
    /// whole tens, hundreds, and so on.
    std::int32_t value {};
    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(DecimalPlaces const&) const noexcept = default;
};

/// A count of significant digits. Must be at least 1.
struct SignificantDigits
{
    /// How many significant digits to keep.
    std::int32_t value {};
    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SignificantDigits const&) const noexcept = default;
};

/// Rounds to a whole number under `roundingMode`.
///
/// The whole decision is expressed against a floored quotient, so the remainder
/// is always in `[0, denominator)` and "which side is it on" is a comparison of
/// `remainder` with `denominator - remainder` -- no doubling, so no overflow.
[[nodiscard]] constexpr std::expected<Rational::Int, ArithmeticError> checked_round_to_int(
    Rational unrounded, RoundingMode roundingMode) noexcept
{
    auto const split = detail::floor_divmod(unrounded.numerator(), unrounded.denominator());
    if (split.remainder == 0)
        return split.quotient;

    auto const toCeiling = [&]() -> std::expected<Rational::Int, ArithmeticError> {
        std::optional<Rational::Int> const raised = detail::add_checked_or_none(split.quotient, Rational::Int { 1 });
        if (!raised)
            return std::unexpected { ArithmeticError::Overflow };
        return *raised;
    };

    bool const positive = unrounded.numerator() > 0;

    switch (roundingMode)
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
    Rational::Int const distanceUp = unrounded.denominator() - split.remainder;
    if (split.remainder < distanceUp)
        return split.quotient;
    if (split.remainder > distanceUp)
        return toCeiling();

    switch (roundingMode)
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

/// Throwing spelling of `checked_round_to_int`.
[[nodiscard]] constexpr Rational::Int round_to_int(Rational unrounded, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round_to_int(unrounded, roundingMode));
}

/// Rounds to a whole number under `roundingMode`, as a `Rational` rather than a bare `Int`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_integer(Rational unrounded,
                                                                                          RoundingMode roundingMode) noexcept
{
    std::expected<Rational::Int, ArithmeticError> const rounded = checked_round_to_int(unrounded, roundingMode);
    if (!rounded)
        return std::unexpected { rounded.error() };
    return Rational { *rounded };
}

/// Throwing spelling of `checked_round_to_integer`.
[[nodiscard]] constexpr Rational round_to_integer(Rational unrounded, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round_to_integer(unrounded, roundingMode));
}

/// Rounds `unrounded` to the nearest multiple of `increment` under `roundingMode`.
///
/// This is the primitive the decimal-place and significant-digit forms are built
/// on, and it is also what snapping a computed sieve size onto a standard sieve
/// series needs (`snap.hpp`).
///
/// @pre `increment` is strictly positive; otherwise DomainError.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_multiple(
    Rational unrounded, Rational increment, RoundingMode roundingMode) noexcept
{
    if (increment.sign() <= 0)
        return std::unexpected { ArithmeticError::DomainError };

    std::expected<Rational, ArithmeticError> const multiples = checked_div(unrounded, increment);
    if (!multiples)
        return multiples;

    std::expected<Rational::Int, ArithmeticError> const steps = checked_round_to_int(*multiples, roundingMode);
    if (!steps)
        return std::unexpected { steps.error() };

    return checked_mul(Rational { *steps }, increment);
}

/// Throwing spelling of `checked_round_to_multiple`.
[[nodiscard]] constexpr Rational round_to_multiple(Rational unrounded, Rational increment, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round_to_multiple(unrounded, increment, roundingMode));
}

namespace detail
{
    /// Whether `|numerator| / denominator >= 10^exponent`, exactly and without
    /// ever constructing 10^exponent as a Rational -- which is impossible at the
    /// extremes of the representable range. A scaled side beyond the largest
    /// `Rational::Int` already decides the comparison, and is never formed.
    [[nodiscard]] constexpr bool at_least_pow10(UInt128 magnitudeNumerator,
                                                UInt128 magnitudeDenominator,
                                                int exponent) noexcept
    {
        constexpr UInt128 Largest = wide_magnitude(std::numeric_limits<Rational::Int>::max());
        if (exponent >= 0)
        {
            std::optional<UInt128> const powerOfTen = u128_pow10(exponent);
            std::optional<UInt128> const scaledDenominator =
                powerOfTen ? u128_mul_checked(magnitudeDenominator, *powerOfTen) : std::nullopt;
            // Beyond the largest numerator, the quotient is below 10^exponent.
            if (!scaledDenominator || Largest < *scaledDenominator)
                return false;
            FORMULA_CENSUS_NOTE(Intermediate, *scaledDenominator);
            return !(magnitudeNumerator < *scaledDenominator);
        }
        std::optional<UInt128> const powerOfTen = u128_pow10(-exponent);
        std::optional<UInt128> const scaledNumerator =
            powerOfTen ? u128_mul_checked(magnitudeNumerator, *powerOfTen) : std::nullopt;
        if (!scaledNumerator || Largest < *scaledNumerator)
            return true;
        FORMULA_CENSUS_NOTE(Intermediate, *scaledNumerator);
        return !(*scaledNumerator < magnitudeDenominator);
    }
} // namespace detail

/// `floor(log10(|value|))`: the exponent `e` with `10^e <= |value| < 10^(e+1)`.
///
/// @return DomainError for zero, which has no decimal exponent.
[[nodiscard]] constexpr std::expected<int, ArithmeticError> checked_decimal_exponent(Rational examinedValue) noexcept
{
    if (examinedValue.is_zero())
        return std::unexpected { ArithmeticError::DomainError };

    detail::UInt128 const magnitudeNumerator = detail::wide_magnitude(examinedValue.numerator());
    detail::UInt128 const magnitudeDenominator = detail::wide_magnitude(examinedValue.denominator());

    // The digit counts bracket the answer to within one: with dn digits in the
    // numerator and dd in the denominator, floor(log10(n/d)) is either
    // dn - dd or dn - dd - 1.
    int exponent = detail::decimal_digits(examinedValue.numerator()) - detail::decimal_digits(examinedValue.denominator());
    if (!detail::at_least_pow10(magnitudeNumerator, magnitudeDenominator, exponent))
        --exponent;
    return exponent;
}

/// Rounds `unrounded` to `places` decimal places under `roundingMode`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round(Rational unrounded,
                                                                               DecimalPlaces places,
                                                                               RoundingMode roundingMode) noexcept
{
    // The step is 10^-places, which must itself be representable.
    if (places.value > 18 || places.value < -18)
        return std::unexpected { ArithmeticError::Overflow };

    std::expected<Rational, ArithmeticError> const increment = Rational::from_decimal(1, -places.value);
    if (!increment)
        return increment;
    return checked_round_to_multiple(unrounded, *increment, roundingMode);
}

/// Throwing spelling of `checked_round(Rational, DecimalPlaces, RoundingMode)`.
[[nodiscard]] constexpr Rational round(Rational unrounded, DecimalPlaces places, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round(unrounded, places, roundingMode));
}

/// Rounds `unrounded` to `significant` significant digits under `roundingMode`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round(Rational unrounded,
                                                                               SignificantDigits significant,
                                                                               RoundingMode roundingMode) noexcept
{
    if (significant.value < 1)
        return std::unexpected { ArithmeticError::DomainError };
    if (unrounded.is_zero())
        return unrounded;

    std::expected<int, ArithmeticError> const exponent = checked_decimal_exponent(unrounded);
    if (!exponent)
        return std::unexpected { exponent.error() };

    // Keeping `significant` digits of a value whose leading digit sits at 10^e
    // means rounding at the 10^(e - significant + 1) place.
    long long const places = static_cast<long long>(significant.value) - 1 - static_cast<long long>(*exponent);
    if (places > 18 || places < -18)
        return std::unexpected { ArithmeticError::Overflow };

    return checked_round(unrounded, DecimalPlaces { static_cast<std::int32_t>(places) }, roundingMode);
}

/// Throwing spelling of `checked_round(Rational, SignificantDigits, RoundingMode)`.
[[nodiscard]] constexpr Rational round(Rational unrounded, SignificantDigits significant, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round(unrounded, significant, roundingMode));
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
///   representable: its denominator, a power of two, must stay below 2^127.
///   That limit is set by the value's magnitude. Measured: `0.0001`, a 53-bit
///   numerator over 2^66, converts; `1e-30`, over 2^147, does not.
/// - Rounding to a POSITIVE number of places `N` scales by `10^N`, cancelling
///   common factors of two against the denominator first, so what must fit in
///   `Rational::Int` is `|numerator| * (10^N / gcd(10^N, denominator))` -- for
///   a binary denominator, `|numerator| * 5^N`. There the limit is set by the
///   **numerator's** magnitude, not the denominator's and not the value's
///   size: `1 / 2^121` rounds at all 18 places, while a 100-bit numerator over
///   the same denominator does not. A `double` below 2^53 in magnitude has a
///   numerator of at most 53 bits, and rounding it forms at most
///   2^53 * 5^18 * 2^18, below 2^113, so it rounds at every place from 0 to
///   18. A whole `double` past that has a numerator of its own magnitude and
///   nothing to cancel: 1e21 is refused at 18 places, and 1e38 at 1.
/// - Rounding to a NEGATIVE number of places -- to whole tens or hundreds --
///   uses an integer step, which multiplies the **denominator** instead. There
///   the denominator is the constraint: `2^-100` is refused at -18 places.
///
/// Prefer `from_decimal` for an exact decimal; use this function only for a
/// genuinely measured `double`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rational_from_double(double floating,
                                                                                      DecimalPlaces places,
                                                                                      RoundingMode roundingMode) noexcept
{
    std::expected<Rational, ArithmeticError> const exactValue = Rational::from_double_exact(floating);
    if (!exactValue)
        return exactValue;
    return checked_round(*exactValue, places, roundingMode);
}

} // namespace formula
