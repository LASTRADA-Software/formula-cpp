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
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/vocabulary.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
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

    /// The value an overlay fixed this quantity at, in `unit`; empty for a
    /// quantity the formula reads from the specimen.
    ///
    /// A row for an overridden constant (`OverriddenConstantNode`,
    /// `overlay.hpp`) that looked like any other would tell a reader to supply
    /// a value the formula will never read, and hide the one it does -- the
    /// gap a trace step that said "variable" had, on the documentation page
    /// instead of in the derivation.
    std::optional<Rational> fixedValue {};

    /// What the overlay that fixed the value cited for it; empty when it cited
    /// nothing, and for a quantity nothing fixed. Whether the row is fixed is
    /// `fixedValue`'s to say, not this.
    Citation fixedBy {};

    /// The expression a jurisdiction defined this quantity by, rendered in the
    /// dialect `document()` was asked for; empty for a quantity nothing
    /// defined.
    ///
    /// A row for a derived quantity (`DerivedQuantityNode`, `overlay.hpp`)
    /// that looked like any other would ask a reader to supply a value the
    /// formula computes instead, and hide how it computes it -- the reason
    /// `fixedValue` exists, for a definition rather than a number. The
    /// quantities the definition reads have rows of their own, after this one.
    std::optional<std::string> derivedAs {};

    /// What the overlay that defined the quantity cited for the definition;
    /// empty when it cited nothing, and for a quantity nothing defined.
    /// Whether the row is derived is `derivedAs`'s to say, not this.
    Citation derivedBy {};

    /// For a fixed or derived row: whether the formula ALSO reads this
    /// quantity from the environment somewhere, besides where an overlay fixed
    /// or defined it. False for a row that is neither, which is read and
    /// nothing else.
    ///
    /// Only a formula assembled by hand has both -- `apply` substitutes every
    /// use -- and then a row saying only "fixed at 97/100" would hide that the
    /// specimen's value is read as well, and one saying only "read" would hide
    /// the fixed value. It says both, in whichever order the two were met.
    bool alsoReadAsInput {};

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
    /// One entry per formula a jurisdiction replaced wholesale
    /// (`ReplacedVariantNode`, `overlay.hpp`), in the order met: what the
    /// overlay cited, or an empty `Citation` when it cited nothing.
    ///
    /// An entry even for a replacement that cited nothing, for the reason
    /// `SymbolEntry::fixedValue` marks a constant whatever its citation: a
    /// page that said nothing of an uncited replacement would read exactly as
    /// the base standard's page for a formula that is not the base
    /// standard's. A cited replacement's citation also joins `citations`.
    std::vector<Citation> replacedBy {};
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
    ///
    /// `renamed` is the vocabulary `document()` was given, as the one thing a
    /// walk needs from it: which quantities it writes differently, and how.
    /// Held as data rather than as a template parameter so that no `collect`
    /// overload below has to know a vocabulary exists -- only `add_row`, which
    /// writes every row's symbol, reads it, through `symbol_in`.
    struct Walk
    {
        Documentation documentation {};
        std::vector<void const*> seenQuantities {};
        /// The dialect `document()` was asked for, which a derived quantity's
        /// definition is rendered in.
        Dialect dialect = Dialect::Plain;
        std::vector<std::pair<void const*, std::string_view>> renamed {};
    };

    /// @p node rendered in @p dialect, chosen at run time -- for the one
    /// place a walk renders a sub-expression rather than the whole formula.
    template <Node N>
    [[nodiscard]] std::string render_in(Dialect dialect, N const& node)
    {
        switch (dialect)
        {
            case Dialect::Plain:
                break;
            case Dialect::Markdown:
                return render<Dialect::Markdown>(node);
            case Dialect::LaTeX:
                return render<Dialect::LaTeX>(node);
        }
        return render<Dialect::Plain>(node);
    }

    /// Whether a row is marked as substituted by an overlay: fixed, or
    /// derived.
    [[nodiscard]] inline bool is_substituted(SymbolEntry const& entry) noexcept
    {
        return entry.fixedValue.has_value() || entry.derivedAs.has_value();
    }

    /// How @p Q is written in @p walk: as the walk's vocabulary renamed it,
    /// or as `Describe<Q>::symbol` says. Keyed on the cv-unqualified type,
    /// matching `ScopedVocabulary`'s own resolution.
    template <Described Q>
    [[nodiscard]] std::string_view symbol_in(Walk const& walk)
    {
        void const* const key = &quantityIdentity<std::remove_cv_t<Q>>;
        for (auto const& [quantity, symbol]: walk.renamed)
            if (quantity == key)
                return symbol;
        return symbol_of<Q>(DefaultVocabulary {});
    }

    /// Records what @p vocabulary renames, for `symbol_in`. The symbol itself
    /// comes from `symbol_of`, so a walk resolves a quantity exactly as
    /// `render()` and the trace do; only the list of quantities is read here.
    inline void note_renamings(Walk&, DefaultVocabulary const&) {}

    template <typename... Es>
    void note_renamings(Walk& walk, ScopedVocabulary<Es...> const& vocabulary)
    {
        (walk.renamed.emplace_back(&quantityIdentity<std::remove_cv_t<typename Es::quantity>>,
                                   symbol_of<typename Es::quantity>(vocabulary)),
         ...);
    }

    // Not load-bearing, just this file's convention: every collect() call's
    // first argument is `Walk&`, so `formula::detail` -- Walk's namespace --
    // is always in ADL's search set, which is why every overload below is
    // found regardless of declaration order. Re-measured when the three lookup
    // overloads below were added: deleting all 17 declarations in this block
    // and rebuilding the whole test suite succeeds on cl 19.51, clang-cl 22,
    // clang 20.1.8 and g++ 14.2. Every one of those is a conformant two-phase
    // lookup -- `CMakeLists.txt` puts `/permissive-` on every cl compile line
    // as an INTERFACE requirement of the library, read off this file's own
    // entry in `compile_commands.json` rather than assumed -- so no leg of
    // that measurement rested on MSVC's permissive mode. (The earlier wording
    // said "all five", which was the count when this was first measured.)
    // That is an implementation detail, not a guarantee, so each overload
    // stays declared here rather than relying on it.

    template <Described Q>
    void collect(Walk& walk, VarNode<Q> const& node);

    template <Described Q>
    void collect(Walk& walk, OverriddenConstantNode<Q> const& node);

    template <Described Q, Node Expr>
    void collect(Walk& walk, DerivedQuantityNode<Q, Expr> const& node);

    template <Node Expr>
    void collect(Walk& walk, ReplacedVariantNode<Expr> const& node);

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

    template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    void collect(Walk& walk, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node);

    template <KeyTable Keys, Unit ResultUnit>
    void collect(Walk& walk, ExactLookupNode<Keys, ResultUnit> const& node);

    template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    void collect(Walk& walk, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node);

    template <Comparison Op, Node Left, Node Right>
    void collect(Walk& walk, PredicateNode<Op, Left, Right> const& node);

    template <Predicate P, Node Then, Node Else>
    void collect(Walk& walk, WhenNode<P, Then, Else> const& node);

    template <Predicate P>
    void collect(Walk& walk, Constraint<P> const& node);

    /// Finds @p Q's row in the symbol table, adding a plain one when @p Q has
    /// none yet; @p row is its index. True when the row was added now.
    ///
    /// Deduplicated by quantity type -- see `SymbolEntry`. `seenQuantities`
    /// and `symbols` grow together, so one index names both.
    template <Described Q>
    bool add_row(Walk& walk, std::size_t& row)
    {
        void const* const key = &quantityIdentity<Q>;
        row = 0;
        while (row < walk.seenQuantities.size() && walk.seenQuantities[row] != key)
            ++row;
        if (row < walk.seenQuantities.size())
            return false;
        walk.seenQuantities.push_back(key);
        walk.documentation.symbols.push_back(SymbolEntry {
            .symbol = symbol_in<Q>(walk), .description = Describe<Q>::description, .unit = Describe<Q>::unit });
        return true;
    }

    /// A variable contributes one row to the symbol table -- unless its
    /// quantity type has already contributed one, in which case the second
    /// use of that quantity adds nothing. A different quantity that merely
    /// renders the same symbol is not caught by this check and gets its own
    /// row; see the note on `SymbolEntry`.
    ///
    /// A quantity an overlay fixed or defined earlier in the same walk --
    /// possible only in a formula assembled by hand -- keeps its marked row,
    /// which is marked as also read: see `SymbolEntry::alsoReadAsInput`.
    template <Described Q>
    void collect(Walk& walk, VarNode<Q> const&)
    {
        std::size_t row = 0;
        if (add_row<Q>(walk, row))
            return;
        SymbolEntry& entry = walk.documentation.symbols[row];
        if (is_substituted(entry))
            entry.alsoReadAsInput = true;
    }

    /// An overridden constant contributes its quantity's row as a variable
    /// does, marked as fixed: the overlay's value, and what it cited. See
    /// `SymbolEntry::fixedValue` for why a plain row would mislead.
    ///
    /// Chosen over the `VarNode` overload above, which it derives from, for
    /// the reason its `checked_evaluate_si` overload is (`overlay.hpp`).
    ///
    /// A quantity read plainly earlier in the same walk -- possible only in a
    /// formula assembled by hand, since `apply` fixes every use of it -- has
    /// its row marked fixed and also read, rather than left plain: the formula
    /// reads both, and a page must say so whichever order the two uses were
    /// met in. See `SymbolEntry::alsoReadAsInput`.
    template <Described Q>
    void collect(Walk& walk, OverriddenConstantNode<Q> const& node)
    {
        std::size_t row = 0;
        bool const added = add_row<Q>(walk, row);
        SymbolEntry& entry = walk.documentation.symbols[row];
        if (entry.fixedValue.has_value())
            return;
        // An existing row that no overlay marked was contributed by a plain
        // read.
        if (!added && !is_substituted(entry))
            entry.alsoReadAsInput = true;
        entry.fixedValue = node.value();
        entry.fixedBy = node.source();
    }

    /// A derived quantity contributes its quantity's row, marked as derived:
    /// the definition, rendered in the page's dialect, and what the overlay
    /// cited -- see `SymbolEntry::derivedAs`. Then the definition is walked,
    /// after the row, for the quantities it reads: they are inputs of the
    /// formula as much as any other.
    ///
    /// A plain read of the same quantity, before or after, marks the row as
    /// also read, as it does for a fixed row.
    template <Described Q, Node Expr>
    void collect(Walk& walk, DerivedQuantityNode<Q, Expr> const& node)
    {
        std::size_t row = 0;
        bool const added = add_row<Q>(walk, row);
        {
            SymbolEntry& entry = walk.documentation.symbols[row];
            if (!entry.derivedAs.has_value())
            {
                if (!added && !is_substituted(entry))
                    entry.alsoReadAsInput = true;
                entry.derivedAs = render_in(walk.dialect, node.expression());
                entry.derivedBy = node.source();
            }
        }
        // After the entry reference is done with: walking may add rows, and
        // a vector that grows invalidates references into it.
        collect(walk, node.expression());
    }

    /// A replaced formula is marked as replaced, cited or not -- see
    /// `Documentation::replacedBy` -- and walked as the formula; what the
    /// overlay cited for it also joins the citations when it cited anything,
    /// the guard `collect(Walk&, Constraint<P> const&)` has, for its reason.
    template <Node Expr>
    void collect(Walk& walk, ReplacedVariantNode<Expr> const& node)
    {
        walk.documentation.replacedBy.push_back(node.source());
        if (!(node.source() == Citation {}))
            walk.documentation.citations.push_back(node.source());
        collect(walk, node.replacement());
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

    /// A lookup table names no variable, and neither half of one could: a
    /// banded lookup's bands live in its type and its corrections are runtime
    /// numbers (`lookup.hpp`), while a symbol table's rows are the quantities a
    /// formula *reads*. The operand is the one thing here that reads anything,
    /// and it is walked for the reason `RoundNode`'s operand is walked: the
    /// table decides which number comes out, not which variables went in.
    template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    void collect(Walk& walk, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// An exact lookup contributes nothing, and that is a fact about this node
    /// kind rather than a decision taken here: it has no operand at all
    /// (`lookup.hpp`). Its key is a discriminator rather than a quantity, so it
    /// reaches the node as runtime state instead of as a sub-expression, and
    /// there is no child to walk. Rendering does put that key where the other
    /// two kinds put their operand -- `lookup(key Cylinder, ...)` -- so the subject
    /// position of the rendered formula is occupied by something a reader may
    /// well take for a variable; it names none, has no unit and earns no row.
    /// Empty for the reason `collect(Walk&, ConstantNode<U> const&)` is empty,
    /// not for want of looking.
    template <KeyTable Keys, Unit ResultUnit>
    void collect(Walk&, ExactLookupNode<Keys, ResultUnit> const&)
    {
    }

    /// An interpolating lookup walks its operand for the reason a banded one
    /// does. What is particular to this kind changes nothing about it: the
    /// answer between two rows is computed rather than read off the table, and
    /// a computed number is still a number, not a variable.
    template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    void collect(Walk& walk, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node)
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
    ///
    /// **Only when the citation is not blank.** `constraint(predicate,
    /// verdict, citation = {})` (`constraint.hpp`) makes `citation` optional
    /// -- the two-argument call is an ordinary, supported way to declare a
    /// constraint, used by this project's own guide, example and several
    /// tests -- so `node.citation` is not always something a caller meant to
    /// cite. `DocumentedNode` has no equivalent guard because
    /// `documented(expr, citation)` requires the citation argument; nothing
    /// here has ever been able to construct a `DocumentedNode` with a blank
    /// one to compare against. Pushing unconditionally would turn
    /// `documentation.citations.empty()` from "this formula cites nothing"
    /// into "this formula cites nothing, unless it read an uncited
    /// constraint", and would render a bare, five-blank-field citation entry
    /// on a generated page. `Citation`'s memberwise `operator==` against a
    /// value-initialised `Citation {}` is exactly "every field empty".
    template <Predicate P>
    void collect(Walk& walk, Constraint<P> const& node)
    {
        if (!(node.citation == Citation {}))
            walk.documentation.citations.push_back(node.citation);
        collect(walk, node.predicate);
    }
} // namespace detail

/// Documents @p node: renders it in dialect @p D and walks it for the
/// citations and symbol table a documentation page needs.
///
/// Every symbol -- in the rendered formula and in the symbol table alike -- is
/// written as @p vocabulary says (`vocabulary.hpp`); each row's description
/// and unit stay the quantity's own, because a vocabulary renames how a
/// quantity is written, never what it is.
template <Dialect D = Dialect::Plain, Node N, Vocabulary V>
[[nodiscard]] Documentation document(N const& node, V const& vocabulary)
{
    detail::Walk walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) }, .dialect = D };
    detail::note_renamings(walk, vocabulary);
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents @p node in the default vocabulary, which renames nothing.
template <Dialect D = Dialect::Plain, Node N>
[[nodiscard]] Documentation document(N const& node)
{
    return document<D>(node, DefaultVocabulary {});
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
///
/// Every symbol is written as @p vocabulary says, as in the `Node` overload.
template <Dialect D = Dialect::Plain, Predicate P, Vocabulary V>
[[nodiscard]] Documentation document(Constraint<P> const& node, V const& vocabulary)
{
    detail::Walk walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) }, .dialect = D };
    detail::note_renamings(walk, vocabulary);
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents @p node in the default vocabulary, which renames nothing.
template <Dialect D = Dialect::Plain, Predicate P>
[[nodiscard]] Documentation document(Constraint<P> const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

} // namespace formula
