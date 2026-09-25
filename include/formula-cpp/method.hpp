// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A method's variants: `variants(variant<Cube>(expr), variant<Cylinder>(expr))`.
///
/// A method reports one quantity by more than one formula, and which formula
/// applies is a property of the specimen, the apparatus or the product --
/// never of a number in the environment. Spec section 9.1 says why that
/// choice has to belong to the library: written as ordinary C++ -- an `if` on
/// specimen shape -- it is control flow the library never sees, so it cannot
/// reach the trace, and an inspector asking *"why the cylinder formula?"*
/// gets no answer. Since the audit trail is the product, the decision has to
/// be **declared**, not merely executed.
///
/// This header declares the variants and the tags they apply to, bundles them
/// with a rounding rule and a constraint set into a `Method`, and selects a
/// variant by tag in `evaluate_method`. What records that selection as a
/// trace step is built on top of these.
///
/// **Why `variant<Tag>` and not the spec's own `when<Tag>`.** Spec section
/// 9.1 sketches the selector as `formula::when<Cube>(expr)`. This library
/// spells it `variant<Cube>(expr)`, and the departure is measured rather than
/// preferred. `conditional.hpp` already ships `when(predicate, thenBranch,
/// elseBranch)` -- a runtime-predicate ternary, a different question with a
/// different arity. Both were declared in namespace `formula` and called, on
/// cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and g++-14 14.2.0:
///
///  - `when<P>(pred, then, else)` -- **one** explicit template argument,
///    **three** call arguments -- binds `P` to the shipped ternary's first
///    template parameter and compiles **SILENTLY** on all five. An author who
///    believes they are naming a tag there gets the ternary, and is saved
///    only by the accident that their tag is not a `Predicate`.
///  - Every mis-spelling produces the same uninformative diagnostic on all
///    five -- `error C2672: 'formula::when': no matching overloaded function
///    found` from cl, `error: no matching function for call to 'when'` from
///    clang and g++ -- naming neither of the two concepts, because both are
///    in the overload set and both failed.
///
/// One qualifier, recorded so that nobody re-opens this on it: the spec's own
/// one-argument spelling, `when<Cube>(expr)`, does resolve to the tag
/// overload, because `Cube` is not a `Predicate` and the ternary is removed
/// by substitution failure. What does not resolve is the neighbouring
/// three-argument spelling above, and no diagnostic anywhere separates the
/// two concepts. So the departure rests on readability plus a neighbouring
/// silent mis-binding -- not on the spec's own spelling being broken.
///
/// **What a pack refuses, and why here rather than later.** Four rules, the
/// first three enforced by `Variants` itself and the fourth by each
/// `VariantCase` it holds:
///
///  1. every argument is a variant -- a `VariantCase`, which is what
///     `variant<Tag>(...)` returns;
///  2. there is at least one of them;
///  3. they all report the same dimension;
///  4. every variant's tag is a plain class type.
///
/// The first two exist because `variants(...)` over a bare pack accepted
/// nonsense in silence. Measured on cl 19.51 at `/W4 /WX`, **exit 0, no
/// diagnostics**: `variants(var<EdgeX>, var<EdgeX>)` -- a pack with no tags
/// anywhere -- `variants(var<Force>)`, and `variants()`. The first slipped
/// through rule 3 because a `VarNode` happens to publish a `dimension`; the
/// second never reached it, a one-element pack having no pair; the third is
/// vacuous. All three are refused now, at the earliest point where the
/// mistake is still the author's own call rather than something several
/// layers away.
///
/// **Variants agree in the quantity they report, and at this layer that means
/// the dimension.** Variants are heterogeneous by design -- the spec's own
/// pair are different *types* -- so the thing they must share is what they
/// report, not how they are shaped. The declared unit is the other half of
/// that agreement, and it is deliberately not checked here: no expression
/// node publishes a unit. `ConstantNode` publishes the unit its own
/// coefficient is stated in, which is not the unit of any expression
/// containing it, and no other node publishes one at all. The declared unit
/// is a property of the quantity a method *reports*, and a method reporting
/// one `Measured<Q>` has exactly one of them by construction -- `measured.hpp`
/// says a `Measured<Q>` is in `Q`'s declared unit and carries none of its
/// own. So the unit half is enforced where the reported quantity is named,
/// and the dimension half is enforced here, where the variants are.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

namespace detail
{
    /// Whether @p Tag passes `RequirePlainClassTag`, asked without firing it.
    ///
    /// The ONLY statement of the rule: `RequirePlainClassTag` asserts this
    /// rather than restating it, so that the gate in `RequireSelectableTag`
    /// cannot drift from the guard it gates. With two copies, a guard
    /// tightened in one place would leave the gate open for the newly refused
    /// tag, and the no-match message would fire on top of it.
    template <typename Tag>
    inline constexpr bool isPlainClassTag = std::is_class_v<Tag> && std::is_same_v<Tag, std::remove_cv_t<Tag>>;

    /// Fails to compile when a tag is not a class type free of `const` and
    /// `volatile` -- whether a variant declares it or `evaluate_method` is
    /// asked to select by it, which is why its message says "this tag" and
    /// not "the tag of this variant": a selection tag belongs to no variant.
    ///
    /// `void`, `int` and `Cube&` name nothing a specimen can be, so a variant
    /// tagged with one can never be selected -- the same author's mistake an
    /// empty pack is. `const Cube` is subtler and is the reason for the
    /// `remove_cv_t` half: it is still a class type, but it is a different
    /// type from `Cube`, so two variants tagged `Cube` and `const Cube` would
    /// escape any duplicate-tag check that compares tags with `is_same_v`.
    ///
    /// Deliberately not `std::is_empty_v`: that would demand a complete type,
    /// and an incomplete `struct Cube;` is a legitimate tag -- it is never
    /// instantiated, which is the point of a tag.
    template <typename Tag>
    struct RequirePlainClassTag
    {
        static_assert(isPlainClassTag<Tag>,
                      "formula: this tag is not a plain class type; a tag names what a variant "
                      "applies to, so it must be a class type without const or volatile "
                      "-- not void, a fundamental type or a reference, and not const Cube where "
                      "Cube is meant -- and the offending tag appears in this diagnostic as the "
                      "template argument of RequirePlainClassTag");

        static constexpr bool value = true;
    };
} // namespace detail

/// One variant of a method: the expression that applies when the specimen,
/// apparatus or product matches `Tag`.
///
/// `Tag` is an ordinary class type, never cv-qualified and never
/// instantiated -- what a variant applies to is a *type*, so that selecting
/// one is a compile-time fact the type system can state rather than a string
/// nobody checks. It need not be complete. `detail::RequirePlainClassTag`
/// enforces this, in this class body rather than only in `variant()` below,
/// for the reason `Variants` gives for its own checks: this is a public
/// aggregate, so a `VariantCase<...>` can be declared with no factory call.
///
/// Deliberately not a `Node`. A variant does not stand where a number stands;
/// it names one of the formulas a method chooses between. Phase 10 settled
/// that test for lookups -- a lookup *is* a node because it produces a
/// quantity -- and it comes out the other way here.
template <typename Tag, Node Expr>
struct VariantCase
{
    static_assert(detail::RequirePlainClassTag<Tag>::value);

    /// What this variant applies to: the discriminator `variant<Tag>` was
    /// spelled with.
    using tag = Tag;

    /// The expression evaluated when this variant is the one selected.
    Expr expression {};

    /// The dimension this variant reports. Every variant of one method must
    /// publish the same one -- see `detail::RequireVariantsAgree`.
    static constexpr Dimension dimension = Expr::dimension;
};

/// The spelling of one variant in a method: `variant<Cube>(expr)`. See the
/// file comment for why this is not spelled `when<Cube>`.
template <typename Tag, Node Expr>
[[nodiscard]] constexpr VariantCase<Tag, Expr> variant(Expr expression) noexcept
{
    return VariantCase<Tag, Expr> { expression };
}

namespace detail
{
    /// Whether a type is a `VariantCase` -- whatever produced it, whether
    /// `variant<Tag>(...)` or a directly initialised `VariantCase<Tag, Expr>`.
    ///
    /// A trait rather than a concept, because the refusal below wants to name
    /// the offending type in a message of ours. A concept on `variants()`
    /// would instead give "no matching function for call to 'variants'",
    /// which is the diagnostic this header already refuses once, in the
    /// `when` discussion above.
    template <typename T>
    struct IsVariantCase: std::false_type
    {
    };

    template <typename Tag, Node Expr>
    struct IsVariantCase<VariantCase<Tag, Expr>>: std::true_type
    {
    };

    /// Fails to compile when something that is not a variant was handed to
    /// `variants(...)`.
    ///
    /// Templated on the argument's position as well as its type. The type
    /// alone is an expression node spelled out in full, which an author has to
    /// match against every argument by eye; the position says which one it is
    /// outright.
    template <std::size_t Index, typename Argument>
    struct RequireVariant
    {
        static_assert(IsVariantCase<Argument>::value,
                      "formula: this argument of variants(...) is not a variant of a method; every "
                      "argument must be a VariantCase, which variant<Tag>(expression) is the ordinary "
                      "way to write -- the offending argument appears in this diagnostic as the "
                      "template argument Argument of RequireVariant, and Index is its ZERO-BASED "
                      "position, so 0 is the first argument");

        static constexpr bool value = true;
    };

    /// True when every argument is a variant; pairs each with its position so
    /// that `RequireVariant` can report it.
    template <typename Indices, typename... Cs>
    struct RequireEveryArgumentIsAVariant;

    template <std::size_t... Indices, typename... Cs>
    struct RequireEveryArgumentIsAVariant<std::index_sequence<Indices...>, Cs...>
    {
        /// True when every one of `Cs` is a variant.
        static constexpr bool value = (RequireVariant<Indices, Cs>::value && ...);
    };

    /// Fails to compile for `variants()` -- a method with nothing to choose
    /// between.
    ///
    /// Not the same refusal as "no variant matched this specimen", which is a
    /// property of one evaluation and belongs in an outcome. A pack with no
    /// variants can never match anything, for any tag, ever: that is the
    /// author's mistake, so it is refused where the author wrote it.
    template <typename... Cs>
    struct RequireAtLeastOneVariant
    {
        static_assert(sizeof...(Cs) != 0,
                      "formula: this method declares no variants at all; a method with nothing to "
                      "choose between can never produce a result for any specimen, so an empty "
                      "variants() is the author's mistake rather than an outcome to report");

        static constexpr bool value = true;
    };

    /// Fails to compile when two variants of one method measure different
    /// dimensions.
    ///
    /// A named template, and templated on the two offending *variants* rather
    /// than on the pack that holds them, for the same diagnostic reason as
    /// `RequireAddendsAgree` in `expression.hpp` -- see that one for the
    /// measurement behind it. Taking `VariantCase`s rather than their
    /// expressions is what puts the tags in the message: an author who wrote
    /// three variants is told which two disagree by the names they chose,
    /// not by two expression types they have to decode back into tags.
    template <typename First, typename Other>
    struct RequireVariantsAgree
    {
        static_assert(First::dimension == Other::dimension,
                      "formula: two variants of this method measure different dimensions; every "
                      "variant must report the same quantity, because a method reports one -- the "
                      "two offending variants appear in this diagnostic as the template arguments "
                      "of RequireVariantsAgree");

        static constexpr bool value = true;
    };

    /// True when every variant in a pack agrees with the first one.
    ///
    /// The primary template is the empty pack, where the statement is
    /// vacuously true: no variants, no pair to disagree.
    template <typename... Cs>
    struct RequireAllVariantsAgree
    {
        /// Always true; the refusal is `RequireVariantsAgree`'s, not this
        /// template's, so that one message answers for one rule.
        static constexpr bool value = true;
    };

    /// Every variant is compared against the FIRST one, not against its
    /// neighbour. Both are correct for a rule this transitive, and the first
    /// is the better diagnostic: a pack of five whose third variant is wrong
    /// reports one disagreement rather than two, and reports it against the
    /// variant the author most likely considers the reference.
    ///
    /// The fold does not short-circuit the way its `&&` suggests. Naming
    /// `RequireVariantsAgree<First, Rest>::value` requires that class to be
    /// complete, so every one of them is instantiated when the expression is
    /// formed rather than only up to the first failure. Measured on a pack
    /// whose second AND third variants disagree, in two different ways:
    /// clang-cl 22.1.3, clang++ 20.1.8 and g++ 13.3 each report the refusal
    /// twice, naming both offenders. cl 19.51 reports it once; fixing that
    /// offender and rebuilding then reports the other, so a cl user is told
    /// about both, one build at a time. Why cl collapses them was not
    /// established, only that it does.
    template <typename First, typename... Rest>
    struct RequireAllVariantsAgree<First, Rest...>
    {
        /// True when `First` agrees with every one of `Rest`.
        static constexpr bool value = (RequireVariantsAgree<First, Rest>::value && ...);
    };

    /// Three of the four rules a variants pack obeys, asked in an order that
    /// matters. The fourth, the tag rule, is each `VariantCase`'s own.
    ///
    /// One `static_assert` in `Variants` rather than three, so that the rules
    /// can be sequenced. The agreement rule is asked **only once every
    /// argument is a variant**: a non-variant has no `dimension` to compare,
    /// and asking anyway buries the one message that matters. Measured on cl
    /// 19.51 before this gate existed, `variants(42, 43)` reported six errors
    /// -- `C2825`, `C2510` and `C2065`, once for `First` and once for `Other`
    /// -- every one of them the compiler's own wording for "that has no such
    /// member", and not one of them ours. `method_variants_agreement_gated.cpp`
    /// pins the gate, by refusing any output that names `RequireVariantsAgree`.
    ///
    /// `std::conditional_t` and not `if constexpr`, because the gated line is
    /// a `static_assert` declaration in a class body, where no statement --
    /// `if constexpr` included -- can appear. Naming
    /// `RequireAllVariantsAgree<Cs...>` as a template argument does not
    /// instantiate it, and only the selected branch's `value` does -- **but
    /// only while `RequireAllVariantsAgree`'s template parameters stay
    /// unconstrained.** A constraint is checked when the template-id is
    /// formed, not when it is used, so a constrained pack there
    /// (`template <VariantLike... Cs>`) makes the unselected branch a hard
    /// error and silently ends the laziness this gate relies on.
    ///
    /// **Adding a rule here.** Any new rule that reads a member of `Cs` --
    /// `Cs::tag`, `Cs::dimension`, anything a `VariantCase` publishes and a
    /// non-variant does not -- must sit behind the same
    /// `everyArgumentIsAVariant` gate, AND its template name must be added to
    /// the `REJECT` list of `method_variants_agreement_gated` in
    /// `test/CMakeLists.txt`. Miss the gate and
    /// `variants(variant<Cube>(...), 42)` grows a fresh cascade of the
    /// compiler's own errors under the new rule's name; miss the `REJECT` and
    /// nothing notices, because that case rejects only the names it lists.
    /// That is a likelier mistake than the constraint trap just above.
    template <typename... Cs>
    struct RequireWellFormedVariants
    {
        static_assert(RequireAtLeastOneVariant<Cs...>::value);
        static_assert(RequireEveryArgumentIsAVariant<std::index_sequence_for<Cs...>, Cs...>::value);

        /// Whether the agreement rule has anything it can ask about.
        static constexpr bool everyArgumentIsAVariant = (IsVariantCase<Cs>::value && ...);

        static_assert(std::conditional_t<everyArgumentIsAVariant, RequireAllVariantsAgree<Cs...>, std::true_type>::value);

        static constexpr bool value = true;
    };
} // namespace detail

/// The variants of one method, in declaration order:
/// `variants(variant<Cube>(...), variant<Cylinder>(...))`.
///
/// Bundles variants the way `ConstraintSet` in `constraint.hpp` bundles
/// constraints and `environment()` bundles entries -- a factory taking a pack
/// by value, returning a class template over that pack -- so a reader who
/// already knows either of those recognises this immediately. A plain
/// aggregate for the same reason `ConstraintSet` is one: there is no
/// invariant here that an encapsulated class would protect, only an ordered
/// bundle to be walked.
///
/// A `std::tuple` and not a `std::array`, because the variants of one method
/// are genuinely different types -- that is the whole point, and an array
/// could not hold the spec's own pair.
///
/// The checks sit in this class body rather than in `variants()` below,
/// deliberately: this is a public aggregate with a public member, so a
/// `Variants<...>` can be declared directly with no factory call anywhere,
/// and a check placed only in the factory would let that route through. The
/// same mistake was found, and fixed, in the lookup tables of phase 10.
template <typename... Cs>
struct Variants
{
    static_assert(detail::RequireWellFormedVariants<Cs...>::value);

    /// The variants, in the order `variants(...)` was called with them. That
    /// order is part of the contract: selection reports which index fired,
    /// and a reader matches it back to the declaration by counting.
    std::tuple<Cs...> cases {};
};

/// Builds a method's variants pack: `variants(a, b, c)`. See the file comment
/// for the four rules a pack has to satisfy.
template <typename... Cs>
[[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept
{
    return Variants<Cs...> { std::tuple<Cs...> { cases... } };
}

/// A method's rounding rule, declared rather than applied after the fact, so
/// the trace can say which rule fired and where it came from (spec section
/// 9.1). Carries no operand: it is applied to whichever variant is selected.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
struct RoundingRule
{
    /// The unit the rounding happens in -- see `rounding_node.hpp` for why a
    /// rounding that does not name one means nothing.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;
};

/// The spelling of a method's rounding rule:
/// `rounding_rule<unit::Megapascal, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>()`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr RoundingRule<U, Places, Mode> rounding_rule() noexcept
{
    return {};
}

namespace detail
{
    /// Whether a type is a `RoundingRule`.
    template <typename T>
    struct IsRoundingRule: std::false_type
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode>
    struct IsRoundingRule<RoundingRule<U, Places, Mode>>: std::true_type
    {
    };

    /// The dimension a variants pack reports, asked without firing any rule.
    ///
    /// `known` is true only for a non-empty `Variants` of `VariantCase`s that
    /// all report the dimension of the first. Every other pack is one
    /// `Variants` refuses itself, and a rule comparing something against the
    /// pack's dimension has nothing true to say about it: there is no such
    /// dimension, and the refusal that matters is already the pack's.
    template <typename Vs>
    struct VariantsDimension
    {
        /// There is no one dimension to report.
        static constexpr bool known = false;
    };

    template <typename Tag, Node Expr, typename... Tags, Node... Exprs>
    struct VariantsDimension<Variants<VariantCase<Tag, Expr>, VariantCase<Tags, Exprs>...>>
    {
        /// Whether every variant agrees with the first.
        static constexpr bool known = ((Exprs::dimension == Expr::dimension) && ...);
        /// The dimension of the first variant, which is every variant's when
        /// `known` holds.
        static constexpr Dimension dimension = Expr::dimension;
    };

    /// Fails to compile when a method's rounding rule rounds in a unit that
    /// does not measure the dimension its variants report.
    ///
    /// A `Megapascal` rule on a method whose variants measure a length used to
    /// be accepted by `method(...)` and refused only inside `evaluate_method`,
    /// by the rounding node it builds -- so a method nobody evaluated in a test
    /// would ship broken. Templated on the variants pack and the rule, the two
    /// places the two dimensions come from, so that both appear in the
    /// diagnostic.
    template <typename Vs, typename Rounding>
    struct RequireRoundingRuleMeasuresVariants
    {
        static_assert(Rounding::unit.dimension == VariantsDimension<Vs>::dimension,
                      "formula: this method's rounding rule rounds in a unit that does not measure the "
                      "dimension its variants report; the rule rounds whichever variant is selected, so "
                      "its unit must measure what every variant measures -- the variants and the "
                      "rounding rule appear in this diagnostic as the template arguments Vs and Rounding "
                      "of RequireRoundingRuleMeasuresVariants");

        static constexpr bool value = true;
    };

    /// Whether `RequireRoundingRuleMeasuresVariants` has anything true to ask.
    ///
    /// Gated for the reason `RequireWellFormedVariants` gates its agreement
    /// rule: a pack of variants that disagree, or that is not a pack of
    /// variants at all, is already refused by `Variants`, and asking this rule
    /// as well would add a second error -- measured against whichever variant
    /// happened to be first -- to the one that names the mistake.
    /// `method_rounding_rule_gated.cpp` pins that by refusing any output that
    /// names this rule. A `Rounding` that is not a `RoundingRule` is not
    /// refused here either; it has no `unit` to compare.
    template <typename Vs, typename Rounding>
    inline constexpr bool canAskRoundingRule = IsRoundingRule<Rounding>::value && VariantsDimension<Vs>::known;
} // namespace detail

/// One method: the variants it chooses between, the rounding rule it applies
/// to whichever one is chosen, and the constraints it checks.
///
/// `constraintSet` is the `ConstraintSet` exactly as `constraints(...)` built
/// it, held as an ordinary member and never unpacked, so that it can be
/// handed straight to `check_all()`; `ConstraintSet` in `constraint.hpp` says
/// why that is the shape.
///
/// The rounding rule's unit must measure the variants' dimension. That is
/// checked in this class body rather than only in `method()` below, for the
/// reason `Variants` gives for its own checks: this is a public aggregate, so
/// a `Method<...>` can be declared with no factory call.
template <typename Vs, typename Rounding, typename Constraints>
struct Method
{
    // `remove_cv_t` because `Method<decltype(pack), ...>` over a `constexpr`
    // pack names a `const Variants<...>`, which no specialisation of
    // `VariantsDimension` matches -- and an unmatched pack reads as "nothing
    // to ask", which would switch the rule off without a word.
    static_assert(
        std::conditional_t<detail::canAskRoundingRule<std::remove_cv_t<Vs>, std::remove_cv_t<Rounding>>,
                           detail::RequireRoundingRuleMeasuresVariants<std::remove_cv_t<Vs>, std::remove_cv_t<Rounding>>,
                           std::true_type>::value);

    /// The variants, as `variants(...)` built them.
    Vs variantSet {};
    /// The rule applied to the selected variant's result.
    Rounding rounding {};
    /// The constraints, as `constraints(...)` built them.
    Constraints constraintSet {};
};

/// Builds a method:
/// `method(variants(...), rounding_rule<...>(), constraints(...))`.
template <typename Vs, typename Rounding, typename Constraints>
[[nodiscard]] constexpr Method<Vs, Rounding, Constraints> method(Vs variantSet,
                                                                 Rounding rounding,
                                                                 Constraints constraintSet) noexcept
{
    return Method<Vs, Rounding, Constraints> { variantSet, rounding, constraintSet };
}

namespace detail
{
    /// Refuses a tag no variant declares. There is no fallback variant and no
    /// "first match wins": a specimen matching no variant has no result, the
    /// same ruling phase 9 made for `bool satisfied()` and phase 10 made for a
    /// lookup miss. An author who wants a catch-all writes one.
    template <typename Tag, typename... Cs>
    struct RequireVariantForTag
    {
        static_assert((std::is_same_v<Tag, typename Cs::tag> || ...),
                      "formula: this method declares no variant for that tag; a method that matches "
                      "nothing has no result, so add a variant for it or select a tag it declares");

        static constexpr bool value = true;
    };

    /// The two rules a selection tag obeys, asked in an order that matters.
    ///
    /// A tag selected with is held to the rule a declared tag is held to,
    /// through the same guard, so `evaluate_method<const Cube>` is told what
    /// is wrong with `const Cube` rather than that no variant declares it --
    /// true, but it sends the author looking at the variants instead of at
    /// the tag. The match is therefore asked only once the tag is well
    /// formed, gated through `std::conditional_t` for the reason
    /// `RequireWellFormedVariants` gives. `method_selection_tag_cv_qualified.cpp`
    /// pins the gate by refusing any output that carries the no-match text.
    template <typename Tag, typename... Cs>
    struct RequireSelectableTag
    {
        static_assert(RequirePlainClassTag<Tag>::value);
        static_assert(std::conditional_t<isPlainClassTag<Tag>, RequireVariantForTag<Tag, Cs...>, std::true_type>::value);

        static constexpr bool value = true;
    };

    /// The position of the variant tagged @p Tag.
    ///
    /// Answers 0 when there is none. That answer is never used --
    /// `RequireSelectableTag` has already refused the build -- and it is 0
    /// rather than `sizeof...(Cs)` so that the `std::get` it feeds stays in
    /// range and adds no error of the standard library's to our refusal.
    template <typename Tag, typename... Cs>
    [[nodiscard]] consteval std::size_t variant_index() noexcept
    {
        constexpr bool matches[] = { std::is_same_v<Tag, typename Cs::tag>... };
        for (std::size_t index = 0; index < sizeof...(Cs); ++index)
            if (matches[index])
                return index;
        return 0;
    }

    /// Selects the variant tagged `Tag` from a variants pack.
    template <typename Tag, typename Vs>
    struct SelectVariant;

    /// The only specialisation: a `Variants` pack, whose variants' tags are
    /// what selection compares against.
    template <typename Tag, typename... Cs>
    struct SelectVariant<Tag, Variants<Cs...>>
    {
        static_assert(RequireSelectableTag<Tag, Cs...>::value);

        /// Where in `Variants::cases` the selected variant sits.
        static constexpr std::size_t index = variant_index<Tag, Cs...>();
    };
} // namespace detail

/// Evaluates the variant of @p m tagged `Tag`, rounded by @p m's own rounding
/// rule, in the coherent SI unit of its dimension -- the same unit every
/// `Evaluated<Rep>` in this library is in.
///
/// `Tag` is never deduced: which variant applies is a property of the
/// specimen, stated by the caller, never inferred from a number. A tag no
/// variant declares is refused at compile time rather than answered with a
/// fallback -- see `detail::RequireVariantForTag`.
///
/// The rounding is the ordinary rounding node wrapped around the selected
/// expression, so it rounds in the rule's unit exactly as
/// `rounded<U, Places, Mode>(...)` does, and a sink sees the steps it would
/// see for that node. That is also why `Rep = double` is refused here as it
/// is there -- see `RepRounding<double>`.
template <typename Tag, typename Rep = Rational, typename M, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> evaluate_method(M const& m, Env const& environment, Sink sink = {}) noexcept
{
    using Selection = detail::SelectVariant<Tag, std::remove_cvref_t<decltype(m.variantSet)>>;
    using Rule = std::remove_cvref_t<decltype(m.rounding)>;

    auto const& selected = std::get<Selection::index>(m.variantSet.cases);
    return detail::dispatch<Rep>(rounded<Rule::unit, Rule::places, Rule::mode>(selected.expression), environment, sink);
}

} // namespace formula
