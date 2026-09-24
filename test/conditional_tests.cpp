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

TEST_CASE("a threshold selects between two formulas", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty,
                                          var<Strength> * formula::number(formula::Rational { 2 }),
                                          var<Strength> * formula::number(formula::Rational { 4 }));

    constexpr auto high = formula::checked_evaluate<Strength>(chosen, strengthOf(60));
    REQUIRE(high.has_value());
    CHECK(high->measurement().value() == formula::Rational { 120 });

    constexpr auto low = formula::checked_evaluate<Strength>(chosen, strengthOf(40));
    REQUIRE(low.has_value());
    CHECK(low->measurement().value() == formula::Rational { 160 });
}

TEST_CASE("the branch not taken is never evaluated", "[conditional]")
{
    // The else branch divides by zero. If both branches were evaluated, this
    // would fail; because only the selected one runs, it does not. This is
    // not a micro-optimisation -- a guarded formula exists precisely because
    // the other branch is invalid for these inputs.
    constexpr auto nonZero = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 0 });
    constexpr auto guarded = formula::when(nonZero,
                                           var<Strength>,
                                           var<Strength> / formula::number(formula::Rational { 0 }));

    constexpr auto result = formula::checked_evaluate<Strength>(guarded, strengthOf(60));
    REQUIRE(result.has_value());
    CHECK(result->measurement().value() == formula::Rational { 60 });
}

TEST_CASE("an absent predicate makes the result absent, not the else branch", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen =
        formula::when(overFifty, var<Strength>, formula::constant<unit::Megapascal>(formula::Rational { 0 }));
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto result = formula::checked_evaluate<Strength>(chosen, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}
