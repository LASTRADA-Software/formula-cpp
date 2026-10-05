// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>

using formula::ArithmeticError;
using formula::ParsedDecimal;
using formula::Rational;

namespace
{
    constexpr bool parses_as(std::string_view spelling, Rational expectedValue, std::int32_t expectedPlaces)
    {
        std::expected<ParsedDecimal, ArithmeticError> const parsed = formula::parse_decimal_text(spelling);
        return parsed.has_value() && parsed->value == expectedValue && parsed->places == expectedPlaces;
    }

    constexpr bool refused_as(std::string_view spelling, ArithmeticError expectedError)
    {
        std::expected<ParsedDecimal, ArithmeticError> const parsed = formula::parse_decimal_text(spelling);
        return !parsed.has_value() && parsed.error() == expectedError;
    }
} // namespace

TEST_CASE("parse_decimal_text: the value and the places as typed", "[rational][text]")
{
    STATIC_REQUIRE(parses_as("2.400", Rational { 12, 5 }, 3));
    STATIC_REQUIRE(parses_as("2.4", Rational { 12, 5 }, 1));
    STATIC_REQUIRE(parses_as("7", Rational { 7 }, 0));
    STATIC_REQUIRE(parses_as("1.5e3", Rational { 1500 }, 0));
    STATIC_REQUIRE(parses_as("15e-1", Rational { 3, 2 }, 1));
    STATIC_REQUIRE(parses_as("1e-3", Rational { 1, 1000 }, 3));
    STATIC_REQUIRE(parses_as("2.5E+2", Rational { 250 }, 0));
    STATIC_REQUIRE(parses_as("-27.3", Rational { -273, 10 }, 1));
    STATIC_REQUIRE(parses_as("+0.0", Rational {}, 1));
    STATIC_REQUIRE(parses_as("-0", Rational {}, 0));
    STATIC_REQUIRE(parses_as("-0.0", Rational {}, 1));
    STATIC_REQUIRE(parses_as(".5", Rational { 1, 2 }, 1));
    STATIC_REQUIRE(parses_as("5.", Rational { 5 }, 0));
    STATIC_REQUIRE(parses_as("007", Rational { 7 }, 0));
    STATIC_REQUIRE(parses_as("0e99999", Rational {}, 0));
    STATIC_REQUIRE(parses_as("0e-12345", Rational {}, 12345)); // a zero's places count every exponent digit
    STATIC_REQUIRE(parses_as("0.000e5", Rational {}, 0));
}

TEST_CASE("parse_decimal_text: text that is not a decimal is DomainError", "[rational][text]")
{
    for (std::string_view const spelling: { "", " 1", "1 ", "1,5", "1'000", "1_000", "1.2.3", "0x1F", "inf", "nan",
                                            "1e", "1e+", "e5", "+", "-", ".", "+-1", "1e2.5", "--1" })
    {
        CAPTURE(spelling);
        REQUIRE(refused_as(spelling, ArithmeticError::DomainError));
    }
}

TEST_CASE("parse_decimal_text: a value no Rational holds is Overflow", "[rational][text]")
{
    STATIC_REQUIRE(parses_as("170141183460469231731687303715884105727",
                             Rational { std::numeric_limits<formula::Int128>::max() }, 0));
    STATIC_REQUIRE(refused_as("170141183460469231731687303715884105728", ArithmeticError::Overflow));
    STATIC_REQUIRE(parses_as("1e38", *Rational::from_decimal(1, 38), 0));
    STATIC_REQUIRE(refused_as("1e39", ArithmeticError::Overflow));
    STATIC_REQUIRE(refused_as("1e-39", ArithmeticError::Overflow));
    STATIC_REQUIRE(refused_as("1e1001", ArithmeticError::Overflow));
    STATIC_REQUIRE(refused_as("0e-99999999999", ArithmeticError::Overflow)); // places no std::int32_t states
    // A thousand fractional zeros before a digit: refused, not a wrong value and not a hang.
    std::string const deepFraction = "0." + std::string(1000, '0') + "1";
    REQUIRE(refused_as(deepFraction, ArithmeticError::Overflow));
    // Trailing fractional zeros cost nothing, however many.
    std::string const manyTrailingZeros = "1." + std::string(1000, '0');
    std::expected<ParsedDecimal, ArithmeticError> const trailing = formula::parse_decimal_text(manyTrailingZeros);
    REQUIRE(trailing.has_value());
    REQUIRE(trailing->value == Rational { 1 });
    REQUIRE(trailing->places == 1000);
}

TEST_CASE("Rational::from_decimal_text: the value parse_decimal_text reads", "[rational][text]")
{
    STATIC_REQUIRE(*Rational::from_decimal_text("2.400") == Rational { 12, 5 });
    STATIC_REQUIRE(Rational::from_decimal_text("2,4").error() == ArithmeticError::DomainError);
    std::string const fromCsv = "0.0213";
    std::expected<Rational, ArithmeticError> const atRunTime = Rational::from_decimal_text(fromCsv);
    REQUIRE(atRunTime.has_value());
    REQUIRE(*atRunTime == Rational { 213, 10'000 });
}
