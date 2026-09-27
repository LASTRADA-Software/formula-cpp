// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Ordinary least squares: the straight line `y = a + b x` fitted to a curve's
/// points and values, as a named opaque operation (`opaque.hpp`) whose two
/// outputs, `intercept` and `slope`, are ordinary nodes.
///
/// A fit is not an expression tree: its coefficients come from sums over
/// every pair of points, which no node of this library states. So it is an
/// opaque operation, and the trace says its inside is not shown. What the
/// trace does show is what went in -- the curve, point by point -- and both
/// coefficients that came out.
///
/// **Exact in `Rational`**, and generic over `Rep` through `RepTraits`. It uses
/// the centred sums, `S_xx = sum (x - mean x)^2` and `S_xy = sum (x - mean
/// x)(y - mean y)`: phase 15's spike (step 3) measured them overflowing at
/// the same first size as the uncentred sums on every data shape it tried,
/// and at fewer sizes (60 against 63 of the 127 from 2 to 128 points, for
/// three-decimal readings of a few thousand). **Overflow depends on the data
/// far more than on the number of points**, and is not monotone in it: the
/// same shape of readings passed at 64 points and failed at 34. An
/// intermediate beyond `Rational`'s range returns `Overflow` -- never a wrong
/// number -- and `double` is the representation to fall back to, through
/// `compute` itself: a curve is evaluated only in `Rational` (`curve.hpp`).
///
/// **A domain with fewer than two distinct points** -- one point, or points
/// all equal -- has no line through it, and the fit returns its own
/// `DomainError`, never a slope of zero. A curve already refuses equal points
/// itself (`curve.hpp`), so through a curve that case is the curve's failure;
/// one point is the fit's.
///
/// Nothing else is offered: no line through the origin, no weights, no
/// residuals or coefficient of determination.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/rational.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace formula
{

/// Ordinary least squares over a curve: `y = intercept + slope x`, with `x`
/// the curve's points and `y` its values. `intercept` has the values'
/// dimension, and `slope` the values' over the points'.
struct LinearLeastSquares
{
    /// What the trace calls it.
    static constexpr std::string_view name = "linear least squares";
    /// One curve: its points, then its values.
    static constexpr std::array shapes { InputShape::Curve };
    /// The two coefficients, in this order.
    static constexpr std::array<std::string_view, 2> outputs { "intercept", "slope" };

    /// `intercept` in the values' dimension, `slope` in the values' over the
    /// points'. Any two dimensions are accepted.
    static consteval std::optional<std::array<Dimension, 2>> output_dimensions(
        std::array<Dimension, 2> pointsAndValues) noexcept
    {
        return std::array { pointsAndValues[1], pointsAndValues[1] / pointsAndValues[0] };
    }

    /// The fit, in coherent SI: @p points and @p pointValues, pair by pair.
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, ArithmeticError> compute(std::span<Rep const> points,
                                                                                std::span<Rep const> pointValues) noexcept
    {
        using Traits = RepTraits<Rep>;
        std::expected<Rep, ArithmeticError> const zero = Traits::from(Rational { 0 });
        std::expected<Rep, ArithmeticError> const pointCount =
            Traits::from(Rational { static_cast<std::int64_t>(points.size()) });
        if (!zero.has_value() || !pointCount.has_value())
            return std::unexpected { ArithmeticError::Overflow };

        // The means.
        Rep sumOfPoints = *zero;
        Rep sumOfValues = *zero;
        for (std::size_t at = 0; at < points.size(); ++at)
        {
            std::expected<Rep, ArithmeticError> const nextPoints = Traits::add(sumOfPoints, points[at]);
            if (!nextPoints.has_value())
                return std::unexpected { nextPoints.error() };
            sumOfPoints = *nextPoints;
            std::expected<Rep, ArithmeticError> const nextValues = Traits::add(sumOfValues, pointValues[at]);
            if (!nextValues.has_value())
                return std::unexpected { nextValues.error() };
            sumOfValues = *nextValues;
        }
        std::expected<Rep, ArithmeticError> const meanPoint = Traits::divide(sumOfPoints, *pointCount);
        if (!meanPoint.has_value())
            return std::unexpected { meanPoint.error() };
        std::expected<Rep, ArithmeticError> const meanValue = Traits::divide(sumOfValues, *pointCount);
        if (!meanValue.has_value())
            return std::unexpected { meanValue.error() };

        // The centred sums.
        Rep spreadOfPoints = *zero;
        Rep coSpread = *zero;
        for (std::size_t at = 0; at < points.size(); ++at)
        {
            std::expected<Rep, ArithmeticError> const pointOffset = Traits::subtract(points[at], *meanPoint);
            if (!pointOffset.has_value())
                return std::unexpected { pointOffset.error() };
            std::expected<Rep, ArithmeticError> const valueOffset = Traits::subtract(pointValues[at], *meanValue);
            if (!valueOffset.has_value())
                return std::unexpected { valueOffset.error() };
            std::expected<Rep, ArithmeticError> const squared = Traits::multiply(*pointOffset, *pointOffset);
            if (!squared.has_value())
                return std::unexpected { squared.error() };
            std::expected<Rep, ArithmeticError> const product = Traits::multiply(*pointOffset, *valueOffset);
            if (!product.has_value())
                return std::unexpected { product.error() };
            std::expected<Rep, ArithmeticError> const nextSpread = Traits::add(spreadOfPoints, *squared);
            if (!nextSpread.has_value())
                return std::unexpected { nextSpread.error() };
            spreadOfPoints = *nextSpread;
            std::expected<Rep, ArithmeticError> const nextCoSpread = Traits::add(coSpread, *product);
            if (!nextCoSpread.has_value())
                return std::unexpected { nextCoSpread.error() };
            coSpread = *nextCoSpread;
        }

        // Fewer than two distinct points: no line, and no slope of zero.
        if (spreadOfPoints == *zero)
            return std::unexpected { ArithmeticError::DomainError };

        std::expected<Rep, ArithmeticError> const slope = Traits::divide(coSpread, spreadOfPoints);
        if (!slope.has_value())
            return std::unexpected { slope.error() };
        std::expected<Rep, ArithmeticError> const rise = Traits::multiply(*slope, *meanPoint);
        if (!rise.has_value())
            return std::unexpected { rise.error() };
        std::expected<Rep, ArithmeticError> const intercept = Traits::subtract(*meanValue, *rise);
        if (!intercept.has_value())
            return std::unexpected { intercept.error() };
        return std::array { *intercept, *slope };
    }
};

/// A straight line fitted to @p fitted, for the reason @p citation gives:
/// `linear_least_squares(curve(series<Elapsed, 4>, series<Length, 4>), { ...
/// })`. Its outputs are `opaque_output<"intercept">` and
/// `opaque_output<"slope">`.
template <CurveExpression C>
[[nodiscard]] constexpr OpaqueCall<LinearLeastSquares, C> linear_least_squares(C fitted, Citation citation) noexcept
{
    return opaque<LinearLeastSquares>(citation, fitted);
}

namespace detail
{
    /// Fails to compile when `linear_least_squares` is given anything but a
    /// curve. Named so the arguments print.
    template <typename... Given>
    struct RequireFitOfCurve
    {
        static_assert(sizeof...(Given) == 0,
                      "formula: linear_least_squares fits a curve; pair the domain and the values with "
                      "curve(domain, values)");

        static constexpr bool value = true;
    };

    /// What a refused `linear_least_squares` returns: nothing anything can
    /// use, so nothing downstream refuses again.
    struct RefusedFit
    {
    };
} // namespace detail

/// Anything but a curve handed to `linear_least_squares`: refused in this
/// library's words.
template <typename NotCurve>
    requires(!CurveExpression<NotCurve>)
[[nodiscard]] constexpr detail::RefusedFit linear_least_squares(NotCurve, Citation) noexcept
{
    static_assert(detail::RequireFitOfCurve<NotCurve>::value);
    return {};
}

/// Two loose series handed to `linear_least_squares`: refused in this
/// library's words. A curve pairs the domain with its values, which two
/// series would have to re-derive.
template <typename Domain, typename Values>
[[nodiscard]] constexpr detail::RefusedFit linear_least_squares(Domain, Values, Citation) noexcept
{
    static_assert(detail::RequireFitOfCurve<Domain, Values>::value);
    return {};
}

} // namespace formula
