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
#include <formula-cpp/constraint.hpp>
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
    /// How the variable is written in the formula.
    std::string_view symbol {};
    /// What the variable means, in words.
    std::string_view description {};
    /// The unit its values are expressed in.
    Unit unit {};

    /// Memberwise equality.
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
    /// A distinct address per @tparam Q, used to deduplicate the symbol table
    /// by quantity *type* without reaching for RTTI (`typeid`, `<typeindex>`).
    /// This library depends on neither elsewhere, and a header-only library
    /// should not make a consumer who builds with RTTI disabled pay for one
    /// bit of bookkeeping inside a single opt-in header.
    ///
    /// What the deduplication relies on is narrow: within a single walk, the
    /// address is stable for a given @p Q and different for every other one.
    /// A `Walk` lives and dies inside one `document()` call, so those
    /// comparisons never cross a translation unit.
    ///
    /// `inline` is here for a different reason, and it is not decoration.
    /// `document()` is a function template and so is implicitly inline; if its
    /// behaviour turned on an address that differed between translation units,
    /// that would be one function with two behaviours. `inline` makes the
    /// object one per program and the question moot -- measured on cl 19.51,
    /// clang-cl 22 and g++ 13.3, which agree. Without it the object would be
    /// one per translation unit on g++ and one per program on the other two,
    /// a difference this library has already been bitten by once elsewhere.
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

    // Not load-bearing, just this file's convention: every collect() call's
    // first argument is `Walk&`, so `formula::detail` -- Walk's namespace --
    // is always in ADL's search set, which is why every overload below is
    // found regardless of declaration order (confirmed by removing all five
    // and rebuilding). That is an implementation detail, not a guarantee, so
    // each overload stays declared here rather than relying on it.

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

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk& walk, RoundNode<U, Places, Mode, Operand> const& node);

    template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    void collect(Walk& walk, RoundSignificantNode<U, Digits, Mode, Operand> const& node);

    template <Unit U, FixedString Justification, Node Operand>
    void collect(Walk& walk, NumericValueNode<U, Justification, Operand> const& node);

    template <Comparison Op, Node Left, Node Right>
    void collect(Walk& walk, PredicateNode<Op, Left, Right> const& node);

    template <Predicate P, Node Then, Node Else>
    void collect(Walk& walk, WhenNode<P, Then, Else> const& node);

    template <Predicate P>
    void collect(Walk& walk, Constraint<P> const& node);

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

    /// Rounding changes a number, not the variables it depends on.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk& walk, RoundNode<U, Places, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// Rounding to significant digits changes a number, not the variables it
    /// depends on.
    template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    void collect(Walk& walk, RoundSignificantNode<U, Digits, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// The escape hatch still reads a variable, even though what it produces
    /// no longer carries a dimension.
    template <Unit U, FixedString Justification, Node Operand>
    void collect(Walk& walk, NumericValueNode<U, Justification, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// `PredicateNode` is not a `Node`, but its two sides are; both still name
    /// variables that belong in the symbol table.
    template <Comparison Op, Node Left, Node Right>
    void collect(Walk& walk, PredicateNode<Op, Left, Right> const& node)
    {
        collect(walk, node.lhs);
        collect(walk, node.rhs);
    }

    /// Walks the predicate and *both* branches -- deliberately the opposite of
    /// `checked_evaluate_si(WhenNode ...)` in `conditional.hpp`, which
    /// evaluates only the branch its predicate selects. Evaluation answers
    /// what happened this one time; documentation describes the formula
    /// itself, and a variable read only in the branch not taken on this
    /// occasion still belongs in the symbol table -- omitting it would make
    /// the documentation depend on which inputs happened to be passed in,
    /// which a formula's description must not do. Do not "fix" this to match
    /// evaluation's short-circuiting.
    template <Predicate P, Node Then, Node Else>
    void collect(Walk& walk, WhenNode<P, Then, Else> const& node)
    {
        collect(walk, node.predicate);
        collect(walk, node.thenBranch);
        collect(walk, node.elseBranch);
    }

    /// A constraint carries its own `Citation` instead of being wrapped by
    /// `documented()` -- see `constraint.hpp`'s file comment for why it
    /// cannot be: `DocumentedNode` requires `Node Inner`, and a constraint is
    /// deliberately not a `Node`. So this pushes onto the same citation list
    /// `collect(Walk&, DocumentedNode<Inner> const&)` above pushes onto,
    /// rather than opening a second path into it, then walks the predicate
    /// for the variables both its sides read.
    template <Predicate P>
    void collect(Walk& walk, Constraint<P> const& node)
    {
        walk.documentation.citations.push_back(node.citation);
        collect(walk, node.predicate);
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

/// Documents @p node: renders it in dialect @p D and walks it for the
/// citation and symbol table a documentation page needs.
///
/// A second overload, not the one above, for the identical reason
/// `render()` (`render.hpp`) carries a separate overload for `Constraint`
/// rather than reusing its `Node` one: a `Constraint` is not a `Node` --
/// `constraint.hpp`'s file comment explains why -- so it cannot reach the
/// overload above at all. This is **not** the same gap `documented()`
/// leaves. `documented()` still cannot wrap a constraint, and should not:
/// `DocumentedNode` requires `Node Inner`, and a constraint carries its own
/// `Citation` precisely so that it never needs wrapping in the first place.
/// This overload is the other half -- the one that lets a constraint be
/// documented directly, the way it is already rendered directly -- and it
/// does so through the exact same machinery: `render<D>(node)` dispatches
/// to `render.hpp`'s own `Constraint` overload, and `detail::collect(walk,
/// node)` dispatches to `collect(Walk&, Constraint<P> const&)` above, which
/// pushes the constraint's citation and walks its predicate for the symbol
/// table. Nothing here is new machinery; this overload is what makes that
/// existing machinery reachable from the public API at all.
template <Dialect D = Dialect::Plain, Predicate P>
[[nodiscard]] Documentation document(Constraint<P> const& node)
{
    detail::Walk walk { .documentation = Documentation { .formula = render<D>(node) } };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

} // namespace formula
