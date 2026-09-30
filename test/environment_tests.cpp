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
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
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

TEST_CASE("environment: Entered compares by the measurement it carries", "[environment]")
{
    constexpr auto low = formula::entered(formula::Measured<Ratio> { rat(45, 100) });
    constexpr auto sameAsLow = formula::entered(formula::Measured<Ratio> { rat(45, 100) });
    constexpr auto high = formula::entered(formula::Measured<Ratio> { rat(50, 100) });
    constexpr auto absent = formula::entered(formula::Measured<Ratio>::absent());

    STATIC_REQUIRE(low == sameAsLow);
    STATIC_REQUIRE_FALSE(low == high);
    STATIC_REQUIRE_FALSE(low == absent);
    STATIC_REQUIRE(absent == formula::entered(formula::Measured<Ratio>::absent()));
}

TEST_CASE("measured_series: plain numbers and not_measured stand for elements", "[environment][series]")
{
    using namespace formula::literals;
    constexpr auto mixed = formula::measured_series<Retained>(127, 10.3_r, formula::not_measured, formula::Rational { 1, 3 });
    STATIC_REQUIRE(mixed.size() == 4);
    STATIC_REQUIRE(mixed.element(0) == formula::Measured<Retained> { 127 });
    STATIC_REQUIRE(mixed.element(1) == formula::Measured<Retained> { formula::Rational { 103, 10 } });
    STATIC_REQUIRE(mixed.element(2).is_absent());
    STATIC_REQUIRE(mixed.element(3) == formula::Measured<Retained> { formula::Rational { 1, 3 } });
    // The old spelling is unchanged.
    constexpr auto spelled = formula::measured_series<Retained>(formula::Measured<Retained> { 127 }, formula::Measured<Retained>::absent());
    STATIC_REQUIRE(spelled.element(0) == mixed.element(0));
    STATIC_REQUIRE(spelled.element(1).is_absent());
}
