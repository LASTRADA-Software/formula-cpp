// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula back into something a person reads.
///
/// The walk brackets a child exactly when it binds more loosely than the
/// context it sits in, and raises that context by one for the right operand of
/// a subtraction or a division -- because `a - (b - c)` is not `(a - b) - c`.
/// Bracketing on equal precedence everywhere would produce `(a / b) * 100`,
/// which is noise; never bracketing on equal precedence would produce
/// `a - b - c` for `a - (b - c)`, which is wrong.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<string>`, and a consumer who only evaluates numbers must not compile a
/// string formatter in every translation unit. Include it when you want text.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

#include <string>
#include <string_view>

namespace formula
{

/// How a formula should be spelled.
enum class Dialect
{
    /// Plain text, for a terminal or a log.
    Plain,
    /// Markdown: symbols in backticks, so an underscore is not read as emphasis.
    Markdown,
    /// LaTeX: fractions, radicals and braced exponents.
    LaTeX,
};

namespace detail
{
    /// How tightly a node binds. A trait rather than a number threaded through
    /// the walk, so a new node kind is a specialisation rather than an edit to
    /// every renderer.
    enum class Precedence
    {
        Additive = 1,
        Multiplicative = 2,
        Unary = 3,
        Atom = 4,
    };

    template <typename N>
    struct PrecedenceOf
    {
        static constexpr Precedence value = Precedence::Atom;
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct PrecedenceOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr Precedence value = (Op == BinaryOperator::Add || Op == BinaryOperator::Subtract)
                                                ? Precedence::Additive
                                                : Precedence::Multiplicative;
    };

    template <UnaryOperator Op, Node Operand>
    struct PrecedenceOf<UnaryNode<Op, Operand>>
    {
        static constexpr Precedence value = Precedence::Unary;
    };

    /// A wrapper binds exactly as tightly as what it wraps, so a citation never
    /// changes where a bracket falls.
    template <Node Inner>
    struct PrecedenceOf<DocumentedNode<Inner>>
    {
        static constexpr Precedence value = PrecedenceOf<Inner>::value;
    };

    /// An exact rational as text: `4`, or `1/4` when it is not whole.
    [[nodiscard]] inline std::string number_text(Rational value)
    {
        if (value.denominator() == 1)
            return std::to_string(value.numerator());
        return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
    }
} // namespace detail

template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node);

namespace detail
{
    template <Dialect D, Node Child>
    [[nodiscard]] std::string render_operand(Child const& child, Precedence context)
    {
        std::string text = render<D>(child);
        if (static_cast<int>(PrecedenceOf<Child>::value) < static_cast<int>(context))
            return "(" + text + ")";
        return text;
    }
} // namespace detail

// One overload per node kind. Each is found by argument-dependent lookup from
// `render` below, exactly as the evaluator's overloads are.

template <Dialect D, Described Q>
[[nodiscard]] std::string render_node(VarNode<Q> const&)
{
    std::string symbol { Describe<Q>::symbol };
    if constexpr (D == Dialect::Markdown)
        return "`" + symbol + "`";
    else
        return symbol;
}

template <Dialect D, Unit U>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node)
{
    std::string const text = detail::number_text(node.number);
    constexpr Unit unit = U;
    std::string_view const symbol = view(unit.symbolText);
    return symbol.empty() ? text : text + " " + std::string { symbol };
}

template <Dialect D, UnaryOperator Op, Node Operand>
[[nodiscard]] std::string render_node(UnaryNode<Op, Operand> const& node)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary);
}

template <Dialect D, BinaryOperator Op, Node Left, Node Right>
[[nodiscard]] std::string render_node(BinaryNode<Op, Left, Right> const& node)
{
    constexpr detail::Precedence here = detail::PrecedenceOf<BinaryNode<Op, Left, Right>>::value;
    constexpr detail::Precedence rightContext = (Op == BinaryOperator::Subtract || Op == BinaryOperator::Divide)
                                                    ? static_cast<detail::Precedence>(static_cast<int>(here) + 1)
                                                    : here;

    if constexpr (D == Dialect::LaTeX && Op == BinaryOperator::Divide)
        // \frac groups both sides itself, so neither operand needs a bracket.
        return "\\frac{" + render<D>(node.lhs) + "}{" + render<D>(node.rhs) + "}";
    else
    {
        std::string const lhs = detail::render_operand<D>(node.lhs, here);
        std::string const rhs = detail::render_operand<D>(node.rhs, rightContext);

        if constexpr (Op == BinaryOperator::Add)
            return lhs + " + " + rhs;
        else if constexpr (Op == BinaryOperator::Subtract)
            return lhs + " - " + rhs;
        else if constexpr (Op == BinaryOperator::Multiply)
            return D == Dialect::LaTeX ? lhs + " \\cdot " + rhs : lhs + " * " + rhs;
        else
            return lhs + " / " + rhs;
    }
}

template <Dialect D, int Exponent, Node Operand>
[[nodiscard]] std::string render_node(PowerNode<Exponent, Operand> const& node)
{
    std::string const base = detail::render_operand<D>(node.operand, detail::Precedence::Atom);
    if constexpr (D == Dialect::LaTeX)
        return base + "^{" + std::to_string(Exponent) + "}";
    else
        return base + "^" + std::to_string(Exponent);
}

template <Dialect D, int Degree, Node Operand>
[[nodiscard]] std::string render_node(RootNode<Degree, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    if constexpr (D == Dialect::LaTeX)
    {
        if constexpr (Degree == 2)
            return "\\sqrt{" + inner + "}";
        else
            return "\\sqrt[" + std::to_string(Degree) + "]{" + inner + "}";
    }
    else
    {
        if constexpr (Degree == 2)
            return "sqrt(" + inner + ")";
        else
            return "root" + std::to_string(Degree) + "(" + inner + ")";
    }
}

template <Dialect D>
[[nodiscard]] inline std::string render_node(PiNode const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\pi";
    else
        return "pi";
}

/// A citation is documentation, not arithmetic: it does not appear in the
/// rendered formula. `document()` is what surfaces it.
template <Dialect D, Node Inner>
[[nodiscard]] std::string render_node(DocumentedNode<Inner> const& node)
{
    return render<D>(node.inner);
}

/// Renders @p node in dialect @p D.
template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render<Dialect::Plain>(node);
}

} // namespace formula
