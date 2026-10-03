// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/function.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>

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

struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};
struct Divisor: formula::Quantity<Divisor, "q", "an invented divisor", formula::unit::One>
{
};
struct Share: formula::Quantity<Share, "p", "an invented share", formula::unit::Percent>
{
};

/// @p node evaluated exactly, with the ratio at @p ratioValue.
template <typename N>
[[nodiscard]] constexpr formula::Evaluated<formula::Rational> exactlyAt(N const& node, formula::Rational ratioValue)
{
    return formula::checked_evaluate_si<formula::Rational>(node,
                                                           formula::environment(formula::Measured<Ratio> { ratioValue }));
}

/// @p node evaluated in double, with the ratio at @p ratioValue.
template <typename N>
[[nodiscard]] formula::Evaluated<double> approximatelyAt(N const& node, formula::Rational ratioValue)
{
    return formula::checked_evaluate_si<double>(node, formula::environment(formula::Measured<Ratio> { ratioValue }));
}

/// The error @p evaluated failed with, or nothing when it did not fail.
template <typename Rep>
[[nodiscard]] constexpr std::optional<formula::ArithmeticError> failureOf(formula::Evaluated<Rep> const& evaluated)
{
    return evaluated.has_value() ? std::nullopt : std::optional<formula::ArithmeticError> { evaluated.error() };
}

/// Whether `formula::ln` accepts an argument of type @p T.
template <typename T>
concept LogarithmAccepts = requires(T argument) { formula::ln(argument); };

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

TEST_CASE("function: cbrt evaluates exactly, not merely a root of the right dimension", "[function]")
{
    // Only cbrt's dimension was ever asserted elsewhere; nothing checked the
    // number. 27 m3 is an exact cube, so a degree silently substituted for 3
    // (sqrt(27) is irrational) would turn this from a value into Inexact.
    constexpr auto inputs = formula::environment(formula::Measured<Volume> { rat(27) });
    constexpr auto computed = formula::checked_evaluate<Edge>(formula::cbrt(var<Volume>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(3));
}

TEST_CASE("function: the general root spelling reaches degrees sqrt and cbrt do not name", "[function]")
{
    // 16 m3 to the fourth root is 2, exactly. Using a constant rather than a
    // variable operand also pins that the operand itself, not just the
    // degree, is forwarded: a default-constructed operand would carry 0
    // instead of 16.
    constexpr auto rooted = formula::root<4>(formula::constant<formula::unit::CubicMetre>(rat(16)));
    constexpr formula::Evaluated<formula::Rational> computed =
        formula::checked_evaluate_si<formula::Rational>(rooted, formula::environment());

    STATIC_REQUIRE(decltype(rooted)::degree == 4);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->has_value());
    STATIC_REQUIRE(**computed == rat(2));
}

TEST_CASE("function: a negative exponent inverts the value, not only the dimension", "[function]")
{
    // 4 mm is 1/250 m; raised to the power -1 that is exactly 250. Clamping a
    // negative exponent to 1 -- so this would compute 1/250 instead -- left
    // every existing test green, since only ::dimension was ever asserted for
    // a negative exponent.
    constexpr auto inputs = formula::environment(formula::Measured<Diameter> { rat(4) });
    constexpr formula::Evaluated<formula::Rational> computed =
        formula::checked_evaluate_si<formula::Rational>(formula::pow<-1>(var<Diameter>), inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->has_value());
    STATIC_REQUIRE(**computed == rat(250));
}

TEST_CASE("function: the double representation raises to a power", "[function]")
{
    // Only the exact Rational representation's power evaluation was ever
    // checked against a number; the double specialisation's raise() was
    // exercised nowhere.
    auto const inputs = formula::environment(formula::Measured<Diameter> { rat(200) });
    auto const computed = formula::checked_evaluate_si<double>(formula::pow<2>(var<Diameter>), inputs);

    // 200 mm is 0.2 m; squared is 0.04 m2.
    REQUIRE(computed.has_value());
    REQUIRE(computed->has_value());
    CHECK(**computed > 0.0399999);
    CHECK(**computed < 0.0400001);
}

TEST_CASE("function: the double representation roots a negative value at an odd degree", "[function]")
{
    // The sign branch of RepFunctions<double>::root is only reachable for a
    // negative operand at an odd degree; cbrt(-8) is -2, and dropping the
    // branch entirely (falling through to std::pow(-8, 1/3), which is NaN for
    // a negative base) would leave every other test green.
    auto const inputs = formula::environment(formula::Measured<Volume> { rat(-8) });
    auto const computed = formula::checked_evaluate_si<double>(formula::cbrt(var<Volume>), inputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->has_value());
    CHECK(**computed > -2.0000001);
    CHECK(**computed < -1.9999999);
}

TEST_CASE("function: the double representation refuses an even root of a negative value", "[function]")
{
    // RepFunctions<double>::root's even-degree guard, beside the odd-degree
    // sign branch tested above: sqrt(-4) has no real answer. Falling through
    // to std::pow(4, 0.5) would silently answer 2.0, a wrong number wearing a
    // right one's clothes.
    auto const inputs = formula::environment(formula::Measured<Area> { rat(-4) });
    auto const computed = formula::checked_evaluate_si<double>(formula::sqrt(var<Area>), inputs);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("function: the double representation approximates pi", "[function]")
{
    // Pi is otherwise only ever evaluated in the exact Rational representation.
    auto const inputs = formula::environment();
    auto const computed = formula::checked_evaluate_si<double>(formula::pi, inputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->has_value());
    CHECK(**computed > 3.14159265);
    CHECK(**computed < 3.14159266);
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

TEST_CASE("function: a logarithm or an exponential is a dimensionless node over a dimensionless argument", "[function]")
{
    constexpr auto logarithm = formula::ln(var<Ratio>);
    STATIC_REQUIRE(formula::Node<decltype(logarithm)>);
    STATIC_REQUIRE(decltype(logarithm)::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(logarithm)::function == formula::Transcendental::NaturalLogarithm);
    STATIC_REQUIRE(decltype(formula::log10(var<Ratio>))::function == formula::Transcendental::DecimalLogarithm);
    STATIC_REQUIRE(decltype(formula::exp(var<Ratio>))::function == formula::Transcendental::Exponential);
    // A ratio of two areas is a bare number, and so is a percentage.
    STATIC_REQUIRE(decltype(formula::ln(var<Area> / var<Area>))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::exp(var<Share>))::dimension == formula::dim::Scalar);
    // A formula node only: a bare number is not one, so ln(2) never means a number function.
    STATIC_REQUIRE(LogarithmAccepts<decltype(var<Ratio>)>);
    STATIC_REQUIRE_FALSE(LogarithmAccepts<formula::Rational>);
    STATIC_REQUIRE_FALSE(LogarithmAccepts<double>);
}

TEST_CASE("function: ln log10 and exp are exact where their value is rational", "[function]")
{
    STATIC_REQUIRE(**exactlyAt(formula::ln(var<Ratio>), rat(1)) == rat(0));
    STATIC_REQUIRE(**exactlyAt(formula::exp(var<Ratio>), rat(0)) == rat(1));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1)) == rat(0));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1000)) == rat(3));
    // A power of ten written as one over a power of ten.
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1, 100)) == rat(-2));
    // 10^18, the largest power of ten a 64-bit integer holds, either way up.
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1'000'000'000'000'000'000)) == rat(18));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), rat(1, 1'000'000'000'000'000'000)) == rat(-18));
    // 10^38, the largest power of ten a Rational holds, either way up.
    constexpr formula::Rational::Int tenToNineteen = formula::Rational::Int { 1'000'000'000'000'000'000 } * 10;
    constexpr formula::Rational::Int tenToThirtyEight = tenToNineteen * tenToNineteen;
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), formula::Rational { tenToThirtyEight }) == rat(38));
    STATIC_REQUIRE(**exactlyAt(formula::log10(var<Ratio>), formula::Rational { 1, tenToThirtyEight }) == rat(-38));
}

TEST_CASE("function: a logarithm or an exponential of any other value is Inexact in Rational", "[function]")
{
    constexpr auto Inexact = formula::ArithmeticError::Inexact;
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(2))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(2))) == Inexact);
    // Not powers of ten: 20 ends in a zero, 1001/1000 has a power of ten below the line, 1000/3 above it.
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(20))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(1001, 1000))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(1000, 3))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(1))) == Inexact);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(-1))) == Inexact);
    // However large: exp 1000 is irrational before it is too large, and says so.
    STATIC_REQUIRE(failureOf(exactlyAt(formula::exp(var<Ratio>), rat(1000))) == Inexact);
}

TEST_CASE("function: the logarithm of zero or a negative value is a domain error in both representations", "[function]")
{
    constexpr auto DomainError = formula::ArithmeticError::DomainError;
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(0))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::ln(var<Ratio>), rat(-1))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(0))) == DomainError);
    STATIC_REQUIRE(failureOf(exactlyAt(formula::log10(var<Ratio>), rat(-1))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::ln(var<Ratio>), rat(0))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::ln(var<Ratio>), rat(-1))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::log10(var<Ratio>), rat(0))) == DomainError);
    CHECK(failureOf(approximatelyAt(formula::log10(var<Ratio>), rat(-1))) == DomainError);
    // The double guard is !(v > 0.0), so NaN -- which no Rational converts to -- is refused as well.
    auto const ofNaN = formula::RepFunctions<double>::natural_log(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(ofNaN.has_value());
    CHECK(ofNaN.error() == DomainError);
    auto const decimalOfNaN = formula::RepFunctions<double>::decimal_log(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(decimalOfNaN.has_value());
    CHECK(decimalOfNaN.error() == DomainError);
}

TEST_CASE("function: the double representation answers where the exact one refuses", "[function]")
{
    auto const naturalOfTwo = approximatelyAt(formula::ln(var<Ratio>), rat(2));
    REQUIRE(naturalOfTwo.has_value());
    REQUIRE(naturalOfTwo->has_value());
    CHECK(**naturalOfTwo > 0.69314718);
    CHECK(**naturalOfTwo < 0.69314719);
    auto const decimalOfTwo = approximatelyAt(formula::log10(var<Ratio>), rat(2));
    REQUIRE(decimalOfTwo.has_value());
    REQUIRE(decimalOfTwo->has_value());
    CHECK(**decimalOfTwo > 0.30102999);
    CHECK(**decimalOfTwo < 0.30103000);
    auto const exponentialOfOne = approximatelyAt(formula::exp(var<Ratio>), rat(1));
    REQUIRE(exponentialOfOne.has_value());
    REQUIRE(exponentialOfOne->has_value());
    CHECK(**exponentialOfOne > 2.71828182);
    CHECK(**exponentialOfOne < 2.71828183);
}

TEST_CASE("function: exp too large for double is +inf and passes as a power's does", "[function]")
{
    // RepFunctions<double>::raise lets inf through, and RepTraits<double> says why: the caller asked
    // for double. exp does the same.
    auto const huge = approximatelyAt(formula::exp(var<Ratio>), rat(1000));
    REQUIRE(huge.has_value());
    REQUIRE(huge->has_value());
    CHECK(**huge == std::numeric_limits<double>::infinity());
}

TEST_CASE("function: an absent argument leaves a logarithm or an exponential absent", "[function]")
{
    // Absent, never a domain error: an argument nobody measured is not zero.
    constexpr auto nothingMeasured = formula::environment(formula::Measured<Ratio>::absent());
    STATIC_REQUIRE(!formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Ratio>), nothingMeasured)->has_value());
    STATIC_REQUIRE(
        !formula::checked_evaluate_si<formula::Rational>(formula::log10(var<Ratio>), nothingMeasured)->has_value());
    STATIC_REQUIRE(!formula::checked_evaluate_si<formula::Rational>(formula::exp(var<Ratio>), nothingMeasured)->has_value());
    auto const approximate = formula::checked_evaluate_si<double>(formula::ln(var<Ratio>), nothingMeasured);
    REQUIRE(approximate.has_value());
    CHECK_FALSE(approximate->has_value());
}

TEST_CASE("function: the argument's own failure reaches a logarithm unchanged", "[function]")
{
    // r / q with q = 0 fails with DivisionByZero. The logarithm reports that, not DomainError, which a
    // node that looked at a default value in place of the failure would report.
    constexpr auto inputs = formula::environment(formula::Measured<Ratio> { rat(1) }, formula::Measured<Divisor> { rat(0) });
    constexpr auto DivisionByZero = formula::ArithmeticError::DivisionByZero;
    STATIC_REQUIRE(failureOf(formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Ratio> / var<Divisor>), inputs))
                   == DivisionByZero);
    STATIC_REQUIRE(
        failureOf(formula::checked_evaluate_si<formula::Rational>(formula::exp(var<Ratio> / var<Divisor>), inputs))
        == DivisionByZero);
    CHECK(failureOf(formula::checked_evaluate_si<double>(formula::log10(var<Ratio> / var<Divisor>), inputs))
          == DivisionByZero);
}

TEST_CASE("function: a percentage is read in the coherent unit under a logarithm", "[function]")
{
    // 1000 % is the number 10, so its decimal logarithm is 1 -- not 3, which reading it in percent gives.
    constexpr auto tenfold = formula::environment(formula::Measured<Share> { rat(1000) });
    STATIC_REQUIRE(**formula::checked_evaluate_si<formula::Rational>(formula::log10(var<Share>), tenfold) == rat(1));
    // 5 % is 0.05: ln 0.05 = -2.9957..., where ln 5 would be 1.6094....
    constexpr auto fivePercent = formula::environment(formula::Measured<Share> { rat(5) });
    auto const approximate = formula::checked_evaluate_si<double>(formula::ln(var<Share>), fivePercent);
    REQUIRE(approximate.has_value());
    REQUIRE(approximate->has_value());
    CHECK(**approximate > -2.9957323);
    CHECK(**approximate < -2.9957322);
    // Exactly, ln 0.05 is irrational.
    STATIC_REQUIRE(failureOf(formula::checked_evaluate_si<formula::Rational>(formula::ln(var<Share>), fivePercent))
                   == formula::ArithmeticError::Inexact);
}
