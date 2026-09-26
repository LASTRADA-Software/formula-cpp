// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A series: one quantity at each of the `N` points of a method's domain --
/// the masses retained on a screen analysis's screens, say -- evaluated
/// element by element.
///
/// A series is a family of expression nodes of its own (`SeriesNode`, declared
/// in `expression.hpp` beside `Node`), and deliberately not a kind of `Node`:
/// every `Node` promises one value, and a series in a scalar position is
/// therefore a compile error, which `checked_evaluate`, `evaluate` and
/// `variant<Tag>` put in this library's words.
///
/// **The length is static.** `N` is part of every series node's type, as it is
/// of the input (`MeasuredSeries<Q, N>`, `environment.hpp`), because a method's
/// domain is part of the method and not data. A point the laboratory did not
/// use is an absent element, and each element is absent or present on its own.
/// Evaluation is `noexcept` throughout and allocates nothing: a `std::vector`
/// allocating inside it would turn `bad_alloc` into `std::terminate`.
///
/// **A failure names its element.** A series evaluates to
/// `EvaluatedSeries<Rep, N>`: every element in the coherent SI unit, or a
/// `SeriesFailure` carrying the arithmetic error and, when the failure belongs
/// to one element, that element's zero-based position. There is no partial
/// series -- a series with one wrong element is not a series of right ones --
/// and there is no throwing spelling, which would have to drop the position.
///
/// This header opens the family with its one leaf, `series<Q, N>`, and the two
/// entry points: `checked_evaluate_series_si<Rep>`, the representation-agnostic
/// core, and `checked_evaluate_series<Result>`, the auditable one, which
/// returns a `SeriesOutcome` in `Result`'s own unit.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/sink.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

/// A named series input: the quantity is the key the environment is asked
/// with, and `N` the length it must have been supplied with.
///
/// Empty, like `VarNode`: its whole shape is in the type.
template <Described Q, std::size_t N>
struct SeriesVarNode: SeriesNodeBase
{
    static_assert(DescribesConsistentDimension<Q>,
                  "formula: this quantity describes a dimension its own unit does not measure, so "
                  "no formula containing it can be trusted; the quantity appears in this "
                  "diagnostic as the template argument of SeriesVarNode");

    /// The quantity this node names -- the key an `Environment` is asked with.
    using quantity = Q;

    /// The dimension of each element: the one `Q` describes.
    static constexpr Dimension dimension = Describe<Q>::dimension;

    /// How many elements the series has.
    static constexpr std::size_t length = N;
};

/// The spelling of a series in a formula: `series<Retained, 5>`.
///
/// `inline` for `var`'s reason: it is what guarantees the whole program one
/// object per specialisation. `series_cross_tu` uses it from two translation
/// units.
template <Described Q, std::size_t N>
inline constexpr SeriesVarNode<Q, N> series {};

/// An evaluated series in the coherent SI unit of its dimension: one value per
/// element, each absent when the element was never measured.
template <typename Rep, std::size_t N>
struct SeriesValue
{
    /// The elements, in the series' own order.
    ///
    /// No `{}` initialiser, and the evaluators below default-initialise a
    /// `SeriesValue` rather than writing `{}`: an empty `std::optional` either
    /// way, but value-initialising an array of class type makes cl 19.51
    /// instantiate a compiler-internal `__builtin_array_init_helper` that
    /// declares an `i`, which hides a consumer's global of that name (C4459,
    /// an error under `/WX`). `consumer_globals_tests.cpp` found it here and on
    /// an array of `Measured` in `checked_evaluate_series`, which is
    /// default-initialised for the same reason. Not seen on clang or g++.
    std::array<std::optional<Rep>, N> elements;

    /// Element-wise equality.
    [[nodiscard]] constexpr bool operator==(SeriesValue const&) const noexcept = default;
};

/// Why a series could not be evaluated, and where.
struct SeriesFailure
{
    /// What went wrong.
    ArithmeticError error;

    /// The zero-based position, in the series the failing step produces, of
    /// the element it went wrong at -- or nothing, when the failure belongs to
    /// no single element (a reduction's overflow, say). Never a stand-in
    /// position for such a failure: element 0 would name a real element that
    /// did nothing wrong.
    ///
    /// Zero-based here, as every position in this library's API is. Every
    /// text the library writes -- a trace, a rendering, a message -- shows a
    /// position one-based.
    ///
    /// A series variable can only fail at an element, so nothing in this
    /// header produces the empty shape yet.
    std::optional<std::size_t> element;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SeriesFailure const&) const noexcept = default;
};

/// The result of evaluating a series: every element (each possibly absent), or
/// the one failure that stopped it. Absence stays per element; an arithmetic
/// error fails the whole series.
template <typename Rep, std::size_t N>
using EvaluatedSeries = std::expected<SeriesValue<Rep, N>, SeriesFailure>;

namespace detail
{
    /// Whether @p Sink wants to hear about the series node @p S: true when it
    /// defines **both** `series_entered(node)` and `series_produced(node,
    /// result)`, and false otherwise -- a sink defining only one is told
    /// nothing, rather than told half and left with an `entered` it will never
    /// see matched. The shape `evaluate_method` uses for `variant_entered` and
    /// `variant_produced` (`sink.hpp`), for the same reason. `NullSink`
    /// defines neither and pays nothing, and a sink written before series
    /// existed, whose `entered` and `produced` are constrained on `Node`,
    /// keeps compiling and is told nothing.
    template <typename Sink, typename S, typename Rep>
    concept HearsSeries = requires(Sink sink, S const& node, EvaluatedSeries<Rep, S::length> const& evaluated) {
        sink.series_entered(node);
        sink.series_produced(node, evaluated);
    };

    /// Tells @p sink that @p node is about to be evaluated, if it asks.
    template <typename Rep, SeriesNode S, typename Sink>
    constexpr void tell_series_entered(Sink& sink, S const& node)
    {
        if constexpr (HearsSeries<Sink, S, Rep>)
            sink.series_entered(node);
    }

    /// Tells @p sink what @p node produced, if it asks.
    template <typename Rep, SeriesNode S, typename Sink>
    constexpr void tell_series_produced(Sink& sink, S const& node, EvaluatedSeries<Rep, S::length> const& evaluated)
    {
        if constexpr (HearsSeries<Sink, S, Rep>)
            sink.series_produced(node, evaluated);
    }
} // namespace detail

/// Looks the series for `Q` up in `environment` and converts each present
/// element to the coherent SI unit of its dimension.
///
/// The environment refuses, at compile time and in this library's words, a
/// quantity it holds as a single value and a series of another length than
/// `N`.
///
/// A sink hears about the series through `series_entered` and
/// `series_produced` when it defines both (`detail::HearsSeries`), never
/// through `entered` and `produced`, which are constrained on `Node`.
template <typename Rep = Rational, Described Q, std::size_t N, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, N> checked_evaluate_series_si(SeriesVarNode<Q, N> const& node,
                                                                           Env const& environment,
                                                                           Sink sink = {}) noexcept
{
    detail::tell_series_entered<Rep>(sink, node);
    EvaluatedSeries<Rep, N> const evaluated = [&]() -> EvaluatedSeries<Rep, N> {
        MeasuredSeries<Q, N> const measured = environment.template get_series<Q, N>();
        SeriesValue<Rep, N> inCoherentUnit;
        for (std::size_t at = 0; at < N; ++at)
        {
            Measured<Q> const measuredElement = measured.element(at);
            if (measuredElement.is_absent())
                continue;
            Evaluated<Rep> const elementInSi = detail::in_si<Rep>(*measuredElement.stored(), Describe<Q>::unit);
            if (!elementInSi.has_value())
                return std::unexpected { SeriesFailure { elementInSi.error(), at } };
            inCoherentUnit.elements[at] = **elementInSi;
        }
        return inCoherentUnit;
    }();
    detail::tell_series_produced<Rep>(sink, node, evaluated);
    return evaluated;
}

namespace detail
{
    /// Evaluates the series node @p node with @p sink: `detail::dispatch`'s
    /// counterpart for a `SeriesNode`.
    ///
    /// Every series node kind declares its own `checked_evaluate_series_si`
    /// in `namespace formula`, and ADL finds each at instantiation, including
    /// the ones declared after this point. There is no two-parameter fallback,
    /// unlike `dispatch`: no consumer overload of this name predates the
    /// three-parameter form, so there is nothing to keep reachable.
    template <typename Rep, typename S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSeries<Rep, std::remove_cvref_t<S>::length> dispatch_series(S const& node,
                                                                                                 Env const& environment,
                                                                                                 Sink sink) noexcept
    {
        return checked_evaluate_series_si<Rep>(node, environment, sink);
    }
} // namespace detail

/// The result of evaluating a series for quantity @p Q: `N` measurements in
/// `Q`'s declared unit, and where they came from.
///
/// One source for the whole series: a series is either computed by this
/// library or typed in by a person, never a mixture, because an override
/// replaces the whole result (`checked_evaluate_series`).
template <Described Q, std::size_t N>
class SeriesOutcome
{
  public:
    /// A series and its provenance -- the counterpart of `Outcome::value`.
    [[nodiscard]] static constexpr SeriesOutcome value(std::array<Measured<Q>, N> measurements, ValueSource source) noexcept
    {
        return SeriesOutcome { measurements, source };
    }

    /// Every element, in order.
    [[nodiscard]] constexpr std::array<Measured<Q>, N> elements() const noexcept
    {
        return _elements;
    }

    /// The element at zero-based position @p at. Past the end it is absent:
    /// never a neighbour's value, and never a zero -- but not reported
    /// either, so a caller indexes only inside `for (at = 0; at < N; ++at)`,
    /// never with a position it computed.
    [[nodiscard]] constexpr Measured<Q> element(std::size_t at) const noexcept
    {
        return at < N ? _elements[at] : Measured<Q>::absent();
    }

    /// How many elements there are -- `N`, present or not.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }

    /// Where the series came from.
    [[nodiscard]] constexpr ValueSource source() const noexcept
    {
        return _source;
    }

    /// True when a person typed this series in place of a computed one.
    [[nodiscard]] constexpr bool is_overridden() const noexcept
    {
        return _source == ValueSource::ManuallyEntered;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SeriesOutcome const&) const noexcept = default;

  private:
    constexpr SeriesOutcome(std::array<Measured<Q>, N> measurements, ValueSource source) noexcept:
        _elements { measurements },
        _source { source }
    {
    }

    std::array<Measured<Q>, N> _elements;
    ValueSource _source;
};

/// Evaluates the series @p expression for quantity @p Result, element by
/// element, and returns each element in `Result`'s declared unit.
///
/// `Result` is never deduced, for `checked_evaluate`'s reason. When the
/// environment holds an entered value for `Result`, that is returned with
/// `ValueSource::ManuallyEntered` and the expression is not evaluated; an
/// entered **single** value for `Result` is refused where it is read, because a
/// series cannot be answered with one number.
///
/// There is deliberately no throwing twin: an exception would have to drop the
/// position `SeriesFailure` carries.
template <Described Result, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<SeriesOutcome<Result, S::length>, SeriesFailure> checked_evaluate_series(
    S const& expression, Env const& environment, Sink sink = {}) noexcept
{
    static_assert(detail::RequireResultDimension<Result, S>::value);
    constexpr std::size_t seriesLength = S::length;

    // Gated, unlike `checked_evaluate`: without it g++ 13.3 and 14.2 follow
    // this one refusal with errors from the evaluation below, among them "no
    // matching function for call to ~expected()", while cl 19.51, clang-cl
    // and clang++ give one message either way. Pinned by the REJECT on
    // `evaluate_series_result_dimension_mismatch`.
    if constexpr (!(Describe<Result>::dimension == S::dimension))
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else if constexpr (Env::template is_entered<Result>)
    {
        MeasuredSeries<Result, seriesLength> const typedIn = environment.template get_series<Result, seriesLength>();
        std::array<Measured<Result>, seriesLength> measurements;
        for (std::size_t at = 0; at < seriesLength; ++at)
            measurements[at] = typedIn.element(at);
        return SeriesOutcome<Result, seriesLength>::value(measurements, ValueSource::ManuallyEntered);
    }
    else
    {
        EvaluatedSeries<Rational, seriesLength> const computed =
            detail::dispatch_series<Rational>(expression, environment, sink);
        if (!computed.has_value())
            return std::unexpected { computed.error() };

        std::array<Measured<Result>, seriesLength> inDeclaredUnit;
        for (std::size_t at = 0; at < seriesLength; ++at)
        {
            if (!computed->elements[at].has_value())
                continue;
            std::expected<Rational, ArithmeticError> const elementInResultUnit =
                checked_convert(*computed->elements[at], coherent(S::dimension), Describe<Result>::unit);
            if (!elementInResultUnit.has_value())
                return std::unexpected { SeriesFailure { elementInResultUnit.error(), at } };
            inDeclaredUnit[at] = Measured<Result> { *elementInResultUnit };
        }
        return SeriesOutcome<Result, seriesLength>::value(inDeclaredUnit, ValueSource::Derived);
    }
}

} // namespace formula
