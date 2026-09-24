// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/function.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", formula::unit::SquareMetre>
{
};
struct Volume: formula::Quantity<Volume, "V", "volume", formula::unit::CubicMetre>
{
};
struct Edge: formula::Quantity<Edge, "a", "cube edge", formula::unit::Metre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

} // namespace

TEST_CASE("function: a power multiplies the dimension's exponents", "[function]")
{
    constexpr auto squared = formula::pow<2>(var<Diameter>);
    constexpr auto cubed = formula::pow<3>(var<Diameter>);
    constexpr auto reciprocal = formula::pow<-1>(var<Diameter>);

    STATIC_REQUIRE(decltype(squared)::dimension == formula::dim::Area);
    STATIC_REQUIRE(decltype(cubed)::dimension == formula::dim::Volume);
    STATIC_REQUIRE(decltype(reciprocal)::dimension == formula::dim::Scalar / formula::dim::Length);
    STATIC_REQUIRE(decltype(squared)::exponent == 2);
}

TEST_CASE("function: a root divides the dimension's exponents", "[function]")
{
    constexpr auto side = formula::sqrt(var<Area>);
    constexpr auto edge = formula::cbrt(var<Volume>);

    STATIC_REQUIRE(decltype(side)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(edge)::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(side)::degree == 2);
    STATIC_REQUIRE(decltype(edge)::degree == 3);
}

TEST_CASE("function: a root of an odd dimension gives a fractional exponent", "[function]")
{
    // The square root of a length is dimensionally half a length. Norms do ask
    // for this, so it must be expressible rather than rejected.
    constexpr auto odd = formula::sqrt(var<Edge>);

    STATIC_REQUIRE(odd.dimension.length == formula::exponent(1, 2));
}

TEST_CASE("function: pi is dimensionless", "[function]")
{
    STATIC_REQUIRE(formula::is_dimensionless(formula::PiNode::dimension));
    STATIC_REQUIRE(formula::Node<formula::PiNode>);
}

TEST_CASE("function: a power evaluates exactly", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Diameter> { rat(150) });
    constexpr auto computed = formula::checked_evaluate<Area>(formula::pow<2>(var<Diameter>), inputs);

    // 150 mm is 3/20 m, squared is 9/400 m2.
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(9, 400));
}

TEST_CASE("function: an exact root evaluates exactly", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Area> { rat(9, 400) });
    constexpr auto computed = formula::checked_evaluate<Edge>(formula::sqrt(var<Area>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 20));
}

TEST_CASE("function: an inexact root is refused by the exact representation", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Area> { rat(2) });
    constexpr auto computed = formula::checked_evaluate<Edge>(formula::sqrt(var<Area>), inputs);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Inexact);
}

TEST_CASE("function: the double representation answers where the exact one cannot", "[function]")
{
    auto const inputs = formula::environment(formula::Measured<Area> { rat(2) });
    auto const computed = formula::checked_evaluate_si<double>(formula::sqrt(var<Area>), inputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->has_value());
    CHECK(**computed > 1.41421356);
    CHECK(**computed < 1.41421357);
}

TEST_CASE("function: pi evaluates to the documented approximation", "[function]")
{
    constexpr auto inputs = formula::environment();
    constexpr auto computed = formula::checked_evaluate_si<formula::Rational>(formula::pi, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(**computed == formula::Pi);
}

TEST_CASE("function: a circular area reads like the formula it is", "[function]")
{
    // A = pi * d^2 / 4, with an exact-rational pi.
    constexpr auto area = formula::pi * formula::pow<2>(var<Diameter>) / rat(4);
    constexpr auto inputs = formula::environment(formula::Measured<Diameter> { rat(100) });
    constexpr auto computed = formula::checked_evaluate<Area>(area, inputs);

    STATIC_REQUIRE(computed.has_value());
    CHECK(computed->measurement().value().to_double() > 0.00785398);
    CHECK(computed->measurement().value().to_double() < 0.00785399);
}

TEST_CASE("function: an absent input still propagates through a power", "[function]")
{
    constexpr auto inputs = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto computed = formula::checked_evaluate<Area>(formula::pow<2>(var<Diameter>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}
