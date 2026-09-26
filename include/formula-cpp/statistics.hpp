// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Sample statistics: the count and the mean of a sample, each one value, and
/// so a `Node` -- the bridge from repeated determinations back to a number, as
/// `sum` is for a series.
///
/// **A sample** (`SampleSource`) is any source of repeated determinations of
/// one quantity. In this phase that is a phase 12 series, `series<Q, N>` or
/// any expression over one: a series expression is a sample (T14), so
/// `sample_mean(series<A, 3> / series<B, 3>)` is the mean of the per-element
/// ratios.
///
/// **Absence is strict** (T2). One absent determination makes every
/// statistic of the sample absent -- the count included, which is neither N
/// nor N - 1 but unknown. A mean of the determinations that happen to be
/// present is exactly the wrong number: "a formula with one missing input has
/// no answer" (`measured.hpp`, `combine`).
///
/// **A single value is not a sample.** `sample_mean(var<M>)` is refused in
/// one message of this library's, not in the concept-failure noise of an
/// overload set.
///
/// Each statistic is one trace step over the whole sample, whose operand is
/// the sample's own step with every element.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

/// A source of repeated determinations of one quantity: a phase 12 series,
/// or any expression over one. Its dimension is `S::dimension`, and how many
/// determinations it can hold is `detail::sample_capacity<S>` -- `N` for a
/// series.
template <typename S>
concept SampleSource = SeriesNode<S>;

namespace detail
{
    /// How many determinations @p S can hold: its length, for a series.
    template <SampleSource S>
    inline constexpr std::size_t sample_capacity = std::remove_cvref_t<S>::length;

    /// A sample, evaluated: its present values in the coherent SI unit, each
    /// one's zero-based position in the sample as entered, and how many there
    /// are. The first `count` entries of each array are meaningful.
    ///
    /// No `{}` initialisers: see `SeriesValue` (`series.hpp`) for why an array
    /// of class type is default-initialised here rather than
    /// value-initialised.
    template <typename Rep, std::size_t C>
    struct SampleValue
    {
        /// The values, in the order they were entered.
        std::array<Rep, C> values;
        /// Each value's position in the sample as entered.
        std::array<std::size_t, C> positions;
        /// How many of `values` and `positions` hold a determination.
        std::size_t count;
    };
} // namespace detail

/// The result of evaluating a sample: its values, absent as a whole when the
/// sample is (T2), or the failure that stopped it -- phase 12's
/// `SeriesFailure`, whose `element` is empty when the failure belongs to no
/// determination.
template <typename Rep, std::size_t C>
using EvaluatedSample = std::expected<std::optional<detail::SampleValue<Rep, C>>, SeriesFailure>;

namespace detail
{
    /// Evaluates the sample @p node: `dispatch_series`, plus T2's strict
    /// absence -- one absent element and the whole sample is absent. A
    /// failure keeps its position only when that is one of the sample's own
    /// determinations (`relayed_failure`).
    template <typename Rep, SampleSource S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSample<Rep, sample_capacity<S>> dispatch_sample(S const& node,
                                                                                     Env const& environment,
                                                                                     Sink sink) noexcept
    {
        constexpr std::size_t sampleCapacity = sample_capacity<S>;
        EvaluatedSeries<Rep, sampleCapacity> const evaluated = dispatch_series<Rep>(node, environment, sink);
        if (!evaluated.has_value())
            return std::unexpected { relayed_failure(evaluated) };
        SampleValue<Rep, sampleCapacity> sampled;
        sampled.count = 0;
        for (std::size_t at = 0; at < sampleCapacity; ++at)
        {
            if (!evaluated->elements[at].has_value())
                return std::optional<SampleValue<Rep, sampleCapacity>> {};
            sampled.values[at] = *evaluated->elements[at];
            sampled.positions[at] = at;
            ++sampled.count;
        }
        return std::optional<SampleValue<Rep, sampleCapacity>> { sampled };
    }

    /// Fails to compile when a sample statistic is given a single value.
    /// Named so the operand prints.
    template <typename Operand>
    struct RequireSampleSource
    {
        static_assert(SampleSource<Operand>,
                      "formula: a sample statistic needs a sample (a series or observations), not a single value; "
                      "the operand appears in this diagnostic as the template argument of RequireSampleSource -- "
                      "read a quantity determined several times with series<Q, N>");

        static constexpr bool value = true;
    };
} // namespace detail

/// How many determinations a sample holds: a bare number. Absent when any
/// determination is (T2) -- not N, and not N - 1.
template <SampleSource S>
struct SampleCountNode: NodeBase
{
    /// The sample counted. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S sample;

    /// A count is a bare number.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether the sample was refused -- see `detail::refused_already`.
    static constexpr bool refused = detail::refused_already<S>();
};

/// The mean of a sample: the total of its determinations over their count,
/// in their dimension. Absent when any determination is (T2).
template <SampleSource S>
struct SampleMeanNode: NodeBase
{
    /// The sample averaged. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S sample;

    /// A mean measures what its determinations measure.
    static constexpr Dimension dimension = S::dimension;
    /// Whether the sample was refused -- see `detail::refused_already`.
    static constexpr bool refused = detail::refused_already<S>();
};

/// How many determinations @p sampleSource holds: `sample_count(series<Mass, 6>)`.
template <SampleSource S>
[[nodiscard]] constexpr auto sample_count(S sampleSource) noexcept
{
    return SampleCountNode<S> { {}, sampleSource };
}

/// A single value handed to `sample_count`: refused in this library's words.
/// It returns a bare number, which is what a count would have been, so
/// nothing downstream refuses again. The return type is deduced, so that the
/// refusal is instantiated wherever the call is (see `cumulative`).
template <Node N>
[[nodiscard]] constexpr auto sample_count(N) noexcept
{
    static_assert(detail::RequireSampleSource<N>::value);
    return ConstantNode<unit::One> { {}, Rational { 1 } };
}

/// The mean of @p sampleSource: `sample_mean(series<Mass, 6>)`.
template <SampleSource S>
[[nodiscard]] constexpr auto sample_mean(S sampleSource) noexcept
{
    return SampleMeanNode<S> { {}, sampleSource };
}

/// A single value handed to `sample_mean`: refused in this library's words.
/// It returns the value itself, which is the one value a mean would have
/// been, so nothing downstream refuses again (see `sum`).
template <Node N>
[[nodiscard]] constexpr auto sample_mean(N singleValue) noexcept
{
    static_assert(detail::RequireSampleSource<N>::value);
    return singleValue;
}

namespace detail
{
    /// Tells @p sink the zero-based position of the determination at which a
    /// statistic over a sample failed, when it asks -- after `produced`, so
    /// that the step it amends exists. The scalar channel carries only the
    /// error (`Evaluated<Rep>`); the position survives in the trace, as
    /// phase 12's S8 has it for a series.
    template <typename Sink>
    constexpr void tell_sample_failed_at(Sink& sink, std::size_t at) noexcept
    {
        if constexpr (requires { sink.sample_failed_at(at); })
            sink.sample_failed_at(at);
    }
} // namespace detail

/// Counts the sample's determinations. A failed sample's error is relayed; an
/// absent one makes the count absent.
template <typename Rep = Rational, SampleSource S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SampleCountNode<S> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
        EvaluatedSample<Rep, detail::sample_capacity<S>> const sampled =
            detail::dispatch_sample<Rep>(node.sample, environment, sink);
        if (!sampled.has_value())
            return std::unexpected { sampled.error().error };
        if (!sampled->has_value())
            return detail::nothing<Rep>();
        std::expected<Rep, ArithmeticError> const counted =
            RepTraits<Rep>::from(Rational { static_cast<std::int64_t>((*sampled)->count) });
        if (!counted.has_value())
            return std::unexpected { counted.error() };
        return detail::present<Rep>(*counted);
    }();
    sink.produced(node, evaluated);
    return evaluated;
}

/// Averages the sample's determinations: their total, in the coherent SI
/// unit, over their count. A failed sample's error is relayed; an absent one
/// makes the mean absent; an empty one is `DivisionByZero`. A total that
/// overflows fails the mean, and the trace names the determination at which
/// it did.
template <typename Rep = Rational, SampleSource S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SampleMeanNode<S> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    std::optional<std::size_t> failedAt;
    Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
        EvaluatedSample<Rep, detail::sample_capacity<S>> const sampled =
            detail::dispatch_sample<Rep>(node.sample, environment, sink);
        if (!sampled.has_value())
            return std::unexpected { sampled.error().error };
        if (!sampled->has_value())
            return detail::nothing<Rep>();
        detail::SampleValue<Rep, detail::sample_capacity<S>> const& sampledValues = **sampled;
        if (sampledValues.count == 0)
            return std::unexpected { ArithmeticError::DivisionByZero };

        Rep runningTotal = sampledValues.values[0];
        for (std::size_t taken = 1; taken < sampledValues.count; ++taken)
        {
            std::expected<Rep, ArithmeticError> const added = RepTraits<Rep>::add(runningTotal, sampledValues.values[taken]);
            if (!added.has_value())
            {
                failedAt = sampledValues.positions[taken];
                return std::unexpected { added.error() };
            }
            runningTotal = *added;
        }
        std::expected<Rep, ArithmeticError> const counted =
            RepTraits<Rep>::from(Rational { static_cast<std::int64_t>(sampledValues.count) });
        if (!counted.has_value())
            return std::unexpected { counted.error() };
        std::expected<Rep, ArithmeticError> const averaged = RepTraits<Rep>::divide(runningTotal, *counted);
        if (!averaged.has_value())
            return std::unexpected { averaged.error() };
        return detail::present<Rep>(*averaged);
    }();
    sink.produced(node, evaluated);
    if (failedAt.has_value())
        detail::tell_sample_failed_at(sink, *failedAt);
    return evaluated;
}

} // namespace formula
