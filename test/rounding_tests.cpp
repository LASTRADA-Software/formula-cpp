// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/rounding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iterator> // std::size, used by the distinctness loops
#include <limits>   // std::numeric_limits, used by Task 6's cases

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::Rational;
using formula::RoundingMode;

namespace
{
consteval Rational exact(Rational::Int numerator, Rational::Int denominator)
{
    return Rational { numerator, denominator };
}

constexpr Rational::Int round_int(Rational::Int numerator, Rational::Int denominator, RoundingMode mode)
{
    return formula::round_to_int(Rational { numerator, denominator }, mode);
}
} // namespace

// ---- the seven modes, on a positive value that is not a tie: 7/4 = 1.75 ----

static_assert(round_int(7, 4, RoundingMode::HalfAwayFromZero) == 2);
static_assert(round_int(7, 4, RoundingMode::HalfTowardZero) == 2);
static_assert(round_int(7, 4, RoundingMode::HalfEven) == 2);
static_assert(round_int(7, 4, RoundingMode::Ceiling) == 2);
static_assert(round_int(7, 4, RoundingMode::Floor) == 1);
static_assert(round_int(7, 4, RoundingMode::TowardZero) == 1);
static_assert(round_int(7, 4, RoundingMode::AwayFromZero) == 2);

// ---- the same on a negative value: -7/4 = -1.75 ----

static_assert(round_int(-7, 4, RoundingMode::HalfAwayFromZero) == -2);
static_assert(round_int(-7, 4, RoundingMode::HalfTowardZero) == -2);
static_assert(round_int(-7, 4, RoundingMode::HalfEven) == -2);
static_assert(round_int(-7, 4, RoundingMode::Ceiling) == -1);
static_assert(round_int(-7, 4, RoundingMode::Floor) == -2);
static_assert(round_int(-7, 4, RoundingMode::TowardZero) == -1);
static_assert(round_int(-7, 4, RoundingMode::AwayFromZero) == -2);

// ---- exact ties, where the half-* modes finally differ: 3/2 and 5/2 ----

static_assert(round_int(3, 2, RoundingMode::HalfAwayFromZero) == 2);
static_assert(round_int(3, 2, RoundingMode::HalfTowardZero) == 1);
static_assert(round_int(3, 2, RoundingMode::HalfEven) == 2);
static_assert(round_int(5, 2, RoundingMode::HalfAwayFromZero) == 3);
static_assert(round_int(5, 2, RoundingMode::HalfTowardZero) == 2);
static_assert(round_int(5, 2, RoundingMode::HalfEven) == 2);

static_assert(round_int(-3, 2, RoundingMode::HalfAwayFromZero) == -2);
static_assert(round_int(-3, 2, RoundingMode::HalfTowardZero) == -1);
static_assert(round_int(-3, 2, RoundingMode::HalfEven) == -2);
static_assert(round_int(-5, 2, RoundingMode::HalfAwayFromZero) == -3);
static_assert(round_int(-5, 2, RoundingMode::HalfTowardZero) == -2);
static_assert(round_int(-5, 2, RoundingMode::HalfEven) == -2);

// ---- an exact integer is unchanged under every mode ----

static_assert(round_int(4, 1, RoundingMode::Floor) == 4);
static_assert(round_int(4, 1, RoundingMode::Ceiling) == 4);
static_assert(round_int(-4, 1, RoundingMode::AwayFromZero) == -4);
static_assert(round_int(0, 1, RoundingMode::HalfEven) == 0);

// ---- rounding to a multiple ----

static_assert(formula::round_to_multiple(exact(7, 1), exact(5, 1), RoundingMode::HalfAwayFromZero) == exact(5, 1));
static_assert(formula::round_to_multiple(exact(8, 1), exact(5, 1), RoundingMode::HalfAwayFromZero) == exact(10, 1));
static_assert(formula::round_to_multiple(exact(7, 2), exact(1, 4), RoundingMode::Floor) == exact(7, 2));
static_assert(formula::round_to_multiple(exact(1, 3), exact(1, 4), RoundingMode::Ceiling) == exact(1, 2));
static_assert(formula::round_to_multiple(exact(1, 3), exact(1, 4), RoundingMode::Floor) == exact(1, 4));

static_assert(formula::checked_round_to_multiple(exact(1, 3), Rational { 0 }, RoundingMode::Floor).error()
              == ArithmeticError::DomainError);
static_assert(formula::checked_round_to_multiple(exact(1, 3), exact(-1, 4), RoundingMode::Floor).error()
              == ArithmeticError::DomainError);

namespace
{
/// Returns a checked result's error, failing the test case first if it has a
/// value instead.
///
/// Calling `.error()` on a `std::expected` that holds a value is undefined
/// behaviour, and this STL's hardening turns it into a hard abort rather than a
/// test failure. So a regression that made one of these calls SUCCEED would kill
/// the process instead of turning the test red -- the assertion that exists to
/// catch the regression would be the thing that stops you seeing it. `REQUIRE`
/// stops the case before the dereference can happen.
template <typename T>
[[nodiscard]] ArithmeticError error_of(std::expected<T, ArithmeticError> const& result)
{
    REQUIRE_FALSE(result.has_value());
    return result.error();
}
} // namespace

TEST_CASE("every mode is exercised on positive, negative and tie inputs", "[rounding]")
{
    struct Case
    {
        RoundingMode mode;
        Rational::Int positive; // 1.75
        Rational::Int negative; // -1.75
        Rational::Int tieUp;    // 1.5
        Rational::Int tieDown;  // 2.5
    };

    Case const cases[] = {
        { RoundingMode::HalfAwayFromZero, 2, -2, 2, 3 },
        { RoundingMode::HalfTowardZero, 2, -2, 1, 2 },
        { RoundingMode::HalfEven, 2, -2, 2, 2 },
        { RoundingMode::Ceiling, 2, -1, 2, 3 },
        { RoundingMode::Floor, 1, -2, 1, 2 },
        { RoundingMode::TowardZero, 1, -1, 1, 2 },
        { RoundingMode::AwayFromZero, 2, -2, 2, 3 },
    };

    for (Case const& testCase: cases)
    {
        INFO("mode = " << formula::describe(testCase.mode));
        CHECK(formula::round_to_int(Rational(7, 4), testCase.mode) == testCase.positive);
        CHECK(formula::round_to_int(Rational(-7, 4), testCase.mode) == testCase.negative);
        CHECK(formula::round_to_int(Rational(3, 2), testCase.mode) == testCase.tieUp);
        CHECK(formula::round_to_int(Rational(5, 2), testCase.mode) == testCase.tieDown);
    }
}

TEST_CASE("each mode has a distinct description", "[rounding]")
{
    RoundingMode const modes[] = { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven,
                                   RoundingMode::Ceiling,          RoundingMode::Floor,          RoundingMode::TowardZero,
                                   RoundingMode::AwayFromZero };

    for (std::size_t i = 0; i < std::size(modes); ++i)
    {
        CHECK_FALSE(formula::describe(modes[i]).empty());
        for (std::size_t j = i + 1; j < std::size(modes); ++j)
            CHECK(formula::describe(modes[i]) != formula::describe(modes[j]));
    }
}

TEST_CASE("a non-positive step is a domain error, not a crash", "[rounding]")
{
    CHECK(error_of(formula::checked_round_to_multiple(Rational(1, 3), Rational(0), RoundingMode::Floor))
          == ArithmeticError::DomainError);
    CHECK_THROWS_AS(formula::round_to_multiple(Rational(1, 3), Rational(0), RoundingMode::Floor), ArithmeticException);
}

TEST_CASE("round_to_integer returns a Rational, round_to_int returns an integer", "[rounding]")
{
    Rational const rounded = formula::round_to_integer(Rational(7, 4), RoundingMode::HalfAwayFromZero);
    CHECK(rounded == Rational(2));
    CHECK(rounded.is_integer());
    CHECK(formula::round_to_int(Rational(7, 4), RoundingMode::HalfAwayFromZero) == 2);
}

// ---- decimal places ----

static_assert(formula::round(exact(1, 3), formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(33, -2));
static_assert(formula::round(exact(2, 3), formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(67, -2));
static_assert(formula::round(exact(2, 3), formula::DecimalPlaces { 2 }, RoundingMode::Floor)
              == *Rational::from_decimal(66, -2));
static_assert(formula::round(Rational { 1234 }, formula::DecimalPlaces { -2 }, RoundingMode::HalfAwayFromZero)
              == Rational { 1200 });
static_assert(formula::round(Rational { 1250 }, formula::DecimalPlaces { -2 }, RoundingMode::HalfEven) == Rational { 1200 });
static_assert(formula::round(Rational { 5 }, formula::DecimalPlaces { 0 }, RoundingMode::Floor) == Rational { 5 });

// ---- the decimal exponent, which significant digits is built on ----

static_assert(formula::checked_decimal_exponent(Rational { 1 }) == 0);
static_assert(formula::checked_decimal_exponent(Rational { 9 }) == 0);
static_assert(formula::checked_decimal_exponent(Rational { 10 }) == 1);
static_assert(formula::checked_decimal_exponent(Rational { 999 }) == 2);
static_assert(formula::checked_decimal_exponent(Rational { 1000 }) == 3);
static_assert(formula::checked_decimal_exponent(exact(1, 2)) == -1);
static_assert(formula::checked_decimal_exponent(*Rational::from_decimal(999, -3)) == -1);
static_assert(formula::checked_decimal_exponent(*Rational::from_decimal(1, -3)) == -3);
static_assert(formula::checked_decimal_exponent(Rational { -250 }) == 2);
static_assert(formula::checked_decimal_exponent(Rational { 0 }).error() == ArithmeticError::DomainError);

// ---- significant digits ----

static_assert(formula::round(Rational { 12345 }, formula::SignificantDigits { 3 }, RoundingMode::HalfAwayFromZero)
              == Rational { 12300 });
static_assert(formula::round(Rational { 12345 }, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero)
              == Rational { 12000 });
static_assert(formula::round(*Rational::from_decimal(123456, -6),
                             formula::SignificantDigits { 3 },
                             RoundingMode::HalfAwayFromZero)
              == *Rational::from_decimal(123, -3));
static_assert(formula::round(Rational { 0 }, formula::SignificantDigits { 3 }, RoundingMode::Floor) == Rational { 0 });
static_assert(formula::round(Rational { -12345 }, formula::SignificantDigits { 3 }, RoundingMode::HalfAwayFromZero)
              == Rational { -12300 });
// Rounding across a power of ten still yields the requested digit count.
static_assert(formula::round(*Rational::from_decimal(999, -2),
                             formula::SignificantDigits { 2 },
                             RoundingMode::HalfAwayFromZero)
              == Rational { 10 });

static_assert(formula::checked_round(Rational { 5 }, formula::SignificantDigits { 0 }, RoundingMode::Floor).error()
              == ArithmeticError::DomainError);
static_assert(formula::checked_round(Rational { 5 }, formula::SignificantDigits { -1 }, RoundingMode::Floor).error()
              == ArithmeticError::DomainError);
static_assert(formula::checked_round(Rational { 5 }, formula::DecimalPlaces { 19 }, RoundingMode::Floor).error()
              == ArithmeticError::Overflow);

TEST_CASE("intermediate and final rounding compose, and the order matters", "[rounding]")
{
    // A method that rounds a mean to whole units before using it produces a
    // different answer from one that rounds only at the end. Both must be
    // expressible; neither is a formatting concern.
    Rational const mean = Rational(302, 3); // 100.666...

    Rational const roundedFirst =
        formula::round(mean, formula::DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero) * Rational(2);
    Rational const roundedLast =
        formula::round(mean * Rational(2), formula::DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero);

    CHECK(roundedFirst == Rational(202));
    CHECK(roundedLast == Rational(201));
    CHECK(roundedFirst != roundedLast);
}

TEST_CASE("rounding up is available separately from rounding to nearest", "[rounding]")
{
    Rational const value = *Rational::from_decimal(1001, -3); // 1.001

    CHECK(formula::round(value, formula::DecimalPlaces { 2 }, RoundingMode::Ceiling) == *Rational::from_decimal(101, -2));
    CHECK(formula::round(value, formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero) == Rational(1));
}

TEST_CASE("rational_from_double rounds a measured double onto a decimal scale", "[rounding]")
{
    auto const rounded = formula::rational_from_double(0.45, formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero);
    REQUIRE(rounded.has_value());
    CHECK(rounded->numerator() == 9);
    CHECK(rounded->denominator() == 20);

    auto const third =
        formula::rational_from_double(1.0 / 3.0, formula::DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero);
    REQUIRE(third.has_value());
    CHECK(*third == *Rational::from_decimal(3333, -4));

    double const infinity = std::numeric_limits<double>::infinity();
    CHECK(error_of(formula::rational_from_double(infinity, formula::DecimalPlaces { 2 }, RoundingMode::Floor))
          == ArithmeticError::NotFinite);
}

TEST_CASE("significant digits and decimal places disagree, as they should", "[rounding]")
{
    Rational const value = *Rational::from_decimal(4567, -2); // 45.67

    CHECK(formula::round(value, formula::DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero)
          == *Rational::from_decimal(457, -1));
    CHECK(formula::round(value, formula::SignificantDigits { 2 }, RoundingMode::HalfAwayFromZero) == Rational(46));
}

TEST_CASE("the decimal exponent is correct on both sides of every power of ten", "[rounding]")
{
    for (int exponent = -18; exponent <= 18; ++exponent)
    {
        auto const atPower = Rational::from_decimal(1, exponent);
        if (!atPower)
            continue;

        INFO("exponent = " << exponent);
        CHECK(formula::checked_decimal_exponent(*atPower) == exponent);

        // A hair below the power of ten must land one exponent lower. The
        // epsilon is not representable at the bottom of the range, so guard it
        // rather than dereferencing an empty expected.
        if (auto const epsilon = Rational::from_decimal(1, exponent - 3); epsilon)
        {
            auto const justBelow = formula::checked_sub(*atPower, *epsilon);
            if (justBelow && !justBelow->is_zero())
                CHECK(formula::checked_decimal_exponent(*justBelow) == exponent - 1);
        }

        if (auto const negated = formula::checked_negate(*atPower); negated)
            CHECK(formula::checked_decimal_exponent(*negated) == exponent);
    }
}

TEST_CASE("rounding precision is limited by the numerator, not the magnitude", "[rounding]")
{
    // Rounding to N places scales by 10^N, cancelling factors of two against
    // the denominator first, so what must fit is
    //     |numerator| * (10^N / gcd(10^N, denominator)),
    // which for a binary denominator is |numerator| * 5^N. The NUMERATOR decides.

    // 0.45 as a double is exactly 8106479329266893 / 2^54 -- a 53-bit numerator.
    // Two places fits (8106479329266893 * 5^2), six does not (* 5^6).
    CHECK(formula::rational_from_double(0.45, formula::DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero).has_value());
    CHECK(error_of(formula::rational_from_double(0.45, formula::DecimalPlaces { 6 }, RoundingMode::HalfAwayFromZero))
          == ArithmeticError::Overflow);

    // 0.0001 fails for a DIFFERENT reason: from_double_exact refuses it before
    // any rounding happens, because its exact value needs a denominator above
    // 2^63. That limit really is magnitude-driven; the one above is not.
    CHECK(error_of(Rational::from_double_exact(0.0001)) == ArithmeticError::Overflow);
    CHECK(error_of(formula::rational_from_double(0.0001, formula::DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero))
          == ArithmeticError::Overflow);

    // Same nominal value, built exactly: numerator 1, so ten places is trivial.
    // Same value, different construction, different outcome -- the point.
    CHECK(formula::checked_round(
              *Rational::from_decimal(1, -4), formula::DecimalPlaces { 10 }, RoundingMode::HalfAwayFromZero)
              .has_value());

    // The denominator is not what limits it: numerator 1 over 2^60 rounds at
    // every supported place, while a 53-bit numerator over the SAME denominator
    // does not. This pair is what distinguishes the two explanations.
    CHECK(formula::checked_round(*Rational::make(1, 1LL << 60), formula::DecimalPlaces { 18 }, RoundingMode::Floor)
              .has_value());
    CHECK(error_of(formula::checked_round(
              *Rational::make(8106479329266893LL, 1LL << 60), formula::DecimalPlaces { 18 }, RoundingMode::Floor))
          == ArithmeticError::Overflow);

    // A small-denominator, small-numerator Rational is unaffected throughout.
    CHECK(formula::checked_round(*Rational::make(1, 3), formula::DecimalPlaces { 10 }, RoundingMode::Floor).has_value());
}
