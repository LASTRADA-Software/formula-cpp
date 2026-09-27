// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Curves: a domain series paired with a value series, read at a point in
/// between (`interpolate_at`), and two curves spliced into one (`splice`).
///
/// **A curve is neither a `Node` nor a `SeriesNode`.** It is the only value
/// this library holds that is two vectors -- the points of a domain and the
/// value at each -- and it is its own family, `CurveExpression`, so that
/// handing one where a number or a series is expected is a compile error.
/// `interpolate_at` is the bridge back to one value, and is a `Node`.
///
/// `domain<KeyUnit, Points>` is a series whose elements come from a
/// `BreakpointTable` in the type, validated as every breakpoint table is
/// (`RequireValidBreakpointTable`): it is how a method declares its points
/// once and pairs them with what it measured at each.
///
/// **A curve's domain strictly ascends, checked when it is evaluated.** A
/// declared domain is checked at compile time already; a computed one --
/// `curve(series<Opening, 5>, ...)` -- only arrives at run time, and one that
/// does not strictly ascend fails the curve with a `DomainError` at its first
/// element that is not above the one before it. An absent point is skipped:
/// the present points on either side of it must still ascend.
///
/// **Interpolation** locates the point with the one scan of ascending keys an
/// interpolating lookup uses (`detail::locate_key`, `lookup.hpp`) and computes
/// the value with the one formula for it (`detail::interpolate_between`):
/// the rule is written once. Off the ends of the domain is a miss, never an
/// extrapolation or a clamp. It is carried out in the coherent SI unit, since
/// a computed domain has no declared unit of its own to carry it out in; a
/// linear interpolation's answer does not depend on the scale either axis is
/// stated in. Its exact arithmetic can: a point that is exact in its declared
/// unit may not be in the coherent one. `domain<Millimetre, {1/10^16, 1}>`
/// fails with `Overflow` at its first element, since 1/10^19 m has a
/// denominator no int64 holds, where an interpolating lookup over the same
/// table -- which interpolates in its declared key unit -- answers. The
/// failure is conservative: it never gives a wrong number.
///
/// **A splice** is the sorted union of two curves by domain, of static length
/// `A::length + B::length`, whichever curve is written first. Two points at
/// one domain value are a miss at the second of them, where the two curves
/// meet; and the values must run in the direction the author names -- a
/// required `Monotone`, with no default -- judged on the spliced curve, not on
/// each operand, or it fails at the first element that breaks it. Duplicates
/// are judged over the whole union before the direction, so where a splice
/// fails never depends on which curve was written first. **Nothing is
/// rescaled at the join**: putting one basis onto another is the method's
/// algebra, written with elementwise arithmetic before splicing, where the
/// trace shows it.
///
/// **Absence is strict** (S7): an absent element anywhere in a curve makes an
/// interpolation along it absent, and a splice of it wholly absent.
///
/// Everything here compares, so a curve is evaluated with `Rep = Rational`
/// only, refused otherwise in this library's words: a comparison a few ULPs
/// off picks the wrong segment, silently.

#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace formula
{

namespace detail
{
    /// Fails to compile when a domain declares no points. Named so the table
    /// prints.
    template <BreakpointTable Points>
    struct RequireDomainNotEmpty
    {
        static_assert(Points.size() > 0,
                      "formula: this domain declares no points; a domain is the points a method's series are "
                      "measured at, and a domain of none has nothing to pair, interpolate or trace -- declare at "
                      "least one point");

        static constexpr bool value = true;
    };
} // namespace detail

/// A series whose elements are the points of @p Points, stated in @p U: the
/// points a method measures at, declared once.
///
/// Empty, like `SeriesVarNode`: its whole shape is in the type. The points
/// are validated in the class body, for `InterpolatingLookupNode`'s reason: a
/// public aggregate can be declared without the factory.
template <Unit U, BreakpointTable Points>
struct DomainNode: SeriesNodeBase
{
    static_assert(detail::RequireDomainNotEmpty<Points>::value);
    static_assert(RequireValidBreakpointTable<Points>::value);

    /// The unit the points are declared in.
    static constexpr Unit unit = U;
    /// The dimension of each point: `U`'s.
    static constexpr Dimension dimension = U.dimension;
    /// How many points there are.
    static constexpr std::size_t length = Points.size();
    /// The points, already validated above.
    static constexpr BreakpointTable<Points.size()> points = Points;
    /// Whether the table was refused -- see `detail::refused_already`. A
    /// curve over it asks nothing more of it.
    static constexpr bool refused = Points.size() == 0 || !breakpoint_table_is_well_formed(Points);
};

/// A method's points, declared once: `domain<unit::Millimetre, Screens>`. A
/// variable template, as `series<Q, N>` and `observations<Q, Capacity>` are,
/// since like theirs its whole shape is in the type. `U` and `Points` are the
/// method's declared intent, never deduced.
template <Unit U, BreakpointTable Points>
inline constexpr DomainNode<U, Points> domain {};

/// The empty base every curve node derives from, and what `CurveExpression`
/// recognises. Not `NodeBase` and not `SeriesNodeBase`: a curve is neither
/// one value nor one series.
struct CurveNodeBase
{
};

/// A curve: `curve(...)`, or a `splice` of two.
template <typename T>
concept CurveExpression = std::derived_from<std::remove_cvref_t<T>, CurveNodeBase>;

namespace detail
{
    /// Fails to compile when a curve pairs series of different lengths.
    /// Named so both series, each with its length, print.
    template <typename DomainSeries, typename ValueSeries>
    struct RequireCurveLengthsAgree
    {
        static_assert(DomainSeries::length == ValueSeries::length,
                      "formula: this curve pairs a domain and values of different lengths; the two series appear "
                      "in this diagnostic as the template arguments of RequireCurveLengthsAgree, each with its "
                      "length -- a curve pairs point i of its domain with value i, so both need the method's one "
                      "length");

        static constexpr bool value = true;
    };
} // namespace detail

/// The domain series @p D paired with the value series @p V, element by
/// element: value i is the curve's value at point i.
///
/// No `{}` initialiser on either series, deliberately (defect class 4): see
/// `Corrections` (`lookup.hpp`).
template <SeriesNode D, SeriesNode V>
struct CurveNode: CurveNodeBase
{
    /// Whether either series was refused already -- then nothing more is
    /// asked of their stand-in lengths.
    static constexpr bool operandsRefused = detail::refused_already<D>() || detail::refused_already<V>();

    static_assert(std::conditional_t<!operandsRefused, detail::RequireCurveLengthsAgree<D, V>, std::true_type>::value);

    /// The points, which must strictly ascend.
    D domainSeries;
    /// The value at each point.
    V valueSeries;

    /// How many points the curve has.
    static constexpr std::size_t length = D::length;
    /// The dimension of its points.
    static constexpr Dimension domainDimension = D::dimension;
    /// The dimension of its values: what an interpolation along it produces.
    static constexpr Dimension dimension = V::dimension;
    /// Whether this curve was refused, or holds a refused series.
    static constexpr bool refused = operandsRefused || D::length != V::length;
};

/// Pairs @p domainSeries with @p valueSeries: `curve(domain<unit::Millimetre,
/// Screens>, passing)`.
template <SeriesNode D, SeriesNode V>
[[nodiscard]] constexpr CurveNode<D, V> curve(D domainSeries, V valueSeries) noexcept
{
    return CurveNode<D, V> { {}, domainSeries, valueSeries };
}

namespace detail
{
    /// Fails to compile when `curve` is given a single value where a series
    /// belongs. Named so the operand prints.
    template <typename Operand>
    struct RequireCurveOfSeries
    {
        static_assert(SeriesNode<Operand>,
                      "formula: a curve pairs a series of points with a series of values, and this is a single "
                      "value, not a series; the operand appears in this diagnostic as the template argument of "
                      "RequireCurveOfSeries -- read a quantity measured at every point with series<Q, N>, or declare "
                      "the points with domain<U, Points>");

        static constexpr bool value = true;
    };

    /// @p T as a curve's half: a series as it is, and a single value as a
    /// series refused already (`RefusedSeries`), which silences every check
    /// the curve and anything over it would otherwise make.
    template <typename T>
    struct AsCurveHalf
    {
        using type = T;
    };

    template <Node N>
    struct AsCurveHalf<N>
    {
        using type = RefusedSeries<N::dimension>;
    };

    /// The half itself: the series, or a refused stand-in for a single value.
    template <typename T>
    [[nodiscard]] constexpr typename AsCurveHalf<T>::type as_curve_half(T const& half) noexcept
    {
        if constexpr (Node<T>)
            return RefusedSeries<T::dimension> {};
        else
            return half;
    }
} // namespace detail

/// A single value handed to `curve`, for its points, its values or both:
/// refused in this library's words, once, naming the first single value --
/// the task 5 ruling for `sum` and `cumulative`. It returns a curve of refused
/// series, which every check over it takes as already refused. The return
/// type is deduced, for `cumulative`'s reason.
template <typename D, typename V>
    requires(Node<D> || Node<V>) && (Node<D> || SeriesNode<D>) && (Node<V> || SeriesNode<V>)
[[nodiscard]] constexpr auto curve(D domainHalf, V valueHalf) noexcept
{
    static_assert(detail::RequireCurveOfSeries<std::conditional_t<Node<D>, D, V>>::value);
    return CurveNode<typename detail::AsCurveHalf<D>::type, typename detail::AsCurveHalf<V>::type> {
        {}, detail::as_curve_half(domainHalf), detail::as_curve_half(valueHalf)
    };
}

/// The direction a spliced curve's values must run in. **Required**, with no
/// default: which way a curve runs is the method's, and a splice that assumed
/// one would pass a curve running the other way silently.
enum class Monotone : std::uint8_t
{
    /// Each value at least the one before it.
    NonDecreasing,
    /// Each value at most the one before it.
    NonIncreasing,
};

/// The direction in words, as a rendering and a trace state it.
[[nodiscard]] constexpr std::string_view describe(Monotone direction) noexcept
{
    switch (direction)
    {
        case Monotone::NonDecreasing:
            return "non-decreasing";
        case Monotone::NonIncreasing:
            return "non-increasing";
    }
    return "unknown direction";
}

/// The rule a curve broke where it failed, as its trace step names it.
enum class CurveBreak : std::uint8_t
{
    /// No rule named: the curve did not fail, or failed for another reason.
    None,
    /// A point below the one before it.
    NotAscending,
    /// A point equal to the one before it: stated twice, or where two
    /// spliced curves meet.
    DuplicatePoint,
    /// A spliced value running against the splice's `Monotone`.
    AgainstDirection,
};

namespace detail
{
    /// Fails to compile when two curves whose points measure different
    /// dimensions are spliced. Named so both curves print.
    template <typename A, typename B>
    struct RequireSpliceDomainsAgree
    {
        static_assert(A::domainDimension == B::domainDimension,
                      "formula: the two curves spliced here have domains of different dimensions; the curves "
                      "appear in this diagnostic as the template arguments of RequireSpliceDomainsAgree -- a "
                      "splice sorts the points of both curves into one domain, so they must measure one "
                      "quantity");

        static constexpr bool value = true;
    };

    /// Fails to compile when two curves whose values measure different
    /// dimensions are spliced. Named so both curves print.
    template <typename A, typename B>
    struct RequireSpliceValuesAgree
    {
        static_assert(A::dimension == B::dimension,
                      "formula: the two curves spliced here have values of different dimensions; the curves "
                      "appear in this diagnostic as the template arguments of RequireSpliceValuesAgree -- a "
                      "splice rescales nothing, so put one basis onto the other with elementwise arithmetic "
                      "before splicing");

        static constexpr bool value = true;
    };
} // namespace detail

/// Two curves spliced into one: the sorted union of their points, values
/// running as @p M says.
///
/// No `{}` initialiser on either curve, deliberately (defect class 4).
template <Monotone M, CurveExpression A, CurveExpression B>
struct SpliceNode: CurveNodeBase
{
    /// Whether either curve was refused already.
    static constexpr bool operandsRefused = detail::refused_already<A>() || detail::refused_already<B>();

    static_assert(std::conditional_t<!operandsRefused, detail::RequireSpliceDomainsAgree<A, B>, std::true_type>::value);
    static_assert(std::conditional_t<!operandsRefused, detail::RequireSpliceValuesAgree<A, B>, std::true_type>::value);

    /// The curve written first.
    A first;
    /// The curve written second. Which is which changes nothing.
    B second;

    /// The direction the spliced values must run in.
    static constexpr Monotone monotone = M;
    /// Every point of both curves.
    static constexpr std::size_t length = A::length + B::length;
    /// The dimension of the points.
    static constexpr Dimension domainDimension = A::domainDimension;
    /// The dimension of the values.
    static constexpr Dimension dimension = A::dimension;
    /// Whether this splice was refused, or holds a refused curve.
    static constexpr bool refused =
        operandsRefused || !(A::domainDimension == B::domainDimension) || !(A::dimension == B::dimension);
};

/// Splices @p firstCurve and @p secondCurve into one curve whose values run as
/// @p M says: `splice<Monotone::NonDecreasing>(coarse, fine)`. `M` is never
/// deduced and has no default.
template <Monotone M, CurveExpression A, CurveExpression B>
[[nodiscard]] constexpr SpliceNode<M, A, B> splice(A firstCurve, B secondCurve) noexcept
{
    return SpliceNode<M, A, B> { {}, firstCurve, secondCurve };
}

namespace detail
{
    /// Fails to compile when `splice` is given a single value where a curve
    /// belongs. Named so the operand prints.
    template <typename Operand>
    struct RequireSpliceOfCurves
    {
        static_assert(CurveExpression<Operand>,
                      "formula: splice joins two curves, and this is a single value, not a curve; the operand "
                      "appears in this diagnostic as the template argument of RequireSpliceOfCurves -- pair a "
                      "series of points with a series of values with curve(points, values)");

        static constexpr bool value = true;
    };

    /// @p T as a spliced curve: a curve as it is, and a single value as a
    /// curve of refused series, which silences the splice's own checks.
    template <typename T>
    struct AsSplicedCurve
    {
        using type = T;
    };

    template <Node N>
    struct AsSplicedCurve<N>
    {
        using type = CurveNode<RefusedSeries<N::dimension>, RefusedSeries<N::dimension>>;
    };

    /// The curve itself, or a refused stand-in for a single value.
    template <typename T>
    [[nodiscard]] constexpr typename AsSplicedCurve<T>::type as_spliced_curve(T const& candidate) noexcept
    {
        if constexpr (Node<T>)
            return typename AsSplicedCurve<T>::type { {}, RefusedSeries<T::dimension> {}, RefusedSeries<T::dimension> {} };
        else
            return candidate;
    }
} // namespace detail

/// A single value handed to `splice`: refused in this library's words, once,
/// naming the first single value, as `curve` refuses one. It returns a splice
/// of a refused curve, so nothing over it refuses again.
template <Monotone M, typename A, typename B>
    requires(Node<A> || Node<B>) && (Node<A> || CurveExpression<A>) && (Node<B> || CurveExpression<B>)
[[nodiscard]] constexpr auto splice(A firstOperand, B secondOperand) noexcept
{
    static_assert(detail::RequireSpliceOfCurves<std::conditional_t<Node<A>, A, B>>::value);
    return SpliceNode<M, typename detail::AsSplicedCurve<A>::type, typename detail::AsSplicedCurve<B>::type> {
        {}, detail::as_spliced_curve(firstOperand), detail::as_spliced_curve(secondOperand)
    };
}

namespace detail
{
    /// Fails to compile when a curve is read at a point that does not measure
    /// its domain's dimension. Named so the curve and the point print.
    template <typename C, typename At>
    struct RequireInterpolationPointMatches
    {
        static_assert(C::domainDimension == At::dimension,
                      "formula: this curve is read at a point that does not measure the dimension of its domain; "
                      "the curve and the point appear in this diagnostic as the template arguments of "
                      "RequireInterpolationPointMatches");

        static constexpr bool value = true;
    };
} // namespace detail

/// The value of the curve @p C at the point @p At evaluates to: one value, and
/// so a `Node`.
///
/// No `{}` initialiser on either member, deliberately (defect class 4).
template <CurveExpression C, Node At>
struct InterpolateAlongNode: NodeBase
{
    static_assert(std::conditional_t<!detail::refused_already<C>(),
                                     detail::RequireInterpolationPointMatches<C, At>,
                                     std::true_type>::value);

    /// The curve read.
    C along;
    /// Where it is read.
    At at;

    /// The dimension of the curve's values.
    static constexpr Dimension dimension = C::dimension;
    /// Whether this interpolation was refused, or reads a refused curve.
    static constexpr bool refused = detail::refused_already<C>() || !(C::domainDimension == At::dimension);
};

/// The value of @p curveExpression at @p at: `interpolate_at(curve(screens,
/// passing), constant<unit::Millimetre>(rat(42, 10)))`.
template <CurveExpression C, Node At>
[[nodiscard]] constexpr InterpolateAlongNode<C, At> interpolate_at(C curveExpression, At at) noexcept
{
    return InterpolateAlongNode<C, At> { {}, curveExpression, at };
}

/// An evaluated curve in the coherent SI units of its dimensions: each point
/// and each value, either absent when it was never measured.
template <typename Rep, std::size_t N>
struct CurveValue
{
    /// The points, in order. No `{}` initialiser, and default-initialised by
    /// the evaluators, for `SeriesValue`'s reason (cl C4459).
    std::array<std::optional<Rep>, N> domain;
    /// The value at each point.
    std::array<std::optional<Rep>, N> values;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(CurveValue const&) const noexcept = default;
};

/// The result of evaluating a curve: its points and values, or the one failure
/// that stopped it -- a `SeriesFailure`, whose position is in the curve the
/// failing step produces.
template <typename Rep, std::size_t N>
using EvaluatedCurve = std::expected<CurveValue<Rep, N>, SeriesFailure>;

namespace detail
{
    /// Whether @p Sink wants to hear about the curve node @p C: true when it
    /// defines **both** `curve_entered(node)` and `curve_produced(node,
    /// result)` -- `HearsSeries`' rule, for its reason.
    template <typename Sink, typename C, typename Rep>
    concept HearsCurve = requires(Sink sink, C const& node, EvaluatedCurve<Rep, C::length> const& evaluated) {
        sink.curve_entered(node);
        sink.curve_produced(node, evaluated);
    };

    /// Evaluates the curve node @p node with @p sink: `dispatch_series`'
    /// counterpart for a `CurveExpression`, finding each kind's
    /// `checked_evaluate_curve_si` by ADL.
    template <typename Rep, typename C, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedCurve<Rep, std::remove_cvref_t<C>::length> dispatch_curve(C const& node,
                                                                                               Env const& environment,
                                                                                               Sink sink) noexcept
    {
        return checked_evaluate_curve_si<Rep>(node, environment, sink);
    }

    /// The refusal for any `Rep` but `Rational`, in one place.
    template <typename Rep>
    constexpr void require_exact_curve() noexcept
    {
        static_assert(std::is_same_v<Rep, Rational>,
                      "formula: a curve can only be evaluated with Rep = Rational -- ordering its points, "
                      "locating a point between two and judging a splice's direction all compare, and a "
                      "comparison a few units in the last place off picks the wrong answer silently; evaluate "
                      "this formula with Rep = Rational instead (checked_evaluate<Result> always does)");
    }

    /// Whether every element of @p elements is present.
    template <std::size_t N>
    [[nodiscard]] constexpr bool all_present(std::array<std::optional<Rational>, N> const& elements) noexcept
    {
        for (std::optional<Rational> const& candidate: elements)
            if (!candidate.has_value())
                return false;
        return true;
    }

    /// Where a curve breaks a rule, and which rule.
    struct CurveBreakAt
    {
        /// The position of the offending point, zero-based.
        std::size_t at;
        /// The rule it breaks.
        CurveBreak rule;
    };

    /// The first present point not above the present point before it -- a
    /// `DuplicatePoint` when it equals that one, else `NotAscending` -- or
    /// nothing when the present points strictly ascend. An absent point is
    /// skipped, not judged: a gap in the domain makes a reading along the
    /// curve absent, but it does not excuse the points on either side of it
    /// from ascending.
    ///
    /// Spans, as `interpolate_along`'s are, so that the evaluator and the
    /// trace judge with this one function.
    [[nodiscard]] constexpr std::optional<CurveBreakAt> judge_domain(
        std::span<std::optional<Rational> const> points) noexcept
    {
        std::optional<Rational> previous;
        for (std::size_t at = 0; at < points.size(); ++at)
        {
            if (!points[at].has_value())
                continue;
            if (previous.has_value() && !(*previous < *points[at]))
                return CurveBreakAt { at, *points[at] == *previous ? CurveBreak::DuplicatePoint : CurveBreak::NotAscending };
            previous = points[at];
        }
        return std::nullopt;
    }

    /// Sorts a splice's points ascending, each value moving with its point:
    /// an insertion sort, since a method's curves are a few points each and
    /// nothing here may allocate. Every point must be present.
    constexpr void sort_by_domain(std::span<std::optional<Rational>> points,
                                  std::span<std::optional<Rational>> pointValues) noexcept
    {
        for (std::size_t placed = 1; placed < points.size(); ++placed)
            for (std::size_t at = placed; at > 0 && *points[at] < *points[at - 1]; --at)
            {
                std::swap(points[at], points[at - 1]);
                std::swap(pointValues[at], pointValues[at - 1]);
            }
    }

    /// Judges a splice's sorted union: first a point equal to the one before
    /// it, anywhere in the union, and only then the first value running
    /// against @p direction. In that order because the sort keeps two equal
    /// points in the order they were written, so a direction judged first
    /// would fail beside a duplicate at a position that depends on which
    /// curve came first. Every point and value must be present.
    [[nodiscard]] constexpr std::optional<CurveBreakAt> judge_splice(std::span<std::optional<Rational> const> points,
                                                                     std::span<std::optional<Rational> const> pointValues,
                                                                     Monotone direction) noexcept
    {
        for (std::size_t at = 1; at < points.size(); ++at)
            if (*points[at] == *points[at - 1])
                return CurveBreakAt { at, CurveBreak::DuplicatePoint };
        for (std::size_t at = 1; at < pointValues.size(); ++at)
        {
            bool const against = direction == Monotone::NonDecreasing ? *pointValues[at] < *pointValues[at - 1]
                                                                      : *pointValues[at - 1] < *pointValues[at];
            if (against)
                return CurveBreakAt { at, CurveBreak::AgainstDirection };
        }
        return std::nullopt;
    }

    /// The value along a curve at @p atKey, and where it sat: the curve's points
    /// @p points and values @p values, every one present and the points
    /// strictly ascending, in the coherent SI unit. The one scan is
    /// `locate_key`, and the one formula `interpolate_between`, both an
    /// interpolating lookup's (`lookup.hpp`); a miss is `DomainError`.
    ///
    /// Spans, so that the evaluator's arrays and a trace step's vectors are
    /// read by this one function.
    [[nodiscard]] constexpr std::expected<std::pair<Rational, KeyPosition>, ArithmeticError> interpolate_along(
        std::span<std::optional<Rational> const> points,
        std::span<std::optional<Rational> const> curveValues,
        Rational atKey) noexcept
    {
        auto const pointAt = [&](std::size_t at) -> std::expected<Rational, ArithmeticError> {
            if (!points[at].has_value())
                return std::unexpected { ArithmeticError::DomainError };
            return *points[at];
        };
        std::expected<KeyPosition, ArithmeticError> const located = locate_key(points.size(), pointAt, atKey);
        if (!located.has_value())
            return std::unexpected { located.error() };
        if (!curveValues[located->low].has_value() || !curveValues[located->high].has_value())
            return std::unexpected { ArithmeticError::DomainError };
        if (located->low == located->high)
            return std::pair<Rational, KeyPosition> { *curveValues[located->low], *located };
        std::expected<Rational, ArithmeticError> const answered = interpolate_between(
            *points[located->low], *curveValues[located->low], *points[located->high], *curveValues[located->high], atKey);
        if (!answered.has_value())
            return std::unexpected { answered.error() };
        return std::pair<Rational, KeyPosition> { *answered, *located };
    }
} // namespace detail

/// Reads each point of a declared domain into the coherent SI unit of its
/// dimension. Every point is present; a conversion that overflows fails the
/// series at that point. Any `Rep`: a domain alone compares nothing.
template <typename Rep = Rational, Unit U, BreakpointTable Points, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, Points.size()> checked_evaluate_series_si(DomainNode<U, Points> const& node,
                                                                                       Env const&,
                                                                                       Sink sink = {}) noexcept
{
    constexpr std::size_t domainLength = Points.size();
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, domainLength> const evaluated = [&]() -> EvaluatedSeries<Rep, domainLength> {
        SeriesValue<Rep, domainLength> inCoherentUnit;
        for (std::size_t at = 0; at < domainLength; ++at)
        {
            // Unreachable for a table that passed its validation.
            std::expected<Rational, ArithmeticError> const declared =
                Rational::make(Points[at].numerator, Points[at].denominator);
            if (!declared.has_value())
                return std::unexpected { SeriesFailure { ArithmeticError::DomainError, at } };
            Evaluated<Rep> const pointInSi = detail::in_si<Rep>(*declared, U);
            if (!pointInSi.has_value())
                return std::unexpected { SeriesFailure { pointInSi.error(), at } };
            inCoherentUnit.elements[at] = **pointInSi;
        }
        return inCoherentUnit;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

/// Pairs the evaluated domain and values: the domain first, then the values,
/// each once; a failure of either is relayed (`detail::relayed_failure`).
/// When every point is present they must strictly ascend, or the curve fails
/// at the first point that is not above the one before it. An absent element stays absent here,
/// and makes whatever reads the curve absent.
template <typename Rep = Rational, SeriesNode D, SeriesNode V, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedCurve<Rep, CurveNode<D, V>::length> checked_evaluate_curve_si(CurveNode<D, V> const& node,
                                                                                               Env const& environment,
                                                                                               Sink sink = {}) noexcept
{
    constexpr std::size_t curveLength = CurveNode<D, V>::length;
    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        detail::require_exact_curve<Rep>();
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    }
    // Refused already: the two lengths are not one curve to pair.
    else if constexpr (CurveNode<D, V>::refused)
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else
    {
        if constexpr (detail::HearsCurve<Sink, CurveNode<D, V>, Rep>)
            sink.curve_entered(node);
        EvaluatedCurve<Rep, curveLength> const evaluated = [&]() -> EvaluatedCurve<Rep, curveLength> {
            EvaluatedSeries<Rep, curveLength> const points =
                detail::dispatch_series<Rep>(node.domainSeries, environment, sink);
            if (!points.has_value())
                return std::unexpected { detail::relayed_failure(points) };
            EvaluatedSeries<Rep, curveLength> const pairedValues =
                detail::dispatch_series<Rep>(node.valueSeries, environment, sink);
            if (!pairedValues.has_value())
                return std::unexpected { detail::relayed_failure(pairedValues) };

            CurveValue<Rep, curveLength> paired;
            paired.domain = points->elements;
            paired.values = pairedValues->elements;
            if (std::optional<detail::CurveBreakAt> const disorder = detail::judge_domain(paired.domain))
                return std::unexpected { SeriesFailure { ArithmeticError::DomainError, disorder->at } };
            return paired;
        }();
        if constexpr (detail::HearsCurve<Sink, CurveNode<D, V>, Rep>)
            sink.curve_produced(node, evaluated);
        return evaluated;
    }
}

/// Splices the two curves: the first, then the second, each once; a failure of
/// either is relayed with its error and no element, since its position is in
/// that curve's points, not the splice's. An absent element in either makes
/// the whole splice absent. Otherwise every point of both is sorted by domain
/// -- the order the two were written in plays no part -- and a point equal to
/// the one before it fails the splice there, where the curves meet; only a
/// union with no such point is then judged for a value running against `M`
/// (`detail::judge_splice`).
template <typename Rep = Rational, Monotone M, CurveExpression A, CurveExpression B, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedCurve<Rep, SpliceNode<M, A, B>::length> checked_evaluate_curve_si(
    SpliceNode<M, A, B> const& node, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t splicedLength = SpliceNode<M, A, B>::length;
    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        detail::require_exact_curve<Rep>();
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    }
    else if constexpr (SpliceNode<M, A, B>::refused)
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else
    {
        if constexpr (detail::HearsCurve<Sink, SpliceNode<M, A, B>, Rep>)
            sink.curve_entered(node);
        EvaluatedCurve<Rep, splicedLength> const evaluated = [&]() -> EvaluatedCurve<Rep, splicedLength> {
            EvaluatedCurve<Rep, A::length> const firstCurve = detail::dispatch_curve<Rep>(node.first, environment, sink);
            // An operand's position is in its own points, not in the union:
            // relayed without it, as its own step names it.
            if (!firstCurve.has_value())
                return std::unexpected { SeriesFailure { firstCurve.error().error, std::nullopt } };
            EvaluatedCurve<Rep, B::length> const secondCurve = detail::dispatch_curve<Rep>(node.second, environment, sink);
            if (!secondCurve.has_value())
                return std::unexpected { SeriesFailure { secondCurve.error().error, std::nullopt } };

            // Default-initialised: every element absent.
            CurveValue<Rep, splicedLength> spliced;
            if (!detail::all_present(firstCurve->domain) || !detail::all_present(firstCurve->values)
                || !detail::all_present(secondCurve->domain) || !detail::all_present(secondCurve->values))
                return spliced;

            for (std::size_t at = 0; at < A::length; ++at)
            {
                spliced.domain[at] = firstCurve->domain[at];
                spliced.values[at] = firstCurve->values[at];
            }
            for (std::size_t at = 0; at < B::length; ++at)
            {
                spliced.domain[A::length + at] = secondCurve->domain[at];
                spliced.values[A::length + at] = secondCurve->values[at];
            }
            detail::sort_by_domain(spliced.domain, spliced.values);
            if (std::optional<detail::CurveBreakAt> const broken = detail::judge_splice(spliced.domain, spliced.values, M))
                return std::unexpected { SeriesFailure { ArithmeticError::DomainError, broken->at } };
            return spliced;
        }();
        if constexpr (detail::HearsCurve<Sink, SpliceNode<M, A, B>, Rep>)
            sink.curve_produced(node, evaluated);
        return evaluated;
    }
}

/// The curve's value at the point: the curve first, then the point, each
/// once. A failed curve relays its error -- its position cannot be carried by
/// one value, and the trace names it -- and a failed point relays its own.
/// An absent point, or an absent element anywhere in the curve, makes the
/// answer absent (S7). Off the ends of the domain is a `DomainError` miss;
/// an interpolation whose exact answer is not representable is `Overflow`.
template <typename Rep = Rational, CurveExpression C, Node At, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(InterpolateAlongNode<C, At> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        detail::require_exact_curve<Rep>();
        return std::unexpected { ArithmeticError::DomainError };
    }
    else if constexpr (InterpolateAlongNode<C, At>::refused)
        return std::unexpected { ArithmeticError::DomainError };
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
            EvaluatedCurve<Rep, C::length> const alongCurve = detail::dispatch_curve<Rep>(node.along, environment, sink);
            Evaluated<Rep> const point = detail::dispatch<Rep>(node.at, environment, sink);
            if (!alongCurve.has_value())
                return std::unexpected { alongCurve.error().error };
            if (!point.has_value())
                return std::unexpected { point.error() };
            if (!point->has_value() || !detail::all_present(alongCurve->domain) || !detail::all_present(alongCurve->values))
                return detail::nothing<Rep>();
            std::expected<std::pair<Rational, detail::KeyPosition>, ArithmeticError> const answered =
                detail::interpolate_along(alongCurve->domain, alongCurve->values, **point);
            if (!answered.has_value())
                return std::unexpected { answered.error() };
            return Evaluated<Rep> { answered->first };
        }();
        sink.produced(node, evaluated);
        return evaluated;
    }
}

/// The result of evaluating a curve for the quantities @p DomainResult and
/// @p ValueResult: `N` points in `DomainResult`'s declared unit and `N` values
/// in `ValueResult`'s.
template <Described DomainResult, Described ValueResult, std::size_t N>
class CurveOutcome
{
  public:
    /// A curve's points and values.
    [[nodiscard]] static constexpr CurveOutcome value(std::array<Measured<DomainResult>, N> points,
                                                      std::array<Measured<ValueResult>, N> pointValues) noexcept
    {
        return CurveOutcome { points, pointValues };
    }

    /// Every point, in order.
    [[nodiscard]] constexpr std::array<Measured<DomainResult>, N> domain() const noexcept
    {
        return _domain;
    }

    /// The value at each point.
    [[nodiscard]] constexpr std::array<Measured<ValueResult>, N> values() const noexcept
    {
        return _values;
    }

    /// How many points there are -- `N`, present or not.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(CurveOutcome const&) const noexcept = default;

  private:
    constexpr CurveOutcome(std::array<Measured<DomainResult>, N> points,
                           std::array<Measured<ValueResult>, N> pointValues) noexcept:
        _domain { points },
        _values { pointValues }
    {
    }

    std::array<Measured<DomainResult>, N> _domain;
    std::array<Measured<ValueResult>, N> _values;
};

namespace detail
{
    /// Each present element of @p elements, from the coherent SI unit of
    /// @p dimension into @p Q's declared unit; nothing when a conversion
    /// fails, with the element it failed at.
    template <Described Q, std::size_t N>
    [[nodiscard]] constexpr std::expected<std::array<Measured<Q>, N>, SeriesFailure> in_declared_unit(
        std::array<std::optional<Rational>, N> const& elements, Dimension dimension) noexcept
    {
        std::array<Measured<Q>, N> inQuantityUnit;
        for (std::size_t at = 0; at < N; ++at)
        {
            if (!elements[at].has_value())
                continue;
            std::expected<Rational, ArithmeticError> const inUnit =
                checked_convert(*elements[at], coherent(dimension), Describe<Q>::unit);
            if (!inUnit.has_value())
                return std::unexpected { SeriesFailure { inUnit.error(), at } };
            inQuantityUnit[at] = Measured<Q> { *inUnit };
        }
        return inQuantityUnit;
    }
} // namespace detail

/// Evaluates the curve @p expression, and returns its points in
/// @p DomainResult's declared unit and its values in @p ValueResult's.
///
/// Neither quantity is deduced, for `checked_evaluate`'s reason, and each must
/// measure its half's dimension. A curve cannot be entered by hand -- it is
/// always computed -- so there is no override to consult. There is no throwing
/// twin, for `checked_evaluate_series`' reason.
///
/// A conversion into either declared unit that overflows fails with the
/// position it failed at. The points are converted before the values, but
/// the `SeriesFailure` does not say which of the two failed: a point and its
/// value share one position.
template <Described DomainResult, Described ValueResult, CurveExpression C, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<CurveOutcome<DomainResult, ValueResult, C::length>, SeriesFailure>
checked_evaluate_curve(C const& expression, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t curveLength = C::length;
    constexpr bool domainMatches = Describe<DomainResult>::dimension == C::domainDimension;
    constexpr bool valuesMatch = Describe<ValueResult>::dimension == C::dimension;
    static_assert(domainMatches,
                  "formula: this domain quantity does not measure the dimension of the curve's points; the quantity "
                  "and the curve appear in this diagnostic as the template arguments of checked_evaluate_curve");
    static_assert(valuesMatch,
                  "formula: this value quantity does not measure the dimension of the curve's values; the quantity "
                  "and the curve appear in this diagnostic as the template arguments of checked_evaluate_curve");

    if constexpr (!domainMatches || !valuesMatch)
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else
    {
        EvaluatedCurve<Rational, curveLength> const computed =
            detail::dispatch_curve<Rational>(expression, environment, sink);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        auto const points = detail::in_declared_unit<DomainResult>(computed->domain, C::domainDimension);
        if (!points.has_value())
            return std::unexpected { points.error() };
        auto const valuesInUnit = detail::in_declared_unit<ValueResult>(computed->values, C::dimension);
        if (!valuesInUnit.has_value())
            return std::unexpected { valuesInUnit.error() };
        return CurveOutcome<DomainResult, ValueResult, curveLength>::value(*points, *valuesInUnit);
    }
}

} // namespace formula
