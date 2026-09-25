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

// A second, independent quantity -- so a set of two constraints can fail
// differently on each, which is the only way to tell an ordered result
// from an unordered one (two identical verdicts cannot).
struct Diameter: formula::Quantity<Diameter, "d", "measured diameter", unit::Millimetre>
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

// Both quantities measured, for checking a set of constraints that spans
// both of them.
[[nodiscard]] constexpr auto strengthAndDiameter(long long strengthValue, long long diameterValue)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { strengthValue } },
                                formula::Measured<Diameter> { formula::Rational { diameterValue } });
}

// Strength measured, diameter never measured -- so a constraint over
// `Diameter` comes back not-checked while one over `Strength` still resolves.
[[nodiscard]] constexpr auto strengthOnly(long long strengthValue)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { strengthValue } },
                                formula::Measured<Diameter> {});
}

inline constexpr auto minimumStrength =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" },
                        formula::Citation { .title = "Minimum compressive strength",
                                            .reference = "Example Standard 7:2020",
                                            .section = "5.1" });

inline constexpr auto maximumDiameter =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 100 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" });

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

// -------------------------------------------------------- checking a set

TEST_CASE("constraint set: two violated constraints are both reported, in declaration order", "[constraint]")
{
    // strength 20 violates minimumStrength (20 >= 30 is false); diameter 150
    // violates maximumDiameter (150 <= 100 is false). Both are violated at
    // once -- the case a short-circuiting checker would get wrong.
    constexpr auto environment = strengthAndDiameter(20, 150);

    {
        constexpr auto outcomes = formula::check_all(formula::constraints(minimumStrength, maximumDiameter),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_violated());
        STATIC_REQUIRE(outcomes[1].is_violated());
        REQUIRE(outcomes[0].verdict().has_value());
        REQUIRE(outcomes[1].verdict().has_value());
        CHECK(outcomes[0].verdict()->label == std::string_view { "reject the specimen" });
        CHECK(outcomes[1].verdict()->label == std::string_view { "specimen exceeds diameter tolerance" });
    }

    // Declared the other way around: the results swap position too. Two
    // identical verdicts could not tell an ordered result from an unordered
    // one -- these two distinct verdicts, and this reversal, can.
    {
        constexpr auto outcomes = formula::check_all(formula::constraints(maximumDiameter, minimumStrength),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_violated());
        STATIC_REQUIRE(outcomes[1].is_violated());
        REQUIRE(outcomes[0].verdict().has_value());
        REQUIRE(outcomes[1].verdict().has_value());
        CHECK(outcomes[0].verdict()->label == std::string_view { "specimen exceeds diameter tolerance" });
        CHECK(outcomes[1].verdict()->label == std::string_view { "reject the specimen" });
    }
}

TEST_CASE("constraint set: a satisfied constraint and a violated one are both reported, in either position",
          "[constraint]")
{
    // strength 45 satisfies minimumStrength; diameter 150 still violates
    // maximumDiameter.
    {
        constexpr auto outcomes = formula::check_all(formula::constraints(minimumStrength, maximumDiameter),
                                                      strengthAndDiameter(45, 150));
        STATIC_REQUIRE(outcomes[0].is_satisfied());
        STATIC_REQUIRE(outcomes[1].is_violated());
        CHECK_FALSE(outcomes[0].verdict().has_value());
        REQUIRE(outcomes[1].verdict().has_value());
        CHECK(outcomes[1].verdict()->label == std::string_view { "specimen exceeds diameter tolerance" });
    }

    // Declared the other way around, so the satisfied constraint is the one
    // evaluated last rather than first. The section above would pass even if
    // the checker stopped at the first Satisfied it met -- minimumStrength
    // simply happens to come first there -- purely by the fixture's order,
    // not by anything the test asserts. This section is the one that would
    // actually catch that mutation, because here stopping at the first
    // Satisfied never happens (it comes second), so both sections together
    // prove the property regardless of which position it lands in.
    {
        constexpr auto outcomes = formula::check_all(formula::constraints(maximumDiameter, minimumStrength),
                                                      strengthAndDiameter(45, 150));
        STATIC_REQUIRE(outcomes[0].is_violated());
        STATIC_REQUIRE(outcomes[1].is_satisfied());
        REQUIRE(outcomes[0].verdict().has_value());
        CHECK(outcomes[0].verdict()->label == std::string_view { "specimen exceeds diameter tolerance" });
        CHECK_FALSE(outcomes[1].verdict().has_value());
    }
}

TEST_CASE("constraint set: a violated constraint and an invalid one are both reported, in either position",
          "[constraint]")
{
    // strength 0 violates minimumStrength (0 >= 30 is false) and makes
    // dividesByZero's predicate divide by zero, so checking it comes back
    // invalid -- rather than either being dropped by a checker that stops at
    // the first Invalid it meets.
    constexpr auto environment = strengthOf(0);

    {
        constexpr auto outcomes = formula::check_all(formula::constraints(minimumStrength, dividesByZero),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_violated());
        STATIC_REQUIRE(outcomes[1].is_invalid());
        REQUIRE(outcomes[0].verdict().has_value());
        CHECK(outcomes[0].verdict()->label == std::string_view { "reject the specimen" });
        REQUIRE(outcomes[1].error().has_value());
        CHECK(outcomes[1].error() == formula::ArithmeticError::DivisionByZero);
    }

    // Reversed: the invalid constraint is checked first. A checker that
    // stops at the first Invalid would leave index 1 at its NotChecked
    // default instead of reporting the violation -- this is the direction
    // that actually catches that mutation, the same way the not-checked
    // test's reversed section catches stop-at-first-NotChecked.
    {
        constexpr auto outcomes = formula::check_all(formula::constraints(dividesByZero, minimumStrength),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_invalid());
        STATIC_REQUIRE(outcomes[1].is_violated());
        REQUIRE(outcomes[0].error().has_value());
        CHECK(outcomes[0].error() == formula::ArithmeticError::DivisionByZero);
        REQUIRE(outcomes[1].verdict().has_value());
        CHECK(outcomes[1].verdict()->label == std::string_view { "reject the specimen" });
    }
}

TEST_CASE("constraint set: a not-checked constraint does not suppress a violated one, in either position",
          "[constraint]")
{
    // Diameter is never measured here, so maximumDiameter cannot resolve;
    // strength 20 still violates minimumStrength. A checker that stops at
    // the first violation would still pass this one (the not-checked slot
    // just never runs) -- the reversed declaration below is the one that
    // actually catches that mutation, because there the not-checked result
    // comes first and a checker that stops there would leave the violated
    // slot at its NotChecked default instead of reporting it.
    constexpr auto environment = strengthOnly(20);

    {
        constexpr auto outcomes = formula::check_all(formula::constraints(minimumStrength, maximumDiameter),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_violated());
        STATIC_REQUIRE(outcomes[1].is_not_checked());
        REQUIRE(outcomes[0].verdict().has_value());
        CHECK(outcomes[0].verdict()->label == std::string_view { "reject the specimen" });
    }

    {
        constexpr auto outcomes = formula::check_all(formula::constraints(maximumDiameter, minimumStrength),
                                                      environment);
        STATIC_REQUIRE(outcomes[0].is_not_checked());
        STATIC_REQUIRE(outcomes[1].is_violated());
        REQUIRE(outcomes[1].verdict().has_value());
        CHECK(outcomes[1].verdict()->label == std::string_view { "reject the specimen" });
    }
}
