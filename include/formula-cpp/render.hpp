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

    /// Forwards the *type-level* precedence of what it wraps. That is correct
    /// as far as it goes, but it is not what makes a citation invisible to
    /// bracketing: a wrapped `ConstantNode` needs its *runtime* answer
    /// forwarded too, which this trait cannot do -- see
    /// `precedence_of(DocumentedNode<Inner> const&)` below, which is the
    /// overload that actually closes the gap.
    template <Node Inner>
    struct PrecedenceOf<DocumentedNode<Inner>>
    {
        static constexpr Precedence value = PrecedenceOf<Inner>::value;
    };

    /// The precedence a node binds at, as a runtime value rather than a type.
    ///
    /// `PrecedenceOf` alone cannot bracket a negative constant correctly: it is
    /// a trait keyed on the node's *type*, and `ConstantNode<U>` is one type
    /// whether the number it holds is `5` or `-5`. But the sign is *data*, held
    /// in the `Rational` at runtime, not something the type system ever sees.
    /// A negative constant's rendered text opens with `-`, which reads like a
    /// unary minus, so it must bracket exactly where a `UnaryNode` would --
    /// `precedence_of` gives every node kind that same runtime answer, falling
    /// back to the type-level trait everywhere the trait is already correct.
    template <Node N>
    [[nodiscard]] constexpr Precedence precedence_of(N const&) noexcept
    {
        return PrecedenceOf<N>::value;
    }

    /// A constant's rendered text is not always an atom in two data-dependent
    /// ways the type does not carry: a negative number opens with a `-` that
    /// reads like a unary minus, and a unit with a symbol renders as *two*
    /// tokens ("150 mm") rather than one -- so `pow<2>` of it would otherwise
    /// read as `150 mm^2`, i.e. `150 * mm^2`, when the tree means `(150 mm)^2`.
    /// Both cases must bracket exactly where a `UnaryNode` would.
    template <Unit U>
    [[nodiscard]] constexpr Precedence precedence_of(ConstantNode<U> const& node) noexcept
    {
        constexpr Unit unit = U;
        bool const hasUnitSymbol = !view(unit.symbolText).empty();
        return node.number.sign() < 0 || hasUnitSymbol ? Precedence::Unary : Precedence::Atom;
    }

    /// A wrapper's *type* answer forwards correctly (`PrecedenceOf` above),
    /// but bracketing is decided from the runtime answer, and the generic
    /// `precedence_of(N const&)` overload would fall back to the type-level
    /// trait -- exactly the answer that is wrong once the wrapped node is a
    /// `ConstantNode`, whose bracketing depends on data the type never sees.
    /// Forwarding the runtime call here, not just the type-level trait above,
    /// is what makes wrapping a negative or unit-bearing constant in
    /// `documented()` bracket the same way the bare constant would.
    template <Node Inner>
    [[nodiscard]] constexpr Precedence precedence_of(DocumentedNode<Inner> const& node) noexcept
    {
        return precedence_of(node.inner);
    }

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
        if (static_cast<int>(precedence_of(child)) < static_cast<int>(context))
            return "(" + text + ")";
        return text;
    }
} // namespace detail

// One overload per node kind. Each is found by argument-dependent lookup from
// `render` below, exactly as the evaluator's overloads are.

/// A variable renders as its quantity's symbol -- backtick-quoted in Markdown.
template <Dialect D, Described Q>
[[nodiscard]] std::string render_node(VarNode<Q> const&)
{
    std::string symbol { Describe<Q>::symbol };
    if constexpr (D == Dialect::Markdown)
        return "`" + symbol + "`";
    else
        return symbol;
}

/// A constant renders as its number, followed by its unit's symbol when it has one.
template <Dialect D, Unit U>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node)
{
    std::string const text = detail::number_text(node.number);
    constexpr Unit unit = U;
    std::string_view const symbol = view(unit.symbolText);
    return symbol.empty() ? text : text + " " + std::string { symbol };
}

/// A unary node renders as its operator followed by its (parenthesised if
/// necessary) operand.
template <Dialect D, UnaryOperator Op, Node Operand>
[[nodiscard]] std::string render_node(UnaryNode<Op, Operand> const& node)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary);
}

/// A binary node renders infix, bracketing each side only where its precedence
/// against the parent operator requires it -- see the file comment. Division
/// in `Dialect::LaTeX` renders as `\frac{}{}` instead, which needs no brackets
/// at all because the fraction bar already groups both sides.
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

/// A power renders as its base with the exponent superscript -- braced in LaTeX.
template <Dialect D, int Exponent, Node Operand>
[[nodiscard]] std::string render_node(PowerNode<Exponent, Operand> const& node)
{
    std::string const base = detail::render_operand<D>(node.operand, detail::Precedence::Atom);
    if constexpr (D == Dialect::LaTeX)
        return base + "^{" + std::to_string(Exponent) + "}";
    else
        return base + "^" + std::to_string(Exponent);
}

/// A root renders as `\sqrt{}` (or `\sqrt[n]{}` for a degree other than 2) in
/// LaTeX, and as `sqrt(...)` / `rootN(...)` in every other dialect.
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

/// Pi renders as `\pi` in LaTeX, and as `pi` in every other dialect.
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
