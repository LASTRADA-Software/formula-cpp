// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Calculations: each quantity bound to the expression that calculates it,
/// and the dependency graph those definitions make, known and checked at
/// compile time.
///
/// `define<Q>(expression)` says that the quantity `Q` is calculated by
/// `expression`, and returns a `Definition`. Which quantities that expression
/// reads is known when it is compiled, not discovered when it is evaluated:
/// the walk below lists them, each once, in the order the expression first
/// reads them. That list is what lets a calculation made of definitions know
/// its dependency graph before anything runs.
///
/// **What the walk lists.** A `var<Q>` reads `Q`. Any other node reads what
/// its children read, as `detail::LevelChildren` (`precision.hpp`) lists
/// them. Every node kind this library ships has an entry there but three,
/// which the walk handles before asking: a read from another record
/// (`from_record`, refused below), and the two stand-ins a refusal leaves
/// behind -- a series-valued `from_record`, and arithmetic over a retry --
/// which read nothing. A precision limit reads what its level and its limit
/// expression read, since both are evaluated against the same environment.
/// An overlay's fixed constant reads nothing -- it never asks the environment
/// for its value -- and an overlay's derived quantity reads what its
/// definition reads, not the quantity it stands for. The placeholders a
/// construct binds (`precision_level`, `pass_mean`, `pass_count`) read
/// nothing: they stand for a value the construct works out.
///
/// **A retry's context is listed as reading nothing** (`attempt_number`,
/// `previous_attempt`, `this_attempt`, `attempt_input`), although inside a
/// retry `attempt_input<Q>` reads the series recorded for `Q`. A retry is not
/// a `Node`, so no definition can hold one, and outside a retry the evaluator
/// refuses each of these nodes (`RequireInsideRetry`,
/// `RequireAttemptInputInsideRetry`, `retry.hpp`).
///
/// **A `when()` lists what it may read**, not what one evaluation reads: its
/// condition and both branches, although it evaluates only the branch it
/// takes. So the list may name a quantity one evaluation does not read, and
/// never leaves out one it does.
///
/// **Refused where the definition is written, each with one message:**
///  - an expression of a different dimension from `Q`'s
///    (`RequireDefinitionMeasuresQuantity`);
///  - a series in place of the expression (`RequireSingleValueDefinition`);
///  - a quantity read as a series or as raw observations inside it
///    (`RequireSingleValueReadsInCalculation`) -- a calculation holds single
///    values;
///  - a read from another record, `from_record` (`RequireNoRecordReadInCalculation`)
///    -- a calculation reads its own values;
///  - a node kind of a consumer's own (`RequireCalculationSeesNode`): the walk
///    cannot see inside it, and a quantity read there would be missing from
///    the graph.
///
/// Every check but the series one sits in `Definition`'s class body, so a
/// definition spelled without `define` is refused as well; a series cannot
/// be spelled into one at all, since `Definition` takes a `Node`.
///
/// **A calculation** is a set of definitions: `calculation(define<A>(...),
/// define<B>(...))`. A quantity its definitions read and none of them defines
/// is an *input*. Inputs and defined quantities together are the
/// calculation's quantities, at most 64 of them; the inputs are numbered in
/// the order they are first read (the definitions in the order given, each
/// read left to right), then the defined quantities in the order given. The
/// graph over them -- what each reads, what reads it, what each depends on
/// through any chain of reads and what depends on it -- is worked out at
/// compile time, and so is the *dependency order*: the inputs first, then
/// each defined quantity as soon as everything it reads comes before it,
/// taking the one given first whenever several could come next. So
/// definitions given in dependency order keep their order, and definitions
/// given out of order are reordered.
///
/// The queries (`dependencies_of`, `dependents_of`, `upstream_of`,
/// `affected_by`, `inputs_of`, `calculation_order`) answer a
/// `std::array<std::string_view, N>` of symbols, written as the vocabulary
/// they are given says, in dependency order; `depends_on` answers whether one
/// quantity depends on another through any chain of reads.
///
/// **Refused where the calculation is written, each with one message, and
/// each later check asked only once the earlier ones hold:**
///  - an argument that is not a definition (`RequireDefinitions`), and no
///    argument at all (`RequireSomeDefinition`);
///  - one quantity defined twice (`RequireDistinctDefinitions`);
///  - more than 64 quantities (`RequireCalculationWithinCapacity`);
///  - a definition that reads the quantity it defines
///    (`RequireDefinitionNotSelfReferential`);
///  - definitions that read one another in a cycle, all of the cycle's
///    quantities named in one message (`RequireAcyclicDefinitions`).
///
/// So a quantity defined twice, one definition reading itself, is refused
/// only as defined twice; and a definition that reads itself and also sits
/// on a longer cycle is refused as reading itself, the cycle judged once it
/// no longer does. Two definitions that each read themselves are two
/// mistakes, and draw a message each.
///
/// A calculation holding a definition refused where it was written is asked
/// none of the checks after the first item, and a query over a refused
/// calculation answers nothing and says nothing more. A query about a
/// quantity the calculation neither defines nor reads is refused
/// (`RequireCalculationQuantity`).

#include <formula-cpp/binning.hpp>
#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/series.hpp>

#include <formula-cpp/vocabulary.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

namespace detail
{
    /// Fails to compile when `define<Q>` is given an expression of a
    /// different dimension from `Q`'s. Not asked of an expression refused
    /// already: its one message has been given, and its dimension is a
    /// stand-in.
    template <typename Q, typename Expr>
    struct RequireDefinitionMeasuresQuantity
    {
        static_assert(refused_already<Expr>() || Expr::dimension == Describe<Q>::dimension,
                      "formula: this definition's expression measures a different dimension from the quantity it "
                      "defines; every read of the quantity would be a value it does not measure -- the quantity and "
                      "the expression appear in this diagnostic as the template arguments Q and Expr of "
                      "RequireDefinitionMeasuresQuantity");

        static constexpr bool value = true;
    };

    /// Fails to compile when `define<Q>` is given a series.
    template <typename Expr>
    struct RequireSingleValueDefinition
    {
        static_assert(!SeriesNode<Expr>,
                      "formula: define<Q> takes an expression of one value, and this is a series; a calculation "
                      "holds single values -- the expression appears in this diagnostic as the template argument "
                      "of RequireSingleValueDefinition");

        static constexpr bool value = true;
    };

    /// Fails to compile when a definition reads @p Q as a series or as raw
    /// observations. Instantiated only where the walk meets such a read.
    template <typename Q>
    struct RequireSingleValueReadsInCalculation
    {
        static_assert(alwaysFalse<Q>,
                      "formula: a calculation holds single values, and this definition reads a quantity as a series "
                      "or as raw observations; read it with var<Q> -- the quantity appears in this diagnostic as the "
                      "template argument Q of RequireSingleValueReadsInCalculation");

        static constexpr bool value = true;
    };

    /// Fails to compile when a definition reads from the record bound to
    /// @p Role. Instantiated only where the walk meets such a read.
    template <typename Role>
    struct RequireNoRecordReadInCalculation
    {
        static_assert(alwaysFalse<Role>,
                      "formula: a calculation's definitions read the worksheet's own values, and this one reads from "
                      "another record with from_record; evaluate that formula against a record_context instead -- "
                      "the role appears in this diagnostic as the template argument of "
                      "RequireNoRecordReadInCalculation");

        static constexpr bool value = true;
    };

    /// Fails to compile when a definition holds a node kind the walk cannot
    /// see inside -- a consumer's own. A quantity read inside it would be
    /// missing from the graph, and its value read out of date. Instantiated
    /// only for such a kind.
    template <typename N>
    struct RequireCalculationSeesNode
    {
        static_assert(alwaysFalse<N>,
                      "formula: this definition holds a node kind the calculation cannot see inside; a quantity read "
                      "there would be missing from the dependency graph and read out of date, so the calculation "
                      "refuses it -- the node kind appears in this diagnostic as the template argument N of "
                      "RequireCalculationSeesNode");

        static constexpr bool value = true;
    };

    /// `RequireCalculationSeesNode` for @p N, which the walk could not see
    /// inside, asked only when @p N is a consumer's kind. A library kind
    /// that reached `LevelChildren`'s primary template, or one whose
    /// spelling cannot be read, has been refused there already
    /// (`RequireLevelChildrenFor`, `precision.hpp`); a second message would
    /// report that one omission twice.
    template <typename N>
    struct RequireCalculationSeesConsumerNode
    {
        static constexpr bool refusedByEntry =
            type_argument_text(kind_probe::type_signature<N>()).empty() || declared_in_library<N>();

        static_assert(std::conditional_t<refusedByEntry, std::true_type, RequireCalculationSeesNode<N>>::value);

        static constexpr bool value = true;
    };

    template <typename N>
    struct CalculationReads;

    /// The reads of every node in the tuple @p Children, joined, and whether
    /// the walk accepted every one.
    template <typename Children>
    struct CalculationReadsOfAll;

    template <typename... Children>
    struct CalculationReadsOfAll<std::tuple<Children...>>
    {
        /// Each child's reads, in the children's order; a quantity two
        /// children read is listed twice.
        using type = typename JoinQuantities<typename CalculationReads<std::remove_cv_t<Children>>::type...>::type;
        /// Whether the walk accepted every child.
        static constexpr bool accepted = (CalculationReads<std::remove_cv_t<Children>>::accepted && ...);
    };

    /// What a node kind's `LevelChildren` entry lists besides its children:
    /// the expressions it binds a placeholder in, which it evaluates against
    /// the same environment -- a precision limit's limit expression. Nothing
    /// for any other kind.
    template <typename Entry>
    struct BoundChildren
    {
        /// No expression.
        using type = std::tuple<>;
    };

    template <typename Entry>
        requires requires { typename Entry::bound; }
    struct BoundChildren<Entry>
    {
        /// The entry's bound expressions.
        using type = typename Entry::bound;
    };

    /// What a node refused already reads: nothing, and the walk does not
    /// accept it. Its one message has been given; its children are
    /// stand-ins, and asking about them could add a second (a
    /// `RefusedRetryValue` has no `LevelChildren` entry at all).
    struct RefusedReads
    {
        /// Nothing.
        using type = QuantityList<>;
        /// Not accepted, so that nothing built on the definition judges it
        /// further.
        static constexpr bool accepted = false;
    };

    /// What a node kind @p N reads, through its `LevelChildren` entry: what
    /// its children read, and what the expressions it binds read. A kind
    /// with no entry of its own is a consumer's, which the walk cannot see
    /// inside, and is refused (`RequireCalculationSeesNode`); a library kind
    /// with none is refused by the entry's own primary template
    /// (`RequireLevelChildrenFor`).
    template <typename N>
    struct CalculationReadsInRegistry
    {
      private:
        using Entry = LevelChildren<N>;

        static_assert(std::conditional_t<Entry::seen, std::true_type, RequireCalculationSeesConsumerNode<N>>::value);

        using Children = CalculationReadsOfAll<typename Entry::type>;
        using Bound = CalculationReadsOfAll<typename BoundChildren<Entry>::type>;

      public:
        /// Each quantity read, once, in the order first read.
        using type = UniqueQuantities<typename JoinQuantities<typename Children::type, typename Bound::type>::type>;
        /// Whether the walk accepted this node and everything inside it.
        static constexpr bool accepted = Entry::seen && Children::accepted && Bound::accepted;
    };

    /// The quantities a node of kind @p N reads from the environment it is
    /// evaluated against: each once, in the order first read, walking the
    /// tree left to right. @p N is never cv-qualified here;
    /// `CalculationReadsOf` strips it.
    ///
    /// `accepted` says whether the walk accepted every node: false wherever a
    /// refusal of this header's or an earlier one fired, so that a check built
    /// on the list can stay silent rather than add a second message.
    template <typename N>
    struct CalculationReads: std::conditional_t<refused_already<N>(), RefusedReads, CalculationReadsInRegistry<N>>
    {
    };

    /// A variable reads its quantity. An exact match: an overlay's constant
    /// and derived quantity derive from `VarNode` and are walked through
    /// their own entries -- a constant reads nothing, a derived quantity
    /// what its definition reads.
    template <Described Q>
    struct CalculationReads<VarNode<Q>>
    {
        /// The quantity.
        using type = QuantityList<Q>;
        /// Always.
        static constexpr bool accepted = true;
    };

    /// A series variable: refused, a calculation holds single values.
    template <Described Q, std::size_t N>
    struct CalculationReads<SeriesVarNode<Q, N>>
    {
        static_assert(RequireSingleValueReadsInCalculation<Q>::value);

        /// Nothing, after the refusal.
        using type = QuantityList<>;
        /// Never.
        static constexpr bool accepted = false;
    };

    /// Raw observations: refused, as a series is.
    template <Described Q, std::size_t Capacity>
    struct CalculationReads<ObservationsVarNode<Q, Capacity>>
    {
        static_assert(RequireSingleValueReadsInCalculation<Q>::value);

        /// Nothing, after the refusal.
        using type = QuantityList<>;
        /// Never.
        static constexpr bool accepted = false;
    };

    /// A read from another record: refused, a calculation reads its own
    /// values. Its operand is not walked: what it reads, it reads from the
    /// other record.
    template <typename Role, typename Requirement, Node Operand>
    struct CalculationReads<RecordScopeNode<Role, Requirement, Operand>>
    {
        static_assert(RequireNoRecordReadInCalculation<Role>::value);

        /// Nothing, after the refusal.
        using type = QuantityList<>;
        /// Never.
        static constexpr bool accepted = false;
    };

    /// A series-valued `from_record`, refused where it was written
    /// (`RequireSingleValueScopeOperand`, `record.hpp`): nothing, and no
    /// second message.
    template <typename Operand>
    struct CalculationReads<RefusedSeriesScope<Operand>>: RefusedReads
    {
    };

    /// The quantities @p Expr reads, each once, in the order first read --
    /// see `CalculationReads`.
    template <typename Expr>
    using CalculationReadsOf = typename CalculationReads<std::remove_cv_t<Expr>>::type;
} // namespace detail

/// A quantity bound to the expression that calculates it: built by
/// `define<Q>(expression)`, and refused where it is written for any rule the
/// file comment lists.
///
/// A public aggregate: its checks sit in the class body, so one spelled
/// without `define` is refused as well. What it reads is a property of its
/// type, which no value of it can change.
template <Described Q, Node Expr>
struct Definition
{
    static_assert(detail::RequireDefinitionMeasuresQuantity<Q, Expr>::value);

    /// The quantity this definition calculates.
    using quantity = Q;

    /// The quantities the expression reads, each once, in the order it first
    /// reads them -- a `detail::QuantityList`, which a calculation builds its
    /// dependency graph from. A `when()` contributes its condition and both
    /// branches: what it may read. Listing it refuses a series read, a read
    /// from another record and a node kind the walk cannot see inside.
    using reads = detail::UniqueQuantities<detail::CalculationReadsOf<Expr>>;

    /// Whether the rules checked here hold, asked without firing any: false
    /// for a definition refused here, or holding a node refused already, so
    /// that what is built on it can stay silent instead of adding a second
    /// message. The constant `define` returns in place of a series is valid:
    /// it stands in for the refused definition so that nothing else fires.
    static constexpr bool valid = !detail::refused_already<Expr>() && Expr::dimension == Describe<Q>::dimension
                                  && detail::CalculationReads<std::remove_cv_t<Expr>>::accepted;

    /// The expression that calculates `Q`.
    ///
    /// Deliberately no `{}` default member initialiser: see `Corrections`
    /// (`lookup.hpp`).
    Expr expression;
};

/// Says that `Q` is calculated by @p definingExpression:
/// `define<Total>(var<Subtotal> + var<Vat>)`. Refused where it is written for
/// any rule the file comment lists.
template <Described Q, Node Expr>
[[nodiscard]] constexpr Definition<Q, Expr> define(Expr definingExpression) noexcept
{
    return Definition<Q, Expr> { definingExpression };
}

/// Refused: a calculation holds single values -- see
/// `detail::RequireSingleValueDefinition`. What it returns defines `Q` as a
/// constant of its dimension, so that nothing built on it adds a second
/// message.
template <Described Q, SeriesNode S>
[[nodiscard]] constexpr Definition<Q, ConstantNode<coherent(Describe<Q>::dimension)>> define(S) noexcept
{
    static_assert(detail::RequireSingleValueDefinition<S>::value);
    using Placeholder = ConstantNode<coherent(Describe<Q>::dimension)>;
    return Definition<Q, Placeholder> { Placeholder {} };
}

// ------------------------------------------------------------ calculations

template <typename... Defs>
class Calculation;

namespace detail
{
    /// The most quantities, inputs and definitions together, a calculation
    /// holds: one bit each of a `std::uint64_t`.
    inline constexpr std::size_t calculationCapacity = 64;

    /// Whether @p T is a `Definition`.
    template <typename T>
    inline constexpr bool isDefinition = false;

    template <Described Q, Node Expr>
    inline constexpr bool isDefinition<Definition<Q, Expr>> = true;

    /// Fails to compile when `calculation(...)` is given something other than
    /// a definition.
    template <typename... Ts>
    struct RequireDefinitions
    {
        static_assert((isDefinition<std::remove_cv_t<Ts>> && ...),
                      "formula: calculation(...) takes only definitions made by define<Q>(expression); the arguments "
                      "appear in this diagnostic as the template arguments of RequireDefinitions");

        static constexpr bool value = true;
    };

    /// Fails to compile when `calculation()` is given no definition at all.
    /// Named for the count, which is all there is to print.
    template <std::size_t Count>
    struct RequireSomeDefinition
    {
        static_assert(Count > 0,
                      "formula: a calculation defines at least one quantity; calculation() with no definitions "
                      "calculates nothing");

        static constexpr bool value = true;
    };

    /// Fails to compile when a calculation defines one quantity twice.
    template <typename... Qs>
    struct RequireDistinctDefinitions
    {
        template <typename Q>
        static constexpr std::size_t occurrences = (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Q, Qs> });

        static_assert((... && (occurrences<Qs> == 1)),
                      "formula: this calculation defines the same quantity more than once; first-wins and last-wins "
                      "are equally arbitrary, so neither is guessed -- the quantities defined appear in this "
                      "diagnostic as the template arguments of RequireDistinctDefinitions");

        static constexpr bool value = true;
    };

    /// Fails to compile when a calculation holds more quantities than its
    /// graph has bits for.
    template <std::size_t Count>
    struct RequireCalculationWithinCapacity
    {
        static_assert(Count <= calculationCapacity,
                      "formula: a calculation holds at most 64 quantities, inputs and definitions together, and this "
                      "one holds more; the count appears in this diagnostic as the template argument of "
                      "RequireCalculationWithinCapacity -- split it into two, the second reading the first's results "
                      "as inputs");

        static constexpr bool value = true;
    };

    /// Fails to compile when a definition reads the quantity it defines.
    /// Instantiated only for such a definition.
    template <typename Q>
    struct RequireDefinitionNotSelfReferential
    {
        static_assert(alwaysFalse<Q>,
                      "formula: this definition reads the quantity it defines, so it can never be calculated -- the "
                      "quantity appears in this diagnostic as the template argument Q of "
                      "RequireDefinitionNotSelfReferential");

        static constexpr bool value = true;
    };

    /// Fails to compile when definitions read one another in a cycle; @p Cycle
    /// is the `QuantityList` of every quantity on one, empty when there is
    /// none.
    template <typename Cycle>
    struct RequireAcyclicDefinitions
    {
        static_assert(quantity_count_v<Cycle> == 0,
                      "formula: these definitions read one another in a cycle, so none of them can be calculated "
                      "first -- the quantities on the cycle appear in this diagnostic as the template arguments of "
                      "RequireAcyclicDefinitions");

        static constexpr bool value = true;
    };

    /// The inputs of the definitions @p Defs: the quantities they read that
    /// @p Defined does not hold, each once, in the order first read --
    /// @p Seen, the inputs of the definitions before them, extended one
    /// definition at a time. De-duplicated at every step, so that no list
    /// this walks grows past the inputs found so far and one definition's
    /// reads.
    template <typename Defined, typename Seen, typename... Defs>
    struct CalculationInputs
    {
        /// Every input found.
        using type = Seen;
    };

    template <typename Defined, typename Seen, typename Def, typename... Rest>
    struct CalculationInputs<Defined, Seen, Def, Rest...>:
        CalculationInputs<
            Defined,
            UniqueQuantities<typename JoinQuantities<Seen, QuantitiesWithout<typename Def::reads, Defined>>::type>,
            Rest...>
    {
    };

    /// Whether the definition @p Def reads the quantity it defines.
    template <typename Def>
    inline constexpr bool reads_itself =
        quantity_index_v<typename Def::quantity, typename Def::reads> < quantity_count_v<typename Def::reads>;

    /// One bit for each quantity of a list, at its position among @p Slots.
    /// Asked only of a list every quantity of which @p Slots holds, below the
    /// capacity: every bit is below 64.
    template <typename Slots, typename... Reads>
    [[nodiscard]] consteval std::uint64_t slot_mask(QuantityList<Reads...> const*) noexcept
    {
        return (std::uint64_t { 0 } | ... | (std::uint64_t { 1 } << quantity_index_v<Reads, Slots>));
    }

    /// What each of @p Slots reads directly, one row of bits each: nothing
    /// for an input, and for a definition its `reads`. The definitions are
    /// the last `sizeof...(Defs)` slots, in the order given.
    template <typename Slots, typename... Defs>
    [[nodiscard]] consteval std::array<std::uint64_t, quantity_count_v<Slots>> direct_reads() noexcept
    {
        std::array<std::uint64_t, quantity_count_v<Slots>> readRows {};
        std::size_t filled = quantity_count_v<Slots> - sizeof...(Defs);
        ((readRows[filled++] = slot_mask<Slots>(static_cast<typename Defs::reads const*>(nullptr))), ...);
        return readRows;
    }

    /// @p readRows with rows and columns exchanged: who reads each slot, from
    /// what each slot reads. N^2 bit tests.
    template <std::size_t N>
    [[nodiscard]] consteval std::array<std::uint64_t, N> transposed(std::array<std::uint64_t, N> const& readRows) noexcept
    {
        std::array<std::uint64_t, N> readerRows {};
        for (std::size_t from = 0; from < N; ++from)
            for (std::size_t into = 0; into < N; ++into)
                if (((readRows[from] >> into) & 1u) != 0)
                    readerRows[into] |= std::uint64_t { 1 } << from;
        return readerRows;
    }

    /// Everything each slot reaches through any chain of @p readRows --
    /// Warshall's closure over rows of bits: N^2 word operations.
    template <std::size_t N>
    [[nodiscard]] consteval std::array<std::uint64_t, N> transitive(std::array<std::uint64_t, N> readRows) noexcept
    {
        for (std::size_t through = 0; through < N; ++through)
            for (std::size_t from = 0; from < N; ++from)
                if (((readRows[from] >> through) & 1u) != 0)
                    readRows[from] |= readRows[through];
        return readRows;
    }

    /// The slots in dependency order: each as soon as every slot it reads is
    /// placed, the lowest-numbered first among those that could come next --
    /// so the inputs first, in order, then the definitions in the order given
    /// wherever that order allows. N^2 bit tests. On a cycle the slots on it
    /// are never placed; such a calculation is refused and this order never
    /// read.
    template <std::size_t N>
    [[nodiscard]] consteval std::array<std::size_t, N> dependency_order(
        std::array<std::uint64_t, N> const& readRows) noexcept
    {
        std::array<std::size_t, N> ordered {};
        std::uint64_t placed = 0;
        for (std::size_t placedCount = 0; placedCount < N; ++placedCount)
        {
            std::size_t chosen = N;
            for (std::size_t candidate = 0; candidate < N && chosen == N; ++candidate)
                if (((placed >> candidate) & 1u) == 0 && (readRows[candidate] & ~placed) == 0)
                    chosen = candidate;
            if (chosen == N)
                return ordered;
            ordered[placedCount] = chosen;
            placed |= std::uint64_t { 1 } << chosen;
        }
        return ordered;
    }

    /// The slots that reach themselves, read off the diagonal of @p reach,
    /// what each slot reaches through any chain of reads. Once no definition
    /// reads itself directly, those are exactly the quantities on a cycle.
    template <std::size_t N>
    [[nodiscard]] consteval std::uint64_t reaching_themselves(std::array<std::uint64_t, N> const& reach) noexcept
    {
        std::uint64_t onCycle = 0;
        for (std::size_t slot = 0; slot < N; ++slot)
            if (((reach[slot] >> slot) & 1u) != 0)
                onCycle |= std::uint64_t { 1 } << slot;
        return onCycle;
    }

    /// The @p bitCount lowest bits set: every bit for the capacity. A
    /// function rather than a conditional expression at the call, where cl
    /// warns of the shift by 64 in the branch not taken.
    [[nodiscard]] consteval std::uint64_t lowest_bits(std::size_t bitCount) noexcept
    {
        return bitCount >= calculationCapacity ? ~std::uint64_t { 0 } : (std::uint64_t { 1 } << bitCount) - 1;
    }

    /// The positions of the bits set in @p Mask, lowest first.
    template <std::uint64_t Mask>
    [[nodiscard]] consteval std::array<std::size_t, static_cast<std::size_t>(std::popcount(Mask))> set_bits() noexcept
    {
        std::array<std::size_t, static_cast<std::size_t>(std::popcount(Mask))> positions {};
        std::size_t filled = 0;
        for (std::size_t bit = 0; bit < calculationCapacity; ++bit)
            if (((Mask >> bit) & 1u) != 0)
                positions[filled++] = bit;
        return positions;
    }

    template <typename Slots, auto Positions, typename Sequence>
    struct QuantitiesAtPositions;

    template <typename Slots, auto Positions, std::size_t... Is>
    struct QuantitiesAtPositions<Slots, Positions, std::index_sequence<Is...>>
    {
        /// The quantities at @p Positions, each below the length of @p Slots.
        using type = QuantityList<QuantityAt<Positions[Is], Slots>...>;
    };

    /// The quantities of @p Slots whose bits @p Mask sets, in slot order.
    template <typename Slots, std::uint64_t Mask>
    using QuantitiesInMask = typename QuantitiesAtPositions<
        Slots,
        set_bits<Mask>(),
        std::make_index_sequence<static_cast<std::size_t>(std::popcount(Mask))>>::type;

    /// The graph's rows, for a calculation whose quantities fit the capacity:
    /// each a namespace-scope function's result, since a static member's
    /// initialiser cannot call the class's own member functions.
    template <typename Slots, typename... Defs>
    struct CalculationRows
    {
        /// How many quantities.
        static constexpr std::size_t slotCount = quantity_count_v<Slots>;
        /// What each quantity reads directly.
        static constexpr std::array<std::uint64_t, slotCount> reads = direct_reads<Slots, Defs...>();
        /// What reads each quantity directly.
        static constexpr std::array<std::uint64_t, slotCount> readers = transposed(reads);
        /// What each quantity depends on through any chain of reads.
        static constexpr std::array<std::uint64_t, slotCount> upstream = transitive(reads);
        /// What depends on each quantity through any chain of reads.
        static constexpr std::array<std::uint64_t, slotCount> downstream = transposed(upstream);
        /// The quantities in dependency order.
        static constexpr std::array<std::size_t, slotCount> order = dependency_order(reads);
        /// The quantities that reach themselves: those on a cycle, and any
        /// definition that reads itself.
        static constexpr std::uint64_t reachingThemselves = reaching_themselves(upstream);
        /// How many inputs: the slots before the definitions. At most 63,
        /// since a calculation defines at least one quantity.
        static constexpr std::size_t inputCount = slotCount - sizeof...(Defs);
        /// The inputs' bits.
        static constexpr std::uint64_t inputMask = lowest_bits(inputCount);
        /// The defined quantities' bits.
        static constexpr std::uint64_t definedMask = lowest_bits(slotCount) & ~inputMask;
    };

    /// The rows of a calculation whose graph is not built -- refused before
    /// it could be: all empty, and never read.
    template <std::size_t SlotCount>
    struct UnconnectedRows
    {
        /// Nothing.
        static constexpr std::array<std::uint64_t, SlotCount> reads {};
        /// Nothing.
        static constexpr std::array<std::uint64_t, SlotCount> readers {};
        /// Nothing.
        static constexpr std::array<std::uint64_t, SlotCount> upstream {};
        /// Nothing.
        static constexpr std::array<std::uint64_t, SlotCount> downstream {};
        /// Nothing.
        static constexpr std::array<std::size_t, SlotCount> order {};
        /// Nothing, so that no cycle is reported.
        static constexpr std::uint64_t reachingThemselves = 0;
        /// Nothing.
        static constexpr std::uint64_t inputMask = 0;
        /// Nothing.
        static constexpr std::uint64_t definedMask = 0;
    };

    /// The dependency graph of the definitions @p Defs, every one of which is
    /// a `Definition`: its quantities (`slots`, the inputs first), what each
    /// reads and what reads it, directly and through any chain, and the
    /// dependency order. Each rule `Calculation` refuses is a flag here, asked
    /// without firing; `valid` is all of them.
    template <typename... Defs>
    struct DefinedCalculationGraph
    {
        /// The quantities defined, in the order given.
        using defined = QuantityList<typename Defs::quantity...>;
        /// The quantities read and not defined, in the order first read.
        using inputs = typename CalculationInputs<defined, QuantityList<>, Defs...>::type;
        /// Every quantity: the inputs, then the defined quantities.
        using slots = typename JoinQuantities<inputs, defined>::type;

        /// How many inputs.
        static constexpr std::size_t inputCount = quantity_count_v<inputs>;
        /// How many quantities.
        static constexpr std::size_t slotCount = quantity_count_v<slots>;

        /// Whether every definition is one no refusal of its own met.
        static constexpr bool definitionsValid = (Defs::valid && ...);
        /// Whether no quantity is defined twice.
        static constexpr bool distinct = quantity_count_v<UniqueQuantities<defined>> == sizeof...(Defs);
        /// Whether the quantities fit the graph's bits.
        static constexpr bool withinCapacity = slotCount <= calculationCapacity;
        /// Whether no definition reads the quantity it defines.
        static constexpr bool selfReferenceFree = (!reads_itself<Defs> && ...);
        /// Whether the rows can be built: each check before it holds.
        static constexpr bool connectable = definitionsValid && distinct && withinCapacity;

      private:
        using Rows = std::conditional_t<connectable, CalculationRows<slots, Defs...>, UnconnectedRows<slotCount>>;

      public:
        /// What each quantity reads directly.
        static constexpr std::array<std::uint64_t, slotCount> reads = Rows::reads;
        /// What reads each quantity directly.
        static constexpr std::array<std::uint64_t, slotCount> readers = Rows::readers;
        /// What each quantity depends on through any chain of reads.
        static constexpr std::array<std::uint64_t, slotCount> upstream = Rows::upstream;
        /// What depends on each quantity through any chain of reads.
        static constexpr std::array<std::uint64_t, slotCount> downstream = Rows::downstream;
        /// The quantities in dependency order.
        static constexpr std::array<std::size_t, slotCount> order = Rows::order;
        /// The inputs' bits.
        static constexpr std::uint64_t inputMask = Rows::inputMask;
        /// The defined quantities' bits.
        static constexpr std::uint64_t definedMask = Rows::definedMask;
        /// The bits of the quantities on a cycle -- judged only once the rows
        /// are built and no definition reads itself, and nothing before.
        static constexpr std::uint64_t cycleMask =
            connectable && selfReferenceFree ? Rows::reachingThemselves : std::uint64_t { 0 };
        /// Every quantity on a cycle, in slot order: empty when there is none,
        /// and when the cycles are not judged.
        using cycle = QuantitiesInMask<slots, cycleMask>;

        /// Whether every rule holds: the graph is a valid one.
        static constexpr bool valid = connectable && selfReferenceFree && cycleMask == 0;

        /// The position of @p Q among the slots, or `slotCount` when the
        /// calculation neither defines nor reads it.
        template <typename Q>
        static constexpr std::size_t slot_of = quantity_index_v<Q, slots>;
        /// Whether the calculation defines or reads @p Q.
        template <typename Q>
        static constexpr bool holds = slot_of<Q> < slotCount;
        /// Whether the calculation defines @p Q.
        template <typename Q>
        static constexpr bool defines = holds<Q> && slot_of<Q> >= inputCount;
        /// Whether @p Q depends on @p P through any chain of reads; false
        /// when the calculation does not hold both.
        template <typename Q, typename P>
        static constexpr bool depends_on =
            holds<Q> && holds<P> && ((upstream[slot_of<Q>] >> slot_of<P>) & 1u) != 0;
    };

    /// What stands for the graph of a calculation refused for its arguments
    /// -- none, or one that is not a definition: nothing can be built, and
    /// nothing is held.
    struct UnbuiltCalculationGraph
    {
        /// Never.
        static constexpr bool valid = false;
        /// Nothing.
        template <typename Q>
        static constexpr bool holds = false;
        /// Nothing.
        template <typename Q, typename P>
        static constexpr bool depends_on = false;
    };

    /// Whether @p Defs are arguments a calculation can be built from: at
    /// least one, and every one a definition.
    template <typename... Defs>
    inline constexpr bool calculationArgumentsAccepted =
        sizeof...(Defs) > 0 && (isDefinition<std::remove_cv_t<Defs>> && ...);

    /// The graph of `Calculation<Defs...>`, whatever its arguments.
    template <typename... Defs>
    struct CalculationGraph:
        std::conditional_t<calculationArgumentsAccepted<Defs...>, DefinedCalculationGraph<Defs...>, UnbuiltCalculationGraph>
    {
    };

    /// Every check of a calculation after its arguments', in order, each
    /// asked only once every check before it holds, so that one mistake
    /// draws one message: its definitions' own, then one quantity defined
    /// twice, then the capacity, then a definition reading itself, then a
    /// cycle. Two definitions that each read themselves are two mistakes, and
    /// draw a message each. Instantiated only for accepted arguments.
    template <typename... Defs>
    struct CalculationGraphChecks
    {
        using Graph = DefinedCalculationGraph<Defs...>;

        static_assert(std::conditional_t<Graph::definitionsValid,
                                         RequireDistinctDefinitions<typename Defs::quantity...>,
                                         std::true_type>::value);
        static_assert(std::conditional_t<Graph::definitionsValid && Graph::distinct,
                                         RequireCalculationWithinCapacity<Graph::slotCount>,
                                         std::true_type>::value);
        static_assert((std::conditional_t<Graph::connectable && reads_itself<Defs>,
                                          RequireDefinitionNotSelfReferential<typename Defs::quantity>,
                                          std::true_type>::value
                       && ...));
        static_assert(std::conditional_t<Graph::connectable && Graph::selfReferenceFree,
                                         RequireAcyclicDefinitions<typename Graph::cycle>,
                                         std::true_type>::value);

        static constexpr bool value = true;
    };

    template <typename Calc>
    struct CalculationGraphOf;

    template <typename... Defs>
    struct CalculationGraphOf<Calculation<Defs...>>
    {
        /// The calculation's graph.
        using type = CalculationGraph<Defs...>;
    };

    /// Fails to compile when a query names a quantity the calculation neither
    /// defines nor reads. Asked only of a valid calculation.
    template <typename Q, typename Calc>
    struct RequireCalculationQuantity
    {
        static_assert(CalculationGraphOf<Calc>::type::template holds<Q>,
                      "formula: this calculation neither defines nor reads this quantity; the quantity and the "
                      "calculation appear in this diagnostic as the template arguments of RequireCalculationQuantity");

        static constexpr bool value = true;
    };

    /// Every slot's symbol under @p vocabulary, in slot order.
    template <Vocabulary V, typename... Qs>
    [[nodiscard]] constexpr std::array<std::string_view, sizeof...(Qs)> slot_symbols(QuantityList<Qs...> const*,
                                                                                    V const& vocabulary) noexcept
    {
        return { symbol_of<Qs>(vocabulary)... };
    }

    /// What a query answers when there is nothing to name.
    ///
    /// A constant at namespace scope, copied, rather than `{}` written inside
    /// a function: every array of `std::string_view` a function template here
    /// makes is built from its elements, never value-initialised. cl
    /// value-initialises such an array through a helper of its own that
    /// declares a local named `i`, so a consumer's global of that name drew
    /// warning C4459 through this header (measured on cl 19.51, for an array
    /// of length 0 as for one of length 2; a constant like this one does not).
    inline constexpr std::array<std::string_view, 0> noSymbols {};

    /// The slots whose bits @p Mask sets, in dependency order.
    template <typename Graph, std::uint64_t Mask>
    [[nodiscard]] consteval std::array<std::size_t, static_cast<std::size_t>(std::popcount(Mask))> slots_in_order() noexcept
    {
        std::array<std::size_t, static_cast<std::size_t>(std::popcount(Mask))> inOrder {};
        std::size_t filled = 0;
        for (std::size_t const slot: Graph::order)
            if (((Mask >> slot) & 1u) != 0)
                inOrder[filled++] = slot;
        return inOrder;
    }

    /// The symbols of the slots whose bits @p Mask sets, in dependency order;
    /// @p Is counts them.
    template <typename Graph, std::uint64_t Mask, Vocabulary V, std::size_t... Is>
    [[nodiscard]] constexpr std::array<std::string_view, sizeof...(Is)> symbols_at(V const& vocabulary,
                                                                                  std::index_sequence<Is...>) noexcept
    {
        constexpr std::array<std::size_t, sizeof...(Is)> inOrder = slots_in_order<Graph, Mask>();
        std::array<std::string_view, Graph::slotCount> const everySymbol =
            slot_symbols(static_cast<typename Graph::slots const*>(nullptr), vocabulary);
        return { everySymbol[inOrder[Is]]... };
    }

    /// The symbols of the slots whose bits @p Mask sets, in dependency order.
    template <typename Graph, std::uint64_t Mask, Vocabulary V>
    [[nodiscard]] constexpr std::array<std::string_view, static_cast<std::size_t>(std::popcount(Mask))> symbols_in_order(
        V const& vocabulary) noexcept
    {
        constexpr std::size_t named = static_cast<std::size_t>(std::popcount(Mask));
        if constexpr (named == 0)
            return noSymbols;
        else
            return symbols_at<Graph, Mask>(vocabulary, std::make_index_sequence<named> {});
    }

    /// Which of a quantity's rows a query reads.
    enum class GraphRelation : std::uint8_t
    {
        Reads,
        Readers,
        Upstream,
        Downstream,
    };

    /// The row @p Relation of the graph @p Graph, for the slot @p Slot.
    template <GraphRelation Relation, typename Graph, std::size_t Slot>
    [[nodiscard]] consteval std::uint64_t related_slots() noexcept
    {
        if constexpr (Relation == GraphRelation::Reads)
            return Graph::reads[Slot];
        else if constexpr (Relation == GraphRelation::Readers)
            return Graph::readers[Slot];
        else if constexpr (Relation == GraphRelation::Upstream)
            return Graph::upstream[Slot];
        else
            return Graph::downstream[Slot];
    }

    /// The symbols of what stands in @p Relation to @p Q in the calculation
    /// of @p Defs, in dependency order: refused for a quantity it does not
    /// hold, and nothing, silently, for a calculation refused already.
    template <GraphRelation Relation, Described Q, typename... Defs, Vocabulary V>
    [[nodiscard]] constexpr auto related_symbols(V const& vocabulary) noexcept
    {
        using Graph = CalculationGraph<Defs...>;
        static_assert(
            std::conditional_t<Graph::valid, RequireCalculationQuantity<Q, Calculation<Defs...>>, std::true_type>::value);
        if constexpr (Graph::valid && Graph::template holds<Q>)
            return symbols_in_order<Graph, related_slots<Relation, Graph, Graph::template slot_of<Q>>()>(vocabulary);
        else
            return detail::noSymbols;
    }
} // namespace detail

/// A set of definitions whose dependency graph is known, and checked, at
/// compile time: built by `calculation(define<A>(...), ...)`. See the file
/// comment for what the graph is, and what is refused.
///
/// A public aggregate: its checks sit in the class body, so one spelled
/// without `calculation` is refused as well. Its graph is a property of its
/// type, which no value of it can change.
template <typename... Defs>
class Calculation
{
    static_assert(detail::RequireSomeDefinition<sizeof...(Defs)>::value);
    static_assert(detail::RequireDefinitions<Defs...>::value);
    static_assert(std::conditional_t<detail::calculationArgumentsAccepted<Defs...>,
                                     detail::CalculationGraphChecks<Defs...>,
                                     std::true_type>::value);

  public:
    /// The definitions, in the order given.
    ///
    /// Deliberately no `{}` default member initialiser: they hold
    /// expressions -- see `Corrections` (`lookup.hpp`).
    std::tuple<Defs...> definitions;
};

/// Builds a calculation from @p definitions, in any order:
/// `calculation(define<Total>(var<Subtotal> + var<Vat>), define<Vat>(var<Subtotal> * Rational { 19, 100 }))`.
/// Refused where it is written for any rule the file comment lists.
template <typename... Defs>
[[nodiscard]] constexpr Calculation<Defs...> calculation(Defs... definitions) noexcept
{
    return Calculation<Defs...> { std::tuple<Defs...> { definitions... } };
}

/// What @p Q reads directly, in dependency order: its definition's reads,
/// and nothing for an input. Each symbol is written as @p vocabulary says.
template <Described Q, typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto dependencies_of(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    return detail::related_symbols<detail::GraphRelation::Reads, Q, Ds...>(vocabulary);
}

/// What reads @p Q directly, in dependency order.
template <Described Q, typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto dependents_of(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    return detail::related_symbols<detail::GraphRelation::Readers, Q, Ds...>(vocabulary);
}

/// What @p Q depends on through any chain of reads, in dependency order:
/// every quantity a change to which can change it.
template <Described Q, typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto upstream_of(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    return detail::related_symbols<detail::GraphRelation::Upstream, Q, Ds...>(vocabulary);
}

/// What depends on @p Q through any chain of reads, in dependency order:
/// every quantity a change to it can change.
template <Described Q, typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto affected_by(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    return detail::related_symbols<detail::GraphRelation::Downstream, Q, Ds...>(vocabulary);
}

/// The calculation's inputs -- what its definitions read and none of them
/// defines -- in the order they are first read.
template <typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto inputs_of(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    using Graph = detail::CalculationGraph<Ds...>;
    if constexpr (Graph::valid)
        return detail::symbols_in_order<Graph, Graph::inputMask>(vocabulary);
    else
        return detail::noSymbols;
}

/// The quantities the calculation defines, in dependency order: the order it
/// calculates them in.
template <typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] constexpr auto calculation_order(Calculation<Ds...> const&, V const& vocabulary = V {}) noexcept
{
    using Graph = detail::CalculationGraph<Ds...>;
    if constexpr (Graph::valid)
        return detail::symbols_in_order<Graph, Graph::definedMask>(vocabulary);
    else
        return detail::noSymbols;
}

/// Whether @p Q depends on @p P through any chain of reads: whether a
/// change to @p P can change @p Q. False for a quantity and itself.
template <Described Q, Described P, typename... Ds>
[[nodiscard]] constexpr bool depends_on(Calculation<Ds...> const&) noexcept
{
    using Graph = detail::CalculationGraph<Ds...>;
    static_assert(
        std::conditional_t<Graph::valid, detail::RequireCalculationQuantity<Q, Calculation<Ds...>>, std::true_type>::value);
    static_assert(
        std::conditional_t<Graph::valid, detail::RequireCalculationQuantity<P, Calculation<Ds...>>, std::true_type>::value);
    if constexpr (Graph::valid)
        return Graph::template depends_on<Q, P>;
    else
        return false;
}

} // namespace formula
