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

/// A second quantity of the same dimension, so a test can put absence on the
/// right-hand side of a comparison rather than only on the left.
struct Threshold: formula::Quantity<Threshold, "f_lim", "strength threshold", unit::Megapascal>
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

TEST_CASE("every comparison operator is correct at its own boundary", "[predicate]")
{
    // Four of the six operators had no committed test. Each `if constexpr`
    // arm is independent, so a copy-paste swap in one -- Less implemented as
    // LessOrEqual, say -- would pass every test that never lands exactly on
    // the threshold. Thresholds in test methods fall on round numbers people
    // aim at, so the boundary is the common case, not the rare one.
    constexpr auto fifty = formula::constant<unit::Megapascal>(formula::Rational { 50 });

    constexpr auto less = var<Strength> < fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(less, strengthOf(49)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(less, strengthOf(50)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(less, strengthOf(51)) == false);

    constexpr auto lessOrEqual = var<Strength> <= fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(lessOrEqual, strengthOf(49)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(lessOrEqual, strengthOf(50)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(lessOrEqual, strengthOf(51)) == false);

    constexpr auto greater = var<Strength> > fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greater, strengthOf(49)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greater, strengthOf(50)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greater, strengthOf(51)) == true);

    constexpr auto greaterOrEqual = var<Strength> >= fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greaterOrEqual, strengthOf(49)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greaterOrEqual, strengthOf(50)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(greaterOrEqual, strengthOf(51)) == true);

    constexpr auto equal = var<Strength> == fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(equal, strengthOf(49)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(equal, strengthOf(50)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(equal, strengthOf(51)) == false);

    constexpr auto notEqual = var<Strength> != fifty;
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(notEqual, strengthOf(49)) == true);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(notEqual, strengthOf(50)) == false);
    STATIC_REQUIRE(**formula::checked_evaluate_predicate(notEqual, strengthOf(51)) == true);
}

TEST_CASE("absence on either side, or both, gives absence", "[predicate]")
{
    // The committed suite covered only an absent left operand. Each position
    // is a separate branch in the evaluator.
    constexpr auto compared = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });

    constexpr auto absentLeft = formula::checked_evaluate_predicate(
        compared, formula::environment(formula::Measured<Strength>::absent()));
    STATIC_REQUIRE(absentLeft.has_value());
    STATIC_REQUIRE_FALSE(absentLeft->has_value());

    // A variable on the right, so absence can be put there instead.
    constexpr auto bothSides = var<Strength> > var<Threshold>;

    constexpr auto absentRight = formula::checked_evaluate_predicate(
        bothSides,
        formula::environment(formula::Measured<Strength> { formula::Rational { 60 } },
                             formula::Measured<Threshold>::absent()));
    STATIC_REQUIRE(absentRight.has_value());
    STATIC_REQUIRE_FALSE(absentRight->has_value());

    constexpr auto absentBoth = formula::checked_evaluate_predicate(
        bothSides,
        formula::environment(formula::Measured<Strength>::absent(), formula::Measured<Threshold>::absent()));
    STATIC_REQUIRE(absentBoth.has_value());
    STATIC_REQUIRE_FALSE(absentBoth->has_value());
}
