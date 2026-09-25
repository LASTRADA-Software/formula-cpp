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

/// A signed quantity, so the mode cases below can also land on a tie *below*
/// zero -- which is the only place four of the seven modes separate from each
/// other. On a non-negative value Ceiling and AwayFromZero agree, and so do
/// Floor and TowardZero.
struct Deviation: formula::Quantity<Deviation, "e", "dimensional deviation", unit::Millimetre>
{
};

[[nodiscard]] constexpr auto millimetres(long long value)
{
    return formula::environment(formula::Measured<Diameter> { formula::Rational { value, 100 } });
}

/// @p millimetres rounded to one decimal place of a millimetre under @p Mode,
/// through an actual rounding node, and back out in millimetres.
template <RoundingMode Mode>
[[nodiscard]] constexpr formula::Rational toOneDecimalPlace(formula::Rational millimetresOfDeviation)
{
    constexpr auto node = formula::rounded<unit::Millimetre, DecimalPlaces { 1 }, Mode>(var<Deviation>);
    auto const outcome = formula::checked_evaluate<Deviation>(
        node, formula::environment(formula::Measured<Deviation> { millimetresOfDeviation }));
    return outcome.has_value() && outcome->is_value() ? outcome->measurement().value() : formula::Rational {};
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

TEST_CASE("every rounding mode reaches the node, at a value that lands exactly on a tie",
          "[rounding-node]")
{
    // The Mode template argument is one of the three things a caller must
    // supply to a rounding node, and until this case nothing verified it
    // reached the rounding at all: making RepRounding<Rational>::round_in
    // ignore its `mode` argument and always round half away from zero left
    // the whole suite green. The three cases that actually rounded all used
    // HalfAwayFromZero; the one naming Ceiling only asserted a dimension, and
    // the one naming Floor had an absent operand and never rounded.
    //
    // One value per shape of disagreement, each landing exactly on a tie:
    //
    //  - 12.25 mm separates the three half modes from each other (12.2 is the
    //    even neighbour, so HalfEven goes down where HalfAwayFromZero goes
    //    up);
    //  - 12.35 mm is the same tie one step along, where 12.4 is the even
    //    neighbour -- so HalfEven goes UP here, which is what distinguishes
    //    it from HalfTowardZero rather than merely from HalfAwayFromZero;
    //  - -12.25 mm separates Ceiling from AwayFromZero and Floor from
    //    TowardZero, which agree on every non-negative value.
    constexpr formula::Rational evenNeighbourBelow { 1225, 100 };   // 12.25 mm
    constexpr formula::Rational evenNeighbourAbove { 1235, 100 };   // 12.35 mm
    constexpr formula::Rational belowZero { -1225, 100 };           // -12.25 mm

    constexpr formula::Rational twoTwo { 122, 10 };
    constexpr formula::Rational twoThree { 123, 10 };
    constexpr formula::Rational twoFour { 124, 10 };

    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfAwayFromZero>(evenNeighbourBelow) == twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfTowardZero>(evenNeighbourBelow) == twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfEven>(evenNeighbourBelow) == twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Ceiling>(evenNeighbourBelow) == twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Floor>(evenNeighbourBelow) == twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::TowardZero>(evenNeighbourBelow) == twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::AwayFromZero>(evenNeighbourBelow) == twoThree);

    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfAwayFromZero>(evenNeighbourAbove) == twoFour);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfTowardZero>(evenNeighbourAbove) == twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfEven>(evenNeighbourAbove) == twoFour);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Ceiling>(evenNeighbourAbove) == twoFour);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Floor>(evenNeighbourAbove) == twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::TowardZero>(evenNeighbourAbove) == twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::AwayFromZero>(evenNeighbourAbove) == twoFour);

    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfAwayFromZero>(belowZero) == -twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfTowardZero>(belowZero) == -twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::HalfEven>(belowZero) == -twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Ceiling>(belowZero) == -twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::Floor>(belowZero) == -twoThree);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::TowardZero>(belowZero) == -twoTwo);
    STATIC_REQUIRE(toOneDecimalPlace<RoundingMode::AwayFromZero>(belowZero) == -twoThree);
}
