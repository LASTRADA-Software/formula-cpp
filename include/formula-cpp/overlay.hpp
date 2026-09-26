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
/// type. That is ruling V3 of the phase's design decisions, settled by a spike
/// that built both shapes: the compile-time overlay met every one of section
/// 16.7's demands and cost nothing at the call site, where a runtime overlay
/// could not replace a formula without type erasure. The set of jurisdictions
/// is closed and lives in the type; which one applies is a runtime choice made
/// among methods that already exist.
///
/// This header provides four operations:
///
///  - `with_constant<Q>(value)` fixes the quantity `Q` to `value` wherever the
///    method uses it -- the national body fixing a constant the base standard
///    left open;
///  - `pin_variant<Tag>()` keeps only the variant tagged `Tag`, making it
///    mandatory;
///  - `prune_variant<Tag>()` deletes the variant tagged `Tag` outright;
///  - `with_rounding<U, Places, Mode>()` replaces the method's rounding rule,
///    and a trace then says the rule applied was the overlay's.
///
/// **An override that would silently do nothing is refused.** An overlay is
/// written once per jurisdiction and read by nobody until an inspector asks why
/// a number came out as it did; an operation that names the wrong quantity or
/// the wrong variant changes nothing, produces no error, and yields a method
/// that is the base method under a jurisdiction's name. So each of these is a
/// build error, in words of this library's own:
///
///  - `with_constant<Q>` for a `Q` no variant or constraint of the method the
///    overlay **produces** uses -- judged of the result, so that the answer
///    never depends on the order the operations are listed in;
///  - `pin_variant` or `prune_variant` of a tag no variant declares;
///  - pruning every variant -- refused here, before the empty pack would be,
///    because the empty pack's own message says the author declared no
///    variants, which is false of the author's method;
///  - one overlay listing the same operation twice, which leaves the first
///    silently overridden by the second -- and two `with_rounding` of any
///    granularities are the same operation, since a method has one rule;
///  - one overlay that both pins and prunes: a pin already states the whole
///    selection, so a prune beside it either does nothing or contradicts it;
///  - `with_constant` over an expression holding a node kind this header cannot
///    see inside, where a use of `Q` would silently keep reading the
///    environment.
///
/// Pinning a method's only variant is deliberately **not** refused, although
/// it changes nothing. The refusals above exist to catch a *mistaken name*,
/// and a pin that names the one variant there is names it correctly: the
/// method it yields is exactly what the overlay declares, a method whose only
/// variant is that one. A `with_rounding` of the granularity the method
/// already has is accepted for the same reason, and does change something:
/// the rule is then the jurisdiction's, and the trace says so.
///
/// A `with_rounding` whose unit does not measure what the method reports is
/// refused by the method it produces, in `Method`'s own words: the result of
/// `apply` is a `Method`, and holds the rule to the same dimension check the
/// method's own rule was held to.
///
/// Operations apply **in the order the overlay lists them**, each to the method
/// the previous one produced. "Nothing" in the rule above is nothing in the
/// method the overlay produces, not in the method as it stood when one
/// operation was applied. So `overlay(pin_variant<Cube>(), with_constant<Q>(v))`
/// and `overlay(with_constant<Q>(v), prune_variant<Cylinder>())` are both
/// refused when only the variant that goes away reads `Q`: in either order,
/// the method produced never reads it.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

/// A quantity whose value an overlay has fixed: what `with_constant<Q>(value)`
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
/// `k_s = 97/100 [fixed by jurisdiction overlay: ...]`. An ordinary variable
/// step would be true of the value and false of where it came from: it reads
/// as a number the specimen supplied. `document()` marks it the same way, as
/// a symbol row fixed at `value` (`SymbolEntry::fixedValue`).
template <Described Q>
struct OverriddenConstantNode: VarNode<Q>
{
    /// The value the overlay fixed, in `Q`'s declared unit.
    Rational value {};
    /// Where the overlay's value comes from, as the overlay's author cited it;
    /// empty when they cited nothing.
    Citation source {};
};

/// An overridden constant evaluates to the overlay's value, converted from
/// `Q`'s declared unit to the coherent SI unit like any other leaf, and never
/// consults @p environment -- see `OverriddenConstantNode` for why. The sink
/// is told about the node as its own type, so that a trace can say the value
/// was fixed by an overlay rather than read from the specimen.
///
/// Chosen over the `VarNode<Q>` overload in `evaluate.hpp` for every
/// `OverriddenConstantNode<Q>`, because binding the node to its own type is an
/// identity conversion and binding it to its base is not.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(OverriddenConstantNode<Q> const& node,
                                                           Env const&,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const result = detail::in_si<Rep>(node.value, Describe<Q>::unit);
    sink.produced(node, result);
    return result;
}

/// The operation `with_constant<Q>(value)` builds: fix `Q` to `value`.
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
/// constant means: the value is no longer the specimen's to state.
///
/// @p source records where the value comes from -- a national annex, say --
/// and travels with every node the override leaves behind.
///
/// Refused when no variant or constraint uses `Q`: see the file comment.
template <Described Q>
[[nodiscard]] constexpr ConstantOverride<Q> with_constant(Rational value, Citation source = {}) noexcept
{
    return ConstantOverride<Q> { value, source };
}

/// The operation `pin_variant<Tag>()` builds: keep only the variant tagged
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
};

/// Keeps only the variant tagged `Tag`, making it the one a jurisdiction
/// requires. Selecting any other tag from the overlaid method is then a build
/// error, exactly as selecting a tag the method never declared is.
///
/// Refused when no variant declares `Tag`: see the file comment.
template <typename Tag>
[[nodiscard]] constexpr VariantPin<Tag> pin_variant() noexcept
{
    return {};
}

/// The operation `prune_variant<Tag>()` builds: delete the variant tagged
/// `Tag`. `Tag` obeys the tag rule, as `VariantPin` says.
template <typename Tag>
struct VariantPrune
{
    static_assert(detail::RequirePlainClassTag<Tag>::value);

    /// The variant that is deleted.
    using tag = Tag;
};

/// Deletes the variant tagged `Tag`; the others keep their declaration order.
///
/// Refused when no variant declares `Tag`, and when it is the last variant
/// left: see the file comment.
template <typename Tag>
[[nodiscard]] constexpr VariantPrune<Tag> prune_variant() noexcept
{
    return {};
}

/// The operation `with_rounding<U, Places, Mode>()` builds: replace the
/// method's rounding rule.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
struct RoundingOverride
{
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
/// @p source cites where the rule comes from, as `with_constant`'s does;
/// "jurisdiction overlay" alone does not say which jurisdiction.
///
/// Refused when `U` does not measure what the method reports, by the method
/// the overlay produces, and when one overlay lists it twice: see the file
/// comment. Across overlays, the later one's rule holds.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr RoundingOverride<U, Places, Mode> with_rounding(Citation source = {}) noexcept
{
    return RoundingOverride<U, Places, Mode> { source };
}

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

    /// Fails to compile when something that is not an overlay operation was
    /// handed to `overlay(...)`. Templated on the position for the reason
    /// `RequireVariant` is.
    template <std::size_t Index, typename Operation>
    struct RequireOverlayOperation
    {
        static_assert(IsOverlayOperation<Operation>::value,
                      "formula: this argument of overlay(...) is not an overlay operation; every argument "
                      "must be what with_constant<Q>(value), pin_variant<Tag>(), prune_variant<Tag>() or "
                      "with_rounding<U, Places, Mode>() returns -- the offending argument appears in this "
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
    /// Every operation is its own type, except `with_rounding`: a method has
    /// one rounding rule, so `with_rounding<Megapascal, 1>` and
    /// `with_rounding<Megapascal, 2>` in one overlay are two different types
    /// of which the second silently replaces the first.
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
                      "formula: this overlay lists the same operation twice; two overrides of one quantity, "
                      "two pins or prunes of one variant, or two rounding overrides, leave the first silently "
                      "doing nothing -- the first of the two appears in this diagnostic as the template "
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

    /// Whether an overlay's list passes every rule `RequireWellFormedOverlay`
    /// asks, asked without firing any of them.
    template <typename... Ops>
    inline constexpr bool isWellFormedOverlay =
        (IsOverlayOperation<Ops>::value && ...) && all_distinct<typename OperationIdentity<Ops>::type...>()
        && !pinsAndPrunes<Ops...>;
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

/// Builds an overlay: `overlay(with_constant<Q>(v), prune_variant<Cube>())`.
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
    /// A dependent `false`, so that a refusal inside a template fires only
    /// when that template is instantiated.
    template <typename>
    inline constexpr bool alwaysFalse = false;

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

    /// How `with_constant<Q>` rewrites a node of type @p N, one specialisation
    /// per node kind this library ships.
    ///
    /// Each answers three things together, so that no two of them can drift:
    ///
    ///  - `known`: whether every node in the subtree is a kind this header
    ///    can see inside;
    ///  - `mentions`: whether the subtree uses `Q` -- as `var<Q>`, or as an
    ///    `OverriddenConstantNode<Q>` an earlier overlay left;
    ///  - `type` and `apply`: the rewritten subtree, with every such use now
    ///    an `OverriddenConstantNode<Q>` holding the new value, and every
    ///    other node, runtime contents included, carried over unchanged.
    ///
    /// The primary template is every other node kind. It answers `known =
    /// false`, and its `apply` is refused -- see `RequireOverlaySeesNode`.
    template <typename Q, typename N>
    struct ConstantRewrite
    {
        /// A node kind this header does not know.
        static constexpr bool known = false;
        /// Unknowable; `known` is what the caller asks first.
        static constexpr bool mentions = false;
        /// Unchanged, because it cannot be looked into.
        using type = N;

        /// Refused; see `RequireOverlaySeesNode`.
        [[nodiscard]] static constexpr type apply(N const& node, ConstantOverride<Q> const&) noexcept
        {
            static_assert(RequireOverlaySeesNode<N>::value);
            return node;
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
    template <typename Q, typename N>
    using ConstantRewriteOf = ConstantRewrite<Q, std::remove_cv_t<N>>;

    /// A variable: replaced when it names `Q`, and left alone otherwise.
    template <typename Q, Described P>
    struct ConstantRewrite<Q, VarNode<P>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether this is `var<Q>`.
        static constexpr bool mentions = std::is_same_v<Q, P>;
        /// The overridden constant when this is `var<Q>`, and this variable
        /// otherwise.
        using type = std::conditional_t<mentions, OverriddenConstantNode<Q>, VarNode<P>>;

        /// The rewritten node.
        [[nodiscard]] static constexpr type apply([[maybe_unused]] VarNode<P> const& node,
                                                  [[maybe_unused]] ConstantOverride<Q> const& overriding) noexcept
        {
            if constexpr (mentions)
                return type { {}, overriding.value, overriding.source };
            else
                return node;
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
    template <typename Q, Described P>
    struct ConstantRewrite<Q, OverriddenConstantNode<P>>
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// Whether this is `Q`'s.
        static constexpr bool mentions = std::is_same_v<Q, P>;
        /// Unchanged: an overridden constant stays one.
        using type = OverriddenConstantNode<P>;

        /// The node, with the new value and source when it is `Q`'s.
        [[nodiscard]] static constexpr type apply(OverriddenConstantNode<P> const& node,
                                                  [[maybe_unused]] ConstantOverride<Q> const& overriding) noexcept
        {
            if constexpr (mentions)
                return type { {}, overriding.value, overriding.source };
            else
                return node;
        }
    };

    /// A leaf that names no quantity: carried over unchanged.
    template <typename Q, typename N>
    struct ConstantRewriteLeaf
    {
        /// A kind this header knows.
        static constexpr bool known = true;
        /// A leaf naming no quantity never uses `Q`.
        static constexpr bool mentions = false;
        /// Unchanged.
        using type = N;

        /// The node itself.
        [[nodiscard]] static constexpr type apply(N const& node, ConstantOverride<Q> const&) noexcept
        {
            return node;
        }
    };

    template <typename Q, Unit U>
    struct ConstantRewrite<Q, ConstantNode<U>>: ConstantRewriteLeaf<Q, ConstantNode<U>>
    {
    };

    template <typename Q>
    struct ConstantRewrite<Q, PiNode>: ConstantRewriteLeaf<Q, PiNode>
    {
    };

    /// An exact lookup has no operand: its key is data, not an expression.
    template <typename Q, KeyTable Keys, Unit ResultUnit>
    struct ConstantRewrite<Q, ExactLookupNode<Keys, ResultUnit>>: ConstantRewriteLeaf<Q, ExactLookupNode<Keys, ResultUnit>>
    {
    };

    /// A node whose only child is its `operand`, and whose only runtime state
    /// that child is, rebuilt as @p Rebuilt around the rewritten operand.
    template <typename Q, typename Operand, typename Rebuilt>
    struct ConstantRewriteOperand
    {
        /// How the operand is rewritten.
        using Inner = ConstantRewriteOf<Q, Operand>;

        /// Whether the operand is a kind this header knows, all the way down.
        static constexpr bool known = Inner::known;
        /// Whether the operand uses `Q`.
        static constexpr bool mentions = Inner::mentions;
        /// The same node kind, around the rewritten operand.
        using type = Rebuilt;

        /// The node, around the rewritten operand.
        template <typename N>
        [[nodiscard]] static constexpr type apply(N const& node, ConstantOverride<Q> const& overriding) noexcept
        {
            return type { {}, Inner::apply(node.operand, overriding) };
        }
    };

    template <typename Q, UnaryOperator Op, Node Operand>
    struct ConstantRewrite<Q, UnaryNode<Op, Operand>>:
        ConstantRewriteOperand<Q, Operand, UnaryNode<Op, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, int Exponent, Node Operand>
    struct ConstantRewrite<Q, PowerNode<Exponent, Operand>>:
        ConstantRewriteOperand<Q, Operand, PowerNode<Exponent, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, int Degree, Node Operand>
    struct ConstantRewrite<Q, RootNode<Degree, Operand>>:
        ConstantRewriteOperand<Q, Operand, RootNode<Degree, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Q, RoundNode<U, Places, Mode, Operand>>:
        ConstantRewriteOperand<Q, Operand, RoundNode<U, Places, Mode, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    struct ConstantRewrite<Q, RoundSignificantNode<U, Digits, Mode, Operand>>:
        ConstantRewriteOperand<Q,
                               Operand,
                               RoundSignificantNode<U, Digits, Mode, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, Unit U, FixedString Justification, Node Operand>
    struct ConstantRewrite<Q, NumericValueNode<U, Justification, Operand>>:
        ConstantRewriteOperand<Q, Operand, NumericValueNode<U, Justification, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    /// A lookup whose key is an expression: its operand is rewritten, and its
    /// table's contents -- runtime state, like a `ConstantNode`'s number --
    /// are carried over.
    template <typename Q, typename Operand, typename Rebuilt>
    struct ConstantRewriteLookup: ConstantRewriteOperand<Q, Operand, Rebuilt>
    {
        /// The lookup, around the rewritten operand, with its contents.
        template <typename N>
        [[nodiscard]] static constexpr Rebuilt apply(N const& node, ConstantOverride<Q> const& overriding) noexcept
        {
            return Rebuilt { {}, node.corrections, ConstantRewriteOf<Q, Operand>::apply(node.operand, overriding) };
        }
    };

    template <typename Q, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    struct ConstantRewrite<Q, BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand>>:
        ConstantRewriteLookup<Q,
                              Operand,
                              BandedLookupNode<KeyUnit, Bands, ResultUnit, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    template <typename Q, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    struct ConstantRewrite<Q, InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand>>:
        ConstantRewriteLookup<
            Q,
            Operand,
            InterpolatingLookupNode<KeyUnit, Points, ResultUnit, typename ConstantRewriteOf<Q, Operand>::type>>
    {
    };

    /// A citation's wrapper: the wrapped formula is rewritten and the citation
    /// carried over.
    template <typename Q, Node Inner>
    struct ConstantRewrite<Q, DocumentedNode<Inner>>
    {
        /// How the wrapped formula is rewritten.
        using Wrapped = ConstantRewriteOf<Q, Inner>;

        /// Whether the wrapped formula is known all the way down.
        static constexpr bool known = Wrapped::known;
        /// Whether the wrapped formula uses `Q`.
        static constexpr bool mentions = Wrapped::mentions;
        /// The wrapper, around the rewritten formula.
        using type = DocumentedNode<typename Wrapped::type>;

        /// The wrapper, around the rewritten formula, with its citation.
        [[nodiscard]] static constexpr type apply(DocumentedNode<Inner> const& node,
                                                  ConstantOverride<Q> const& overriding) noexcept
        {
            return type { {}, Wrapped::apply(node.inner, overriding), node.citation };
        }
    };

    template <typename Q, BinaryOperator Op, Node Left, Node Right>
    struct ConstantRewrite<Q, BinaryNode<Op, Left, Right>>
    {
        /// How the left side is rewritten.
        using LeftRewrite = ConstantRewriteOf<Q, Left>;
        /// How the right side is rewritten.
        using RightRewrite = ConstantRewriteOf<Q, Right>;

        /// Whether both sides are known all the way down.
        static constexpr bool known = LeftRewrite::known && RightRewrite::known;
        /// Whether either side uses `Q`.
        static constexpr bool mentions = LeftRewrite::mentions || RightRewrite::mentions;
        /// The same operator, over the rewritten sides.
        using type = BinaryNode<Op, typename LeftRewrite::type, typename RightRewrite::type>;

        /// The node, over the rewritten sides.
        [[nodiscard]] static constexpr type apply(BinaryNode<Op, Left, Right> const& node,
                                                  ConstantOverride<Q> const& overriding) noexcept
        {
            return type { {}, LeftRewrite::apply(node.lhs, overriding), RightRewrite::apply(node.rhs, overriding) };
        }
    };

    /// A comparison is not a `Node`, but a `when()` and a constraint hold one,
    /// and either side of it may use `Q`.
    template <typename Q, Comparison Op, Node Left, Node Right>
    struct ConstantRewrite<Q, PredicateNode<Op, Left, Right>>
    {
        /// How the left side is rewritten.
        using LeftRewrite = ConstantRewriteOf<Q, Left>;
        /// How the right side is rewritten.
        using RightRewrite = ConstantRewriteOf<Q, Right>;

        /// Whether both sides are known all the way down.
        static constexpr bool known = LeftRewrite::known && RightRewrite::known;
        /// Whether either side uses `Q`.
        static constexpr bool mentions = LeftRewrite::mentions || RightRewrite::mentions;
        /// The same comparison, over the rewritten sides.
        using type = PredicateNode<Op, typename LeftRewrite::type, typename RightRewrite::type>;

        /// The comparison, over the rewritten sides.
        [[nodiscard]] static constexpr type apply(PredicateNode<Op, Left, Right> const& node,
                                                  ConstantOverride<Q> const& overriding) noexcept
        {
            return type { LeftRewrite::apply(node.lhs, overriding), RightRewrite::apply(node.rhs, overriding) };
        }
    };

    template <typename Q, Predicate P, Node Then, Node Else>
    struct ConstantRewrite<Q, WhenNode<P, Then, Else>>
    {
        /// How the condition is rewritten.
        using PredicateRewrite = ConstantRewriteOf<Q, P>;
        /// How the branch taken when it holds is rewritten.
        using ThenRewrite = ConstantRewriteOf<Q, Then>;
        /// How the branch taken when it does not is rewritten.
        using ElseRewrite = ConstantRewriteOf<Q, Else>;

        /// Whether all three are known all the way down.
        static constexpr bool known = PredicateRewrite::known && ThenRewrite::known && ElseRewrite::known;
        /// Whether any of the three uses `Q`.
        static constexpr bool mentions = PredicateRewrite::mentions || ThenRewrite::mentions || ElseRewrite::mentions;
        /// The same conditional, over the rewritten parts.
        using type = WhenNode<typename PredicateRewrite::type, typename ThenRewrite::type, typename ElseRewrite::type>;

        /// The conditional, over the rewritten parts.
        [[nodiscard]] static constexpr type apply(WhenNode<P, Then, Else> const& node,
                                                  ConstantOverride<Q> const& overriding) noexcept
        {
            return type { {},
                          PredicateRewrite::apply(node.predicate, overriding),
                          ThenRewrite::apply(node.thenBranch, overriding),
                          ElseRewrite::apply(node.elseBranch, overriding) };
        }
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

    /// What `with_constant<Q>` asks of the method an overlay produces.
    ///
    /// Whether `Q` is used is asked only once every node is a kind the
    /// rewrite knows. An unknown node may be exactly where `Q` is used, so the
    /// honest answer there is not "unused" but "cannot tell", and the refusal
    /// that says so is `RequireOverlaySeesNode`'s, raised by the rewrite.
    template <typename Q, typename Vs, typename Constraints>
    struct RequireConstantApplies;

    template <typename Q, typename... Tags, Node... Exprs, Predicate... Ps>
    struct RequireConstantApplies<Q, Variants<VariantCase<Tags, Exprs>...>, ConstraintSet<Ps...>>
    {
        /// Whether every variant and constraint is known all the way down.
        static constexpr bool known =
            (ConstantRewriteOf<Q, Exprs>::known && ...) && (ConstantRewriteOf<Q, Ps>::known && ...);
        /// Whether any variant or constraint uses `Q`.
        static constexpr bool used =
            (ConstantRewriteOf<Q, Exprs>::mentions || ...) || (ConstantRewriteOf<Q, Ps>::mentions || ...);

        static_assert(std::conditional_t<known, RequireConstantUsed<Q, used>, std::true_type>::value);

        static constexpr bool value = true;
    };

    /// A variant with `Q` fixed in its expression; the tag is unchanged.
    template <typename Q, typename Tag, Node Expr>
    [[nodiscard]] constexpr auto rewrite_variant(VariantCase<Tag, Expr> const& original,
                                                 ConstantOverride<Q> const& overriding) noexcept
    {
        using Rewrite = ConstantRewriteOf<Q, Expr>;
        return VariantCase<Tag, typename Rewrite::type> { Rewrite::apply(original.expression, overriding) };
    }

    /// A constraint with `Q` fixed in its predicate; its verdict and citation
    /// are unchanged.
    template <typename Q, Predicate P>
    [[nodiscard]] constexpr auto rewrite_constraint(Constraint<P> const& original,
                                                    ConstantOverride<Q> const& overriding) noexcept
    {
        using Rewrite = ConstantRewriteOf<Q, P>;
        return Constraint<typename Rewrite::type> { Rewrite::apply(original.predicate, overriding),
                                                    original.verdict,
                                                    original.citation };
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
        std::size_t count = 0;
        for (std::size_t index = 0; index < sizeof...(Cs); ++index)
            if (!matches[index])
                kept[count++] = index;
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
    /// The layout comes from `PublishedLayout::select`, never from positions
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
                           pack.published.template select<positionsWithout<Tag, Cs...>[Kept]...>());
    }

    /// `with_constant<Q>`: every variant and constraint, with `Q` fixed.
    ///
    /// Whether anything reads `Q` is not asked here, against the method as it
    /// stands at this step, but once, of the method the whole overlay
    /// produces -- see `RequireOverridesRead`.
    template <typename Q, typename... Cs, typename Rounding, Predicate... Ps>
    [[nodiscard]] constexpr auto apply_operation(ConstantOverride<Q> const& overriding,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 ConstraintSet<Ps...> const& constraintSet) noexcept
    {
        auto rewritten = std::apply(
            [&](auto const&... cases) { return formula::variants(rewrite_variant(cases, overriding)...); }, pack.cases);
        // Rewriting a variant's expression moves nothing, so the layout is
        // carried over as it stands, already checked.
        rewritten.published = pack.published;
        return formula::method(
            rewritten,
            rounding,
            std::apply([&](auto const&... items) { return formula::constraints(rewrite_constraint(items, overriding)...); },
                       constraintSet.items));
    }

    /// `pin_variant<Tag>`: the variant tagged `Tag`, alone.
    ///
    /// Answers with the method unchanged when the pin has been refused, as the
    /// prune below does. `variant_index` falls back to position 0 for a tag no
    /// variant declares, and a method built from that fallback is one the
    /// author never wrote: the operations after the pin would be judged
    /// against it, and could be refused for what it lacks.
    template <typename Tag, typename... Cs, typename Rounding, Predicate... Ps>
    [[nodiscard]] constexpr auto apply_operation(VariantPin<Tag> const&,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 ConstraintSet<Ps...> const& constraintSet) noexcept
    {
        static_assert(
            std::conditional_t<isPlainClassTag<Tag>, RequireOverlayNamesDeclaredVariant<Tag, Cs...>, std::true_type>::value);

        if constexpr (namesDeclaredVariant<Tag, Cs...>)
            return formula::method(republished(formula::variants(std::get<variant_index<Tag, Cs...>()>(pack.cases)),
                                               pack.published.template select<variant_index<Tag, Cs...>()>()),
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
    template <typename Tag, typename... Cs, typename Rounding, Predicate... Ps>
    [[nodiscard]] constexpr auto apply_operation(VariantPrune<Tag> const&,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const& rounding,
                                                 ConstraintSet<Ps...> const& constraintSet) noexcept
    {
        static_assert(
            std::conditional_t<isPlainClassTag<Tag>, RequireOverlayNamesDeclaredVariant<Tag, Cs...>, std::true_type>::value);
        static_assert(std::conditional_t<namesDeclaredVariant<Tag, Cs...>,
                                         RequirePruneLeavesAVariant<Tag, sizeof...(Cs) - 1>,
                                         std::true_type>::value);

        if constexpr (namesDeclaredVariant<Tag, Cs...> && sizeof...(Cs) > 1)
            return formula::method(
                variants_without<Tag>(pack, std::make_index_sequence<sizeof...(Cs) - 1> {}), rounding, constraintSet);
        else
            return formula::method(pack, rounding, constraintSet);
    }

    /// `with_rounding<U, Places, Mode>`: the same variants and constraints,
    /// rounded by the overlay's rule, which records that it is the
    /// overlay's and what the overlay cited.
    ///
    /// A rule whose unit does not measure the variants' dimension is refused
    /// by the `Method` this builds, in the words its own rule would be
    /// refused in -- see `RequireRoundingRuleMeasuresVariants`.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, typename... Cs, typename Rounding, Predicate... Ps>
    [[nodiscard]] constexpr auto apply_operation(RoundingOverride<U, Places, Mode> const& overriding,
                                                 Variants<Cs...> const& pack,
                                                 Rounding const&,
                                                 ConstraintSet<Ps...> const& constraintSet) noexcept
    {
        return formula::method(pack,
                               RoundingRuleAccess::overlaid<RoundingRule<U, Places, Mode>>(overriding.source),
                               constraintSet);
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
        IsKnownParts<Q, std::remove_cv_t<Vs>, std::remove_cv_t<Constraints>>
    {
    };

    /// Whether one operation of an overlay still does something in the method
    /// @p M the overlay produced. Only `with_constant` can stop doing
    /// something after it has been applied -- a later pin or prune can remove
    /// every variant that reads its quantity -- so every other operation is
    /// true here; theirs are refused where they are applied.
    ///
    /// @p Input is the method the overlay was applied to, and the question is
    /// asked only when every node of it is a kind the rewrite knows -- see
    /// `IsKnownMethod`.
    template <typename M, typename Input, typename Operation>
    struct RequireOperationRead: std::true_type
    {
    };

    template <typename Vs, typename Rounding, typename Constraints, typename Input, typename Q>
    struct RequireOperationRead<Method<Vs, Rounding, Constraints>, Input, ConstantOverride<Q>>:
        std::bool_constant<std::conditional_t<IsKnownMethod<Q, Input>::value,
                                              RequireConstantApplies<Q, std::remove_cv_t<Vs>, std::remove_cv_t<Constraints>>,
                                              std::true_type>::value>
    {
    };

    /// Fails to compile when some `with_constant<Q>` of an overlay fixes a
    /// quantity nothing in the method it produced reads.
    ///
    /// Asked of the **result**, never of the method as it stands when the
    /// constant is applied. The rule is that an override doing nothing in the
    /// method the overlay produces is refused, and asked per step it would
    /// depend on order: `overlay(with_constant<Q>(v), pin_variant<Cube>())`
    /// and `overlay(pin_variant<Cube>(), with_constant<Q>(v))` produce the same
    /// method, and a per-step check refused only the second.
    template <typename M, typename Input, typename... Ops>
    struct RequireOverridesRead
    {
        static constexpr bool value = (RequireOperationRead<M, Input, Ops>::value && ...);
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
        auto const result = detail::apply_from<0>(o.operations, m);
        static_assert(detail::RequireOverridesRead<std::remove_cv_t<decltype(result)>,
                                                   Method<Vs, Rounding, Constraints>,
                                                   Ops...>::value);
        return result;
    }
}

} // namespace formula
