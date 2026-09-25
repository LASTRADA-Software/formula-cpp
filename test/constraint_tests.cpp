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

// An environment that measured nothing at all, for the not-checked case.
[[nodiscard]] constexpr auto nothingMeasured()
{
    return formula::environment(formula::Measured<Strength> {});
}

inline constexpr auto minimumStrength =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" },
                        formula::Citation { .title = "Minimum compressive strength",
                                            .reference = "Example Standard 7:2020",
                                            .section = "5.1" });

// Same shape predicate_tests.cpp's own arithmetic-failure case uses: a
// division by a measured zero, wrapped as a constraint so `check` has
// something whose predicate cannot be evaluated at all.
inline constexpr auto dividesByZero =
    formula::constraint((var<Strength> / formula::number(formula::Rational { 0 }))
                             > formula::constant<unit::Megapascal>(formula::Rational { 1 }),
                        formula::Verdict { "specimen result is unusable" });
} // namespace

TEST_CASE("constraint: a satisfied relationship yields no verdict", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, strengthOf(45));
    STATIC_REQUIRE(outcome.is_satisfied());
    CHECK_FALSE(outcome.verdict().has_value());
}

TEST_CASE("constraint: a violated relationship carries the verdict it was declared with", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, strengthOf(20));
    STATIC_REQUIRE(outcome.is_violated());
    REQUIRE(outcome.verdict().has_value());
    CHECK(outcome.verdict()->label == std::string_view { "reject the specimen" });
}

TEST_CASE("constraint: an unmeasured input is not checked, and is never satisfied", "[constraint]")
{
    constexpr auto outcome = formula::check(minimumStrength, nothingMeasured());
    STATIC_REQUIRE(outcome.is_not_checked());
    STATIC_REQUIRE(!outcome.is_satisfied());   // the property this whole task exists for
    STATIC_REQUIRE(!outcome.is_violated());    // and it is not a failure either
}

TEST_CASE("constraint: arithmetic that breaks while checking is invalid, not satisfied", "[constraint]")
{
    constexpr auto outcome = formula::check(dividesByZero, strengthOf(0));
    STATIC_REQUIRE(outcome.is_invalid());
    STATIC_REQUIRE(!outcome.is_satisfied());
    REQUIRE(outcome.error().has_value());
    CHECK(outcome.error() == formula::ArithmeticError::DivisionByZero);
}
