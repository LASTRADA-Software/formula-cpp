// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Ordinary least squares: the straight line `y = a + b x` fitted to a curve's
/// points and values, as a named opaque operation (`opaque.hpp`) whose two
/// outputs, `intercept` and `slope`, are ordinary nodes; or fitted through raw
/// observations (`observations.hpp`), paired row by row and as many as were
/// made, by a second operation that also reports `r squared`, the coefficient
/// of determination, and `points`, the number of observations it fitted.
///
/// A fit is not an expression tree: its coefficients come from sums, over the
/// points, of squares and products of each point's coordinates about their
/// means, which no node of this library states. So it is an
/// opaque operation, and the trace says its inside is not shown. What the
/// trace does show is what went in -- the curve, point by point -- and both
/// coefficients that came out.
///
/// **Exact in `Rational`**, and generic over `Rep` through `RepTraits`. It uses
/// the centred sums, `S_xx = sum (x - mean x)^2` and `S_xy = sum (x - mean
/// x)(y - mean y)`: phase 15's spike (step 3) measured them overflowing at
/// the same first size as the uncentred sums on every data shape it tried,
/// and at fewer sizes. `docs/numeric-headroom.md` ("Least squares") carries
/// the census, regenerated with every build: for three-decimal readings near
/// 2410 N, 57 of the 127 sizes from 2 to 128 points overflow, the first at
/// 34. **Overflow depends on the data far more than on the number of
/// points**, and is not monotone in it: the same shape of readings passes at
/// some sizes above 34 and fails at others. An
/// intermediate beyond `Rational`'s range returns `Overflow`, never a wrong
/// number.
///
/// **Rounded where it is used, it answers further.**
/// `rounded_output<"slope", U, Places, Mode>(fit)` runs `compute_exact`, the
/// same line from uncentred 256-bit integer sums, and reports the slope at the
/// precision the method declares: on those readings at every size from 2 to
/// 128 points. A different denominator on every point outgrows even that from
/// 58 points, and the answer is `Overflow`.
///
/// **In `double`, the fit is the consumer's own route, outside the
/// library.** A curve is evaluated only in `Rational` (`curve.hpp`), so a fit
/// is too, and `checked_evaluate_si<double>` on one is refused. What remains
/// is calling `LinearLeastSquares::compute<double>` directly, and that costs
/// everything the library otherwise does: it takes bare numbers, which the
/// caller must have converted to coherent units by hand, and returns bare
/// coefficients in coherent units -- a slope in metres per second, not in a
/// quantity's declared unit. Nothing checks their dimensions, and nothing is
/// traced, rendered or documented; the citation goes nowhere. Where the exact
/// fit overflows, `rounded_output` is the traced answer, not `double`.
///
/// **Fewer than two distinct points** -- none, one, or points all equal --
/// have no line through them, and the fit returns its own `DomainError`,
/// never a slope of zero. That is decided before any sum, by comparing the
/// points themselves, so it holds in every `Rep`: in `double`, three points
/// of 0.1 have a mean of 0.10000000000000002 and a spread that is rounding
/// noise, not zero. Spans of different lengths are a `DomainError` too, and
/// never a read past the shorter. A curve already refuses equal points
/// itself (`curve.hpp`), so through a curve that case is the curve's failure;
/// one point is the fit's.
///
/// **A line through raw observations** is decided by the curve fit's
/// pre-checks, and one more: the values are not all equal. It answers three
/// ways. Exactly through `opaque_output`, in the wide integers of
/// `detail/least_squares_kernel.hpp`, every output a `Rational` or all of them
/// `Overflow`. Correctly rounded through `rounded_output`, which rounds the
/// kernel's wide result and so answers where the exact route overflows --
/// within the kernel's width, and beyond it `Overflow`. Approximately in
/// `double` through `checked_evaluate_si<double>`, untraced. A flat response is its own `DomainError`, so an R² acceptance is
/// never passed by one.
///
/// Nothing else is offered: no line through the origin, no weights, no
/// residuals; R² only through raw observations, whose fit is a separate
/// operation with its own pinned lines.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/detail/least_squares_kernel.hpp>
#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/observations.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/rational.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

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

    /// The fit, in coherent units: @p points and @p pointValues, pair by pair.
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, ArithmeticError> compute(std::span<Rep const> points,
                                                                                std::span<Rep const> pointValues) noexcept
    {
        using Traits = RepTraits<Rep>;
        // Decided exactly, before any sum: see the file comment. No point, or
        // one, has no second point distinct from the first either.
        if (points.size() != pointValues.size())
            return std::unexpected { ArithmeticError::DomainError };
        bool anotherPoint = false;
        for (Rep const& each: points)
            if (!(each == points[0]))
                anotherPoint = true;
        if (!anotherPoint)
            return std::unexpected { ArithmeticError::DomainError };

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

        // A backstop only: distinct points were checked above.
        if (spreadOfPoints == *zero)
            return std::unexpected { ArithmeticError::DomainError };

        std::expected<Rep, ArithmeticError> const fittedSlope = Traits::divide(coSpread, spreadOfPoints);
        if (!fittedSlope.has_value())
            return std::unexpected { fittedSlope.error() };
        std::expected<Rep, ArithmeticError> const rise = Traits::multiply(*fittedSlope, *meanPoint);
        if (!rise.has_value())
            return std::unexpected { rise.error() };
        std::expected<Rep, ArithmeticError> const fittedIntercept = Traits::subtract(*meanValue, *rise);
        if (!fittedIntercept.has_value())
            return std::unexpected { fittedIntercept.error() };
        return std::array { *fittedIntercept, *fittedSlope };
    }

    /// The width `compute_exact` works in: eight limbs, 256 bits.
    static constexpr std::size_t exact_limbs = 8;

    /// The fit, exactly, for a `rounded_output`: intercept then slope, in
    /// coherent units, as fractions of `exact_limbs`-limb integers, not
    /// reduced. The same line as `compute`'s, from uncentred integer sums
    /// rather than centred ones: each series is first brought to one common
    /// denominator -- `X = x Dx`, `Y = y Dy` -- and then, with `n` points,
    ///
    ///     slope     = (n Sxy - Sx Sy) Dx / ((n Sxx - Sx^2) Dy)
    ///     intercept = (Sy Sxx - Sx Sxy)   / ((n Sxx - Sx^2) Dy)
    ///
    /// where `Sx` is the sum of `X`, `Sxy` of `X Y`, and so on. Nothing is
    /// rounded here; `rounded_output` rounds the one output it is asked for.
    ///
    /// The same refusals as `compute`, decided the same way: spans of
    /// different lengths, and fewer than two distinct points, are the fit's own
    /// `DomainError`. A common denominator, a sum or a product that leaves 256
    /// bits is `Overflow` -- on readings with a different denominator on every
    /// point from 58 points (`docs/numeric-headroom.md`), never a wrong line.
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 2>, ArithmeticError> compute_exact(
        std::span<Rational const> domainPoints, std::span<Rational const> pointValues) noexcept
    {
        using Wide = detail::WideUnsigned<exact_limbs>;
        using Signed = detail::WideSigned<exact_limbs>;
        if (domainPoints.size() != pointValues.size())
            return std::unexpected { ArithmeticError::DomainError };
        bool anotherPoint = false;
        for (Rational const& each: domainPoints)
            if (!(each == domainPoints[0]))
                anotherPoint = true;
        if (!anotherPoint)
            return std::unexpected { ArithmeticError::DomainError };

        // One common denominator for the points, and one for the values.
        std::optional<Wide> pointScale = Wide::from_u64(1);
        std::optional<Wide> valueScale = Wide::from_u64(1);
        for (std::size_t at = 0; at < domainPoints.size(); ++at)
        {
            if (pointScale)
                pointScale = detail::lcm_checked_or_none(
                    *pointScale, Wide::from_u64(static_cast<std::uint64_t>(domainPoints[at].denominator())));
            if (valueScale)
                valueScale = detail::lcm_checked_or_none(
                    *valueScale, Wide::from_u64(static_cast<std::uint64_t>(pointValues[at].denominator())));
        }
        if (!pointScale || !valueScale)
            return std::unexpected { ArithmeticError::Overflow };

        // The four integer sums: of X, of Y, of X^2 and of X Y.
        Signed sumOfPoints {};
        Signed sumOfValues {};
        Signed sumOfSquares {};
        Signed sumOfProducts {};
        for (std::size_t at = 0; at < domainPoints.size(); ++at)
        {
            std::optional<Signed> const scaledPoint = detail::scaled_to_denominator(domainPoints[at], *pointScale);
            std::optional<Signed> const scaledValue = detail::scaled_to_denominator(pointValues[at], *valueScale);
            if (!scaledPoint || !scaledValue)
                return std::unexpected { ArithmeticError::Overflow };
            std::optional<Signed> const squareTerm = detail::mul_checked_or_none(*scaledPoint, *scaledPoint);
            std::optional<Signed> const productTerm = detail::mul_checked_or_none(*scaledPoint, *scaledValue);
            std::optional<Signed> const withPoint = detail::add_checked_or_none(sumOfPoints, *scaledPoint);
            std::optional<Signed> const withValue = detail::add_checked_or_none(sumOfValues, *scaledValue);
            std::optional<Signed> const withSquare =
                squareTerm ? detail::add_checked_or_none(sumOfSquares, *squareTerm) : std::nullopt;
            std::optional<Signed> const withProduct =
                productTerm ? detail::add_checked_or_none(sumOfProducts, *productTerm) : std::nullopt;
            if (!withPoint || !withValue || !withSquare || !withProduct)
                return std::unexpected { ArithmeticError::Overflow };
            sumOfPoints = *withPoint;
            sumOfValues = *withValue;
            sumOfSquares = *withSquare;
            sumOfProducts = *withProduct;
        }

        // n Sxx - Sx^2, n Sxy - Sx Sy and Sy Sxx - Sx Sxy.
        Signed const pointCount { false, Wide::from_u64(domainPoints.size()) };
        std::optional<Signed> const countedSquares = detail::mul_checked_or_none(pointCount, sumOfSquares);
        std::optional<Signed> const squaredSum = detail::mul_checked_or_none(sumOfPoints, sumOfPoints);
        std::optional<Signed> const countedProducts = detail::mul_checked_or_none(pointCount, sumOfProducts);
        std::optional<Signed> const crossSum = detail::mul_checked_or_none(sumOfPoints, sumOfValues);
        std::optional<Signed> const valuesBySquares = detail::mul_checked_or_none(sumOfValues, sumOfSquares);
        std::optional<Signed> const pointsByProducts = detail::mul_checked_or_none(sumOfPoints, sumOfProducts);
        if (!countedSquares || !squaredSum || !countedProducts || !crossSum || !valuesBySquares || !pointsByProducts)
            return std::unexpected { ArithmeticError::Overflow };
        std::optional<Signed> const pointSpread = detail::sub_checked_or_none(*countedSquares, *squaredSum);
        std::optional<Signed> const riseTerm = detail::sub_checked_or_none(*countedProducts, *crossSum);
        std::optional<Signed> const interceptTerm = detail::sub_checked_or_none(*valuesBySquares, *pointsByProducts);
        if (!pointSpread || !riseTerm || !interceptTerm)
            return std::unexpected { ArithmeticError::Overflow };
        // A backstop only: distinct points were checked above.
        if (pointSpread->negative || pointSpread->magnitude.is_zero())
            return std::unexpected { ArithmeticError::DomainError };

        std::optional<Wide> const slopeNumerator = detail::mul_checked_or_none(riseTerm->magnitude, *pointScale);
        std::optional<Wide> const sharedDenominator = detail::mul_checked_or_none(pointSpread->magnitude, *valueScale);
        if (!slopeNumerator || !sharedDenominator)
            return std::unexpected { ArithmeticError::Overflow };
        return std::array {
            detail::WideRatio<exact_limbs> { interceptTerm->negative, interceptTerm->magnitude, *sharedDenominator },
            detail::WideRatio<exact_limbs> { riseTerm->negative, *slopeNumerator, *sharedDenominator }
        };
    }
};

/// Ordinary least squares over raw observations: `y = intercept + slope x`,
/// with `x` the first input's observations and `y` the second's, paired row
/// by row -- observation i of each belongs to row i, so both are built from
/// the same rows. How many there are is data. Besides the coefficients it
/// reports `r squared`, the coefficient of determination, S_xy^2 / (S_xx
/// S_yy), and `points`, the number of observations it fitted, exact in
/// every representation -- for degrees of freedom `n - 2`, say.
///
/// Decided exactly, on the observations themselves and before any sum, in
/// every `Rep`, each the fit's own `DomainError`: both inputs hold as many
/// observations, at least two, the points are not all equal, and the values
/// are not all equal -- R^2 would be 0/0, so a flat response never passes an
/// R^2 acceptance.
///
/// Three routes. `compute_exact`, which `rounded_output` rounds, is the exact
/// kernel (`detail/least_squares_kernel.hpp`) in wide integers.
/// `compute<Rational>`, behind `opaque_output`, is the same kernel with every
/// output narrowed to a `Rational`: all four answer, or all fail with
/// `Overflow` when one does not fit. `compute<double>`, behind
/// `checked_evaluate_si<double>`, is the kernel's approximate route, untraced.
struct LinearLeastSquaresOfObservations
{
    /// What the trace calls it: the curve fit's name, since it fits the same line.
    static constexpr std::string_view name = "linear least squares";
    /// Two sets of raw observations: the points, then the values.
    static constexpr std::array shapes { InputShape::Observations, InputShape::Observations };
    /// The outputs, in this order.
    static constexpr std::array<std::string_view, 4> outputs { "intercept", "slope", "r squared", "points" };
    /// The exact kernel's width for one regressor.
    static constexpr std::size_t exact_limbs = detail::regression_limbs(1);

    /// `intercept` in the values' dimension, `slope` in the values' over the
    /// points', `r squared` and `points` bare numbers. Any two dimensions are
    /// accepted.
    static consteval std::optional<std::array<Dimension, 4>> output_dimensions(
        std::array<Dimension, 2> pointsAndValues) noexcept
    {
        return std::array { pointsAndValues[1], pointsAndValues[1] / pointsAndValues[0], dim::Scalar, dim::Scalar };
    }

    /// The fit, exactly, as wide fractions in coherent units.
    static constexpr std::expected<std::array<detail::WideRatio<exact_limbs>, 4>, ArithmeticError> compute_exact(
        std::span<Rational const> pointObservations, std::span<Rational const> valueObservations) noexcept
    {
        return detail::exact_regression<1>(std::array { pointObservations }, valueObservations);
    }

    /// The fit in coherent units: exactly, every output a `Rational` or all
    /// `Overflow`, for `Rep = Rational`; approximately otherwise.
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 4>, ArithmeticError> compute(
        std::span<Rep const> pointObservations, std::span<Rep const> valueObservations) noexcept
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            std::expected<std::array<detail::WideRatio<exact_limbs>, 4>, ArithmeticError> const exact =
                compute_exact(pointObservations, valueObservations);
            if (!exact.has_value())
                return std::unexpected { exact.error() };
            return detail::narrowed_all(*exact);
        }
        else
            return detail::approximate_regression<Rep, 1>(std::array { pointObservations }, valueObservations);
    }
};

/// A straight line fitted to @p fitted, for the reason @p citation gives:
/// `linear_least_squares(curve(series<Elapsed, 4>, series<Length, 4>), { ...
/// })`. Its outputs are `opaque_output<"intercept">` and
/// `opaque_output<"slope">`.
///
/// The citation is required and has no default, as `opaque()`'s has not;
/// `{}` states that the method gives none, and is shown as
/// `(no citation given)`.
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
                      "formula: linear_least_squares fits a curve, or points and values read as observations; pair a "
                      "domain series and a value series with curve(domain, values), or read both with "
                      "observations<Q, Capacity>");

        static constexpr bool value = true;
    };

    /// Fails to compile when `linear_least_squares` is given no citation.
    template <typename Given>
    struct RequireFitCitation
    {
        static_assert(sizeof(Given) == 0,
                      "formula: linear_least_squares needs a citation, the reason the method fits a line "
                      "here; pass {} when it gives none");

        static constexpr bool value = true;
    };

    /// The quantity @p T observes, or @p T itself for observations refused
    /// already, which observe none.
    template <typename T>
    struct ObservedQuantity
    {
        using type = T;
    };

    template <typename T>
        requires requires { typename T::quantity; }
    struct ObservedQuantity<T>
    {
        using type = typename T::quantity;
    };

    /// Fails to compile when a line through observations reads one quantity
    /// as its points and its values. Named so the quantity prints.
    template <typename Q>
    struct RequireFitOfTwoQuantities
    {
        static_assert(!std::is_same_v<Q, Q>,
                      "formula: linear_least_squares reads one quantity as both its points and its values, and a "
                      "line through every observation against itself says nothing; read the points and the values "
                      "as two quantities -- the quantity appears in this diagnostic as the template argument Q of "
                      "RequireFitOfTwoQuantities");

        static constexpr bool value = true;
    };

    /// What a refused line through observations returns: the operation over
    /// refused observations, so that each of its four outputs can be taken and
    /// evaluated and asks nothing again.
    using RefusedObservationFit = OpaqueCall<LinearLeastSquaresOfObservations, RefusedObservations, RefusedObservations>;

    [[nodiscard]] constexpr RefusedObservationFit refused_observation_fit(Citation citation) noexcept
    {
        return RefusedObservationFit { std::tuple<RefusedObservations, RefusedObservations> {}, citation };
    }

    /// @p T's dimension, or `dim::Scalar` for something that has none.
    template <typename T>
    [[nodiscard]] consteval Dimension dimension_or_scalar() noexcept
    {
        if constexpr (requires { T::dimension; })
            return T::dimension;
        else
            return dim::Scalar;
    }

    /// What a refused `linear_least_squares` returns: a fit of a refused
    /// curve (`CurveNode::refused`), in @p Domain's and @p Values'
    /// dimensions when they have one. Its outputs are nodes, refused as the
    /// call is, so taking one and evaluating it compiles and asks nothing
    /// again -- the one mistake draws the one message.
    template <typename Domain, typename Values>
    using RefusedFit =
        OpaqueCall<LinearLeastSquares,
                   CurveNode<RefusedSeries<dimension_or_scalar<Domain>()>, RefusedSeries<dimension_or_scalar<Values>()>>>;

    template <typename Domain, typename Values>
    [[nodiscard]] constexpr RefusedFit<Domain, Values> refused_fit(Citation citation) noexcept
    {
        using Refused =
            CurveNode<RefusedSeries<dimension_or_scalar<Domain>()>, RefusedSeries<dimension_or_scalar<Values>()>>;
        return RefusedFit<Domain, Values> { std::tuple<Refused> { Refused { {}, {}, {} } }, citation };
    }
} // namespace detail

/// A curve handed to `linear_least_squares` without a citation: refused in
/// this library's words. Anything else handed to it alone is refused the
/// same way, once: the citation is missing either way.
template <typename Fitted>
[[nodiscard]] constexpr auto linear_least_squares(Fitted) noexcept
{
    static_assert(detail::RequireFitCitation<Fitted>::value);
    if constexpr (CurveExpression<Fitted>)
        return detail::refused_fit<detail::RefusedSeries<Fitted::domainDimension>, Fitted>(Citation {});
    else
        return detail::refused_fit<void, Fitted>(Citation {});
}

/// Anything but a curve handed to `linear_least_squares`: refused in this
/// library's words. The return type is deduced, so that the refusal is
/// instantiated wherever the call is (`cumulative`, `series.hpp`).
template <typename NotCurve>
    requires(!CurveExpression<NotCurve>)
[[nodiscard]] constexpr auto linear_least_squares(NotCurve, Citation citation) noexcept
{
    static_assert(detail::RequireFitOfCurve<NotCurve>::value);
    if constexpr (ObservationsNode<NotCurve>)
        return detail::refused_observation_fit(citation);
    else
        return detail::refused_fit<void, NotCurve>(citation);
}

/// Two loose series handed to `linear_least_squares`: refused in this
/// library's words. A curve pairs the domain with its values, which two
/// series would have to re-derive.
template <typename Domain, typename Values>
    requires(!(ObservationsNode<Domain> && ObservationsNode<Values>))
[[nodiscard]] constexpr auto linear_least_squares(Domain, Values, Citation citation) noexcept
{
    static_assert(detail::RequireFitOfCurve<Domain, Values>::value);
    if constexpr (ObservationsNode<Domain> || ObservationsNode<Values>)
        return detail::refused_observation_fit(citation);
    else
        return detail::refused_fit<Domain, Values>(citation);
}

/// A straight line through raw observations, paired row by row, for the
/// reason @p citation gives: `linear_least_squares(observations<Elapsed, 64>,
/// observations<Length, 64>, { ... })`. Its outputs are `intercept`, `slope`,
/// `r squared` and `points`. One quantity read as both is refused in this
/// library's words.
template <ObservationsNode X, ObservationsNode Y>
[[nodiscard]] constexpr auto linear_least_squares(X pointObservations, Y valueObservations, Citation citation) noexcept
{
    using PointsQuantity = typename detail::ObservedQuantity<X>::type;
    constexpr bool oneQuantity = std::is_same_v<PointsQuantity, typename detail::ObservedQuantity<Y>::type>;
    static_assert(std::conditional_t<oneQuantity, detail::RequireFitOfTwoQuantities<PointsQuantity>, std::true_type>::value);
    if constexpr (oneQuantity)
        return detail::refused_observation_fit(citation);
    else
        return opaque<LinearLeastSquaresOfObservations>(citation, pointObservations, valueObservations);
}

/// Raw observations handed to `linear_least_squares` without a citation:
/// refused in this library's words, as a curve without one is.
template <ObservationsNode X, ObservationsNode Y>
[[nodiscard]] constexpr auto linear_least_squares(X, Y) noexcept
{
    static_assert(detail::RequireFitCitation<X>::value);
    return detail::refused_observation_fit(Citation {});
}
} // namespace formula
