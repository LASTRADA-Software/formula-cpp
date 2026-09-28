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
/// variant by tag in `evaluate_method`, which tells a sink that asks which
/// variant it selected -- see `VariantSelection` (`sink.hpp`) and
/// `StepKind::VariantSelected` (`trace.hpp`).
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
/// **What a pack refuses, and why here rather than later.** Five rules, the
/// first four enforced by `Variants` itself and the fifth by each
/// `VariantCase` it holds:
///
///  1. every argument is a variant -- a `VariantCase`, which is what
///     `variant<Tag>(...)` returns;
///  2. there is at least one of them;
///  3. they all report the same dimension;
///  4. no two of them declare the same tag;
///  5. every variant's tag is a plain class type.
///
/// Rule 4 is about tags, which are types, and so it is decidable for every
/// pack. Whether two variants' *conditions* overlap is not: a condition over
/// a runtime value, such as the number a `ConstantNode` holds, leaves nothing
/// in the type to compare. So this header refuses repeated tags and does not
/// attempt the general question.
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
/// is a property of the quantity a method *reports*, and a method has no
/// typed result that names one: `evaluate_method` answers in the coherent SI
/// unit of the variants' dimension (see `overlay.hpp`). **So nothing enforces
/// the unit half.** The dimension half is enforced here, where the variants
/// are; the unit a method is read in is its rounding rule's, which the
/// rounding rule's own check holds to that dimension.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/tag.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
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
    ///
    /// Deliberately no `{}` default member initialiser: with one, a method
    /// holding a lookup fails to compile on clang++ and clang-cl, and cl
    /// answers the trait wrongly -- see `Corrections` (`lookup.hpp`).
    Expr expression;

    /// The dimension this variant reports. Every variant of one method must
    /// publish the same one -- see `detail::RequireVariantsAgree`.
    static constexpr Dimension dimension = Expr::dimension;
    /// Whether its expression was refused -- see `detail::refused_already`.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Expr>();
};

/// The spelling of one variant in a method: `variant<Cube>(expr)`. See the
/// file comment for why this is not spelled `when<Cube>`.
template <typename Tag, Node Expr>
[[nodiscard]] constexpr VariantCase<Tag, Expr> variant(Expr expression) noexcept
{
    return VariantCase<Tag, Expr> { expression };
}

/// A series handed to `variant<Tag>`: fails to compile, in this library's
/// words. A method reports one value, so a variant
/// holds a `Node`; a series becomes one only through a reduction.
///
/// What it returns is a variant of the right tag and dimension over a
/// placeholder constant, so that `variants(...)` and `method(...)` around the
/// call find nothing further to refuse: the author made one mistake, and is
/// told about one.
template <typename Tag, SeriesNode Expr>
[[nodiscard]] constexpr VariantCase<Tag, ConstantNode<coherent(Expr::dimension)>> variant(Expr) noexcept
{
    static_assert(detail::RequireSingleValueExpression<Expr>::value);
    return VariantCase<Tag, ConstantNode<coherent(Expr::dimension)>> { ConstantNode<coherent(Expr::dimension)> {} };
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

    /// Whether a type is a `VariantCase` whose tag passes the tag rule, asked
    /// without firing it. False for anything that is not a variant, so it
    /// never reads a `tag` that is not there.
    template <typename T>
    struct CaseTagIsPlain: std::false_type
    {
    };

    template <typename Tag, Node Expr>
    struct CaseTagIsPlain<VariantCase<Tag, Expr>>: std::bool_constant<isPlainClassTag<Tag>>
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
        static_assert(refused_already<First>() || refused_already<Other>() || First::dimension == Other::dimension,
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

    /// Row @p T of the same-type table `first_repeated_pair` reads: whether
    /// `T` is each of `Ts`, in order.
    template <typename T, typename... Ts>
    inline constexpr std::array<bool, sizeof...(Ts)> isSameAsEach { std::is_same_v<T, Ts>... };

    /// Two positions in a pack. `first == second` is no pair at all, and is
    /// what `first_repeated_pair` answers when every type is distinct.
    struct PositionPair
    {
        /// The earlier of the two positions.
        std::size_t first = 0;
        /// The later of the two positions.
        std::size_t second = 0;
    };

    /// The first two positions of @p Ts holding the same type, ordered by the
    /// later position; `{ 0, 0 }` when there are none.
    ///
    /// Compares EVERY pair, not only neighbours: a repeated tag is as much a
    /// mistake four variants apart as it is side by side, and nothing about
    /// tags orders them so that a repeat would have to be adjacent.
    ///
    /// The ONLY statement of the distinct-tags rule: `all_distinct` and
    /// `RequireDistinctVariantTags` both ask it, for the reason
    /// `isPlainClassTag` gives -- two copies of a rule drift.
    template <typename... Ts>
    [[nodiscard]] consteval PositionPair first_repeated_pair() noexcept
    {
        constexpr std::size_t typeCount = sizeof...(Ts);
        constexpr std::array<std::array<bool, typeCount>, typeCount> same { isSameAsEach<Ts, Ts...>... };
        for (std::size_t later = 1; later < typeCount; ++later)
            for (std::size_t earlier = 0; earlier < later; ++earlier)
                if (same[earlier][later])
                    return PositionPair { earlier, later };
        return PositionPair {};
    }

    /// True when no two of @p Ts are the same type.
    template <typename... Ts>
    [[nodiscard]] consteval bool all_distinct() noexcept
    {
        constexpr PositionPair repeated = first_repeated_pair<Ts...>();
        return repeated.first == repeated.second;
    }

    /// Fails to compile when two variants of one method declare the same tag.
    ///
    /// A method with two variants for one tag has no answer to "which one
    /// applies", and selecting the first would leave the second as dead code
    /// nobody is told about.
    ///
    /// Templated on the tag and on BOTH positions that declare it, so that the
    /// diagnostic names the mistake outright: `RequireTagDeclaredOnce<1, 3,
    /// Cylinder>` rather than a whole pack to be searched by eye. Only
    /// `RequireDistinctVariantTags` instantiates it, and hands it
    /// `first == second` when there is no repeat -- see `PositionPair`.
    template <std::size_t First, std::size_t Second, typename Tag>
    struct RequireTagDeclaredOnce
    {
        static_assert(First == Second,
                      "formula: this method declares two variants for the same tag; a method with two "
                      "variants for one tag has no answer to which of them applies -- the tag appears in "
                      "this diagnostic as the template argument Tag of RequireTagDeclaredOnce, and First "
                      "and Second are the ZERO-BASED positions of the two variants that declare it, so 0 "
                      "is the first variant");

        static constexpr bool value = true;
    };

    /// True when no two variants in a pack declare the same tag; the refusal
    /// is `RequireTagDeclaredOnce`'s.
    ///
    /// Reads `Cs::tag` here, inside the template, and never in the argument
    /// list that names it -- which is what lets `RequireWellFormedVariants`
    /// gate it: `RequireDistinctVariantTags<typename Cs::tag...>` would read
    /// the tags where the template-id is formed, gate or no gate.
    ///
    /// The primary template is the empty pack, vacuously true, for the reason
    /// `RequireAllVariantsAgree` has one: `variants()` passes the gate in
    /// `RequireWellFormedVariants`, and must be told only that it is empty.
    /// Only the partial specialisation below reads a tag, and it needs a
    /// `First` to read one from, which an empty pack cannot supply -- so the
    /// empty pack lands here. Were this primary template only declared, the
    /// empty pack would land on an undefined template instead, and the
    /// compiler would add its own error for that to the one message that
    /// matters.
    template <typename... Cs>
    struct RequireDistinctVariantTags
    {
        /// Always true: an empty pack repeats nothing.
        static constexpr bool value = true;
    };

    template <typename First, typename... Rest>
    struct RequireDistinctVariantTags<First, Rest...>
    {
        /// Where the first repeated tag sits, if anywhere.
        static constexpr PositionPair repeated = first_repeated_pair<typename First::tag, typename Rest::tag...>();

        /// True when no tag is repeated.
        static constexpr bool value = RequireTagDeclaredOnce<
            repeated.first,
            repeated.second,
            std::tuple_element_t<repeated.first, std::tuple<typename First::tag, typename Rest::tag...>>>::value;
    };

    /// Four of the five rules a variants pack obeys, asked in an order that
    /// matters. The fifth, the tag rule, is each `VariantCase`'s own.
    ///
    /// One `static_assert` in `Variants` rather than four, so that the rules
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
    ///
    /// The distinct-tag rule reads `Cs::tag`, so it sits behind that gate,
    /// and `method_variants_agreement_gated` rejects its name too. It sits
    /// behind a second gate as well: every tag is a plain class type. Two
    /// `variant<int>` would otherwise be refused twice for one mistake -- once
    /// by the tag rule and once as a repeated tag -- and fixing the tag clears
    /// both, so only the tag rule's message says what to fix.
    /// `RequireSelectableTag` gates its match on `isPlainClassTag` for the
    /// same reason; `method_duplicate_tag_not_plain.cpp` pins this gate.
    ///
    /// It is deliberately NOT gated on the agreement rule, nor the agreement
    /// rule on it: the two are independent. Measured with
    /// `variants(variant<Cube>(pressure), variant<Cube>(length))` on cl
    /// 19.51, clang-cl 22.1.3, clang++ 20.1.8 and g++ 13.3: all four report
    /// both refusals in one build. cl included -- it collapses two failures
    /// of the SAME rule, as `RequireAllVariantsAgree` records, but not
    /// failures of two different rules.
    template <typename... Cs>
    struct RequireWellFormedVariants
    {
        static_assert(RequireAtLeastOneVariant<Cs...>::value);
        static_assert(RequireEveryArgumentIsAVariant<std::index_sequence_for<Cs...>, Cs...>::value);

        /// Whether the rules that read a variant's members have anything they
        /// can ask about.
        static constexpr bool everyArgumentIsAVariant = (IsVariantCase<Cs>::value && ...);

        /// Whether every argument is a variant whose tag passes the tag rule.
        static constexpr bool everyTagIsPlain = (CaseTagIsPlain<Cs>::value && ...);

        static_assert(std::conditional_t<everyArgumentIsAVariant, RequireAllVariantsAgree<Cs...>, std::true_type>::value);
        static_assert(std::conditional_t<everyArgumentIsAVariant && everyTagIsPlain,
                                         RequireDistinctVariantTags<Cs...>,
                                         std::true_type>::value);

        static constexpr bool value = true;
    };
} // namespace detail

namespace detail
{
    /// `0, 1, ..., Count - 1`: the published positions of a pack no overlay
    /// has touched.
    template <std::size_t Count>
    [[nodiscard]] constexpr std::array<std::size_t, Count> positions_in_order() noexcept
    {
        std::array<std::size_t, Count> positions {};
        for (std::size_t entryIndex = 0; entryIndex < Count; ++entryIndex)
            positions[entryIndex] = entryIndex;
        return positions;
    }

    /// Whether @p positions could be where a pack's variants were published in
    /// a method of @p total: each below @p total, and each above the one
    /// before it.
    ///
    /// **Strictly increasing, not merely distinct.** A pin or a prune keeps
    /// the variants it keeps in declaration order, so their published
    /// positions always increase; `{ 1, 0 }` is distinct and below the count,
    /// and no overlay can produce it -- it would report each of two variants
    /// as the other. Increasing also implies distinct, so one comparison per
    /// neighbouring pair says both.
    template <std::size_t Count>
    [[nodiscard]] constexpr bool is_published_layout(std::array<std::size_t, Count> const& positions,
                                                     std::size_t total) noexcept
    {
        for (std::size_t entryIndex = 0; entryIndex < Count; ++entryIndex)
        {
            if (positions[entryIndex] >= total)
                return false;
            if (entryIndex > 0 && positions[entryIndex - 1] >= positions[entryIndex])
                return false;
        }
        return true;
    }

    /// Reached only while building a published layout that is not one -- see
    /// `PublishedLayout`. Its name is the refusal.
    ///
    /// **Why a name and not a `static_assert`.** The positions are values, not
    /// types: a pack pruned down to `(Cylinder, Prism)` is published at
    /// `{ 1, 2 }` of 3, and a pack of exactly the same type written by hand
    /// at `{ 0, 1 }` of 2, so there is nothing in the type to assert on.
    /// Instead this function is deliberately not `constexpr`, and it is only
    /// ever called from a `consteval` constructor. A layout that is not one
    /// therefore fails to compile, and each compiler names the function it
    /// could not call. It is never called at run time -- there is no run time
    /// path to it at all -- which is why its body is empty rather than one
    /// that ends the program.
    inline void published_positions_must_increase_and_stay_below_the_published_count() noexcept {}

    template <std::size_t Count>
    class PublishedLayout;

    /// The last parameter of every constructor that refuses an author's
    /// statement of provenance -- `RoundingRule`'s, `OverlaidConstraints`',
    /// `ConstraintOrigin`'s, the three overlay nodes', `RoundingRuleNode`'s and
    /// `PublishedLayout`'s. Defaulted, so braces still reach the constructor and
    /// are refused in the library's words.
    ///
    /// **Why it is there.** An explicit specialisation of a refusing
    /// constructor spelt with the arguments braces give it -- `template <>
    /// formula::ConstraintOrigin::ConstraintOrigin(ConstraintProvenance,
    /// Citation) noexcept: _provenance { ... } {}` -- matches no member while
    /// this parameter is there, so the obvious spelling of that forgery does
    /// not compile. That is all it does.
    ///
    /// **What no C++ library can stop.** An explicit specialisation of a
    /// member of a library class template -- a constructor, an accessor such
    /// as `RoundingRule<...>::provenance()`, a defaulted default constructor
    /// -- is a member definition, with a member's access to the private
    /// fields, and it can make any of them say anything. So can friend
    /// injection, which names a `detail::` type without spelling `detail::`.
    /// Both were measured forging a rounding rule's provenance on all three
    /// compilers. The only supported customisation points are `TagName`,
    /// `EnumeratorName`, `Describe`, `RepTraits`, `OpaqueOperation` and the
    /// vocabulary; specialising any other formula-cpp template or member is
    /// outside the contract, and can make a trace say anything.
    struct ProvenanceStatedByAuthor
    {
    };

    /// The one door to a layout stated by hand, and checked: for this
    /// library's own negative cases, which pin the check, and for nothing
    /// else. Code that spells `detail::` has stepped outside the contract.
    struct PublishedLayoutAccess
    {
        /// @p published of @p publishedCount, refused at compile time unless
        /// every position is below @p publishedCount and above the one
        /// before it.
        template <std::size_t Count>
        [[nodiscard]] static consteval PublishedLayout<Count> checked(std::array<std::size_t, Count> const& published,
                                                                      std::size_t publishedCount) noexcept;

        /// The variants of @p layout at @p Kept, each keeping its published
        /// position -- for `apply_operation` of `pin_variant` and
        /// `prune_variant` (`overlay.hpp`), its only callers. See
        /// `PublishedLayout::kept`.
        template <std::size_t... Kept, std::size_t Count>
        [[nodiscard]] static constexpr PublishedLayout<sizeof...(Kept)> selected(
            PublishedLayout<Count> const& layout) noexcept;

        /// @p layout, marked as pinned by an overlay citing @p cited -- for
        /// `apply_operation` of `pin_variant` (`overlay.hpp`), its only caller.
        template <std::size_t Count>
        [[nodiscard]] static constexpr PublishedLayout<Count> pinned(PublishedLayout<Count> layout,
                                                                     Citation const& cited) noexcept;

        /// @p layout, marked as having had one more variant pruned, by an
        /// overlay citing @p cited -- for `apply_operation` of
        /// `prune_variant`, its only caller.
        template <std::size_t Count>
        [[nodiscard]] static constexpr PublishedLayout<Count> pruned(PublishedLayout<Count> layout,
                                                                     Citation const& cited) noexcept;
    };

    /// Where each variant of a pack sits in the method **as published**, and
    /// how many variants that method declares -- see `Variants::published`.
    ///
    /// Encapsulated, unlike the rest of `Variants`, because this is the one
    /// part with an invariant of its own: every position below the count, and
    /// each above the one before it (see `is_published_layout`). As a pair of
    /// public members it let anyone state a layout that is not one -- the same
    /// position for two variants, two variants swapped, or a 4th of 3 -- and a
    /// trace then counted in it without a word. There are two ways to build
    /// one, and neither can build a layout that breaks that invariant:
    ///
    ///  - the default constructor, declaration order;
    ///  - `kept<Kept...>()`, which is how `apply` (`overlay.hpp`) carries a
    ///    layout through a pin or a prune, reached only through
    ///    `PublishedLayoutAccess::selected`.
    ///
    /// **A layout is not stated from outside the library, except by
    /// copying one** (see below). The constructor that takes positions and a
    /// count is public, so that braces reach it, and refuses whenever it is
    /// used, in the library's words. Once it checked the layout and accepted
    /// any well-formed one, so `{ { 5, 7 }, 9 }` made a two-variant method
    /// that no overlay touched report its variants as the 6th and 8th of 9.
    /// `select<Kept...>()` is public for the same reason and refuses the same
    /// way: while it made the selection itself, `decltype(nine.published)
    /// {}.select<5, 7>()`, taken from a throwaway pack of nine, did the same
    /// thing with no prune on record. The checked constructor and `kept` are
    /// private, reached through `PublishedLayoutAccess`, which the overlay
    /// operations and the negative cases pinning the check use.
    ///
    /// **Why a selection and not the checking constructor.** Which variants a pin
    /// or a prune keeps is known from the tags, at compile time, but where they
    /// were published is not: the pack an overlay is applied to may itself be
    /// the result of an earlier overlay, held in a variable whose layout is
    /// run time data. `kept` takes the positions to keep as template
    /// arguments, checked by the same rule at compile time, and copies their
    /// published positions out of a layout that is already valid. An
    /// increasing selection of entries from an increasing layout below its
    /// count is one too, so nothing is left to check at run time.
    ///
    /// **It also records the pin and the prunes** -- `pinned()`, `pinned_by()`,
    /// `pruned_count()` and `pruned_by()`, with what the overlays cited --
    /// stated only through `PublishedLayoutAccess` by the two operations that
    /// make them, and carried wherever the positions are. The fields are
    /// private for the reason the positions are: which variants a method
    /// keeps is a jurisdiction's decision, the library's to record.
    ///
    /// **What remains: a copy.** A layout copied from another method's pack
    /// of the same size -- `pack.published = other.published`, or the same in
    /// an aggregate initialiser -- or reset to declaration order by
    /// `pack.published = {}`, is one the library made, pin or prune included,
    /// and indistinguishable from what a legitimate prune produces: `{ 1, 2 }`
    /// of 3 is both. So is a layout reached by explicitly specialising one of
    /// this class's members, which is outside the contract -- see
    /// `ProvenanceStatedByAuthor`.
    /// `published` is a public member so that a pack stays an aggregate, and
    /// copying a value is always possible; assigning it is the author's
    /// explicit act.
    template <std::size_t Count>
    class PublishedLayout
    {
      public:
        /// In declaration order, of `Count`: a pack no overlay has touched.
        constexpr PublishedLayout() noexcept:
            _positions { positions_in_order<Count>() },
            _total { Count }
        {
        }

        /// Refuses whenever it is used: a layout is not stated from outside
        /// the library. Public, so that `{ { 5, 7 }, 9 }` reaches this and is
        /// refused in the library's words rather than the compiler's "is
        /// private". A template only so that the refusal waits for a use:
        /// @p Stated is never given.
        template <bool Stated = false>
        constexpr PublishedLayout(std::array<std::size_t, Count> const& published,
                                  std::size_t publishedCount,
                                  ProvenanceStatedByAuthor = {}) noexcept:
            _positions { published },
            _total { publishedCount }
        {
            static_assert(Stated,
                          "formula: a variant's published position is stated only by the library -- by "
                          "variants(...), and carried by apply() through a pin or a prune -- since a layout written "
                          "by hand could make a trace report a position and a count no method has; build the pack "
                          "with variants(...)");
        }

        /// Refuses whenever it is used: which variants a layout keeps is a
        /// pin's or a prune's, carried by `apply()`. Public, so that
        /// `layout.select<5, 7>()` is refused in the library's words rather
        /// than the compiler's "is private".
        template <std::size_t... Kept>
        [[nodiscard]] constexpr PublishedLayout<sizeof...(Kept)> select() const noexcept
        {
            static_assert(sizeof...(Kept) == static_cast<std::size_t>(-1),
                          "formula: a variant's published position is stated only by the library -- which variants "
                          "a layout keeps is carried by apply() through a pin or a prune, and a selection made by "
                          "hand could make a trace report a position and a count with no prune on record; use "
                          "pin_variant or prune_variant in an overlay");
            return {};
        }

        /// The ZERO-BASED published position of the variant at @p overlaidIndex.
        [[nodiscard]] constexpr std::size_t position(std::size_t overlaidIndex) const noexcept
        {
            return _positions[overlaidIndex];
        }

        /// How many variants the method declares as published.
        [[nodiscard]] constexpr std::size_t count() const noexcept
        {
            return _total;
        }

        /// Whether an overlay pinned the pack this layout belongs to to its
        /// one variant.
        [[nodiscard]] constexpr bool pinned() const noexcept
        {
            return _pinned;
        }

        /// What the overlay that pinned it cited; empty when none did.
        [[nodiscard]] constexpr Citation const& pinned_by() const noexcept
        {
            return _pinnedBy;
        }

        /// How many variants overlays pruned from the pack.
        [[nodiscard]] constexpr std::size_t pruned_count() const noexcept
        {
            return _prunedCount;
        }

        /// What the last overlay that pruned cited; empty when none did.
        [[nodiscard]] constexpr Citation const& pruned_by() const noexcept
        {
            return _prunedBy;
        }

      private:
        template <std::size_t>
        friend class PublishedLayout;

        friend struct PublishedLayoutAccess;

        /// Marks the checking constructor.
        struct Checked
        {
        };

        /// @p published of @p publishedCount, refused at compile time unless
        /// every position is below @p publishedCount and above the one before
        /// it. `consteval`, so that no layout reaches a trace unchecked.
        consteval PublishedLayout(Checked,
                                  std::array<std::size_t, Count> const& published,
                                  std::size_t publishedCount) noexcept:
            _positions { published },
            _total { publishedCount }
        {
            if (!is_published_layout(published, publishedCount))
                published_positions_must_increase_and_stay_below_the_published_count();
        }

        /// The layout of the variants at @p Kept, in that order: each keeps
        /// its published position, and the count is unchanged.
        ///
        /// @p Kept are positions in THIS pack, not published ones, and are
        /// held to the rule the layout itself obeys -- each below `Count`,
        /// each above the one before -- by the constructor that forms them, at
        /// compile time. So a selection keeps declaration order, as a pin or a
        /// prune does.
        template <std::size_t... Kept>
        [[nodiscard]] constexpr PublishedLayout<sizeof...(Kept)> kept() const noexcept
        {
            constexpr PublishedLayout<sizeof...(Kept)> checkedKept { typename PublishedLayout<sizeof...(Kept)>::Checked {},
                                                                     std::array<std::size_t, sizeof...(Kept)> { Kept... },
                                                                     Count };
            static_cast<void>(checkedKept);
            PublishedLayout<sizeof...(Kept)> keptLayout { typename PublishedLayout<sizeof...(Kept)>::Selected {},
                                                          { _positions[Kept]... },
                                                          _total };
            keptLayout._pinned = _pinned;
            keptLayout._pinnedBy = _pinnedBy;
            keptLayout._prunedCount = _prunedCount;
            keptLayout._prunedBy = _prunedBy;
            return keptLayout;
        }

        /// Marks the constructor only `kept` uses.
        struct Selected
        {
        };

        /// A layout `kept` has already shown valid.
        constexpr PublishedLayout(Selected,
                                  std::array<std::size_t, Count> const& published,
                                  std::size_t publishedCount) noexcept:
            _positions { published },
            _total { publishedCount }
        {
        }

        std::array<std::size_t, Count> _positions;
        std::size_t _total;
        /// Stated only through `PublishedLayoutAccess`, as the positions are:
        /// a pin or a prune is a jurisdiction's decision, the library's to
        /// record.
        bool _pinned = false;
        Citation _pinnedBy {};
        std::size_t _prunedCount = 0;
        Citation _prunedBy {};
    };

    template <std::size_t Count>
    consteval PublishedLayout<Count> PublishedLayoutAccess::checked(std::array<std::size_t, Count> const& published,
                                                                    std::size_t publishedCount) noexcept
    {
        return PublishedLayout<Count> { typename PublishedLayout<Count>::Checked {}, published, publishedCount };
    }

    template <std::size_t... Kept, std::size_t Count>
    constexpr PublishedLayout<sizeof...(Kept)> PublishedLayoutAccess::selected(PublishedLayout<Count> const& layout) noexcept
    {
        return layout.template kept<Kept...>();
    }

    template <std::size_t Count>
    constexpr PublishedLayout<Count> PublishedLayoutAccess::pinned(PublishedLayout<Count> layout,
                                                                   Citation const& cited) noexcept
    {
        layout._pinned = true;
        layout._pinnedBy = cited;
        return layout;
    }

    template <std::size_t Count>
    constexpr PublishedLayout<Count> PublishedLayoutAccess::pruned(PublishedLayout<Count> layout,
                                                                   Citation const& cited) noexcept
    {
        ++layout._prunedCount;
        layout._prunedBy = cited;
        return layout;
    }
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
    ///
    /// Deliberately no `{}` default member initialiser. With one, no method
    /// fails to compile, but asking whether this type -- or a tuple holding
    /// it -- is default-constructible hard-errors or answers `true` when a
    /// lookup is inside; see `Corrections` (`lookup.hpp`).
    std::tuple<Cs...> cases;

    /// For each of `cases`, its ZERO-BASED position in the method **as
    /// published** -- the `variants(...)` an author wrote, before any overlay
    /// pinned or pruned one -- and how many variants that method declares.
    /// What a trace reports as the selected variant's position and count
    /// (`Step::variantIndex`, `Step::variantCount`), so that a reader counting
    /// back in the only `variants(...)` in the source lands on the variant
    /// that ran.
    ///
    /// `variants(...)` numbers them in order; `apply` (`overlay.hpp`) carries
    /// them through a pin or a prune, so that a published `(Cube, Cylinder,
    /// Prism)` with `Cube` pruned still reports `Cylinder` as the 2nd of 3,
    /// not the 1st of 2 -- and records that an overlay pruned or pinned, so
    /// that the trace says it. A layout whose positions do not increase, or
    /// reach the count, is refused -- see `detail::PublishedLayout`.
    ///
    /// **Maintained by the library; copying one here is the author's act.**
    /// A layout cannot be stated by hand -- `{ { 5, 7 }, 9 }` and
    /// `published.select<5, 7>()` are refused -- but another method's layout
    /// of the same size, copied here, is one the
    /// library made, and cannot be told from what a legitimate prune leaves:
    /// `{ 1, 2 }` of 3 is both. A trace then counts in the layout the author
    /// put here.
    ///
    /// A default member initialiser is safe here, unlike on `cases`: it names
    /// no variant's type, so asking whether this pack is default-constructible
    /// instantiates nothing of theirs.
    detail::PublishedLayout<sizeof...(Cs)> published {};
};

/// Builds a method's variants pack: `variants(a, b, c)`. See the file comment
/// for the five rules a pack has to satisfy.
template <typename... Cs>
[[nodiscard]] constexpr Variants<Cs...> variants(Cs... cases) noexcept
{
    return Variants<Cs...> { std::tuple<Cs...> { cases... } };
}

/// Where a method's rounding rule comes from.
///
/// Spec section 9.1 asks the trace to say which rounding rule applied **and
/// where it came from**. A trace naming only the granularity is true whether
/// the method's author or a jurisdiction chose it, and so says nothing about
/// the second half.
///
/// Checked on GCC under `-Wshadow`, the way `StepKind::PiConstant` had to be:
/// nothing in namespace `formula` is spelt `MethodDefault` or
/// `JurisdictionOverlay`.
///
/// `MethodDefault` where `ConstraintProvenance` says `MethodOwn`, on purpose:
/// a method's rule is a default a jurisdiction may override in part.
enum class RoundingProvenance : std::uint8_t
{
    /// The rule the method was declared with: `method(..., rounding_rule<...>(), ...)`.
    MethodDefault,
    /// A rule a jurisdiction's overlay put in its place: `with_rounding<...>(source)`
    /// (`overlay.hpp`).
    JurisdictionOverlay,
};

namespace detail
{
    /// Marks the one constructor of `RoundingRule` that states an overlay's
    /// provenance.
    struct OverlaidRule
    {
    };

    /// The one way to build a rounding rule an overlay set:
    /// `apply_operation` for `with_rounding` (`overlay.hpp`) is its only
    /// caller. The provenance a trace reports is a statement of fact about
    /// where a rule came from, so it is the library's to state, never an
    /// author's -- see `RequireLibraryStatesProvenance`.
    struct RoundingRuleAccess
    {
        /// @p Rule as a jurisdiction overlay's, citing @p source.
        template <typename Rule>
        [[nodiscard]] static constexpr Rule overlaid(Citation source) noexcept
        {
            return Rule { OverlaidRule {}, source };
        }
    };

    /// Fails to compile when an author states a rounding rule's provenance.
    ///
    /// A method's own rule reporting itself as a jurisdiction overlay's would
    /// put a false statement into every trace of that method, in the one field
    /// an inspector reads to learn whose rule it was. `rounding_rule<...>()`
    /// is the method's own rule and `with_rounding` an overlay's; nothing
    /// else is either.
    template <typename Rule>
    struct RequireLibraryStatesProvenance
    {
        static_assert(!std::is_same_v<Rule, Rule>,
                      "formula: a rounding rule's provenance is the library's to state, not an author's; "
                      "rounding_rule<...>() is the method's own rule, and with_rounding<...>(citation) applied by an "
                      "overlay is a jurisdiction's -- the rule appears in this diagnostic as the template "
                      "argument of RequireLibraryStatesProvenance");

        static constexpr bool value = true;
    };

    struct RoundingRuleNodeAccess;

    /// Marks the private constructor `RoundingRuleNodeAccess` calls.
    struct MethodMade
    {
    };

    /// Fails to compile when an author builds a `RoundingRuleNode`. The node
    /// makes a trace say a value was rounded by a method's rule -- the
    /// method's own or a jurisdiction's -- so one built by hand and put in a
    /// formula would say so where there is no method.
    template <typename RuleNode>
    struct RequireMethodMadeRoundingNode
    {
        static_assert(!std::is_same_v<RuleNode, RuleNode>,
                      "formula: only evaluate_method builds a rounding rule node; it makes a trace say a value was "
                      "rounded by a method's rule, so one built by hand would say so where there is no method -- "
                      "round with rounded<...>(...) instead; the node appears in this diagnostic as the template "
                      "argument of RequireMethodMadeRoundingNode");

        static constexpr bool value = true;
    };
} // namespace detail

/// A method's rounding rule, declared rather than applied after the fact, so
/// the trace can say which rule fired and where it came from (spec section
/// 9.1). Carries no operand: it is applied to whichever variant is selected.
///
/// The granularity is the type; where it came from is data, private, and set
/// only by the library -- see `detail::RoundingRuleAccess`.
///
/// **The guard governs how a rule is created, not where a copy travels.** A
/// rule `with_rounding` produced keeps its provenance when it is copied --
/// into another `method(...)`, say, as `apply(overlay, m).rounding` -- and
/// that is legitimate: the claim stays true of the rule, which is the
/// jurisdiction's wherever it is used. Likewise `Method::rounding` is a public
/// member, and assigning it a default rule relabels a jurisdiction's rule as
/// the method's own. Both are explicit acts on public members, as assigning
/// `Variants::published` is; what no author can do, short of reinterpreting a
/// rule's bytes -- `std::bit_cast` at run time -- or explicitly specialising
/// one of its members, is create a rule that states a provenance the library
/// did not give it. A specialisation such as `template <> constexpr
/// RoundingProvenance RoundingRule<...>::provenance() const noexcept` can make
/// the rule say anything, and no C++ library can stop it; it is outside the
/// contract, see `detail::ProvenanceStatedByAuthor`. `with_rounding`
/// (`overlay.hpp`) can replace a rule with one of the same granularity -- a
/// jurisdiction adopting the base standard's rounding in its own name -- and
/// the trace must still say whose rule it was.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
class RoundingRule
{
  public:
    /// The unit the rounding happens in -- see `rounding_node.hpp` for why a
    /// rounding that does not name one means nothing.
    static constexpr Unit unit = U;
    /// How many decimal places of `unit` to keep.
    static constexpr DecimalPlaces places = Places;
    /// Which way to break ties, and which way to go.
    static constexpr RoundingMode mode = Mode;

    /// The method's own rule: what `rounding_rule<...>()` returns.
    constexpr RoundingRule() noexcept = default;

    /// Refused: see `detail::RequireLibraryStatesProvenance`. Declared only so
    /// that `RoundingRule<...> { RoundingProvenance::JurisdictionOverlay, c }`
    /// is refused in this library's words rather than the compiler's.
    constexpr RoundingRule(RoundingProvenance, Citation = {}, detail::ProvenanceStatedByAuthor = {}) noexcept
    {
        static_assert(detail::RequireLibraryStatesProvenance<RoundingRule>::value);
    }

    /// Where the rule comes from: the method's own, unless an overlay's
    /// `with_rounding` replaced it.
    [[nodiscard]] constexpr RoundingProvenance provenance() const noexcept
    {
        return _provenance;
    }

    /// What the overlay that replaced the rule cited for it; empty when it
    /// cited nothing, and for the method's own rule.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

  private:
    friend struct detail::RoundingRuleAccess;

    /// An overlay's rule, citing @p cited -- reachable only through
    /// `detail::RoundingRuleAccess`.
    constexpr RoundingRule(detail::OverlaidRule, Citation cited) noexcept:
        _provenance { RoundingProvenance::JurisdictionOverlay },
        _source { cited }
    {
    }

    RoundingProvenance _provenance = RoundingProvenance::MethodDefault;
    Citation _source {};
};

/// The spelling of a method's rounding rule:
/// `rounding_rule<unit::Megapascal, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>()`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr RoundingRule<U, Places, Mode> rounding_rule() noexcept
{
    return {};
}

/// Whose constraints a method checks: its own, or a jurisdiction's that
/// replaced them. Recorded for the reason `RoundingProvenance` is: a verdict
/// saying only that a specimen was rejected is true whether the method's
/// author or a jurisdiction wrote the check, and so says nothing about whose
/// acceptance logic rejected it.
///
/// Checked on GCC under `-Wshadow`, as `RoundingProvenance` was: nothing in
/// namespace `formula` is spelt `MethodOwn`, and `JurisdictionOverlay` is an
/// enumerator of a scoped enumeration there too, which does not collide.
///
/// `MethodOwn` where `RoundingProvenance` says `MethodDefault`, on purpose:
/// constraints are the method's own until a jurisdiction replaces them whole.
enum class ConstraintProvenance : std::uint8_t
{
    /// The constraints the method was declared with: `method(..., constraints(...))`.
    MethodOwn,
    /// Constraints a jurisdiction's overlay put in their place:
    /// `with_constraints(..., source)` (`overlay.hpp`).
    JurisdictionOverlay,
};

namespace detail
{
    /// Marks the constructors of `ConstraintOrigin` and `OverlaidConstraints`
    /// that state an overlay's provenance.
    struct ConstraintsOverlaid
    {
    };

    /// The one way to build constraints an overlay set, and the one way to
    /// read whose a method's constraints are: `apply_operation` for
    /// `with_constraints` (`overlay.hpp`) is the only caller of `overlaid`
    /// -- see `RequireLibraryStatesConstraintProvenance`.
    struct ConstraintOriginAccess;

    /// Fails to compile when an author states whose a method's constraints
    /// are. A method's own constraints reporting themselves as a
    /// jurisdiction's would put a false statement beside every verdict of
    /// that method, in the one clause an inspector reads to learn whose check
    /// it was.
    template <typename Provenance>
    struct RequireLibraryStatesConstraintProvenance
    {
        static_assert(!std::is_same_v<Provenance, Provenance>,
                      "formula: whose a method's constraints are is the library's to state, not an author's; "
                      "method(..., constraints(...)) declares the method's own, and with_constraints(...) applied "
                      "by an overlay a jurisdiction's");

        static constexpr bool value = true;
    };
} // namespace detail

/// A jurisdiction's constraints: the `ConstraintSet` an overlay's
/// `with_constraints` put in place of a method's own, together with what the
/// overlay cited. It is what `Method::constraintSet` holds after that
/// overlay, so that whose the constraints are travels **with** them, as a
/// `RoundingRule`'s provenance travels with the rule.
///
/// Built only by the library -- see `detail::ConstraintOriginAccess` -- and
/// the constructor that would let an author build one is refused in the
/// library's words. A method's own constraints are a plain `ConstraintSet`,
/// which says nothing of any jurisdiction.
///
/// **A copy keeps its claim, and the claim stays true.** `method(o.variantSet,
/// o.rounding, o.constraintSet)`, rebuilt from an overlaid method `o`, still
/// holds the jurisdiction's constraints, and its verdicts still say so, as a
/// copied `RoundingRule` still says whose rule it is. Three routes remain
/// that no type can close:
///
///  - `method(o.variantSet, o.rounding, o.constraintSet.constraintSet())`
///    hands the jurisdiction's constraints over as a plain set, one call, and
///    makes them the new method's own. Reading the constraints has to be
///    possible, and a plain set says nothing of any jurisdiction;
///  - reinterpreting an object's bytes (`std::bit_cast` at run time) makes it
///    anything;
///  - explicitly specialising `OverlaidConstraints` over a predicate of the
///    program's own types gives a class of this name with whatever members
///    the specialisation declares. Specialising a library template is outside
///    this library's contract, and no code can forbid it.
template <Predicate... Ps>
class OverlaidConstraints
{
  public:
    /// Refused: see `detail::RequireLibraryStatesConstraintProvenance`.
    /// Declared only so that `OverlaidConstraints<...> { constraints(...), c }`
    /// is refused in this library's words rather than the compiler's. The
    /// members are initialised so that the refusal is the only message: a set
    /// holding a lookup has no default to fall back on.
    constexpr OverlaidConstraints(ConstraintSet<Ps...> replacement,
                                  Citation cited = {},
                                  detail::ProvenanceStatedByAuthor = {}) noexcept:
        _constraintSet { replacement },
        _source { cited }
    {
        static_assert(detail::RequireLibraryStatesConstraintProvenance<OverlaidConstraints>::value);
    }

    /// The constraints, as the overlay stated them.
    [[nodiscard]] constexpr ConstraintSet<Ps...> const& constraintSet() const noexcept
    {
        return _constraintSet;
    }

    /// What the overlay cited for them; empty when it cited nothing.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

  private:
    friend struct detail::ConstraintOriginAccess;

    /// The overlay's constraints, citing @p cited -- reachable only through
    /// `detail::ConstraintOriginAccess`.
    constexpr OverlaidConstraints(detail::ConstraintsOverlaid, ConstraintSet<Ps...> replacement, Citation cited) noexcept:
        _constraintSet { replacement },
        _source { cited }
    {
    }

    // Deliberately no `{}` default member initialiser: the set holds
    // predicates, which hold expressions -- see `Corrections` (`lookup.hpp`).
    ConstraintSet<Ps...> _constraintSet;
    Citation _source {};
};

/// Whose constraints a method checks, and what the overlay that supplied them
/// cited, as `check_method` tells a sink and `constraint_origin` answers:
/// read from the method's constraint part, a plain `ConstraintSet` being the
/// method's own and an `OverlaidConstraints` a jurisdiction's. It is a value
/// read off constraints, not stored beside them, so it cannot be put next to
/// constraints it does not describe.
///
/// Default-constructed it is the method's own, and only the library makes one
/// a jurisdiction's, short of reinterpreting its bytes (`std::bit_cast` at run
/// time); the constructor that would let an author state one is
/// refused in the library's words. The guard does not reach a sink's own
/// hooks: they are public members, and code that calls them by hand writes
/// whatever trace it likes, as it could by calling `variant_entered`.
class ConstraintOrigin
{
  public:
    /// The method's own constraints.
    constexpr ConstraintOrigin() noexcept = default;

    /// Refused: see `detail::RequireLibraryStatesConstraintProvenance`.
    /// Declared only so that `ConstraintOrigin { ConstraintProvenance::JurisdictionOverlay, c }`
    /// is refused in this library's words rather than the compiler's. A
    /// template, so that the refusal waits until it is called.
    template <typename Provenance>
        requires std::is_same_v<Provenance, ConstraintProvenance>
    constexpr ConstraintOrigin(Provenance, Citation = {}, detail::ProvenanceStatedByAuthor = {}) noexcept
    {
        static_assert(detail::RequireLibraryStatesConstraintProvenance<Provenance>::value);
    }

    /// Whose constraints they are: the method's own, unless an overlay's
    /// `with_constraints` replaced them.
    [[nodiscard]] constexpr ConstraintProvenance provenance() const noexcept
    {
        return _provenance;
    }

    /// What the overlay that replaced the constraints cited; empty when it
    /// cited nothing, and for the method's own.
    [[nodiscard]] constexpr Citation const& source() const noexcept
    {
        return _source;
    }

  private:
    friend struct detail::ConstraintOriginAccess;

    /// An overlay's constraints, citing @p cited -- reachable only through
    /// `detail::ConstraintOriginAccess`.
    constexpr ConstraintOrigin(detail::ConstraintsOverlaid, Citation cited) noexcept:
        _provenance { ConstraintProvenance::JurisdictionOverlay },
        _source { cited }
    {
    }

    ConstraintProvenance _provenance = ConstraintProvenance::MethodOwn;
    Citation _source {};
};

namespace detail
{
    struct ConstraintOriginAccess
    {
        /// @p replacement as a jurisdiction overlay's constraints, citing
        /// @p source.
        template <Predicate... Ps>
        [[nodiscard]] static constexpr OverlaidConstraints<Ps...> overlaid(ConstraintSet<Ps...> const& replacement,
                                                                           Citation source) noexcept
        {
            return OverlaidConstraints<Ps...> { ConstraintsOverlaid {}, replacement, source };
        }

        /// A method's own constraints.
        template <Predicate... Ps>
        [[nodiscard]] static constexpr ConstraintOrigin of(ConstraintSet<Ps...> const&) noexcept
        {
            return ConstraintOrigin {};
        }

        /// A jurisdiction's constraints, citing what its overlay cited.
        template <Predicate... Ps>
        [[nodiscard]] static constexpr ConstraintOrigin of(OverlaidConstraints<Ps...> const& overlaidSet) noexcept
        {
            return ConstraintOrigin { ConstraintsOverlaid {}, overlaidSet.source() };
        }
    };

    /// The `ConstraintSet` a method's constraint part holds, whichever kind of
    /// part it is -- what `check_all` and the overlay's rewrite work on.
    template <Predicate... Ps>
    [[nodiscard]] constexpr ConstraintSet<Ps...> const& constraint_set_of(ConstraintSet<Ps...> const& ownSet) noexcept
    {
        return ownSet;
    }

    template <Predicate... Ps>
    [[nodiscard]] constexpr ConstraintSet<Ps...> const& constraint_set_of(
        OverlaidConstraints<Ps...> const& overlaidSet) noexcept
    {
        return overlaidSet.constraintSet();
    }

    /// The `ConstraintSet` type a method's constraint part holds; the part
    /// itself for anything else, so that a malformed method's part reaches
    /// the shape rules unchanged.
    template <typename Constraints>
    struct PlainConstraintsOf
    {
        /// The part itself.
        using type = Constraints;
    };

    template <Predicate... Ps>
    struct PlainConstraintsOf<OverlaidConstraints<Ps...>>
    {
        /// The jurisdiction's set.
        using type = ConstraintSet<Ps...>;
    };

    template <typename Constraints>
    using PlainConstraints = typename PlainConstraintsOf<Constraints>::type;
} // namespace detail

namespace detail
{
    /// Whether a type is a `Variants` pack.
    template <typename T>
    struct IsVariants: std::false_type
    {
    };

    template <typename... Cs>
    struct IsVariants<Variants<Cs...>>: std::true_type
    {
    };

    /// Whether a type is a `ConstraintSet`.
    template <typename T>
    struct IsConstraintSet: std::false_type
    {
    };

    template <Predicate... Ps>
    struct IsConstraintSet<ConstraintSet<Ps...>>: std::true_type
    {
    };

    /// Whether a type can be a method's constraint part: the method's own
    /// `ConstraintSet`, or the `OverlaidConstraints` a jurisdiction's overlay
    /// put in its place.
    template <typename T>
    struct IsMethodConstraints: IsConstraintSet<T>
    {
    };

    template <Predicate... Ps>
    struct IsMethodConstraints<OverlaidConstraints<Ps...>>: std::true_type
    {
    };

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
        /// Whether any variant's expression was refused -- see
        /// `detail::refused_already`.
        static constexpr detail::RefusedFlag refused = refused_already<Expr>() || (refused_already<Exprs>() || ...);
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
        static_assert(refused_already<VariantsDimension<Vs>>() || Rounding::unit.dimension == VariantsDimension<Vs>::dimension,
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
    /// rule, twice over. A `Rounding` that is not a `RoundingRule` has no
    /// `unit`, and a `Vs` that is not a `Variants` has no dimension; the shape
    /// rules in `RequireWellFormedMethod` refuse both, by name.
    /// `method_arguments_out_of_order_rule_last.cpp` pins that half. And a pack of
    /// variants that disagree is already refused by `Variants`: asking this
    /// rule as well would add a second error -- measured against whichever
    /// variant happened to be first -- to the one that names the mistake.
    /// `method_rounding_rule_gated.cpp` pins that half, and
    /// `method_rounding_rule_gated_not_a_variant.cpp` pins the same gate for a
    /// pack that holds a non-variant. Each case pins its route by refusing
    /// any output that names this rule.
    template <typename Vs, typename Rounding>
    inline constexpr bool canAskRoundingRule = IsRoundingRule<Rounding>::value && VariantsDimension<Vs>::known;

    /// The rules a method obeys, asked in an order that matters.
    ///
    /// First its shape: the variants, the rounding rule and the constraints,
    /// each the kind of thing its factory builds. `method(...)` takes three
    /// arguments of unrelated types, so nothing stops an author passing them
    /// in the wrong order. Before these rules,
    /// `method(rounding_rule<...>(), variants(...), constraints())` compiled
    /// on cl 19.51 and clang-cl 22, and failed only at `evaluate_method`, with
    /// the compiler's own words for it: cl's `C2027: use of undefined type
    /// SelectVariant<...>`, clang-cl's "implicit instantiation of undefined
    /// template". Each rule names the
    /// part it refuses, so a swapped pair is reported as the two parts that
    /// are wrong.
    ///
    /// Then the rounding rule's dimension, only once there is a rule and an
    /// agreed dimension to compare -- see `canAskRoundingRule`.
    template <typename Vs, typename Rounding, typename Constraints>
    struct RequireWellFormedMethod
    {
        static_assert(IsVariants<Vs>::value,
                      "formula: this method's variants are not a variants pack; a method is "
                      "method(variants(...), rounding_rule<...>(), constraints(...)), in that order -- "
                      "the offending type appears in this diagnostic as the template argument Vs of "
                      "RequireWellFormedMethod");
        static_assert(IsRoundingRule<Rounding>::value,
                      "formula: this method's rounding rule is not a rounding rule; a method is "
                      "method(variants(...), rounding_rule<...>(), constraints(...)), in that order -- "
                      "the offending type appears in this diagnostic as the template argument Rounding "
                      "of RequireWellFormedMethod");
        static_assert(IsMethodConstraints<Constraints>::value,
                      "formula: this method's constraints are not a constraint set; a method is "
                      "method(variants(...), rounding_rule<...>(), constraints(...)), in that order -- "
                      "the offending type appears in this diagnostic as the template argument "
                      "Constraints of RequireWellFormedMethod");

        static_assert(std::conditional_t<canAskRoundingRule<Vs, Rounding>,
                                         RequireRoundingRuleMeasuresVariants<Vs, Rounding>,
                                         std::true_type>::value);

        static constexpr bool value = true;
    };
} // namespace detail

/// One method: the variants it chooses between, the rounding rule it applies
/// to whichever one is chosen, and the constraints it checks.
///
/// `constraintSet` is the `ConstraintSet` exactly as `constraints(...)` built
/// it, held as an ordinary member and never unpacked, so that it can be
/// handed straight to `check_all()`; `ConstraintSet` in `constraint.hpp` says
/// why that is the shape. After an overlay's `with_constraints` it is an
/// `OverlaidConstraints` instead, which carries whose constraints they are
/// with them -- see `OverlaidConstraints` -- and `check_method` tells a sink
/// so.
///
/// Each part must be the kind of thing its factory builds, and the rounding
/// rule's unit must measure the variants' dimension -- see
/// `detail::RequireWellFormedMethod`. That is checked in this class body
/// rather than only in `method()` below, for the reason `Variants` gives for
/// its own checks: this is a public aggregate, so a `Method<...>` can be
/// declared with no factory call.
template <typename Vs, typename Rounding, typename Constraints>
struct Method
{
    // `remove_cv_t` because `Method<decltype(pack), ...>` over `constexpr`
    // parts names `const Variants<...>` and the like, which no specialisation
    // of the shape traits or of `VariantsDimension` matches: the shape rules
    // would refuse a well-formed method, and the dimension rule would read an
    // unmatched pack as "nothing to ask" and switch off without a word.
    static_assert(detail::RequireWellFormedMethod<std::remove_cv_t<Vs>,
                                                  std::remove_cv_t<Rounding>,
                                                  std::remove_cv_t<Constraints>>::value);

    /// The variants, as `variants(...)` built them.
    ///
    /// Deliberately no `{}` default member initialiser. With one, no method
    /// fails to compile, but asking whether this type -- or a tuple holding
    /// it -- is default-constructible hard-errors or answers `true` when a
    /// lookup is inside; see `Corrections` (`lookup.hpp`).
    Vs variantSet;
    /// The rule applied to the selected variant's result.
    Rounding rounding {};
    /// The constraints, as `constraints(...)` built them, or as an overlay's
    /// `with_constraints` replaced them (`OverlaidConstraints`).
    Constraints constraintSet;
};

namespace detail
{
    /// Whether a type is a `Method`, well formed or not.
    template <typename M>
    struct IsMethod: std::false_type
    {
    };

    template <typename Vs, typename Rounding, typename Constraints>
    struct IsMethod<Method<Vs, Rounding, Constraints>>: std::true_type
    {
    };

    /// Whether every variant in a pack has a tag the tag rule accepts, asked
    /// without firing it.
    template <typename Vs>
    struct VariantTagsArePlain: std::false_type
    {
    };

    template <typename... Tags, Node... Exprs>
    struct VariantTagsArePlain<Variants<VariantCase<Tags, Exprs>...>>: std::bool_constant<(isPlainClassTag<Tags> && ...)>
    {
    };

    /// Whether no two variants in a pack declare the same tag, asked without
    /// firing `RequireDistinctVariantTags`.
    ///
    /// Matches the pack by its type alone, so it never completes the
    /// `Variants` it is asked about -- which is what lets it answer false,
    /// where `tags_are_distinct` below cannot: a pack that repeats a tag
    /// fails to compile the moment it is completed.
    template <typename Vs>
    struct VariantTagsAreDistinct: std::false_type
    {
    };

    template <typename... Tags, Node... Exprs>
    struct VariantTagsAreDistinct<Variants<VariantCase<Tags, Exprs>...>>: std::bool_constant<all_distinct<Tags...>()>
    {
    };

    /// True when no two variants declare the same tag. A method with two
    /// variants for one tag has no answer to "which one applies", and picking
    /// the first would make the second silently dead code.
    ///
    /// Asked of a pack that exists, so it is true of every pack it can be
    /// asked about: a `Variants` that repeats a tag is refused where it is
    /// completed, by `RequireDistinctVariantTags`. It states the rule for a
    /// reader and for a test's control; `VariantTagsAreDistinct` is the form
    /// that can answer false.
    template <typename... Cs>
    [[nodiscard]] constexpr bool tags_are_distinct(Variants<Cs...> const&) noexcept
    {
        return all_distinct<typename Cs::tag...>();
    }

    /// Whether a rounding rule's unit measures a pack's agreed dimension,
    /// asked without firing `RequireRoundingRuleMeasuresVariants`. Only ever
    /// instantiated behind the checks that give both sides a meaning -- see
    /// `IsWellFormedMethod`.
    template <typename Vs, typename Rounding>
    struct RoundingRuleMeasuresVariants: std::bool_constant<Rounding::unit.dimension == VariantsDimension<Vs>::dimension>
    {
    };

    /// Whether a `Method` passes every rule its class body asks, and every
    /// rule its variants ask, asked without firing any of them.
    ///
    /// `std::conjunction` rather than `&&`, because it stops instantiating at
    /// the first false member: `RoundingRuleMeasuresVariants` reads
    /// `Rounding::unit`, which only a `RoundingRule` has, so it must not be
    /// instantiated for a method whose parts are in the wrong order.
    template <typename M>
    struct IsWellFormedMethod: std::false_type
    {
    };

    template <typename Vs, typename Rounding, typename Constraints>
    struct IsWellFormedMethod<Method<Vs, Rounding, Constraints>>:
        std::conjunction<IsVariants<std::remove_cv_t<Vs>>,
                         IsRoundingRule<std::remove_cv_t<Rounding>>,
                         IsMethodConstraints<std::remove_cv_t<Constraints>>,
                         std::bool_constant<VariantsDimension<std::remove_cv_t<Vs>>::known>,
                         VariantTagsArePlain<std::remove_cv_t<Vs>>,
                         VariantTagsAreDistinct<std::remove_cv_t<Vs>>,
                         RoundingRuleMeasuresVariants<std::remove_cv_t<Vs>, std::remove_cv_t<Rounding>>>
    {
    };
} // namespace detail

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
        for (std::size_t caseIndex = 0; caseIndex < sizeof...(Cs); ++caseIndex)
            if (matches[caseIndex])
                return caseIndex;
        return 0;
    }

    /// The first two positions of @p names holding the same text, ordered by
    /// the later position; `{ 0, 0 }` when there are none -- the rule
    /// `first_repeated_pair` states for types, for the names a trace shows.
    template <std::size_t Count>
    [[nodiscard]] consteval PositionPair first_repeated_name(std::array<std::string_view, Count> const& names) noexcept
    {
        for (std::size_t later = 1; later < Count; ++later)
            for (std::size_t earlier = 0; earlier < later; ++earlier)
                if (names[earlier] == names[later])
                    return PositionPair { earlier, later };
        return PositionPair {};
    }

    /// Fails to compile when two variants of one method are spelt the same
    /// wherever the library shows a tag -- `TagName<Cube>` spelling
    /// `"Cylinder"` beside a real `Cylinder`, say. The tags are distinct
    /// types (`RequireTagDeclaredOnce`), but a trace line naming the variant
    /// that ran could not tell a reader which one it was but by its position.
    ///
    /// Templated on both positions and both tags, for the reason
    /// `RequireTagDeclaredOnce` is: the diagnostic names the mistake outright.
    template <std::size_t First, std::size_t Second, typename FirstTag, typename SecondTag>
    struct RequireTagNamesDistinct
    {
        static_assert(First == Second,
                      "formula: two variants of this method are spelt the same in a trace, so a line naming the "
                      "variant that ran could not say which of them it was -- the tags appear in this diagnostic "
                      "as the template arguments FirstTag and SecondTag of RequireTagNamesDistinct, and First and "
                      "Second are the ZERO-BASED positions of their variants; spell them apart with "
                      "formula::TagName");

        static constexpr bool value = true;
    };

    /// True when no two variants of a pack are spelt the same by `tag_name`;
    /// the refusal is `RequireTagNamesDistinct`'s. Asked by `evaluate_method`,
    /// where the names are shown, of every variant of the pack -- so a
    /// `TagName` of any of them is read the first time the method is
    /// evaluated with any tag.
    template <typename Vs>
    struct RequireDistinctTagNames;

    template <typename... Cs>
    struct RequireDistinctTagNames<Variants<Cs...>>
    {
        static constexpr std::array<std::string_view, sizeof...(Cs)> names { tag_name<typename Cs::tag>()... };
        static constexpr PositionPair repeated = first_repeated_name(names);

        static_assert(
            RequireTagNamesDistinct<repeated.first,
                                    repeated.second,
                                    typename std::tuple_element_t<repeated.first, std::tuple<Cs...>>::tag,
                                    typename std::tuple_element_t<repeated.second, std::tuple<Cs...>>::tag>::value);

        static constexpr bool value = true;
    };

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

/// A method's rounding rule, applied: the selected variant rounded as a
/// `RoundNode` rounds it, carrying where the rule came from.
///
/// **It rounds exactly as a `RoundNode` does**, and derives from one, so the
/// unit, the places and the mode are stated once. What it adds is provenance.
/// A method's rule used to be applied through an ordinary `rounded<>` node,
/// traced as an ordinary `Round` step, which says to how many places a value
/// was rounded but not whose rule that was: the method's author's, or a
/// jurisdiction's. A trace records this node as a
/// `StepKind::RoundingRuleApplied` step (`trace.hpp`), which says both.
///
/// **Only `evaluate_method` builds one**, around the variant it selected and
/// from the method's own rule. The node holds that `RoundingRule` itself
/// rather than a provenance and a citation of its own, and its building
/// constructor is private, reachable only through
/// `detail::RoundingRuleNodeAccess`; the public one refuses, in this library's
/// words. So a trace's "method default" or "jurisdiction overlay" is always
/// said of a method's rule -- and it is exactly as true as that rule's
/// provenance, which only the library creates (see
/// `detail::RequireLibraryStatesProvenance`). A method holding a copy of an
/// overlay's rule, `method(..., apply(overlay, m).rounding, ...)`, is traced
/// as that overlay's rule, which is true of it -- see `RoundingRule`.
template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
class RoundingRuleNode: public RoundNode<U, Places, Mode, Operand>
{
  public:
    /// Refused: see `detail::RequireMethodMadeRoundingNode`. Declared only so
    /// that building one by hand is refused in this library's words.
    explicit constexpr RoundingRuleNode(Operand rounded,
                                        RoundingRule<U, Places, Mode> const& ruleApplied = {},
                                        detail::ProvenanceStatedByAuthor = {}) noexcept:
        RoundNode<U, Places, Mode, Operand> { {}, rounded },
        _rule { ruleApplied }
    {
        static_assert(detail::RequireMethodMadeRoundingNode<RoundingRuleNode>::value);
    }

    /// The rule applied: its granularity is this node's, and its provenance
    /// and citation are what a trace reports of it.
    [[nodiscard]] constexpr RoundingRule<U, Places, Mode> const& rule() const noexcept
    {
        return _rule;
    }

  private:
    friend struct detail::RoundingRuleNodeAccess;

    constexpr RoundingRuleNode(detail::MethodMade,
                               Operand rounded,
                               RoundingRule<U, Places, Mode> const& ruleApplied) noexcept:
        RoundNode<U, Places, Mode, Operand> { {}, rounded },
        _rule { ruleApplied }
    {
    }

    RoundingRule<U, Places, Mode> _rule;
};

namespace detail
{
    /// The one way to build a `RoundingRuleNode`: `evaluate_method` is its only
    /// caller.
    struct RoundingRuleNodeAccess
    {
        /// @p operand rounded by @p rule.
        template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
        [[nodiscard]] static constexpr RoundingRuleNode<U, Places, Mode, Operand> applied(
            Operand operand, RoundingRule<U, Places, Mode> const& rule) noexcept
        {
            return RoundingRuleNode<U, Places, Mode, Operand> { MethodMade {}, operand, rule };
        }
    };
} // namespace detail

/// Rounds as the `RoundNode` it derives from, and tells @p sink about the
/// node as its own type, so that a trace can say where the rule came from.
///
/// Chosen over the `RoundNode` overload in `rounding_node.hpp` because binding
/// the node to its own type is an identity conversion and binding it to its
/// base is not -- the reason `OverriddenConstantNode`'s overload
/// (`overlay.hpp`) is chosen over `VarNode`'s.
template <typename Rep = Rational,
          Unit U,
          DecimalPlaces Places,
          RoundingMode Mode,
          Node Operand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RoundingRuleNode<U, Places, Mode, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    return detail::round_to_places<Rep>(node, environment, sink);
}

/// Evaluates the variant of @p m tagged `Tag`, rounded by @p m's own rounding
/// rule, in the coherent SI unit of its dimension -- the same unit every
/// `Evaluated<Rep>` in this library is in.
///
/// `Tag` is never deduced: which variant applies is a property of the
/// specimen, stated by the caller, never inferred from a number. A tag no
/// variant declares is refused at compile time rather than answered with a
/// fallback -- see `detail::RequireVariantForTag`.
///
/// **The selection is recorded, not merely made.** A sink that defines
/// `variant_entered` and `variant_produced` is told which variant fired --
/// its tag's name, its position and how many there were -- around the
/// evaluation of that variant, so a `RecordingSink` (`trace.hpp`) records a
/// `StepKind::VariantSelected` step whose one operand is the rounded
/// variant. That is the answer to "why the cylinder formula?" that spec
/// section 9.1 asks for; see `VariantSelection` (`sink.hpp`).
///
/// The rounding is a `RoundingRuleNode` wrapped around the selected
/// expression, so it rounds in the rule's unit exactly as
/// `rounded<U, Places, Mode>(...)` does, and a sink is told where the rule
/// came from: the method's own, or a jurisdiction's overlay. That is also why
/// `Rep = double` is refused here as it is for `rounded<>` -- see
/// `RepRounding<double>`.
///
/// A malformed `Method` is refused where it is declared, and this body is
/// then not instantiated at all, so that evaluating one adds nothing to that
/// refusal. Without the gate, measured on clang-cl 22, clang++ 20 and g++ 13:
/// a rounding rule of the wrong dimension was reported a second time by the
/// rounding node this body builds, and parts in the wrong order added the
/// compiler's own error for the undefined `SelectVariant` it names. cl 19.51
/// reported neither, gate or no gate -- it did not go on into this body once
/// the class had failed -- so on cl the gate is invisible and the two cases
/// below pass either way.
/// `method_evaluate_rounding_rule_dimension_mismatch.cpp` and
/// `method_evaluate_arguments_out_of_order.cpp` pin the gate by refusing
/// that second message and that template's name. `if constexpr` rather than
/// the `std::conditional_t` the class bodies above use, because this is a
/// function body, where a discarded statement is exactly what is wanted.
///
/// Anything that is not a `Method` at all still reaches the body, and is
/// refused there in the compiler's own words, as it always was.
template <typename Tag, typename Rep = Rational, typename M, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> evaluate_method(M const& m, Env const& environment, Sink sink = {}) noexcept
{
    if constexpr (detail::IsMethod<M>::value && !detail::IsWellFormedMethod<M>::value)
    {
        // Unreachable: the declaration of `M` has already failed to compile.
        static_cast<void>(m);
        static_cast<void>(environment);
        static_cast<void>(sink);
        return {};
    }
    else
    {
        using Selection = detail::SelectVariant<Tag, std::remove_cvref_t<decltype(m.variantSet)>>;
        using Rule = std::remove_cvref_t<decltype(m.rounding)>;

        // The locals here and in `check_method` have names no consumer's
        // global is likely to share: a local hiding a global is C4459 on cl
        // at /W4, raised in this header but in the consumer's build -- see
        // `method_consumer_globals_tests.cpp`.
        auto const& selectedCase = std::get<Selection::index>(m.variantSet.cases);
        using Selected = std::remove_cvref_t<decltype(selectedCase.expression)>;
        RoundingRuleNode<Rule::unit, Rule::places, Rule::mode, Selected> const roundedVariant =
            detail::RoundingRuleNodeAccess::applied<Rule::unit, Rule::places, Rule::mode, Selected>(selectedCase.expression,
                                                                                                    m.rounding);

        // The selection is named whether or not the sink asks for it, so that
        // a broken `TagName` specialization, or a tag whose name cannot be
        // shown, is refused the first time the method is evaluated with that
        // tag, traced or not. The position and count are the method's as
        // published, which an overlay may differ from -- see
        // `Variants::published`.
        constexpr std::string_view selectedTagName = tag_name<Tag>();
        static_assert(detail::RequireDistinctTagNames<std::remove_cvref_t<decltype(m.variantSet)>>::value);
        VariantSelection const variantSelection {
            selectedTagName,
            m.variantSet.published.position(Selection::index),
            m.variantSet.published.count(),
            m.variantSet.published.pruned_count(),
            m.variantSet.published.pruned_count() == 0 ? nullptr : &m.variantSet.published.pruned_by(),
            m.variantSet.published.pinned() ? &m.variantSet.published.pinned_by() : nullptr,
        };
        if constexpr (requires(Evaluated<Rep> const& variantResult) {
                          sink.variant_entered(variantSelection);
                          sink.variant_produced(variantSelection, variantResult);
                      })
        {
            sink.variant_entered(variantSelection);
            Evaluated<Rep> const variantResult = detail::dispatch<Rep>(roundedVariant, environment, sink);
            sink.variant_produced(variantSelection, variantResult);
            return variantResult;
        }
        else
            return detail::dispatch<Rep>(roundedVariant, environment, sink);
    }
}

/// Checks every constraint of @p m against @p environment, and returns one
/// `ConstraintOutcome` per constraint at the index it holds -- exactly
/// `check_all` over the set the method holds, handed over whole, never
/// unpacked: `check_all(m.constraintSet, environment)` for a method's own
/// constraints, `check_all(m.constraintSet.constraintSet(), environment)`
/// for a jurisdiction's (`OverlaidConstraints`). So the size of the result is
/// the number of constraints the method holds: an overlay's
/// `with_constraints` that replaced one constraint with three makes it three.
///
/// **Whose constraints they are is told, not merely known.** A sink that
/// defines `acceptance_entered` and `acceptance_produced` is told the method's
/// `ConstraintOrigin` around the checks -- both or neither, asked in one
/// `requires`, as `evaluate_method` asks for its pair -- so a `RecordingSink`
/// (`trace.hpp`) records a `StepKind::AcceptanceChecked` step whose operands
/// are the constraints' verdicts, in the order of the result, and marks each
/// verdict with whose it was. That step is recorded even for a method with no
/// constraints at all, so a trace of a method whose overlay removed every
/// check says so, and by whose authority.
///
/// A malformed `Method` is refused where it is declared, and this body is
/// then not instantiated, for the reason `evaluate_method` gives.
template <typename Rep = Rational, typename M, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr auto check_method(M const& m, Env const& environment, Sink sink = {}) noexcept
{
    if constexpr (detail::IsMethod<M>::value && !detail::IsWellFormedMethod<M>::value)
    {
        // Unreachable: the declaration of `M` has already failed to compile.
        static_cast<void>(m);
        static_cast<void>(environment);
        static_cast<void>(sink);
        return std::array<ConstraintOutcome, 0> {};
    }
    else
    {
        ConstraintOrigin const acceptanceOrigin = detail::ConstraintOriginAccess::of(m.constraintSet);
        if constexpr (requires {
                          sink.acceptance_entered(acceptanceOrigin);
                          sink.acceptance_produced(acceptanceOrigin);
                      })
        {
            sink.acceptance_entered(acceptanceOrigin);
            auto const acceptanceOutcomes = check_all<Rep>(detail::constraint_set_of(m.constraintSet), environment, sink);
            sink.acceptance_produced(acceptanceOrigin);
            return acceptanceOutcomes;
        }
        else
            return check_all<Rep>(detail::constraint_set_of(m.constraintSet), environment, sink);
    }
}

/// Whose constraints @p m checks, and what the overlay that supplied them
/// cited: what `check_method` tells a sink. Read from the constraint part
/// itself, so it is true of whatever method holds that part -- see
/// `OverlaidConstraints`.
template <typename Vs, typename Rounding, typename Constraints>
[[nodiscard]] constexpr ConstraintOrigin constraint_origin(Method<Vs, Rounding, Constraints> const& m) noexcept
{
    return detail::ConstraintOriginAccess::of(m.constraintSet);
}

} // namespace formula
