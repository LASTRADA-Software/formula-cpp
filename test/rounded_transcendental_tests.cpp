// SPDX-License-Identifier: Apache-2.0
//
// Every expected value below was computed while planning with Python 3.13's decimal module at 150
// significant digits, and agrees with the kernel's own reference table (transcendental_tests.cpp).
//
// A value that reaches the kernel is checked at run time: one rounding through it costs about a quarter
// of a compiler's default constant-evaluation budget. The special points and the early exits, which
// never reach it, are checked at compile time.
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <expected>
#include <optional>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using formula::var;

struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", unit::One>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", unit::One>
{
};
struct Share: formula::Quantity<Share, "p", "an invented share", unit::Percent>
{
};

/// @p node evaluated exactly with the ratio at @p ratioValue: its value, or its error.
template <typename N>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> at(N const& node, Rational ratioValue)
{
    formula::Evaluated<Rational> const evaluated =
        formula::checked_evaluate_si<Rational>(node, formula::environment(formula::Measured<Ratio> { ratioValue }));
    if (!evaluated.has_value())
        return std::unexpected { evaluated.error() };
    if (!evaluated->has_value())
        return std::unexpected { formula::ArithmeticError::NotFinite };   // absent: no fixture expects it
    return **evaluated;
}

template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> lnAt(Rational ratioValue)
{
    return at(formula::rounded_ln<Places, Mode>(var<Ratio>), ratioValue);
}
template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> log10At(Rational ratioValue)
{
    return at(formula::rounded_log10<Places, Mode>(var<Ratio>), ratioValue);
}
template <DecimalPlaces Places, RoundingMode Mode>
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> expAt(Rational ratioValue)
{
    return at(formula::rounded_exp<Places, Mode>(var<Ratio>), ratioValue);
}

using Outcome = std::expected<Rational, formula::ArithmeticError>;
constexpr Outcome overflow { std::unexpected { formula::ArithmeticError::Overflow } };
constexpr Outcome domainError { std::unexpected { formula::ArithmeticError::DomainError } };
} // namespace

TEST_CASE("rounded_transcendental: ln 2 to 4 dp in every mode and of 1/2 with the directions paired the other way",
          "[rounded_transcendental]")
{
    // ln 2 = 0.693147...: the nearest and downward modes keep 0.6931, the upward ones 0.6932.
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfTowardZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 2 }) == Rational { 1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 2 }) == Rational { 6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::AwayFromZero>(Rational { 2 }) == Rational { 1733, 2500 });
    // ln 1/2 = -0.693147...: Ceiling and TowardZero keep -0.6931, Floor and AwayFromZero -0.6932.
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 1, 2 }) == Rational { -1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::AwayFromZero>(Rational { 1, 2 }) == Rational { -1733, 2500 });
    CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1, 2 }) == Rational { -6931, 10000 });
    // A pure number, whatever the argument was.
    STATIC_REQUIRE(decltype(formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>))::dimension
                   == formula::dim::Scalar);
}

TEST_CASE("rounded_transcendental: log10 2 to 3 dp and exp 1 to 4 dp and exp -1 to 3 dp", "[rounded_transcendental]")
{
    // log10 2 = 0.30102...: 0.301, and 0.302 upward.
    CHECK(log10At<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { 2 }) == Rational { 301, 1000 });
    CHECK(log10At<DecimalPlaces { 3 }, RoundingMode::Ceiling>(Rational { 2 }) == Rational { 151, 500 });
    // exp 1 = 2.718281...: the nearest modes round up to 2.7183, where Floor and TowardZero keep 2.7182.
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(Rational { 1 }) == Rational { 27183, 10000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 1 }) == Rational { 27183, 10000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::Floor>(Rational { 1 }) == Rational { 13591, 5000 });
    CHECK(expAt<DecimalPlaces { 4 }, RoundingMode::TowardZero>(Rational { 1 }) == Rational { 13591, 5000 });
    // exp -1 = 0.367879...: 0.368 = 46/125, and 0.367 downward.
    CHECK(expAt<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { -1 }) == Rational { 46, 125 });
    CHECK(expAt<DecimalPlaces { 3 }, RoundingMode::Floor>(Rational { -1 }) == Rational { 367, 1000 });
}

TEST_CASE("rounded_transcendental: a special point ties and the mode decides it", "[rounded_transcendental]")
{
    // log10 10^15 = 15, exactly halfway between 10 and 20 at -1 dp.
    constexpr Rational tenTo15 { 1'000'000'000'000'000 };
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(tenTo15) == Rational { 20 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(tenTo15) == Rational { 10 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfEven>(tenTo15) == Rational { 20 });
    // log10 10^5 = 5, halfway between 0 and 10: even is 0 this time.
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 100'000 }) == Rational { 10 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(Rational { 100'000 }) == Rational {});
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfEven>(Rational { 100'000 }) == Rational {});
    // log10 10^-15 = -15, written as one over a power of ten.
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero>(Rational { 1, 1'000'000'000'000'000 })
                   == Rational { -20 });
    STATIC_REQUIRE(log10At<DecimalPlaces { -1 }, RoundingMode::HalfTowardZero>(Rational { 1, 1'000'000'000'000'000 })
                   == Rational { -10 });
    // ln 1 and exp 0 are exact too.
    STATIC_REQUIRE(lnAt<DecimalPlaces { 4 }, RoundingMode::Ceiling>(Rational { 1 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { -1 }, RoundingMode::HalfEven>(Rational {}) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { -1 }, RoundingMode::Ceiling>(Rational {}) == Rational { 10 });
}

TEST_CASE("rounded_transcendental: an exponential too large to hold is Overflow", "[rounded_transcendental]")
{
    // exp 50 = 5.18 * 10^21: past the early bound.
    STATIC_REQUIRE(expAt<DecimalPlaces { 6 }, RoundingMode::HalfAwayFromZero>(Rational { 50 }) == overflow);
    // exp 43.7 = 9.52 * 10^18: through the kernel. Floor to whole 10^18s keeps 9 * 10^18, which fits;
    // the nearest modes give 10^19, which does not.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 437, 10 }) == Rational { 9'000'000'000'000'000'000 });
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::HalfAwayFromZero>(Rational { 437, 10 }) == overflow);
    // exp 44 = 1.29 * 10^19 fits at no places.
    CHECK(expAt<DecimalPlaces { -18 }, RoundingMode::Floor>(Rational { 44 }) == overflow);
    // exp 43 = 4727839468229346561.47...: whole, it fits.
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero>(Rational { 43 })
          == Rational { 4'727'839'468'229'346'561 });
    CHECK(expAt<DecimalPlaces { 0 }, RoundingMode::Ceiling>(Rational { 43 }) == Rational { 4'727'839'468'229'346'562 });
}

TEST_CASE("rounded_transcendental: a tiny exponential is zero or one unit by mode", "[rounded_transcendental]")
{
    // exp -50 = 1.9 * 10^-22, below a quarter of any last kept unit: 0, except one unit upward.
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::HalfAwayFromZero>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::Floor>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::TowardZero>(Rational { -50 }) == Rational {});
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::Ceiling>(Rational { -50 }) == Rational { 1, 1000 });
    STATIC_REQUIRE(expAt<DecimalPlaces { 3 }, RoundingMode::AwayFromZero>(Rational { -50 }) == Rational { 1, 1000 });
    STATIC_REQUIRE(expAt<DecimalPlaces { -2 }, RoundingMode::Ceiling>(Rational { -50 }) == Rational { 100 });
    STATIC_REQUIRE(expAt<DecimalPlaces { -2 }, RoundingMode::HalfEven>(Rational { -50 }) == Rational {});
    // exp -43 = 2.1 * 10^-19, through the kernel at its lowest power of two, 2^-63.
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(Rational { -43 }) == Rational {});
    CHECK(expAt<DecimalPlaces { 18 }, RoundingMode::Ceiling>(Rational { -43 }) == Rational { 1, 1'000'000'000'000'000'000 });
}

TEST_CASE("rounded_transcendental: absence and failures come first and in order", "[rounded_transcendental]")
{
    constexpr auto nothingMeasured = formula::environment(formula::Measured<Ratio>::absent());
    STATIC_REQUIRE(!formula::checked_evaluate_si<Rational>(
                        formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>), nothingMeasured)
                        ->has_value());
    STATIC_REQUIRE(!formula::checked_evaluate_si<Rational>(
                        formula::rounded_exp<DecimalPlaces { 4 }, RoundingMode::HalfEven>(var<Ratio>), nothingMeasured)
                        ->has_value());
    // The argument's own failure, unchanged.
    constexpr auto dividedByZero = formula::environment(formula::Measured<Ratio> { Rational { 1 } },
                                                        formula::Measured<Divisor> { Rational {} });
    STATIC_REQUIRE(formula::checked_evaluate_si<Rational>(
                       formula::rounded_log10<DecimalPlaces { 2 }, RoundingMode::HalfEven>(var<Ratio> / var<Divisor>),
                       dividedByZero)
                       .error()
                   == formula::ArithmeticError::DivisionByZero);
    // A logarithm of zero or below; before places out of range.
    STATIC_REQUIRE(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational {}) == domainError);
    STATIC_REQUIRE(log10At<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { -1 }) == domainError);
    STATIC_REQUIRE(lnAt<DecimalPlaces { 19 }, RoundingMode::HalfEven>(Rational { -1 }) == domainError);
    STATIC_REQUIRE(lnAt<DecimalPlaces { 19 }, RoundingMode::HalfEven>(Rational { 2 }) == overflow);
    STATIC_REQUIRE(lnAt<DecimalPlaces { -19 }, RoundingMode::HalfEven>(Rational { 1 }) == overflow);
    // Places out of range before the tiny rule.
    STATIC_REQUIRE(expAt<DecimalPlaces { 19 }, RoundingMode::Ceiling>(Rational { -50 }) == overflow);
}

TEST_CASE("rounded_transcendental: a percentage is read in the coherent unit", "[rounded_transcendental]")
{
    // 5 % is 0.05: ln 0.05 = -2.99573..., -2.9957; ln 5 would give 1.6094. Through the kernel, so at run time.
    constexpr auto fivePercent = formula::environment(formula::Measured<Share> { Rational { 5 } });
    CHECK(**formula::checked_evaluate_si<Rational>(
              formula::rounded_ln<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(var<Share>), fivePercent)
          == Rational { -29957, 10000 });
    // 1000 % is 10, a special point: exactly 1, at compile time.
    constexpr auto tenfold = formula::environment(formula::Measured<Share> { Rational { 1000 } });
    STATIC_REQUIRE(**formula::checked_evaluate_si<Rational>(
                       formula::rounded_log10<DecimalPlaces { 0 }, RoundingMode::HalfEven>(var<Share>), tenfold)
                   == Rational { 1 });
}

TEST_CASE("rounded_transcendental: a value a double cannot tell from a boundary rounds right", "[rounded_transcendental]")
{
    // log10(10^18 - 1) = 17.99999999999999999956...: 4.3 * 10^-19 below 18. The doubles next to 18 are
    // 3.6 * 10^-15 apart, so a double computation can answer no better than 18, and 18.00 under Floor.
    CHECK(log10At<DecimalPlaces { 2 }, RoundingMode::Floor>(Rational { 999'999'999'999'999'999 }) == Rational { 1799, 100 });
    CHECK(log10At<DecimalPlaces { 17 }, RoundingMode::Floor>(Rational { 999'999'999'999'999'999 })
          == Rational::from_decimal(1'799'999'999'999'999'999, -17));
    CHECK(log10At<DecimalPlaces { 17 }, RoundingMode::HalfEven>(Rational { 999'999'999'999'999'999 }) == Rational { 18 });
}

TEST_CASE("rounded_transcendental: a rounding the kernel cannot decide is Overflow", "[rounded_transcendental]")
{
    // 1.25 * 10^-37 below a tie at 18 places: the half modes cannot be decided (the true rounding is 0).
    constexpr Rational nearTie { 2'000'000'000'000'000'001, 2'000'000'000'000'000'000 };
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::HalfEven>(nearTie) == overflow);
    CHECK(lnAt<DecimalPlaces { 18 }, RoundingMode::Floor>(nearTie) == Rational {});
}

TEST_CASE("rounded_transcendental: rounded of a plain logarithm stays an exact logarithm that fails",
          "[rounded_transcendental]")
{
    // rounded<>(ln(x)) does not become rounded_ln: the plain node fails before the rounding sees a value.
    STATIC_REQUIRE(
        at(formula::rounded<unit::One, DecimalPlaces { 4 }, RoundingMode::HalfEven>(formula::ln(var<Ratio>)), Rational { 2 })
        == Outcome { std::unexpected { formula::ArithmeticError::Inexact } });
}
