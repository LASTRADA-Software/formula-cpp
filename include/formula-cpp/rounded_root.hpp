// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A square root rounded exactly to a declared granularity.
///
/// Under `Rational`, `sqrt(x)` answers only when the root is itself rational
/// (`checked_exact_nth_root`), so the square root of almost every variance a
/// laboratory measures is `ArithmeticError::Inexact`. Rounding afterwards
/// cannot help: the root fails before any rounding node sees it. Yet a
/// standard deviation reported "to 0.01 g" is a perfectly exact number -- the
/// decimal that the true, irrational root rounds to -- and this header computes
/// that number, without ever holding an approximation of the root.
///
/// `rounded_sqrt<U, Places, Mode>(radicand)` is one fused node rather than a
/// `RoundNode` around a `RootNode`, deliberately:
///
///  - Its trace step shows the radicand, which is exact, and the rounded
///    result, which is exact. No step ever shows an irrational number as if it
///    were one.
///  - `rounded<...>(sqrt(x))` keeps meaning what it says, an exact root that
///    fails when there is none. Overloading `RoundNode` to switch algorithms
///    underneath it would switch a method's rounding rule too, since
///    `RoundingRuleNode` derives from `RoundNode`.
///
/// The rounding happens in `U`, never in the coherent unit: the radicand is
/// converted into `U` squared first, exactly as a `RoundNode` converts into its
/// own unit. "To 2 dp of g" of a mass whose variance is stored in kg^2 is not
/// "to 2 dp of kg".

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// Fails to compile when the unit a rounded square root is stated in does
    /// not measure the square root of its radicand's dimension: the root of a
    /// variance in g^2 is a mass, and rounding it "to 0.01 mm" is a category
    /// error, not a rounding one.
    template <Unit U, typename Radicand>
    struct RequireRootUnitMatches
    {
        static_assert(U.dimension * U.dimension == Radicand::dimension,
                      "formula: this rounded_sqrt names a unit whose square does not measure the dimension of "
                      "its radicand; the unit and the radicand appear in this diagnostic as the template "
                      "arguments of RequireRootUnitMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when the unit a rounded square root is stated in has an
    /// offset, as degrees Celsius has. The root of a squared quantity is a
    /// magnitude -- a spread, not a reading on a scale -- and a unit with an
    /// offset states readings: every later reader converts the node's value
    /// through that offset, so a spread of 2 K would read as -271.15 degC.
    /// Name the offset-free unit of the same dimension instead, kelvin for a
    /// temperature.
    ///
    /// Asked only when @p DimensionMatches, so that a unit that is wrong in
    /// both ways draws `RequireRootUnitMatches`'s one message and not this one
    /// as well: fixing the dimension is the first change either way.
    template <Unit U, bool DimensionMatches>
    struct RequireRootUnitWithoutOffset
    {
        static_assert(!DimensionMatches || U.offsetNumerator == 0,
                      "formula: this rounded_sqrt names a unit with an offset, such as degrees Celsius; the root "
                      "of a squared quantity is a spread, which an offset unit would misread as a point on its "
                      "scale -- name the offset-free unit of the same dimension, such as kelvin");

        static constexpr bool value = true;
    };

    /// @p leftFactor times @p rightFactor, or nothing when the product leaves
    /// 128 bits.
    [[nodiscard]] constexpr std::optional<UInt128> mul_unsigned_or_none(UInt128 leftFactor, UInt128 rightFactor) noexcept
    {
        std::optional<UInt128> const product = u128_mul_checked(leftFactor, rightFactor);
        if (product)
            FORMULA_CENSUS_NOTE(Unsigned, *product);
        return product;
    }

    /// 10 to the @p exponent, for a non-negative @p exponent, or nothing when
    /// it leaves 128 bits.
    [[nodiscard]] constexpr std::optional<UInt128> unsigned_pow10(std::int64_t exponent) noexcept
    {
        return exponent < 0 || exponent > 38 ? std::nullopt : u128_pow10(static_cast<int>(exponent));
    }

    /// The square root of @p radicandInUnitSquared, rounded to @p places
    /// decimal places under @p roundingMode -- the correctly rounded decimal of the
    /// true root, whether that root is rational or not.
    ///
    /// The one implementation behind `RoundedRootNode`. The radicand is
    /// already in the square of the unit the rounding is stated in, and the
    /// answer is in that unit.
    ///
    /// **A rational root is the only place a tie can occur**, so it is handed
    /// to `checked_round`, which decides ties for every other rounding node.
    /// Otherwise the root is irrational, and with v = a/b and S = 10^(2p):
    ///
    ///  - v * S is split into q + r/B, a whole part and a remainder, so that
    ///    a * S never has to fit;
    ///  - f = isqrt(q), so that f <= root(v) * 10^p < f + 1, strictly on the
    ///    right because the root is irrational;
    ///  - `Floor` and `TowardZero` give f, and `Ceiling` and `AwayFromZero`
    ///    give f + 1 (the root is not negative, so the directed modes pair up);
    ///  - the three half modes ask whether root(v * S) > f + 1/2, that is
    ///    v * S > f^2 + f + 1/4, which is exactly
    ///    `q > f^2 + f || (q == f^2 + f && 4r > B)`. Equality would make the
    ///    root rational, so it never occurs here, and the three half modes
    ///    agree.
    ///
    /// `4r > B` is computed as `r > B / 4`, which is the same comparison for
    /// integers and cannot overflow.
    ///
    /// A negative @p places rounds to whole tens, hundreds and so on, as it
    /// does for `checked_round`: then S is 1/10^(2|p|), and it is B that grows
    /// instead of q.
    ///
    /// Every intermediate is a 128-bit unsigned integer (`detail::UInt128`).
    /// So the headroom is `floor(v) * 10^(2p) < 2^128` (and
    /// `b * 10^(2p) < 2^128` for the remainder): an integer radicand of about
    /// 10^6 fits at 16 places and overflows at 17. **The bound is on the
    /// denominator b too**, whatever the value: at p places a denominator above
    /// about 3.4 * 10^(38 - 2|p|) overflows, at a negative p because B is
    /// b * 10^(2|p|), even where the rounded answer itself would fit. Beyond
    /// the headroom the answer is `ArithmeticError::Overflow`, never a wrapped
    /// or clamped value.
    ///
    /// @return the rounded root; `DomainError` for a negative radicand, which
    ///         has no real root; `Overflow` beyond the headroom above.
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_square_root(Rational radicandInUnitSquared,
                                                                                         DecimalPlaces places,
                                                                                         RoundingMode roundingMode) noexcept
    {
        if (radicandInUnitSquared.sign() < 0)
            return std::unexpected { ArithmeticError::DomainError };
        // As `checked_round` reports them, whichever path the root takes.
        if (places.value > 18 || places.value < -18)
            return std::unexpected { ArithmeticError::Overflow };

        std::expected<Rational, ArithmeticError> const exactRoot = checked_exact_nth_root(radicandInUnitSquared, 2);
        if (exactRoot.has_value())
            return checked_round(*exactRoot, places, roundingMode);
        if (exactRoot.error() != ArithmeticError::Inexact)
            return std::unexpected { exactRoot.error() };

        UInt128 const wholeNumerator = wide_magnitude(radicandInUnitSquared.numerator());
        UInt128 const wholeDenominator = wide_magnitude(radicandInUnitSquared.denominator());
        auto const doubledPlaces = std::int64_t { 2 } * places.value;

        // v * S = wholePart + leftover / divisor.
        UInt128 wholePart {};
        UInt128 leftover {};
        UInt128 divisor = wholeDenominator;
        if (doubledPlaces >= 0)
        {
            std::optional<UInt128> const powerOfTen = unsigned_pow10(doubledPlaces);
            if (!powerOfTen)
                return std::unexpected { ArithmeticError::Overflow };
            UInt128Division const split = u128_divmod(wholeNumerator, wholeDenominator);
            std::optional<UInt128> const scaledWhole = mul_unsigned_or_none(split.quotient, *powerOfTen);
            // Below wholeDenominator * scale, so it fits whenever that does.
            std::optional<UInt128> const scaledPart = mul_unsigned_or_none(split.remainder, *powerOfTen);
            if (!scaledWhole || !scaledPart)
                return std::unexpected { ArithmeticError::Overflow };
            UInt128Division const partSplit = u128_divmod(*scaledPart, wholeDenominator);
            wholePart = u128_add(*scaledWhole, partSplit.quotient);
            if (wholePart < *scaledWhole)
                return std::unexpected { ArithmeticError::Overflow };
            FORMULA_CENSUS_NOTE(Unsigned, wholePart);
            leftover = partSplit.remainder;
        }
        else
        {
            std::optional<UInt128> const shrink = unsigned_pow10(-doubledPlaces);
            std::optional<UInt128> const widened = shrink ? mul_unsigned_or_none(wholeDenominator, *shrink) : std::nullopt;
            if (!widened)
                return std::unexpected { ArithmeticError::Overflow };
            divisor = *widened;
            UInt128Division const split = u128_divmod(wholeNumerator, divisor);
            wholePart = split.quotient;
            leftover = split.remainder;
        }

        std::uint64_t const floorDigits = u128_isqrt(wholePart);
        // f^2 + f is below (f + 1)^2 <= 2^128, so it fits.
        UInt128 const halfwayWhole = u128_add(u128_mul_words(floorDigits, floorDigits), UInt128::from_u64(floorDigits));
        bool const aboveHalfway =
            halfwayWhole < wholePart
            || (wholePart == halfwayWhole && u128_divmod(divisor, UInt128::from_u64(4)).quotient < leftover);

        UInt128 keptDigits = UInt128::from_u64(floorDigits);
        UInt128 const raisedDigits = u128_add(keptDigits, UInt128::from_u64(1));
        switch (roundingMode)
        {
            case RoundingMode::Floor:
            case RoundingMode::TowardZero:
                break;
            case RoundingMode::Ceiling:
            case RoundingMode::AwayFromZero:
                keptDigits = raisedDigits;
                break;
            case RoundingMode::HalfAwayFromZero:
            case RoundingMode::HalfTowardZero:
            case RoundingMode::HalfEven:
                keptDigits = aboveHalfway ? raisedDigits : keptDigits;
                break;
        }

        // At most 2^64, which `Rational::Int` holds.
        return Rational::from_decimal(signed_from_magnitude(keptDigits, false), -places.value);
    }

    /// The square root of @p radicandInSi, a value in the coherent unit of
    /// `unit`'s dimension squared, rounded in @p unit and returned in the
    /// coherent unit of `unit`'s dimension.
    ///
    /// The conversions are `checked_convert`'s, into and out of `unit`'s factor
    /// and its square -- never a ratio written out here. `unit` has no offset:
    /// `RoundedRootNode` refuses one (`RequireRootUnitWithoutOffset`), so the
    /// scale built here is `unit` itself as far as any conversion can tell.
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_square_root_in(
        Rational radicandInSi, Unit unit, DecimalPlaces places, RoundingMode roundingMode) noexcept
    {
        std::expected<Rational, ArithmeticError> const unitFactor =
            Rational::make(unit.magnitudeNumerator, unit.magnitudeDenominator);
        if (!unitFactor)
            return unitFactor;
        std::expected<Rational, ArithmeticError> const factorSquared = checked_mul(*unitFactor, *unitFactor);
        if (!factorSquared)
            return factorSquared;

        // A `Unit` holds its magnitude in 64 bits: a factor or a square wider
        // than that is beyond any scale this can build.
        std::optional<std::int64_t> const factorTop = narrow_to_int64(unitFactor->numerator());
        std::optional<std::int64_t> const factorBottom = narrow_to_int64(unitFactor->denominator());
        std::optional<std::int64_t> const squaredTop = narrow_to_int64(factorSquared->numerator());
        std::optional<std::int64_t> const squaredBottom = narrow_to_int64(factorSquared->denominator());
        if (!factorTop || !factorBottom || !squaredTop || !squaredBottom)
            return std::unexpected { ArithmeticError::Overflow };

        Unit const unitScale { .dimension = unit.dimension,
                               .magnitudeNumerator = *factorTop,
                               .magnitudeDenominator = *factorBottom };
        Unit const scaleSquared { .dimension = unit.dimension * unit.dimension,
                                  .magnitudeNumerator = *squaredTop,
                                  .magnitudeDenominator = *squaredBottom };

        std::expected<Rational, ArithmeticError> const inUnitSquared =
            checked_convert(radicandInSi, coherent(scaleSquared.dimension), scaleSquared);
        if (!inUnitSquared)
            return inUnitSquared;
        std::expected<Rational, ArithmeticError> const roundedRoot =
            rounded_square_root(*inUnitSquared, places, roundingMode);
        if (!roundedRoot)
            return roundedRoot;
        return checked_convert(*roundedRoot, unitScale, coherent(unitScale.dimension));
    }
} // namespace detail

/// The square root of @p Radicand, rounded to @p Places decimal places of
/// @p U under @p Mode -- exactly, whether the root is rational or not.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
struct RoundedRootNode: NodeBase
{
    static_assert(detail::RequireRootUnitMatches<U, Radicand>::value);
    static_assert(detail::RequireRootUnitWithoutOffset<U, U.dimension * U.dimension == Radicand::dimension>::value);
    static_assert(detail::RequireNamedScaledScalar<U>::value);
    static_assert(detail::RequireAsciiKey<U>::value);

    /// The expression whose square root is taken: a variance, a mean square,
    /// a sum of squared uncertainties.
    ///
    /// Deliberately no `{}` default member initialiser: with one, a method
    /// holding a lookup under this member fails to compile on clang++,
    /// clang-cl or g++, and cl answers the trait wrongly -- see `Corrections`
    /// (`lookup.hpp`).
    Radicand radicand;

    /// The unit the rounding happens in, and which the root is a value of.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to go. The three half modes differ only on a tie, and a tie
    /// needs a rational root.
    static constexpr RoundingMode mode = Mode;
    /// The square root of the radicand's dimension, which `U` measures.
    static constexpr Dimension dimension = U.dimension;
};

/// The square root of `radicand`, rounded exactly to `Places` decimal places
/// of `U`:
/// `rounded_sqrt<unit::Gram, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero>(var<Variance>)`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
[[nodiscard]] constexpr auto rounded_sqrt(Radicand radicand) noexcept
{
    return RoundedRootNode<U, Places, Mode, Radicand> { {}, radicand };
}

/// The square root of `radicand`, rounded as @p R names:
/// `rounded_sqrt<hundredthGram>(var<Variance>)`.
template <DecimalRounding R, Node Radicand>
[[nodiscard]] constexpr auto rounded_sqrt(Radicand radicand) noexcept
{
    return rounded_sqrt<R.unit, R.places, R.mode>(radicand);
}

/// Evaluates the radicand, then rounds its square root in `U`.
///
/// Under `Rational` this is `detail::rounded_square_root`, exact. Under any
/// other representation it is that representation's own root
/// (`RepFunctions<Rep>::root`, `std::pow` for `double`) followed by its own
/// rounding (`RepRounding<Rep>::round_in`). For `double` that rounding refuses
/// to compile, as it does for every rounding node, and for the reason
/// `RepRounding<double>` gives: a decimal granularity is exactly what binary
/// floating point cannot honour.
template <typename Rep = Rational,
          Unit U,
          DecimalPlaces Places,
          RoundingMode Mode,
          Node Radicand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundedRootNode<U, Places, Mode, Radicand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedRadicand = detail::dispatch<Rep>(node.radicand, environment, sink);
    if (!evaluatedRadicand.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedRadicand.error());
    }
    if (!evaluatedRadicand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> roundedRoot = std::unexpected { ArithmeticError::DomainError };
    if constexpr (std::is_same_v<Rep, Rational>)
        roundedRoot = detail::rounded_square_root_in(**evaluatedRadicand, U, Places, Mode);
    else
    {
        std::expected<Rep, ArithmeticError> const rooted = RepFunctions<Rep>::root(**evaluatedRadicand, 2);
        roundedRoot = rooted.has_value() ? RepRounding<Rep>::round_in(*rooted, U, Places, Mode)
                                         : std::expected<Rep, ArithmeticError> { std::unexpected { rooted.error() } };
    }
    Evaluated<Rep> const evaluated = roundedRoot.has_value() ? detail::present<Rep>(*roundedRoot)
                                                             : Evaluated<Rep> { std::unexpected { roundedRoot.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

} // namespace formula
