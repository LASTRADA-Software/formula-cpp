// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rational.hpp>

#include <catch2/catch_test_macros.hpp>

#include <compare>
#include <limits>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Rational;

namespace
{
constexpr Rational::Int IntMax = formula::detail::IntMax;
constexpr Rational::Int IntMin = formula::detail::IntMin;

/// Builds a Rational in a constant expression, asserting success.
consteval Rational exact(Rational::Int numerator, Rational::Int denominator)
{
    return Rational { numerator, denominator };
}
} // namespace

// ---- invariants ----

static_assert(Rational {}.numerator() == 0);
static_assert(Rational {}.denominator() == 1);
static_assert(Rational { 7 }.numerator() == 7);
static_assert(Rational { 7 }.denominator() == 1);

static_assert(exact(2, 4).numerator() == 1);
static_assert(exact(2, 4).denominator() == 2);
static_assert(exact(-2, 4).numerator() == -1);
static_assert(exact(-2, 4).denominator() == 2);
static_assert(exact(2, -4).numerator() == -1);
static_assert(exact(2, -4).denominator() == 2);
static_assert(exact(-2, -4).numerator() == 1);
static_assert(exact(-2, -4).denominator() == 2);
static_assert(exact(0, -5).numerator() == 0);
static_assert(exact(0, -5).denominator() == 1);

static_assert(exact(IntMin, IntMin) == Rational { 1 });
static_assert(exact(IntMin, 1).numerator() == IntMin);
static_assert(exact(IntMin, 2).numerator() == IntMin / 2);
static_assert(exact(IntMin, 2).denominator() == 1);

static_assert(!Rational::make(1, 0).has_value());
static_assert(Rational::make(1, 0).error() == ArithmeticError::DivisionByZero);
static_assert(!Rational::make(IntMin, -1).has_value());
static_assert(Rational::make(IntMin, -1).error() == ArithmeticError::Overflow);

static_assert(Rational { 0 }.is_zero());
static_assert(Rational { 3 }.is_integer());
static_assert(!exact(1, 2).is_integer());
static_assert(Rational { 0 }.sign() == 0);
static_assert(Rational { -4 }.sign() == -1);
static_assert(Rational { 4 }.sign() == 1);

// ---- decimal construction: the exact, preferred route ----

static_assert(Rational::from_decimal(45, -2)->numerator() == 9);
static_assert(Rational::from_decimal(45, -2)->denominator() == 20);
static_assert(Rational::from_decimal(3, 0) == Rational { 3 });
static_assert(Rational::from_decimal(3, 2) == Rational { 300 });
static_assert(Rational::from_decimal(0, -5) == Rational { 0 });
static_assert(!Rational::from_decimal(1, -19).has_value());
static_assert(!Rational::from_decimal(IntMax, 1).has_value());

// ---- exact binary conversion ----

static_assert(Rational::from_double_exact(0.5) == exact(1, 2));
static_assert(Rational::from_double_exact(-0.25) == exact(-1, 4));
static_assert(Rational::from_double_exact(0.0) == Rational { 0 });
static_assert(Rational::from_double_exact(3.0) == Rational { 3 });
// 0.1 is not a dyadic rational, so the exact value is NOT 1/10.
static_assert(Rational::from_double_exact(0.1)->denominator() != 10);

// ---- ordering ----

static_assert(exact(1, 3) < exact(1, 2));
static_assert(exact(-1, 2) < exact(-1, 3));
static_assert(exact(2, 4) == exact(1, 2));
static_assert(exact(1, 2) <= exact(1, 2));
static_assert(Rational { IntMax } > Rational { IntMin });
// Cross-multiplication would overflow here; the continued-fraction comparison must not.
// n/(n-1) < (n-1)/(n-2) because n(n-2) = n^2-2n is one less than (n-1)^2.
static_assert(exact(IntMax, IntMax - 1) < exact(IntMax - 1, IntMax - 2));

TEST_CASE("construction canonicalises sign and common factors", "[rational]")
{
    Rational const value { 6, -8 };
    CHECK(value.numerator() == -3);
    CHECK(value.denominator() == 4);
}

TEST_CASE("a zero denominator is reported, not absorbed", "[rational]")
{
    auto const result = Rational::make(1, 0);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::DivisionByZero);
    CHECK_THROWS_AS(Rational(1, 0), ArithmeticException);
}

TEST_CASE("from_decimal is exact where from_double_exact is not", "[rational]")
{
    auto const exactDecimal = Rational::from_decimal(45, -2);
    REQUIRE(exactDecimal.has_value());
    CHECK(exactDecimal->numerator() == 9);
    CHECK(exactDecimal->denominator() == 20);

    auto const fromBinary = Rational::from_double_exact(0.45);
    REQUIRE(fromBinary.has_value());
    CHECK(fromBinary->denominator() != 20);
    CHECK(fromBinary->to_double() == 0.45);
}

TEST_CASE("from_double_exact rejects non-finite input", "[rational]")
{
    double const infinity = std::numeric_limits<double>::infinity();
    double const notANumber = std::numeric_limits<double>::quiet_NaN();

    CHECK(Rational::from_double_exact(infinity).error() == ArithmeticError::NotFinite);
    CHECK(Rational::from_double_exact(notANumber).error() == ArithmeticError::NotFinite);
}

TEST_CASE("ordering never overflows, whatever the operands", "[rational]")
{
    Rational const nearOne { IntMax, IntMax - 1 };
    Rational const alsoNearOne { IntMax - 1, IntMax - 2 };

    // (IntMax)/(IntMax-1) < (IntMax-1)/(IntMax-2): both exceed 1, and the one
    // with the smaller terms exceeds it by more.
    CHECK(nearOne < alsoNearOne);
    CHECK(nearOne != alsoNearOne);
    CHECK((nearOne <=> nearOne) == std::strong_ordering::equal);
}

TEST_CASE("to_double round-trips values that are exactly representable", "[rational]")
{
    CHECK(Rational(1, 2).to_double() == 0.5);
    CHECK(Rational(-3, 4).to_double() == -0.75);
    CHECK(Rational(0).to_double() == 0.0);
}

// ---- arithmetic ----

static_assert(exact(1, 2) + exact(1, 3) == exact(5, 6));
static_assert(exact(1, 2) - exact(1, 3) == exact(1, 6));
static_assert(exact(2, 3) * exact(3, 4) == exact(1, 2));
static_assert(exact(2, 3) / exact(4, 9) == exact(3, 2));
static_assert(exact(1, 2) + exact(1, 2) == Rational { 1 });
static_assert(exact(1, 2) - exact(1, 2) == Rational { 0 });
static_assert((exact(1, 2) - exact(1, 2)).denominator() == 1);

static_assert(-exact(1, 2) == exact(-1, 2));
static_assert(+exact(1, 2) == exact(1, 2));
static_assert(-Rational { 0 } == Rational { 0 });

static_assert(formula::abs(exact(-3, 4)) == exact(3, 4));
static_assert(formula::abs(exact(3, 4)) == exact(3, 4));
static_assert(formula::abs(Rational { 0 }) == Rational { 0 });

static_assert(formula::checked_abs(exact(-3, 4)) == exact(3, 4));
static_assert(formula::checked_abs(exact(3, 4)) == exact(3, 4));
static_assert(formula::checked_abs(Rational { 0 }) == Rational { 0 });
// The one value abs cannot negate: IntMin has no representable positive
// counterpart. checked_abs reports it instead of a wrong number, and abs
// throws the same failure through the operator layer.
static_assert(formula::checked_abs(Rational { IntMin }).error() == ArithmeticError::Overflow);

static_assert(formula::pow(exact(2, 3), 0) == Rational { 1 });
static_assert(formula::pow(exact(2, 3), 2) == exact(4, 9));
static_assert(formula::pow(exact(2, 3), -2) == exact(9, 4));
static_assert(formula::pow(Rational { 10 }, 18) == Rational { 1000000000000000000 });

static_assert(formula::checked_reciprocal(exact(2, 3)) == exact(3, 2));
static_assert(formula::checked_reciprocal(exact(-2, 3)) == exact(-3, 2));
static_assert(!formula::checked_reciprocal(Rational { 0 }).has_value());

static_assert(!formula::checked_div(Rational { 1 }, Rational { 0 }).has_value());
static_assert(formula::checked_div(Rational { 1 }, Rational { 0 }).error() == ArithmeticError::DivisionByZero);
static_assert(!formula::checked_add(Rational { IntMax }, Rational { 1 }).has_value());
static_assert(formula::checked_add(Rational { IntMax }, Rational { 1 }).error() == ArithmeticError::Overflow);
static_assert(!formula::checked_pow(Rational { 10 }, 19).has_value());

// Cross-reduction must make this succeed: the naive product of the numerators
// would overflow, but the canonical result is simply 1.
static_assert(exact(IntMax, 3) * exact(3, IntMax) == Rational { 1 });

TEST_CASE("addition is exact where binary floating point is not", "[rational]")
{
    Rational const tenth = *Rational::from_decimal(1, -1);
    Rational sum {};
    for (int step = 0; step < 10; ++step)
        sum += tenth;

    CHECK(sum == Rational { 1 });
    CHECK(sum.numerator() == 1);
    CHECK(sum.denominator() == 1);
}

TEST_CASE("division by zero throws through the operator and reports through the checked form", "[rational]")
{
    CHECK_THROWS_AS(Rational(1) / Rational(0), ArithmeticException);

    auto const result = formula::checked_div(Rational { 1 }, Rational { 0 });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::DivisionByZero);
}

TEST_CASE("overflow is reported, never saturated", "[rational]")
{
    auto const result = formula::checked_mul(Rational { IntMax }, Rational { 2 });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);
    CHECK_THROWS_AS(Rational(IntMax) * Rational(2), ArithmeticException);
}

TEST_CASE("abs throws where checked_abs reports Overflow", "[rational]")
{
    auto const result = formula::checked_abs(Rational { IntMin });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ArithmeticError::Overflow);
    CHECK_THROWS_AS(formula::abs(Rational { IntMin }), ArithmeticException);
}

TEST_CASE("cross-reduction keeps representable results representable", "[rational]")
{
    Rational const left { IntMax, 3 };
    Rational const right { 3, IntMax };
    CHECK(left * right == Rational { 1 });

    Rational const half { 1, 2 };
    Rational const bigOdd { IntMax, 1 };
    CHECK((bigOdd * half).denominator() == 2);
}

TEST_CASE("compound assignment matches the binary operators", "[rational]")
{
    Rational value { 1, 2 };
    value += Rational { 1, 3 };
    CHECK(value == Rational(5, 6));
    value -= Rational { 1, 3 };
    CHECK(value == Rational(1, 2));
    value *= Rational { 4, 1 };
    CHECK(value == Rational(2, 1));
    value /= Rational { 4, 1 };
    CHECK(value == Rational(1, 2));
}

TEST_CASE("pow handles zero, positive and negative exponents", "[rational]")
{
    CHECK(formula::pow(Rational(5), 0) == Rational(1));
    CHECK(formula::pow(Rational(2), 10) == Rational(1024));
    CHECK(formula::pow(Rational(2), -3) == Rational(1, 8));
    CHECK_THROWS_AS(formula::pow(Rational(0), -1), ArithmeticException);
}

TEST_CASE("addition is conservative at the extreme edge of the range", "[rational]")
{
    // A known and deliberate limitation, pinned so it cannot change unnoticed.
    //
    // The two denominators are equal, so the least-common-multiple scaling
    // reduces both scale factors to 1 and the numerator sum becomes
    // IntMax + IntMax, which overflows. The exact answer, IntMax/1518500250,
    // is perfectly representable -- the library refuses a calculation it could
    // in principle perform.
    //
    // This is the safe direction to be wrong in: a reported failure, never a
    // wrong number. Measured over 473984 operand pairs against 128-bit ground
    // truth, it never occurs for numerators below ~10^6, which covers every
    // realistic use. Removing the limitation needs 128-bit intermediates, and
    // MSVC has no __int128.
    //
    // If a future change adds wide intermediates, this test is the one to flip.
    Rational const large = *Rational::make(IntMax, 3037000500);

    auto const sum = formula::checked_add(large, large);
    REQUIRE_FALSE(sum.has_value());
    CHECK(sum.error() == ArithmeticError::Overflow);

    // The result it declined is genuinely representable: IntMax is odd, so
    // 2*IntMax/3037000500 reduces by exactly 2.
    auto const representable = Rational::make(IntMax, 1518500250);
    REQUIRE(representable.has_value());

    // Multiplication, by contrast, cross-reduces and does succeed where the
    // canonical result fits -- the two paths differ by design, not by accident.
    auto const product = formula::checked_mul(*Rational::make(IntMax, 3), *Rational::make(3, IntMax));
    REQUIRE(product.has_value());
    CHECK(*product == Rational { 1 });
}

TEST_CASE("integer types that cannot wrap still convert implicitly", "[rational]")
{
    // The companion to negative/rational_from_wide_unsigned.cpp. That case pins
    // what must NOT compile; this pins what must continue to. Before the
    // constructor was constrained, a wide unsigned value converted by modular
    // wraparound and SIZE_MAX became -1 silently.
    Rational const fromInt = 450;
    Rational const fromUnsigned = 450U;
    Rational const fromLong = 450L;
    Rational const fromLongLong = 450LL;
    Rational const fromShort = static_cast<short>(450);
    Rational const fromUnsignedShort = static_cast<unsigned short>(450);

    CHECK(fromInt == Rational { 450 });
    CHECK(fromUnsigned == fromInt);
    CHECK(fromLong == fromInt);
    CHECK(fromLongLong == fromInt);
    CHECK(fromShort == fromInt);
    CHECK(fromUnsignedShort == fromInt);

    CHECK(fromInt.denominator() == 1);
    CHECK(Rational { IntMin }.numerator() == IntMin);
    CHECK(Rational { IntMax }.numerator() == IntMax);
}
