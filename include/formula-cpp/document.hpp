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
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounded_transcendental.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/vocabulary.hpp>
#include <formula-cpp/yields.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
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

    /// The expression a calculation calculates this quantity by, rendered in
    /// the dialect `document()` was asked for; empty for a quantity no
    /// calculation calculates -- an input of one, and every row of a page
    /// documenting anything else.
    ///
    /// Not `derivedAs`, which is an overlay's definition, cited, standing in
    /// one formula for a quantity it would otherwise read: this is the
    /// calculation's own, whose result every definition reading the quantity
    /// reads. A quantity an overlay fixed or derived inside a definition of a
    /// calculation that also calculates it has both, on one row.
    std::optional<std::string> calculatedAs {};

    /// For a fixed or derived row: whether the formula ALSO reads this
    /// quantity plainly somewhere -- from the environment, or, for a quantity
    /// a calculation calculates, its calculated value -- besides where an
    /// overlay fixed or defined it. False for a row that is neither, which is
    /// read and nothing else.
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

    /// The role of the record this quantity is read from, as `tag_name`
    /// spells it; empty for a quantity read from the record being evaluated.
    ///
    /// Rows are kept per (role, quantity): `f_c` read here and `f_c` read
    /// from Reference are two inputs, and one merged row would tell a reader
    /// to supply one value where the formula reads two. Set by the walk from
    /// the scope it is inside, and by nothing else.
    ///
    /// Always empty for a row an overlay fixed or derived, wherever it was
    /// met: the fixed value is the overlay's and the definition is one
    /// definition, read from no record, so a constant used here and inside a
    /// scope has one row, not a second labelled with the scope's record. The
    /// quantities a definition reads are inputs like any other, and keyed by
    /// the scope they are read in.
    std::string_view record {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(SymbolEntry const&) const noexcept = default;
};

/// A rejection of outliers the formula holds, as its page states it: the
/// criterion, its limit rendered in the page's dialect and vocabulary, the
/// four parameters that shape the result, and the author's verdict and
/// citation.
struct RejectionEntry
{
    /// Which statistic the criterion compares.
    CriterionKind criterion {};
    /// The limit expression, rendered as the formula is.
    std::string limit {};
    /// How many candidates one pass rejects.
    PerPass perPass {};
    /// What becomes of a determination exactly on the limit.
    OnLimit onLimit {};
    /// k: the most rejected in total.
    std::size_t atMost {};
    /// m: the fewest that may remain.
    std::size_t keepAtLeast {};
    /// What the author declares when the bound is reached.
    Verdict verdict {};
    /// Where the rule comes from.
    Citation citation {};

    /// Memberwise equality.
    [[nodiscard]] bool operator==(RejectionEntry const&) const = default;
};

/// One opaque operation a formula calls, as a documentation page lists it:
/// what ran, why the method uses it there, and what it produces. Its inside
/// is not shown, on the page or in the trace; what is shown is that it ran.
struct OpaqueOperationEntry
{
    /// The operation's name, `Op::name`.
    std::string_view name {};
    /// What the call cited -- empty when it cited nothing, and still an entry.
    Citation citation {};
    /// The operation's output names, in its declared order.
    std::vector<std::string_view> outputs {};
    /// Each output's dimension, index for index with `outputs`, as the call's
    /// inputs gave them: a page states what each output measures.
    std::vector<Dimension> outputDimensions {};

    /// Memberwise equality.
    [[nodiscard]] bool operator==(OpaqueOperationEntry const&) const = default;
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
    /// appear when the formula is read left to right -- after, for a
    /// calculation, the quantities it calculates (see the `Calculation`
    /// overload of `document`).
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
    /// One entry per rejection of outliers the formula holds, in the order
    /// met (`RejectionEntry`). A cited rejection's citation also joins
    /// `citations`.
    std::vector<RejectionEntry> rejections {};
    /// One entry per opaque call the formula makes (`opaque.hpp`), in the
    /// order met, each call once however many of its outputs the formula
    /// uses: an entry even for a call that cited nothing, for
    /// `replacedBy`'s reason. A cited call's citation also joins `citations`.
    std::vector<OpaqueOperationEntry> opaqueOperations {};
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

    /// A distinct address per record role @p Role, for keying the symbol
    /// table by role as well as by quantity. Writable, `inline` and not
    /// `constexpr`, for exactly the reasons `quantityIdentity` gives. The
    /// record being evaluated has no identity of its own: its rows are keyed
    /// by a null role.
    template <typename Role>
    inline bool roleIdentity = false;

    /// Which row a walk has already added: the role it was read from (null
    /// for the record being evaluated) and the quantity.
    struct SeenRow
    {
        void const* role;
        void const* quantity;
    };

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
        std::vector<SeenRow> seenQuantities {};
        /// Which call type each of `documentation.opaqueOperations` came from,
        /// index for index, so that two outputs of one call are one entry.
        std::vector<void const*> seenCalls {};
        /// The dialect `document()` was asked for, which a derived quantity's
        /// definition is rendered in.
        Dialect dialect = Dialect::Plain;
        V vocabulary;
        /// While a `from_record` scope's operand is walked: that scope's role,
        /// by name and by identity; empty and null otherwise. Every row added
        /// meanwhile is that record's.
        std::string_view role {};
        void const* roleIdentity = nullptr;
        /// While a retry is walked: the attempts it allows, the length of the
        /// series `attempt_input` reads; 0 otherwise.
        std::size_t attemptLimit = 0;
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

    template <Vocabulary V>
    void collect(Walk<V>& walk, AttemptNumberNode const& node);

    template <Vocabulary V, Described R>
    void collect(Walk<V>& walk, PreviousAttemptNode<R> const& node);

    template <Vocabulary V, Described R>
    void collect(Walk<V>& walk, ThisAttemptNode<R> const& node);

    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, AttemptInputNode<Q> const& node);

    template <Vocabulary V, UnaryOperator Op, Node Operand>
    void collect(Walk<V>& walk, UnaryNode<Op, Operand> const& node);

    template <Vocabulary V, int Exponent, Node Operand>
    void collect(Walk<V>& walk, PowerNode<Exponent, Operand> const& node);

    template <Vocabulary V, int Degree, Node Operand>
    void collect(Walk<V>& walk, RootNode<Degree, Operand> const& node);

    template <Vocabulary V, Transcendental F, Node Operand>
    void collect(Walk<V>& walk, TranscendentalNode<F, Operand> const& node);

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

    template <Vocabulary V, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundedTranscendentalNode<F, Places, Mode, Operand> const& node);

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

    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, PassMeanNode<Q> const& node);

    template <Vocabulary V>
    void collect(Walk<V>& walk, PassCountNode const& node);

    template <Vocabulary V, PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    void collect(Walk<V>& walk, RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node);

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

    template <Vocabulary V, typename Role, typename Requirement, Node Operand>
    void collect(Walk<V>& walk, RecordScopeNode<Role, Requirement, Operand> const& node);

    template <Vocabulary V, typename Operand>
    void collect(Walk<V>& walk, RefusedSeriesScope<Operand> const& node);

    template <Vocabulary V, std::size_t I, typename Op, typename... Inputs, typename Origin>
    void collect(Walk<V>& walk, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin> const& node);

    template <Vocabulary V,
              std::size_t I,
              typename Op,
              typename... Inputs,
              Unit U,
              DecimalPlaces Places,
              RoundingMode Mode,
              typename Origin>
    void collect(Walk<V>& walk, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node);

    /// A distinct address per opaque call type, for `quantityIdentity`'s
    /// reason and in its writable form.
    template <typename Call>
    inline bool callIdentity = false;

    /// Finds @p Q's row in the symbol table, adding a plain one when @p Q has
    /// none yet; @p row is its index. True when the row was added now.
    ///
    /// Deduplicated by quantity type, and by the record it is read from --
    /// see `SymbolEntry` and `SymbolEntry::record`. `seenQuantities` and
    /// `symbols` grow together, so one index names both.
    ///
    /// @p readFromRecord is false for a row an overlay fixed or derived, which
    /// is keyed as this record's wherever it is met -- see
    /// `SymbolEntry::record`.
    template <Described Q, Vocabulary V>
    bool add_row(Walk<V>& walk, std::size_t& row, bool readFromRecord = true)
    {
        void const* const identity = &quantityIdentity<Q>;
        void const* const rowRole = readFromRecord ? walk.roleIdentity : nullptr;
        row = 0;
        while (row < walk.seenQuantities.size()
               && (walk.seenQuantities[row].quantity != identity || walk.seenQuantities[row].role != rowRole))
            ++row;
        if (row < walk.seenQuantities.size())
            return false;
        walk.seenQuantities.push_back(SeenRow { .role = rowRole, .quantity = identity });
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .record = readFromRecord ? walk.role : std::string_view {} });
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
        bool const added = add_row<Q>(walk, symbolRow, false);
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
        bool const added = add_row<Q>(walk, symbolRow, false);
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

    /// A retry's context nodes read no variable: the attempt number is the
    /// method's own counter, and the result's row is the retry's own, added
    /// first by `document()` for a retry and marked there as iterated.
    template <Vocabulary V>
    void collect(Walk<V>&, AttemptNumberNode const&)
    {
    }

    template <Vocabulary V, Described R>
    void collect(Walk<V>&, PreviousAttemptNode<R> const&)
    {
    }

    template <Vocabulary V, Described R>
    void collect(Walk<V>&, ThisAttemptNode<R> const&)
    {
    }

    /// A distinct address per quantity @p Q read by `attempt_input`, for
    /// `seriesIdentity`'s reason: its row is its own, whatever else reads `Q`.
    template <typename Q>
    inline bool attemptInputIdentity = false;

    /// The determinations a retry reads, one per attempt, contribute one row:
    /// a series of as many values as the retry allows attempts
    /// (`Walk::attemptLimit`), which is what the environment must hold.
    template <Vocabulary V, Described Q>
    void collect(Walk<V>& walk, AttemptInputNode<Q> const&)
    {
        // Outside a retry there is no series to name; `document()` refuses
        // such a formula, so this only keeps a walk begun by hand honest.
        if (walk.attemptLimit == 0)
            return;
        void const* const identity = &attemptInputIdentity<Q>;
        for (SeenRow const& seen: walk.seenQuantities)
            if (seen.quantity == identity && seen.role == walk.roleIdentity)
                return;
        // `series<Q, Max>` read too names the same entry of the environment:
        // one row, not two.
        for (SymbolEntry const& listed: walk.documentation.symbols)
            if (listed.shape == ValueShape::Series && listed.length == walk.attemptLimit
                && listed.symbol == symbol_of<Q>(walk.vocabulary) && listed.record == walk.role
                && listed.unit == Describe<Q>::unit)
                return;
        walk.seenQuantities.push_back(SeenRow { .role = walk.roleIdentity, .quantity = identity });
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .shape = ValueShape::Series,
                                                           .length = walk.attemptLimit,
                                                           .record = walk.role });
    }

    /// Words appended to a retry's result's description, so that its row says
    /// the value is iterated. A description rather than a `ValueShape`: the
    /// shape says what a quantity is read as, and an iterated result is still
    /// one value.
    inline constexpr std::string_view iteratedSuffix = " (iterated: the value of the attempt a retry accepted)";

    /// @p R's description followed by `iteratedSuffix`, in static storage so
    /// that the row's view outlives the page.
    template <Described R>
    struct IteratedDescription
    {
        static constexpr std::size_t length = Describe<R>::description.size() + iteratedSuffix.size();
        static constexpr std::array<char, length> text = [] {
            std::array<char, length> joined {};
            std::size_t at = 0;
            for (char const spelt: Describe<R>::description)
                joined[at++] = spelt;
            for (char const spelt: iteratedSuffix)
                joined[at++] = spelt;
            return joined;
        }();
        static constexpr std::string_view view { text.data(), length };
    };
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

    /// A logarithm or an exponential reads what its argument reads.
    template <Vocabulary V, Transcendental F, Node Operand>
    void collect(Walk<V>& walk, TranscendentalNode<F, Operand> const& node)
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

    /// A rounded logarithm or exponential reads what its argument reads, as a rounding node reads what
    /// its operand does.
    template <Vocabulary V, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    void collect(Walk<V>& walk, RoundedTranscendentalNode<F, Places, Mode, Operand> const& node)
    {
        collect(walk, node.operand);
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
    /// length, from the same record. Deduplicated on quantity, shape and
    /// length (`seriesIdentity`) and on the record it is read from, as a
    /// variable is (`SymbolEntry::record`), so a single value of the same
    /// quantity, a series of it over another length, or the same series read
    /// from another record, is a row of its own.
    template <Vocabulary V, Described Q, std::size_t N>
    void collect(Walk<V>& walk, SeriesVarNode<Q, N> const&)
    {
        void const* const identity = &seriesIdentity<Q, N>;
        for (SeenRow const& seen: walk.seenQuantities)
        {
            if (seen.quantity == identity && seen.role == walk.roleIdentity)
                return;
            // The retry's recorded determinations of `Q`, one per attempt,
            // are this series already: one row.
            if (seen.quantity == &attemptInputIdentity<Q> && seen.role == walk.roleIdentity && walk.attemptLimit == N)
                return;
        }
        walk.seenQuantities.push_back(SeenRow { .role = walk.roleIdentity, .quantity = identity });
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .shape = ValueShape::Series,
                                                           .length = N,
                                                           .record = walk.role });
    }

    /// Raw observations contribute one row, marked as observations of at
    /// most @p Capacity -- unless the same quantity has already contributed
    /// such a row of that capacity, from the same record. Deduplicated as a
    /// series variable is, on quantity, shape and capacity
    /// (`observationsIdentity`) and on the record it is read from.
    template <Vocabulary V, Described Q, std::size_t Capacity>
    void collect(Walk<V>& walk, ObservationsVarNode<Q, Capacity> const&)
    {
        void const* const identity = &observationsIdentity<Q, Capacity>;
        for (SeenRow const& seen: walk.seenQuantities)
            if (seen.quantity == identity && seen.role == walk.roleIdentity)
                return;
        walk.seenQuantities.push_back(SeenRow { .role = walk.roleIdentity, .quantity = identity });
        walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                                           .description = Describe<Q>::description,
                                                           .unit = Describe<Q>::unit,
                                                           .shape = ValueShape::Observations,
                                                           .length = Capacity,
                                                           .record = walk.role });
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

    /// A pass placeholder names no quantity of its own: it is a value the
    /// rejection computed, not one measured.
    template <Vocabulary V, Described Q>
    void collect(Walk<V>&, PassMeanNode<Q> const&)
    {
    }

    template <Vocabulary V>
    void collect(Walk<V>&, PassCountNode const&)
    {
    }

    /// A rejection lists its sample's row and whatever its limit reads, and
    /// states itself: criterion, limit, the four parameters, verdict and
    /// citation (`RejectionEntry`).
    template <Vocabulary V, PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    void collect(Walk<V>& walk, RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node)
    {
        Citation const& cited = node.citation;
        if (!cited.title.empty() || !cited.reference.empty() || !cited.section.empty() || !cited.equation.empty())
            walk.documentation.citations.push_back(cited);
        collect(walk, node.sample);
        collect(walk, node.criterion.limit);
        walk.documentation.rejections.push_back(
            RejectionEntry { .criterion = Criterion::kind,
                             .limit = render_in(walk.dialect, node.criterion.limit, walk.vocabulary),
                             .perPass = P,
                             .onLimit = L,
                             .atMost = detail::bound_value<AtMostT>,
                             .keepAtLeast = detail::bound_value<KeepAtLeastT>,
                             .verdict = node.verdict,
                             .citation = node.citation });
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
    /// A read from another record contributes its operand's rows, each keyed
    /// by the scope's role as well as its quantity, so a quantity read both
    /// here and there has a row for each. The walk's role is restored after,
    /// for the rows that follow the scope.
    template <Vocabulary V, typename Role, typename Requirement, Node Operand>
    void collect(Walk<V>& walk, RecordScopeNode<Role, Requirement, Operand> const& node)
    {
        std::string_view const outerRole = walk.role;
        void const* const outerRoleIdentity = walk.roleIdentity;
        walk.role = tag_name<Role>();
        walk.roleIdentity = &roleIdentity<Role>;
        collect(walk, node.operand);
        walk.role = outerRole;
        walk.roleIdentity = outerRoleIdentity;
    }

    /// A refused series-valued read from another record names nothing, as a
    /// refused series names nothing: it only keeps `document` from adding a
    /// second error to the refusal that produced it.
    template <Vocabulary V, typename Operand>
    void collect(Walk<V>&, RefusedSeriesScope<Operand> const&)
    {
    }

    /// An opaque output lists its call -- once per call, however many of its
    /// outputs are used: one call is one call type with one citation -- and
    /// walks the call's inputs, which name its variables.
    template <Vocabulary V, std::size_t I, typename Op, typename... Inputs, typename Origin>
    void collect(Walk<V>& walk, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin> const& node)
    {
        void const* const identity = &callIdentity<OpaqueCall<Op, Inputs...>>;
        bool metAlready = false;
        for (std::size_t entryAt = 0; entryAt < walk.seenCalls.size(); ++entryAt)
            if (walk.seenCalls[entryAt] == identity
                && walk.documentation.opaqueOperations[entryAt].citation == node.call.citation)
                metAlready = true;
        if (!metAlready)
        {
            walk.seenCalls.push_back(identity);
            walk.documentation.opaqueOperations.push_back(OpaqueOperationEntry {
                .name = Op::name,
                .citation = node.call.citation,
                .outputs = std::vector<std::string_view>(Op::outputs.begin(), Op::outputs.end()),
                .outputDimensions = std::vector<Dimension>(OpaqueCall<Op, Inputs...>::output_dimensions.begin(),
                                                           OpaqueCall<Op, Inputs...>::output_dimensions.end()) });
            if (!(node.call.citation == Citation {}))
                walk.documentation.citations.push_back(node.call.citation);
        }
        std::apply([&](auto const&... inputs) { (collect(walk, inputs), ...); }, node.call.inputs);
    }

    /// A rounded opaque output lists its call as an output of that call does
    /// -- once per call, whichever outputs are used and whether they are
    /// rounded -- and reads what the call's inputs read. The precision is in
    /// the formula's text already.
    template <Vocabulary V,
              std::size_t I,
              typename Op,
              typename... Inputs,
              Unit U,
              DecimalPlaces Places,
              RoundingMode Mode,
              typename Origin>
    void collect(Walk<V>& walk, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& node)
    {
        collect(walk, unrounded(node));
    }

    /// Asks `RequireAttemptInputOnlyInRetry` of each of the definitions
    /// @p Ds: a calculation documented is no retry's, as a formula
    /// documented on its own is not.
    template <typename... Ds>
    struct CalculationDocumentChecks
    {
        static_assert((RequireAttemptInputOnlyInRetry<std::remove_cv_t<decltype(Ds::expression)>>::value && ...));

        /// True: every check is asked above.
        static constexpr bool value = true;
    };

    /// Walks the definitions of @p definitionSet for the rows and citations
    /// they hold, in the order the calculation calculates them in, which
    /// @p Placed counts along.
    template <Vocabulary V, typename... Ds, std::size_t... Placed>
    void collect_definitions(Walk<V>& walk, Calculation<Ds...> const& definitionSet, std::index_sequence<Placed...>)
    {
        using Graph = CalculationGraph<Ds...>;
        (collect(walk,
                 std::get<Graph::order[Graph::inputCount + Placed] - Graph::inputCount>(definitionSet.definitions)
                     .expression),
         ...);
    }

    /// The row of the quantity the definition at @p Defined calculates,
    /// with that definition: the row the walk of the definitions gave the
    /// quantity -- read, or fixed or derived by an overlay -- when it gave
    /// one, and a plain one when it did not. The walk's row is marked in
    /// @p taken, so that it is not listed a second time.
    ///
    /// Merged after the walk rather than added before it: a row already there
    /// when an overlay's constant is met is taken for one a plain read added
    /// (`collect` for `OverriddenConstantNode`), which a calculated row is
    /// not.
    template <std::size_t Defined, Vocabulary V, typename... Ds>
    [[nodiscard]] SymbolEntry calculated_row(Walk<V> const& walk,
                                             Calculation<Ds...> const& definitionSet,
                                             std::vector<bool>& taken)
    {
        using Q = typename std::tuple_element_t<Defined, std::tuple<Ds...>>::quantity;
        SymbolEntry calculatedEntry { .symbol = symbol_of<Q>(walk.vocabulary),
                                      .description = Describe<Q>::description,
                                      .unit = Describe<Q>::unit };
        for (std::size_t walkedRow = 0; walkedRow < walk.seenQuantities.size(); ++walkedRow)
            if (walk.seenQuantities[walkedRow].quantity == &quantityIdentity<Q>
                && walk.seenQuantities[walkedRow].role == nullptr)
            {
                calculatedEntry = walk.documentation.symbols[walkedRow];
                taken[walkedRow] = true;
            }
        calculatedEntry.calculatedAs =
            render_in(walk.dialect, std::get<Defined>(definitionSet.definitions).expression, walk.vocabulary);
        return calculatedEntry;
    }

    /// The rows of the quantities @p definitionSet calculates, each with its
    /// definition, in the order the calculation calculates them in, which
    /// @p Placed counts along.
    template <Vocabulary V, typename... Ds, std::size_t... Placed>
    [[nodiscard]] std::vector<SymbolEntry> calculated_rows(Walk<V> const& walk,
                                                           Calculation<Ds...> const& definitionSet,
                                                           std::vector<bool>& taken,
                                                           std::index_sequence<Placed...>)
    {
        using Graph = CalculationGraph<Ds...>;
        return { calculated_row<Graph::order[Graph::inputCount + Placed] - Graph::inputCount>(
            walk, definitionSet, taken)... };
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
    // A formula documented on its own is no retry's: see
    // `detail::RequireAttemptInputOnlyInRetry`.
    static_assert(detail::RequireAttemptInputOnlyInRetry<N>::value);
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
    // A formula documented on its own is no retry's: see
    // `detail::RequireAttemptInputOnlyInRetry`.
    static_assert(detail::RequireAttemptInputOnlyInRetry<S>::value);
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
    // A formula documented on its own is no retry's: see
    // `detail::RequireAttemptInputOnlyInRetry`.
    static_assert(detail::RequireAttemptInputOnlyInRetry<C>::value);
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
    // A formula documented on its own is no retry's: see
    // `detail::RequireAttemptInputOnlyInRetry`.
    static_assert(detail::RequireAttemptInputOnlyInRetry<Constraint<P>>::value);
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

/// Documents a retry: renders it in dialect @p D (`render()` for a retry),
/// lists its citation, and gives its result the first symbol row, whose
/// description says the value is iterated (`detail::IteratedDescription`);
/// then the rows of what its starting value, its attempt and its acceptance
/// read.
template <Dialect D = Dialect::Plain,
          Described R,
          std::size_t Max,
          FirstJudged J,
          typename Start,
          typename A,
          typename P,
          Vocabulary V>
[[nodiscard]] Documentation document(Retry<R, Max, J, Start, A, P> const& retrying, V const& vocabulary)
{
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(retrying, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary,
                           .attemptLimit = Max };
    if (!(retrying.citation == Citation {}))
        walk.documentation.citations.push_back(retrying.citation);
    walk.seenQuantities.push_back(detail::SeenRow { .role = walk.roleIdentity, .quantity = &detail::quantityIdentity<R> });
    walk.documentation.symbols.push_back(SymbolEntry { .symbol = symbol_of<R>(vocabulary),
                                                       .description = detail::IteratedDescription<R>::view,
                                                       .unit = Describe<R>::unit });
    if constexpr (detail::StartTraits<Start>::states)
        detail::collect(walk, retrying.start.expression);
    detail::collect(walk, retrying.attempt);
    detail::collect(walk, retrying.accept);
    return std::move(walk.documentation);
}

/// Documents a retry in the default vocabulary, which renames nothing.
template <Dialect D = Dialect::Plain, Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
[[nodiscard]] Documentation document(Retry<R, Max, J, Start, A, P> const& node)
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
    // A formula documented on its own is no retry's: see
    // `detail::RequireAttemptInputOnlyInRetry`.
    static_assert(detail::RequireAttemptInputOnlyInRetry<Conformity<U, S>>::value);
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

/// Documents the calculation @p definitionSet: renders it in dialect @p D,
/// one line per definition as `render` writes a calculation, and walks each
/// definition for the citations and symbol table a documentation page
/// needs.
///
/// The symbol table opens with one row per calculated quantity, in the
/// order the calculation calculates them in, each with its definition
/// rendered in dialect @p D (`SymbolEntry::calculatedAs`). The inputs
/// follow, each once, in the order the definitions first read them when
/// read in that order -- the order they first appear in `formula`, which can
/// differ from the order `inputs_of` and `describe_graph` list them in, the
/// order the calculation first met them in its definitions as given. The
/// citations are what the `Node` overload collects from each definition --
/// each `documented()` citation among them -- definition by definition in
/// that order, each definition's outermost first, and each as often as it
/// is met.
///
/// Every symbol is written as @p vocabulary says, as in the `Node`
/// overload. A calculation refused where it was written is documented as
/// an empty page, and nothing more is said of it. A definition reading
/// `attempt_input` is refused here, as a formula documented on its own is.
template <Dialect D = Dialect::Plain, typename... Ds, Vocabulary V>
[[nodiscard]] Documentation document(Calculation<Ds...> const& definitionSet, V const& vocabulary)
{
    using Graph = detail::CalculationGraph<Ds...>;
    static_assert(std::conditional_t<Graph::valid, detail::CalculationDocumentChecks<Ds...>, std::true_type>::value);
    detail::Walk<V> walk { .documentation = Documentation { .formula = render<D>(definitionSet, vocabulary) },
                           .seenQuantities = {},
                           .dialect = D,
                           .vocabulary = vocabulary };
    if constexpr (Graph::valid)
    {
        detail::collect_definitions(walk, definitionSet, std::make_index_sequence<sizeof...(Ds)> {});
        std::vector<bool> taken(walk.documentation.symbols.size(), false);
        std::vector<SymbolEntry> pageRows =
            detail::calculated_rows(walk, definitionSet, taken, std::make_index_sequence<sizeof...(Ds)> {});
        for (std::size_t walkedRow = 0; walkedRow < taken.size(); ++walkedRow)
            if (!taken[walkedRow])
                pageRows.push_back(std::move(walk.documentation.symbols[walkedRow]));
        walk.documentation.symbols = std::move(pageRows);
    }
    return std::move(walk.documentation);
}

/// Documents a calculation in the default vocabulary, which renames
/// nothing.
template <Dialect D = Dialect::Plain, typename... Ds>
[[nodiscard]] Documentation document(Calculation<Ds...> const& node)
{
    return document<D>(node, DefaultVocabulary {});
}

/// Documents the formula @p boundFormula holds (`yields.hpp`) as
/// `document<D>(boundFormula.expression, vocabulary)` does: the same page,
/// every symbol as @p vocabulary says. The result quantity is not added to
/// it -- rendering names no result.
template <Dialect D = Dialect::Plain, Described Q, typename E, Vocabulary V = DefaultVocabulary>
[[nodiscard]] Documentation document(Yields<Q, E> const& boundFormula, V const& vocabulary = V {})
{
    return document<D>(boundFormula.expression, vocabulary);
}

/// Documents @p node as `document<D>(node, vocabulary)` does, with every
/// number the page writes -- in the formula's text, a derived quantity's
/// derivation and a criterion's limit -- written as @p renderOptions says
/// (`RenderOptions`, `render.hpp`). Everything else on the page is the same
/// either way: a constant an overlay fixed is held as a `Rational`
/// (`SymbolEntry::fixedValue`), for the caller to write as it chooses.
template <Dialect D = Dialect::Plain, typename X, Vocabulary V>
    requires requires(X const& written, V const& writtenIn) { document<D>(written, writtenIn); }
[[nodiscard]] Documentation document(X const& node, V const& vocabulary, RenderOptions renderOptions)
{
    return document<D>(node, detail::styled(vocabulary, renderOptions.numbers));
}

/// Documents @p node as `document<D>(node, DefaultVocabulary {}, renderOptions)`
/// does: every symbol as `Describe<Q>::symbol` says and every number as
/// @p renderOptions says, without naming a vocabulary that renames nothing.
template <Dialect D = Dialect::Plain, typename X>
    requires requires(X const& written, DefaultVocabulary const& byDefault) { document<D>(written, byDefault); }
[[nodiscard]] Documentation document(X const& node, RenderOptions renderOptions)
{
    return document<D>(node, DefaultVocabulary {}, renderOptions);
}

} // namespace formula
