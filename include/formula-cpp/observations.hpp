// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Raw observations: as many values of one quantity as were observed, read
/// with `observations<Q, Capacity>` from a `MeasuredObservations<Q, Capacity>`
/// in the environment (`environment.hpp`). How many there are is data; only
/// the most there can be is part of the type.
///
/// Read by a binning (`binning.hpp`), which counts them into classes, and by
/// the sample statistics (`statistics.hpp`), which summarise them, and, as an
/// input of an opaque operation (`opaque.hpp`), by a fit. A header of
/// their own, so that a reader of observations need not include the classes,
/// lookups and bands a binning brings.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <type_traits>

namespace formula
{

/// The base every observations node derives from: an expression whose value
/// is as many values of one quantity as were observed. Neither a `Node`,
/// which promises one value, nor a `SeriesNode`, which promises one at each
/// point of a domain.
struct ObservationsNodeBase
{
};

/// Anything that holds raw observations: what a binning, a sample statistic and
/// an opaque operation's `InputShape::Observations` input read.
template <typename T>
concept ObservationsNode = std::derived_from<std::remove_cvref_t<T>, ObservationsNodeBase>;

/// A named set of raw observations: the quantity is the key the environment
/// is asked with, and `Capacity` the capacity they must have been supplied
/// with.
///
/// Empty, like `SeriesVarNode`: its whole shape is in the type.
template <Described Q, std::size_t Capacity>
struct ObservationsVarNode: ObservationsNodeBase
{
    static_assert(DescribesConsistentDimension<Q>,
                  "formula: this quantity describes a dimension its own unit does not measure, so "
                  "no formula containing it can be trusted; the quantity appears in this "
                  "diagnostic as the template argument of ObservationsVarNode");
    static_assert(detail::RequireNamedScaledScalar<Describe<Q>::unit>::value);

    /// The quantity this node names -- the key an `Environment` is asked with.
    using quantity = Q;

    /// The dimension of each observation: the one `Q` describes.
    static constexpr Dimension dimension = Describe<Q>::dimension;

    /// The most observations there can be.
    static constexpr std::size_t capacity = Capacity;
};

/// The spelling of raw observations in a formula: `observations<Size, 50>`.
///
/// `inline` for `var`'s reason: it guarantees the whole program one object
/// per specialisation.
template <Described Q, std::size_t Capacity>
inline constexpr ObservationsVarNode<Q, Capacity> observations {};

/// Raw observations read and converted: the first `count` of `elements`, each
/// in the coherent unit of its dimension. Default-initialised: no
/// observations.
template <typename Rep, std::size_t Capacity>
struct ObservationsValue
{
    /// The observations, in the order made; past `count` unused. No `{}`
    /// initialiser, for `SeriesValue::elements`'s reason: value-initialising
    /// an array of class type makes cl declare an `i` that hides a
    /// consumer's global.
    std::array<Rep, Capacity> elements;

    /// How many observations were made.
    std::size_t count = 0;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(ObservationsValue const&) const noexcept = default;
};

/// The result of reading raw observations: every one, or the failure that
/// stopped it, at that observation's position.
template <typename Rep, std::size_t Capacity>
using EvaluatedObservations = std::expected<ObservationsValue<Rep, Capacity>, SeriesFailure>;

namespace detail
{
    /// Raw observations that were refused already: what `binned` bins, and a
    /// fit reads, in place of an operand that is not observations, so that
    /// nothing downstream adds a message to the refusal.
    struct RefusedObservations: ObservationsNodeBase
    {
        /// No dimension to check against: the key check is off for it.
        static constexpr Dimension dimension {};
        /// None.
        static constexpr std::size_t capacity = 0;
        /// Refused already (`detail::refused_already`): a call that reads
        /// these in place of observations it refused asks nothing more.
        static constexpr RefusedFlag refused = true;
    };

    /// Whether @p Sink wants to hear about raw observations: true when it
    /// defines `observations_produced(node, result)`.
    template <typename Sink, typename O, typename Rep>
    concept HearsObservations =
        requires(Sink sink, O const& node, EvaluatedObservations<Rep, O::capacity> const& evaluated) {
            sink.observations_produced(node, evaluated);
        };

    /// Reads @p node's observations from @p environment into the coherent
    /// unit, and tells @p sink what was read, if it asks. A conversion that
    /// overflows fails at that observation.
    template <typename Rep, Described Q, std::size_t Capacity, typename Env, typename Sink>
    [[nodiscard]] constexpr EvaluatedObservations<Rep, Capacity> evaluate_observations(
        ObservationsVarNode<Q, Capacity> const& node, Env const& environment, Sink& sink) noexcept
    {
        EvaluatedObservations<Rep, Capacity> const evaluated = [&]() -> EvaluatedObservations<Rep, Capacity> {
            MeasuredObservations<Q, Capacity> const observed = environment.template get_observations<Q, Capacity>();
            ObservationsValue<Rep, Capacity> inCoherentUnit;
            for (std::size_t at = 0; at < observed.size(); ++at)
            {
                Measured<Q> const made = observed.observation(at);
                Evaluated<Rep> const observationInSi = in_si<Rep>(*made.stored(), Describe<Q>::unit);
                if (!observationInSi.has_value())
                    return std::unexpected { SeriesFailure { observationInSi.error(), at, FailureSite::InputObservation } };
                inCoherentUnit.elements[at] = **observationInSi;
            }
            // Every place past the count is written too, with zero: copying
            // an unwritten `double` would read an indeterminate value, which
            // no constant evaluation accepts.
            for (std::size_t at = observed.size(); at < Capacity; ++at)
                inCoherentUnit.elements[at] = Rep {};
            inCoherentUnit.count = observed.size();
            return inCoherentUnit;
        }();
        if constexpr (HearsObservations<Sink, ObservationsVarNode<Q, Capacity>, Rep>)
            sink.observations_produced(node, evaluated);
        return evaluated;
    }
} // namespace detail

} // namespace formula
