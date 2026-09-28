// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Sample statistics: the count and the mean of a sample, each one value, and
/// so a `Node` -- the bridge from repeated determinations back to a number, as
/// `sum` is for a series.
///
/// **A sample** (`SampleSource`) is any source of repeated determinations of
/// one quantity. That is a phase 12 series, `series<Q, N>` or any expression
/// over one: a series expression is a sample, so
/// `sample_mean(series<A, 3> / series<B, 3>)` is the mean of the per-element
/// ratios. It is also raw observations, `observations<Q, Capacity>`, whose
/// count is known only at run time: `Capacity` is a bound, not a count, and
/// every statistic reads the observations made -- `sample_count` of six made
/// in room for eight is 6, and none made is an empty sample.
///
/// **Absence is strict**. One absent determination makes every
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

#include <formula-cpp/binning.hpp>
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

namespace detail
{
    /// Whether @p T is a sample transformer -- a source of determinations that
    /// is neither a series nor a `Node`: a rejection of outliers
    /// (`RejectionNode`, `rejection.hpp`), which specialises this. Its
    /// determinations are evaluated by a `checked_evaluate_sample_si` found
    /// by argument-dependent lookup.
    template <typename T>
    inline constexpr bool is_sample_transformer = false;

    /// Whether @p T is raw observations of one quantity
    /// (`ObservationsVarNode`, `binning.hpp`), a sample whose count is known
    /// only at run time.
    template <typename T>
    inline constexpr bool is_observations_sample = false;

    template <Described Q, std::size_t Capacity>
    inline constexpr bool is_observations_sample<ObservationsVarNode<Q, Capacity>> = true;
} // namespace detail

/// A source of repeated determinations of one quantity: a phase 12 series,
/// any expression over one, raw observations (`observations<Q, Capacity>`),
/// or a rejection of outliers from any of these (`without_outliers`,
/// `rejection.hpp`). Its dimension is `S::dimension`, and how many
/// determinations it can hold is `detail::sample_capacity<S>` -- `N` for a
/// series, `Capacity` for observations; how many it does hold is known when
/// it is evaluated.
template <typename S>
concept SampleSource = SeriesNode<S> || detail::is_observations_sample<std::remove_cvref_t<S>>
                       || detail::is_sample_transformer<std::remove_cvref_t<S>>;

namespace detail
{
    /// How many determinations @p S can hold: its length, for a series; its
    /// declared capacity, for observations and a transformer.
    template <SampleSource S>
    inline constexpr std::size_t sample_capacity = [] {
        if constexpr (requires { std::remove_cvref_t<S>::capacity; })
            return std::size_t { std::remove_cvref_t<S>::capacity };
        else
            return std::size_t { std::remove_cvref_t<S>::length };
    }();

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
/// sample is, or the failure that stopped it -- phase 12's
/// `SeriesFailure`, whose `element` is empty when the failure belongs to no
/// determination.
template <typename Rep, std::size_t C>
using EvaluatedSample = std::expected<std::optional<detail::SampleValue<Rep, C>>, SeriesFailure>;

namespace detail
{
    /// `dispatch_sample` for a series: every element, or absent as a whole.
    /// A failure keeps its position only when that is one of the sample's
    /// own determinations (`relayed_failure`).
    template <typename Rep, SeriesNode S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSample<Rep, sample_capacity<S>> dispatch_series_sample(S const& node,
                                                                                            Env const& environment,
                                                                                            Sink sink) noexcept
    {
        constexpr std::size_t sampleCapacity = sample_capacity<S>;
        EvaluatedSeries<Rep, sampleCapacity> const evaluated = dispatch_series<Rep>(node, environment, sink);
        if (!evaluated.has_value())
            return std::unexpected { relayed_failure(evaluated) };
        SampleValue<Rep, sampleCapacity> drawnSample;
        drawnSample.count = 0;
        for (std::size_t at = 0; at < sampleCapacity; ++at)
        {
            if (!evaluated->elements[at].has_value())
                return std::optional<SampleValue<Rep, sampleCapacity>> {};
            drawnSample.values[at] = *evaluated->elements[at];
            drawnSample.positions[at] = at;
            ++drawnSample.count;
        }
        return std::optional<SampleValue<Rep, sampleCapacity>> { drawnSample };
    }

    /// `dispatch_sample` for raw observations: the ones made, in the order
    /// made, and as many as were made -- never the capacity. Each is present,
    /// so the sample is never absent; a failure reading one is relayed at its
    /// position, which is the sample's own.
    template <typename Rep, Described Q, std::size_t Capacity, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSample<Rep, Capacity> dispatch_observations_sample(
        ObservationsVarNode<Q, Capacity> const& node, Env const& environment, Sink sink) noexcept
    {
        EvaluatedObservations<Rep, Capacity> const evaluated = evaluate_observations<Rep>(node, environment, sink);
        if (!evaluated.has_value())
            return std::unexpected { evaluated.error() };
        // Every place is copied -- `evaluate_observations` writes the unfilled
        // ones with zero -- each at its own position; `count` says which hold
        // a determination.
        SampleValue<Rep, Capacity> drawnSample;
        for (std::size_t at = 0; at < Capacity; ++at)
        {
            drawnSample.values[at] = evaluated->elements[at];
            drawnSample.positions[at] = at;
        }
        drawnSample.count = evaluated->count;
        return std::optional<SampleValue<Rep, Capacity>> { drawnSample };
    }

    /// Evaluates the sample @p node: a series through
    /// `dispatch_series_sample`, observations through
    /// `dispatch_observations_sample`, a transformer through the
    /// `checked_evaluate_sample_si` its header declares.
    template <typename Rep, SampleSource S, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedSample<Rep, sample_capacity<S>> dispatch_sample(S const& node,
                                                                                     Env const& environment,
                                                                                     Sink sink) noexcept
    {
        if constexpr (SeriesNode<S>)
            return dispatch_series_sample<Rep>(node, environment, sink);
        else if constexpr (is_observations_sample<std::remove_cvref_t<S>>)
            return dispatch_observations_sample<Rep>(node, environment, sink);
        else
            return checked_evaluate_sample_si<Rep>(node, environment, sink);
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
/// determination is -- not N, and not N - 1.
template <SampleSource S>
struct SampleCountNode: NodeBase
{
    /// The sample counted. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S sample;

    /// A count is a bare number.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether the sample was refused -- see `detail::refused_already`. Read
    /// by nothing yet: no check above a statistic compares what a refused
    /// sample stands in for. Kept, as `SumNode` keeps its own, so that the
    /// first one that does -- a rejection over a statistic, say -- is gated
    /// without another change here.
    static constexpr detail::RefusedFlag refused = detail::refused_already<S>();
};

/// The mean of a sample: the total of its determinations over their count,
/// in their dimension. Absent when any determination is.
template <SampleSource S>
struct SampleMeanNode: NodeBase
{
    /// The sample averaged. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    S sample;

    /// A mean measures what its determinations measure.
    static constexpr Dimension dimension = S::dimension;
    /// Whether the sample was refused -- see `detail::refused_already`. Read
    /// by nothing yet: no check above a statistic compares what a refused
    /// sample stands in for. Kept, as `SumNode` keeps its own, so that the
    /// first one that does -- a rejection over a statistic, say -- is gated
    /// without another change here.
    static constexpr detail::RefusedFlag refused = detail::refused_already<S>();
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

/// The sample variance: the squared deviations from the mean, totalled and
/// divided by n - 1 -- the **sample** variance, as the name says, not the
/// population's. In the square of the determinations' dimension. Absent when
/// any determination is; fewer than two is `DomainError`.
template <SampleSource S>
struct SampleVarianceNode: NodeBase
{
    /// The sample. No `{}` initialiser, deliberately: see `Corrections`
    /// (`lookup.hpp`).
    S sample;

    /// A variance measures the square of what its determinations measure.
    static constexpr Dimension dimension = S::dimension * S::dimension;
    /// Whether the sample was refused -- see `detail::refused_already`. Read
    /// by nothing yet: no check above a statistic compares what a refused
    /// sample stands in for. Kept, as `SumNode` keeps its own, so that the
    /// first one that does -- a rejection over a statistic, say -- is gated
    /// without another change here.
    static constexpr detail::RefusedFlag refused = detail::refused_already<S>();
};

/// The range of a sample: its largest determination less its smallest, in
/// their dimension. Absent when any determination is; one determination
/// has a range of 0.
template <SampleSource S>
struct SampleRangeNode: NodeBase
{
    /// The sample. No `{}` initialiser, deliberately: see `Corrections`
    /// (`lookup.hpp`).
    S sample;

    /// A range measures what its determinations measure.
    static constexpr Dimension dimension = S::dimension;
    /// Whether the sample was refused -- see `detail::refused_already`. Read
    /// by nothing yet: no check above a statistic compares what a refused
    /// sample stands in for. Kept, as `SumNode` keeps its own, so that the
    /// first one that does -- a rejection over a statistic, say -- is gated
    /// without another change here.
    static constexpr detail::RefusedFlag refused = detail::refused_already<S>();
};

/// The sample variance of @p sampleSource: `sample_variance(series<Mass, 6>)`.
template <SampleSource S>
[[nodiscard]] constexpr auto sample_variance(S sampleSource) noexcept
{
    return SampleVarianceNode<S> { {}, sampleSource };
}

/// A single value handed to `sample_variance`: refused in this library's
/// words. It returns the value's square, which has the dimension a variance
/// would have had, so nothing downstream refuses again.
template <Node N>
[[nodiscard]] constexpr auto sample_variance(N singleValue) noexcept
{
    static_assert(detail::RequireSampleSource<N>::value);
    return singleValue * singleValue;
}

/// The range of @p sampleSource: `sample_range(series<Mass, 6>)`.
template <SampleSource S>
[[nodiscard]] constexpr auto sample_range(S sampleSource) noexcept
{
    return SampleRangeNode<S> { {}, sampleSource };
}

/// A single value handed to `sample_range`: refused in this library's words.
/// It returns the value itself, which has the dimension a range would have
/// had, so nothing downstream refuses again.
template <Node N>
[[nodiscard]] constexpr auto sample_range(N singleValue) noexcept
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
    /// it does for a series.
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
        EvaluatedSample<Rep, detail::sample_capacity<S>> const drawnSample =
            detail::dispatch_sample<Rep>(node.sample, environment, sink);
        if (!drawnSample.has_value())
            return std::unexpected { drawnSample.error().error };
        if (!drawnSample->has_value())
            return detail::nothing<Rep>();
        std::expected<Rep, ArithmeticError> const countedValue =
            RepTraits<Rep>::from(Rational { static_cast<std::int64_t>((*drawnSample)->count) });
        if (!countedValue.has_value())
            return std::unexpected { countedValue.error() };
        return detail::present<Rep>(*countedValue);
    }();
    sink.produced(node, evaluated);
    return evaluated;
}

namespace detail
{
    /// The mean of @p drawnSample's determinations, in the coherent SI unit: their
    /// total over their count. An empty sample is `DivisionByZero`. A total
    /// that overflows fails, and @p failedAt learns the position of the
    /// determination it overflowed at.
    template <typename Rep, std::size_t C>
    [[nodiscard]] constexpr std::expected<Rep, ArithmeticError> mean_of(SampleValue<Rep, C> const& drawnSample,
                                                                        std::optional<std::size_t>& failedAt) noexcept
    {
        if (drawnSample.count == 0)
            return std::unexpected { ArithmeticError::DivisionByZero };

        Rep runningTotal = drawnSample.values[0];
        for (std::size_t taken = 1; taken < drawnSample.count; ++taken)
        {
            std::expected<Rep, ArithmeticError> const added = RepTraits<Rep>::add(runningTotal, drawnSample.values[taken]);
            if (!added.has_value())
            {
                failedAt = drawnSample.positions[taken];
                return std::unexpected { added.error() };
            }
            runningTotal = *added;
        }
        std::expected<Rep, ArithmeticError> const countedValue =
            RepTraits<Rep>::from(Rational { static_cast<std::int64_t>(drawnSample.count) });
        if (!countedValue.has_value())
            return std::unexpected { countedValue.error() };
        return RepTraits<Rep>::divide(runningTotal, *countedValue);
    }

    /// The evaluation every sample statistic shares: the sample, then
    /// @p reduce over its determinations, with the sink told the node, the
    /// result, and -- when @p reduce names one -- the determination a failure
    /// arose at. A failed sample relays its error; an absent one makes the
    /// statistic absent.
    template <typename Rep, typename N, typename S, typename Env, typename Sink, typename Reduce>
    [[nodiscard]] constexpr Evaluated<Rep> evaluate_statistic(
        N const& node, S const& sampleSource, Env const& environment, Sink sink, Reduce reduce) noexcept
    {
        sink.entered(node);
        std::optional<std::size_t> failedAt;
        Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
            EvaluatedSample<Rep, sample_capacity<S>> const drawnSample =
                dispatch_sample<Rep>(sampleSource, environment, sink);
            if (!drawnSample.has_value())
                return std::unexpected { drawnSample.error().error };
            if (!drawnSample->has_value())
                return nothing<Rep>();
            std::expected<Rep, ArithmeticError> const reduced = reduce(**drawnSample, failedAt);
            if (!reduced.has_value())
                return std::unexpected { reduced.error() };
            return present<Rep>(*reduced);
        }();
        sink.produced(node, evaluated);
        if (failedAt.has_value())
            tell_sample_failed_at(sink, *failedAt);
        return evaluated;
    }
} // namespace detail

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
    return detail::evaluate_statistic<Rep>(
        node,
        node.sample,
        environment,
        sink,
        [](detail::SampleValue<Rep, detail::sample_capacity<S>> const& drawnSample, std::optional<std::size_t>& failedAt) {
            return detail::mean_of(drawnSample, failedAt);
        });
}

/// The sample variance, in **two passes**: the mean, then the squared
/// deviations from it, totalled and divided by n - 1. Fewer than two
/// determinations is `DomainError`: n - 1 is then no count of anything.
///
/// **Headroom, as measured.** The one-pass textbook
/// form, (sum x^2 - (sum x)^2 / n) / (n - 1), squares the determinations
/// themselves, and at large *magnitudes* overflows first: at 2^25 times
/// fixture A's masses, where this form holds until 2^31
/// (`statistics_tests.cpp`). At fine *resolution* it is the other way round:
/// dividing by n before squaring puts n^2 into every deviation's
/// denominator, and at 6 dp in g near 40 g with n = 6 this form overflows on
/// 423 of 1000 samples (the overflow census's draw, docs/numeric-headroom.md),
/// where the one-pass form was found holding more often.
/// Every such failure is `Overflow`, naming the determination it arose at --
/// never a wrong value.
template <typename Rep = Rational, SampleSource S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SampleVarianceNode<S> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    return detail::evaluate_statistic<Rep>(
        node,
        node.sample,
        environment,
        sink,
        [](detail::SampleValue<Rep, detail::sample_capacity<S>> const& drawnSample,
           std::optional<std::size_t>& failedAt) -> std::expected<Rep, ArithmeticError> {
            if (drawnSample.count < 2)
                return std::unexpected { ArithmeticError::DomainError };
            std::expected<Rep, ArithmeticError> const sampleMean = detail::mean_of(drawnSample, failedAt);
            if (!sampleMean.has_value())
                return sampleMean;

            std::optional<Rep> squaresTotal;
            for (std::size_t taken = 0; taken < drawnSample.count; ++taken)
            {
                std::expected<Rep, ArithmeticError> const fromMean =
                    RepTraits<Rep>::subtract(drawnSample.values[taken], *sampleMean);
                std::expected<Rep, ArithmeticError> const squared =
                    fromMean.has_value() ? RepTraits<Rep>::multiply(*fromMean, *fromMean) : fromMean;
                std::expected<Rep, ArithmeticError> const added = !squared.has_value() || !squaresTotal.has_value()
                                                                      ? squared
                                                                      : RepTraits<Rep>::add(*squaresTotal, *squared);
                if (!added.has_value())
                {
                    failedAt = drawnSample.positions[taken];
                    return added;
                }
                squaresTotal = *added;
            }
            std::expected<Rep, ArithmeticError> const freedom =
                RepTraits<Rep>::from(Rational { static_cast<std::int64_t>(drawnSample.count - 1) });
            if (!freedom.has_value())
                return freedom;
            return RepTraits<Rep>::divide(*squaresTotal, *freedom);
        });
}

namespace detail
{
    /// The largest of @p drawnSample's determinations less the smallest, found by
    /// comparing every one -- never the last less the first. **An unordered
    /// determination -- a NaN, under `Rep = double` -- is the range**, wherever
    /// it stands: comparisons with it are false, so a running minimum and
    /// maximum would skip it unless it came first, and the range would depend
    /// on the order the determinations were typed in. Never true of `Rational`.
    template <typename Rep, std::size_t C>
    [[nodiscard]] constexpr std::expected<Rep, ArithmeticError> range_of(SampleValue<Rep, C> const& drawnSample) noexcept
    {
        if (drawnSample.count == 0)
            return std::unexpected { ArithmeticError::DomainError };
        Rep lowestSeen = drawnSample.values[0];
        Rep highestSeen = drawnSample.values[0];
        for (std::size_t taken = 0; taken < drawnSample.count; ++taken)
        {
            Rep const candidateValue = drawnSample.values[taken];
            if (!(candidateValue == candidateValue))
                return candidateValue;
            if (candidateValue < lowestSeen)
                lowestSeen = candidateValue;
            if (highestSeen < candidateValue)
                highestSeen = candidateValue;
        }
        return RepTraits<Rep>::subtract(highestSeen, lowestSeen);
    }
} // namespace detail

/// The range: the largest determination less the smallest (`detail::range_of`).
/// It takes a value and makes no decision, so it works for any `Rep`.
template <typename Rep = Rational, SampleSource S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SampleRangeNode<S> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    return detail::evaluate_statistic<Rep>(node,
                                           node.sample,
                                           environment,
                                           sink,
                                           [](detail::SampleValue<Rep, detail::sample_capacity<S>> const& drawnSample,
                                              std::optional<std::size_t>&) { return detail::range_of(drawnSample); });
}

} // namespace formula
