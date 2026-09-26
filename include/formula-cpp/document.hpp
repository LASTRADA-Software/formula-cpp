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

#include <formula-cpp/binning.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/vocabulary.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace formula
{

/// Whether a symbol-table row is read as one value or as a series of them.
enum class ValueShape : std::uint8_t
{
    /// One value: `var<Q>`.
    Single,
    /// A series of `SymbolEntry::length` values: `series<Q, N>`.
    Series,
    /// Raw observations, at most `SymbolEntry::length` of them:
    /// `observations<Q, Capacity>`.
    Observations,
};

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
    /// use -- and then a row saying only "fixed at 863/1000" would hide that the
    /// specimen's value is read as well, and one saying only "read" would hide
    /// the fixed value. It says both, in whichever order the two were met.
    bool alsoReadAsInput {};

    /// Whether the formula reads this quantity as one value or as a series.
    /// The rendered formula already marks a series (`x_m(i)`, see
    /// `detail::series_marker`); this row says it again, for a reader who
    /// starts at the table.
    ValueShape shape = ValueShape::Single;

    /// How many values the formula reads for this row: the series' length
    /// `N`, the most observations there can be, or one for a single value.
    std::size_t length = 1;

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
    ///
    /// **Writable, and deliberately not `constexpr` or `const`.** Identical
    /// COMDAT folding -- cl's `/OPT:ICF`, lld's `--icf=all` -- may give two
    /// read-only objects of identical contents one address, and every
    /// `quantityIdentity<Q>` has the same contents. Folded, two quantities
    /// would share one symbol-table row. Writable data is never folded, so an
    /// address of this object is one per type however the program is linked.
    /// Nothing writes to it. (The folding was not reproduced here: this
    /// removes the dependence on its not happening rather than a failure
    /// seen.)
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
    inline bool quantityIdentity = false;

    /// A distinct address per quantity @p Q read as a series of @p N, for
    /// `quantityIdentity`'s reason and in the same writable form. Distinct from
    /// `quantityIdentity<Q>` and from every other length, so that the symbol
    /// table has one row per quantity, shape and length: `var<Q>` and
    /// `series<Q, 5>` in one formula are two rows with one symbol.
    template <typename Q, std::size_t N>
    inline bool seriesIdentity = false;

    /// A distinct address per quantity @p Q read as observations of at most
    /// @p Capacity, for `seriesIdentity`'s reason: a series of `Q` and
    /// observations of `Q` in one formula are two rows.
    template <typename Q, std::size_t Capacity>
    inline bool observationsIdentity = false;

    /// The walk's own state: the `Documentation` being assembled, plus which
    /// quantities have already contributed a row, tracked in parallel because
    /// a `std::vector<SymbolEntry>` holds no type to check against. Not part
    /// of the published surface -- `document()` unwraps `documentation` before
    /// returning it.
    ///
    /// `vocabulary` is the one `document()` was given, held whole rather than
    /// as a list of what it renames: a derived quantity's definition is
    /// rendered with it, in the page's words (`render_in`), and a row's symbol
    /// is `symbol_of<Q>(walk.vocabulary)`, resolved by the quantity's type
    /// exactly as `render()` and the trace resolve it.
    template <Vocabulary V>
    struct Walk
    {
        Documentation documentation {};
        std::vector<void const*> seenQuantities {};
        /// The dialect `document()` was asked for, which a derived quantity's
        /// definition is rendered in.
        Dialect dialect = Dialect::Plain;
        V vocabulary;
    };

    /// @p node rendered in @p dialect, chosen at run time, and in
    /// @p vocabulary -- for the one place a walk renders a sub-expression
    /// rather than the whole formula, which must be in the same words as the
    /// formula it sits beside.
    template <Node N, Vocabulary V>
    [[nodiscard]] std::string render_in(Dialect dialect, N const& node, V const& vocabulary)
    {
        switch (dialect)
        {
            case Dialect::Plain:
                break;
            case Dialect::Markdown:
                return render<Dialect::Markdown>(node, vocabulary);
            case Dialect::LaTeX:
                return render<Dialect::LaTeX>(node, vocabulary);
        }
        return render<Dialect::Plain>(node, vocabulary);
    }

    /// Whether a row is marked as substituted by an overlay: fixed, or
    /// derived.
    [[nodiscard]] inline bool is_substituted(SymbolEntry const& symbolEntry) noexcept
    {
        return symbolEntry.fixedValue.has_value() || symbolEntry.derivedAs.has_value();
    }

    // Not load-bearing, just this file's convention: every collect() call's
    // first argument is a `Walk<V>&`, so `formula::detail` -- Walk's namespace --
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

    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, VarNode<Q> const& node);

    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, OverriddenConstantNode<Q> const& node);

    template <Vocabulary V, Described Q, Node Expr>
    void collect(Walk<V>& walk, DerivedQuantityNode<Q, Expr> const& node);

    template <Vocabulary V, Node Expr>
    void collect(Walk<V>& walk, ReplacedVariantNode<Expr> const& node);

    template <Vocabulary V, Unit U>
    void collect(Walk<V>& walk, ConstantNode<U> const& node);

    template <Vocabulary V>
    void collect(Walk<V>& walk, PiNode const& node);

    template <Vocabulary V, UnaryOperator Op, Node Operand>
    void collect(Walk<V>& walk, UnaryNode<Op, Operand> const& node);

    template <Vocabulary V, int Exponent, Node Operand>
    void collect(Walk<V>& walk, PowerNode<Exponent, Operand> const& node);

    template <Vocabulary V, int Degree, Node Operand>
    void collect(Walk<V>& walk, RootNode<Degree, Operand> const& node);

    template <Vocabulary V, BinaryOperator Op, Node Left, Node Right>
    void collect(Walk<V>& walk, BinaryNode<Op, Left, Right> const& node);

    template <Vocabulary V, Node Inner>
    void collect(Walk<V>& walk, DocumentedNode<Inner> const& node);

    template <Vocabulary V, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundNode<U, Places, Mode, Operand> const& node);

    template <Vocabulary V, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundSignificantNode<U, Digits, Mode, Operand> const& node);

    template <Vocabulary V, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    void collect(Walk<V>& walk, RoundedRootNode<U, Places, Mode, Radicand> const& node);

    template <Vocabulary V, Unit U, FixedString Justification, Node Operand>
    void collect(Walk<V>& walk, NumericValueNode<U, Justification, Operand> const& node);

    template <Vocabulary V, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    void collect(Walk<V>& walk, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node);

    template <Vocabulary V, KeyTable Keys, Unit ResultUnit>
    void collect(Walk<V>& walk, ExactLookupNode<Keys, ResultUnit> const& node);

    template <Vocabulary V, SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    void collect(Walk<V>& walk, SampleSizeLookupNode<Sizes, ResultUnit, Count> const& node);

    template <Vocabulary V, Node Operand>
    void collect(Walk<V>& walk, AbsoluteValueNode<Operand> const& node);

    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, PrecisionLevelNode<Q> const& node);

    template <Vocabulary V, PrecisionKind K, Node Level, Node Limit>
    void collect(Walk<V>& walk, PrecisionLimitNode<K, Level, Limit> const& node);

    template <Vocabulary V, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    void collect(Walk<V>& walk, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node);

    template <Vocabulary V, Comparison Op, Node Left, Node Right>
    void collect(Walk<V>& walk, PredicateNode<Op, Left, Right> const& node);

    template <Vocabulary V, Predicate P, Node Then, Node Else>
    void collect(Walk<V>& walk, WhenNode<P, Then, Else> const& node);

    template <Vocabulary V, Predicate P>
    void collect(Walk<V>& walk, Constraint<P> const& node);

    template <Vocabulary V, Described Q, std::size_t N>
    void collect(Walk<V>& walk, SeriesVarNode<Q, N> const& node);

    template <Vocabulary V, Unit U, std::size_t N>
    void collect(Walk<V>& walk, SeriesConstantNode<U, N> const& node);

    template <Vocabulary V, UnaryOperator Op, SeriesNode Operand>
    void collect(Walk<V>& walk, ElementwiseUnaryNode<Op, Operand> const& node);

    template <Vocabulary V, BinaryOperator Op, typename Left, typename Right>
    void collect(Walk<V>& walk, ElementwiseBinaryNode<Op, Left, Right> const& node);

    template <Vocabulary V, CumulativeDirection D, SeriesNode S>
    void collect(Walk<V>& walk, CumulativeNode<D, S> const& node);

    template <Vocabulary V, SeriesNode S>
    void collect(Walk<V>& walk, SumNode<S> const& node);

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleCountNode<S> const& node);

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleMeanNode<S> const& node);

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleVarianceNode<S> const& node);

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleRangeNode<S> const& node);

    template <Vocabulary V, Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    void collect(Walk<V>& walk, ElementwiseRoundNode<U, Places, Mode, S> const& node);

    template <Vocabulary V, Dimension Dim>
    void collect(Walk<V>& walk, RefusedSeries<Dim> const& node);

    template <Vocabulary V, Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    void collect(Walk<V>& walk, SnapNode<KeyUnit, Permitted, Tie, Operand> const& node);

    template <Vocabulary V, Unit U, BreakpointTable Points>
    void collect(Walk<V>& walk, DomainNode<U, Points> const& node);

    template <Vocabulary V, Described Q, std::size_t Capacity>
    void collect(Walk<V>& walk, ObservationsVarNode<Q, Capacity> const& node);

    template <Vocabulary V>
    void collect(Walk<V>& walk, RefusedObservations const& node);

    template <Vocabulary V, Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
    void collect(Walk<V>& walk, BinnedNode<KeyUnit, Classes, Obs> const& node);

    template <Vocabulary V, SeriesNode DomainSeries, SeriesNode ValueSeries>
    void collect(Walk<V>& walk, CurveNode<DomainSeries, ValueSeries> const& node);

    template <Vocabulary V, Monotone M, CurveExpression A, CurveExpression B>
    void collect(Walk<V>& walk, SpliceNode<M, A, B> const& node);

    template <Vocabulary V, CurveExpression C, Node At>
    void collect(Walk<V>& walk, InterpolateAlongNode<C, At> const& node);

    /// Finds @p Q's row in the symbol table, adding a plain one when @p Q has
    /// none yet; @p row is its index. True when the row was added now.
    ///
    /// Deduplicated by quantity type -- see `SymbolEntry`. `seenQuantities`
    /// and `symbols` grow together, so one index names both.
    template <Described Q, Vocabulary V>
    bool add_row(Walk<V>& walk, std::size_t& row)
    {
        void const* const identity = &quantityIdentity<Q>;
        row = 0;
        while (row < walk.seenQuantities.size() && walk.seenQuantities[row] != identity)
            ++row;
        if (row < walk.seenQuantities.size())
            return false;
        walk.seenQuantities.push_back(identity);
        walk.documentation.symbols.push_back(SymbolEntry {
            .symbol = symbol_of<Q>(walk.vocabulary), .description = Describe<Q>::description, .unit = Describe<Q>::unit });
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
    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, VarNode<Q> const&)
    {
        std::size_t symbolRow = 0;
        if (add_row<Q>(walk, symbolRow))
            return;
        SymbolEntry& symbolEntry = walk.documentation.symbols[symbolRow];
        if (is_substituted(symbolEntry))
            symbolEntry.alsoReadAsInput = true;
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
    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, OverriddenConstantNode<Q> const& node)
    {
        std::size_t symbolRow = 0;
        bool const added = add_row<Q>(walk, symbolRow);
        SymbolEntry& symbolEntry = walk.documentation.symbols[symbolRow];
        if (symbolEntry.fixedValue.has_value())
            return;
        // An existing row that no overlay marked was contributed by a plain
        // read.
        if (!added && !is_substituted(symbolEntry))
            symbolEntry.alsoReadAsInput = true;
        symbolEntry.fixedValue = node.value();
        symbolEntry.fixedBy = node.source();
    }

    /// A derived quantity contributes its quantity's row, marked as derived:
    /// the definition, rendered in the page's dialect, and what the overlay
    /// cited -- see `SymbolEntry::derivedAs`. Then the definition is walked,
    /// after the row, for the quantities it reads: they are inputs of the
    /// formula as much as any other.
    ///
    /// A plain read of the same quantity, before or after, marks the row as
    /// also read, as it does for a fixed row.
    template <Vocabulary V, Described Q, Node Expr>
    void collect(Walk<V>& walk, DerivedQuantityNode<Q, Expr> const& node)
    {
        std::size_t symbolRow = 0;
        bool const added = add_row<Q>(walk, symbolRow);
        {
            SymbolEntry& symbolEntry = walk.documentation.symbols[symbolRow];
            if (!symbolEntry.derivedAs.has_value())
            {
                if (!added && !is_substituted(symbolEntry))
                    symbolEntry.alsoReadAsInput = true;
                symbolEntry.derivedAs = render_in(walk.dialect, node.expression(), walk.vocabulary);
                symbolEntry.derivedBy = node.source();
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
    template <Vocabulary V, Node Expr>
    void collect(Walk<V>& walk, ReplacedVariantNode<Expr> const& node)
    {
        walk.documentation.replacedBy.push_back(node.source());
        if (!(node.source() == Citation {}))
            walk.documentation.citations.push_back(node.source());
        collect(walk, node.replacement());
    }

    /// A literal coefficient names no variable.
    template <Vocabulary V, Unit U>
    void collect(Walk<V>&, ConstantNode<U> const&)
    {
    }

    /// Pi is a constant, not a variable.
    template <Vocabulary V>
    void collect(Walk<V>&, PiNode const&)
    {
    }

    template <Vocabulary V, UnaryOperator Op, Node Operand>
    void collect(Walk<V>& walk, UnaryNode<Op, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    template <Vocabulary V, int Exponent, Node Operand>
    void collect(Walk<V>& walk, PowerNode<Exponent, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    template <Vocabulary V, int Degree, Node Operand>
    void collect(Walk<V>& walk, RootNode<Degree, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// Left before right -- what makes first-appearance order match reading
    /// order, rather than some incidental order of construction.
    template <Vocabulary V, BinaryOperator Op, Node Left, Node Right>
    void collect(Walk<V>& walk, BinaryNode<Op, Left, Right> const& node)
    {
        collect(walk, node.lhs);
        collect(walk, node.rhs);
    }

    /// Pushing the citation before recursing is what makes the citation list
    /// outermost-first: the node closest to the root of the tree is visited,
    /// and therefore pushed, first.
    template <Vocabulary V, Node Inner>
    void collect(Walk<V>& walk, DocumentedNode<Inner> const& node)
    {
        walk.documentation.citations.push_back(node.citation);
        collect(walk, node.inner);
    }

    /// Rounding changes a number, not the variables it depends on.
    template <Vocabulary V, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundNode<U, Places, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// Rounding to significant digits changes a number, not the variables it
    /// depends on.
    template <Vocabulary V, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundSignificantNode<U, Digits, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// An absolute value reads what its operand reads.
    template <Vocabulary V, Node Operand>
    void collect(Walk<V>& walk, AbsoluteValueNode<Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// A precision limit's level placeholder is no input: it is the level the
    /// limit's own level expression produced, and that expression's
    /// variables are collected from it. `Q` only names the unit, so it gets
    /// no row of its own for being named here.
    template <Vocabulary V, Described Q>
    void collect(Walk<V>&, PrecisionLevelNode<Q> const&)
    {
    }

    /// A precision limit reads what its level and its limit read, level first,
    /// as the evaluation does.
    template <Vocabulary V, PrecisionKind K, Node Level, Node Limit>
    void collect(Walk<V>& walk, PrecisionLimitNode<K, Level, Limit> const& node)
    {
        collect(walk, node.level);
        collect(walk, node.limit);
    }

    /// A critical-value lookup reads what its count reads. Its sizes are
    /// printed by `render()` in the page's formula, and its values are data,
    /// as a lookup's corrections are.
    template <Vocabulary V, SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    void collect(Walk<V>& walk, SampleSizeLookupNode<Sizes, ResultUnit, Count> const& node)
    {
        collect(walk, node.count);
    }

    /// A rounded square root reads what its radicand reads, as a rounding
    /// node reads what its operand does.
    template <Vocabulary V, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    void collect(Walk<V>& walk, RoundedRootNode<U, Places, Mode, Radicand> const& node)
    {
        collect(walk, node.radicand);
    }

    /// The escape hatch still reads a variable, even though what it produces
    /// no longer carries a dimension.
    template <Vocabulary V, Unit U, FixedString Justification, Node Operand>
    void collect(Walk<V>& walk, NumericValueNode<U, Justification, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// A lookup table names no variable, and neither half of one could: a
    /// banded lookup's bands live in its type and its corrections are runtime
    /// numbers (`lookup.hpp`), while a symbol table's rows are the quantities a
    /// formula *reads*. The operand is the one thing here that reads anything,
    /// and it is walked for the reason `RoundNode`'s operand is walked: the
    /// table decides which number comes out, not which variables went in.
    template <Vocabulary V, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    void collect(Walk<V>& walk, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node)
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
    template <Vocabulary V, KeyTable Keys, Unit ResultUnit>
    void collect(Walk<V>&, ExactLookupNode<Keys, ResultUnit> const&)
    {
    }

    /// An interpolating lookup walks its operand for the reason a banded one
    /// does. What is particular to this kind changes nothing about it: the
    /// answer between two rows is computed rather than read off the table, and
    /// a computed number is still a number, not a variable.
    template <Vocabulary V, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    void collect(Walk<V>& walk, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// `PredicateNode` is not a `Node`, but its two sides are; both still name
    /// variables that belong in the symbol table.
    template <Vocabulary V, Comparison Op, Node Left, Node Right>
    void collect(Walk<V>& walk, PredicateNode<Op, Left, Right> const& node)
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
    template <Vocabulary V, Predicate P, Node Then, Node Else>
    void collect(Walk<V>& walk, WhenNode<P, Then, Else> const& node)
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
    template <Vocabulary V, Predicate P>
    void collect(Walk<V>& walk, Constraint<P> const& node)
    {
        if (!(node.citation == Citation {}))
            walk.documentation.citations.push_back(node.citation);
        collect(walk, node.predicate);
    }

    /// A series variable contributes one row, marked as a series of @p N --
    /// unless the same quantity has already contributed a series row of that
    /// length. Deduplicated on quantity, shape and length (`seriesIdentity`),
    /// so a single value of the same quantity, or a series of it over another
    /// length, is a row of its own.
    template <Vocabulary V, Described Q, std::size_t N>
    void collect(Walk<V>& walk, SeriesVarNode<Q, N> const&)
    {
        void const* const identity = &seriesIdentity<Q, N>;
        for (void const* const seen: walk.seenQuantities)
            if (seen == identity)
                return;
        walk.seenQuantities.push_back(identity);
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .shape = ValueShape::Series,
                                                           .length = N });
    }

    /// Raw observations contribute one row, marked as observations of at
    /// most @p Capacity -- unless the same quantity has already contributed
    /// such a row of that capacity. Deduplicated as a series variable is,
    /// on quantity, shape and capacity (`observationsIdentity`).
    template <Vocabulary V, Described Q, std::size_t Capacity>
    void collect(Walk<V>& walk, ObservationsVarNode<Q, Capacity> const&)
    {
        void const* const identity = &observationsIdentity<Q, Capacity>;
        for (void const* const seen: walk.seenQuantities)
            if (seen == identity)
                return;
        walk.seenQuantities.push_back(identity);
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .shape = ValueShape::Observations,
                                                           .length = Capacity });
    }

    /// Observations refused already name nothing.
    template <Vocabulary V>
    void collect(Walk<V>&, RefusedObservations const&)
    {
    }

    /// A binning names nothing of its own; its observations do. Its classes
    /// are `render()`'s to print, as a lookup's bands are.
    template <Vocabulary V, Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
    void collect(Walk<V>& walk, BinnedNode<KeyUnit, Classes, Obs> const& node)
    {
        collect(walk, node.source);
    }

    /// A per-element constant names no variable, as a scalar constant names
    /// none.
    template <Vocabulary V, Unit U, std::size_t N>
    void collect(Walk<V>&, SeriesConstantNode<U, N> const&)
    {
    }

    template <Vocabulary V, UnaryOperator Op, SeriesNode Operand>
    void collect(Walk<V>& walk, ElementwiseUnaryNode<Op, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// Left before right, as for a scalar `BinaryNode`, so the table reads in
    /// the formula's order.
    template <Vocabulary V, BinaryOperator Op, typename Left, typename Right>
    void collect(Walk<V>& walk, ElementwiseBinaryNode<Op, Left, Right> const& node)
    {
        collect(walk, node.lhs);
        collect(walk, node.rhs);
    }

    /// A running total names nothing of its own; its series does.
    template <Vocabulary V, CumulativeDirection D, SeriesNode S>
    void collect(Walk<V>& walk, CumulativeNode<D, S> const& node)
    {
        collect(walk, node.operand);
    }

    /// A sum is one value, but what it reads is a series, and the row says
    /// so: the series variable beneath it contributes its series row.
    template <Vocabulary V, SeriesNode S>
    void collect(Walk<V>& walk, SumNode<S> const& node)
    {
        collect(walk, node.operand);
    }

    /// A sample statistic is one value, but what it reads is a sample, and
    /// the row says so: the series beneath it contributes its series row.
    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleCountNode<S> const& node)
    {
        collect(walk, node.sample);
    }

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleMeanNode<S> const& node)
    {
        collect(walk, node.sample);
    }

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleVarianceNode<S> const& node)
    {
        collect(walk, node.sample);
    }

    template <Vocabulary V, SampleSource S>
    void collect(Walk<V>& walk, SampleRangeNode<S> const& node)
    {
        collect(walk, node.sample);
    }

    /// A refused series names nothing: it only keeps `document` from adding a
    /// second error to the refusal that produced it.
    template <Vocabulary V, Dimension Dim>
    void collect(Walk<V>&, RefusedSeries<Dim> const&)
    {
    }

    /// A snap names nothing of its own; its operand does. Its set is
    /// `render()`'s to print, as a lookup's rows are.
    template <Vocabulary V, Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    void collect(Walk<V>& walk, SnapNode<KeyUnit, Permitted, Tie, Operand> const& node)
    {
        collect(walk, node.operand);
    }

    /// A per-element rounding names nothing of its own; its series does.
    template <Vocabulary V, Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    void collect(Walk<V>& walk, ElementwiseRoundNode<U, Places, Mode, S> const& node)
    {
        collect(walk, node.operand);
    }

    /// A declared domain names nothing: its points are `render()`'s to print.
    template <Vocabulary V, Unit U, BreakpointTable Points>
    void collect(Walk<V>&, DomainNode<U, Points> const&)
    {
    }

    /// A curve names nothing of its own; its two series do, the domain first.
    template <Vocabulary V, SeriesNode DomainSeries, SeriesNode ValueSeries>
    void collect(Walk<V>& walk, CurveNode<DomainSeries, ValueSeries> const& node)
    {
        collect(walk, node.domainSeries);
        collect(walk, node.valueSeries);
    }

    /// A splice names nothing of its own; its curves do, in the order
    /// written.
    template <Vocabulary V, Monotone M, CurveExpression A, CurveExpression B>
    void collect(Walk<V>& walk, SpliceNode<M, A, B> const& node)
    {
        collect(walk, node.first);
        collect(walk, node.second);
    }

    /// An interpolation names nothing of its own; its curve and its point do.
    template <Vocabulary V, CurveExpression C, Node At>
    void collect(Walk<V>& walk, InterpolateAlongNode<C, At> const& node)
    {
        collect(walk, node.along);
        collect(walk, node.at);
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
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents @p node in the default vocabulary, which renames nothing.
template <Dialect D = Dialect::Plain, Node N>
[[nodiscard]] Documentation document(N const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

/// Documents the series @p node: renders it in dialect @p D, each series
/// variable marked, and walks it for the symbol table, whose rows say which
/// quantities are read as a series and how long (`SymbolEntry::shape`,
/// `SymbolEntry::length`). A series is not a `Node` (`expression.hpp`), so it
/// needs this overload rather than the one above.
template <Dialect D = Dialect::Plain, SeriesNode S, Vocabulary V>
[[nodiscard]] Documentation document(S const& node, V const& vocabulary)
{
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents the series @p node in the default vocabulary, which renames
/// nothing.
template <Dialect D = Dialect::Plain, SeriesNode S>
[[nodiscard]] Documentation document(S const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

/// Documents the curve @p node: renders it in dialect @p D, each series marked,
/// and walks both halves for the symbol table. A curve is neither a `Node`
/// nor a series (`curve.hpp`), so it needs this overload.
template <Dialect D = Dialect::Plain, CurveExpression C, Vocabulary V>
[[nodiscard]] Documentation document(C const& node, V const& vocabulary)
{
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents the curve @p node in the default vocabulary, which renames
/// nothing.
template <Dialect D = Dialect::Plain, CurveExpression C>
[[nodiscard]] Documentation document(C const& node)
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
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(node, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    detail::collect(walk, node);
    return std::move(walk.documentation);
}

/// Documents @p node in the default vocabulary, which renames nothing.
template <Dialect D = Dialect::Plain, Predicate P>
[[nodiscard]] Documentation document(Constraint<P> const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

/// Documents a conformity check: its rendering in dialect @p D, its citation
/// when it has one -- the author's own, as a constraint's is -- and the
/// symbol table of its subject.
///
/// The page states the check, never the numbers' origin: an envelope's
/// limits are master data (`conformity.hpp`), and the page shows them as
/// the check was built with them.
template <Dialect D = Dialect::Plain, Unit U, SeriesNode S, Vocabulary V>
[[nodiscard]] Documentation document(Conformity<U, S> const& conformityCheck, V const& vocabulary)
{
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(conformityCheck, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    // An uncited check pushes nothing, for the reason the constraint overload
    // of `collect` gives.
    if (!(conformityCheck.citation == Citation {}))
        walk.documentation.citations.push_back(conformityCheck.citation);
    detail::collect(walk, conformityCheck.subject);
    return std::move(walk.documentation);
}

/// Documents a conformity check in the default vocabulary, which renames
/// nothing.
template <Dialect D = Dialect::Plain, Unit U, SeriesNode S>
[[nodiscard]] Documentation document(Conformity<U, S> const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

} // namespace formula
