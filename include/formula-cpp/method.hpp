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
/// This header is the declaration half: the variants, and the tags they apply
/// to. What selects among them, and what records that selection as a trace
/// step, are built on top of these.
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
/// **What a pack refuses, and why here rather than later.** Three rules, all
/// enforced by `Variants` itself:
///
///  1. every argument is a variant -- something `variant<Tag>(...)` returned;
///  2. there is at least one of them;
///  3. they all report the same dimension.
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
#include <formula-cpp/expression.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace formula
{

/// One variant of a method: the expression that applies when the specimen,
/// apparatus or product matches `Tag`.
///
/// `Tag` is an ordinary empty type and is never instantiated -- what a
/// variant applies to is a *type*, so that selecting one is a compile-time
/// fact the type system can state rather than a string nobody checks.
///
/// Deliberately not a `Node`. A variant does not stand where a number stands;
/// it names one of the formulas a method chooses between. Phase 10 settled
/// that test for lookups -- a lookup *is* a node because it produces a
/// quantity -- and it comes out the other way here.
template <typename Tag, Node Expr>
struct VariantCase
{
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
    /// Whether a type is something `variant<Tag>(...)` produced.
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

    /// The three rules a variants pack obeys, asked in an order that matters.
    ///
    /// One `static_assert` in `Variants` rather than three, so that the rules
    /// can be sequenced. The agreement rule is asked **only once every
    /// argument is a variant**: a non-variant has no `dimension` to compare,
    /// and asking anyway buries the one message that matters. Measured on cl
    /// 19.51 before this gate existed, `variants(42, 43)` reported six errors
    /// -- `C2825`, `C2510` and `C2065`, once for `First` and once for `Other`
    /// -- every one of them the compiler's own wording for "that has no such
    /// member", and not one of them ours.
    ///
    /// `std::conditional_t` and not `if constexpr`, because this is a
    /// constant initialiser rather than a statement; naming
    /// `RequireAllVariantsAgree<Cs...>` as a template argument does not
    /// instantiate it, and only the selected branch's `value` does.
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

/// Builds a method's variants pack: `variants(a, b, c)`. See `Variants` for
/// the three rules a pack has to satisfy.
template <typename... Cs>
[[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept
{
    return Variants<Cs...> { std::tuple<Cs...> { cases... } };
}

} // namespace formula
