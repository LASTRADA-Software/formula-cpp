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
#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>

#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// @p F of @p argument, rounded to @p places decimal places under @p roundingMode -- the correctly
    /// rounded decimal of the true value, rational or not. The decision, in order: a logarithm of zero or
    /// below is `DomainError`; places outside -18...18 are `Overflow`, as for `checked_round`; a special
    /// point -- the only values that can tie -- goes to `checked_round`; the exponential of more than 44
    /// is `Overflow` (every rounding of exp 44 exceeds `Rational::Int`), and of less than -43 is below a
    /// quarter of the last kept unit at any places accepted, so 0, or one unit under `Ceiling` and
    /// `AwayFromZero`; everything else is the kernel's enclosure, rounded by `decide_rounding`, which
    /// answers `Overflow` when the kept integer does not fit and when the two ends round differently.
    /// The special points are `RepFunctions<Rational>`'s, through `transcendental_of`: their value, and
    /// `Inexact` elsewhere.
    template <Transcendental F>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> rounded_transcendental(
        Rational argument, DecimalPlaces places, RoundingMode roundingMode) noexcept
    {
        if constexpr (F != Transcendental::Exponential)
        {
            if (argument.sign() <= 0)
                return std::unexpected { ArithmeticError::DomainError };
        }
        if (places.value > 18 || places.value < -18)
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
            if (argument > Rational { 44 })
                return std::unexpected { ArithmeticError::Overflow };
            if (argument < Rational { -43 })
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
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_ln(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::NaturalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// The decimal logarithm of `operand`, rounded exactly to `Places` decimal places.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_log10(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::DecimalLogarithm, Places, Mode, Operand> { {}, operand };
}

/// The exponential of `operand`, rounded exactly to `Places` decimal places.
template <DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] constexpr auto rounded_exp(Operand operand) noexcept
{
    return RoundedTranscendentalNode<Transcendental::Exponential, Places, Mode, Operand> { {}, operand };
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
