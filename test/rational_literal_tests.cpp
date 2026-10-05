// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace formula::literals;
using formula::Rational;

TEST_CASE("_r: an integer spelling is that integer", "[rational][literal]")
{
    STATIC_REQUIRE(457_r == Rational { 457 });
    STATIC_REQUIRE(0_r == Rational {});
    STATIC_REQUIRE(1'000'003_r == Rational { 1'000'003 });
    STATIC_REQUIRE(9'223'372'036'854'775'807_r == Rational { formula::detail::IntMax });
}

TEST_CASE("_r: a decimal spelling is its exact value, not the nearest double", "[rational][literal]")
{
    STATIC_REQUIRE(27.3_r == Rational { 273, 10 });
    STATIC_REQUIRE(0.47_r == Rational { 47, 100 }); // the double 0.47 is not 47/100
    STATIC_REQUIRE(0.0213_r == Rational { 213, 10'000 });
    STATIC_REQUIRE(.5_r == Rational { 1, 2 });
    STATIC_REQUIRE(5._r == Rational { 5 });
    STATIC_REQUIRE(1'000.5_r == Rational { 2'001, 2 });
    STATIC_REQUIRE(1_r / 3 == Rational { 1, 3 }); // a fraction is still spelled as one
}

TEST_CASE("_r: trailing fractional zeros cost nothing", "[rational][literal]")
{
    STATIC_REQUIRE(4.210_r == Rational { 421, 100 });
    STATIC_REQUIRE(1.500000000000000000000000_r == Rational { 3, 2 }); // 24 places, most of them zeros
}

TEST_CASE("_r: an exponent scales exactly", "[rational][literal]")
{
    STATIC_REQUIRE(1.5e-3_r == Rational { 3, 2'000 });
    STATIC_REQUIRE(7e2_r == Rational { 700 });
    STATIC_REQUIRE(1.3E+2_r == Rational { 130 });
    STATIC_REQUIRE(1'3.7e1_r == Rational { 137 });
}

TEST_CASE("_r: a minus sign is Rational's own negation", "[rational][literal]")
{
    STATIC_REQUIRE(-27.3_r == Rational { -273, 10 });
}

TEST_CASE("_r: the edges of what is exact", "[rational][literal]")
{
    STATIC_REQUIRE(00.5_r == Rational { 1, 2 });     // a leading zero is fine once there is a point
    STATIC_REQUIRE(0e3_r == Rational {});
    STATIC_REQUIRE(0e1001_r == Rational {}); // zero at any exponent, as parse_decimal_text reads "0e99999"
    STATIC_REQUIRE(1e-18_r == Rational { 1, 1'000'000'000'000'000'000 });
}

TEST_CASE("_r: a literal wider than 64 bits", "[rational][literal]")
{
    STATIC_REQUIRE(12'345'678'901'234'567'890_r == 1'234'567'890_r * 10'000'000'000_r + 1'234'567'890_r);
    STATIC_REQUIRE(0.000'000'000'000'000'000'1_r == *Rational::from_decimal(1, -19));
}
