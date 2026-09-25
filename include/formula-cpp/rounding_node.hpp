// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Rounding as a position in a formula, not as output formatting.
///
/// A method may round an input to a coarse granularity *before* it enters a
/// formula and round the result differently afterwards. Both are specified,
/// and they give different numbers, so a library that rounds only at output
/// produces wrong ones. That is why rounding is a node: it happens where the
/// formula says it happens.
///
/// Every rounding node names the **unit it rounds in**, and this is not
/// decoration. "To two decimal places" means nothing about a quantity until
/// you say two decimal places *of what*: a length rounded to two places in
/// metres and the same length rounded to two places in millimetres are
/// different numbers. The evaluator works in the coherent SI unit of each
/// dimension, so a node converts into its stated unit, rounds there, and
/// converts back.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

namespace formula
{

namespace detail
{
    /// Fails to compile when a rounding node's unit does not measure the
    /// dimension of what it wraps -- rounding a mass "to 0.1 mm" is not a
    /// rounding error, it is a category error.
    template <Unit U, typename Operand>
    struct RequireRoundingUnitMatches
    {
        static_assert(U.dimension == Operand::dimension,
                      "formula: this rounding node names a unit that does not measure the dimension of the "
                      "expression it rounds; the unit's dimension and the operand appear in this diagnostic as "
                      "the template arguments of RequireRoundingUnitMatches");

        static constexpr bool value = true;
    };
} // namespace detail

/// @p Operand rounded to @p Places decimal places of @p U, under @p Mode.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
struct RoundNode: NodeBase
{
    static_assert(detail::RequireRoundingUnitMatches<U, Operand>::value);

    /// The expression being rounded.
    Operand operand {};

    /// The unit the rounding happens in.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;
    /// Rounding changes a number, never its dimension.
    static constexpr Dimension dimension = Operand::dimension;
};

/// @p Operand rounded to @p Digits significant digits of @p U, under @p Mode.
template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
struct RoundSignificantNode: NodeBase
{
    static_assert(detail::RequireRoundingUnitMatches<U, Operand>::value);

    /// The expression being rounded.
    Operand operand {};

    /// The unit the rounding happens in.
    static constexpr Unit unit = U;
    /// How many significant digits to keep.
    static constexpr SignificantDigits digits = Digits;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;
    /// Rounding changes a number, never its dimension.
    static constexpr Dimension dimension = Operand::dimension;
};

/// `operand` rounded to `Places` decimal places of `U`:
/// `rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(var<Diameter>)`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded(Operand operand) noexcept
{
    return RoundNode<U, Places, Mode, Operand> { {}, operand };
}

/// `operand` rounded to `Digits` significant digits of `U`.
template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_to_digits(Operand operand) noexcept
{
    return RoundSignificantNode<U, Digits, Mode, Operand> { {}, operand };
}

/// Rounding per representation, including the unit conversion it needs.
///
/// A **public extension point**, for the same reason as `RepTraits` and
/// `RepFunctions`: a consumer teaching the evaluator a new representation
/// specialises all three, and specialising two of them reaches only as far as
/// the first rounding node in a formula.
///
/// Rounding lives here rather than in `RepTraits` because it needs the unit
/// conversion too, and because the conversion is exact for `Rational` and is
/// not for `double` -- a difference that belongs in a named specialisation
/// rather than inside an `if constexpr` in the evaluator.
template <typename Rep>
struct RepRounding;

/// Exact rational rounding: converts into the named unit, rounds there, and
/// converts back, all as exact multiply-then-divide steps.
template <>
struct RepRounding<Rational>
{
    /// Converts into @p unit, rounds there, and converts back -- all exactly.
    /// Fails with `DomainError` if the unit does not measure this dimension,
    /// and with `Overflow` if any step leaves `Rational`'s range.
    template <typename Parameter>
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError>
    round_in(Rational value, Unit unit, Parameter parameter, RoundingMode mode) noexcept
    {
        std::expected<Rational, ArithmeticError> const inUnit =
            checked_convert(value, coherent(unit.dimension), unit);
        if (!inUnit.has_value())
            return inUnit;

        std::expected<Rational, ArithmeticError> const roundedValue = checked_round(*inUnit, parameter, mode);
        if (!roundedValue.has_value())
            return roundedValue;

        return checked_convert(*roundedValue, unit, coherent(unit.dimension));
    }
};

/// `double` cannot honour a rounding node the way this library means "round
/// to 0.1 mm": that phrase promises an exact decimal boundary, and hitting it
/// needs two things `double` does not have. First, the conversion into the
/// named unit multiplies by that unit's magnitude, which for most units --
/// a millimetre's 1/1000 included -- is not exactly representable in binary,
/// unlike in `Rational`. Second, and more serious, deciding which side of a
/// decimal step a value falls on is exactly the operation binary
/// floating-point is unreliable at: a value a few ULPs off its intended
/// decimal boundary rounds to the wrong side of it, silently, with no
/// `ArithmeticError` to report -- and that failure mode is the entire reason
/// `rounding.hpp` is built on exact `Rational` arithmetic rather than
/// `std::round`. A rounding node exists to reproduce a spec's stated
/// precision exactly; a `double` implementation that occasionally rounds the
/// wrong way would defeat that purpose while looking, to anyone who did not
/// hand-check its edge cases, like it worked.
///
/// So this is refused at compile time instead, the same call `render_trace`
/// makes for a non-`Rational` trace and for the same reason: a formula that
/// contains a rounding node fails to compile when evaluated with
/// `Rep = double`, naming this specialisation in the diagnostic, rather than
/// silently producing a value nobody can trust. `checked_evaluate` -- the
/// entry point every rounding node test in this library uses -- always works
/// in `Rational`, so this restriction is never reached from there; it bites
/// only a caller who explicitly asks `checked_evaluate_si` for `Rep = double`
/// on a formula containing a rounding node.
template <>
struct RepRounding<double>
{
    /// Always fails to compile -- see the class comment. `Parameter` is a
    /// member-function template parameter (`DecimalPlaces` or
    /// `SignificantDigits`) rather than the enclosing specialisation's own,
    /// purely so the `static_assert` below is dependent and therefore fires
    /// only when this function is actually instantiated, not merely declared.
    template <typename Parameter>
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> round_in(double, Unit, Parameter,
                                                                                   RoundingMode) noexcept
    {
        static_assert(sizeof(Parameter) == 0,
                      "formula: a rounding node cannot be evaluated with Rep = double -- rounding to a named "
                      "decimal precision needs the exact decimal arithmetic double cannot give, which is why "
                      "RepRounding<double> refuses rather than silently rounding some values the wrong way; "
                      "evaluate this formula with Rep = Rational instead");
        return std::unexpected { ArithmeticError::DomainError };
    }
};

/// Evaluates the operand, converts into `U`, rounds, and converts back.
template <typename Rep = Rational, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand, typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundNode<U, Places, Mode, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const operand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!operand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { operand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!operand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const rounded = RepRounding<Rep>::round_in(**operand, U, Places, Mode);
    Evaluated<Rep> const result =
        rounded.has_value() ? detail::present<Rep>(*rounded) : Evaluated<Rep> { std::unexpected { rounded.error() } };
    sink.produced(node, result);
    return result;
}

/// Evaluates the operand, converts into `U`, rounds to `Digits` significant
/// digits, and converts back.
template <typename Rep = Rational, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand, typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundSignificantNode<U, Digits, Mode, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const operand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!operand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { operand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!operand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const rounded = RepRounding<Rep>::round_in(**operand, U, Digits, Mode);
    Evaluated<Rep> const result =
        rounded.has_value() ? detail::present<Rep>(*rounded) : Evaluated<Rep> { std::unexpected { rounded.error() } };
    sink.produced(node, result);
    return result;
}

} // namespace formula
