// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A formula documenting itself: the rendered text, what it cites, and the
/// variables it reads.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<vector>`, and a consumer who only evaluates numbers must not compile a
/// symbol table and a citation list into every translation unit. Include it
/// when you want a documentation page, alongside `render.hpp` for the text
/// itself.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/render.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace formula
{

/// One row of a formula's symbol table: how a variable is written, what it
/// means, and the unit its values are expressed in.
struct SymbolEntry
{
    std::string_view symbol {};
    std::string_view description {};
    Unit unit {};

    [[nodiscard]] constexpr bool operator==(SymbolEntry const&) const noexcept = default;
};

/// Everything a documentation page needs from a formula: the formula itself
/// rendered to text, what it cites, and the symbol table for what it reads.
struct Documentation
{
    /// The formula, rendered in the dialect `document()` was asked for.
    std::string formula {};
    /// What the formula cites, outermost first -- the citation nearest the
    /// root of the tree comes first, so a reader meets the formula's own name
    /// before the definitions it is built from.
    std::vector<Citation> citations {};
    /// The variables the formula reads, each once, in the order they first
    /// appear when the formula is read left to right.
    std::vector<SymbolEntry> symbols {};
};

namespace detail
{
    // Forward declared so that a node whose children may themselves be any
    // node kind -- BinaryNode, DocumentedNode -- can recurse into a child
    // before every overload below has been declared.

    template <Described Q>
    void collect(Documentation& documentation, VarNode<Q> const& node);

    template <Unit U>
    void collect(Documentation& documentation, ConstantNode<U> const& node);

    void collect(Documentation& documentation, PiNode const& node);

    template <UnaryOperator Op, Node Operand>
    void collect(Documentation& documentation, UnaryNode<Op, Operand> const& node);

    template <int Exponent, Node Operand>
    void collect(Documentation& documentation, PowerNode<Exponent, Operand> const& node);

    template <int Degree, Node Operand>
    void collect(Documentation& documentation, RootNode<Degree, Operand> const& node);

    template <BinaryOperator Op, Node Left, Node Right>
    void collect(Documentation& documentation, BinaryNode<Op, Left, Right> const& node);

    template <Node Inner>
    void collect(Documentation& documentation, DocumentedNode<Inner> const& node);

    /// A variable contributes one row to the symbol table -- unless a row for
    /// the same symbol is already there, in which case the second use of a
    /// quantity adds nothing.
    template <Described Q>
    void collect(Documentation& documentation, VarNode<Q> const&)
    {
        std::string_view const symbol = Describe<Q>::symbol;
        for (SymbolEntry const& entry: documentation.symbols)
            if (entry.symbol == symbol)
                return;
        documentation.symbols.push_back(
            SymbolEntry { .symbol = symbol, .description = Describe<Q>::description, .unit = Describe<Q>::unit });
    }

    /// A literal coefficient names no variable.
    template <Unit U>
    void collect(Documentation&, ConstantNode<U> const&)
    {
    }

    /// Pi is a constant, not a variable.
    inline void collect(Documentation&, PiNode const&) {}

    template <UnaryOperator Op, Node Operand>
    void collect(Documentation& documentation, UnaryNode<Op, Operand> const& node)
    {
        collect(documentation, node.operand);
    }

    template <int Exponent, Node Operand>
    void collect(Documentation& documentation, PowerNode<Exponent, Operand> const& node)
    {
        collect(documentation, node.operand);
    }

    template <int Degree, Node Operand>
    void collect(Documentation& documentation, RootNode<Degree, Operand> const& node)
    {
        collect(documentation, node.operand);
    }

    /// Left before right -- what makes first-appearance order match reading
    /// order, rather than some incidental order of construction.
    template <BinaryOperator Op, Node Left, Node Right>
    void collect(Documentation& documentation, BinaryNode<Op, Left, Right> const& node)
    {
        collect(documentation, node.lhs);
        collect(documentation, node.rhs);
    }

    /// Pushing the citation before recursing is what makes the citation list
    /// outermost-first: the node closest to the root of the tree is visited,
    /// and therefore pushed, first.
    template <Node Inner>
    void collect(Documentation& documentation, DocumentedNode<Inner> const& node)
    {
        documentation.citations.push_back(node.citation);
        collect(documentation, node.inner);
    }
} // namespace detail

/// Documents @p node: renders it in dialect @p D and walks it for the
/// citations and symbol table a documentation page needs.
template <Dialect D = Dialect::Plain, Node N>
[[nodiscard]] Documentation document(N const& node)
{
    Documentation documentation { .formula = render<D>(node) };
    detail::collect(documentation, node);
    return documentation;
}

} // namespace formula
