// SPDX-License-Identifier: Apache-2.0
//
// A method holding a lookup, of each kind, at a variant's root and nested
// inside it, and in a constraint.
//
// Its own file because it is a portability regression: on clang++ 20 with
// libstdc++ 14, `std::tuple<VariantCase<...>>` asks whether each variant is
// implicitly default-constructible, and a lookup node used to answer that
// question by instantiating the body of `Corrections<N>`'s wrong-count
// constructor -- whose `static_assert` then refused a perfectly good method.
// g++, cl and clang-cl accepted the same code, so nothing but a clang++ build
// with libstdc++ could show it. See `Corrections` in `lookup.hpp`.
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

enum class Shape : std::uint8_t
{
    Square,
    Round,
};

inline constexpr formula::KeyTable<Shape, 2> Keys { Shape::Square, Shape::Round };
inline constexpr formula::BandTable<2> Bands { formula::band(0, 1, 2, 1), formula::band(2, 1, 6, 1) };
inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(0), formula::breakpoint(8) };

// Every lookup below answers a different number for a ratio of 3, and none of
// them is 3 itself, so a method that evaluated the wrong variant, the wrong
// row or the bare operand cannot pass.
[[nodiscard]] constexpr auto banded()
{
    // 3 falls in [2, 6), the second band: 7.
    return formula::banded_lookup<unit::One, Bands, unit::One>(var<Ratio>,
                                                               { formula::Rational { 1 }, formula::Rational { 7 } });
}

[[nodiscard]] constexpr auto interpolating()
{
    // 3/8 of the way from 0 to 80: 30.
    return formula::interpolating_lookup<unit::One, Points, unit::One>(
        var<Ratio>, { formula::Rational { 0 }, formula::Rational { 80 } });
}

[[nodiscard]] constexpr auto exact()
{
    // The second key's row: 5.
    return formula::exact_lookup<Keys, unit::One>(Shape::Round, { formula::Rational { 2 }, formula::Rational { 5 } });
}

[[nodiscard]] constexpr auto unrounded()
{
    return formula::rounding_rule<unit::One, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>();
}

inline constexpr auto ratioOfThree = formula::environment(formula::Measured<Ratio> { formula::Rational { 3 } });

template <typename Tag, typename M>
[[nodiscard]] constexpr formula::Rational answer(M const& m)
{
    return formula::evaluate_method<Tag>(m, ratioOfThree)->value();
}
} // namespace

TEST_CASE("a method whose variant is a banded lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(banded())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 7 });
}

TEST_CASE("a method whose variant is an interpolating lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(interpolating())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 30 });
}

TEST_CASE("a method whose variant is an exact lookup evaluates it", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(exact())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 5 });
}

TEST_CASE("a method evaluates a lookup nested inside a variant's expression", "[method][lookup]")
{
    // The spec's own shape: a measured quantity scaled by a tabulated
    // correction, 3 * 5.
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(var<Ratio> * exact())), unrounded(), formula::constraints());
    constexpr auto result = formula::evaluate_method<Cube>(m, ratioOfThree);
    STATIC_REQUIRE(result.has_value());
    STATIC_REQUIRE(result->has_value());
    STATIC_REQUIRE(result->value() == formula::Rational { 15 });
}

TEST_CASE("a method selects between variants that are both lookups", "[method][lookup]")
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(banded()), formula::variant<Cylinder>(interpolating())),
                        unrounded(),
                        formula::constraints());
    STATIC_REQUIRE(answer<Cube>(m) == formula::Rational { 7 });
    STATIC_REQUIRE(answer<Cylinder>(m) == formula::Rational { 30 });
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
