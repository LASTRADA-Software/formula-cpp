// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Strength: formula::Quantity<Strength, "f", "material strength", unit::Megapascal>
{
};

// A dimensionless result quantity for `numeric_value_of`'s output to land in.
struct NumericValue: formula::Quantity<NumericValue, "n", "a bare number read off a quantity", unit::One>
{
};

// Every justification in this repository is invented. Naming a real standard
// would put copyrighted material in a public repository. The text is repeated
// at each call site rather than factored into a named constant: the
// justification is a `FixedString` non-type template parameter, so it must be
// a string literal (or an equivalent constant array) at the point of use, the
// same way a `Quantity`'s symbol and description are always written inline.
constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

[[nodiscard]] constexpr auto megapascals(long long value)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { value } });
}
} // namespace

TEST_CASE("numeric_value_of reads the operand's number in the unit it names", "[escape]")
{
    // 60 MPa read in megapascals is 60; the same 60 MPa read in pascals is
    // 60000000 -- these must differ, or the named unit is not load-bearing.
    constexpr auto inMegapascals = formula::numeric_value_of<unit::Megapascal,
                                                             "Example Standard 9:2020 states this coefficient "
                                                             "over the numeric value in MPa">(var<Strength>);
    constexpr auto inPascals = formula::numeric_value_of<unit::Pascal,
                                                         "Example Standard 9:2020 states this coefficient over "
                                                         "the numeric value in MPa">(var<Strength>);
    constexpr auto environment = megapascals(60);

    constexpr auto megapascalResult = formula::checked_evaluate<NumericValue>(inMegapascals, environment);
    REQUIRE(megapascalResult.has_value());
    REQUIRE(megapascalResult->is_value());
    CHECK(megapascalResult->measurement().value() == formula::Rational { 60 });

    constexpr auto pascalResult = formula::checked_evaluate<NumericValue>(inPascals, environment);
    REQUIRE(pascalResult.has_value());
    REQUIRE(pascalResult->is_value());
    CHECK(pascalResult->measurement().value() == formula::Rational { 60000000 });
}

TEST_CASE("numeric_value_of produces a dimensionless value", "[escape]")
{
    constexpr auto node = formula::numeric_value_of<unit::Megapascal,
                                                    "Example Standard 9:2020 states this coefficient over the "
                                                    "numeric value in MPa">(var<Strength>);
    STATIC_REQUIRE(decltype(node)::dimension == formula::dim::Scalar);
}

TEST_CASE("numeric_value_of carries its justification, readably", "[escape]")
{
    constexpr auto node = formula::numeric_value_of<unit::Megapascal,
                                                    "Example Standard 9:2020 states this coefficient over the "
                                                    "numeric value in MPa">(var<Strength>);
    STATIC_REQUIRE(decltype(node)::justification
                   == std::string_view { "Example Standard 9:2020 states this coefficient over the numeric "
                                         "value in MPa" });
}

TEST_CASE("an absent operand stays absent rather than becoming a bare zero", "[escape]")
{
    constexpr auto node = formula::numeric_value_of<unit::Megapascal,
                                                    "Example Standard 9:2020 states this coefficient over the "
                                                    "numeric value in MPa">(var<Strength>);
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto result = formula::checked_evaluate<NumericValue>(node, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}

TEST_CASE("numeric_value_of composes with the rest of a formula", "[escape]")
{
    // Multiplying by a dimensionless coefficient must still be dimensionless
    // and must still evaluate -- the escape hatch produces an ordinary node,
    // not a dead end.
    constexpr auto node = formula::numeric_value_of<unit::Megapascal,
                                                    "Example Standard 9:2020 states this coefficient over the "
                                                    "numeric value in MPa">(var<Strength>)
                          * rat(2);
    STATIC_REQUIRE(decltype(node)::dimension == formula::dim::Scalar);

    constexpr auto environment = megapascals(60);
    constexpr auto result = formula::checked_evaluate<NumericValue>(node, environment);
    REQUIRE(result.has_value());
    REQUIRE(result->is_value());
    CHECK(result->measurement().value() == formula::Rational { 120 });
}
