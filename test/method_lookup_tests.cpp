// SPDX-License-Identifier: Apache-2.0
//
// A method holding a lookup, of each kind, at a variant's root, under every
// member that holds a child expression, and in a constraint.
//
// Its own file because it is a portability regression: `std::tuple`, which
// holds a method's variants and constraints, asks whether each element is
// default-constructible, and a lookup used to answer by instantiating the
// body of `Corrections<N>`'s wrong-count constructor -- whose `static_assert`
// then refused a perfectly good method. Measured on clang++ 20 with
// libstdc++ 14 and on g++ 14 for a method of one variant, and on g++ 13 and
// clang++ 20 with libc++ 22 once it had two; cl and clang-cl accepted it.
// Most rows below fail on every one of those six toolchains when a member
// regains its `{}`; the exceptions, and why, are in `Corrections` in
// `lookup.hpp`. On cl it is the `refuses_default_construction` rows that
// fail: cl compiles the method, but answers the trait wrongly.
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Cylinder
{
};

struct Ratio: formula::Quantity<Ratio, "r", "slenderness ratio", unit::One>
{
};

// A name of its own, not the `Shape` `overlay_tests.cpp` uses with a table of
// the same values: two translation units' anonymous-namespace key
// enumerations of one name, with tables of the same values, fail to link on
// clang -- see `lookup.hpp`.
enum class LookupShape : std::uint8_t
{
    Square,
    Round,
};

inline constexpr formula::KeyTable<LookupShape, 2> Keys { LookupShape::Square, LookupShape::Round };
inline constexpr formula::BandTable<2> Bands { formula::band(0, 1, 193, 100), formula::band(193, 100, 437, 100) };
inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(0), formula::breakpoint(713, 100) };
// Tables of no rows, whose `Corrections<0>` IS default-constructible: the one
// case in which a lookup's own `operand` is reached by the probe at all.
inline constexpr formula::BandTable<0> NoBands {};
inline constexpr formula::BreakpointTable<0> NoPoints {};

// Every lookup below answers a different number for a ratio of 3, and none of
// them is 3 itself, so a method that evaluated the wrong variant, the wrong
// row or the bare operand cannot pass.
[[nodiscard]] constexpr auto banded()
{
    // 3 falls in [1.93, 4.37), the second band: 3.71.
    return formula::banded_lookup<unit::One, Bands, unit::One>(
        var<Ratio>, { formula::Rational { 863, 1000 }, formula::Rational { 371, 100 } });
}

[[nodiscard]] constexpr auto interpolating()
{
    // 300/713 of the way from 0.781 to 1.494: 1.081.
    return formula::interpolating_lookup<unit::One, Points, unit::One>(
        var<Ratio>, { formula::Rational { 781, 1000 }, formula::Rational { 1494, 1000 } });
}

[[nodiscard]] constexpr auto exact()
{
    // The second key's row: 2.917.
    return formula::exact_lookup<Keys, unit::One>(LookupShape::Round,
                                                  { formula::Rational { 1093, 1000 }, formula::Rational { 2917, 1000 } });
}

// Nine places: no answer below has more than six, so the rule leaves every
// one as it is, with room for an edit that adds a digit.
[[nodiscard]] constexpr auto unrounded()
{
    return formula::rounding_rule<unit::One, formula::DecimalPlaces { 9 }, formula::RoundingMode::HalfAwayFromZero>();
}

inline constexpr auto ratioOfThree = formula::environment(formula::Measured<Ratio> { formula::Rational { 3 } });

template <typename Tag, typename M>
[[nodiscard]] constexpr formula::Rational answer(M const& m)
{
    return formula::evaluate_method<Tag>(m, ratioOfThree)->value();
}

[[nodiscard]] constexpr auto exactPerfectSquare()
{
    // A perfect square, for the root row below: the second key's row, 0.7569.
    return formula::exact_lookup<Keys, unit::One>(LookupShape::Round,
                                                  { formula::Rational { 1093, 1000 }, formula::Rational { 7569, 10000 } });
}

// Whether @p expression survives being a method's variant: it compiles in
// one, and the method answers @p expected for it. TWO variants, both holding
// it, because g++ 13 and clang++ with libc++ only asked their question of a
// tuple of two or more elements, where g++ 14 and clang++ with libstdc++
// asked it of one.
template <formula::Node Expr>
[[nodiscard]] constexpr bool survives_a_method(Expr expression, formula::Rational expected)
{
    auto const m =
        formula::method(formula::variants(formula::variant<Cube>(expression), formula::variant<Cylinder>(expression)),
                        unrounded(),
                        formula::constraints());
    return answer<Cube>(m) == expected && answer<Cylinder>(m) == expected;
}

// Whether a method holding @p expression compiles and, evaluated, misses:
// what a lookup over an EMPTY table must do, since no row can match.
template <formula::Node Expr>
[[nodiscard]] constexpr bool misses_in_a_method(Expr expression)
{
    auto const m =
        formula::method(formula::variants(formula::variant<Cube>(expression), formula::variant<Cylinder>(expression)),
                        unrounded(),
                        formula::constraints());
    return !formula::evaluate_method<Cube>(m, ratioOfThree).has_value();
}

// Whether @p expression, and a variant holding it, say -- in every standard
// spelling of the question -- that they cannot be default-constructed: the
// question `std::tuple` asks. The line that pins a restored `{}` on cl,
// which compiles the method itself but then answers this `true`.
template <formula::Node Expr>
[[nodiscard]] constexpr bool refuses_default_construction(Expr)
{
    using Case = formula::VariantCase<Cube, Expr>;
    return !std::is_default_constructible_v<Expr> && !std::default_initializable<Expr>
           && !std::is_default_constructible_v<Case> && !std::default_initializable<Case>;
}
} // namespace

TEST_CASE("a method whose variant is a banded lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(banded())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 371, 100 });
}

TEST_CASE("a method whose variant is an interpolating lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(interpolating())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 1081, 1000 });
}

TEST_CASE("a method whose variant is an exact lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(exact())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 2917, 1000 });
}

TEST_CASE("a method evaluates a lookup nested inside a variant's expression", "[method][lookup]")
{
    // The spec's own shape: a measured quantity scaled by a tabulated
    // correction, 3 * 2.917.
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(var<Ratio> * exact())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 8751, 1000 });
}

TEST_CASE("a method selects between variants that are both lookups", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(banded()), formula::variant<Cylinder>(interpolating())),
                        unrounded(),
                        formula::constraints());
    STATIC_REQUIRE(answer<Cube>(m) == formula::Rational { 371, 100 });
    STATIC_REQUIRE(answer<Cylinder>(m) == formula::Rational { 1081, 1000 });
}

TEST_CASE("a method's constraint may hold a lookup", "[method][lookup]")
{
    // `ConstraintSet` is a tuple as well, and so asks the same question of
    // each `Constraint` that `Variants` asks of each variant.
    constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Ratio>)),
                                       unrounded(),
                                       formula::constraints(formula::constraint(
                                           var<Ratio> < banded(), formula::Verdict { "ratio above the band's limit" })));
    constexpr auto outcomes = formula::check_all(m.constraintSet, ratioOfThree);
    STATIC_REQUIRE(outcomes[0].is_satisfied());
    STATIC_REQUIRE(answer<Cube>(m) == formula::Rational { 3 });
}

TEST_CASE("a lookup, and anything holding one, says it cannot be default-constructed", "[method][lookup]")
{
    // A table's corrections have to be stated, so none of these can be
    // default-constructed -- and every one of them now SAYS so, rather than
    // answering yes (the traits) or failing to compile (the concepts), which
    // is what a trait probe inside `std::tuple` tripped over.
    using Banded = decltype(banded());
    using Nested = decltype(var<Ratio> * exact());
    using Case = decltype(formula::variant<Cube>(var<Ratio> * exact()));

    STATIC_REQUIRE(!std::is_default_constructible_v<Banded>);
    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(interpolating())>);
    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(exact())>);
    STATIC_REQUIRE(!std::default_initializable<Banded>);
    STATIC_REQUIRE(!std::is_default_constructible_v<Nested>);
    STATIC_REQUIRE(!std::is_default_constructible_v<Case>);

    // The control: a lookup is still copyable, which every factory and every
    // tuple above relies on.
    STATIC_REQUIRE(std::is_copy_constructible_v<Banded>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Case>);
}

TEST_CASE("a lookup survives a method under every member that holds a child expression", "[method][lookup]")
{
    // One row per node member declared without a `{}` default member
    // initialiser so that a method can hold a lookup -- see `Corrections` in
    // `lookup.hpp`. Each row puts a lookup directly under that member and
    // under no other node member on the way to the variant, so restoring `{}`
    // on one member fails that member's own row. `VariantCase::expression` is
    // under every row.
    // WhenNode::predicate, and PredicateNode::lhs under it: 3.71 > 3 holds.
    STATIC_REQUIRE(
        survives_a_method(formula::when(banded() > var<Ratio>, var<Ratio>, formula::number(formula::Rational { 0 })),
                          formula::Rational { 3 }));
    STATIC_REQUIRE(refuses_default_construction(
        formula::when(banded() > var<Ratio>, var<Ratio>, formula::number(formula::Rational { 0 }))));
    // WhenNode::thenBranch.
    STATIC_REQUIRE(
        survives_a_method(formula::when(var<Ratio> > formula::number(formula::Rational { 137, 100 }), banded(), var<Ratio>),
                          formula::Rational { 371, 100 }));
    STATIC_REQUIRE(refuses_default_construction(
        formula::when(var<Ratio> > formula::number(formula::Rational { 137, 100 }), banded(), var<Ratio>)));
    // WhenNode::elseBranch.
    STATIC_REQUIRE(
        survives_a_method(formula::when(var<Ratio> > formula::number(formula::Rational { 137, 10 }), var<Ratio>, banded()),
                          formula::Rational { 371, 100 }));
    STATIC_REQUIRE(refuses_default_construction(
        formula::when(var<Ratio> > formula::number(formula::Rational { 137, 10 }), var<Ratio>, banded())));
    // DocumentedNode::inner.
    STATIC_REQUIRE(survives_a_method(
        formula::documented(
            banded(),
            formula::Citation { .title = "Band correction", .reference = "Example Standard 1:2020", .section = "4.2" }),
        formula::Rational { 371, 100 }));
    STATIC_REQUIRE(refuses_default_construction(formula::documented(
        banded(),
        formula::Citation { .title = "Band correction", .reference = "Example Standard 1:2020", .section = "4.2" })));
    // UnaryNode::operand.
    STATIC_REQUIRE(survives_a_method(-banded(), formula::Rational { -371, 100 }));
    STATIC_REQUIRE(refuses_default_construction(-banded()));
    // BinaryNode::lhs.
    STATIC_REQUIRE(survives_a_method(banded() * var<Ratio>, formula::Rational { 1113, 100 }));
    STATIC_REQUIRE(refuses_default_construction(banded() * var<Ratio>));
    // BinaryNode::rhs.
    STATIC_REQUIRE(survives_a_method(var<Ratio> * exact(), formula::Rational { 8751, 1000 }));
    STATIC_REQUIRE(refuses_default_construction(var<Ratio> * exact()));
    // PowerNode::operand: 2.917 squared.
    STATIC_REQUIRE(survives_a_method(formula::pow<2>(exact()), formula::Rational { 8508889, 1000000 }));
    STATIC_REQUIRE(refuses_default_construction(formula::pow<2>(exact())));
    // RootNode::operand: the square root of 0.7569.
    STATIC_REQUIRE(survives_a_method(formula::sqrt(exactPerfectSquare()), formula::Rational { 87, 100 }));
    STATIC_REQUIRE(refuses_default_construction(formula::sqrt(exactPerfectSquare())));
    // RoundNode::operand: 1.081 to two decimal places, 1.08 -- distinct from
    // both rows (0.781, 1.494) and from the operand read as zero (0.781), so
    // the rounded value still proves which number the lookup computed.
    STATIC_REQUIRE(survives_a_method(
        formula::rounded<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(interpolating()),
        formula::Rational { 27, 25 }));
    STATIC_REQUIRE(refuses_default_construction(
        formula::rounded<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            interpolating())));
    // RoundSignificantNode::operand: 1.081 to three significant digits, 1.08,
    // for the same reason.
    STATIC_REQUIRE(survives_a_method(
        formula::rounded_to_digits<unit::One, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfAwayFromZero>(
            interpolating()),
        formula::Rational { 27, 25 }));
    STATIC_REQUIRE(refuses_default_construction(
        formula::rounded_to_digits<unit::One, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfAwayFromZero>(
            interpolating())));
    // NumericValueNode::operand.
    STATIC_REQUIRE(survives_a_method(formula::numeric_value_of<unit::One, "a ratio is already a pure number">(banded()),
                                     formula::Rational { 371, 100 }));
    STATIC_REQUIRE(
        refuses_default_construction(formula::numeric_value_of<unit::One, "a ratio is already a pure number">(banded())));
    // BandedLookupNode::operand: the exact lookup's 2.917 falls in [1.93, 4.37).
    STATIC_REQUIRE(survives_a_method(formula::banded_lookup<unit::One, Bands, unit::One>(
                                         exact(), { formula::Rational { 863, 1000 }, formula::Rational { 371, 100 } }),
                                     formula::Rational { 371, 100 }));
    STATIC_REQUIRE(refuses_default_construction(formula::banded_lookup<unit::One, Bands, unit::One>(
        exact(), { formula::Rational { 863, 1000 }, formula::Rational { 371, 100 } })));
    // InterpolatingLookupNode::operand: 2917/7130 of the way from 0.781 to
    // 1.494.
    STATIC_REQUIRE(survives_a_method(formula::interpolating_lookup<unit::One, Points, unit::One>(
                                         exact(), { formula::Rational { 781, 1000 }, formula::Rational { 1494, 1000 } }),
                                     formula::Rational { 10727, 10000 }));
    STATIC_REQUIRE(refuses_default_construction(formula::interpolating_lookup<unit::One, Points, unit::One>(
        exact(), { formula::Rational { 781, 1000 }, formula::Rational { 1494, 1000 } })));
    // The two lookups' own `operand`, which the probe reaches only past a
    // `Corrections<0>`: under a table with rows, `corrections` comes first
    // and refuses before `operand` is looked at.
    STATIC_REQUIRE(misses_in_a_method(formula::banded_lookup<unit::One, NoBands, unit::One>(exact(), {})));
    STATIC_REQUIRE(refuses_default_construction(formula::banded_lookup<unit::One, NoBands, unit::One>(exact(), {})));
    STATIC_REQUIRE(misses_in_a_method(formula::interpolating_lookup<unit::One, NoPoints, unit::One>(exact(), {})));
    STATIC_REQUIRE(refuses_default_construction(formula::interpolating_lookup<unit::One, NoPoints, unit::One>(exact(), {})));
}

TEST_CASE("a lookup survives a method's constraints under either comparand", "[method][lookup]")
{
    // Constraint::predicate, with the lookup under PredicateNode::rhs in the
    // first constraint and under PredicateNode::lhs in the second.
    constexpr auto underRight =
        formula::constraint(var<Ratio> < banded(), formula::Verdict { "ratio above the band's limit" });
    constexpr auto underLeft =
        formula::constraint(banded() > var<Ratio>, formula::Verdict { "ratio above the band's limit" });
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(var<Ratio>), formula::variant<Cylinder>(var<Ratio>)),
                        unrounded(),
                        formula::constraints(underRight, underLeft));
    constexpr auto outcomes = formula::check_all(m.constraintSet, ratioOfThree);
    STATIC_REQUIRE(outcomes[0].is_satisfied());
    STATIC_REQUIRE(outcomes[1].is_satisfied());

    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(underRight)>);
    STATIC_REQUIRE(!std::default_initializable<decltype(underRight)>);
    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(underLeft)>);
    STATIC_REQUIRE(!std::default_initializable<decltype(underLeft)>);
}

TEST_CASE("a method holding a lookup, and a tuple of them, cannot be default-constructed", "[method][lookup]")
{
    // One step further out than the rows above: the question asked of a
    // method itself, of its variants and constraint packs, and by a tuple
    // holding methods.
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(banded()), formula::variant<Cylinder>(interpolating())),
                        unrounded(),
                        formula::constraints(formula::constraint(var<Ratio> < banded(),
                                                                 formula::Verdict { "ratio above the band's limit" })));
    using Method = std::remove_cvref_t<decltype(m)>;

    STATIC_REQUIRE(!std::is_default_constructible_v<Method>);
    STATIC_REQUIRE(!std::default_initializable<Method>);
    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(m.variantSet)>);
    STATIC_REQUIRE(!std::is_default_constructible_v<decltype(m.constraintSet)>);
    STATIC_REQUIRE(!std::is_default_constructible_v<std::tuple<Method, Method>>);
    STATIC_REQUIRE(std::tuple_size_v<std::tuple<Method, Method>> == 2);
}

TEST_CASE("a method whose only lookup is in a constraint cannot be default-constructed either", "[method][lookup]")
{
    // Its variants pack IS default-constructible, so the probe goes on to
    // `Method::constraintSet` -- which it never reaches in the case above,
    // where `variantSet` refuses first.
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(var<Ratio>), formula::variant<Cylinder>(var<Ratio>)),
                        unrounded(),
                        formula::constraints(formula::constraint(var<Ratio> < banded(),
                                                                 formula::Verdict { "ratio above the band's limit" })));
    using Method = std::remove_cvref_t<decltype(m)>;

    STATIC_REQUIRE(!std::is_default_constructible_v<Method>);
    STATIC_REQUIRE(!std::default_initializable<Method>);
    STATIC_REQUIRE(!std::is_default_constructible_v<std::tuple<Method, Method>>);
    STATIC_REQUIRE(answer<Cube>(m) == formula::Rational { 3 });
}
