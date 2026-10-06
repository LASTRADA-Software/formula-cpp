// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A jurisdiction's changes to a method: `apply(overlay(...), method)`.
///
/// Spec section 16.7 found, following one measurement through five
/// jurisdictions, that the core algebra is stable everywhere and that around
/// it each jurisdiction varies independently -- which constants apply, which
/// variants exist at all, which one is mandatory. A `jurisdiction` enum
/// threaded into one function does not survive that, so a jurisdiction is an
/// **overlay over a method**: a declared list of changes, applied to a method,
/// which yields a method.
///
/// An overlay is applied at **compile time** and the method it yields is a new
/// type. Both shapes were built and compared: the compile-time overlay met every one of section
/// 16.7's demands and cost nothing at the call site, where a runtime overlay
/// could not replace a formula without type erasure. The set of jurisdictions
/// is closed and lives in the type; which one applies is a runtime choice made
/// among methods that already exist.
///
/// This header provides seven operations:
///
///  - `with_constant<Q>(value, source)` fixes the quantity `Q` to `value` wherever the
///    method uses it -- the national body fixing a constant the base standard
///    left open;
///  - `add_derived<Q>(expression, source)` defines `Q` by an expression over other
///    inputs wherever the method uses it -- the jurisdiction computing what
///    the base standard left to the specimen;
///  - `pin_variant<Tag>(source)` keeps only the variant tagged `Tag`, making it
///    mandatory;
///  - `prune_variant<Tag>(source)` deletes the variant tagged `Tag` outright;
///  - `replace_variant<Tag>(expression, source)` replaces the formula of the variant
///    tagged `Tag` wholesale;
///  - `with_rounding<U, Places, Mode>(source)` replaces the method's rounding rule;
///  - `with_constraints(constraints(...), source)` replaces the method's constraints
///    wholesale, with as many as the jurisdiction states -- more, fewer or
///    none.
///
/// Every one of them is **said** in the trace, not merely done: a fixed
/// constant, a derived quantity, a replaced formula and an overlay's rounding
/// rule each record a step naming the jurisdiction's overlay and what it
/// cited, each verdict of an overlay's constraints is marked as the
/// jurisdiction's, and the step recording which variant was selected says
/// when an overlay pinned the method to it or pruned others from it. The nodes, rules and origins those steps are read from
/// can be built only by the library -- see `detail::OverlayNodeAccess`, `RoundingRule` and `OverlaidConstraints`.
///
/// **A declared unit is not an overlay's to change, and none of these
/// operations touches one.** Spec section 16.7 asks that an overlay can
/// "change the declared unit"; `measured.hpp` makes a `Measured<Q>` a value in
/// `Q`'s declared unit that carries no unit of its own, so a number can never
/// disagree with its label, and that invariant stands. A method has no typed
/// result to relabel in any case: `evaluate_method` answers in the coherent
/// unit of the variants' dimension, and the unit a jurisdiction *reports* in
/// is its rounding unit, which `with_rounding` changes. What a jurisdiction
/// cannot change is the dimension a method reports -- `replace_variant`
/// refuses a formula of another dimension, and `add_derived` a definition of
/// another dimension from its quantity's. A method whose result is a typed
/// `Measured<Q>` is outside this header, and recorded as a follow-up.
///
/// **An override that would silently do nothing is refused.** An overlay is
/// written once per jurisdiction and read by nobody until an inspector asks why
/// a number came out as it did; an operation that names the wrong quantity or
/// the wrong variant changes nothing, produces no error, and yields a method
/// that is the base method under a jurisdiction's name. So each of these is a
/// build error, in words of this library's own:
///
///  - `with_constant<Q>` or `add_derived<Q>` for a `Q` no variant or
///    constraint of the method the overlay **produces** uses -- judged of the
///    result, so that the answer never depends on the order the operations
///    are listed in;
///  - a produced method that reads `Q` both where an overlay fixed or
///    derived it and, elsewhere, as a plain `var<Q>` -- put back by an
///    operation listed after the substitution, by a later overlay, or by
///    definitions that read each other -- which would read the environment
///    while every other use reads the overlay. Judged against the whole
///    produced method, whichever overlay left each substitution;
///  - a `with_constant<Q>` or `add_derived<Q>` whose every use an operation
///    listed after it in the same overlay removed, putting back a plain
///    `var<Q>` -- the substitution then does nothing at all;
///  - a `with_constant<Q>` or `add_derived<Q>` that met no use of `Q` where it
///    is listed, while an operation listed after it in the same overlay puts
///    in a plain `var<Q>` -- the substitution does nothing either, where
///    listed after that operation it would have applied;
///  - `pin_variant`, `prune_variant` or `replace_variant` of a tag no variant
///    declares, and a `replace_variant` whose variant the produced method no
///    longer holds, in either order;
///  - `replace_variant` with a formula of a different dimension from the
///    method's, and `add_derived<Q>` with an expression of a different
///    dimension from `Q`'s or one that reads `Q` itself;
///  - pruning every variant -- refused here, before the empty pack would be,
///    because the empty pack's own message says the author declared no
///    variants, which is false of the author's method;
///  - one overlay listing the same operation twice, which leaves the first
///    silently overridden by the second -- and two `with_rounding` of any
///    granularities are the same operation, since a method has one rule, as
///    are a `with_constant<Q>` and an `add_derived<Q>`, two replacements
///    of one variant, and two `with_constraints`;
///  - one overlay that both pins and prunes: a pin already states the whole
///    selection, so a prune beside it either does nothing or contradicts it;
///  - `with_constant` or `add_derived` over an expression holding a node kind
///    this header cannot see inside, where a use of `Q` would silently keep
///    reading the environment.
///
/// Pinning a method's only variant is deliberately **not** refused, although
/// it changes nothing. The refusals above exist to catch a *mistaken name*,
/// and a pin that names the one variant there is names it correctly: the
/// method it yields is exactly what the overlay declares, a method whose only
/// variant is that one. A `with_rounding` of the granularity the method
/// already has is accepted for the same reason, and does change something:
/// the rule is then the jurisdiction's, and the trace says so.
///
/// A `with_constraints` is accepted whatever it holds, for the same reason:
/// the base method's own constraints restated make them the jurisdiction's,
/// and `with_constraints(constraints(), source)` -- no constraints at all -- is a
/// jurisdiction that checks nothing the base standard checks. A method
/// declared with `constraints()` is already well formed, and the removal is
/// not silent: `check_method` records a step for a method's constraints even
/// when there are none, and that step names the overlay.
///
/// A `with_rounding` whose unit does not measure what the method reports is
/// refused where the overlay applies it, in `Method`'s own words: the rule is
/// held to the same dimension check the method's own rule was held to. The
/// operations after it are applied to the method as it stood, so the refusal
/// is raised once, not again by each method they build.
///
/// Operations apply **in the order the overlay lists them**, each to the method
/// the previous one produced. "Nothing" in the rule above is nothing in the
/// method the overlay produces, not in the method as it stood when one
/// operation was applied. So `overlay(pin_variant<Cube>(source), with_constant<Q>(v, source))`
/// and `overlay(with_constant<Q>(v, source), prune_variant<Cylinder>(source))` are both
/// refused when only the variant that goes away reads `Q`: in either order,
/// the method produced never reads it.

#include <formula-cpp/binning.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounded_transcendental.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/statistics.hpp>

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

namespace detail
{
    struct OverlayNodeAccess;

    /// Marks the one constructor of each overlay node that builds it -- the
    /// private one `OverlayNodeAccess` calls.
    struct OverlayMade
    {
    };

    /// A dependent `false`, so that a refusal inside a template fires only
    /// when that template is instantiated.
    template <typename>
    inline constexpr bool alwaysFalse = false;

    /// Fails to compile when an author builds a node only an overlay may
    /// build. Each such node makes a trace or `document()` say that a
    /// jurisdiction fixed a value, defined a quantity or replaced a formula;
    /// one built by hand would say so of something no overlay did.
    template <typename OverlayNode>
    struct RequireOverlayMadeNode
    {
        static_assert(alwaysFalse<OverlayNode>,
                      "formula: only an overlay builds this node; it makes a trace say a jurisdiction fixed a "
                      "value, defined a quantity or replaced a formula, so one built by hand would say so of "
                      "something no overlay did -- use with_constant, add_derived or replace_variant in an "
                      "overlay(...) given to apply; the node appears in this diagnostic as the template argument "
                      "OverlayNode of RequireOverlayMadeNode");

        static constexpr bool value = true;
    };
} // namespace detail

/// A quantity whose value an overlay has fixed: what `with_constant<Q>(value, source)`
/// leaves where the method had `var<Q>`.
///
/// **It keeps `Q`'s identity**, and that is the reason it exists rather than
/// an anonymous `ConstantNode` holding the same number. The question an
/// inspector asks of a jurisdiction's constant is *"why 0.97?"*, and a bare
/// `0.97` in the formula has lost the one fact that answers it -- that this
/// is the shape factor, fixed by that jurisdiction. So it derives from
/// `VarNode<Q>` and reads as `Q` everywhere a variable is read:
/// `render()` spells it as `Q`'s symbol through its `VarNode` overload, which
/// accepts this node as the base it is, and `document()` lists `Q` in its
/// symbol table -- through an overload of its own, which marks the row fixed.
///
/// Where it differs is evaluation: it never asks the environment. It evaluates
/// to `value`, taken in `Q`'s declared unit exactly as a `Measured<Q>` is --
/// so the overlay's value **wins over** any `Q` the environment supplies, and
/// an environment need not supply `Q` at all.
///
/// **Its trace step says the value was fixed.** The evaluator reports this
/// node to a sink as its own type, and a `RecordingSink` records a
/// `StepKind::OverriddenConstant` step (`trace.hpp`): `Q`'s symbol, in `Q`'s
/// unit, holding the overlay's value and citing `source` -- rendered as
/// `k_s = 863/1000 [fixed by jurisdiction overlay: ...]`. An ordinary variable
/// step would be true of the value and false of where it came from: it reads
/// as a number the specimen supplied. `document()` marks it the same way, as
/// a symbol row fixed at `value` (`SymbolEntry::fixedValue`).
///
/// **Only an overlay makes one.** Its value and citation are private, and the
/// constructor that builds it is reachable only through
/// `detail::OverlayNodeAccess`, which `with_constant` applied by `apply` is
/// the caller of; the public one refuses, in this library's words. A node an author
/// could build would make a trace say "fixed by jurisdiction overlay" of a
/// value no overlay fixed. A copy of a node an overlay made keeps saying so
/// wherever it is put, which is true of it -- the same line `RoundingRule`
/// draws between creating a claim and carrying one.
template <Described Q>
class OverriddenConstantNode: public VarNode<Q>
{
  public:
    /// The value the overlay fixed, in `Q`'s declared unit.
    [[nodiscard]] constexpr Rational value() const noexcept
    {
        return _value;
    }

    /// Where the overlay's value comes from, as the overlay's author cited it;
    /// empty when they cited nothing.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

    /// Refused: see `detail::RequireOverlayMadeNode`. Declared only so that
    /// building one by hand is refused in this library's words.
    explicit constexpr OverriddenConstantNode(Rational fixed,
                                              Citation cited = {},
                                              detail::ProvenanceStatedByAuthor = {}) noexcept:
        VarNode<Q> {},
        _value { fixed },
        _source { cited }
    {
        static_assert(detail::RequireOverlayMadeNode<OverriddenConstantNode>::value);
    }

  private:
    friend struct detail::OverlayNodeAccess;

    constexpr OverriddenConstantNode(detail::OverlayMade, Rational fixed, Citation cited) noexcept:
        VarNode<Q> {},
        _value { fixed },
        _source { cited }
    {
    }

    Rational _value;
    Citation _source;
};

/// A quantity a jurisdiction defines by an expression over other inputs: what
/// `add_derived<Q>(expression, source)` leaves where the method had `var<Q>`.
///
/// It keeps `Q`'s identity for the reason `OverriddenConstantNode` does, and
/// reads as `Q` in `render()` through the same `VarNode` overload. Where it
/// differs is what it evaluates to: not a fixed number but `expression`,
/// evaluated against the same environment -- so the inputs `expression`
/// reads are the specimen's, and `Q` itself is never asked for. A trace
/// records a `StepKind::DerivedQuantity` step whose operand is the
/// expression's own derivation, rendered as `k_s = #3 = ... [derived by
/// jurisdiction overlay: ...]`, and `document()` marks `Q`'s row as derived,
/// with the expression and the citation (`SymbolEntry::derivedAs`).
///
/// Its expression's dimension is `Q`'s, and it never reads `Q` itself: both
/// are refused where the overlay operation is declared -- see
/// `QuantityDerivation`.
///
/// **Only an overlay makes one**, for the reason `OverriddenConstantNode`
/// gives. No `{}` default member initialiser on the expression: see
/// `Corrections` (`lookup.hpp`).
template <Described Q, Node Expr>
class DerivedQuantityNode: public VarNode<Q>
{
  public:
    /// The expression the jurisdiction defines `Q` by.
    [[nodiscard]] constexpr Expr const& expression() const noexcept
    {
        return _expression;
    }

    /// Where the definition comes from, as the overlay's author cited it;
    /// empty when they cited nothing.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

    /// Refused: see `detail::RequireOverlayMadeNode`.
    explicit constexpr DerivedQuantityNode(Expr definition,
                                           Citation cited = {},
                                           detail::ProvenanceStatedByAuthor = {}) noexcept:
        VarNode<Q> {},
        _expression { definition },
        _source { cited }
    {
        static_assert(detail::RequireOverlayMadeNode<DerivedQuantityNode>::value);
    }

  private:
    friend struct detail::OverlayNodeAccess;

    constexpr DerivedQuantityNode(detail::OverlayMade, Expr definition, Citation cited) noexcept:
        VarNode<Q> {},
        _expression { definition },
        _source { cited }
    {
    }

    Expr _expression;
    Citation _source;
};

/// A variant's formula as a jurisdiction replaced it wholesale: what
/// `replace_variant<Tag>(expression, source)` leaves as the variant's expression.
///
/// It evaluates and renders as the replacement, and adds what the replacement
/// alone cannot say: that the formula is not the method's but a
/// jurisdiction's. A trace records a `StepKind::ReplacedVariant` step over the
/// replacement's derivation, rendered as `#5 = ... [replaced by jurisdiction
/// overlay: ...]` -- a step of its own, under the variant selection, because
/// what it marks is the formula that ran rather than the choice of which
/// variant ran: the selection step is the same whether or not the formula was
/// replaced, and a selection step that also carried a replacement would need
/// a second job the variant's own position does not need.
///
/// **Only an overlay makes one**, for the reason `OverriddenConstantNode`
/// gives. No `{}` default member initialiser on the replacement: see
/// `Corrections` (`lookup.hpp`).
template <Node Expr>
class ReplacedVariantNode: public NodeBase
{
  public:
    /// What the variant reports, which is the replacement's -- and, as the
    /// replacement is refused otherwise, the method's.
    static constexpr Dimension dimension = Expr::dimension;

    /// The formula that replaced the variant's.
    [[nodiscard]] constexpr Expr const& replacement() const noexcept
    {
        return _replacement;
    }

    /// Where the replacement comes from, as the overlay's author cited it;
    /// empty when they cited nothing.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

    /// Refused: see `detail::RequireOverlayMadeNode`.
    explicit constexpr ReplacedVariantNode(Expr formula, Citation cited = {}, detail::ProvenanceStatedByAuthor = {}) noexcept
        :
        NodeBase {},
        _replacement { formula },
        _source { cited }
    {
        static_assert(detail::RequireOverlayMadeNode<ReplacedVariantNode>::value);
    }

  private:
    friend struct detail::OverlayNodeAccess;

    constexpr ReplacedVariantNode(detail::OverlayMade, Expr formula, Citation cited) noexcept:
        NodeBase {},
        _replacement { formula },
        _source { cited }
    {
    }

    Expr _replacement;
    Citation _source;
};

namespace detail
{
    /// The one way to build the three nodes an overlay leaves behind -- the
    /// only friend of each. Called from this header's own rewrite and
    /// replacement machinery and nowhere else; an author reaching it has
    /// written `detail::`, which is outside this library's interface.
    struct OverlayNodeAccess
    {
        /// `Q` fixed at @p value, citing @p source.
        template <Described Q>
        [[nodiscard]] static constexpr OverriddenConstantNode<Q> fixed(Rational value, Citation source) noexcept
        {
            return OverriddenConstantNode<Q> { OverlayMade {}, value, source };
        }

        /// `Q` defined by @p definition, citing @p source.
        template <Described Q, Node Expr>
        [[nodiscard]] static constexpr DerivedQuantityNode<Q, Expr> derived(Expr definition, Citation source) noexcept
        {
            return DerivedQuantityNode<Q, Expr> { OverlayMade {}, definition, source };
        }

        /// A variant's formula replaced by @p replacement, citing @p source.
        template <Node Expr>
        [[nodiscard]] static constexpr ReplacedVariantNode<Expr> replaced(Expr replacement, Citation source) noexcept
        {
            return ReplacedVariantNode<Expr> { OverlayMade {}, replacement, source };
        }
    };

    /// The overlay's node kinds, seen by a precision limit's checks
    /// (`precision.hpp`): a derived quantity is its definition, so an overlay
    /// that defines a quantity read in a level by `precision_level` makes the
    /// rewritten limit refuse, where `apply` builds it. Each is required, not
    /// a refinement: the two quantity nodes derive from `VarNode`, whose entry
    /// does not reach them.
    template <Described Q>
    struct LevelChildren<OverriddenConstantNode<Q>>: LevelLeaf
    {
    };

    template <Described Q, Node Expr>
    struct LevelChildren<DerivedQuantityNode<Q, Expr>>: LevelParent<Expr>
    {
    };

    template <Node Expr>
    struct LevelChildren<ReplacedVariantNode<Expr>>: LevelParent<Expr>
    {
    };
} // namespace detail

/// An overridden constant evaluates to the overlay's value, converted from
/// `Q`'s declared unit to the coherent unit like any other leaf, and never
/// consults the environment -- see `OverriddenConstantNode` for why. The sink
/// is told about the node as its own type, so that a trace can say the value
/// was fixed by an overlay rather than read from the specimen.
///
/// Chosen over the `VarNode<Q>` overload in `evaluate.hpp` for every
/// `OverriddenConstantNode<Q>`, because binding the node to its own type is an
/// identity conversion and binding it to its base is not.
///
namespace detail
{
    /// Tells @p sink about the environment's entry for `Q` that @p node --
    /// an overlay's fixed constant or derived quantity -- replaced, when the
    /// environment holds one: whether it was measured or typed in, through
    /// the optional hook `sink.replaced_entry_source(node, source)`, and when
    /// it held no value, through `sink.replaced_entry_empty(node)`.
    ///
    /// A hook of its own rather than a variable's `input_source`: the source
    /// is the replaced entry's, never the value the step shows, which is the
    /// jurisdiction's. A sink reading `input_source` as "this value was typed
    /// in" is therefore never told it for a constant. Each hook is asked for
    /// only when the sink defines it, and the source only when the
    /// environment can say, as a variable's is (`known_source`,
    /// `evaluate.hpp`).
    template <Described Q, typename Env, typename ReplacingNode, typename Sink>
    constexpr void report_replaced_entry(ReplacingNode const& node, Env const& environment, Sink& sink) noexcept
    {
        if constexpr (requires { Env::template provides<Q>; })
            if constexpr (Env::template provides<Q>)
            {
                if constexpr (requires { sink.replaced_entry_source(node, ValueSource::Measured); }
                              && KnowsSource<Env, Q>)
                    sink.replaced_entry_source(node, known_source<Q>(environment));
                if constexpr (requires { sink.replaced_entry_empty(node); })
                    if (environment.template get<Q>().is_absent())
                        sink.replaced_entry_empty(node);
            }
    }
} // namespace detail

/// When the environment does hold a value for `Q`, a sink that asks is told
/// where that value came from -- the value the overlay's constant replaced
/// -- and when it held none (`detail::report_replaced_entry`). So a trace can
/// say that the overlay replaced a value a person typed in, rather than stay
/// silent about it, and an entry left empty by hand is not traced as a value
/// that was replaced.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(OverriddenConstantNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluated = detail::in_si<Rep>(node.value(), Describe<Q>::unit);
    detail::report_replaced_entry<Q>(node, environment, sink);
    sink.produced(node, evaluated);
    return evaluated;
}

/// A derived quantity evaluates to its expression, against the same
/// environment, and never reads the environment's value for `Q` -- see
/// `DerivedQuantityNode`. The sink is told about the node as its own type, and
/// the expression's own steps become its operands.
///
/// When the environment does hold an entry for `Q`, a sink that asks is told
/// where it came from, and when it held no value, exactly as for a fixed
/// constant (`detail::report_replaced_entry`): the typed value was not used,
/// and the trace says so. Told after the expression, whose own steps each
/// begin and end with the sink's pending source empty, and just before
/// `produced`.
///
/// Chosen over the `VarNode<Q>` overload for the reason the
/// `OverriddenConstantNode` overload above is.
template <typename Rep = Rational, Described Q, Node Expr, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(DerivedQuantityNode<Q, Expr> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluated = detail::dispatch<Rep>(node.expression(), environment, sink);
    detail::report_replaced_entry<Q>(node, environment, sink);
    sink.produced(node, evaluated);
    return evaluated;
}

/// A replaced variant evaluates to its replacement; the sink is told about
/// the node as its own type, with the replacement's steps as its operand.
template <typename Rep = Rational, Node Expr, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ReplacedVariantNode<Expr> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluated = detail::dispatch<Rep>(node.replacement(), environment, sink);
    sink.produced(node, evaluated);
    return evaluated;
}

/// The operation `with_constant<Q>(value, source)` builds: fix `Q` to `value`.
template <Described Q>
struct ConstantOverride
{
    /// The quantity whose value is fixed.
    using quantity = Q;

    /// The value, in `Q`'s declared unit.
    Rational value {};
    /// Where the value comes from; carried onto every node it replaces.
    Citation source {};
};

/// Fixes the quantity `Q` to @p value, in `Q`'s declared unit, wherever the
/// method the overlay is applied to uses it -- in every variant and in every
/// constraint.
///
/// **The overlay's value wins over the environment's.** Each `var<Q>` becomes
/// an `OverriddenConstantNode<Q>`, which evaluates to @p value without asking
/// the environment, so a `Q` supplied there is ignored by the overlaid method
/// and need not be supplied at all. That is what a jurisdiction fixing a
/// constant means: the value is not the specimen's to state.
///
/// @p source records where the value comes from -- a national annex, say --
/// and travels with every node the override leaves behind. Every operation
/// takes a citation argument: the trace exists to say why a value is what it
/// is, and "fixed by jurisdiction overlay" with no citation says nothing a
/// reader can check. An empty one still compiles, and is shown as `(no
/// citation given)` in every clause that would have cited it.
///
/// Refused when no variant or constraint uses `Q`: see the file comment.
template <Described Q>
[[nodiscard]] constexpr ConstantOverride<Q> with_constant(Rational value, Citation source) noexcept
{
    return ConstantOverride<Q> { value, source };
}

/// Refuses a constant with no citation, in the library's words rather than
/// the compiler's "too few arguments". A template on @p Stated only so that
/// the refusal waits for a call.
template <Described Q, bool Stated = false>
[[nodiscard]] constexpr ConstantOverride<Q> with_constant(Rational value) noexcept
{
    static_assert(Stated,
                  "formula: with_constant<Q>(value) was given no citation; a fixed value is a jurisdiction's "
                  "decision, and a trace must say whose -- pass the Citation of the clause that states it");
    return ConstantOverride<Q> { value, {} };
}

/// The operation `pin_variant<Tag>(source)` builds: keep only the variant tagged
/// `Tag`.
///
/// `Tag` obeys the tag rule a variant's tag obeys, through the same guard,
/// asked in this class body for the reason `VariantCase` asks it in its own.
template <typename Tag>
struct VariantPin
{
    static_assert(detail::RequirePlainClassTag<Tag>::value);

    /// The variant that is kept.
    using tag = Tag;

    /// Where the requirement comes from; carried onto the method's variants,
    /// and from there into the trace of the variant selected.
    Citation source {};
};

/// Keeps only the variant tagged `Tag`, making it the one a jurisdiction
/// requires. Selecting any other tag from the overlaid method is then a build
/// error, exactly as selecting a tag the method never declared is.
///
/// Refused when no variant declares `Tag`: see the file comment.
///
/// @p source cites where the requirement comes from, and is required: which
/// variant is mandatory is a jurisdiction's decision (spec section 16.7), and
/// a pin nobody can attribute is what the trace exists to prevent. The trace
/// of the selected variant says the jurisdiction pinned it, `[variant
/// Cylinder (3rd of 3), selected by tag; pinned by jurisdiction overlay:
/// ...]`.
template <typename Tag>
[[nodiscard]] constexpr VariantPin<Tag> pin_variant(Citation source) noexcept
{
    return VariantPin<Tag> { source };
}

/// Refuses a pin with no citation, in the library's words rather than the
/// compiler's "too few arguments". A template on @p Stated only so that the
/// refusal waits for a call.
template <typename Tag, bool Stated = false>
[[nodiscard]] constexpr VariantPin<Tag> pin_variant() noexcept
{
    static_assert(Stated,
                  "formula: pin_variant<Tag>() was given no citation; which variant is mandatory is a "
                  "jurisdiction's decision, and a trace must say whose -- pass the Citation of the clause that "
                  "makes it, pin_variant<Tag>(citation)");
    return {};
}

/// The operation `prune_variant<Tag>(source)` builds: delete the variant tagged
/// `Tag`. `Tag` obeys the tag rule, as `VariantPin` says.
template <typename Tag>
struct VariantPrune
{
    static_assert(detail::RequirePlainClassTag<Tag>::value);

    /// The variant that is deleted.
    using tag = Tag;

    /// Where the deletion comes from; carried as `VariantPin::source` is.
    Citation source {};
};

/// Deletes the variant tagged `Tag`; the others keep their declaration order.
///
/// Refused when no variant declares `Tag`, and when it is the last variant
/// left: see the file comment.
///
/// @p source cites where the deletion comes from, and is required, for the
/// reason `pin_variant`'s is. The trace of the variant selected from what is
/// left says how many were pruned and by whom, `[variant Cylinder (2nd of
/// 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: ...]`.
template <typename Tag>
[[nodiscard]] constexpr VariantPrune<Tag> prune_variant(Citation source) noexcept
{
    return VariantPrune<Tag> { source };
}

/// Refuses a prune with no citation, as the `pin_variant` overload above
/// refuses a pin.
template <typename Tag, bool Stated = false>
[[nodiscard]] constexpr VariantPrune<Tag> prune_variant() noexcept
{
    static_assert(Stated,
                  "formula: prune_variant<Tag>() was given no citation; which variants apply is a jurisdiction's "
                  "decision, and a trace must say whose -- pass the Citation of the clause that deletes it, "
                  "prune_variant<Tag>(citation)");
    return {};
}

/// The operation `with_rounding<U, Places, Mode>(source)` builds: replace the
/// method's rounding rule.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
struct RoundingOverride
{
    static_assert(detail::RequireNamedScaledScalar<U>::value);
    static_assert(detail::RequireAsciiKey<U>::value);

    /// The rule that replaces the method's own, as `rounding_rule<>()` would
    /// spell it.
    using rule = RoundingRule<U, Places, Mode>;

    /// Where the rule comes from; carried onto the method's rule, and from
    /// there into the trace.
    Citation source {};
};

/// Replaces the rounding rule of the method the overlay is applied to with
/// `Places` decimal places of `U`, under `Mode` -- the jurisdiction rounding
/// more finely, or more coarsely, than the base standard.
///
/// **The trace says whose rule it was.** The method's rule then records
/// `RoundingProvenance::JurisdictionOverlay` and @p source, and a trace of the
/// overlaid method renders its rounding step as `rounded to 2 dp
/// (jurisdiction overlay: ...)` where the base method's reads `rounded to 1 dp
/// (method default)`. Which rule applied is half of what spec section 9.1
/// asks for; where it came from is the other half.
///
/// @p source cites where the rule comes from, and is required, as
/// `with_constant`'s is; "jurisdiction overlay" alone does not say which
/// jurisdiction.
///
/// Refused when `U` does not measure what the method reports, by the method
/// the overlay produces, and when one overlay lists it twice: see the file
/// comment. Across overlays, the later one's rule holds.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr RoundingOverride<U, Places, Mode> with_rounding(Citation source) noexcept
{
    return RoundingOverride<U, Places, Mode> { source };
}

/// Replaces the rounding rule as `with_rounding<U, Places, Mode>(source)` does,
/// with the rounding @p R names, cited as @p citedAs: `with_rounding<hundredthMpa>(citation)`.
template <DecimalRounding R>
[[nodiscard]] constexpr RoundingOverride<R.unit, R.places, R.mode> with_rounding(Citation citedAs) noexcept
{
    return RoundingOverride<R.unit, R.places, R.mode> { citedAs };
}

/// Refuses a rounding rule with no citation, as `with_constant`'s overload
/// refuses a constant.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, bool Stated = false>
[[nodiscard]] constexpr RoundingOverride<U, Places, Mode> with_rounding() noexcept
{
    static_assert(Stated,
                  "formula: with_rounding<...>() was given no citation; a rounding rule is a jurisdiction's "
                  "decision, and a trace must say whose -- pass the Citation of the clause that states it");
    return RoundingOverride<U, Places, Mode> {};
}

/// Refuses a rounding rule with no citation, as the three-argument overload
/// does.
template <DecimalRounding R, bool Stated = false>
[[nodiscard]] constexpr RoundingOverride<R.unit, R.places, R.mode> with_rounding() noexcept
{
    return with_rounding<R.unit, R.places, R.mode, Stated>();
}

/// The operation `replace_variant<Tag>(expression, source)` builds: replace the
/// formula of the variant tagged `Tag` wholesale.
///
/// `Tag` obeys the tag rule, as `VariantPin` says. No `{}` default member
/// initialiser on the expression: see `Corrections` (`lookup.hpp`).
template <typename Tag, Node Expr>
struct VariantReplacement
{
    static_assert(detail::RequirePlainClassTag<Tag>::value);

    /// The variant whose formula is replaced.
    using tag = Tag;

    /// The formula that replaces it.
    Expr expression;
    /// Where the replacement comes from; carried into the method, and from
    /// there into the trace.
    Citation source {};
};

/// Replaces the formula of the variant tagged `Tag` with @p expression -- the
/// jurisdiction whose method for that specimen is not the base standard's at
/// all. The variant keeps its tag and its published position, and a trace of
/// it says the formula was replaced, and by whose authority: see
/// `ReplacedVariantNode`.
///
/// Refused when no variant of the method declares `Tag`, when the method the
/// overlay produces does not hold that variant (pinned or pruned away, in
/// either order), when @p expression measures a different dimension from the
/// method's, and when one overlay replaces the same variant twice: see the
/// file comment.
///
/// @p source cites whose formula it is, and is required, as `with_constant`'s
/// is.
template <typename Tag, Node Expr>
[[nodiscard]] constexpr VariantReplacement<Tag, Expr> replace_variant(Expr expression, Citation source) noexcept
{
    return VariantReplacement<Tag, Expr> { expression, source };
}

/// Refuses a replacement with no citation, as `with_constant`'s overload
/// refuses a constant.
template <typename Tag, bool Stated = false, Node Expr>
[[nodiscard]] constexpr VariantReplacement<Tag, Expr> replace_variant(Expr expression) noexcept
{
    static_assert(Stated,
                  "formula: replace_variant<Tag>(expression) was given no citation; a replaced formula is a "
                  "jurisdiction's decision, and a trace must say whose -- pass the Citation of the clause that "
                  "states it");
    return VariantReplacement<Tag, Expr> { expression, {} };
}

/// The operation `with_constraints(constraints(...), source)` builds: replace the
/// method's constraints wholesale.
///
/// No `{}` default member initialiser on the constraint set: it holds
/// predicates, which hold expressions -- see `Corrections` (`lookup.hpp`).
template <Predicate... Ps>
struct ConstraintsOverride
{
    /// The constraints that replace the method's own.
    ConstraintSet<Ps...> constraintSet;
    /// Where they come from; carried with the constraints into the
    /// `OverlaidConstraints` the method then holds, and from there beside
    /// every verdict in the trace.
    Citation source {};
};

/// Replaces the constraints of the method the overlay is applied to with
/// @p replacement -- the jurisdiction whose acceptance logic is not the base
/// standard's: one comparing a pair of determinations where the base checks
/// one, one requiring a mean of three. (A category code in place of a numeric
/// limit is expressible only for a category fixed where the constraint is
/// written: `exact_lookup` takes its key as a value, not from the
/// environment, so no constraint can yet judge the specimen's own category.)
/// The number of constraints is the jurisdiction's, so
/// `check_method` on the overlaid method answers with as many outcomes as
/// @p replacement holds: more than the base method's, fewer, or none.
///
/// **The trace says whose constraints they were.** The method then holds an
/// `OverlaidConstraints` (`method.hpp`) carrying @p source with the
/// constraints, so the claim travels with them into any method built from
/// that part, and every verdict `check_method` records says so --
/// `[satisfied; jurisdiction overlay: ...]` where the base method's reads
/// `[satisfied; the method's own constraint]`.
///
/// A `with_constant` or an `add_derived` listed **after** it reaches inside
/// the new constraints, as it reaches inside every other part of the method.
/// Listed **before** it, the substitution was made in the constraints this
/// replaces, so new constraints reading the quantity plainly are refused by
/// the rule a later replacement's plain use is refused by, and a quantity only
/// the replaced constraints read is refused as read by nothing -- in either
/// order. Refused when one overlay lists it twice: the second would silently
/// discard the first. Across overlays, the later one's constraints hold --
/// and that includes a later overlay's replacing the only constraints an
/// earlier overlay's constant or definition reached, which is accepted, as a
/// later `replace_variant` of the only variant reading one is: the earlier
/// substitution then does not apply.
///
/// @p source cites whose constraints they are, and is required, as
/// `with_constant`'s is.
template <Predicate... Ps>
[[nodiscard]] constexpr ConstraintsOverride<Ps...> with_constraints(ConstraintSet<Ps...> replacement,
                                                                    Citation source) noexcept
{
    return ConstraintsOverride<Ps...> { replacement, source };
}

/// Refuses constraints with no citation, as `with_constant`'s overload
/// refuses a constant.
template <bool Stated = false, Predicate... Ps>
[[nodiscard]] constexpr ConstraintsOverride<Ps...> with_constraints(ConstraintSet<Ps...> replacement) noexcept
{
    static_assert(Stated,
                  "formula: with_constraints(constraints(...)) was given no citation; a method's acceptance checks "
                  "are a jurisdiction's decision, and a trace must say whose -- pass the Citation of the clause "
                  "that states it");
    return ConstraintsOverride<Ps...> { replacement, {} };
}

/// The operation `add_derived<Q>(expression, source)` builds; defined below the
/// rewrite machinery its class body asks.
template <Described Q, Node Expr>
struct QuantityDerivation;

namespace detail
{
    /// Whether a type is one of the operations an overlay can list.
    template <typename T>
    struct IsOverlayOperation: std::false_type
    {
    };

    template <Described Q>
    struct IsOverlayOperation<ConstantOverride<Q>>: std::true_type
    {
    };

    template <typename Tag>
    struct IsOverlayOperation<VariantPin<Tag>>: std::true_type
    {
    };

    template <typename Tag>
    struct IsOverlayOperation<VariantPrune<Tag>>: std::true_type
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode>
    struct IsOverlayOperation<RoundingOverride<U, Places, Mode>>: std::true_type
    {
    };

    template <typename Tag, Node Expr>
    struct IsOverlayOperation<VariantReplacement<Tag, Expr>>: std::true_type
    {
    };

    template <Described Q, Node Expr>
    struct IsOverlayOperation<QuantityDerivation<Q, Expr>>: std::true_type
    {
    };

    template <Predicate... Ps>
    struct IsOverlayOperation<ConstraintsOverride<Ps...>>: std::true_type
    {
    };

    /// Fails to compile when something that is not an overlay operation was
    /// handed to `overlay(...)`. Templated on the position for the reason
    /// `RequireVariant` is.
    template <std::size_t Index, typename Operation>
    struct RequireOverlayOperation
    {
        static_assert(IsOverlayOperation<Operation>::value,
                      "formula: this argument of overlay(...) is not an overlay operation; every argument "
                      "must be what with_constant<Q>(value, citation), add_derived<Q>(expression, citation), "
                      "pin_variant<Tag>(citation), prune_variant<Tag>(citation), replace_variant<Tag>(expression, "
                      "citation), with_rounding<U, Places, Mode>(citation) or with_constraints(constraints(...), "
                      "citation) returns -- the offending argument appears in this "
                      "diagnostic as the template argument Operation of RequireOverlayOperation, and Index is "
                      "its ZERO-BASED position, so 0 is the first argument");

        static constexpr bool value = true;
    };

    /// True when every argument is an overlay operation.
    template <typename Indices, typename... Ops>
    struct RequireEveryArgumentIsAnOperation;

    template <std::size_t... Indices, typename... Ops>
    struct RequireEveryArgumentIsAnOperation<std::index_sequence<Indices...>, Ops...>
    {
        /// True when every one of `Ops` is an overlay operation.
        static constexpr bool value = (RequireOverlayOperation<Indices, Ops>::value && ...);
    };

    /// What every `with_rounding` of an overlay is, for the repeat rule: one
    /// operation, whatever its granularity -- see `OperationIdentity`.
    struct AnyRoundingOverride
    {
    };

    /// What an operation is, for the repeat rule: two operations with the same
    /// identity cannot both do something.
    ///
    /// An operation's identity is what it acts on, not its type:
    /// `with_rounding<Megapascal, 1>` and `with_rounding<Megapascal, 2>` are
    /// two different types acting on the method's one rule, and of two in one
    /// overlay the second silently replaces the first. The same holds of two
    /// substitutions for one quantity and two replacements of one variant --
    /// see `SubstitutionFor` and `ReplacementOf`. Pins and prunes are their
    /// own identity, since a pin's or a prune's type is already its tag.
    template <typename Operation>
    struct OperationIdentity
    {
        /// The operation itself.
        using type = Operation;
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode>
    struct OperationIdentity<RoundingOverride<U, Places, Mode>>
    {
        /// Every rounding override alike.
        using type = AnyRoundingOverride;
    };

    /// What every `with_constraints` of an overlay is, for the repeat rule:
    /// a method has one set of constraints, and of two replacements of it the
    /// second silently discards the first, whatever either holds.
    struct AnyConstraintsOverride
    {
    };

    template <Predicate... Ps>
    struct OperationIdentity<ConstraintsOverride<Ps...>>
    {
        /// Every replacement of the constraints alike.
        using type = AnyConstraintsOverride;
    };

    /// What every operation that substitutes for `Q` is, for the repeat rule:
    /// `with_constant<Q>` and `add_derived<Q>` both replace every use of `Q`,
    /// so of two in one overlay -- two constants, two definitions, or one of
    /// each -- the second silently undoes the first, whatever their values or
    /// expressions.
    template <typename Q>
    struct SubstitutionFor
    {
    };

    /// What every replacement of the variant tagged `Tag` is, for the repeat
    /// rule: of two, the second silently discards the first's formula.
    template <typename Tag>
    struct ReplacementOf
    {
    };

    template <Described Q>
    struct OperationIdentity<ConstantOverride<Q>>
    {
        /// Every substitution for `Q` alike.
        using type = SubstitutionFor<Q>;
    };

    template <Described Q, Node Expr>
    struct OperationIdentity<QuantityDerivation<Q, Expr>>
    {
        /// Every substitution for `Q` alike.
        using type = SubstitutionFor<Q>;
    };

    template <typename Tag, Node Expr>
    struct OperationIdentity<VariantReplacement<Tag, Expr>>
    {
        /// Every replacement of the variant tagged `Tag` alike, whatever its
        /// formula.
        using type = ReplacementOf<Tag>;
    };

    /// Fails to compile when one overlay lists the same operation twice.
    ///
    /// Two operations are the same when they have the same identity, which is
    /// exactly the case where one of them does nothing: two overrides of one
    /// quantity are both `ConstantOverride<Q>` whatever their values, and the
    /// second replaces the first; two pins of one variant are both
    /// `VariantPin<Tag>`, and the second pins what is already pinned; two
    /// rounding overrides of any granularities are both the method's one rule
    /// -- see `OperationIdentity`. Asked through `first_repeated_pair`, the
    /// same statement of "no two alike" that the distinct-tag rule of
    /// `method.hpp` asks.
    template <std::size_t First, std::size_t Second, typename Operation>
    struct RequireOperationListedOnce
    {
        static_assert(First == Second,
                      "formula: this overlay lists the same operation twice; two constants or definitions of "
                      "one quantity, two pins, prunes or replacements of one variant, two rounding overrides, "
                      "or two replacements of the constraints, leave the first silently doing nothing -- the "
                      "first of the two appears in "
                      "this diagnostic as the template "
                      "argument Operation of RequireOperationListedOnce, and First and Second are the "
                      "ZERO-BASED positions of the two arguments that list it, so 0 is the first argument");

        static constexpr bool value = true;
    };

    /// True when no two operations are alike; the refusal is
    /// `RequireOperationListedOnce`'s. The empty overlay repeats nothing.
    template <typename... Ops>
    struct RequireDistinctOperations
    {
        /// Always true: an empty overlay repeats nothing.
        static constexpr bool value = true;
    };

    template <typename First, typename... Rest>
    struct RequireDistinctOperations<First, Rest...>
    {
        /// Where the first repeated operation sits, if anywhere.
        static constexpr PositionPair repeated =
            first_repeated_pair<typename OperationIdentity<First>::type, typename OperationIdentity<Rest>::type...>();

        /// True when no operation is repeated.
        static constexpr bool value =
            RequireOperationListedOnce<repeated.first,
                                       repeated.second,
                                       std::tuple_element_t<repeated.first, std::tuple<First, Rest...>>>::value;
    };

    /// Whether an operation is a pin.
    template <typename T>
    struct IsVariantPin: std::false_type
    {
    };

    template <typename Tag>
    struct IsVariantPin<VariantPin<Tag>>: std::true_type
    {
    };

    /// Whether an operation is a prune.
    template <typename T>
    struct IsVariantPrune: std::false_type
    {
    };

    template <typename Tag>
    struct IsVariantPrune<VariantPrune<Tag>>: std::true_type
    {
    };

    /// Whether an overlay both pins and prunes -- a combination refused by
    /// `RequireNoPruneBesidePin`, in whichever order the two are listed.
    template <typename... Ops>
    inline constexpr bool pinsAndPrunes = (IsVariantPin<Ops>::value || ...) && (IsVariantPrune<Ops>::value || ...);

    /// Fails to compile when one overlay both pins a variant and prunes one.
    ///
    /// A pin already states the whole selection: the method keeps that one
    /// variant and nothing else. A prune beside it can then only do nothing
    /// -- pruning a variant the pin drops anyway -- or contradict it -- pruning
    /// the variant pinned. Neither is a jurisdiction's intent, and which of the
    /// two it is should not depend on the order the author listed them in, so
    /// the combination is refused whatever the order.
    template <bool PinsAndPrunes>
    struct RequireNoPruneBesidePin
    {
        static_assert(!PinsAndPrunes,
                      "formula: this overlay both pins a variant and prunes one; a pin already keeps exactly one "
                      "variant, so a prune beside it either removes a variant the pin drops anyway or removes the "
                      "one it pins -- list the pin alone, or the prunes alone");

        static constexpr bool value = true;
    };

    /// The rules an overlay's list obeys. The repeat rule and the pin-or-prune
    /// rule are asked only once every argument is an operation, for the reason
    /// `RequireWellFormedVariants` gates its own: `overlay(42, 42)` is two
    /// arguments that are not operations, and saying also that it lists one
    /// operation twice would bury the message that matters.
    template <typename... Ops>
    struct RequireWellFormedOverlay
    {
        static_assert(RequireEveryArgumentIsAnOperation<std::index_sequence_for<Ops...>, Ops...>::value);

        /// Whether the rules that compare operations have operations to compare.
        static constexpr bool everyArgumentIsAnOperation = (IsOverlayOperation<Ops>::value && ...);

        static_assert(
            std::conditional_t<everyArgumentIsAnOperation, RequireDistinctOperations<Ops...>, std::true_type>::value);
        static_assert(RequireNoPruneBesidePin<everyArgumentIsAnOperation && pinsAndPrunes<Ops...>>::value);

        static constexpr bool value = true;
    };

    /// Whether an operation passes the rules its own class body asks: true for
    /// every operation but a derivation its class body refuses. (A replacement
    /// whose tag is refused needs no entry: it is never applied, and every
    /// check that could follow waits on `ReplacementApplied`.) Asked by `isWellFormedOverlay`,
    /// so that `apply` is not instantiated over an operation already refused
    /// where it was written -- which would otherwise go on to be judged, and
    /// refused a second time, in the words of a rule about the result.
    template <typename Operation>
    struct IsValidOperation: std::true_type
    {
    };

    template <Described Q, Node Expr>
    struct IsValidOperation<QuantityDerivation<Q, Expr>>: std::bool_constant<QuantityDerivation<Q, Expr>::valid>
    {
    };

    /// Whether an overlay's list passes every rule `RequireWellFormedOverlay`
    /// asks, and every rule its operations' own class bodies ask, asked
    /// without firing any of them.
    template <typename... Ops>
    inline constexpr bool isWellFormedOverlay =
        (IsOverlayOperation<Ops>::value && ...) && all_distinct<typename OperationIdentity<Ops>::type...>()
        && !pinsAndPrunes<Ops...> && (IsValidOperation<Ops>::value && ...);
} // namespace detail

/// One jurisdiction's changes to a method, in the order they apply.
///
/// A plain aggregate over a pack, as `Variants` and `ConstraintSet` are, and
/// checked in this class body rather than only in `overlay()` for the reason
/// `Variants` gives: an `Overlay<...>` can be declared with no factory call.
template <typename... Ops>
struct Overlay
{
    static_assert(detail::RequireWellFormedOverlay<Ops...>::value);

    /// The operations, in the order they apply.
    std::tuple<Ops...> operations;
};

/// Builds an overlay: `overlay(with_constant<Q>(v, source), prune_variant<Cube>(source))`.
///
/// An empty `overlay()` is accepted, and applying it yields the method
/// unchanged. It declares no change rather than a change that silently fails
/// to happen, and it is what a jurisdiction adopting the base method as it
/// stands honestly is.
template <typename... Ops>
[[nodiscard]] constexpr Overlay<Ops...> overlay(Ops... operations) noexcept
{
    return Overlay<Ops...> { std::tuple<Ops...> { operations... } };
}

namespace detail
{
    /// Fails to compile when `with_constant` meets a node kind it does not
    /// know. Such a node may hold a `var<Q>` the rewrite cannot reach, which
    /// would then go on reading the environment while every reachable use of
    /// `Q` reads the overlay -- one formula evaluating one quantity at two
    /// values. Refusing is the only answer that is never silently wrong.
    template <typename N>
    struct RequireOverlaySeesNode
    {
        static_assert(alwaysFalse<N>,
                      "formula: this overlay overrides a constant in an expression holding a node kind it "
                      "cannot see inside; a use of the quantity there would silently keep its environment "
                      "value, so the overlay refuses rather than rewrite part of the formula -- the node kind "
                      "appears in this diagnostic as the template argument N of RequireOverlaySeesNode");

        static constexpr bool value = true;
    };

    /// The same refusal for `add_derived`, in its own words: a use of the
    /// quantity inside a node kind the rewrite cannot see would keep reading
    /// the environment while every other use evaluates the definition.
    template <typename N>
    struct RequireDerivationSeesNode
    {
        static_assert(alwaysFalse<N>,
                      "formula: this overlay derives a quantity in an expression holding a node kind it cannot "
                      "see inside; a use of the quantity there would silently keep its environment value, so the "
                      "overlay refuses rather than rewrite part of the formula -- the node kind appears in this "
                      "diagnostic as the template argument N of RequireDerivationSeesNode");

        static constexpr bool value = true;
    };

    /// Whether a substitution is `add_derived`'s rather than `with_constant`'s.
    template <typename Sub>
    struct IsDerivation: std::false_type
    {
    };

    template <Described Q, Node Expr>
    struct IsDerivation<QuantityDerivation<Q, Expr>>: std::true_type
    {
    };

    /// The node a substitution leaves where the method had `var<Q>`: an
    /// overridden constant for `with_constant`, a derived quantity for
    /// `add_derived` -- built through `OverlayNodeAccess`, the one way to build
    /// either.
    template <Described Q>
    [[nodiscard]] constexpr OverriddenConstantNode<Q> substitute(ConstantOverride<Q> const& overriding) noexcept
    {
        return OverlayNodeAccess::fixed<Q>(overriding.value, overriding.source);
    }

    /// Defined below `QuantityDerivation`, which it reads.
    template <Described Q, Node Expr>
    [[nodiscard]] constexpr DerivedQuantityNode<Q, Expr> substitute(QuantityDerivation<Q, Expr> const& deriving) noexcept;

    /// A stand-in substitution for `Q`, for asking whether an expression is
    /// known and uses `Q` before any real substitution exists --
    /// `QuantityDerivation` asks it of its own expression in its class body,
    /// where the class is still incomplete. Its `substitute` is declared for
    /// the rewrite's `type` to name, and never called.
    template <Described Q>
    struct QuantityProbe
    {
        /// The quantity asked about.
        using quantity = Q;
    };

    template <Described Q>
    [[nodiscard]] constexpr VarNode<Q> substitute(QuantityProbe<Q> const&) noexcept
    {
        return {};
    }

    /// A stand-in that counts only the PLAIN uses of `Q` -- `var<Q>` itself,
    /// not the nodes a substitution left -- and one that counts only those
    /// nodes. The result check asks both of the produced method: a
    /// substitution is in effect only if its node is still there, and it is
    /// in effect everywhere only if no plain use is.
    template <Described Q>
    struct PlainUseProbe
    {
        /// The quantity asked about.
        using quantity = Q;
    };

    template <Described Q>
    struct SubstitutedUseProbe
    {
        /// The quantity asked about.
        using quantity = Q;
    };

    template <Described Q>
    [[nodiscard]] constexpr VarNode<Q> substitute(PlainUseProbe<Q> const&) noexcept
    {
        return {};
    }

    template <Described Q>
    [[nodiscard]] constexpr VarNode<Q> substitute(SubstitutedUseProbe<Q> const&) noexcept
    {
        return {};
    }

    /// A stand-in that counts only the uses of `Q` as a SERIES --
    /// `series<Q, N>` -- which no substitution replaces: one constant or one
    /// definition cannot stand for a value at every point. The result check
    /// asks it, to refuse such a substitution in words that say so.
    template <Described Q>
    struct SeriesUseProbe
    {
        /// The quantity asked about.
        using quantity = Q;
    };

    template <Described Q>
    [[nodiscard]] constexpr VarNode<Q> substitute(SeriesUseProbe<Q> const&) noexcept
    {
        return {};
    }

    /// A stand-in substitution for no quantity at all, for asking only
    /// whether every node of an expression is a kind the rewrite knows. Its
    /// `substitute` is declared for the rewrite's `type` to name, and never
    /// called.
    struct KnownProbe
    {
        /// No quantity: nothing is ever this one.
        using quantity = void;
    };

    [[nodiscard]] constexpr KnownProbe substitute(KnownProbe const& probe) noexcept
    {
        return probe;
    }

    /// Whether @p Sub's `mentions` counts a plain `var<Q>`: every
    /// substitution and probe does, except `SubstitutedUseProbe`.
    template <typename Sub>
    inline constexpr bool countsPlainUse = true;

    template <Described Q>
    inline constexpr bool countsPlainUse<SubstitutedUseProbe<Q>> = false;

    template <Described Q>
    inline constexpr bool countsPlainUse<SeriesUseProbe<Q>> = false;

    /// Whether @p Sub's `mentions` counts a node a substitution for `Q` left:
    /// every substitution and probe does, except `PlainUseProbe`.
    template <typename Sub>
    inline constexpr bool countsSubstitutedUse = true;

    template <Described Q>
    inline constexpr bool countsSubstitutedUse<PlainUseProbe<Q>> = false;

    template <Described Q>
    inline constexpr bool countsSubstitutedUse<SeriesUseProbe<Q>> = false;

    /// Whether @p Sub's `mentions` counts `series<Q, N>`: only
    /// `SeriesUseProbe` does. A substitution never replaces a series, so to
    /// it a series of `Q` is not a use of `Q` it could be in effect at.
    template <typename Sub>
    inline constexpr bool countsSeriesUse = false;

    template <Described Q>
    inline constexpr bool countsSeriesUse<SeriesUseProbe<Q>> = true;

    /// The type `substitute` returns for @p Sub.
    template <typename Sub>
    using Substituted = decltype(substitute(std::declval<Sub const&>()));

    /// How a substitution @p Sub for a quantity `Q` -- `with_constant<Q>` or
    /// `add_derived<Q>`, whose `quantity` is `Q` -- rewrites a node of type
    /// @p N, one specialisation per node kind this library ships.
    ///
    /// Each answers three things together, so that no two of them can drift:
    ///
    ///  - `known`: whether every node in the subtree is a kind this header
    ///    can see inside;
    ///  - `mentions`: whether the subtree uses `Q` -- as `var<Q>`, or as an
    ///    `OverriddenConstantNode<Q>` or a `DerivedQuantityNode<Q, ...>` an
    ///    earlier overlay left;
    ///  - `type` and `apply`: the rewritten subtree, with every such use now
    ///    what `substitute` builds for @p Sub, and every other node, runtime
    ///    contents included, carried over unchanged.
    ///
    /// The primary template is every other node kind. It answers `known =
    /// false`, and its `apply` is refused -- see `RequireOverlaySeesNode` and
    /// `RequireDerivationSeesNode`.
    template <typename Sub, typename N>
    struct ConstantRewrite
    {
        /// A node kind this header does not know.
        static constexpr bool known = false;
        /// Unknowable; `known` is what the caller asks first.
        static constexpr bool mentions = false;
        /// Unchanged, because it cannot be looked into.
        using type = N;

        /// Refused, in the words of the operation that met it; see
        /// `RequireOverlaySeesNode`.
        [[nodiscard]] static constexpr type apply(N const& original, Sub const&) noexcept
        {
            if constexpr (IsDerivation<Sub>::value)
                static_assert(RequireDerivationSeesNode<N>::value);
            else
                static_assert(RequireOverlaySeesNode<N>::value);
            return original;
        }
    };

    /// How `with_constant<Q>` rewrites a child of type @p N, whatever its cv
    /// qualification.
    ///
    /// A node aggregate declared off the factory path takes its child types
    /// from wherever the author took them, and `decltype` of a `constexpr`
    /// variable is `const`: `DocumentedNode<decltype(sub)> { {}, sub, ... }`
    /// holds a `const` child, which `Node` accepts and the evaluator, the
    /// renderer and `document()` all handle. No partial specialisation of
    /// `ConstantRewrite` matches a `const` type, so every child is rewritten
    /// through this, and never through `ConstantRewrite` directly -- otherwise
    /// the overlay would refuse a node kind it knows as one it cannot see
    /// inside. `Method` strips the same qualifier from its parts, for the same
    /// reason. The rewritten child is unqualified: a new node holds it by
    /// value.
    template <typename Sub, typename N>
    using ConstantRewriteOf = ConstantRewrite<Sub, std::remove_cv_t<N>>;

    /// A variable: replaced when it names `Q`, and left alone otherwise.
    template <typename Sub, Described P>
    struct ConstantRewrite<Sub, VarNode<P>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether this is `var<Q>`.
        static constexpr bool isQ = std::is_same_v<typename Sub::quantity, P>;
        /// Whether this is a use of `Q` that @p Sub counts -- see
        /// `countsPlainUse`.
        static constexpr bool mentions = isQ && countsPlainUse<Sub>;
        /// The substitution when this is `var<Q>`, and this variable
        /// otherwise.
        using type = std::conditional_t<isQ, Substituted<Sub>, VarNode<P>>;

        /// The rewritten node.
        [[nodiscard]] static constexpr type apply([[maybe_unused]] VarNode<P> const& original,
                                                  [[maybe_unused]] Sub const& overriding) noexcept
        {
            if constexpr (isQ)
                return substitute(overriding);
            else
                return original;
        }
    };

    /// A constant an earlier overlay fixed: fixed again, to the new value and
    /// source, when it is `Q`'s. That is what applying a national overlay over
    /// a regional one means -- the later overlay's value is the one that
    /// holds.
    ///
    /// Needed as well as the `VarNode` specialisation above, and not merely
    /// for the re-override: a class template's partial specialisations never
    /// match a derived class, so without this one an overridden constant would
    /// fall to the primary template and be refused as a kind nobody knows.
    template <typename Sub, Described P>
    struct ConstantRewrite<Sub, OverriddenConstantNode<P>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether this is `Q`'s.
        static constexpr bool isQ = std::is_same_v<typename Sub::quantity, P>;
        /// Whether this is a use of `Q` that @p Sub counts -- see
        /// `countsSubstitutedUse`.
        static constexpr bool mentions = isQ && countsSubstitutedUse<Sub>;
        /// The new substitution when this is `Q`'s -- a constant fixed again,
        /// or now defined -- and this node otherwise.
        using type = std::conditional_t<isQ, Substituted<Sub>, OverriddenConstantNode<P>>;

        /// The substitution when it is `Q`'s, and the node otherwise.
        [[nodiscard]] static constexpr type apply([[maybe_unused]] OverriddenConstantNode<P> const& original,
                                                  [[maybe_unused]] Sub const& overriding) noexcept
        {
            if constexpr (isQ)
                return substitute(overriding);
            else
                return original;
        }
    };

    /// A quantity an earlier overlay defined: replaced wholesale when it is
    /// `Q`'s -- the later overlay's definition or constant is the one that
    /// holds -- and otherwise rewritten inside, since its definition may read
    /// `Q` and is evaluated where it stands. Needed for the derived-class
    /// reason the `OverriddenConstantNode` specialisation gives.
    template <typename Sub, Described P, Node Expr>
    struct ConstantRewrite<Sub, DerivedQuantityNode<P, Expr>>
    {
        /// How the definition is rewritten, when the node is kept.
        using DefinitionRewrite = ConstantRewriteOf<Sub, Expr>;
        /// Whether this node is `Q`'s own, and so replaced whole.
        static constexpr bool isQ = std::is_same_v<typename Sub::quantity, P>;

        /// Known when replaced whole -- nothing of it survives to hide a use
        /// -- and otherwise when its definition is.
        static constexpr bool known = isQ || DefinitionRewrite::known;
        /// Whether this is `Q`'s -- a use @p Sub counts, see
        /// `countsSubstitutedUse` -- or its definition uses `Q`.
        ///
        /// The definition is asked even when the node is `Q`'s own. A real
        /// substitution replaces such a node whole, so the answer is `true`
        /// either way; but the result check's `PlainUseProbe` asks whether a
        /// plain `var<Q>` is left anywhere, and one inside `Q`'s own definition
        /// -- put there by a later definition of another quantity, a cycle --
        /// is evaluated, reading the environment. `overlay_derived_cycle`
        /// pins it.
        static constexpr bool mentions = (isQ && countsSubstitutedUse<Sub>) || DefinitionRewrite::mentions;
        /// The substitution when it is `Q`'s, and the same quantity over the
        /// rewritten definition otherwise.
        using type = std::conditional_t<isQ, Substituted<Sub>, DerivedQuantityNode<P, typename DefinitionRewrite::type>>;

        /// The rewritten node, keeping its citation when it is kept.
        [[nodiscard]] static constexpr type apply(DerivedQuantityNode<P, Expr> const& original,
                                                  Sub const& overriding) noexcept
        {
            if constexpr (isQ)
                return substitute(overriding);
            else
                return OverlayNodeAccess::derived<P>(DefinitionRewrite::apply(original.expression(), overriding),
                                                     original.source());
        }
    };

    /// A replaced variant: its replacement rewritten, its citation kept.
    template <typename Sub, Node Expr>
    struct ConstantRewrite<Sub, ReplacedVariantNode<Expr>>
    {
        /// How the replacement is rewritten.
        using Replacement = ConstantRewriteOf<Sub, Expr>;

        /// Whether the replacement is known all the way down.
        static constexpr bool known = Replacement::known;
        /// Whether the replacement uses `Q`.
        static constexpr bool mentions = Replacement::mentions;
        /// The marker, around the rewritten replacement.
        using type = ReplacedVariantNode<typename Replacement::type>;

        /// The marker, around the rewritten replacement, with its citation.
        [[nodiscard]] static constexpr type apply(ReplacedVariantNode<Expr> const& original, Sub const& overriding) noexcept
        {
            return OverlayNodeAccess::replaced(Replacement::apply(original.replacement(), overriding), original.source());
        }
    };

    /// A leaf that names no quantity: carried over unchanged.
    template <typename Sub, typename N>
    struct ConstantRewriteLeaf
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// A leaf naming no quantity never uses `Q`.
        static constexpr bool mentions = false;
        /// Unchanged.
        using type = N;

        /// The node itself.
        [[nodiscard]] static constexpr type apply(N const& original, Sub const&) noexcept
        {
            return original;
        }
    };

    template <typename Sub, Unit U>
    struct ConstantRewrite<Sub, ConstantNode<U>>: ConstantRewriteLeaf<Sub, ConstantNode<U>>
    {
    };

    template <typename Sub>
    struct ConstantRewrite<Sub, PiNode>: ConstantRewriteLeaf<Sub, PiNode>
    {
    };

    /// An exact lookup has no operand: its key is data, not an expression.
    template <typename Sub, KeyTable Keys, Unit ResultUnit>
    struct ConstantRewrite<Sub, ExactLookupNode<Keys, ResultUnit>>:
        ConstantRewriteLeaf<Sub, ExactLookupNode<Keys, ResultUnit>>
    {
    };

    /// A node whose only child is its `operand`, and whose only runtime state
    /// that child is, rebuilt as @p Rebuilt around the rewritten operand.
    template <typename Sub, typename Operand, typename Rebuilt>
    struct ConstantRewriteOperand
    {
        /// How the operand is rewritten.
        using Inner = ConstantRewriteOf<Sub, Operand>;

        /// Whether the operand is a kind this header knows, all the way down.
        static constexpr bool known = Inner::known;
        /// Whether the operand uses `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same node kind, around the rewritten operand.
        using type = Rebuilt;

        /// The node, around the rewritten operand.
        template <typename N>
        [[nodiscard]] static constexpr type apply(N const& node, Sub const& overriding) noexcept
        {
            return type { {}, Inner::apply(node.operand, overriding) };
        }
    };

    template <typename Sub, UnaryOperator Op, Node Operand>
    struct ConstantRewrite<Sub, UnaryNode<Op, Operand>>:
        ConstantRewriteOperand<Sub, Operand, UnaryNode<Op, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, int Exponent, Node Operand>
    struct ConstantRewrite<Sub, PowerNode<Exponent, Operand>>:
        ConstantRewriteOperand<Sub, Operand, PowerNode<Exponent, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, int Degree, Node Operand>
    struct ConstantRewrite<Sub, RootNode<Degree, Operand>>:
        ConstantRewriteOperand<Sub, Operand, RootNode<Degree, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Transcendental F, Node Operand>
    struct ConstantRewrite<Sub, TranscendentalNode<F, Operand>>:
        ConstantRewriteOperand<Sub, Operand, TranscendentalNode<F, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Sub, RoundNode<U, Places, Mode, Operand>>:
        ConstantRewriteOperand<Sub, Operand, RoundNode<U, Places, Mode, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Sub, RoundSignificantNode<U, Digits, Mode, Operand>>:
        ConstantRewriteOperand<Sub,
                               Operand,
                               RoundSignificantNode<U, Digits, Mode, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    struct ConstantRewrite<Sub, SnapNode<KeyUnit, Permitted, Tie, Operand>>:
        ConstantRewriteOperand<Sub,
                               Operand,
                               SnapNode<KeyUnit, Permitted, Tie, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    /// A rounded square root, rebuilt around its rewritten radicand. Not
    /// `ConstantRewriteOperand`, whose `apply` reads a member named `operand`:
    /// this node's one child is its `radicand`.
    template <typename Sub, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    struct ConstantRewrite<Sub, RoundedRootNode<U, Places, Mode, Radicand>>
    {
        /// How the radicand is rewritten.
        using Inner = ConstantRewriteOf<Sub, Radicand>;

        /// Whether the radicand is a kind this header knows, all the way down.
        static constexpr bool known = Inner::known;
        /// Whether the radicand uses `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same rounded root, around the rewritten radicand.
        using type = RoundedRootNode<U, Places, Mode, typename Inner::type>;

        /// The node, around the rewritten radicand.
        [[nodiscard]] static constexpr type apply(RoundedRootNode<U, Places, Mode, Radicand> const& node,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, Inner::apply(node.radicand, overriding) };
        }
    };

    template <typename Sub, Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Sub, RoundedTranscendentalNode<F, Places, Mode, Operand>>:
        ConstantRewriteOperand<Sub,
                               Operand,
                               RoundedTranscendentalNode<F, Places, Mode, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Node Operand>
    struct ConstantRewrite<Sub, AbsoluteValueNode<Operand>>:
        ConstantRewriteOperand<Sub, Operand, AbsoluteValueNode<typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    /// A precision limit's level names no input quantity: carried over
    /// unchanged. The quantity it names is for its unit, and a constant fixed
    /// for that quantity does not reach the level, which is the level the
    /// limit's level expression produced.
    template <typename Sub, Described Q>
    struct ConstantRewrite<Sub, PrecisionLevelNode<Q>>: ConstantRewriteLeaf<Sub, PrecisionLevelNode<Q>>
    {
    };

    /// A precision limit, both its level and its limit rewritten.
    template <typename Sub, PrecisionKind K, Node Level, Node Limit>
    struct ConstantRewrite<Sub, PrecisionLimitNode<K, Level, Limit>>
    {
        /// How the level expression is rewritten.
        using LevelRewrite = ConstantRewriteOf<Sub, Level>;
        /// How the limit expression is rewritten.
        using LimitRewrite = ConstantRewriteOf<Sub, Limit>;

        /// Whether both are kinds this header knows, all the way down.
        static constexpr bool known = LevelRewrite::known && LimitRewrite::known;
        /// Whether either uses `Q`.
        static constexpr bool mentions = LevelRewrite::mentions || LimitRewrite::mentions;
        /// The same limit, around the rewritten level and limit.
        using type = PrecisionLimitNode<K, typename LevelRewrite::type, typename LimitRewrite::type>;

        /// The node, around the rewritten level and limit.
        [[nodiscard]] static constexpr type apply(PrecisionLimitNode<K, Level, Limit> const& node,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, LevelRewrite::apply(node.level, overriding), LimitRewrite::apply(node.limit, overriding) };
        }
    };

    /// A critical-value lookup, rebuilt around its rewritten count with its
    /// own values: a jurisdiction fixing a quantity changes what the count
    /// reads, never the table.
    template <typename Sub, SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    struct ConstantRewrite<Sub, SampleSizeLookupNode<Sizes, ResultUnit, Count>>
    {
        /// How the count is rewritten.
        using Inner = ConstantRewriteOf<Sub, Count>;

        /// Whether the count is a kind this header knows, all the way down.
        static constexpr bool known = Inner::known;
        /// Whether the count uses `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same lookup, around the rewritten count.
        using type = SampleSizeLookupNode<Sizes, ResultUnit, typename Inner::type>;

        /// The node, around the rewritten count, with the same values.
        [[nodiscard]] static constexpr type apply(SampleSizeLookupNode<Sizes, ResultUnit, Count> const& node,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, node.corrections, Inner::apply(node.count, overriding) };
        }
    };

    template <typename Sub, Unit U, FixedString Justification, Node Operand>
    struct ConstantRewrite<Sub, NumericValueNode<U, Justification, Operand>>:
        ConstantRewriteOperand<Sub,
                               Operand,
                               NumericValueNode<U, Justification, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    /// A lookup whose key is an expression: its operand is rewritten, and its
    /// table's contents -- runtime state, like a `ConstantNode`'s number --
    /// are carried over.
    template <typename Sub, typename Operand, typename Rebuilt>
    struct ConstantRewriteLookup: ConstantRewriteOperand<Sub, Operand, Rebuilt>
    {
        /// The lookup, around the rewritten operand, with its contents.
        template <typename N>
        [[nodiscard]] static constexpr Rebuilt apply(N const& node, Sub const& overriding) noexcept
        {
            return Rebuilt { {}, node.corrections, ConstantRewriteOf<Sub, Operand>::apply(node.operand, overriding) };
        }
    };

    template <typename Sub, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    struct ConstantRewrite<Sub, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand>>:
        ConstantRewriteLookup<Sub,
                              Operand,
                              BandedLookupNode<KeyUnit, Bands, ResultUnit, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    struct ConstantRewrite<Sub, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand>>:
        ConstantRewriteLookup<
            Sub,
            Operand,
            InterpolatingLookupNode<KeyUnit, Points, ResultUnit, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    /// A citation's wrapper: the wrapped formula is rewritten and the citation
    /// carried over.
    template <typename Sub, Node Inner>
    struct ConstantRewrite<Sub, DocumentedNode<Inner>>
    {
        /// How the wrapped formula is rewritten.
        using Wrapped = ConstantRewriteOf<Sub, Inner>;

        /// Whether the wrapped formula is known all the way down.
        static constexpr bool known = Wrapped::known;
        /// Whether the wrapped formula uses `Q`.
        static constexpr bool mentions = Wrapped::mentions;
        /// The wrapper, around the rewritten formula.
        using type = DocumentedNode<typename Wrapped::type>;

        /// The wrapper, around the rewritten formula, with its citation.
        [[nodiscard]] static constexpr type apply(DocumentedNode<Inner> const& original, Sub const& overriding) noexcept
        {
            return type { {}, Wrapped::apply(original.inner, overriding), original.citation };
        }
    };

    /// A series variable: known, and never replaced -- not even when it is a
    /// series of `Q`, since one constant or one definition cannot stand for a
    /// value at every point. A substitution for a quantity the method reads
    /// as a series is refused by the result check (`RequireConstantApplies`),
    /// which finds it through `SeriesUseProbe`.
    template <typename Sub, Described P, std::size_t N>
    struct ConstantRewrite<Sub, SeriesVarNode<P, N>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether this is a series of `Q` and @p Sub counts one -- see
        /// `countsSeriesUse`.
        static constexpr bool mentions = std::is_same_v<typename Sub::quantity, P> && countsSeriesUse<Sub>;
        /// Unchanged.
        using type = SeriesVarNode<P, N>;

        /// The node itself.
        [[nodiscard]] static constexpr type apply(SeriesVarNode<P, N> const& original, Sub const&) noexcept
        {
            return original;
        }
    };

    /// A refused series names nothing, so an overlay over a method holding one
    /// adds no "cannot see inside" message to the refusal that produced it.
    template <typename Sub, Dimension Dim>
    struct ConstantRewrite<Sub, RefusedSeries<Dim>>: ConstantRewriteLeaf<Sub, RefusedSeries<Dim>>
    {
    };

    /// A per-element constant names no quantity; its values are carried over.
    template <typename Sub, Unit U, std::size_t N>
    struct ConstantRewrite<Sub, SeriesConstantNode<U, N>>: ConstantRewriteLeaf<Sub, SeriesConstantNode<U, N>>
    {
    };

    /// A declared domain names no quantity: its points are in its type.
    template <typename Sub, Unit U, BreakpointTable Points>
    struct ConstantRewrite<Sub, DomainNode<U, Points>>: ConstantRewriteLeaf<Sub, DomainNode<U, Points>>
    {
    };

    /// A node of two children, @p First and @p Second, rebuilt as @p Rebuilt
    /// around both rewritten -- a curve's two series, a splice's two curves,
    /// an interpolation's curve and point. @p Children reads the two back off
    /// a node, in the order `Rebuilt` is aggregate-initialised.
    template <typename Sub,
              typename First,
              typename Second,
              template <typename, typename> typename Rebuilt,
              typename Children>
    struct ConstantRewriteTwo
    {
        /// How the first child is rewritten.
        using FirstRewrite = ConstantRewriteOf<Sub, First>;
        /// How the second child is rewritten.
        using SecondRewrite = ConstantRewriteOf<Sub, Second>;

        /// Whether both children are known all the way down.
        static constexpr bool known = FirstRewrite::known && SecondRewrite::known;
        /// Whether either child uses `Q`.
        static constexpr bool mentions = FirstRewrite::mentions || SecondRewrite::mentions;
        /// The same node kind, around the rewritten children.
        using type = Rebuilt<typename FirstRewrite::type, typename SecondRewrite::type>;

        /// The node, around the rewritten children.
        template <typename N>
        [[nodiscard]] static constexpr type apply(N const& original, Sub const& overriding) noexcept
        {
            return type { {},
                          FirstRewrite::apply(Children::first_of(original), overriding),
                          SecondRewrite::apply(Children::second_of(original), overriding) };
        }
    };

    /// A curve's two series.
    struct CurveChildren
    {
        template <typename N>
        [[nodiscard]] static constexpr auto const& first_of(N const& original) noexcept
        {
            return original.domainSeries;
        }
        template <typename N>
        [[nodiscard]] static constexpr auto const& second_of(N const& original) noexcept
        {
            return original.valueSeries;
        }
    };

    /// A splice's two curves.
    struct SpliceChildren
    {
        template <typename N>
        [[nodiscard]] static constexpr auto const& first_of(N const& original) noexcept
        {
            return original.first;
        }
        template <typename N>
        [[nodiscard]] static constexpr auto const& second_of(N const& original) noexcept
        {
            return original.second;
        }
    };

    /// An interpolation's curve and point.
    struct InterpolationChildren
    {
        template <typename N>
        [[nodiscard]] static constexpr auto const& first_of(N const& original) noexcept
        {
            return original.along;
        }
        template <typename N>
        [[nodiscard]] static constexpr auto const& second_of(N const& original) noexcept
        {
            return original.at;
        }
    };

    /// The three kinds as templates of their two children alone, for
    /// `ConstantRewriteTwo`: aliases, whose template heads carry no
    /// constraint for a template template parameter to be matched against.
    template <typename DomainSeries, typename ValueSeries>
    using CurveOf = CurveNode<DomainSeries, ValueSeries>;

    template <typename C, typename At>
    using InterpolationOf = InterpolateAlongNode<C, At>;

    template <Monotone M>
    struct SpliceIn
    {
        template <typename A, typename B>
        using type = SpliceNode<M, A, B>;
    };

    template <typename Sub, SeriesNode DomainSeries, SeriesNode ValueSeries>
    struct ConstantRewrite<Sub, CurveNode<DomainSeries, ValueSeries>>:
        ConstantRewriteTwo<Sub, DomainSeries, ValueSeries, CurveOf, CurveChildren>
    {
    };

    template <typename Sub, Monotone M, CurveExpression A, CurveExpression B>
    struct ConstantRewrite<Sub, SpliceNode<M, A, B>>:
        ConstantRewriteTwo<Sub, A, B, SpliceIn<M>::template type, SpliceChildren>
    {
    };

    template <typename Sub, CurveExpression C, Node At>
    struct ConstantRewrite<Sub, InterpolateAlongNode<C, At>>:
        ConstantRewriteTwo<Sub, C, At, InterpolationOf, InterpolationChildren>
    {
    };

    /// An opaque output, rewritten **through its call's inputs**: an opaque
    /// operation's `compute` receives evaluated input values and never the
    /// environment (`opaque.hpp`), so its inputs are everything it reads, and
    /// a quantity fixed or defined in them is fixed or defined for the whole
    /// call. Known when every input is; the call is rebuilt around the
    /// rewritten inputs with the **same** citation.
    ///
    /// **A refused call is never rebuilt**, and is not known: rebuilt around
    /// new input types it would be a new call type, whose class body refuses
    /// it a second time -- one mistake, two messages. Not known, every result
    /// check over it stays silent; the program is ill-formed already, and
    /// they have nothing true to add. `apply` returns it as it was.
    template <typename Sub, std::size_t I, typename Op, typename... Inputs, typename Origin>
    struct ConstantRewrite<Sub, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>>
    {
        /// Whether the call was refused where it was written.
        static constexpr bool refusedCall = OpaqueCall<Op, Inputs...>::refused;
        /// Whether every input is a kind this header knows, all the way down,
        /// and the call was not refused.
        static constexpr bool known = !refusedCall && (ConstantRewriteOf<Sub, Inputs>::known && ...);
        /// Whether any input uses `Q`.
        static constexpr bool mentions = (ConstantRewriteOf<Sub, Inputs>::mentions || ...);
        /// The call, around the rewritten inputs.
        using Call = OpaqueCall<Op, typename ConstantRewriteOf<Sub, Inputs>::type...>;
        /// The same output of the rewritten call; the output itself when the
        /// call was refused.
        using type =
            std::conditional_t<refusedCall, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>, OpaqueOutputNode<I, Call, Origin>>;

        /// The output, of the call around the rewritten inputs, with its
        /// citation; the original when the call was refused.
        [[nodiscard]] static constexpr type apply(OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin> const& original,
                                                  Sub const& overriding) noexcept
        {
            if constexpr (refusedCall)
            {
                (void) overriding;
                return original;
            }
            else
                return type { {},
                              Call { [&]<std::size_t... At>(std::index_sequence<At...>) {
                                        return std::tuple<typename ConstantRewriteOf<Sub, Inputs>::type...> {
                                            ConstantRewriteOf<Sub, Inputs>::apply(std::get<At>(original.call.inputs),
                                                                                  overriding)...
                                        };
                                    }(std::index_sequence_for<Inputs...> {}),
                                     original.call.citation } };
        }
    };

    /// A rounded opaque output, rewritten as the output it rounds is -- through
    /// its call's inputs, with the call's citation, and never when the call
    /// was refused -- and rounded as before.
    template <typename Sub,
              std::size_t I,
              typename Op,
              typename... Inputs,
              Unit U,
              DecimalPlaces Places,
              RoundingMode Mode,
              typename Origin>
    struct ConstantRewrite<Sub, RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>>
    {
        /// How the output it rounds is rewritten.
        using Output = ConstantRewrite<Sub, OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>>;
        /// Whether every input is a kind this header knows, and the call was not refused.
        static constexpr bool known = Output::known;
        /// Whether any input uses `Q`.
        static constexpr bool mentions = Output::mentions;
        /// The same rounding, of the output of the rewritten call.
        using type = RoundedOpaqueOutputNode<I, decltype(Output::type::call), U, Places, Mode, Origin>;

        /// The node, over the rewritten call.
        [[nodiscard]] static constexpr type apply(
            RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin> const& original,
            Sub const& overriding) noexcept
        {
            return type { {}, Output::apply(unrounded(original), overriding).call };
        }
    };

    template <typename Sub, UnaryOperator Op, SeriesNode Operand>
    struct ConstantRewrite<Sub, ElementwiseUnaryNode<Op, Operand>>:
        ConstantRewriteOperand<Sub, Operand, ElementwiseUnaryNode<Op, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };

    template <typename Sub, CumulativeDirection D, SeriesNode S>
    struct ConstantRewrite<Sub, CumulativeNode<D, S>>:
        ConstantRewriteOperand<Sub, S, CumulativeNode<D, typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    /// A sample statistic, rebuilt around its rewritten sample.
    template <typename Sub, typename Sample, typename Rebuilt>
    struct ConstantRewriteSample
    {
        /// How the sample is rewritten.
        using Inner = ConstantRewriteOf<Sub, Sample>;

        /// Whether the sample is a kind this header knows, all the way down.
        static constexpr bool known = Inner::known;
        /// Whether the sample uses `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same statistic, around the rewritten sample.
        using type = Rebuilt;

        /// The statistic, around the rewritten sample.
        template <typename N>
        [[nodiscard]] static constexpr type apply(N const& node, Sub const& overriding) noexcept
        {
            return type { {}, Inner::apply(node.sample, overriding) };
        }
    };

    /// The pass placeholders name no quantity; carried over unchanged.
    template <typename Sub, Described Q>
    struct ConstantRewrite<Sub, PassMeanNode<Q>>: ConstantRewriteLeaf<Sub, PassMeanNode<Q>>
    {
    };

    template <typename Sub>
    struct ConstantRewrite<Sub, PassCountNode>: ConstantRewriteLeaf<Sub, PassCountNode>
    {
    };

    /// A criterion of the same kind over @p NewLimit.
    template <typename Criterion, typename NewLimit>
    struct RebindCriterion;

    template <Node Limit, typename NewLimit>
    struct RebindCriterion<DeviationFromMean<Limit>, NewLimit>
    {
        using type = DeviationFromMean<NewLimit>;
    };

    template <Node Limit, typename NewLimit>
    struct RebindCriterion<DeviationInStddevs<Limit>, NewLimit>
    {
        using type = DeviationInStddevs<NewLimit>;
    };

    template <Node Limit, typename NewLimit>
    struct RebindCriterion<GapToRange<Limit>, NewLimit>
    {
        using type = GapToRange<NewLimit>;
    };

    /// A rejection, rebuilt around its rewritten sample and limit, with its
    /// own bounds, verdict and citation: a jurisdiction's tolerance reaches
    /// the limit (`with_constant<Tolerance>`, §16.7), and a substitution for
    /// the sample's quantity is refused by the result check, as it is for any
    /// series (`RequireConstantNotSeries`'s message).
    template <typename Sub, PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    struct ConstantRewrite<Sub, RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion>>
    {
        /// How the sample is rewritten.
        using SampleRewrite = ConstantRewriteOf<Sub, S>;
        /// How the limit is rewritten.
        using LimitRewrite = ConstantRewriteOf<Sub, std::remove_cvref_t<decltype(std::declval<Criterion>().limit)>>;
        /// The criterion, over the rewritten limit.
        using RewrittenCriterion = typename RebindCriterion<Criterion, typename LimitRewrite::type>::type;

        /// Whether both are kinds this header knows, all the way down.
        static constexpr bool known = SampleRewrite::known && LimitRewrite::known;
        /// Whether either uses `Q`.
        static constexpr bool mentions = SampleRewrite::mentions || LimitRewrite::mentions;
        /// The same rejection, around the rewritten sample and limit.
        using type = RejectionNode<P, L, AtMostT, KeepAtLeastT, typename SampleRewrite::type, RewrittenCriterion>;

        /// The rejection, rebuilt.
        [[nodiscard]] static constexpr type apply(RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node,
                                                  Sub const& overriding) noexcept
        {
            return type { SampleRewrite::apply(node.sample, overriding),
                          RewrittenCriterion { LimitRewrite::apply(node.criterion.limit, overriding) },
                          node.verdict,
                          node.citation };
        }
    };

    template <typename Sub, SampleSource S>
    struct ConstantRewrite<Sub, SampleCountNode<S>>:
        ConstantRewriteSample<Sub, S, SampleCountNode<typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    template <typename Sub, SampleSource S>
    struct ConstantRewrite<Sub, SampleMeanNode<S>>:
        ConstantRewriteSample<Sub, S, SampleMeanNode<typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    template <typename Sub, SampleSource S>
    struct ConstantRewrite<Sub, SampleVarianceNode<S>>:
        ConstantRewriteSample<Sub, S, SampleVarianceNode<typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    template <typename Sub, SampleSource S>
    struct ConstantRewrite<Sub, SampleRangeNode<S>>:
        ConstantRewriteSample<Sub, S, SampleRangeNode<typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    template <typename Sub, SeriesNode S>
    struct ConstantRewrite<Sub, SumNode<S>>:
        ConstantRewriteOperand<Sub, S, SumNode<typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    template <typename Sub, Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    struct ConstantRewrite<Sub, ElementwiseRoundNode<U, Places, Mode, S>>:
        ConstantRewriteOperand<Sub, S, ElementwiseRoundNode<U, Places, Mode, typename ConstantRewriteOf<Sub, S>::type>>
    {
    };

    /// Raw observations: known, and never replaced, as a series variable is
    /// -- one constant cannot stand for every observation -- and counted as
    /// a series use of `Q`, so that a substitution for it is refused by the
    /// result check.
    template <typename Sub, Described P, std::size_t Capacity>
    struct ConstantRewrite<Sub, ObservationsVarNode<P, Capacity>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether these are observations of `Q` and @p Sub counts a series
        /// use -- see `countsSeriesUse`.
        static constexpr bool mentions = std::is_same_v<typename Sub::quantity, P> && countsSeriesUse<Sub>;
        /// Unchanged.
        using type = ObservationsVarNode<P, Capacity>;

        /// The node itself.
        [[nodiscard]] static constexpr type apply(ObservationsVarNode<P, Capacity> const& original, Sub const&) noexcept
        {
            return original;
        }
    };

    /// Observations refused already name nothing, as a refused series names
    /// nothing.
    template <typename Sub>
    struct ConstantRewrite<Sub, RefusedObservations>: ConstantRewriteLeaf<Sub, RefusedObservations>
    {
    };

    /// A binning, around its rewritten observations; its classes are in its
    /// type.
    template <typename Sub, Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
    struct ConstantRewrite<Sub, BinnedNode<KeyUnit, Classes, Obs>>
    {
        /// How the observations are rewritten.
        using Inner = ConstantRewriteOf<Sub, Obs>;

        /// Whether the observations are known.
        static constexpr bool known = Inner::known;
        /// Whether the observations use `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same binning, around the rewritten observations.
        using type = BinnedNode<KeyUnit, Classes, typename Inner::type>;

        /// The binning, around the rewritten observations.
        [[nodiscard]] static constexpr type apply(BinnedNode<KeyUnit, Classes, Obs> const& original,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, Inner::apply(original.source, overriding) };
        }
    };

    /// An elementwise operation, either side a series or a broadcast scalar:
    /// a `var<Q>` in the scalar side is replaced as it is anywhere else.
    template <typename Sub, BinaryOperator Op, typename Left, typename Right>
    struct ConstantRewrite<Sub, ElementwiseBinaryNode<Op, Left, Right>>
    {
        /// How the left side is rewritten.
        using LeftRewrite = ConstantRewriteOf<Sub, Left>;
        /// How the right side is rewritten.
        using RightRewrite = ConstantRewriteOf<Sub, Right>;

        /// Whether both sides are known all the way down.
        static constexpr bool known = LeftRewrite::known && RightRewrite::known;
        /// Whether either side uses `Q`.
        static constexpr bool mentions = LeftRewrite::mentions || RightRewrite::mentions;
        /// The same operation, over the rewritten sides.
        using type = ElementwiseBinaryNode<Op, typename LeftRewrite::type, typename RightRewrite::type>;

        /// The node, over the rewritten sides.
        [[nodiscard]] static constexpr type apply(ElementwiseBinaryNode<Op, Left, Right> const& original,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, LeftRewrite::apply(original.lhs, overriding), RightRewrite::apply(original.rhs, overriding) };
        }
    };

    template <typename Sub, BinaryOperator Op, Node Left, Node Right>
    struct ConstantRewrite<Sub, BinaryNode<Op, Left, Right>>
    {
        /// How the left side is rewritten.
        using LeftRewrite = ConstantRewriteOf<Sub, Left>;
        /// How the right side is rewritten.
        using RightRewrite = ConstantRewriteOf<Sub, Right>;

        /// Whether both sides are known all the way down.
        static constexpr bool known = LeftRewrite::known && RightRewrite::known;
        /// Whether either side uses `Q`.
        static constexpr bool mentions = LeftRewrite::mentions || RightRewrite::mentions;
        /// The same operator, over the rewritten sides.
        using type = BinaryNode<Op, typename LeftRewrite::type, typename RightRewrite::type>;

        /// The node, over the rewritten sides.
        [[nodiscard]] static constexpr type apply(BinaryNode<Op, Left, Right> const& original,
                                                  Sub const& overriding) noexcept
        {
            return type { {}, LeftRewrite::apply(original.lhs, overriding), RightRewrite::apply(original.rhs, overriding) };
        }
    };

    /// A comparison is not a `Node`, but a `when()` and a constraint hold one,
    /// and either side of it may use `Q`.
    template <typename Sub, Comparison Op, Node Left, Node Right>
    struct ConstantRewrite<Sub, PredicateNode<Op, Left, Right>>
    {
        /// How the left side is rewritten.
        using LeftRewrite = ConstantRewriteOf<Sub, Left>;
        /// How the right side is rewritten.
        using RightRewrite = ConstantRewriteOf<Sub, Right>;

        /// Whether both sides are known all the way down.
        static constexpr bool known = LeftRewrite::known && RightRewrite::known;
        /// Whether either side uses `Q`.
        static constexpr bool mentions = LeftRewrite::mentions || RightRewrite::mentions;
        /// The same comparison, over the rewritten sides.
        using type = PredicateNode<Op, typename LeftRewrite::type, typename RightRewrite::type>;

        /// The comparison, over the rewritten sides.
        [[nodiscard]] static constexpr type apply(PredicateNode<Op, Left, Right> const& original,
                                                  Sub const& overriding) noexcept
        {
            return type { LeftRewrite::apply(original.lhs, overriding), RightRewrite::apply(original.rhs, overriding) };
        }
    };

    template <typename Sub, Predicate P, Node Then, Node Else>
    struct ConstantRewrite<Sub, WhenNode<P, Then, Else>>
    {
        /// How the condition is rewritten.
        using PredicateRewrite = ConstantRewriteOf<Sub, P>;
        /// How the branch taken when it holds is rewritten.
        using ThenRewrite = ConstantRewriteOf<Sub, Then>;
        /// How the branch taken when it does not is rewritten.
        using ElseRewrite = ConstantRewriteOf<Sub, Else>;

        /// Whether all three are known all the way down.
        static constexpr bool known = PredicateRewrite::known && ThenRewrite::known && ElseRewrite::known;
        /// Whether any of the three uses `Q`.
        static constexpr bool mentions = PredicateRewrite::mentions || ThenRewrite::mentions || ElseRewrite::mentions;
        /// The same conditional, over the rewritten parts.
        using type = WhenNode<typename PredicateRewrite::type, typename ThenRewrite::type, typename ElseRewrite::type>;

        /// The conditional, over the rewritten parts.
        [[nodiscard]] static constexpr type apply(WhenNode<P, Then, Else> const& original, Sub const& overriding) noexcept
        {
            return type { {},
                          PredicateRewrite::apply(original.predicate, overriding),
                          ThenRewrite::apply(original.thenBranch, overriding),
                          ElseRewrite::apply(original.elseBranch, overriding) };
        }
    };

    /// The quantities that have a node a substitution left -- an overridden
    /// constant or a derived quantity -- anywhere in a subtree of type @p N,
    /// repeats and all. One specialisation per node kind this library ships,
    /// as `ConstantRewrite` has; the primary template, every other node kind,
    /// answers none, since nothing inside it can be seen -- and a method
    /// holding one is refused by the rewrite before this is asked.
    template <typename N>
    struct SubstitutedIn
    {
        /// Nothing that can be seen.
        using type = QuantityList<>;
    };

    /// @p N, whatever its cv qualification -- see `ConstantRewriteOf`.
    template <typename N>
    using SubstitutedInOf = typename SubstitutedIn<std::remove_cv_t<N>>::type;

    /// The quantities substituted in any of @p Children.
    template <typename... Children>
    using SubstitutedInAll = typename JoinQuantities<SubstitutedInOf<Children>...>::type;

    template <Described P>
    struct SubstitutedIn<OverriddenConstantNode<P>>
    {
        /// The quantity fixed.
        using type = QuantityList<P>;
    };

    template <Described P, Node Expr>
    struct SubstitutedIn<DerivedQuantityNode<P, Expr>>
    {
        /// The quantity defined, and whatever its definition substitutes.
        using type = typename JoinQuantities<QuantityList<P>, SubstitutedInOf<Expr>>::type;
    };

    template <Node Expr>
    struct SubstitutedIn<ReplacedVariantNode<Expr>>
    {
        /// Whatever the replacement substitutes.
        using type = SubstitutedInOf<Expr>;
    };

    /// A node whose only child is its `operand`.
    template <typename Operand>
    struct SubstitutedInOperand
    {
        /// Whatever the operand substitutes.
        using type = SubstitutedInOf<Operand>;
    };

    template <UnaryOperator Op, Node Operand>
    struct SubstitutedIn<UnaryNode<Op, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <int Exponent, Node Operand>
    struct SubstitutedIn<PowerNode<Exponent, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <int Degree, Node Operand>
    struct SubstitutedIn<RootNode<Degree, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    /// Needed for correctness, not only for completeness: the primary answers "none", so without it a
    /// substitution inside a logarithm or an exponential would be invisible to the whole-method rule.
    template <Transcendental F, Node Operand>
    struct SubstitutedIn<TranscendentalNode<F, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct SubstitutedIn<RoundNode<U, Places, Mode, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    struct SubstitutedIn<RoundSignificantNode<U, Digits, Mode, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    struct SubstitutedIn<RoundedRootNode<U, Places, Mode, Radicand>>: SubstitutedInOperand<Radicand>
    {
    };

    template <Transcendental F, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct SubstitutedIn<RoundedTranscendentalNode<F, Places, Mode, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    struct SubstitutedIn<SampleSizeLookupNode<Sizes, ResultUnit, Count>>: SubstitutedInOperand<Count>
    {
    };

    template <Node Operand>
    struct SubstitutedIn<AbsoluteValueNode<Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <PrecisionKind K, Node Level, Node Limit>
    struct SubstitutedIn<PrecisionLimitNode<K, Level, Limit>>
    {
        /// Whatever the level and the limit substitute.
        using type = SubstitutedInAll<Level, Limit>;
    };

    template <Unit U, FixedString Justification, Node Operand>
    struct SubstitutedIn<NumericValueNode<U, Justification, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    struct SubstitutedIn<SnapNode<KeyUnit, Permitted, Tie, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    struct SubstitutedIn<BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    struct SubstitutedIn<InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <Node Inner>
    struct SubstitutedIn<DocumentedNode<Inner>>: SubstitutedInOperand<Inner>
    {
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct SubstitutedIn<BinaryNode<Op, Left, Right>>
    {
        /// Whatever either side substitutes.
        using type = SubstitutedInAll<Left, Right>;
    };

    template <Comparison Op, Node Left, Node Right>
    struct SubstitutedIn<PredicateNode<Op, Left, Right>>
    {
        /// Whatever either side substitutes.
        using type = SubstitutedInAll<Left, Right>;
    };

    template <Predicate P, Node Then, Node Else>
    struct SubstitutedIn<WhenNode<P, Then, Else>>
    {
        /// Whatever the condition or either branch substitutes.
        using type = SubstitutedInAll<P, Then, Else>;
    };

    template <UnaryOperator Op, SeriesNode Operand>
    struct SubstitutedIn<ElementwiseUnaryNode<Op, Operand>>: SubstitutedInOperand<Operand>
    {
    };

    template <CumulativeDirection D, SeriesNode S>
    struct SubstitutedIn<CumulativeNode<D, S>>: SubstitutedInOperand<S>
    {
    };

    template <SeriesNode S>
    struct SubstitutedIn<SumNode<S>>: SubstitutedInOperand<S>
    {
    };

    template <SeriesNode DomainSeries, SeriesNode ValueSeries>
    struct SubstitutedIn<CurveNode<DomainSeries, ValueSeries>>
    {
        /// Whatever either series substitutes.
        using type = SubstitutedInAll<DomainSeries, ValueSeries>;
    };

    template <Monotone M, CurveExpression A, CurveExpression B>
    struct SubstitutedIn<SpliceNode<M, A, B>>
    {
        /// Whatever either curve substitutes.
        using type = SubstitutedInAll<A, B>;
    };

    template <CurveExpression C, Node At>
    struct SubstitutedIn<InterpolateAlongNode<C, At>>
    {
        /// Whatever the curve or the point substitutes.
        using type = SubstitutedInAll<C, At>;
    };

    template <SampleSource S>
    struct SubstitutedIn<SampleCountNode<S>>: SubstitutedInOperand<S>
    {
    };

    template <PerPass P, OnLimit L, typename AtMostT, typename KeepAtLeastT, typename S, typename Criterion>
    struct SubstitutedIn<RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion>>
    {
        /// Whatever the sample and the limit substitute.
        using type = SubstitutedInAll<S, std::remove_cvref_t<decltype(std::declval<Criterion>().limit)>>;
    };

    template <SampleSource S>
    struct SubstitutedIn<SampleMeanNode<S>>: SubstitutedInOperand<S>
    {
    };

    template <SampleSource S>
    struct SubstitutedIn<SampleVarianceNode<S>>: SubstitutedInOperand<S>
    {
    };

    template <SampleSource S>
    struct SubstitutedIn<SampleRangeNode<S>>: SubstitutedInOperand<S>
    {
    };

    template <Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    struct SubstitutedIn<ElementwiseRoundNode<U, Places, Mode, S>>: SubstitutedInOperand<S>
    {
    };

    template <BinaryOperator Op, typename Left, typename Right>
    struct SubstitutedIn<ElementwiseBinaryNode<Op, Left, Right>>
    {
        /// Whatever either side substitutes.
        using type = SubstitutedInAll<Left, Right>;
    };

    template <std::size_t I, typename Op, typename... Inputs, typename Origin>
    struct SubstitutedIn<OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>>
    {
        /// Whatever any of the call's inputs substitutes.
        using type = SubstitutedInAll<Inputs...>;
    };

    template <std::size_t I,
              typename Op,
              typename... Inputs,
              Unit U,
              DecimalPlaces Places,
              RoundingMode Mode,
              typename Origin>
    struct SubstitutedIn<RoundedOpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, U, Places, Mode, Origin>>
    {
        /// Whatever any of the call's inputs substitutes.
        using type = SubstitutedInAll<Inputs...>;
    };

    /// Fails to compile when `with_constant<Q>` is applied to a method that
    /// never uses `Q`. Such an override changes nothing, and the likeliest
    /// reason is that it names the wrong quantity.
    template <typename Q, bool Used>
    struct RequireConstantUsed
    {
        static_assert(Used,
                      "formula: this overlay overrides a quantity that no variant or constraint of the method "
                      "uses; an override nobody reads would silently do nothing, most likely because it names "
                      "the wrong quantity -- the quantity appears in this diagnostic as the template argument "
                      "Q of RequireConstantUsed");

        static constexpr bool value = true;
    };

    /// Fails to compile when `with_constant<Q>` is applied to a method that
    /// reads `Q` as a series (`series<Q, N>`) or as raw observations
    /// (`observations<Q, Capacity>`). A series is a value at every point of
    /// the method's domain, and observations as many values as were made; one
    /// constant cannot stand for them, and the substitution would leave them
    /// reading the environment. The words are the calculation's for the same
    /// reads (`RequireSingleValueReadsInCalculation`).
    ///
    /// Not `RequireConstantUsed`'s message: a variant or constraint does use
    /// `Q`, and a message saying none does would be false.
    template <typename Q, bool NotASeries>
    struct RequireConstantNotSeries
    {
        static_assert(NotASeries,
                      "formula: this overlay fixes a quantity the method reads as a series or as raw observations; "
                      "one constant cannot stand for many values -- the quantity appears in this diagnostic as the "
                      "template argument Q of RequireConstantNotSeries");

        static constexpr bool value = true;
    };

    /// `RequireConstantNotSeries`'s rule for `add_derived<Q>`, in words that
    /// say what the overlay did.
    template <typename Q, bool NotASeries>
    struct RequireDerivationNotSeries
    {
        static_assert(NotASeries,
                      "formula: this overlay derives a quantity the method reads as a series or as raw observations; "
                      "one definition cannot stand for many values -- the quantity appears in this diagnostic as the "
                      "template argument Q of RequireDerivationNotSeries");

        static constexpr bool value = true;
    };

    /// Fails to compile when `add_derived<Q>` is applied to a method that
    /// never uses `Q` -- `RequireConstantUsed`'s rule, in words that say what
    /// the overlay did.
    template <typename Q, bool Used>
    struct RequireDerivationUsed
    {
        static_assert(Used,
                      "formula: this overlay derives a quantity that no variant or constraint of the method "
                      "uses; a definition nobody reads would silently do nothing, most likely because it names "
                      "the wrong quantity -- the quantity appears in this diagnostic as the template argument "
                      "Q of RequireDerivationUsed");

        static constexpr bool value = true;
    };

    /// Fails to compile when the produced method reads `Q` both through a
    /// node an overlay's substitution left -- a fixed constant or a
    /// definition -- and, somewhere else, as a plain `var<Q>`, which reads the
    /// environment. One formula would then evaluate one quantity at two
    /// values, and the trace would say the jurisdiction fixed or defined a
    /// quantity the method also takes from the specimen.
    ///
    /// Three routes lead here, and the message names all three because the
    /// fix differs: an operation listed after the substitution that reads `Q`
    /// again (list the substitution after it); a later overlay that does so
    /// (substitute `Q` again in that overlay); and definitions that read each
    /// other (a cycle, which no ordering fixes).
    template <typename Q, bool Everywhere>
    struct RequireSubstitutionEverywhere
    {
        static_assert(Everywhere,
                      "formula: this method reads a quantity both where an overlay fixed or derived it and, "
                      "elsewhere, unsubstituted from the environment; one formula would evaluate one quantity at "
                      "two values -- an operation listed after the substitution, a later overlay, or definitions "
                      "that read each other put the plain use back; the quantity appears in this diagnostic as "
                      "the template argument Q of RequireSubstitutionEverywhere");

        static constexpr bool value = true;
    };

    /// Fails to compile when `with_constant<Q>` fixed `Q` where the method
    /// read it, and an operation listed after it in the same overlay removed
    /// every one of those uses and put back one that reads `Q` from the
    /// environment. The method the overlay produces then reads `Q` only
    /// unsubstituted: the constant does nothing, and the overlay claims to fix
    /// a quantity the method takes from the specimen.
    ///
    /// Not `RequireSubstitutionEverywhere`'s message, which says the method
    /// reads `Q` both where it was fixed and elsewhere: here nothing fixed is
    /// left. Nor `RequireConstantUsed`'s: the method does read `Q`.
    template <typename Q, bool Reached>
    struct RequireConstantNotBypassed
    {
        static_assert(Reached,
                      "formula: this overlay fixes a quantity, and an operation listed after the constant removed "
                      "every use it fixed and put back one that reads the quantity from the environment; the "
                      "method it produces reads the quantity only unsubstituted, so the constant does nothing -- "
                      "list the constant after that operation; the quantity appears in this diagnostic as the "
                      "template argument Q of RequireConstantNotBypassed");

        static constexpr bool value = true;
    };

    /// `RequireConstantNotBypassed`'s rule for `add_derived<Q>`, in words
    /// that say what the overlay did.
    template <typename Q, bool Reached>
    struct RequireDerivationNotBypassed
    {
        static_assert(Reached,
                      "formula: this overlay derives a quantity, and an operation listed after the definition "
                      "removed every use it replaced and put back one that reads the quantity from the environment; "
                      "the method it produces reads the quantity only unsubstituted, so the definition does "
                      "nothing -- list the definition after that operation; the quantity appears in this "
                      "diagnostic as the template argument Q of RequireDerivationNotBypassed");

        static constexpr bool value = true;
    };

    /// Fails to compile when `with_constant<Q>` met no use of `Q` where it
    /// was applied, and an operation listed after it in the same overlay put
    /// in a use that reads `Q` from the environment. The method the overlay
    /// produces then reads `Q` only unfixed: the constant does nothing, though
    /// listed after that operation it would have fixed that use.
    ///
    /// Not `RequireConstantUsed`'s message: the method does read `Q`. Nor
    /// `RequireConstantNotBypassed`'s: the constant fixed nothing for a later
    /// operation to remove.
    template <typename Q, bool Follows>
    struct RequireConstantPrecedesItsUse
    {
        static_assert(Follows,
                      "formula: this overlay fixes a quantity that nothing read where the constant is listed, and "
                      "an operation listed after it reads the quantity from the environment; the method it "
                      "produces reads the quantity only unfixed, so the constant does nothing -- list the constant "
                      "after that operation; the quantity appears in this diagnostic as the template argument Q "
                      "of RequireConstantPrecedesItsUse");

        static constexpr bool value = true;
    };

    /// `RequireConstantPrecedesItsUse`'s rule for `add_derived<Q>`, in words
    /// that say what the overlay did. The operation that reads `Q` may itself
    /// be a definition that `Q`'s definition reads; listed the other way
    /// round the two are a cycle, and the message says so.
    template <typename Q, bool Follows>
    struct RequireDerivationPrecedesItsUse
    {
        static_assert(Follows,
                      "formula: this overlay derives a quantity that nothing read where the definition is listed, "
                      "and an operation listed after it reads the quantity from the environment; the method it "
                      "produces reads the quantity only undefined, so the definition does nothing -- list the "
                      "definition after that operation (if that operation is itself a definition that reads this "
                      "quantity, the two form a cycle); the quantity appears in this diagnostic as the template "
                      "argument Q of RequireDerivationPrecedesItsUse");

        static constexpr bool value = true;
    };

    /// What a substitution @p Sub for `Q` asks of the method an overlay
    /// produces: that it is still in effect somewhere, and that it is in
    /// effect everywhere `Q` is read.
    ///
    /// Whether `Q` is used is asked only once every node is a kind the
    /// rewrite knows. An unknown node may be exactly where `Q` is used, so the
    /// honest answer there is not "unused" but "cannot tell", and the refusal
    /// that says so is `RequireOverlaySeesNode`'s, raised by the rewrite.
    ///
    /// @p Reached says whether the substitution met a use of `Q` in the method
    /// as it stood when it was applied. It does not decide whether to refuse
    /// -- that is judged of the result alone -- only which refusal is true: a
    /// substitution that met uses a later operation removed was bypassed, one
    /// that met none while a later operation put one in was listed too early,
    /// and one that met none with none put in did nothing at all.
    template <typename Sub, typename Vs, typename Constraints, bool Reached = true>
    struct RequireConstantApplies;

    template <typename Sub, typename... Tags, Node... Exprs, Predicate... Ps, bool Reached>
    struct RequireConstantApplies<Sub, Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>, Reached>
    {
        /// The quantity substituted.
        using Q = typename Sub::quantity;

        /// Whether every variant and constraint is known all the way down.
        static constexpr bool known =
            (ConstantRewriteOf<Sub, Exprs>::known && ...) && (ConstantRewriteOf<Sub, Ps>::known && ...);
        /// Whether a plain `var<Q>` is left anywhere. The substitution replaced
        /// every one it met, so a plain use left was put back after it -- by a
        /// later operation, or by a definition cycle that reaches inside `Q`'s
        /// own definition.
        static constexpr bool plainLeft = (ConstantRewriteOf<PlainUseProbe<Q>, Exprs>::mentions || ...)
                                          || (ConstantRewriteOf<PlainUseProbe<Q>, Ps>::mentions || ...);
        /// Whether a node a substitution for `Q` left is still anywhere.
        static constexpr bool used = (ConstantRewriteOf<SubstitutedUseProbe<Q>, Exprs>::mentions || ...)
                                     || (ConstantRewriteOf<SubstitutedUseProbe<Q>, Ps>::mentions || ...);
        /// Whether the method reads `Q` as a series anywhere, which no
        /// substitution replaces.
        static constexpr bool readsAsSeries = (ConstantRewriteOf<SeriesUseProbe<Q>, Exprs>::mentions || ...)
                                              || (ConstantRewriteOf<SeriesUseProbe<Q>, Ps>::mentions || ...);

        /// The refusal for a quantity read as a series, in the words of the
        /// operation that cannot stand for it.
        using SeriesRefusal = std::conditional_t<IsDerivation<Sub>::value,
                                                 RequireDerivationNotSeries<Q, !readsAsSeries>,
                                                 RequireConstantNotSeries<Q, !readsAsSeries>>;

        /// The refusal in the words of the operation that did nothing.
        using Refusal =
            std::conditional_t<IsDerivation<Sub>::value, RequireDerivationUsed<Q, used>, RequireConstantUsed<Q, used>>;

        /// The refusal for a plain use left where nothing substituted is: in
        /// the words of the operation it bypassed.
        using Bypassed = std::conditional_t<IsDerivation<Sub>::value,
                                            RequireDerivationNotBypassed<Q, !plainLeft>,
                                            RequireConstantNotBypassed<Q, !plainLeft>>;

        /// The refusal for a plain use put in after a substitution that met
        /// none: in the words of the operation listed too early.
        using TooEarly = std::conditional_t<IsDerivation<Sub>::value,
                                            RequireDerivationPrecedesItsUse<Q, !plainLeft>,
                                            RequireConstantPrecedesItsUse<Q, !plainLeft>>;

        // Each message says only what is there. A series of `Q`: the
        // substitution cannot stand for it, whatever else is true, and every
        // other message is gated off -- "nothing reads it" would be false. A
        // node the substitution left, beside a plain use: the method reads `Q`
        // both ways. No such node, but a plain use, and the substitution did
        // meet uses when it was applied: a later operation removed them, so
        // it was bypassed. No such node, a plain use, and no use met: a later
        // operation put the use in, and the substitution was listed before
        // it. No such node and no plain use: nothing reads `Q`, so the
        // substitution does nothing.
        static constexpr bool judged = known && !readsAsSeries;
        static_assert(std::conditional_t<known, SeriesRefusal, std::true_type>::value);
        static_assert(
            std::conditional_t<judged && used, RequireSubstitutionEverywhere<Q, !plainLeft>, std::true_type>::value);
        static_assert(std::conditional_t<judged && !used && Reached, Bypassed, std::true_type>::value);
        static_assert(std::conditional_t<judged && !used && !Reached, TooEarly, std::true_type>::value);
        static_assert(std::conditional_t<judged && !used && !plainLeft, Refusal, std::true_type>::value);

        static constexpr bool value = true;
    };

    /// A variant with the substitution made in its expression; the tag is
    /// unchanged.
    template <typename Sub, typename Tag, Node Expr>
    [[nodiscard]] constexpr auto rewrite_variant(VariantCase<Tag, Expr> const& original, Sub const& overriding) noexcept
    {
        using Rewrite = ConstantRewriteOf<Sub, Expr>;
        return VariantCase<Tag, typename Rewrite::type> { Rewrite::apply(original.expression, overriding) };
    }

    /// A constraint with the substitution made in its predicate; its verdict
    /// and citation are unchanged.
    template <typename Sub, Predicate P>
    [[nodiscard]] constexpr auto rewrite_constraint(Constraint<P> const& original, Sub const& overriding) noexcept
    {
        using Rewrite = ConstantRewriteOf<Sub, P>;
        return Constraint<typename Rewrite::type> { Rewrite::apply(original.predicate, overriding),
                                                    original.verdict,
                                                    original.citation };
    }

    /// A method's own constraints with the substitution made in every one.
    template <typename Sub, Predicate... Ps>
    [[nodiscard]] constexpr auto rewrite_constraints(ConstraintSet<Ps...> const& ownSet, Sub const& overriding) noexcept
    {
        return std::apply(
            [&](auto const&... predicates) { return formula::constraints(rewrite_constraint(predicates, overriding)...); },
            ownSet.items);
    }

    /// A jurisdiction's constraints with the substitution made in every one,
    /// still the jurisdiction's and still citing what its overlay cited: a
    /// later substitution changes what a constraint reads, not whose it is.
    template <typename Sub, Predicate... Ps>
    [[nodiscard]] constexpr auto rewrite_constraints(OverlaidConstraints<Ps...> const& overlaidSet,
                                                     Sub const& overriding) noexcept
    {
        return ConstraintOriginAccess::overlaid(rewrite_constraints(overlaidSet.constraintSet(), overriding),
                                                overlaidSet.source());
    }

    /// Fails to compile when `add_derived<Q>` defines `Q` by an expression of
    /// a different dimension: every use of `Q` would then evaluate to a
    /// quantity it is not.
    template <typename Q, typename Expr>
    struct RequireDerivationMeasuresQuantity
    {
        static_assert(refused_already<Expr>() || Expr::dimension == Describe<Q>::dimension,
                      "formula: this overlay derives a quantity from an expression of a different dimension; "
                      "every use of the quantity would evaluate to something it does not measure -- the "
                      "quantity and the expression appear in this diagnostic as the template arguments Q and "
                      "Expr of RequireDerivationMeasuresQuantity");

        static constexpr bool value = true;
    };

    /// Fails to compile when `add_derived<Q>` defines `Q` by an expression that
    /// reads `Q` itself. The definition replaces every use of `Q` but its own,
    /// which would go on reading the environment: one formula evaluating one
    /// quantity at two values, one of them the specimen's and one the
    /// jurisdiction's.
    template <typename Q, bool ReadsItself>
    struct RequireDerivationNotSelfReferential
    {
        static_assert(!ReadsItself,
                      "formula: this overlay derives a quantity from an expression that reads the quantity "
                      "itself; that use would keep its environment value while every other use evaluates the "
                      "definition -- the quantity appears in this diagnostic as the template argument Q of "
                      "RequireDerivationNotSelfReferential");

        static constexpr bool value = true;
    };
} // namespace detail

/// The operation `add_derived<Q>(expression, source)` builds: define `Q` by
/// `expression` wherever the method uses it.
///
/// Its class body refuses, where the overlay is written: an expression of a
/// different dimension from `Q`'s; an expression holding a node kind the
/// overlay cannot see inside, since that is where a use of `Q` could hide; and
/// -- once every node is known -- an expression that reads `Q` itself. The
/// dimension rule is not gated on the other two. An operation it refuses is
/// not applied at all: `valid` is part of what `apply` asks before
/// instantiating its body, so a refused definition is never also judged
/// against the method it would have produced. No `{}` default member
/// initialiser on the expression: see `Corrections` (`lookup.hpp`).
template <Described Q, Node Expr>
struct QuantityDerivation
{
    static_assert(detail::RequireDerivationMeasuresQuantity<Q, Expr>::value);

    /// Whether every node of the expression is a kind the overlay can see
    /// inside -- see `detail::ConstantRewrite`.
    static constexpr bool known = detail::ConstantRewriteOf<detail::QuantityProbe<Q>, Expr>::known;

    // Not asked of an expression refused already, such as one holding a
    // refused opaque call: its one message has been given.
    static_assert(std::conditional_t<known || detail::refused_already<Expr>(),
                                     std::true_type,
                                     detail::RequireDerivationSeesNode<Expr>>::value);
    static_assert(
        std::conditional_t<
            known,
            detail::RequireDerivationNotSelfReferential<Q,
                                                        detail::ConstantRewriteOf<detail::QuantityProbe<Q>, Expr>::mentions>,
            std::true_type>::value);

    /// Whether every rule above holds, asked without firing any: what
    /// `detail::IsValidOperation` reports to `apply`.
    static constexpr bool valid = Expr::dimension == Describe<Q>::dimension && known
                                  && !detail::ConstantRewriteOf<detail::QuantityProbe<Q>, Expr>::mentions;

    /// The quantity defined.
    using quantity = Q;

    /// The expression `Q` is defined by.
    Expr expression;
    /// Where the definition comes from; carried onto every node it replaces.
    Citation source {};
};

/// Defines the quantity `Q` by @p expression wherever the method the overlay is
/// applied to uses it -- in every variant and in every constraint: the
/// jurisdiction that computes what the base standard left to the specimen.
///
/// Each `var<Q>` becomes a `DerivedQuantityNode<Q, Expr>`, which keeps `Q`'s
/// identity and evaluates @p expression against the same environment, so the
/// environment need not supply `Q`. A trace records the definition as its own
/// step and `document()` marks `Q`'s row as derived: see `DerivedQuantityNode`.
///
/// @p source records where the definition comes from, and is required, as
/// `with_constant`'s is.
///
/// Refused when @p expression measures a different dimension from `Q`, when
/// it reads `Q`, and when no variant or constraint of the method the overlay
/// produces uses `Q`: see the file comment.
template <Described Q, Node Expr>
[[nodiscard]] constexpr QuantityDerivation<Q, Expr> add_derived(Expr expression, Citation source) noexcept
{
    return QuantityDerivation<Q, Expr> { expression, source };
}

/// Refuses a definition with no citation, as `with_constant`'s overload
/// refuses a constant.
template <Described Q, bool Stated = false, Node Expr>
[[nodiscard]] constexpr QuantityDerivation<Q, Expr> add_derived(Expr expression) noexcept
{
    static_assert(Stated,
                  "formula: add_derived<Q>(expression) was given no citation; a definition is a jurisdiction's "
                  "decision, and a trace must say whose -- pass the Citation of the clause that states it");
    return QuantityDerivation<Q, Expr> { expression, {} };
}

namespace detail
{
    template <Described Q, Node Expr>
    [[nodiscard]] constexpr DerivedQuantityNode<Q, Expr> substitute(QuantityDerivation<Q, Expr> const& deriving) noexcept
    {
        return OverlayNodeAccess::derived<Q>(deriving.expression, deriving.source);
    }

    /// Fails to compile when an overlay pins or prunes a tag no variant of the
    /// method declares, at the point the operation is applied -- which, since
    /// operations apply in order, may be after an earlier one removed it.
    template <typename Tag, typename... Cs>
    struct RequireOverlayNamesDeclaredVariant
    {
        static_assert((std::is_same_v<Tag, typename Cs::tag> || ...),
                      "formula: this overlay pins or prunes a variant the method does not declare; an overlay "
                      "that names a variant by mistake would silently do nothing -- the tag appears in this "
                      "diagnostic as the template argument Tag of RequireOverlayNamesDeclaredVariant");

        static constexpr bool value = true;
    };

    /// Fails to compile when pruning `Tag` would leave the method no variant
    /// at all.
    ///
    /// Asked here, before the empty pack is built, because the empty pack's
    /// own refusal -- "this method declares no variants at all" -- is false of
    /// what the author wrote: their method declares variants, and their
    /// overlay removed the last of them.
    template <typename Tag, std::size_t Remaining>
    struct RequirePruneLeavesAVariant
    {
        static_assert(Remaining != 0,
                      "formula: this overlay prunes every variant of the method; a method left with nothing to "
                      "choose between can never produce a result, so an overlay that removes its last variant "
                      "is a mistake rather than a jurisdiction -- the last variant's tag appears in this "
                      "diagnostic as the template argument Tag of RequirePruneLeavesAVariant");

        static constexpr bool value = true;
    };

    /// Whether pinning or pruning `Tag` has a variant to act on: the tag
    /// passes the tag rule, and some variant declares it. The tag rule itself
    /// is `VariantPin`'s and `VariantPrune`'s to refuse, in their class
    /// bodies; this is what keeps the declared-variant rule quiet until it
    /// has something true to say, for the reason `RequireSelectableTag` gives.
    template <typename Tag, typename... Cs>
    inline constexpr bool namesDeclaredVariant = isPlainClassTag<Tag> && (std::is_same_v<Tag, typename Cs::tag> || ...);

    /// The positions of every variant except the one tagged `Tag`, which is
    /// declared exactly once -- `namesDeclaredVariant` and the distinct-tag
    /// rule between them guarantee it.
    template <typename Tag, typename... Cs>
    [[nodiscard]] consteval std::array<std::size_t, sizeof...(Cs) - 1> positions_without() noexcept
    {
        constexpr bool matches[] = { std::is_same_v<Tag, typename Cs::tag>... };
        std::array<std::size_t, sizeof...(Cs) - 1> kept {};
        std::size_t keptCount = 0;
        for (std::size_t caseIndex = 0; caseIndex < sizeof...(Cs); ++caseIndex)
            if (!matches[caseIndex])
                kept[keptCount++] = caseIndex;
        return kept;
    }

    /// `positions_without`, held where a pack expansion can read it.
    template <typename Tag, typename... Cs>
    inline constexpr std::array<std::size_t, sizeof...(Cs) - 1> positionsWithout = positions_without<Tag, Cs...>();

    /// @p pack with the published positions and count of the pack it was
    /// made from -- see `Variants::published`, which every operation below
    /// carries through, so that a trace counts in the method as published
    /// rather than in the one an overlay produced.
    ///
    /// The layout comes from `PublishedLayoutAccess::selected`, never from positions
    /// stated here: which variants are kept is known from the tags at compile
    /// time, but where they were published is the input pack's, which may be
    /// run time data -- see `PublishedLayout`.
    template <typename... Ds>
    [[nodiscard]] constexpr Variants<Ds...> republished(Variants<Ds...> pack,
                                                        PublishedLayout<sizeof...(Ds)> const& layout) noexcept
    {
        pack.published = layout;
        return pack;
    }

    /// The variants at `positionsWithout<Tag, Cs...>`, in order, each keeping
    /// its published position.
    template <typename Tag, typename... Cs, std::size_t... Kept>
    [[nodiscard]] constexpr auto variants_without(Variants<Cs...> const& pack, std::index_sequence<Kept...>) noexcept
    {
        return republished(formula::variants(std::get<positionsWithout<Tag, Cs...>[Kept]>(pack.cases)...),
                           PublishedLayoutAccess::selected<positionsWithout<Tag, Cs...>[Kept]...>(pack.published));
    }

    /// Whether an operation substitutes for a quantity: `with_constant` or
    /// `add_derived`.
    template <typename Operation>
    struct IsSubstitution: IsDerivation<Operation>
    {
    };

    template <Described Q>
    struct IsSubstitution<ConstantOverride<Q>>: std::true_type
    {
    };

    /// `with_constant<Q>` or `add_derived<Q>`: every variant and constraint,
    /// with `Q` substituted.
    ///
    /// Whether anything reads `Q` is not asked here, against the method as it
    /// stands at this step, but once, of the method the whole overlay
    /// produces -- see `RequireOverridesRead`.
    template <typename Sub, typename... Cs, typename Rounding, typename Constraints>
        requires IsSubstitution<Sub>::value
    [[nodiscard]] constexpr auto apply_operation(Sub const& overriding,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const& constraintSet) noexcept
    {
        auto rewritten = std::apply(
            [&](auto const&... cases) { return formula::variants(rewrite_variant(cases, overriding)...); }, pack.cases);
        // Rewriting a variant's expression moves nothing, so the layout is
        // carried over as it stands, already checked.
        rewritten.published = pack.published;
        return formula::method(rewritten, rounding, rewrite_constraints(constraintSet, overriding));
    }

    /// `pin_variant<Tag>`: the variant tagged `Tag`, alone.
    ///
    /// Answers with the method unchanged when the pin has been refused, as the
    /// prune below does. `variant_index` falls back to position 0 for a tag no
    /// variant declares, and a method built from that fallback is one the
    /// author never wrote: the operations after the pin would be judged
    /// against it, and could be refused for what it lacks.
    template <typename Tag, typename... Cs, typename Rounding, typename Constraints>
    [[nodiscard]] constexpr auto apply_operation(VariantPin<Tag> const& pinning,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const& constraintSet) noexcept
    {
        static_assert(
            std::conditional_t<isPlainClassTag<Tag>, RequireOverlayNamesDeclaredVariant<Tag, Cs...>, std::true_type>::value);

        if constexpr (namesDeclaredVariant<Tag, Cs...>)
            return formula::method(
                republished(
                    formula::variants(std::get<variant_index<Tag, Cs...>()>(pack.cases)),
                    PublishedLayoutAccess::pinned(
                        PublishedLayoutAccess::selected<variant_index<Tag, Cs...>()>(pack.published), pinning.source)),
                rounding,
                constraintSet);
        else
            return formula::method(pack, rounding, constraintSet);
    }

    /// `prune_variant<Tag>`: every variant but the one tagged `Tag`.
    ///
    /// Answers with the method unchanged whenever the build has already been
    /// refused, so that a refused prune adds nothing of the compiler's own to
    /// the refusal -- in particular, never the empty pack's message.
    template <typename Tag, typename... Cs, typename Rounding, typename Constraints>
    [[nodiscard]] constexpr auto apply_operation(VariantPrune<Tag> const& pruning,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const& constraintSet) noexcept
    {
        static_assert(
            std::conditional_t<isPlainClassTag<Tag>, RequireOverlayNamesDeclaredVariant<Tag, Cs...>, std::true_type>::value);
        static_assert(std::conditional_t<namesDeclaredVariant<Tag, Cs...>,
                                         RequirePruneLeavesAVariant<Tag, sizeof...(Cs) - 1>,
                                         std::true_type>::value);

        if constexpr (namesDeclaredVariant<Tag, Cs...> && sizeof...(Cs) > 1)
        {
            auto kept = variants_without<Tag>(pack, std::make_index_sequence<sizeof...(Cs) - 1> {});
            kept.published = PublishedLayoutAccess::pruned(kept.published, pruning.source);
            return formula::method(kept, rounding, constraintSet);
        }
        else
            return formula::method(pack, rounding, constraintSet);
    }

    /// `with_rounding<U, Places, Mode>`: the same variants and constraints,
    /// rounded by the overlay's rule, which records that it is the
    /// overlay's and what the overlay cited.
    ///
    /// A rule whose unit does not measure the variants' dimension is refused
    /// here, in the words a method's own rule would be refused in -- see
    /// `RequireRoundingRuleMeasuresVariants` -- and answers with the method
    /// unchanged, as a refused pin or prune does. Built into the method, the
    /// rule would be refused again by every method a later operation of the
    /// overlay builds from it: g++ 13.3 and clang++ 20.1.8 printed one refusal
    /// per method, cl 19.51 one in all.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, typename... Cs, typename Rounding, typename Constraints>
    [[nodiscard]] constexpr auto apply_operation(RoundingOverride<U, Places, Mode> const& overriding,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const& constraintSet) noexcept
    {
        using Rule = RoundingRule<U, Places, Mode>;
        constexpr bool measures =
            !VariantsDimension<Variants<Cs...>>::known || U.dimension == VariantsDimension<Variants<Cs...>>::dimension;
        static_assert(
            std::conditional_t<measures, std::true_type, RequireRoundingRuleMeasuresVariants<Variants<Cs...>, Rule>>::value);

        if constexpr (measures)
            return formula::method(pack, RoundingRuleAccess::overlaid<Rule>(overriding.source), constraintSet);
        else
            return formula::method(pack, rounding, constraintSet);
    }

    /// `with_constraints(..., source)`: the same variants and rounding rule, checked
    /// against the overlay's constraints, which the method records as the
    /// overlay's, with what the overlay cited. The constraints replaced are
    /// dropped whole, of whatever number, and so is whose they were.
    template <Predicate... Rs, typename... Cs, typename Rounding, typename Constraints>
    [[nodiscard]] constexpr auto apply_operation(ConstraintsOverride<Rs...> const& overriding,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const&) noexcept
    {
        return formula::method(
            pack, rounding, ConstraintOriginAccess::overlaid(overriding.constraintSet, overriding.source));
    }

    /// Fails to compile when a replacement measures a different dimension
    /// from the method it is put in. A method reports one quantity, and a
    /// replacement changes one variant: it cannot change what the method
    /// reports, so a replacement that would is the author's mistake. Named
    /// here, in this library's words, before the variants pack's own
    /// agreement rule could say only that two variants disagree.
    template <typename Tag, Dimension Reported, Dimension Replacement>
    struct RequireReplacementKeepsDimension
    {
        static_assert(Reported == Replacement,
                      "formula: this overlay replaces a variant with a formula of a different dimension from the "
                      "method's; a replacement changes one variant, never what the method reports -- the tag, the "
                      "method's dimension and the replacement's appear in this diagnostic as the template "
                      "arguments Tag, Reported and Replacement of RequireReplacementKeepsDimension");

        static constexpr bool value = true;
    };

    /// The variants of @p pack with the one at @p Replaced's position replaced
    /// by @p replacement, each keeping its published position.
    template <std::size_t Replaced, typename Tag, typename Replacement, typename... Cs, std::size_t... Indices>
    [[nodiscard]] constexpr auto variants_replacing(Variants<Cs...> const& pack,
                                                    Replacement const& replacement,
                                                    std::index_sequence<Indices...>) noexcept
    {
        auto replacedPack = formula::variants([&]() {
            if constexpr (Indices == Replaced)
                return VariantCase<Tag, Replacement> { replacement };
            else
                return std::get<Indices>(pack.cases);
        }()...);
        // Replacing a formula moves no variant, so the layout is carried over
        // as it stands, already checked.
        replacedPack.published = pack.published;
        return replacedPack;
    }

    /// `replace_variant<Tag>`: the variant tagged `Tag`, with its formula
    /// wrapped as a jurisdiction's replacement.
    ///
    /// Nothing is refused here. Whether the tag is declared, and whether the
    /// replacement keeps the method's dimension, are judged against the result
    /// -- see its `RequireOperationRead` -- so that the message never depends on
    /// the order of the operations. A replacement of an absent tag or of a
    /// different dimension leaves the method unchanged, so that the variants
    /// pack's own agreement rule never sees the mismatch.
    template <typename Tag, typename Expr, typename... Cs, typename Rounding, typename Constraints>
    [[nodiscard]] constexpr auto apply_operation(VariantReplacement<Tag, Expr> const& replacing,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 Constraints const& constraintSet) noexcept
    {
        if constexpr (namesDeclaredVariant<Tag, Cs...>)
        {
            constexpr Dimension reported = VariantsDimension<Variants<Cs...>>::dimension;
            if constexpr (reported == Expr::dimension)
                return formula::method(variants_replacing<variant_index<Tag, Cs...>(), Tag>(
                                           pack,
                                           OverlayNodeAccess::replaced(replacing.expression, replacing.source),
                                           std::index_sequence_for<Cs...> {}),
                                       rounding,
                                       constraintSet);
            else
                return formula::method(pack, rounding, constraintSet);
        }
        else
            return formula::method(pack, rounding, constraintSet);
    }

    /// Whether every node of every variant and constraint of a method is a kind
    /// the rewrite knows, asked without firing anything.
    ///
    /// Asked of the method an overlay is **applied to**, as a gate on the
    /// result check below. The rewrite passes an unknown node through
    /// untouched, and a pin or a prune only ever removes nodes, so "every node
    /// of the input is known" is exactly "`RequireOverlaySeesNode` did not
    /// fire". Asking the produced method instead is not the same: a later pin
    /// or prune can remove the variant holding the unknown node, and the
    /// result would then look all-known and add "nothing reads it" to the
    /// refusal that already said the overlay cannot see where it is read.
    ///
    /// Strips `const` from the method's parts for the reason `Method` does:
    /// a `Method<decltype(pack), ...>` names `const Variants<...>`, which no
    /// specialisation below matches, and reading that as "not all known"
    /// would silently switch the result check off.
    template <typename Q, typename Vs, typename Constraints>
    struct IsKnownParts: std::false_type
    {
    };

    template <typename Q, typename... Tags, Node... Exprs, Predicate... Ps>
    struct IsKnownParts<Q, Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>>:
        std::bool_constant<(ConstantRewriteOf<Q, Exprs>::known && ...) && (ConstantRewriteOf<Q, Ps>::known && ...)>
    {
    };

    template <typename Q, typename M>
    struct IsKnownMethod: std::false_type
    {
    };

    template <typename Q, typename Vs, typename Rounding, typename Constraints>
    struct IsKnownMethod<Q, Method<Vs, Rounding, Constraints>>:
        IsKnownParts<Q, std::remove_cv_t<Vs>, PlainConstraints<std::remove_cv_t<Constraints>>>
    {
    };

    /// Whether one operation of an overlay still does something in the method
    /// @p M the overlay produced. Three can stop doing something after they
    /// have been applied -- a later pin or prune can remove every variant that
    /// reads a `with_constant`'s or an `add_derived`'s quantity, or the variant
    /// a `replace_variant` replaced -- so they are judged here, against the
    /// result; every other operation is true here, and refused where it is
    /// applied.
    ///
    /// @p Input is the method the overlay was applied to, and the question is
    /// asked only when every node of it is a kind the rewrite knows -- see
    /// `IsKnownMethod`.
    ///
    /// @p Reached is whether a substitution met a use where it was applied --
    /// see `RequireConstantApplies`; the other operations ignore it.
    template <typename M, typename Input, typename Operation, bool Reached = true>
    struct RequireOperationRead: std::true_type
    {
    };

    template <typename Vs, typename Rounding, typename Constraints, typename Input, typename Q, bool Reached>
    struct RequireOperationRead<Method<Vs, Rounding, Constraints>, Input, ConstantOverride<Q>, Reached>:
        std::bool_constant<std::conditional_t<IsKnownMethod<ConstantOverride<Q>, Input>::value,
                                              RequireConstantApplies<ConstantOverride<Q>,
                                                                     std::remove_cv_t<Vs>,
                                                                     PlainConstraints<std::remove_cv_t<Constraints>>,
                                                                     Reached>,
                                              std::true_type>::value>
    {
    };

    /// `add_derived<Q>` is judged exactly as `with_constant<Q>` is.
    template <typename Vs, typename Rounding, typename Constraints, typename Input, typename Q, typename Expr, bool Reached>
    struct RequireOperationRead<Method<Vs, Rounding, Constraints>, Input, QuantityDerivation<Q, Expr>, Reached>:
        std::bool_constant<std::conditional_t<IsKnownMethod<QuantityDerivation<Q, Expr>, Input>::value,
                                              RequireConstantApplies<QuantityDerivation<Q, Expr>,
                                                                     std::remove_cv_t<Vs>,
                                                                     PlainConstraints<std::remove_cv_t<Constraints>>,
                                                                     Reached>,
                                              std::true_type>::value>
    {
    };

    /// Whether a method's variants report @p D, and the dimension they
    /// report -- asked of a well-formed method, which is the only kind `apply`
    /// judges.
    template <typename M, Dimension D>
    struct MethodDimensionIs: std::false_type
    {
        /// Nothing to report.
        static constexpr Dimension reported = D;
    };

    template <typename Vs, typename Rounding, typename Constraints, Dimension D>
    struct MethodDimensionIs<Method<Vs, Rounding, Constraints>, D>:
        std::bool_constant<VariantsDimension<std::remove_cv_t<Vs>>::dimension == D>
    {
        /// What the method's variants report.
        static constexpr Dimension reported = VariantsDimension<std::remove_cv_t<Vs>>::dimension;
    };

    /// Whether a method's variants pack declares a variant tagged `Tag`,
    /// asked of its type alone.
    template <typename Tag, typename M>
    struct MethodDeclares: std::false_type
    {
    };

    template <typename Tag, typename... Tags, Node... Exprs, typename Rounding, typename Constraints>
    struct MethodDeclares<Tag, Method<Variants<VariantCase<Tags, Exprs>...>, Rounding, Constraints>>:
        std::bool_constant<(std::is_same_v<Tag, Tags> || ...)>
    {
    };

    template <typename Tag, typename Vs, typename Rounding, typename Constraints>
    struct MethodDeclares<Tag, Method<Vs const, Rounding, Constraints>>:
        MethodDeclares<Tag, Method<Vs, Rounding, Constraints>>
    {
    };

    /// Fails to compile when `replace_variant<Tag>` names a tag the method the
    /// overlay was applied to does not declare.
    template <typename Tag, bool Declared>
    struct RequireReplacementNamesDeclaredVariant
    {
        static_assert(Declared,
                      "formula: this overlay replaces a variant the method does not declare; a replacement that "
                      "names a variant by mistake would silently do nothing -- the tag appears in this diagnostic "
                      "as the template argument Tag of RequireReplacementNamesDeclaredVariant");

        static constexpr bool value = true;
    };

    /// Fails to compile when the method an overlay produces does not hold the
    /// variant it replaced -- pruned, or pinned away, before or after.
    template <typename Tag, bool Held>
    struct RequireReplacementHeld
    {
        static_assert(Held,
                      "formula: this overlay replaces a variant that the method it produces does not hold, "
                      "because another operation of the overlay pins or prunes it away; the replacement would "
                      "silently do nothing -- the tag appears in this diagnostic as the template argument Tag "
                      "of RequireReplacementHeld");

        static constexpr bool value = true;
    };

    /// `replace_variant<Tag>` is judged against the result, as `with_constant`
    /// is, and in two steps so that one mistake gets one message: a tag the
    /// method the overlay was applied to never declared is a mistaken name;
    /// only a tag it did declare can then be one the produced method does not
    /// hold. A tag that is not a plain class type is `VariantReplacement`'s
    /// to refuse, and neither is asked of it.
    ///
    /// Judged here rather than where the replacement is applied, because the
    /// two orders -- replace then prune, prune then replace -- are the same
    /// mistake and must get the same message.
    template <typename Vs,
              typename Rounding,
              typename Constraints,
              typename Input,
              typename Tag,
              typename Expr,
              bool Reached>
    struct RequireOperationRead<Method<Vs, Rounding, Constraints>, Input, VariantReplacement<Tag, Expr>, Reached>
    {
        /// Whether the method the overlay was applied to declares the tag.
        static constexpr bool declared = isPlainClassTag<Tag> && MethodDeclares<Tag, Input>::value;
        /// Whether the replacement measures what that method reports.
        static constexpr bool keepsDimension = MethodDimensionIs<Input, Expr::dimension>::value;

        static_assert(std::conditional_t<isPlainClassTag<Tag>,
                                         RequireReplacementNamesDeclaredVariant<Tag, declared>,
                                         std::true_type>::value);
        static_assert(
            std::conditional_t<
                declared && !refused_already<Expr>(),
                RequireReplacementKeepsDimension<Tag, MethodDimensionIs<Input, Expr::dimension>::reported, Expr::dimension>,
                std::true_type>::value);
        static_assert(
            std::conditional_t<declared && keepsDimension,
                               RequireReplacementHeld<Tag, MethodDeclares<Tag, Method<Vs, Rounding, Constraints>>::value>,
                               std::true_type>::value);

        static constexpr bool value = true;
    };

    /// Whether every `replace_variant` of an overlay was applied -- its tag
    /// declared, its dimension the method's, and its variant still held by
    /// the method produced -- asked without firing anything.
    ///
    /// A replacement refused for any of those leaves the method without it,
    /// so a substitution whose quantity only the replacement reads would find
    /// nothing reading it, and add "no variant uses it" to the refusal that
    /// says what is wrong with the replacement. The substitution checks wait
    /// on this, as the operations after a refused pin wait on the pin.
    template <typename M, typename Input, typename Operation>
    struct ReplacementApplied: std::true_type
    {
    };

    template <typename M, typename Input, typename Tag, typename Expr>
    struct ReplacementApplied<M, Input, VariantReplacement<Tag, Expr>>:
        std::bool_constant<isPlainClassTag<Tag> && MethodDeclares<Tag, Input>::value
                           && MethodDimensionIs<Input, Expr::dimension>::value && MethodDeclares<Tag, M>::value>
    {
    };

    /// The method an overlay's first @p I operations produce from @p M: the
    /// method as it stands when the operation at position @p I is applied.
    /// The same `apply_operation` calls `apply_from` makes, named by type.
    template <std::size_t I, typename M, typename... Ops>
    struct MethodBefore
    {
        /// The method before the operation at position `I - 1`.
        using Previous = typename MethodBefore<I - 1, M, Ops...>::type;
        /// That method, with the operation at position `I - 1` applied.
        using type = std::remove_cvref_t<decltype(apply_operation(
            std::declval<std::tuple_element_t<I - 1, std::tuple<Ops...>> const&>(),
            std::declval<Previous const&>().variantSet,
            std::declval<Previous const&>().rounding,
            std::declval<Previous const&>().constraintSet))>;
    };

    template <typename M, typename... Ops>
    struct MethodBefore<0, M, Ops...>
    {
        /// The method the overlay was applied to.
        using type = M;
    };

    /// Whether `Q` is used anywhere in a method's parts, plainly or through a
    /// node a substitution left, definitions included. True for anything
    /// that is not a variants pack and a constraint set, where it is not
    /// asked.
    template <typename Q, typename Vs, typename Constraints>
    struct MentionedInParts: std::true_type
    {
    };

    template <typename Q, typename... Tags, Node... Exprs, Predicate... Ps>
    struct MentionedInParts<Q, Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>>:
        std::bool_constant<(ConstantRewriteOf<QuantityProbe<Q>, Exprs>::mentions || ...)
                           || (ConstantRewriteOf<QuantityProbe<Q>, Ps>::mentions || ...)>
    {
    };

    template <typename Q, typename M>
    struct MentionedIn: std::true_type
    {
    };

    template <typename Q, typename Vs, typename Rounding, typename Constraints>
    struct MentionedIn<Q, Method<Vs, Rounding, Constraints>>:
        MentionedInParts<Q, std::remove_cv_t<Vs>, PlainConstraints<std::remove_cv_t<Constraints>>>
    {
    };

    /// Whether the operation at position @p I, if it substitutes for a
    /// quantity, met a use of it in the method as it stood when it was
    /// applied. True for every other operation, which never asks.
    template <std::size_t I, typename M, typename... Ops>
    struct ReachedAt: std::true_type
    {
    };

    template <std::size_t I, typename M, typename... Ops>
        requires IsSubstitution<std::tuple_element_t<I, std::tuple<Ops...>>>::value
    struct ReachedAt<I, M, Ops...>:
        MentionedIn<typename std::tuple_element_t<I, std::tuple<Ops...>>::quantity,
                    typename MethodBefore<I, M, Ops...>::type>
    {
    };

    /// Fails to compile when some `with_constant<Q>` of an overlay fixes a
    /// quantity nothing in the method it produced reads.
    ///
    /// Asked of the **result**, never of the method as it stands when the
    /// constant is applied. The rule is that an override doing nothing in the
    /// method the overlay produces is refused, and asked per step it would
    /// depend on order: `overlay(with_constant<Q>(v, source), pin_variant<Cube>(source))`
    /// and `overlay(pin_variant<Cube>(source), with_constant<Q>(v, source))` produce the same
    /// method, and a per-step check refused only the second.
    template <typename M, typename Input, typename... Ops>
    struct RequireOverridesRead
    {
        /// Whether every replacement was applied -- see `ReplacementApplied`.
        static constexpr bool replacementsApplied = (ReplacementApplied<M, Input, Ops>::value && ...);

        /// The operation at position @p I.
        template <std::size_t I>
        using Operation = std::tuple_element_t<I, std::tuple<Ops...>>;

        /// A substitution's check waits on every replacement having been
        /// applied; every other operation's check is asked regardless.
        template <std::size_t I>
        using Check = std::conditional_t<IsSubstitution<Operation<I>>::value && !replacementsApplied,
                                         std::true_type,
                                         RequireOperationRead<M, Input, Operation<I>, ReachedAt<I, Input, Ops...>::value>>;

        /// Every operation's check, by position.
        template <std::size_t... Is>
        [[nodiscard]] static constexpr bool all(std::index_sequence<Is...>) noexcept
        {
            return (Check<Is>::value && ...);
        }

        static constexpr bool value = all(std::index_sequence_for<Ops...> {});
    };

    /// Whether a plain `var<Q>` is left anywhere in a method's parts.
    template <typename Q, typename Vs, typename Constraints>
    struct PlainUseLeft: std::false_type
    {
    };

    template <typename Q, typename... Tags, Node... Exprs, Predicate... Ps>
    struct PlainUseLeft<Q, Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>>:
        std::bool_constant<(ConstantRewriteOf<PlainUseProbe<Q>, Exprs>::mentions || ...)
                           || (ConstantRewriteOf<PlainUseProbe<Q>, Ps>::mentions || ...)>
    {
    };

    /// The rule `RequireSubstitutionEverywhere` states, judged against the
    /// WHOLE produced method: for every quantity with a substitution's node
    /// anywhere in it, whichever overlay left that node, no plain use may be
    /// left. So a later overlay that reads a quantity an earlier one fixed,
    /// where a node of that substitution survives, or that completes a
    /// definition cycle, is refused, as one overlay would be.
    ///
    /// **Not every case one overlay refuses.** A later overlay whose
    /// `replace_variant` or `with_constraints` removes every node an earlier
    /// overlay's substitution left, and puts back a plain use, leaves this
    /// rule nothing to find: the later overlay's formula or constraints hold,
    /// and the earlier constant or definition does not apply. That is
    /// "across overlays, the later one holds", accepted rather than refused;
    /// within one overlay the same order is refused as bypassed
    /// (`RequireConstantNotBypassed`).
    ///
    /// A quantity this overlay itself substitutes is asked here as well as by
    /// its own substitution's check (`RequireConstantApplies`). Both name the
    /// same specialisation of `RequireSubstitutionEverywhere`, and a class
    /// template specialisation is instantiated once, so the refusal is
    /// reported once -- counted on cl, g++ and clang++ for
    /// `overlay_derived_cycle`. The substitution's own check is still needed:
    /// when a later operation removed every node the substitution left and put
    /// a plain use back, the quantity has no substitution's node for this rule
    /// to find, and that check refuses it as bypassed -- or, when it met no
    /// use where it was listed, as listed before the one put in.
    template <typename Substituted, typename Vs, typename Constraints>
    struct RequireEverySubstitutionEverywhere;

    template <typename... Qs, typename Vs, typename Constraints>
    struct RequireEverySubstitutionEverywhere<QuantityList<Qs...>, Vs, Constraints>
    {
        static constexpr bool value =
            (RequireSubstitutionEverywhere<Qs, !PlainUseLeft<Qs, Vs, Constraints>::value>::value && ...);
    };

    /// The quantities substituted anywhere in a method's parts.
    template <typename Vs, typename Constraints>
    struct SubstitutedInParts
    {
        /// Nothing, for anything but a variants pack and a constraint set.
        using type = QuantityList<>;
    };

    template <typename... Tags, Node... Exprs, Predicate... Ps>
    struct SubstitutedInParts<Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>>
    {
        /// Every quantity with a substitution's node in a variant or a
        /// constraint.
        using type = SubstitutedInAll<Exprs..., Ps...>;
    };

    /// The whole-method rule, asked of the produced method @p M, once the
    /// checks it depends on have something true to say: every replacement
    /// applied, and every node of the method it was applied to, and of the
    /// method produced, a kind the rewrite knows.
    template <typename M, typename Input, typename... Ops>
    struct RequireSubstitutionsHold: std::true_type
    {
    };

    template <typename Vs, typename Rounding, typename Constraints, typename Input, typename... Ops>
    struct RequireSubstitutionsHold<Method<Vs, Rounding, Constraints>, Input, Ops...>
    {
        /// The produced method, as `IsKnownMethod` names it.
        using Produced = Method<Vs, Rounding, Constraints>;
        /// Whether both methods can be seen inside, all the way down.
        static constexpr bool known = IsKnownMethod<KnownProbe, Input>::value && IsKnownMethod<KnownProbe, Produced>::value;
        /// Whether every replacement was applied.
        static constexpr bool replacementsApplied = RequireOverridesRead<Produced, Input, Ops...>::replacementsApplied;

        /// Whether the rule has anything true to say.
        static constexpr bool askable = known && replacementsApplied;

        static constexpr bool value = std::conditional_t<
            askable,
            RequireEverySubstitutionEverywhere<
                typename SubstitutedInParts<std::remove_cv_t<Vs>, PlainConstraints<std::remove_cv_t<Constraints>>>::type,
                std::remove_cv_t<Vs>,
                PlainConstraints<std::remove_cv_t<Constraints>>>,
            std::true_type>::value;
    };

    /// Applies the operations of an overlay from position @p Index onwards,
    /// each to the method the one before it produced.
    template <std::size_t Index, typename... Ops, typename M>
    [[nodiscard]] constexpr auto apply_from(std::tuple<Ops...> const& operations, M const& m) noexcept
    {
        if constexpr (Index == sizeof...(Ops))
            return m;
        else
            return apply_from<Index + 1>(
                operations, apply_operation(std::get<Index>(operations), m.variantSet, m.rounding, m.constraintSet));
    }
} // namespace detail

/// Applies @p o to @p m, and returns the method that results: a new type,
/// built at compile time -- see the file comment.
///
/// @p m itself is unchanged, so one base method can carry every jurisdiction's
/// overlay side by side, and which of the resulting methods applies to a
/// sample is a runtime choice among values that already exist.
///
/// A malformed overlay or method is refused where it is declared, and this
/// body is then not instantiated at all, so that applying one adds nothing to
/// that refusal -- the gate `evaluate_method` has, for the reason it gives.
template <typename... Ops, typename Vs, typename Rounding, typename Constraints>
[[nodiscard]] constexpr auto apply(Overlay<Ops...> const& o, Method<Vs, Rounding, Constraints> const& m) noexcept
{
    if constexpr (!detail::isWellFormedOverlay<Ops...>
                  || !detail::IsWellFormedMethod<Method<Vs, Rounding, Constraints>>::value)
    {
        // Unreachable: the declaration of the overlay or of the method has
        // already failed to compile.
        static_cast<void>(o);
        return m;
    }
    else
    {
        auto const overlaidMethod = detail::apply_from<0>(o.operations, m);
        static_assert(detail::RequireOverridesRead<std::remove_cv_t<decltype(overlaidMethod)>,
                                                   Method<Vs, Rounding, Constraints>,
                                                   Ops...>::value);
        static_assert(detail::RequireSubstitutionsHold<std::remove_cv_t<decltype(overlaidMethod)>,
                                                       Method<Vs, Rounding, Constraints>,
                                                       Ops...>::value);
        return overlaidMethod;
    }
}

} // namespace formula
