// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The decimal a wide fraction rounds to, exactly: the rounding `rounded_output`
/// states for a value `Rational` cannot hold (`opaque.hpp`), and the rule for a
/// value known only between two bounds.
///
/// `round_wide_ratio` is built to agree with `checked_round`: for a value
/// `checked_round` accepts, it gives the same result in all seven modes at every
/// place from -38 to 38 (`MaximumDecimalPlaces`), in a width that holds any
/// `Rational::Int` times 10^38 -- 256 bits, `WideRatio<8>`
/// (`test/wide_rounding_tests.cpp` checks 95498 such cases). A narrower width
/// refuses with `Overflow` where the scaled value leaves it. It also answers
/// some values `checked_round` refuses -- where a numerator times 10^places
/// leaves 128 bits but the rounded result fits -- and refuses, with
/// `Overflow`, a result that does not fit `Rational`, as `checked_round` does.
///
/// Every fraction is reduced before it is scaled, so an operation may hand over
/// one far from lowest terms: the width only has to hold the value, not the way
/// it was reached.
///
/// Integer arithmetic only (`detail/wide_int.hpp`): no floating point, no
/// intrinsic, no compiler 128-bit integer (`__int128`).

#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace formula::detail
{

/// @p unreduced in lowest terms, its sign kept; zero as zero over one, never
/// negative. @pre the denominator is not zero.
template <std::size_t L>
[[nodiscard]] constexpr WideRatio<L> reduced(WideRatio<L> const& unreduced) noexcept
{
    if (unreduced.numerator.is_zero())
        return WideRatio<L> { .negative = false,
                              .numerator = WideUnsigned<L> {},
                              .denominator = WideUnsigned<L>::from_u64(1) };
    WideUnsigned<L> const common = gcd(unreduced.numerator, unreduced.denominator);
    return WideRatio<L> { unreduced.negative,
                          divmod(unreduced.numerator, common).quotient,
                          divmod(unreduced.denominator, common).quotient };
}

/// @p exact as a wide fraction. Four limbs at least, so that a 128-bit
/// numerator fits.
template <std::size_t L>
    requires(L >= 4)
[[nodiscard]] constexpr WideRatio<L> wide_from_rational(Rational exact) noexcept
{
    return WideRatio<L> { exact.numerator() < 0,
                          WideUnsigned<L>::from_u128(wide_magnitude(exact.numerator())),
                          WideUnsigned<L>::from_u128(wide_magnitude(exact.denominator())) };
}

/// @p exact times @p commonDenominator, an integer; nothing when it does not
/// fit. @pre `exact.denominator()` divides @p commonDenominator.
template <std::size_t L>
    requires(L >= 4)
[[nodiscard]] constexpr std::optional<WideSigned<L>> scaled_to_denominator(Rational exact,
                                                                           WideUnsigned<L> const& commonDenominator) noexcept
{
    WideUnsigned<L> const cofactor =
        divmod(commonDenominator, WideUnsigned<L>::from_u128(wide_magnitude(exact.denominator()))).quotient;
    std::optional<WideUnsigned<L>> const scaledMagnitude =
        mul_checked_or_none(WideUnsigned<L>::from_u128(wide_magnitude(exact.numerator())), cofactor);
    if (!scaledMagnitude)
        return std::nullopt;
    return WideSigned<L> { exact.numerator() < 0, *scaledMagnitude };
}

/// The decimal @p unrounded rounds to at @p places under @p roundingMode, as
/// `Rational::from_decimal(kept, -places)`. `Overflow` for places outside
/// -`MaximumDecimalPlaces` to `MaximumDecimalPlaces`, or a kept integer that
/// does not fit `Rational::Int`, as `checked_round` reports them, and where
/// the value scaled by 10^places leaves `L` limbs; `DivisionByZero` for a
/// zero denominator.
///
/// The magnitude is rounded and the sign applied afterwards: the directed
/// modes read the sign (Floor rounds a negative magnitude up, Ceiling a
/// positive one), the pairs `checked_round` reaches by flooring the signed
/// value.
template <std::size_t L>
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> round_wide_ratio(WideRatio<L> const& unrounded,
                                                                                  DecimalPlaces places,
                                                                                  RoundingMode roundingMode) noexcept
{
    if (places.value > MaximumDecimalPlaces || places.value < -MaximumDecimalPlaces)
        return std::unexpected { ArithmeticError::Overflow };
    if (unrounded.denominator.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    WideRatio<L> const inLowestTerms = reduced(unrounded);
    auto const placesMagnitude = static_cast<std::size_t>(places.value < 0 ? -places.value : places.value);
    std::optional<WideUnsigned<L>> const powerOfTen = pow10<L>(placesMagnitude);
    if (!powerOfTen)
        return std::unexpected { ArithmeticError::Overflow };
    // v * 10^places, as a numerator over a denominator: a positive places
    // scales the numerator, a negative one the denominator.
    std::optional<WideUnsigned<L>> const scaledNumerator = places.value >= 0
                                                               ? mul_checked_or_none(inLowestTerms.numerator, *powerOfTen)
                                                               : std::optional<WideUnsigned<L>> { inLowestTerms.numerator };
    std::optional<WideUnsigned<L>> const scaledDenominator =
        places.value < 0 ? mul_checked_or_none(inLowestTerms.denominator, *powerOfTen)
                         : std::optional<WideUnsigned<L>> { inLowestTerms.denominator };
    if (!scaledNumerator || !scaledDenominator)
        return std::unexpected { ArithmeticError::Overflow };

    WideDivision<L> const split = divmod(*scaledNumerator, *scaledDenominator);
    bool awayFromZero = false; // one step up in magnitude
    if (!split.remainder.is_zero())
    {
        // Which side of the half: remainder against denominator - remainder, never doubled.
        std::strong_ordering const side = split.remainder <=> *sub_checked_or_none(*scaledDenominator, split.remainder);
        switch (roundingMode)
        {
            case RoundingMode::TowardZero:
                awayFromZero = false;
                break;
            case RoundingMode::AwayFromZero:
                awayFromZero = true;
                break;
            case RoundingMode::Floor:
                awayFromZero = inLowestTerms.negative;
                break;
            case RoundingMode::Ceiling:
                awayFromZero = !inLowestTerms.negative;
                break;
            case RoundingMode::HalfAwayFromZero:
                awayFromZero = side != std::strong_ordering::less;
                break;
            case RoundingMode::HalfTowardZero:
                awayFromZero = side == std::strong_ordering::greater;
                break;
            case RoundingMode::HalfEven:
                awayFromZero = side == std::strong_ordering::greater
                               || (side == std::strong_ordering::equal && (split.quotient.limb(0) & 1U) != 0);
                break;
        }
    }
    std::optional<WideUnsigned<L>> const kept =
        awayFromZero ? add_small_checked_or_none(split.quotient, 1U) : split.quotient;
    std::optional<UInt128> const keptMagnitude = kept ? kept->to_u128() : std::nullopt;
    std::optional<Rational::Int> const mantissa =
        keptMagnitude ? rational_int_from_magnitude(*keptMagnitude, inLowestTerms.negative) : std::nullopt;
    if (!mantissa)
        return std::unexpected { ArithmeticError::Overflow };
    return Rational::from_decimal(*mantissa, -places.value);
}

/// @p unreduced as the `Rational` of the same value: reduced first, then
/// `Overflow` when its numerator or denominator does not fit `Rational::Int`,
/// `DivisionByZero` for a zero denominator.
template <std::size_t L>
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> narrow_wide_ratio(WideRatio<L> const& unreduced) noexcept
{
    if (unreduced.denominator.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    WideRatio<L> const inLowestTerms = reduced(unreduced);
    std::optional<UInt128> const numeratorMagnitude = inLowestTerms.numerator.to_u128();
    std::optional<UInt128> const denominatorMagnitude = inLowestTerms.denominator.to_u128();
    std::optional<Rational::Int> const signedNumerator =
        numeratorMagnitude ? rational_int_from_magnitude(*numeratorMagnitude, inLowestTerms.negative) : std::nullopt;
    std::optional<Rational::Int> const positiveDenominator =
        denominatorMagnitude ? rational_int_from_magnitude(*denominatorMagnitude, false) : std::nullopt;
    if (!signedNumerator || !positiveDenominator)
        return std::unexpected { ArithmeticError::Overflow };
    return Rational::make(*signedNumerator, *positiveDenominator);
}

/// @p coherentValue, a value in the coherent unit of @p roundedIn's
/// dimension, rounded to @p places of @p roundedIn under @p roundingMode, and
/// answered in the coherent unit again: what `RepRounding<Rational>::round_in`
/// does for a `Rational` (`rounding_node.hpp`), exactly, for a wider value.
/// The conversion into @p roundedIn is a multiplication by the unit's
/// magnitude, inverted; the conversion back is `checked_convert`'s.
///
/// A unit with an offset is `DomainError` here: `rounded_output` refuses one
/// at compile time, and this is the run-time backstop. A zero magnitude
/// numerator is `DomainError` and a zero denominator `DivisionByZero`, as
/// `checked_convert` answers them.
template <std::size_t L>
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_in_unit(WideRatio<L> const& coherentValue,
                                                                                 Unit const& roundedIn,
                                                                                 DecimalPlaces places,
                                                                                 RoundingMode roundingMode) noexcept
{
    if (roundedIn.magnitudeNumerator == 0 || roundedIn.offsetNumerator != 0)
        return std::unexpected { ArithmeticError::DomainError };
    if (roundedIn.magnitudeDenominator == 0 || coherentValue.denominator.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    WideRatio<L> const inLowestTerms = reduced(coherentValue);
    bool const flipped = (roundedIn.magnitudeNumerator < 0) != (roundedIn.magnitudeDenominator < 0);
    std::optional<WideUnsigned<L>> const inUnitNumerator =
        mul_checked_or_none(inLowestTerms.numerator, WideUnsigned<L>::from_u64(magnitude(roundedIn.magnitudeDenominator)));
    std::optional<WideUnsigned<L>> const inUnitDenominator =
        mul_checked_or_none(inLowestTerms.denominator, WideUnsigned<L>::from_u64(magnitude(roundedIn.magnitudeNumerator)));
    if (!inUnitNumerator || !inUnitDenominator)
        return std::unexpected { ArithmeticError::Overflow };
    std::expected<Rational, ArithmeticError> const roundedThere = round_wide_ratio(
        WideRatio<L> {
            inLowestTerms.negative != flipped && !inUnitNumerator->is_zero(), *inUnitNumerator, *inUnitDenominator },
        places,
        roundingMode);
    if (!roundedThere)
        return roundedThere;
    return checked_convert(*roundedThere, roundedIn, Unit { .dimension = roundedIn.dimension });
}

/// The decimal a value known only to lie between @p lowerBound and
/// @p upperBound rounds to at @p places under @p roundingMode, when both
/// bounds round alike.
///
/// Rounding is monotone in every mode, so a value between two bounds that round
/// alike rounds as they do; bounds that round differently straddle a boundary
/// the working precision cannot place the value on, and the answer is
/// `Overflow`, never a guess. Nothing else is decided here: which bounds, and
/// how tight, is the caller's. An error from rounding either bound is
/// returned as it is.
template <std::size_t L>
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> decide_rounding(WideRatio<L> const& lowerBound,
                                                                                 WideRatio<L> const& upperBound,
                                                                                 DecimalPlaces places,
                                                                                 RoundingMode roundingMode) noexcept
{
    std::expected<Rational, ArithmeticError> const roundedLower = round_wide_ratio(lowerBound, places, roundingMode);
    if (!roundedLower)
        return roundedLower;
    std::expected<Rational, ArithmeticError> const roundedUpper = round_wide_ratio(upperBound, places, roundingMode);
    if (!roundedUpper)
        return roundedUpper;
    if (!(*roundedLower == *roundedUpper))
        return std::unexpected { ArithmeticError::Overflow };
    return roundedLower;
}

} // namespace formula::detail
