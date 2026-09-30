// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Binning: raw observations of one quantity counted into declared classes --
/// the particles of a sample, measured one by one, counted into size classes.
///
/// **The observations are read as they were made**, `observations<Q,
/// Capacity>`, from a `MeasuredObservations<Q, Capacity>` in the environment
/// (`environment.hpp`): how many there are is data, and only the most there
/// can be is part of the type. **The classes are a `BandTable`**, validated by
/// the shipped `RequireValidBandTable` -- no gap, no overlap, each class's low
/// below its high -- and half-open like every band (`band.hpp`): an
/// observation exactly on a boundary between two classes is counted in the
/// upper one, and one equal to the last class's high bound is in none.
///
/// **An observation in no class is a miss, never dropped.** The binning fails
/// with `DomainError` at that observation's position: a count that silently
/// left out an observation the method did not define is the confident wrong
/// answer this library refuses, as a banded lookup's miss is (`lookup.hpp`).
/// The class is found by that lookup's own `detail::find_band`, so the
/// half-open rule is written once.
///
/// `binned<KeyUnit, Classes>(observations<Q, Capacity>)` is a series of
/// `Classes.size()` counts, each present and dimensionless, in the order the
/// classes are declared; `binned(...) / sum(binned(...))` is each class's
/// share. **Its failure names an observation, not a class**: the position a
/// `SeriesFailure` carries is the observation's, marked
/// `FailureSite::InputObservation`, and an operation over the counts relays
/// the failure without it (`detail::relayed_failure`), since it would name a
/// count that is not at fault.
///
/// The observations themselves are declared in `observations.hpp`, which this
/// header includes.
///
/// Everything here compares, so binning is evaluated with `Rep = Rational`
/// only, refused otherwise in this library's words, as a curve is
/// (`curve.hpp`).

#include <formula-cpp/band.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/observations.hpp>
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
#include <type_traits>

namespace formula
{

namespace detail
{
    /// Fails to compile when `binned` is given something other than raw
    /// observations. Named so the operand prints.
    template <typename T>
    struct RequireBinnedOfObservations
    {
        static_assert(ObservationsNode<T>,
                      "formula: binned counts raw observations into classes, and this is not a set of "
                      "observations; the operand appears in this diagnostic as the template argument of "
                      "RequireBinnedOfObservations -- read them with observations<Q, Capacity> and supply "
                      "them with MeasuredObservations; a series already holds one value at each point of a "
                      "domain, and a single value is one observation, not a set of them");

        static constexpr bool value = true;
    };

    /// Fails to compile when a binning declares no classes: every observation
    /// would miss. Named so the table prints.
    template <BandTable Classes>
    struct RequireBinnedClassesNotEmpty
    {
        static_assert(Classes.size() > 0,
                      "formula: this binning declares no classes; every observation would fall in none of them, "
                      "and a series of no counts has nothing to divide or trace -- the table appears in this "
                      "diagnostic as the template argument of RequireBinnedClassesNotEmpty");

        static constexpr bool value = true;
    };

    /// Fails to compile when a binning's key unit does not measure its
    /// observations' dimension. Named so the unit and the observations print.
    template <Unit KeyUnit, typename Obs>
    struct RequireBinnedKeyMatches
    {
        static_assert(KeyUnit.dimension == Obs::dimension,
                      "formula: this binning's key unit does not measure the dimension of the observations it "
                      "bins; the unit and the observations appear in this diagnostic as the template arguments "
                      "of RequireBinnedKeyMatches");

        static constexpr bool value = true;
    };

    /// Refuses, in this library's words, every representation but
    /// `Rational`, when instantiated.
    template <typename Rep>
    constexpr void require_exact_binning() noexcept
    {
        static_assert(std::is_same_v<Rep, Rational>,
                      "formula: a binning can only be evaluated with Rep = Rational -- deciding which class an "
                      "observation falls in compares it with the class bounds, and a comparison a few units in "
                      "the last place off counts it in the wrong class silently; evaluate this formula with "
                      "Rep = Rational instead (checked_evaluate_series<Result> always does)");
    }
} // namespace detail

/// Raw observations counted into declared classes: a series of
/// `Classes.size()` counts, dimensionless, in the classes' declared order.
///
/// The checks sit in the class body, for `SnapNode`'s reason: the node is a
/// public aggregate. No classes refuses once and gates the band validation
/// and the unit check off; malformed classes gate the unit check off; and
/// refused observations gate it off too.
template <Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
struct BinnedNode: SeriesNodeBase
{
    /// Whether the operand was refused already (`binned` of something that is
    /// not observations).
    static constexpr detail::RefusedFlag refused = std::is_same_v<Obs, detail::RefusedObservations>;

    /// Whether the classes are non-empty and a well-formed band table --
    /// asked of the predicates alone, never of the refusals' `value`, for
    /// `SnapNode::setIsValid`'s reason.
    static constexpr bool classesAreValid = Classes.size() > 0 && band_table_is_well_formed(Classes);

    static_assert(detail::RequireBinnedClassesNotEmpty<Classes>::value);
    static_assert(std::conditional_t<(Classes.size() > 0), RequireValidBandTable<Classes>, std::true_type>::value);
    static_assert(std::conditional_t<classesAreValid && !refused,
                                     detail::RequireBinnedKeyMatches<KeyUnit, Obs>,
                                     std::true_type>::value);

    /// The observations counted. No `{}` initialiser, deliberately: see
    /// `Corrections` (`lookup.hpp`).
    Obs source;

    /// The unit the classes are stated in, and each observation is compared
    /// in.
    static constexpr Unit unit = KeyUnit;
    /// The classes, already validated above.
    static constexpr BandTable<Classes.size()> classes = Classes;

    /// A count is a number of observations, of no dimension.
    static constexpr Dimension dimension = dim::Scalar;
    /// One count per class.
    static constexpr std::size_t length = Classes.size();
};

/// Counts @p source's observations into @p Classes, stated in @p KeyUnit:
/// `binned<unit::Millimetre, sizeClasses>(observations<Size, 50>)`.
///
/// Anything but raw observations -- a series, a single value -- is refused
/// once, in this library's words, and binned as refused observations, so
/// that nothing else is said about it.
template <Unit KeyUnit, BandTable Classes, typename Obs>
[[nodiscard]] constexpr auto binned(Obs source) noexcept
{
    static_assert(detail::RequireBinnedOfObservations<Obs>::value);
    if constexpr (ObservationsNode<Obs>)
        return BinnedNode<KeyUnit, Classes, Obs> { {}, source };
    else
        return BinnedNode<KeyUnit, Classes, detail::RefusedObservations> { {}, detail::RefusedObservations {} };
}

/// Counts the observations into the classes: the observations first, once;
/// a failure reading them is relayed at its observation. Then each
/// observation, in the order made, is converted into `KeyUnit` and counted in
/// the class holding it (`detail::find_band`); one in no class fails the
/// binning with `DomainError` at its position, and so does one whose
/// conversion overflows, with `Overflow`. Every count is present, zero for a
/// class nothing fell in.
template <typename Rep = Rational,
          Unit KeyUnit,
          BandTable Classes,
          ObservationsNode Obs,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr EvaluatedSeries<Rep, Classes.size()> checked_evaluate_series_si(
    BinnedNode<KeyUnit, Classes, Obs> const& node, Env const& environment, Sink sink = {}) noexcept
{
    constexpr std::size_t classCount = Classes.size();
    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        detail::require_exact_binning<Rep>();
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    }
    // Refused already: nothing was binned, and nothing is told.
    else if constexpr (BinnedNode<KeyUnit, Classes, Obs>::refused)
        return std::unexpected { SeriesFailure { ArithmeticError::DomainError, std::nullopt } };
    else
    {
        detail::tell_series_entered<Rep>(sink, node);
        EvaluatedSeries<Rep, classCount> const evaluated = [&]() -> EvaluatedSeries<Rep, classCount> {
            EvaluatedObservations<Rep, Obs::capacity> const observed =
                detail::evaluate_observations<Rep>(node.source, environment, sink);
            if (!observed.has_value())
                return std::unexpected { observed.error() };

            std::array<std::int64_t, classCount> tally {};
            for (std::size_t at = 0; at < observed->count; ++at)
            {
                std::expected<Rational, ArithmeticError> const inKey =
                    checked_convert(observed->elements[at], coherent(KeyUnit.dimension), KeyUnit);
                if (!inKey.has_value())
                    return std::unexpected { SeriesFailure { inKey.error(), at, FailureSite::InputObservation } };
                std::optional<std::size_t> const holding = detail::find_band<Classes>(*inKey);
                if (!holding.has_value())
                    return std::unexpected { SeriesFailure {
                        ArithmeticError::DomainError, at, FailureSite::InputObservation } };
                ++tally[*holding];
            }
            SeriesValue<Rep, classCount> counts;
            for (std::size_t classIndex = 0; classIndex < classCount; ++classIndex)
                counts.elements[classIndex] = Rational { tally[classIndex] };
            return counts;
        }();
        detail::tell_series_produced<Rep>(sink, node, evaluated);
        return evaluated;
    }
}

} // namespace formula
