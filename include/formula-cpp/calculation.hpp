// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Definitions: each quantity bound to the expression that calculates it.
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

#include <cstddef>
#include <tuple>
#include <type_traits>

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

} // namespace formula
