// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A logarithm or an exponential rounded exactly to declared decimal places.
///
/// Under `Rational`, `ln(x)`, `log10(x)` and `exp(x)` answer only where the value is rational -- ln 1,
/// log10 10^k, exp 0 -- and are `ArithmeticError::Inexact` elsewhere, as `sqrt(2)` is. A method that
/// reports a logarithm "to 0.0001" states an exact number all the same: the decimal the true, irrational
/// value rounds to. This header computes that number with integer arithmetic
/// (`detail/transcendental.hpp`), never holding an approximation of the value.
///
/// `rounded_ln<Places, Mode>(x)` is one fused node rather than a `RoundNode` around a
/// `TranscendentalNode`, for the reasons `rounded_root.hpp` gives for `rounded_sqrt`: its trace step shows
/// the argument and the rounded result, both exact, and `rounded<...>(ln(x))` keeps meaning an exact
/// logarithm, which fails where there is none.
///
/// There is no unit: the argument and the result are pure numbers, rounded as they are.

#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/yields.hpp>

#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// The largest argument the exponential's kernel takes, 88.7: below 128 ln 2 = 88.72..., which bounds
    /// its reduction (`detail/transcendental.hpp`), and above ln(2^127) = 88.03..., past which e^x leaves the
    /// largest `Rational` at every places. Above it the answer is `Overflow` without the kernel.
    inline constexpr Rational ExponentialArgumentCap { 887, 10 };

    /// The smallest argument the exponential's kernel takes, -89: e^-89 = 2.2 * 10^-39 is below a quarter of
    /// 10^-38, the finest last kept unit (`MaximumDecimalPlaces`), so below it e^x is below a quarter of the
    /// last kept unit at every places accepted and its rounding is decided without the kernel. 129 ln 2 >
    /// 89 bounds the kernel's reduction (`detail/transcendental.hpp`).
    inline constexpr Rational ExponentialArgumentFloor { -89 };

    /// @p F of @p argument, rounded to @p places decimal places under @p roundingMode -- the correctly
    /// rounded decimal of the true value, rational or not. The decision, in order: a logarithm of zero or
    /// below is `DomainError`; places outside -`MaximumDecimalPlaces`...`MaximumDecimalPlaces` are
    /// `Overflow`, as for `checked_round`; a special point -- the only values that can tie -- goes to
    /// `checked_round`; the exponential of more than 887/10 is `Overflow` (`ExponentialArgumentCap`: no such
    /// value fits a `Rational`), and of less than -89 (`ExponentialArgumentFloor`) is below a quarter of the
    /// last kept unit at any places accepted, so 0, or one unit under `Ceiling` and `AwayFromZero`;
    /// everything else is the kernel's enclosure, rounded by `decide_rounding`, which answers `Overflow`
    /// when the kept integer does not fit -- e^88.5, at every places -- and when the two ends round
    /// differently. A logarithm's two ends are at least 1.6 * 10^-37 apart, more than one unit at 37 places,
    /// so a logarithm the kernel computes is always `Overflow` at 37 or 38 places; its exact points -- ln 1,
    /// log10 10^k -- are answered without the kernel, wherever their value fits: ln 1 at every places, log10 10
    /// and log10 1/10 at 38, log10 10^k for |k| <= 17 at 37. The kernel takes every argument a
    /// `Rational` holds. The special points are
    /// `RepFunctions<Rational>`'s, through `transcendental_of`: their value, and `Inexact` elsewhere.
    template <Transcendental F>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_transcendental(
        Rational argument, DecimalPlaces places, RoundingMode roundingMode) noexcept
    {
        if constexpr (F != Transcendental::Exponential)
        {
            if (argument.sign() <= 0)
                return std::unexpected { ArithmeticError::DomainError };
        }
        if (places.value > MaximumDecimalPlaces || places.value < -MaximumDecimalPlaces)
            return std::unexpected { ArithmeticError::Overflow };

        std::expected<Rational, ArithmeticError> const exact = transcendental_of<F, Rational>(argument);
        if (exact.has_value())
            return checked_round(*exact, places, roundingMode);
        if (exact.error() != ArithmeticError::Inexact)
            return std::unexpected { exact.error() };

        std::optional<Enclosure> enclosure;
        if constexpr (F == Transcendental::NaturalLogarithm)
            enclosure = natural_log_enclosure(argument);
        else if constexpr (F == Transcendental::DecimalLogarithm)
            enclosure = decimal_log_enclosure(argument);
        else
        {
            if (argument > ExponentialArgumentCap)
                return std::unexpected { ArithmeticError::Overflow };
            if (argument < ExponentialArgumentFloor)
            {
                if (roundingMode == RoundingMode::Ceiling || roundingMode == RoundingMode::AwayFromZero)
                    return Rational::from_decimal(1, -places.value);
                return Rational {};
            }
            enclosure = exponential_enclosure(argument);
        }
        if (!enclosure.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        return decide_rounding(enclosure->lower, enclosure->upper, places, roundingMode);
    }
} // namespace detail

/// @p F -- ln, log10 or exp -- of @p Operand, a dimensionless expression, rounded to @p Places decimal
/// places under @p Mode: exactly, whether the value is rational or not.
template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
struct RoundedTranscendentalNode: NodeBase
{
    static_assert(detail::RequireDimensionlessArgument<Operand>::value);

    /// The argument: a bare number.
    ///
    /// Named `operand`, as `ConstantRewriteOperand` (`overlay.hpp`) reads it, and deliberately no `{}`
    /// default member initialiser -- see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// Which function this is.
    static constexpr Transcendental function = F;
    /// How many decimal places to keep; the trace reads it by name, as it reads `RoundNode`'s.
    static constexpr DecimalPlaces places = Places;
    /// Which way to go. The three half modes differ only on a tie, and only a special point -- ln 1,
    /// log10 10^k, exp 0 -- can tie. No `unit`: the value is a pure number, and the trace shows it in the
    /// coherent unit.
    static constexpr RoundingMode mode = Mode;
    /// A pure number.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

/// The natural logarithm of `operand`, a dimensionless expression, rounded exactly to `Places` decimal
/// places: `rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Count> / var<InitialCount>)`.
///
/// The integer kernel (`detail/transcendental.hpp`) takes every argument a `Rational` holds: ln (2^127 - 1)
/// is 88.029691931113054295 at 18 places, under `Floor`. A rounding the kernel cannot decide is `Overflow`:
/// so is every logarithm it computes at 37 or 38 places, where its enclosure is wider than the last kept
/// unit. ln 1 is answered exactly before the kernel is asked, at any places.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_ln(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::NaturalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// A bound formula as `rounded_ln`'s operand: the formula it holds, in its
/// place (`yields.hpp`).
template <DecimalPlaces Places, RoundingMode Mode, typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto rounded_ln(Bound boundFormula) noexcept
{
    return rounded_ln<Places, Mode>(boundFormula.expression);
}

/// The decimal logarithm of `operand`, rounded exactly to `Places` decimal places.
///
/// As for `rounded_ln`, every argument a `Rational` holds; a power of ten, 10^-38 up to 10^38, is answered
/// exactly before the kernel is asked: log10 10^30 is 30.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_log10(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::DecimalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// A bound formula as `rounded_log10`'s operand: the formula it holds, in its
/// place (`yields.hpp`).
template <DecimalPlaces Places, RoundingMode Mode, typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto rounded_log10(Bound boundFormula) noexcept
{
    return rounded_log10<Places, Mode>(boundFormula.expression);
}

/// The exponential of `operand`, rounded exactly to `Places` decimal places.
///
/// Checked in this order: an argument above 887/10 is `Overflow`, since e^x is then past the largest
/// `Rational`; one below -89 is 0, or one last kept unit under `Ceiling` and `AwayFromZero`, whatever its
/// width, so exp(-2^70) is 0; any other goes to the integer kernel (`detail/transcendental.hpp`), which
/// answers wherever the result fits the declared places -- e^45 to 18 places, e^88 to whole units -- and is
/// `Overflow` where it does not, as e^88.5 is at every places. A rounding the kernel cannot decide is
/// `Overflow` too, never a guess.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_exp(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::Exponential, Places, Mode, Operand> { {}, operand };
}

/// A bound formula as `rounded_exp`'s operand: the formula it holds, in its
/// place (`yields.hpp`).
template <DecimalPlaces Places, RoundingMode Mode, typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto rounded_exp(Bound boundFormula) noexcept
{
    return rounded_exp<Places, Mode>(boundFormula.expression);
}

/// Evaluates the argument, then rounds `F` of it. Under `Rational` this is
/// `detail::rounded_transcendental`, exact. Under any other representation it is that representation's
/// function (`RepFunctions<Rep>`) followed by its own rounding (`RepRounding<Rep>::round_in`), which for
/// `double` refuses to compile, as it does for every rounding node.
template <typename Rep = Rational,
          Transcendental F,
          DecimalPlaces Places,
          RoundingMode Mode,
          Node Operand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundedTranscendentalNode<F, Places, Mode, Operand> const& node,
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

    std::expected<Rep, ArithmeticError> roundedValue = std::unexpected { ArithmeticError::DomainError };
    if constexpr (std::is_same_v<Rep, Rational>)
        roundedValue = detail::rounded_transcendental<F>(**evaluatedOperand, Places, Mode);
    else
    {
        std::expected<Rep, ArithmeticError> const unrounded = detail::transcendental_of<F, Rep>(**evaluatedOperand);
        roundedValue = unrounded.has_value() ? RepRounding<Rep>::round_in(*unrounded, unit::One, Places, Mode)
                                             : std::expected<Rep, ArithmeticError> { std::unexpected { unrounded.error() } };
    }
    Evaluated<Rep> const evaluated = roundedValue.has_value() ? detail::present<Rep>(*roundedValue)
                                                              : Evaluated<Rep> { std::unexpected { roundedValue.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

} // namespace formula
