// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/expression.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct BeamLength: formula::Quantity<BeamLength, "L", "beam length", formula::unit::Millimetre>
{
};
struct AppliedForce: formula::Quantity<AppliedForce, "F", "applied force", formula::unit::Kilogram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

} // namespace

TEST_CASE("expression: a variable carries its quantity's dimension", "[expression]")
{
    STATIC_REQUIRE(formula::VarNode<WaterVolume>::dimension == formula::dim::Volume);
    STATIC_REQUIRE(formula::VarNode<BeamLength>::dimension == formula::dim::Length);
    STATIC_REQUIRE(std::is_same_v<formula::VarNode<WaterVolume>::quantity, WaterVolume>);
}

TEST_CASE("expression: every node satisfies the Node concept", "[expression]")
{
    STATIC_REQUIRE(formula::Node<formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE(formula::Node<decltype(var<WaterVolume> + var<CementVolume>)>);
    STATIC_REQUIRE(formula::Node<decltype(-var<WaterVolume>)>);
    STATIC_REQUIRE(formula::Node<decltype(formula::number(rat(2)))>);
    STATIC_REQUIRE_FALSE(formula::Node<formula::Rational>);
    STATIC_REQUIRE_FALSE(formula::Node<int>);
}

TEST_CASE("expression: multiplication and division combine dimensions", "[expression]")
{
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
    constexpr auto moment = var<AppliedForce> * var<BeamLength>;

    STATIC_REQUIRE(formula::is_dimensionless(decltype(ratio)::dimension));
    STATIC_REQUIRE(decltype(moment)::dimension == formula::dim::Mass * formula::dim::Length);
}

TEST_CASE("expression: addition and subtraction keep the shared dimension", "[expression]")
{
    constexpr auto total = var<WaterVolume> + var<CementVolume>;
    constexpr auto excess = var<WaterVolume> - var<CementVolume>;

    STATIC_REQUIRE(decltype(total)::dimension == formula::dim::Volume);
    STATIC_REQUIRE(decltype(excess)::dimension == formula::dim::Volume);
}

TEST_CASE("expression: negation keeps the operand's dimension", "[expression]")
{
    constexpr auto negated = -var<BeamLength>;

    STATIC_REQUIRE(decltype(negated)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(negated)::op == formula::UnaryOperator::Negate);
}

TEST_CASE("expression: a bare number is dimensionless and keeps its value", "[expression]")
{
    constexpr auto half = formula::number(rat(1, 2));

    STATIC_REQUIRE(formula::is_dimensionless(decltype(half)::dimension));
    STATIC_REQUIRE(decltype(half)::unit == formula::unit::One);
    STATIC_REQUIRE(half.number == rat(1, 2));
}

TEST_CASE("expression: a dimensioned constant carries its unit's dimension", "[expression]")
{
    constexpr auto span = formula::constant<formula::unit::Millimetre>(rat(150));

    STATIC_REQUIRE(decltype(span)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(span)::unit == formula::unit::Millimetre);
    STATIC_REQUIRE(span.number == rat(150));
}

TEST_CASE("expression: a Rational mixed into a formula becomes a dimensionless constant", "[expression]")
{
    constexpr auto scaledRight = var<BeamLength> * rat(1, 4);
    constexpr auto scaledLeft = rat(1, 4) * var<BeamLength>;

    STATIC_REQUIRE(decltype(scaledRight)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(scaledLeft)::dimension == formula::dim::Length);
    STATIC_REQUIRE(std::is_same_v<decltype(scaledRight),
                                  formula::BinaryNode<formula::BinaryOperator::Multiply,
                                                      formula::VarNode<BeamLength>,
                                                      formula::ConstantNode<formula::unit::One>> const>);
    // `scaledRight` is itself `constexpr`, so `decltype` of the *variable* does
    // carry the const -- unlike `decltype(ratio.lhs)` above, which names a member.
}

TEST_CASE("expression: the tree keeps its shape and its operands", "[expression]")
{
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;

    STATIC_REQUIRE(decltype(ratio)::op == formula::BinaryOperator::Divide);
    // `decltype` of a member access yields the member's declared type, with no
    // const from the object it was read through -- so no `const` here.
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.lhs), formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio.rhs), formula::VarNode<CementVolume>>);
}

TEST_CASE("expression: a deep tree carries no state beyond its constants", "[expression]")
{
    constexpr auto deep = (var<WaterVolume> + var<CementVolume>) / (var<WaterVolume> - var<CementVolume>) *var<BeamLength>;

    STATIC_REQUIRE(decltype(deep)::dimension == formula::dim::Length);

    // A variable declares no members; a constant declares exactly its number.
    // Do NOT assert a size for a composed tree: a node holds its children by
    // value, and how much an empty child costs inside its parent is the
    // compiler's business. Measured, the five-leaf tree above was 5 bytes on
    // cl and clang-cl and 9 on g++ -- correct on all three, portable on none.
    STATIC_REQUIRE(std::is_empty_v<formula::VarNode<WaterVolume>>);
    STATIC_REQUIRE_FALSE(std::is_empty_v<formula::ConstantNode<formula::unit::One>>);
}

TEST_CASE("expression: the same formula written twice is the same type", "[expression]")
{
    constexpr auto first = var<WaterVolume> / var<CementVolume>;
    constexpr auto second = var<WaterVolume> / var<CementVolume>;

    STATIC_REQUIRE(std::is_same_v<decltype(first), decltype(second)>);
}

#include "expression_cross_tu.hpp"

TEST_CASE("expression: two translation units agree on a formula's type", "[expression]")
{
    // The declaration in the header already forces this: `density_built_in_other_tu`
    // returns `Density`, and the other TU builds the tree with its own operators.
    // If the types differed the program would not link.
    auto const fromOtherTranslationUnit = density_built_in_other_tu();
    auto const fromThisTranslationUnit = formula::var<SampleMass> / formula::var<SampleVolume>;

    STATIC_REQUIRE(std::is_same_v<decltype(fromOtherTranslationUnit), decltype(fromThisTranslationUnit)>);
    CHECK(decltype(fromThisTranslationUnit)::dimension == formula::dim::Density);
}

TEST_CASE("expression: a variable template is one object across translation units", "[expression]")
{
    // `inline constexpr` gives the variable template external linkage with one
    // definition; a missing `inline` would give each TU its own copy, and this
    // is the only test that would notice.
    CHECK(sample_mass_variable_address_in_other_tu() == static_cast<void const*>(&formula::var<SampleMass>));
}
