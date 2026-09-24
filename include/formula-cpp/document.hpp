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
#include <utility>
#include <vector>

namespace formula
{

/// One row of a formula's symbol table: how a variable is written, what it
/// means, and the unit its values are expressed in.
///
/// Deduplicated by quantity type, not by this rendered symbol. Two distinct
/// quantities are free to share a spelling -- whether that is wise is a
/// judgement for whoever writes the formula -- and when they do, each still
/// gets its own row rather than one silently standing in for the other.
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
    /// A compile-time-unique identity for @tparam Q: every instantiation of
    /// `quantityIdentity<Q>` is one `inline` object, so its address is the
    /// same in every translation unit for a given @p Q and different for
    /// every other @p Q. Used to deduplicate the symbol table by quantity
    /// *type* without reaching for RTTI (`typeid`, `<typeindex>`) -- this
    /// library otherwise depends on neither, and a header-only library should
    /// not make a consumer who builds with RTTI disabled pay for one bit of
    /// bookkeeping inside a single opt-in header.
    template <typename Q>
    inline constexpr bool quantityIdentity = false;

    /// The walk's own state: the `Documentation` being assembled, plus which
    /// quantities have already contributed a row, tracked in parallel because
    /// a `std::vector<SymbolEntry>` holds no type to check against. Not part
    /// of the published surface -- `document()` unwraps `documentation` before
    /// returning it.
    struct Walk
    {
        Documentation documentation {};
        std::vector<void const*> seenQuantities {};
    };

    // Forward declared so that a node whose children may themselves be any
    // node kind -- BinaryNode, DocumentedNode -- can recurse into a child
    // before every overload below has been declared.

    template <Described Q>
    void collect(Walk& walk, VarNode<Q> const& node);

    template <Unit U>
    void collect(Walk& walk, ConstantNode<U> const& node);

    void collect(Walk& walk, PiNode const& node);

    template <UnaryOperator Op, Node Operand>
    void collect(Walk& walk, UnaryNode<Op, Operand> const& node);

    template <int Exponent, Node Operand>
    void collect(Walk& walk, PowerNode<Exponent, Operand> const& node);

    template <int Degree, Node Operand>
    void collect(Walk& walk, RootNode<Degree, Operand> const& node);

    template <BinaryOperator Op, Node Left, Node Right>
    void collect(Walk& walk, BinaryNode<Op, Left, Right> const& node);

    template <Node Inner>
    void collect(Walk& walk, DocumentedNode<Inner> const& node);

    /// A variable contributes one row to the symbol table -- unless its
    /// quantity type has already contributed one, in which case the second
    /// use of that quantity adds nothing. A different quantity that merely
    /// renders the same symbol is not caught by this check and gets its own
    /// row; see the note on `SymbolEntry`.
    template <Described Q>
    void collect(Walk& walk, VarNode<Q> const&)
    {
        void const* const key = &quantityIdentity<Q>;
        for (void const* seen: walk.seenQuantities)
            if (seen == key)
                return;
        walk.seenQuantities.push_back(key);
        walk.documentation.symbols.push_back(SymbolEntry {
            .symbol = Describe<Q>::symbol, .description = Describe<Q>::description, .unit = Describe<Q>::unit });
    }

    /// A literal coefficient names no variable.
    template <Unit U>
    void collect(Walk&, ConstantNode<U> const&)
    {
    }

    /// Pi is a constant, not a variable.
    inline void collect(Walk&, PiNode const&) {}

    template <UnaryOperator Op, Node Operand>
    void collect(Walk& walk, UnaryNode<Op, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    template <int Exponent, Node Operand>
    void collect(Walk& walk, PowerNode<Exponent, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    template <int Degree, Node Operand>
    void collect(Walk& walk, RootNode<Degree, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// Left before right -- what makes first-appearance order match reading
    /// order, rather than some incidental order of construction.
    template <BinaryOperator Op, Node Left, Node Right>
    void collect(Walk& walk, BinaryNode<Op, Left, Right> const& node)
    {
        collect(walk, node.lhs);
        collect(walk, node.rhs);
    }

    /// Pushing the citation before recursing is what makes the citation list
    /// outermost-first: the node closest to the root of the tree is visited,
    /// and therefore pushed, first.
    template <Node Inner>
    void collect(Walk& walk, DocumentedNode<Inner> const& node)
    {
        walk.documentation.citations.push_back(node.citation);
        collect(walk, node.inner);
    }
} // namespace detail

/// Documents @p node: renders it in dialect @p D and walks it for the
/// citations and symbol table a documentation page needs.
template <Dialect D = Dialect::Plain, Node N>
[[nodiscard]] Documentation document(N const& node)
{
    detail::Walk walk { .documentation = Documentation { .formula = render<D>(node) } };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

} // namespace formula
