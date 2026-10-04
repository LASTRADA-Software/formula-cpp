// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/rounding_node.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>

namespace
{
namespace unit = formula::unit;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using W4 = formula::detail::WideUnsigned<4>;
using R4 = formula::detail::WideRatio<4>;
using formula::detail::UInt128;
using formula::detail::decide_rounding;
using formula::detail::narrow_wide_ratio;
using formula::detail::round_wide_ratio;
using formula::detail::rounded_in_unit;
using formula::detail::wide_from_rational;

constexpr std::array everyMode { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven,
                                 RoundingMode::Ceiling,          RoundingMode::Floor,          RoundingMode::TowardZero,
                                 RoundingMode::AwayFromZero };
// Signs, halves, zero, and denominators with and without factors of 2 and 5,
// up to 4 * 10^18: the grid the agreement with checked_round is judged on.
constexpr std::array<std::int64_t, 17> gridNumerators { -2001, -1000, -999, -501, -500, -499, -125, -1,  0,
                                                        1,     125,   499,  500,  501,  999,  1000, 2001 };
constexpr std::array<std::int64_t, 11> gridDenominators {
    1, 2, 3, 7, 8, 40, 125, 1000, 1024, 999'999'999'989, 4'000'000'000'000'000'000
};

[[nodiscard]] constexpr R4 from(Rational exact) noexcept
{
    return wide_from_rational<4>(exact);
}

/// The bounds of `Rational::Int`.
constexpr Rational::Int IntMax = std::numeric_limits<Rational::Int>::max();
constexpr Rational::Int IntMin = std::numeric_limits<Rational::Int>::min();

/// 2^127, the magnitude of `IntMin`, and one past it.
constexpr W4 twoToOneTwentySeven = W4::from_u128(UInt128 { std::uint64_t { 1 } << 63, 0 });
constexpr W4 pastIntMin = W4::from_u128(UInt128 { std::uint64_t { 1 } << 63, 1 });
} // namespace

TEST_CASE("wide rounding: a wide fraction rounds as checked_round rounds the same Rational", "[wide-rounding]")
{
    // Wherever checked_round answers, round_wide_ratio gives the same, in
    // every mode and at every place it accepts. Counted: 48833 cases, those
    // of the grid and the four extremes, at every place from -18 to 18 and in
    // every mode, where checked_round answers.
    int checkedCases = 0;
    int agreedCases = 0;
    auto const judge = [&](Rational exact) {
        for (int places = -18; places <= 18; ++places)
            for (RoundingMode const each: everyMode)
            {
                std::expected<Rational, formula::ArithmeticError> const expected =
                    formula::checked_round(exact, DecimalPlaces { places }, each);
                if (!expected.has_value())
                    continue;
                ++checkedCases;
                std::expected<Rational, formula::ArithmeticError> const wide =
                    round_wide_ratio(from(exact), DecimalPlaces { places }, each);
                if (wide.has_value() && *wide == *expected)
                    ++agreedCases;
            }
    };
    for (std::int64_t const numeratorValue: gridNumerators)
        for (std::int64_t const denominatorValue: gridDenominators)
            judge(Rational { numeratorValue, denominatorValue });
    for (Rational const extreme:
         { Rational { IntMin }, Rational { IntMax }, Rational { IntMin, 3 }, Rational { IntMax, 2 } })
        judge(extreme);
    CHECK(checkedCases == 48833);
    CHECK(agreedCases == checkedCases);
}

TEST_CASE("wide rounding: a value checked_round refuses is answered when its rounding fits", "[wide-rounding]")
{
    // (10^21 + 1)/9 at 18 dp: checked_round forms (10^21 + 1) * 10^18, past
    // IntMax, and refuses; the wide fraction, in 256 bits, rounds to
    // 111111111111111111111222222222222222222 / 10^18, which fits.
    constexpr Rational::Int quintillion = 1'000'000'000'000'000'000;
    constexpr Rational unrounded { quintillion * 1000 + 1, 9 };
    STATIC_REQUIRE(formula::checked_round(unrounded, DecimalPlaces { 18 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    constexpr Rational::Int keptDigits = (Rational::Int { 111'111'111'111'111'111 } * 1000 + 111) * quintillion
                                         + 222'222'222'222'222'222;
    STATIC_REQUIRE(*round_wide_ratio(wide_from_rational<8>(unrounded), DecimalPlaces { 18 }, RoundingMode::HalfEven)
                   == Rational { keptDigits, quintillion });
    // 10/3 at 18 dp, which checked_round refused in 64 bits, it answers, as
    // the wide fraction does.
    STATIC_REQUIRE(formula::checked_round(Rational { 10, 3 }, DecimalPlaces { 18 }, RoundingMode::HalfEven).value()
                   == Rational { 3'333'333'333'333'333'333ULL, quintillion });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 10, 3 }), DecimalPlaces { 18 }, RoundingMode::HalfEven)
                   == Rational { 3'333'333'333'333'333'333ULL, quintillion });
}

TEST_CASE("wide rounding: ties follow the mode and the sign", "[wide-rounding]")
{
    // 19/2 = 9.5 and -9.5 at 0 dp: every mode tells a tie apart.
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfEven)
                   == Rational { 10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero)
                   == Rational { 10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfTowardZero)
                   == Rational { 9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfEven)
                   == Rational { -10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::HalfTowardZero)
                   == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::Floor)
                   == Rational { -10 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::Ceiling)
                   == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::TowardZero)
                   == Rational { -9 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { -19, 2 }), DecimalPlaces { 0 }, RoundingMode::AwayFromZero)
                   == Rational { -10 });
    // 19/28 at 4 dp, the fitted slope: 0.678571...
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 28 }), DecimalPlaces { 4 }, RoundingMode::HalfEven)
                   == Rational { 3393, 5000 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 19, 28 }), DecimalPlaces { 4 }, RoundingMode::Floor)
                   == Rational { 1357, 2000 });
    // Negative places: 125 to tens.
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 125 }), DecimalPlaces { -1 }, RoundingMode::HalfEven)
                   == Rational { 120 });
    STATIC_REQUIRE(*round_wide_ratio(from(Rational { 125 }), DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero)
                   == Rational { 130 });
}

TEST_CASE("wide rounding: a fraction wider than 64 bits rounds exactly", "[wide-rounding]")
{
    // (10^30 + 1) / 10^30 at 6 dp: 1, or one step up under Ceiling.
    constexpr W4 tenToThirty = *formula::detail::pow10<4>(30);
    constexpr R4 justAboveOne { false, *formula::detail::add_small_checked_or_none(tenToThirty, 1U), tenToThirty };
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::HalfEven) == Rational { 1 });
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::Floor) == Rational { 1 });
    STATIC_REQUIRE(*round_wide_ratio(justAboveOne, DecimalPlaces { 6 }, RoundingMode::Ceiling)
                   == Rational { 1'000'001, 1'000'000 });
    // 2^70 / 2^60, unreduced: 1024.
    constexpr R4 wideQuotient { false,
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 70),
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 60) };
    STATIC_REQUIRE(*round_wide_ratio(wideQuotient, DecimalPlaces { 0 }, RoundingMode::HalfEven) == Rational { 1024 });
    // 6/4, unreduced: 1.5 exactly at 1 dp.
    STATIC_REQUIRE(
        *round_wide_ratio(R4 { false, W4::from_u64(6), W4::from_u64(4) }, DecimalPlaces { 1 }, RoundingMode::HalfEven)
        == Rational { 3, 2 });
    // 3 * 2^100 over 2^102 is 3/4. Scaled by 10^18 unreduced it needs 162
    // bits; reduced first it fits: the case that pins the reduction.
    constexpr R4 farFromLowest { false,
                                 *formula::detail::shift_left_checked_or_none(W4::from_u64(3), 100),
                                 *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 102) };
    STATIC_REQUIRE(*round_wide_ratio(farFromLowest, DecimalPlaces { 18 }, RoundingMode::HalfEven) == Rational { 3, 4 });
    // 3 * 2^118 over 2^120, 3/4 m again: times the millimetre's 1000 unreduced
    // it needs 130 bits, so rounded_in_unit must reduce before it converts.
    constexpr R4 wideInMetres { false,
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(3), 118),
                                *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 120) };
    STATIC_REQUIRE(*rounded_in_unit(wideInMetres, unit::Millimetre, DecimalPlaces { 12 }, RoundingMode::HalfEven)
                   == Rational { 3, 4 });
}

TEST_CASE("wide rounding: the kept integer must fit Rational and the places must be in range", "[wide-rounding]")
{
    // -2^127 is IntMin; +2^127 is one past IntMax, and -2^127 - 1 one past
    // IntMin: Overflow, never IntMax with the sign lost.
    STATIC_REQUIRE(
        *round_wide_ratio(R4 { true, twoToOneTwentySeven, W4::from_u64(1) }, DecimalPlaces { 0 }, RoundingMode::HalfEven)
        == Rational { IntMin });
    STATIC_REQUIRE(
        round_wide_ratio(R4 { false, twoToOneTwentySeven, W4::from_u64(1) }, DecimalPlaces { 0 }, RoundingMode::HalfEven)
            .error()
        == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(
        round_wide_ratio(R4 { true, pastIntMin, W4::from_u64(1) }, DecimalPlaces { 0 }, RoundingMode::HalfEven).error()
        == formula::ArithmeticError::Overflow);
    // +2^63, which 64 bits refused, is kept.
    STATIC_REQUIRE(*round_wide_ratio(R4 { false, W4::from_u64(std::uint64_t { 1 } << 63), W4::from_u64(1) },
                                     DecimalPlaces { 0 },
                                     RoundingMode::HalfEven)
                   == Rational { std::uint64_t { 1 } << 63 });
    // 2^127 to the nearest 10^18 is 170141183460469231732 * 10^18, past IntMax;
    // 2^70, which 64 bits refused, is 1181 * 10^18.
    STATIC_REQUIRE(round_wide_ratio(R4 { false, twoToOneTwentySeven, W4::from_u64(1) },
                                    DecimalPlaces { -18 },
                                    RoundingMode::HalfEven)
                       .error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(
        *round_wide_ratio(R4 { false, *formula::detail::shift_left_checked_or_none(W4::from_u64(1), 70), W4::from_u64(1) },
                          DecimalPlaces { -18 },
                          RoundingMode::HalfEven)
        == Rational { Rational::Int { 1181 } * 1'000'000'000'000'000'000 });
    STATIC_REQUIRE(round_wide_ratio(from(Rational { 1, 3 }), DecimalPlaces { 19 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(round_wide_ratio(from(Rational { 1, 3 }), DecimalPlaces { -19 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(
        round_wide_ratio(R4 { false, W4::from_u64(1), W4 {} }, DecimalPlaces { 0 }, RoundingMode::HalfEven).error()
        == formula::ArithmeticError::DivisionByZero);
    // Zero rounds to zero, never to a negative zero.
    STATIC_REQUIRE(*round_wide_ratio(R4 { true, W4 {}, W4::from_u64(7) }, DecimalPlaces { 2 }, RoundingMode::Floor)
                   == Rational { 0 });
}

TEST_CASE("wide rounding: narrow_wide_ratio is the exact value or Overflow", "[wide-rounding]")
{
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(6), W4::from_u64(4) }) == Rational { 3, 2 });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { true, twoToOneTwentySeven, W4::from_u64(1) }) == Rational { IntMin });
    STATIC_REQUIRE(narrow_wide_ratio(R4 { false, twoToOneTwentySeven, W4::from_u64(1) }).error()
                   == formula::ArithmeticError::Overflow);
    // -2^127 - 1 is one past IntMin. A denominator of IntMax fits; of 2^127
    // it does not, nor of 2^128 - 1, which a cast to Rational::Int would read
    // as -1.
    STATIC_REQUIRE(narrow_wide_ratio(R4 { true, pastIntMin, W4::from_u64(1) }).error()
                   == formula::ArithmeticError::Overflow);
    constexpr W4 intMaxWide = W4::from_u128(formula::detail::wide_magnitude(IntMax));
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(1), intMaxWide }) == Rational { 1, IntMax });
    STATIC_REQUIRE(narrow_wide_ratio(R4 { false, W4::from_u64(1), twoToOneTwentySeven }).error()
                   == formula::ArithmeticError::Overflow);
    constexpr W4 allOnes = W4::from_u128(UInt128 { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } });
    STATIC_REQUIRE(narrow_wide_ratio(R4 { false, W4::from_u64(1), allOnes }).error() == formula::ArithmeticError::Overflow);
    // The 64-bit edges, which 64 bits refused, fit: 2^63, and denominators
    // of 2^63 and 2^64 - 1.
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(std::uint64_t { 1 } << 63), W4::from_u64(1) })
                   == Rational { std::uint64_t { 1 } << 63 });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(1), W4::from_u64(std::uint64_t { 1 } << 63) })
                   == Rational { 1, Rational::Int { std::uint64_t { 1 } << 63 } });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { false, W4::from_u64(1), W4::from_u64(~std::uint64_t { 0 }) })
                   == Rational { 1, Rational::Int { ~std::uint64_t { 0 } } });
    // 2^64 / (3 * 2^64): reduced first, 1/3.
    constexpr W4 twoToSixtyFour = W4::from_limbs({ 0U, 0U, 1U, 0U });
    STATIC_REQUIRE(
        *narrow_wide_ratio(R4 { false, twoToSixtyFour, *formula::detail::mul_small_checked_or_none(twoToSixtyFour, 3U) })
        == Rational { 1, 3 });
    STATIC_REQUIRE(*narrow_wide_ratio(R4 { true, W4 {}, W4::from_u64(5) }) == Rational { 0 });
}

TEST_CASE("wide rounding: rounded_in_unit rounds in its unit as a rounding node does", "[wide-rounding]")
{
    // 19/28000 m/s is 285/7 mm/min: 40.71 at 2 dp, 40.72 under Ceiling, back in m/s.
    STATIC_REQUIRE(*rounded_in_unit(
                       from(Rational { 19, 28'000 }), unit::MillimetrePerMinute, DecimalPlaces { 2 }, RoundingMode::HalfEven)
                   == Rational { 1357, 2'000'000 });
    STATIC_REQUIRE(*rounded_in_unit(
                       from(Rational { 19, 28'000 }), unit::MillimetrePerMinute, DecimalPlaces { 2 }, RoundingMode::Ceiling)
                   == Rational { 509, 750'000 });
    // An offset or a zero magnitude is refused, as checked_convert refuses a zero one.
    STATIC_REQUIRE(
        rounded_in_unit(from(Rational { 300 }), unit::Celsius, DecimalPlaces { 1 }, RoundingMode::HalfEven).error()
        == formula::ArithmeticError::DomainError);
    constexpr formula::Unit noScale { .dimension = formula::dim::Length, .magnitudeNumerator = 0 };
    STATIC_REQUIRE(rounded_in_unit(from(Rational { 3 }), noScale, DecimalPlaces { 1 }, RoundingMode::HalfEven).error()
                   == formula::ArithmeticError::DomainError);

    // Wherever RepRounding<Rational>::round_in answers, the same result, over
    // units whose factor is and is not a power of ten. Counted: 65450 cases.
    std::array<formula::Unit, 5> const units {
        unit::Gram, unit::Millimetre, unit::MillimetrePerMinute, unit::Minute, unit::Percent
    };
    int checkedCases = 0;
    int agreedCases = 0;
    for (formula::Unit const& roundedIn: units)
        for (std::int64_t const numeratorValue: gridNumerators)
            for (std::int64_t const denominatorValue: gridDenominators)
            {
                Rational const coherentValue { numeratorValue, denominatorValue };
                for (int places = -3; places <= 6; ++places)
                    for (RoundingMode const each: everyMode)
                    {
                        auto const expected = formula::RepRounding<Rational>::round_in(
                            coherentValue, roundedIn, DecimalPlaces { places }, each);
                        if (!expected.has_value())
                            continue;
                        ++checkedCases;
                        auto const wide = rounded_in_unit(from(coherentValue), roundedIn, DecimalPlaces { places }, each);
                        if (wide.has_value() && *wide == *expected)
                            ++agreedCases;
                    }
            }
    CHECK(checkedCases == 65450);
    CHECK(agreedCases == checkedCases);
}

TEST_CASE("wide rounding: decide_rounding answers only when both bounds round alike", "[wide-rounding]")
{
    struct Expected
    {
        RoundingMode mode;
        std::optional<Rational> answer; // empty: Overflow
    };
    auto const bounds = [](std::int64_t lowerNumerator, std::int64_t upperNumerator) {
        return std::array { from(Rational { lowerNumerator, 100'000'000 }), from(Rational { upperNumerator, 100'000'000 }) };
    };
    auto const judge = [](std::array<R4, 2> const& enclosure, std::array<Expected, 7> const& table) {
        for (Expected const& row: table)
        {
            auto const decided = decide_rounding(enclosure[0], enclosure[1], DecimalPlaces { 4 }, row.mode);
            INFO("mode " << static_cast<int>(row.mode));
            if (row.answer.has_value())
            {
                REQUIRE(decided.has_value());
                CHECK(*decided == *row.answer);
            }
            else
            {
                REQUIRE(!decided.has_value());
                CHECK(decided.error() == formula::ArithmeticError::Overflow);
            }
        }
    };
    Rational const low { 7071, 5000 };    // 1.4142
    Rational const high { 14143, 10000 }; // 1.4143
    Rational const minusLow { -7071, 5000 };
    Rational const minusHigh { -14143, 10000 };
    // [1.41421356, 1.41421357]: tight, every mode decides.
    judge(bounds(141421356, 141421357),
          { Expected { RoundingMode::HalfAwayFromZero, low },
            Expected { RoundingMode::HalfTowardZero, low },
            Expected { RoundingMode::HalfEven, low },
            Expected { RoundingMode::Ceiling, high },
            Expected { RoundingMode::Floor, low },
            Expected { RoundingMode::TowardZero, low },
            Expected { RoundingMode::AwayFromZero, high } });
    // [1.41424999, 1.41425001] straddles the half: the half modes cannot decide.
    judge(bounds(141424999, 141425001),
          { Expected { RoundingMode::HalfAwayFromZero, std::nullopt },
            Expected { RoundingMode::HalfTowardZero, std::nullopt },
            Expected { RoundingMode::HalfEven, std::nullopt },
            Expected { RoundingMode::Ceiling, high },
            Expected { RoundingMode::Floor, low },
            Expected { RoundingMode::TowardZero, low },
            Expected { RoundingMode::AwayFromZero, high } });
    // [1.41419999, 1.41420001] straddles a grid point: the directed modes cannot.
    judge(bounds(141419999, 141420001),
          { Expected { RoundingMode::HalfAwayFromZero, low },
            Expected { RoundingMode::HalfTowardZero, low },
            Expected { RoundingMode::HalfEven, low },
            Expected { RoundingMode::Ceiling, std::nullopt },
            Expected { RoundingMode::Floor, std::nullopt },
            Expected { RoundingMode::TowardZero, std::nullopt },
            Expected { RoundingMode::AwayFromZero, std::nullopt } });
    // The negative enclosure of the half: the sign decides the directed modes.
    judge(bounds(-141425001, -141424999),
          { Expected { RoundingMode::HalfAwayFromZero, std::nullopt },
            Expected { RoundingMode::HalfTowardZero, std::nullopt },
            Expected { RoundingMode::HalfEven, std::nullopt },
            Expected { RoundingMode::Ceiling, minusLow },
            Expected { RoundingMode::Floor, minusHigh },
            Expected { RoundingMode::TowardZero, minusLow },
            Expected { RoundingMode::AwayFromZero, minusHigh } });
}

TEST_CASE("wide rounding: a Rational scaled to a common denominator keeps its sign", "[wide-rounding]")
{
    constexpr W4 twelve = W4::from_u64(12);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { -3, 4 }, twelve)->negative);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { -3, 4 }, twelve)->magnitude == W4::from_u64(9));
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { 5, 6 }, twelve)->magnitude == W4::from_u64(10));
    STATIC_REQUIRE(!formula::detail::scaled_to_denominator(Rational { 0 }, twelve)->negative);
    STATIC_REQUIRE(formula::detail::scaled_to_denominator(Rational { 0 }, twelve)->magnitude.is_zero());
}

TEST_CASE("a wide integer holds 128 bits exactly, and says when it holds more", "[wide-int]")
{
    using formula::detail::UInt128;
    using formula::detail::WideUnsigned;
    constexpr UInt128 widest { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } };
    STATIC_REQUIRE(WideUnsigned<4>::from_u128(widest).to_u128() == std::optional<UInt128> { widest });
    STATIC_REQUIRE(WideUnsigned<8>::from_u128(UInt128 { 0x0123456789abcdef, 0xfedcba9876543210 }).to_u128()
                   == std::optional<UInt128> { UInt128 { 0x0123456789abcdef, 0xfedcba9876543210 } });
    constexpr auto twoTo128 = formula::detail::shift_left_checked_or_none(WideUnsigned<8>::from_u64(1), std::size_t { 128 });
    STATIC_REQUIRE(twoTo128.has_value());
    STATIC_REQUIRE(twoTo128->to_u128() == std::nullopt);
}
