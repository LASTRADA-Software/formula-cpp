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
/// two concepts. Two questions, one name, no diagnostic telling them apart.
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

#include <tuple>

namespace formula
{

namespace detail
{
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
    /// whose second AND third variants disagree: clang-cl 22.1.3, clang++
    /// 20.1.8 and g++ 13.3 each report the refusal twice, naming both
    /// offenders. cl 19.51 reports it once -- the instantiations all happen,
    /// but cl prints one diagnostic per `static_assert` SITE rather than per
    /// failure, so a cl user fixes the second disagreement on a second build.
    template <typename First, typename... Rest>
    struct RequireAllVariantsAgree<First, Rest...>
    {
        /// True when `First` agrees with every one of `Rest`.
        static constexpr bool value = (RequireVariantsAgree<First, Rest>::value && ...);
    };
} // namespace detail

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
/// The agreement check sits in this class body rather than in `variants()`
/// below, deliberately: this is a public aggregate with a public member, so a
/// `Variants<...>` can be declared directly with no factory call anywhere,
/// and a check placed only in the factory would let that route through. The
/// same mistake was found, and fixed, in the lookup tables of phase 10.
template <typename... Cs>
struct Variants
{
    static_assert(detail::RequireAllVariantsAgree<Cs...>::value);

    /// The variants, in the order `variants(...)` was called with them. That
    /// order is part of the contract: selection reports which index fired,
    /// and a reader matches it back to the declaration by counting.
    std::tuple<Cs...> cases {};
};

/// Builds a method's variants pack: `variants(a, b, c)`. See `Variants`.
template <typename... Cs>
[[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept
{
    return Variants<Cs...> { std::tuple<Cs...> { cases... } };
}

} // namespace formula
