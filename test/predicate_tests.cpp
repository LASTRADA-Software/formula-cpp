// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto strengthOf(long long value)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { value } });
}
} // namespace

TEST_CASE("a predicate compares two expressions of the same dimension", "[predicate]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });

    constexpr auto high = formula::checked_evaluate_predicate(overFifty, strengthOf(60));
    REQUIRE(high.has_value());
    REQUIRE(high->has_value());
    CHECK(**high == true);

    constexpr auto low = formula::checked_evaluate_predicate(overFifty, strengthOf(40));
    REQUIRE(low.has_value());
    REQUIRE(low->has_value());
    CHECK(**low == false);
}

TEST_CASE("a boundary value is not over the threshold", "[predicate]")
{
    // Exactly 50 is not greater than 50. Worth its own case: an off-by-one
    // here silently selects the wrong formula for every specimen that lands
    // exactly on a threshold, which in a test method is not a rare input --
    // thresholds are chosen to fall on round numbers people aim at.
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto atFifty = formula::checked_evaluate_predicate(overFifty, strengthOf(50));
    REQUIRE(atFifty.has_value());
    REQUIRE(atFifty->has_value());
    CHECK(**atFifty == false);

    constexpr auto atMost = var<Strength> <= formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto atFiftyInclusive = formula::checked_evaluate_predicate(atMost, strengthOf(50));
    REQUIRE(atFiftyInclusive.has_value());
    REQUIRE(atFiftyInclusive->has_value());
    CHECK(**atFiftyInclusive == true);
}

TEST_CASE("an absent operand makes the predicate absent, not false", "[predicate]")
{
    // Answering `false` would silently pick a branch on the strength of a
    // measurement nobody took.
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto unknown = formula::checked_evaluate_predicate(overFifty, environment);
    REQUIRE(unknown.has_value());
    CHECK_FALSE(unknown->has_value());
}

TEST_CASE("an arithmetic failure in a predicate is an error, not a verdict", "[predicate]")
{
    constexpr auto divideByZero =
        (var<Strength> / formula::number(formula::Rational { 0 })) > formula::constant<unit::Megapascal>(formula::Rational { 1 });
    constexpr auto result = formula::checked_evaluate_predicate(divideByZero, strengthOf(60));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == formula::ArithmeticError::DivisionByZero);
}
