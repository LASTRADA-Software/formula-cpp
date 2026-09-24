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
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rounding_node.hpp>
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
        /// A comparison or a `when()` -- binds looser than every arithmetic
        /// operator, so either one needs a bracket wherever it sits as the
        /// operand of `+`, `-`, `*`, `/`, unary negation, or a power. Added in
        /// spec phase 8; every other rung keeps its original number.
        Conditional = 0,
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

    /// A `when()`'s rendered shape ("if ... then ... else ..." or, in LaTeX, a
    /// `\begin{cases}` block) never changes with the data it carries, unlike
    /// `ConstantNode` below -- so the type-level trait alone is enough, and no
    /// runtime `precedence_of` overload is needed to get the right answer.
    template <Predicate P, Node Then, Node Else>
    struct PrecedenceOf<WhenNode<P, Then, Else>>
    {
        static constexpr Precedence value = Precedence::Conditional;
    };

    /// A comparison, for the same reason: its rendered shape is always
    /// `<lhs> <op> <rhs>` regardless of what the operands hold. `PredicateNode`
    /// is not a `Node`, so this trait is read directly by its `render_node`
    /// overload below rather than through the `Node`-constrained
    /// `precedence_of` runtime function -- see that overload.
    template <Comparison Op, Node Left, Node Right>
    struct PrecedenceOf<PredicateNode<Op, Left, Right>>
    {
        static constexpr Precedence value = Precedence::Conditional;
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

/// Renders @p node in dialect @p D. A `PredicateNode` is not a `Node` -- see
/// `predicate.hpp` -- so it needs this second overload rather than the one
/// above; `WhenNode::render_node` calls this one to render its predicate.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(P const& node);

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

/// A rounding node renders as a function call, `round(<operand> to <places>
/// dp of <unit>)` -- braced onto a subscript in LaTeX, the same way a root's
/// degree is. Like `sqrt` and `root` above, the parentheses it always
/// produces already group its own operand, so it needs no `PrecedenceOf`
/// override: the primary template's `Atom` fallback is already the right
/// answer, and unlike `ConstantNode` nothing about that answer depends on the
/// data the node carries, so no runtime `precedence_of` overload either.
///
/// `RoundingMode` deliberately does not appear in this text. A formula's
/// rendered text is what a reader checks against a standard, and a standard
/// states a rounding *granularity* -- "to one decimal place" -- without
/// naming a tie-breaking rule. The mode stays visible in the type, and will
/// surface in the trace and in `document()`; leaving it out here is a
/// decision, not an oversight.
template <Dialect D, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] std::string render_node(RoundNode<U, Places, Mode, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };
    std::string const placesText = std::to_string(Places.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + "\\,\\mathrm{" + unitSymbol + "}}(" + inner + ")";
    else
        return "round(" + inner + " to " + placesText + " dp of " + unitSymbol + ")";
}

/// A significant-digits rounding node, spelled the same way as `RoundNode`
/// above but with "sf" (significant figures) in place of "dp" -- see that
/// overload for why no precedence override is needed and why `RoundingMode`
/// is left out.
template <Dialect D, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
[[nodiscard]] std::string render_node(RoundSignificantNode<U, Digits, Mode, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };
    std::string const digitsText = std::to_string(Digits.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + digitsText + "\\mathrm{sf},\\,\\mathrm{" + unitSymbol + "}}(" + inner + ")";
    else
        return "round(" + inner + " to " + digitsText + " sf of " + unitSymbol + ")";
}

/// The numeric-value escape hatch renders as `numeric(<operand> in <unit>)`,
/// or as a braced quotient in LaTeX. A function call like `RoundNode` above,
/// so the same reasoning applies: no `PrecedenceOf` override needed, and none
/// of its data changes that answer.
///
/// The justification does not appear here either. It is the compile-time
/// record of *why* a rule needed a bare number instead of a quantity -- an
/// audit trail for `document()`, not part of the arithmetic this text states.
template <Dialect D, Unit U, detail::FixedString Justification, Node Operand>
[[nodiscard]] std::string render_node(NumericValueNode<U, Justification, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };

    if constexpr (D == Dialect::LaTeX)
        return "\\{" + inner + "/\\mathrm{" + unitSymbol + "}\\}";
    else
        return "numeric(" + inner + " in " + unitSymbol + ")";
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

/// A predicate renders as `<lhs> <comparison> <rhs>`. Not a `Node`, so it
/// cannot go through `render_operand` -- its own operand context is computed
/// directly from `PrecedenceOf<PredicateNode<...>>` instead, one rung above
/// `Conditional`, exactly the way `BinaryNode`'s right operand above computes
/// its context one rung above its own -- so that a `when()` nested on either
/// side still brackets: `when(...) > threshold` must not read as if the
/// comparison applied only to the else branch.
///
/// `<=`, `>=` and `!=` get the mathematical spelling in LaTeX (`\leq`,
/// `\geq`, `\neq`) rather than the code-like tokens the other dialects use,
/// the same way `BinaryNode`'s `*` becomes `\cdot` there.
template <Dialect D, Comparison Op, Node Left, Node Right>
[[nodiscard]] std::string render_node(PredicateNode<Op, Left, Right> const& node)
{
    constexpr detail::Precedence operandContext =
        static_cast<detail::Precedence>(static_cast<int>(detail::PrecedenceOf<PredicateNode<Op, Left, Right>>::value) + 1);
    std::string const lhs = detail::render_operand<D>(node.lhs, operandContext);
    std::string const rhs = detail::render_operand<D>(node.rhs, operandContext);

    char const* const symbol = [] {
        if constexpr (Op == Comparison::Less)
            return "<";
        else if constexpr (Op == Comparison::LessOrEqual)
            return D == Dialect::LaTeX ? "\\leq" : "<=";
        else if constexpr (Op == Comparison::Greater)
            return ">";
        else if constexpr (Op == Comparison::GreaterOrEqual)
            return D == Dialect::LaTeX ? "\\geq" : ">=";
        else if constexpr (Op == Comparison::Equal)
            return D == Dialect::LaTeX ? "=" : "==";
        else
            return D == Dialect::LaTeX ? "\\neq" : "!=";
    }();

    return lhs + " " + symbol + " " + rhs;
}

/// A conditional renders as `if <predicate> then <then> else <else>` in every
/// dialect but LaTeX, which spells it as a `\begin{cases}` block -- the usual
/// way a case-defined quantity is typeset. The branches are rendered plainly,
/// with no bracket check: each is already delimited on both sides by a
/// keyword (`then` / `else`, or `&` / `\\` in the cases block), the same way
/// a root's operand is delimited by its own parentheses, so nothing there can
/// misread regardless of what the branch is.
template <Dialect D, Predicate P, Node Then, Node Else>
[[nodiscard]] std::string render_node(WhenNode<P, Then, Else> const& node)
{
    std::string const predicateText = render<D>(node.predicate);
    std::string const thenText = render<D>(node.thenBranch);
    std::string const elseText = render<D>(node.elseBranch);

    if constexpr (D == Dialect::LaTeX)
        return "\\begin{cases} " + thenText + " & \\text{if } " + predicateText + " \\\\ " + elseText
               + " & \\text{otherwise} \\end{cases}";
    else
        return "if " + predicateText + " then " + thenText + " else " + elseText;
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

/// Renders @p node in dialect @p D. See the forward declaration above for why
/// this overload -- for `Predicate`, not `Node` -- exists separately.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render<Dialect::Plain>(node);
}

} // namespace formula
