// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;
using formula::DecimalPlaces;
using formula::RoundingMode;

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Length: formula::Quantity<Length, "L", "specimen length", unit::Millimetre>
{
};

[[nodiscard]] constexpr auto millimetres(long long value)
{
    return formula::environment(formula::Measured<Diameter> { formula::Rational { value, 100 } });
}
} // namespace

TEST_CASE("a rounding node rounds in the unit it names, not in coherent SI", "[rounding-node]")
{
    // 12.34 mm to one decimal place is 12.3 mm. In coherent SI the same value
    // is 0.01234 m, and rounding *that* to one decimal place gives 0.0 --
    // which is why the unit is part of the node and not an afterthought.
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(
        var<Diameter>);
    constexpr auto environment = millimetres(1234);   // 12.34 mm

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    // 12.34 mm -> 12.3 mm, converted to coherent SI (0.0123 m) and back into
    // Diameter's own declared unit -- millimetre -- for the final answer: 12.3.
    CHECK(result->measurement().value() == formula::Rational { 123, 10 });
}

TEST_CASE("rounding does not change the dimension of what it wraps", "[rounding-node]")
{
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::Ceiling>(var<Diameter>);
    STATIC_REQUIRE(decltype(node)::dimension == formula::Describe<Diameter>::dimension);
}

TEST_CASE("an absent operand stays absent rather than rounding to zero", "[rounding-node]")
{
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::Floor>(var<Diameter>);
    constexpr auto environment = formula::environment(formula::Measured<Diameter>::absent());

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}

TEST_CASE("intermediate and final rounding are separately expressible", "[rounding-node]")
{
    // The point of rounding being a node: a method may round an input to a
    // coarse granularity BEFORE it enters a formula and round the result
    // differently afterwards. A library that rounds only at output cannot say
    // this, and produces a different number.
    constexpr auto coarseInput =
        formula::rounded<unit::Millimetre, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(var<Diameter>);
    constexpr auto doubled = coarseInput + coarseInput;
    constexpr auto finalResult =
        formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero>(doubled);

    constexpr auto environment = millimetres(1250);   // 12.50 mm

    // Input rounds to 13 mm (half away from zero), doubled is 26 mm, and the
    // final rounding leaves it at 26 mm. Rounding only at the end would have
    // given 25.0 mm -- a different number, from the same formula and the same
    // input.
    constexpr auto result = formula::checked_evaluate<Diameter>(finalResult, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    CHECK(result->measurement().value() == formula::Rational { 26, 1 });
}

TEST_CASE("significant digits are available as a node too", "[rounding-node]")
{
    constexpr auto node =
        formula::rounded_to_digits<unit::Millimetre, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    constexpr auto environment = millimetres(1234);   // 12.34 mm -> 12 mm

    constexpr auto result = formula::checked_evaluate<Diameter>(node, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    CHECK(result->measurement().value() == formula::Rational { 12, 1 });
}
