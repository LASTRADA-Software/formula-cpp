// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/environment.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

constexpr auto twoInputs =
    formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(300) });

} // namespace

TEST_CASE("environment: it answers for the quantities it holds", "[environment]")
{
    STATIC_REQUIRE(decltype(twoInputs)::provides<WaterVolume>);
    STATIC_REQUIRE(decltype(twoInputs)::provides<CementVolume>);
    STATIC_REQUIRE_FALSE(decltype(twoInputs)::provides<Ratio>);
}

TEST_CASE("environment: a value comes back as it went in", "[environment]")
{
    STATIC_REQUIRE(twoInputs.get<WaterVolume>().value() == rat(180));
    STATIC_REQUIRE(twoInputs.get<CementVolume>().value() == rat(300));
}

TEST_CASE("environment: an absent measurement stays absent", "[environment]")
{
    constexpr auto partial =
        formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(300) });

    STATIC_REQUIRE(partial.get<WaterVolume>().is_absent());
    STATIC_REQUIRE(partial.get<CementVolume>().has_value());
}

TEST_CASE("environment: a measurement is sourced as measured", "[environment]")
{
    STATIC_REQUIRE(twoInputs.source_of<WaterVolume>() == formula::ValueSource::Measured);
    STATIC_REQUIRE_FALSE(decltype(twoInputs)::is_entered<WaterVolume>);
}

TEST_CASE("environment: an entered value is sourced as manually entered", "[environment]")
{
    constexpr auto withOverride = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                       formula::entered(formula::Measured<Ratio> { rat(45, 100) }));

    STATIC_REQUIRE(decltype(withOverride)::provides<Ratio>);
    STATIC_REQUIRE(decltype(withOverride)::is_entered<Ratio>);
    STATIC_REQUIRE(withOverride.source_of<Ratio>() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(withOverride.get<Ratio>().value() == rat(45, 100));
    STATIC_REQUIRE_FALSE(decltype(withOverride)::is_entered<WaterVolume>);
}

TEST_CASE("environment: an empty environment provides nothing", "[environment]")
{
    constexpr auto nothing = formula::environment();

    STATIC_REQUIRE_FALSE(decltype(nothing)::provides<WaterVolume>);
}

TEST_CASE("environment: entries keep their identity whatever order they are given in", "[environment]")
{
    constexpr auto reversed =
        formula::environment(formula::Measured<CementVolume> { rat(300) }, formula::Measured<WaterVolume> { rat(180) });

    STATIC_REQUIRE(reversed.get<WaterVolume>().value() == rat(180));
    STATIC_REQUIRE(reversed.get<CementVolume>().value() == rat(300));
}
