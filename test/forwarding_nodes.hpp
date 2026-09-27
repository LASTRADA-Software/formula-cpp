// SPDX-License-Identifier: Apache-2.0
// Consumer node kinds written the one way `docs/tracing.md` says a
// consumer's own node compiles against `RecordingSink`: a three-parameter
// overload that hands the sink on to its operands' `detail::dispatch` and
// reports nothing of its own. Its operands are traced and it is not, so a
// step over one claims the operands' steps, not a step of the node's own --
// which is what a pass-through step (`documented()`, a replaced variant)
// must not mistake for the step it wraps.
#pragma once

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/sink.hpp>

namespace forwarding
{
/// `lhs - rhs`, in their dimension. Over two Celsius temperatures it is a
/// temperature *difference*: a value in kelvins that no Celsius reading's
/// affine conversion may be applied to.
template <formula::Node Lhs, formula::Node Rhs>
struct Difference: formula::NodeBase
{
    static constexpr formula::Dimension dimension = Lhs::dimension;
    Lhs lhs;
    Rhs rhs;
};

/// `lhs / rhs`, in the quotient of their dimensions -- neither operand's.
template <formula::Node Lhs, formula::Node Rhs>
struct Quotient: formula::NodeBase
{
    static constexpr formula::Dimension dimension = Lhs::dimension / Rhs::dimension;
    Lhs lhs;
    Rhs rhs;
};

/// `operand` less a fixed reference, stated in the coherent SI unit of their
/// dimension. It has one operand, of its own dimension, so a pass-through
/// step over it claims exactly one step whose dimension is right -- and over
/// a Celsius reading that step is still a reading, while this node's value is
/// a rise above the reference. Only the node's kind tells the two apart.
template <formula::Node Operand>
struct RiseAbove: formula::NodeBase
{
    static constexpr formula::Dimension dimension = Operand::dimension;
    Operand operand;
    formula::Rational reference;
};

template <typename Rep = formula::Rational, typename Operand, typename Env, typename Sink>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(RiseAbove<Operand> const& node,
                                                                    Env const& environment,
                                                                    Sink sink) noexcept
{
    formula::Evaluated<Rep> const measured = formula::detail::dispatch<Rep>(node.operand, environment, sink);
    if (!measured.has_value() || !measured->has_value())
        return measured;
    return formula::detail::present<Rep>(**measured - node.reference);
}

template <typename Rep = formula::Rational, typename Lhs, typename Rhs, typename Env, typename Sink>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(Difference<Lhs, Rhs> const& node,
                                                                    Env const& environment,
                                                                    Sink sink) noexcept
{
    formula::Evaluated<Rep> const left = formula::detail::dispatch<Rep>(node.lhs, environment, sink);
    formula::Evaluated<Rep> const right = formula::detail::dispatch<Rep>(node.rhs, environment, sink);
    if (!left.has_value() || !left->has_value())
        return left;
    if (!right.has_value() || !right->has_value())
        return right;
    return formula::detail::present<Rep>(**left - **right);
}

template <typename Rep = formula::Rational, typename Lhs, typename Rhs, typename Env, typename Sink>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(Quotient<Lhs, Rhs> const& node,
                                                                    Env const& environment,
                                                                    Sink sink) noexcept
{
    formula::Evaluated<Rep> const left = formula::detail::dispatch<Rep>(node.lhs, environment, sink);
    formula::Evaluated<Rep> const right = formula::detail::dispatch<Rep>(node.rhs, environment, sink);
    if (!left.has_value() || !left->has_value())
        return left;
    if (!right.has_value() || !right->has_value())
        return right;
    return formula::detail::present<Rep>(**left / **right);
}

template <formula::Node Lhs, formula::Node Rhs>
[[nodiscard]] constexpr Difference<Lhs, Rhs> difference(Lhs lhs, Rhs rhs) noexcept
{
    return Difference<Lhs, Rhs> { {}, lhs, rhs };
}

template <formula::Node Operand>
[[nodiscard]] constexpr RiseAbove<Operand> rise_above(Operand operand, formula::Rational reference) noexcept
{
    return RiseAbove<Operand> { {}, operand, reference };
}

template <formula::Node Lhs, formula::Node Rhs>
[[nodiscard]] constexpr Quotient<Lhs, Rhs> quotient(Lhs lhs, Rhs rhs) noexcept
{
    return Quotient<Lhs, Rhs> { {}, lhs, rhs };
}
} // namespace forwarding
