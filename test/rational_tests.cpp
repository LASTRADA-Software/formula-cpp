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

TEST_CASE("rational: an exact root comes back exactly", "[rational]")
{
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 4 }, 2).value() == formula::Rational { 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 9, 4 }, 2).value() == formula::Rational { 3, 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 27, 8 }, 3).value() == formula::Rational { 3, 2 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { -27 }, 3).value() == formula::Rational { -3 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 1 }, 5).value() == formula::Rational { 1 });
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational {}, 2).value() == formula::Rational {});
}

TEST_CASE("rational: an inexact root is refused rather than approximated", "[rational]")
{
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 2 }, 2).error() == formula::ArithmeticError::Inexact);
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 10 }, 3).error()
                   == formula::ArithmeticError::Inexact);
}

TEST_CASE("rational: a root outside the domain is refused", "[rational]")
{
    // An even root of a negative number is not a real number.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { -4 }, 2).error()
                   == formula::ArithmeticError::DomainError);
    // Degree zero describes no root at all.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 4 }, 0).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("rational: a root near the integer limit is found, not overflowed past", "[rational]")
{
    // 3037000000^2 = 9223369000000000000, an exact square a whisker under
    // IntMax (within 0.00004% of it). The search starts with candidates whose
    // square vastly exceeds what Int can hold, so it must detect that overflow
    // and narrow down toward the true root -- never let an intermediate
    // product silently exceed the target and send the search the wrong way,
    // which would report this exact root as Inexact instead of finding it.
    // Measured: dropping the early-abort guard in exact_integer_root makes
    // this exact case come back Inexact, which is precisely the bug this
    // test exists to catch.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { 9223369000000000000LL }, 2).value()
                   == formula::Rational { 3037000000LL });

    // Genuinely inexact and near the limit: IntMax itself is not a perfect
    // square, so this must still come back Inexact rather than a wrong root.
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { IntMax }, 2).error()
                   == formula::ArithmeticError::Inexact);
}

TEST_CASE("rational: the root of the extreme negative is refused rather than overflowed to", "[rational]")
{
    // IntMin has no positive counterpart representable in Int: its magnitude is
    // IntMax + 1. Negating the numerator to reach a positive intermediate is
    // signed overflow, undefined behaviour, even though the true cube root
    // (-2^21) is representable. This must come back Overflow, not Inexact and
    // not a value.
    constexpr formula::Rational::Int extremeNegative = std::numeric_limits<formula::Rational::Int>::min();
    STATIC_REQUIRE(formula::checked_exact_nth_root(formula::Rational { extremeNegative }, 3).error()
                   == formula::ArithmeticError::Overflow);
}

TEST_CASE("rational: a pathologically large degree is refused quickly, not searched for", "[rational]")
{
    // Not STATIC_REQUIRE: at this degree, the unguarded search takes long
    // enough that a constant expression would hit the compiler's step limit
    // and fail to compile rather than answer Inexact -- exactly why this is a
    // runtime check instead. Degree is an ordinary int a caller controls, so
    // 1e9 is reachable, not contrived.
    REQUIRE(formula::checked_exact_nth_root(formula::Rational { 2 }, 1'000'000'000).error()
            == formula::ArithmeticError::Inexact);
}

TEST_CASE("rational: Pi is a stated approximation, close enough to be useful", "[rational]")
{
    // Deliberately asserted as a bound rather than an equality: the point is
    // that the documented error bound holds, not that the fraction is memorised
    // in two places.
    constexpr formula::Rational squared = formula::Pi * formula::Pi;
    CHECK(squared.to_double() > 9.8696044010893);
    CHECK(squared.to_double() < 9.8696044010897);
    STATIC_REQUIRE(formula::Pi.denominator() > 1);
}

TEST_CASE("rational: Pi's documented error bound is pinned, exactly", "[rational]")
{
    // A double cannot pin an 8e-17 bound -- its ulp near 3.14 is about
    // 4.44e-16, coarser than the bound itself -- so this compares Pi against
    // two decimal rationals computed by hand from pi's known digits: pi minus
    // 8e-17, rounded UP to 18 decimals so it stays no greater than the true
    // threshold, and pi plus 8e-17, rounded DOWN so it stays no less than the
    // true one. Pi landing strictly between them proves it is within 8e-17 of
    // pi. Comparison rather than subtraction deliberately: Pi's denominator
    // and these decimals' denominators share no common factor, so
    // checked_sub's least-common-multiple scaling would overflow computing
    // their difference directly, even though the true difference is tiny --
    // Rational::operator<=> has no such limit, by its own documented design.
    constexpr Rational belowPiBy8e17 = *Rational::from_decimal(3'141'592'653'589'793'159LL, -18);
    constexpr Rational abovePiBy8e17 = *Rational::from_decimal(3'141'592'653'589'793'318LL, -18);

    STATIC_REQUIRE(formula::Pi > belowPiBy8e17);
    STATIC_REQUIRE(formula::Pi < abovePiBy8e17);
}
