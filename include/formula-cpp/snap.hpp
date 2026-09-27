// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Snapping: a computed value replaced by the nearest value a method permits
/// -- a computed screen opening by the nearest standard one.
///
/// **Nearest means plain arithmetic distance**, in the key unit. A value
/// exactly midway between two permitted values is ordinary data, not an
/// accident, so the rule that decides it is a **required** template argument,
/// `SnapTie`, with no default -- the precedent `RoundingMode` set.
///
/// **A value outside the permitted set is a miss** (`DomainError`), never the
/// nearest end: a computed value beyond the standardised set is one the method
/// never defined, and quietly snapping it to the largest permitted value is
/// the confident wrong answer this library refuses. This is the interpolating
/// lookup's rule (`lookup.hpp`), for its reason.
///
/// The permitted set is a `BreakpointTable`, validated by the shipped
/// `RequireValidBreakpointTable` -- strictly ascending, every key a number --
/// plus one refusal of its own: it must not be empty.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>

namespace formula
{

/// Which permitted value a value exactly midway between two of them snaps to.
enum class SnapTie : std::uint8_t
{
    /// The lower of the two.
    TowardLower,
    /// The higher of the two.
    TowardHigher,
};

/// `tie` in the words a trace uses: `toward lower`, `toward higher`.
[[nodiscard]] constexpr std::string_view describe(SnapTie tie) noexcept
{
    switch (tie)
    {
        case SnapTie::TowardLower:
            return "toward lower";
        case SnapTie::TowardHigher:
            return "toward higher";
    }
    return "toward an unknown side";
}

namespace detail
{
    /// Fails to compile when a snap's permitted set is empty: there is nothing
    /// to snap to, and every value would miss. Named so the table prints.
    template <BreakpointTable Permitted>
    struct RequirePermittedSetNotEmpty
    {
        static_assert(Permitted.size() > 0,
                      "formula: this snap's permitted set is empty; there is no value to snap to, so every value "
                      "would miss -- the set appears in this diagnostic as the template argument of "
                      "RequirePermittedSetNotEmpty");

        static constexpr bool value = true;
    };

    /// Fails to compile when a snap's key unit does not measure its operand's
    /// dimension. Named so the unit's dimension and the operand print.
    template <Unit KeyUnit, typename Operand>
    struct RequireSnapKeyMatches
    {
        static_assert(refused_already<Operand>() || KeyUnit.dimension == Operand::dimension,
                      "formula: this snap's key unit does not measure the dimension of the expression it snaps; the "
                      "unit's dimension and the operand appear in this diagnostic as the template arguments of "
                      "RequireSnapKeyMatches");

        static constexpr bool value = true;
    };

    /// Where a snap landed: the value, the two permitted neighbours as the set
    /// declared them (`low == high` on an exact hit), and whether the tie rule
    /// decided.
    struct SnapAnswer
    {
        /// The permitted value snapped to, in the key unit.
        Rational snapped;
        /// The two neighbours.
        Segment neighbours;
        /// True when the value sat exactly midway and the tie rule chose.
        bool tieBroken;
    };

    /// Snaps @p key, in the key unit, to the nearest value of @p Permitted,
    /// or reports a miss (`DomainError`) below the first or above the last.
    /// **The one answer**: evaluation and trace both ask it, so the derivation
    /// cannot come to disagree with the number it derives.
    ///
    /// Where the value sits is `locate_key`'s (`lookup.hpp`) -- the one scan
    /// of ascending keys, shared with an interpolating lookup and a curve:
    /// equality first, so a value on a permitted one is an exact hit, never a
    /// tie between it and a neighbour, and a miss off either end. Only the
    /// choice between the two neighbours is the snap's own.
    template <BreakpointTable Permitted, SnapTie Tie>
    [[nodiscard]] constexpr std::expected<SnapAnswer, ArithmeticError> locate_and_snap(Rational key) noexcept
    {
        // A value that does not reduce is unreachable for a set that passed
        // its validation; `locate_key` reports it as a miss anyway.
        auto const keyAt = [](std::size_t pointIndex) {
            return Rational::make(Permitted[pointIndex].numerator, Permitted[pointIndex].denominator);
        };
        std::expected<KeyPosition, ArithmeticError> const located = locate_key(Permitted.size(), keyAt, key);
        if (!located.has_value())
            return std::unexpected { located.error() };

        // Both reduced a moment ago, inside the scan.
        std::expected<Rational, ArithmeticError> const previous = keyAt(located->low);
        std::expected<Rational, ArithmeticError> const rowKey = keyAt(located->high);
        if (!previous.has_value() || !rowKey.has_value())
            return std::unexpected { ArithmeticError::DomainError };
        if (located->low == located->high)
            return SnapAnswer { *rowKey, Segment { Permitted[located->low], Permitted[located->low] }, false };

        std::expected<Rational, ArithmeticError> const belowDistance = checked_sub(key, *previous);
        if (!belowDistance.has_value())
            return std::unexpected { belowDistance.error() };
        std::expected<Rational, ArithmeticError> const aboveDistance = checked_sub(*rowKey, key);
        if (!aboveDistance.has_value())
            return std::unexpected { aboveDistance.error() };

        Segment const neighbours { Permitted[located->low], Permitted[located->high] };
        if (*belowDistance < *aboveDistance)
            return SnapAnswer { *previous, neighbours, false };
        if (*aboveDistance < *belowDistance)
            return SnapAnswer { *rowKey, neighbours, false };
        return SnapAnswer { Tie == SnapTie::TowardLower ? *previous : *rowKey, neighbours, true };
    }
} // namespace detail

/// @p Operand replaced by the nearest value of @p Permitted, stated in
/// @p KeyUnit, a value exactly midway going as @p Tie says.
///
/// The checks sit in the class body, for `InterpolatingLookupNode`'s reason:
/// the node is a public aggregate, and only a class-body assert refuses one
/// declared without the factory. An empty set refuses once, and gates the
/// breakpoint validation and the unit check off; a malformed set gates the
/// unit check off.
template <Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
struct SnapNode: NodeBase
{
    /// Whether the permitted set is non-empty and a well-formed breakpoint
    /// table -- asked of the predicates alone, never of the refusals' `value`,
    /// which clang++ cannot read once their own assert has failed and would
    /// report as a second error.
    static constexpr bool setIsValid = Permitted.size() > 0 && breakpoint_table_is_well_formed(Permitted);

    static_assert(detail::RequirePermittedSetNotEmpty<Permitted>::value);
    static_assert(std::conditional_t<(Permitted.size() > 0), RequireValidBreakpointTable<Permitted>, std::true_type>::value);
    static_assert(std::conditional_t<setIsValid, detail::RequireSnapKeyMatches<KeyUnit, Operand>, std::true_type>::value);

    /// The expression whose value is snapped. No `{}` initialiser,
    /// deliberately: see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// The unit the permitted values are stated in, the value is compared in,
    /// and the result is stated in.
    static constexpr Unit unit = KeyUnit;
    /// The permitted values, already validated above.
    static constexpr BreakpointTable<Permitted.size()> permitted = Permitted;
    /// Which way a value exactly midway goes.
    static constexpr SnapTie tie = Tie;
    /// A snapped value keeps its dimension: the key unit's.
    static constexpr Dimension dimension = KeyUnit.dimension;
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr bool refused = detail::refused_already<Operand>();
};

/// `snapped<KeyUnit, Permitted, Tie>(operand)`: @p operand snapped to the
/// nearest value of `Permitted`, in `KeyUnit`. The tie rule has no default.
template <Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
[[nodiscard]] constexpr SnapNode<KeyUnit, Permitted, Tie, Operand> snapped(Operand snappedOperand) noexcept
{
    return SnapNode<KeyUnit, Permitted, Tie, Operand> { {}, snappedOperand };
}

/// Evaluates the operand, converts it into `KeyUnit`, and snaps it
/// (`detail::locate_and_snap`). Absence and a failed operand pass through; a
/// value outside the set is `DomainError`. `Rational` only: deciding a tie
/// needs exact comparison.
template <typename Rep = Rational,
          Unit KeyUnit,
          BreakpointTable Permitted,
          SnapTie Tie,
          Node Operand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SnapNode<KeyUnit, Permitted, Tie, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedOperand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!evaluatedOperand.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedOperand.error());
    }
    if (!evaluatedOperand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        static_assert(sizeof(Rep) == 0,
                      "formula: a snap can only be evaluated with Rep = Rational -- deciding which permitted value "
                      "is nearest, and whether a value sits exactly midway, needs exact comparison; evaluate this "
                      "formula with Rep = Rational instead (checked_evaluate<Result> always does)");
        return std::unexpected { ArithmeticError::DomainError };
    }
    else
    {
        std::expected<Rational, ArithmeticError> const valueInKey =
            checked_convert(**evaluatedOperand, coherent(KeyUnit.dimension), KeyUnit);
        if (!valueInKey.has_value())
        {
            return detail::report_failure<Rep>(node, sink, valueInKey.error());
        }
        std::expected<detail::SnapAnswer, ArithmeticError> const answered =
            detail::locate_and_snap<Permitted, Tie>(*valueInKey);
        if (!answered.has_value())
        {
            return detail::report_failure<Rep>(node, sink, answered.error());
        }
        Evaluated<Rep> const evaluated = detail::in_si<Rep>(answered->snapped, KeyUnit);
        sink.produced(node, evaluated);
        return evaluated;
    }
}

} // namespace formula
