// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/quantity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using formula::ArithmeticError;
using formula::ArithmeticException;
using formula::DecimalPadding;
using formula::DecimalPlaces;
using formula::Measured;
using formula::NumberNotation;
using formula::NumberStyle;
using formula::NumberText;
using formula::Rational;
using formula::RoundingMode;

namespace dim = formula::dim;
namespace unit = formula::unit;

namespace
{
// The bounds of `Rational::Int`, 2^127 - 1 and -2^127.
inline constexpr Rational::Int IntMax = std::numeric_limits<Rational::Int>::max();
inline constexpr Rational::Int IntMin = std::numeric_limits<Rational::Int>::min();
// The largest 64-bit integer, which a `Rational` once stopped at.
inline constexpr std::int64_t Int64Max = std::numeric_limits<std::int64_t>::max();

/// A quantity in kilojoules: a unit with a symbol, `kJ`, and one declared
/// decimal, which the padded cases below read.
struct ImpactWork: formula::Quantity<ImpactWork, "W_i", "impact work", unit::Kilojoule>
{
};

/// A quantity in `unit::One`, which has no symbol.
struct Share: formula::Quantity<Share, "s_h", "a share", unit::One>
{
};

/// 18 declared decimals and a symbol of 32 bytes with no terminator -- as
/// many as a `Symbol` holds and `view()` reads -- so that a value in it is
/// the longest text this library spells.
inline constexpr formula::Unit Widest {
    .dimension = dim::Scalar,
    .symbolText = formula::Symbol { { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                                      'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5' } },
    .decimals = 18
};
struct WidestReading: formula::Quantity<WidestReading, "r_w", "a reading in the widest unit", Widest>
{
};

/// Declared decimals of -1: whole tens.
inline constexpr formula::Unit Tens { .dimension = dim::Scalar, .symbolText = formula::symbol("t10"), .decimals = -1 };
/// Declared decimals of 19 and of -19, one past what `DecimalPlaces` spans on either side.
inline constexpr formula::Unit TooFine { .dimension = dim::Scalar, .symbolText = formula::symbol("tf"), .decimals = 19 };
inline constexpr formula::Unit TooCoarse { .dimension = dim::Scalar, .symbolText = formula::symbol("tc"), .decimals = -19 };

/// A mass unit of a thousandth of a kilogram with no symbol: the gram's size
/// under no name.
inline constexpr formula::Unit UnnamedGram { .dimension = dim::Mass, .magnitudeDenominator = 1000 };
struct UnnamedMass: formula::Quantity<UnnamedMass, "m_u", "a mass in an unnamed unit of a gram", UnnamedGram>
{
};

/// A count per unnamed gram, a unit of 1000 kg^-1 with no symbol.
inline constexpr formula::Unit UnnamedPerGram { .dimension = dim::Scalar / dim::Mass, .magnitudeNumerator = 1000 };
struct UnnamedLoading: formula::Quantity<UnnamedLoading, "q_u", "a count per unnamed gram", UnnamedPerGram>
{
};

/// The Celsius scale with no symbol: the kelvin's size, its zero at 273.15 K.
inline constexpr formula::Unit UnnamedCelsius { .dimension = dim::Temperature,
                                                .offsetNumerator = 27315,
                                                .offsetDenominator = 100 };
struct UnnamedReading: formula::Quantity<UnnamedReading, "T_u", "a reading on an unnamed scale", UnnamedCelsius>
{
};

/// A mass in grams, with the gram's symbol.
struct NamedMass: formula::Quantity<NamedMass, "m_g", "a mass in grams", unit::Gram>
{
};

/// Four named bases with 15-byte names and every SI base, each to the power
/// 11/13: a coherent unit whose spelling alone is longer than a `NumberText`
/// holds.
inline constexpr formula::Dimension Sprawling = formula::nth_root(
    formula::power(formula::base_dimension("Aaaaaaaaaaaaaaa") * formula::base_dimension("Bbbbbbbbbbbbbbb")
                       * formula::base_dimension("Ccccccccccccccc") * formula::base_dimension("Ddddddddddddddd")
                       * dim::Length * dim::Mass * dim::Time * dim::Current * dim::Temperature * dim::Amount
                       * dim::Luminosity,
                   11),
    13);
inline constexpr formula::Unit UnnamedSprawl { .dimension = Sprawling, .magnitudeDenominator = 1000 };
struct SprawlingReading: formula::Quantity<SprawlingReading, "s_u", "a reading in a sprawling unit", UnnamedSprawl>
{
};

/// Whether `view()` can be called on a @p T.
template <typename T>
concept Viewable = requires(T&& spelled) { std::forward<T>(spelled).view(); };

/// @p exactText with zeros appended up to @p places decimal places.
[[nodiscard]] std::string padded_to(std::string exactText, int places)
{
    if (places <= 0)
        return exactText;
    std::size_t const point = exactText.find('.');
    std::size_t shownPlaces = point == std::string::npos ? 0 : exactText.size() - point - 1;
    if (point == std::string::npos)
        exactText += '.';
    for (; shownPlaces < static_cast<std::size_t>(places); ++shownPlaces)
        exactText += '0';
    return exactText;
}
} // namespace

// ---- exact decimals and fractions ----

TEST_CASE("an exact decimal is shown only when it is the exact value", "[number_text]")
{
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { 3, 5 }) == "0.6");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { 94411, 5000 }) == "18.8822");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { -7, 4 }) == "-1.75");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { 1, 262144 }) == "0.000003814697265625"); // 2^18: 18 places
    STATIC_REQUIRE(!formula::exact_decimal_text(Rational { 1, 524288 }).has_value());             // 2^19: 19 places
    STATIC_REQUIRE(!formula::exact_decimal_text(Rational { 1, 3 }).has_value());
    STATIC_REQUIRE(formula::fraction_text(Rational { -1, 3 }) == "-1/3");

    // Whole numbers, zero, and 10^18 itself as a denominator.
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { 4 }) == "4");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational {}) == "0");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { 1, 1'000'000'000'000'000'000 }) == "0.000000000000000001");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { -1, 20 }) == "-0.05");
    STATIC_REQUIRE(formula::fraction_text(Rational { 3, 5 }) == "3/5");
    STATIC_REQUIRE(formula::fraction_text(Rational { 4 }) == "4");
    STATIC_REQUIRE(formula::fraction_text(Rational {}) == "0");

    // has_exact_decimal draws the same line exact_decimal_text does.
    STATIC_REQUIRE(formula::has_exact_decimal(Rational { 1, 262144 }));
    STATIC_REQUIRE(!formula::has_exact_decimal(Rational { 1, 524288 }));
    STATIC_REQUIRE(!formula::has_exact_decimal(Rational { 1, 3 }));
    STATIC_REQUIRE(!formula::has_exact_decimal(Rational { 7, IntMax }));

    // Both are exact, whatever they show.
    STATIC_REQUIRE(formula::exact_decimal_text(Rational { 3, 5 })->is_exact());
    STATIC_REQUIRE(formula::fraction_text(Rational { 1, 3 }).is_exact());
}

TEST_CASE("the extremes of Rational are spelled in full", "[number_text]")
{
    // The magnitude of IntMin is 2^127, which Int cannot hold; the text has it.
    STATIC_REQUIRE(formula::fraction_text(Rational { IntMin }) == "-170141183460469231731687303715884105728");
    STATIC_REQUIRE(formula::fraction_text(Rational { IntMin, IntMax })
                   == "-170141183460469231731687303715884105728/170141183460469231731687303715884105727");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { IntMin }) == "-170141183460469231731687303715884105728");
    STATIC_REQUIRE(*formula::exact_decimal_text(Rational { IntMax }) == "170141183460469231731687303715884105727");
    STATIC_REQUIRE(
        formula::decimal_text(Rational { IntMin }, DecimalPlaces { 18 }, RoundingMode::HalfEven, DecimalPadding::Padded)
        == "-170141183460469231731687303715884105728.000000000000000000");
}

// ---- rounded decimals ----

TEST_CASE("a rounded decimal is what checked_round rounds to", "[number_text]")
{
    STATIC_REQUIRE(formula::decimal_text(
                       Rational { 23653, 200 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded)
                   == "118.26");
    STATIC_REQUIRE(formula::decimal_text(
                       Rational { 23653, 200 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded)
                   == "118.27");
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 4 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded)
        == "4.00");
    STATIC_REQUIRE(
        formula::decimal_text(Rational { -1, 1000 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded)
        == "0.00"); // no "-0.00"
    STATIC_REQUIRE(
        formula::decimal_text(Rational { -1, 1000 }, DecimalPlaces { 2 }, RoundingMode::Floor, DecimalPadding::Padded)
        == "-0.01");
    STATIC_REQUIRE(formula::decimal_text(
                       Rational { 9999, 1000 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded)
                   == "10.00");
    STATIC_REQUIRE(
        formula::number_text(Rational { 1, 3 }, NumberStyle::approximate_decimal(RoundingMode::HalfEven), unit::One)
        == "\xe2\x89\x88" "0.333");

    // With no places, HalfEven reads the parity of the whole number: 2.5 and
    // 3.5 go to 2 and 4, where rounding every tie up gives 3 and 4.
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 5, 2 }, DecimalPlaces { 0 }, RoundingMode::HalfEven, DecimalPadding::Padded)
        == "2");
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 7, 2 }, DecimalPlaces { 0 }, RoundingMode::HalfEven, DecimalPadding::Padded)
        == "4");
    // Ties on a negative value: toward zero and away from it differ.
    STATIC_REQUIRE(
        formula::decimal_text(Rational { -5, 2 }, DecimalPlaces { 0 }, RoundingMode::HalfTowardZero, DecimalPadding::Padded)
        == "-2");
    STATIC_REQUIRE(formula::decimal_text(
                       Rational { -5, 2 }, DecimalPlaces { 0 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded)
                   == "-3");
    // Trimmed drops the trailing zeros, and the point with them.
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 1, 10 }, DecimalPlaces { 3 }, RoundingMode::HalfEven, DecimalPadding::Trimmed)
        == "0.1");
    STATIC_REQUIRE(formula::decimal_text(
                       Rational { 9999, 1000 }, DecimalPlaces { 2 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Trimmed)
                   == "10");

    // is_exact says whether rounding changed the value, not whether zeros were added.
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 4 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded)
            .is_exact());
    STATIC_REQUIRE(
        !formula::decimal_text(Rational { 23653, 200 }, DecimalPlaces { 2 }, RoundingMode::HalfEven, DecimalPadding::Padded)
             .is_exact());
    STATIC_REQUIRE(
        formula::decimal_text(Rational { 23653, 200 }, DecimalPlaces { 3 }, RoundingMode::HalfEven, DecimalPadding::Padded)
            .is_exact());
}

TEST_CASE("a rounded decimal exists where checked_round overflows", "[number_text]")
{
    NumberText const third =
        formula::decimal_text(Rational { IntMax, 3 }, DecimalPlaces { 18 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    CHECK(third.view() == "56713727820156410577229101238628035242.333333333333333333");
    CHECK_FALSE(third.is_exact());

    auto const rounded = formula::checked_round(Rational { IntMax, 3 }, DecimalPlaces { 18 }, RoundingMode::HalfEven);
    REQUIRE_FALSE(rounded.has_value());
    CHECK(rounded.error() == ArithmeticError::Overflow);

    // The largest 64-bit integer over 3, which overflowed 64 bits, rounds:
    // to the value its text shows.
    auto const roundedThird64 =
        formula::checked_round(Rational { Int64Max, 3 }, DecimalPlaces { 18 }, RoundingMode::HalfEven);
    REQUIRE(roundedThird64.has_value());
    CHECK(*roundedThird64
          == Rational { Rational::Int { 3074457345618258602 } * 1000000000000000000 + 333333333333333333,
                        1000000000000000000 });
    NumberText const third64 = formula::decimal_text(
        Rational { Int64Max, 3 }, DecimalPlaces { 18 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    CHECK(third64.view() == "3074457345618258602.333333333333333333");
}

TEST_CASE("the long division never forms ten times a remainder near IntMax", "[number_text]")
{
    // A denominator of IntMax leaves a remainder of up to IntMax - 1, and ten
    // times that is past 2^128: formed directly, it wraps, and
    // (IntMax - 1)/IntMax -- 0.999..., 38 nines and more -- would read 0.2 to
    // one place. All four values round, so none of these texts is exact;
    // Padded keeps every place, the trailing zeros included.
    Rational const nearOne { IntMax - 1, IntMax };
    Rational const nearMinusOne { -(IntMax - 1), IntMax };
    Rational const nearZero { 1, IntMax };
    constexpr auto padded = DecimalPadding::Padded;

    NumberText const oneEven = formula::decimal_text(nearOne, DecimalPlaces { 1 }, RoundingMode::HalfEven, padded);
    NumberText const oneFloor = formula::decimal_text(nearOne, DecimalPlaces { 1 }, RoundingMode::Floor, padded);
    NumberText const manyEven = formula::decimal_text(nearOne, DecimalPlaces { 18 }, RoundingMode::HalfEven, padded);
    NumberText const manyTowardZero =
        formula::decimal_text(nearOne, DecimalPlaces { 18 }, RoundingMode::TowardZero, padded);
    NumberText const negativeFloor = formula::decimal_text(nearMinusOne, DecimalPlaces { 18 }, RoundingMode::Floor, padded);
    NumberText const negativeCeiling =
        formula::decimal_text(nearMinusOne, DecimalPlaces { 18 }, RoundingMode::Ceiling, padded);
    NumberText const tinyCeiling = formula::decimal_text(nearZero, DecimalPlaces { 18 }, RoundingMode::Ceiling, padded);
    NumberText const tinyEven = formula::decimal_text(nearZero, DecimalPlaces { 18 }, RoundingMode::HalfEven, padded);

    CHECK(oneEven.view() == "1.0");
    CHECK(oneFloor.view() == "0.9");
    CHECK(manyEven.view() == "1.000000000000000000");
    CHECK(manyTowardZero.view() == "0.999999999999999999");
    CHECK(negativeFloor.view() == "-1.000000000000000000");
    CHECK(negativeCeiling.view() == "-0.999999999999999999");
    CHECK(tinyCeiling.view() == "0.000000000000000001");
    CHECK(tinyEven.view() == "0.000000000000000000");
    for (NumberText const* const spelled:
         { &oneEven, &oneFloor, &manyEven, &manyTowardZero, &negativeFloor, &negativeCeiling, &tinyCeiling, &tinyEven })
        CHECK_FALSE(spelled->is_exact());
}

TEST_CASE("more places than text is written at, 18 either way, is refused", "[number_text]")
{
    // Text's own limit, not checked_round's, which rounds 1/3 at 19 places and at -19.
    REQUIRE(formula::checked_round(Rational { 1, 3 }, DecimalPlaces { 19 }, RoundingMode::HalfEven).has_value());
    REQUIRE(formula::checked_round(Rational { 1, 3 }, DecimalPlaces { -19 }, RoundingMode::HalfEven).has_value());

    auto const tooFine = formula::checked_decimal_text(
        Rational { 1, 3 }, DecimalPlaces { 19 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    REQUIRE_FALSE(tooFine.has_value());
    CHECK(tooFine.error() == ArithmeticError::Overflow);

    auto const tooCoarse = formula::checked_decimal_text(
        Rational { 1, 3 }, DecimalPlaces { -19 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    REQUIRE_FALSE(tooCoarse.has_value());
    CHECK(tooCoarse.error() == ArithmeticError::Overflow);

    CHECK_THROWS_AS(
        formula::decimal_text(Rational { 1, 3 }, DecimalPlaces { 19 }, RoundingMode::HalfEven, DecimalPadding::Padded),
        ArithmeticException);
}

TEST_CASE("negative places round to whole tens through checked_round", "[number_text]")
{
    NumberText const evenTens =
        formula::decimal_text(Rational { 125 }, DecimalPlaces { -1 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    NumberText const awayTens = formula::decimal_text(
        Rational { 125 }, DecimalPlaces { -1 }, RoundingMode::HalfAwayFromZero, DecimalPadding::Padded);
    CHECK(evenTens.view() == "120");
    CHECK(awayTens.view() == "130");
    CHECK_FALSE(evenTens.is_exact());
    CHECK_FALSE(awayTens.is_exact());

    NumberText const negativeTens =
        formula::decimal_text(Rational { -125 }, DecimalPlaces { -1 }, RoundingMode::Floor, DecimalPadding::Trimmed);
    CHECK(negativeTens.view() == "-130");

    NumberText const alreadyTens =
        formula::decimal_text(Rational { 120 }, DecimalPlaces { -1 }, RoundingMode::HalfEven, DecimalPadding::Padded);
    CHECK(alreadyTens.view() == "120");
    CHECK(alreadyTens.is_exact());
}

TEST_CASE("a rounding mode that is none of the seven is refused on a tie as checked_round refuses it", "[number_text]")
{
    auto const unknownMode = static_cast<RoundingMode>(42);

    auto const tie =
        formula::checked_decimal_text(Rational { 5, 2 }, DecimalPlaces { 0 }, unknownMode, DecimalPadding::Padded);
    REQUIRE_FALSE(tie.has_value());
    CHECK(tie.error() == ArithmeticError::DomainError);
    auto const roundedTie = formula::checked_round(Rational { 5, 2 }, DecimalPlaces { 0 }, unknownMode);
    REQUIRE_FALSE(roundedTie.has_value());
    CHECK(roundedTie.error() == ArithmeticError::DomainError);

    // Off a tie it is nearest, as there.
    auto const nearTie =
        formula::checked_decimal_text(Rational { 7, 3 }, DecimalPlaces { 0 }, unknownMode, DecimalPadding::Padded);
    REQUIRE(nearTie.has_value());
    CHECK(nearTie->view() == "2");
    CHECK(formula::checked_round(Rational { 7, 3 }, DecimalPlaces { 0 }, unknownMode) == Rational { 2 });
}

TEST_CASE("decimal_text agrees with checked_round wherever checked_round answers", "[number_text]")
{
    std::array<std::int64_t, 9> const divisors { 1, 2, 3, 7, 8, 40, 125, 1000, 1024 };
    std::array<RoundingMode, 7> const roundingModes { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero,
                                                      RoundingMode::HalfEven,         RoundingMode::Ceiling,
                                                      RoundingMode::Floor,            RoundingMode::TowardZero,
                                                      RoundingMode::AwayFromZero };
    constexpr std::int32_t MostPlaces = 6;

    std::size_t compared = 0;
    std::size_t disagreements = 0;
    std::string firstDisagreement;
    for (std::int64_t dividend = -2000; dividend <= 2000; ++dividend)
    {
        for (std::int64_t const divisor: divisors)
        {
            Rational const unrounded { dividend, divisor };
            for (std::int32_t places = 0; places <= MostPlaces; ++places)
            {
                for (RoundingMode const roundingMode: roundingModes)
                {
                    auto const rounded = formula::checked_round(unrounded, DecimalPlaces { places }, roundingMode);
                    if (!rounded)
                        continue;
                    ++compared;

                    NumberText const padded =
                        formula::decimal_text(unrounded, DecimalPlaces { places }, roundingMode, DecimalPadding::Padded);
                    NumberText const trimmed =
                        formula::decimal_text(unrounded, DecimalPlaces { places }, roundingMode, DecimalPadding::Trimmed);
                    std::string const exactRounded { formula::exact_decimal_text(*rounded)->view() };
                    bool const changed = !(*rounded == unrounded);

                    if (padded.view() != padded_to(exactRounded, places) || trimmed.view() != exactRounded
                        || padded.is_exact() == changed || trimmed.is_exact() == changed)
                    {
                        if (disagreements == 0)
                            firstDisagreement = std::to_string(dividend) + "/" + std::to_string(divisor) + " to "
                                                + std::to_string(places) + " places in mode "
                                                + std::to_string(static_cast<int>(roundingMode)) + ": "
                                                + std::string { padded.view() } + " and "
                                                + std::string { trimmed.view() } + ", checked_round gives "
                                                + exactRounded;
                        ++disagreements;
                    }
                }
            }
        }
    }

    INFO(firstDisagreement);
    CHECK(disagreements == 0);
    // Without this, a `continue` above that fired every time would leave the
    // case green with nothing compared. checked_round answers every one of
    // these -- numerators to 2000, places to 6 -- so the count is exact.
    CHECK(compared == std::size_t { 4001 } * divisors.size() * std::size_t { MostPlaces + 1 } * roundingModes.size());
}

// ---- styles ----

TEST_CASE("a style is a fraction unless it asks for more", "[number_text]")
{
    STATIC_REQUIRE(NumberStyle {} == NumberStyle::fraction());
    STATIC_REQUIRE(NumberStyle {}.notation() == NumberNotation::Fraction);
    STATIC_REQUIRE(NumberStyle::exact_decimal().notation() == NumberNotation::ExactDecimal);
    STATIC_REQUIRE(NumberStyle::exact_decimal().padding() == DecimalPadding::Trimmed);
    STATIC_REQUIRE(NumberStyle::exact_decimal(DecimalPadding::Padded).padding() == DecimalPadding::Padded);

    constexpr NumberStyle approximating = NumberStyle::approximate_decimal(RoundingMode::Floor, DecimalPadding::Padded);
    STATIC_REQUIRE(approximating.notation() == NumberNotation::ApproximateDecimal);
    STATIC_REQUIRE(approximating.approximation() == RoundingMode::Floor);
    STATIC_REQUIRE(approximating.padding() == DecimalPadding::Padded);
    STATIC_REQUIRE(approximating != NumberStyle::approximate_decimal(RoundingMode::Ceiling, DecimalPadding::Padded));

    // exact_only drops the approximation and its mode, and keeps the padding.
    STATIC_REQUIRE(approximating.exact_only() == NumberStyle::exact_decimal(DecimalPadding::Padded));
    STATIC_REQUIRE(NumberStyle::approximate_decimal(RoundingMode::Ceiling).exact_only() == NumberStyle::exact_decimal());
    STATIC_REQUIRE(NumberStyle::fraction().exact_only() == NumberStyle::fraction());
    STATIC_REQUIRE(NumberStyle::exact_decimal().exact_only() == NumberStyle::exact_decimal());
}

TEST_CASE("only this library makes a NumberText and a temporary one cannot be viewed", "[number_text]")
{
    STATIC_REQUIRE(!std::is_default_constructible_v<NumberText>);
    STATIC_REQUIRE(Viewable<NumberText const&>);
    STATIC_REQUIRE(!Viewable<NumberText>);
}

TEST_CASE("each notation writes the number its own way", "[number_text]")
{
    constexpr NumberStyle paddedExact = NumberStyle::exact_decimal(DecimalPadding::Padded);
    constexpr NumberStyle evenApproximation = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
    constexpr NumberStyle paddedApproximation =
        NumberStyle::approximate_decimal(RoundingMode::HalfEven, DecimalPadding::Padded);
    constexpr NumberStyle floorApproximation = NumberStyle::approximate_decimal(RoundingMode::Floor);
    constexpr NumberStyle ceilingApproximation = NumberStyle::approximate_decimal(RoundingMode::Ceiling);

    // Fraction: always the fraction.
    STATIC_REQUIRE(formula::number_text(Rational { 3, 5 }, NumberStyle::fraction(), unit::One) == "3/5");

    // ExactDecimal: the decimal when it is the value, else the fraction.
    STATIC_REQUIRE(formula::number_text(Rational { 3, 5 }, NumberStyle::exact_decimal(), unit::One) == "0.6");
    STATIC_REQUIRE(formula::number_text(Rational { 1, 3 }, NumberStyle::exact_decimal(), unit::One) == "1/3");
    STATIC_REQUIRE(formula::number_text(Rational { 1, 3 }, NumberStyle::exact_decimal(), unit::One).is_exact());

    // Padded pads to the unit's decimals, 3 for unit::One, and never cuts short.
    STATIC_REQUIRE(unit::One.decimals == 3);
    STATIC_REQUIRE(formula::number_text(Rational { 3, 5 }, paddedExact, unit::One) == "0.600");
    STATIC_REQUIRE(formula::number_text(Rational { 4 }, paddedExact, unit::One) == "4.000");
    STATIC_REQUIRE(formula::number_text(Rational { 1929, 15625 }, paddedExact, unit::One) == "0.123456");
    STATIC_REQUIRE(formula::number_text(Rational { 1, 3 }, paddedExact, unit::One) == "1/3");

    // ApproximateDecimal: the exact decimal unmarked when there is one...
    STATIC_REQUIRE(formula::number_text(Rational { 3, 5 }, evenApproximation, unit::One) == "0.6");
    STATIC_REQUIRE(formula::number_text(Rational { 3, 5 }, evenApproximation, unit::One).is_exact());
    STATIC_REQUIRE(formula::number_text(Rational { 1929, 15625 }, evenApproximation, unit::One) == "0.123456");
    // ... and otherwise a marked decimal, rounded in the mode it names.
    STATIC_REQUIRE(!formula::number_text(Rational { 1, 3 }, evenApproximation, unit::One).is_exact());
    STATIC_REQUIRE(formula::number_text(Rational { 2, 3 }, evenApproximation, unit::One) == "\xe2\x89\x88" "0.667");
    STATIC_REQUIRE(formula::number_text(Rational { 2, 3 }, floorApproximation, unit::One) == "\xe2\x89\x88" "0.666");
    STATIC_REQUIRE(formula::number_text(Rational { -2, 3 }, floorApproximation, unit::One) == "\xe2\x89\x88" "-0.667");
    STATIC_REQUIRE(formula::number_text(Rational { -2, 3 }, ceilingApproximation, unit::One) == "\xe2\x89\x88" "-0.666");
    // 301/3000 is 0.100333...: to 3 places 0.100, which Trimmed shortens.
    STATIC_REQUIRE(formula::number_text(Rational { 301, 3000 }, evenApproximation, unit::One) == "\xe2\x89\x88" "0.1");
    STATIC_REQUIRE(formula::number_text(Rational { 301, 3000 }, paddedApproximation, unit::One) == "\xe2\x89\x88" "0.100");
    // A negative value that rounds to zero keeps its marker and loses its sign.
    STATIC_REQUIRE(formula::number_text(Rational { -1, 3000 }, paddedApproximation, unit::One) == "\xe2\x89\x88" "0.000");

    // A unit of whole tens: an approximation rounds to tens, an exact
    // decimal is shown in full.
    STATIC_REQUIRE(formula::number_text(Rational { 1253, 3 }, evenApproximation, Tens) == "\xe2\x89\x88" "420");
    STATIC_REQUIRE(formula::number_text(Rational { 5, 2 }, paddedExact, Tens) == "2.5");
}

TEST_CASE("a style that reads the unit's decimals refuses decimals outside what DecimalPlaces spans", "[number_text]")
{
    constexpr NumberStyle paddedExact = NumberStyle::exact_decimal(DecimalPadding::Padded);
    constexpr NumberStyle evenApproximation = NumberStyle::approximate_decimal(RoundingMode::HalfEven);

    // A style that never reads them is unaffected.
    CHECK(formula::number_text(Rational { 1, 2 }, NumberStyle::fraction(), TooFine) == "1/2");
    CHECK(formula::number_text(Rational { 1, 2 }, NumberStyle::exact_decimal(), TooFine) == "0.5");

    // One that pads or approximates is refused, whatever the value.
    auto const padded = formula::checked_number_text(Rational { 1, 2 }, paddedExact, TooFine);
    auto const paddedThird = formula::checked_number_text(Rational { 1, 3 }, paddedExact, TooFine);
    auto const approximated = formula::checked_number_text(Rational { 1, 2 }, evenApproximation, TooFine);
    auto const approximatedCoarse = formula::checked_number_text(Rational { 1, 3 }, evenApproximation, TooCoarse);
    REQUIRE_FALSE(padded.has_value());
    REQUIRE_FALSE(paddedThird.has_value());
    REQUIRE_FALSE(approximated.has_value());
    REQUIRE_FALSE(approximatedCoarse.has_value());
    CHECK(padded.error() == ArithmeticError::Overflow);
    CHECK(paddedThird.error() == ArithmeticError::Overflow);
    CHECK(approximated.error() == ArithmeticError::Overflow);
    CHECK(approximatedCoarse.error() == ArithmeticError::Overflow);

    // -19 decimals on a value with an exact decimal, 1/2. checked_round never
    // runs here -- the exact decimal needs no rounding, and padding to a
    // negative number of places adds nothing -- so only the refusal itself
    // can say Overflow, where without it both would read 0.5.
    // Each checked on its own, so that one failing does not hide the other.
    std::unexpected const refused { ArithmeticError::Overflow };
    CHECK(formula::checked_number_text(Rational { 1, 2 }, paddedExact, TooCoarse) == refused);
    CHECK(formula::checked_number_text(Rational { 1, 2 }, evenApproximation, TooCoarse) == refused);

    CHECK_THROWS_AS(formula::number_text(Rational { 1, 2 }, paddedExact, TooFine), ArithmeticException);
}

// ---- a measured value ----

TEST_CASE("a measured value is its number then its unit's symbol", "[number_text]")
{
    constexpr NumberStyle paddedExact = NumberStyle::exact_decimal(DecimalPadding::Padded);
    constexpr NumberStyle evenApproximation = NumberStyle::approximate_decimal(RoundingMode::HalfEven);
    constexpr NumberStyle ceilingApproximation = NumberStyle::approximate_decimal(RoundingMode::Ceiling);

    STATIC_REQUIRE(unit::Kilojoule.decimals == 1);
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 26, 5 } }, NumberStyle::exact_decimal())
                   == "5.2 kJ");
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 26, 5 } }, NumberStyle::fraction()) == "26/5 kJ");
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 5 } }, NumberStyle::exact_decimal()) == "5 kJ");
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 5 } }, paddedExact) == "5.0 kJ");
    // 47/9 is 5.222...: to 1 place, 5.2 to nearest and 5.3 toward positive infinity.
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 47, 9 } }, evenApproximation)
                   == "\xe2\x89\x88" "5.2 kJ");
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> { Rational { 47, 9 } }, ceilingApproximation)
                   == "\xe2\x89\x88" "5.3 kJ");

    // No symbol, no space.
    STATIC_REQUIRE(formula::number_text(Measured<Share> { Rational { 3, 5 } }, NumberStyle::fraction()) == "3/5");

    // Absent is not zero, and not a number.
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> {}, NumberStyle::exact_decimal()) == "(not measured)");
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> {}, NumberStyle::exact_decimal()) == formula::NotMeasuredText);
    STATIC_REQUIRE(formula::number_text(Measured<ImpactWork> {}, evenApproximation).is_exact());
}

TEST_CASE("a measured value in a dimensioned unit with no symbol is shown in the coherent unit", "[number_text]")
{
    // 3 of a unit of 1/1000 kg is 3/1000 kg: the number is moved into the unit
    // its text names, never left on a scale nothing after it states.
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedMass> { Rational { 3 } }, NumberStyle::fraction()) == "3/1000 kg");
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedMass> { Rational { 3 } }, NumberStyle::exact_decimal())
                   == "0.003 kg");
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedMass> { Rational { 3 } },
                                        NumberStyle::approximate_decimal(RoundingMode::HalfEven))
                   == "0.003 kg");
    // The coherent unit's 3 places are a default nobody chose, as in a trace:
    // never padded to, and never rounding a value that is not zero to `≈0`.
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedMass> { Rational { 30 } },
                                        NumberStyle::exact_decimal(DecimalPadding::Padded))
                   == "0.03 kg");
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedMass> { Rational { 1, 3 } },
                                        NumberStyle::approximate_decimal(RoundingMode::HalfEven))
                   == "\xe2\x89\x88" "0.0003 kg");
    // A unit with only a negative exponent is written with it, and no slash.
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedLoading> { Rational { 3 } }, NumberStyle::fraction())
                   == "3000 kg^-1");
    // A point on an offset scale moves to the coherent unit's: 21 on the
    // unnamed Celsius scale is 294.15 K.
    STATIC_REQUIRE(formula::number_text(Measured<UnnamedReading> { Rational { 21 } }, NumberStyle::exact_decimal())
                   == "294.15 K");

    // A unit with a symbol keeps the number in that unit, as before.
    STATIC_REQUIRE(formula::number_text(Measured<NamedMass> { Rational { 3 } }, NumberStyle::fraction()) == "3 g");

    // A move that fails is refused, never written in the declared unit's scale:
    // the largest number on the unnamed Celsius scale has no place in kelvin.
    STATIC_REQUIRE(formula::checked_number_text(Measured<UnnamedReading> { Rational { IntMax } }, NumberStyle::fraction())
                       .error()
                   == ArithmeticError::Overflow);
    // A coherent unit's spelling that does not fit the buffer is refused as a
    // number too long for it would be.
    STATIC_REQUIRE(formula::checked_number_text(Measured<SprawlingReading> { Rational { 3 } }, NumberStyle::fraction())
                       .error()
                   == ArithmeticError::Overflow);
}

TEST_CASE("the longest text this library spells fits its buffer", "[number_text]")
{
    // A sign, the 39 digits of 2^127, a slash, the 39 digits of 2^127 - 1, a
    // space and a 32-byte symbol: 113 bytes, the longest text the buffer is
    // sized for.
    NumberText const widest =
        formula::number_text(Measured<WidestReading> { Rational { IntMin, IntMax } }, NumberStyle::fraction());
    CHECK(widest.view()
          == "-170141183460469231731687303715884105728/170141183460469231731687303715884105727 "
             "abcdefghijklmnopqrstuvwxyz012345");
    CHECK(widest.view().size() == formula::detail::LongestNumberText);

    // The longest marked decimal -- the marker, a sign, 38 whole digits, a
    // point, 18 places, a space and the symbol -- is 94 bytes, shorter.
    NumberText const widestDecimal = formula::number_text(Measured<WidestReading> { Rational { IntMin, 3 } },
                                                          NumberStyle::approximate_decimal(RoundingMode::HalfEven));
    CHECK(widestDecimal.view()
          == "\xe2\x89\x88" "-56713727820156410577229101238628035242.666666666666666667 "
             "abcdefghijklmnopqrstuvwxyz012345");
    CHECK(widestDecimal.view().size() == 94);
}

// ---- a quantity's own decimal places ----

namespace
{

// Invented: milliamperes, declared to whole milliamperes, and a quantity in
// them that declares one place of its own.
inline constexpr formula::Unit Milliampere { .dimension = dim::Current,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000,
                                             .symbolText = formula::symbol("mA"),
                                             .decimals = 0 };
struct FineCurrent: formula::Quantity<FineCurrent, "I_f", "a current read to a tenth of a milliampere", Milliampere,
                                      DecimalPlaces { 1 }>
{
};
struct CoarseCurrent: formula::Quantity<CoarseCurrent, "I_c", "a current read to whole milliamperes", Milliampere>
{
};

} // namespace

TEST_CASE("a measured value is written to the places its quantity declares", "[number_text][decimals]")
{
    constexpr NumberStyle paddedExact = NumberStyle::exact_decimal(DecimalPadding::Padded);
    constexpr NumberStyle evenApproximation = NumberStyle::approximate_decimal(RoundingMode::HalfEven);

    // 37/3 mA is 12.333... mA: rounded, to one place and to none.
    STATIC_REQUIRE(formula::number_text(Measured<FineCurrent> { Rational { 37, 3 } }, evenApproximation)
                   == "\xe2\x89\x88" "12.3 mA");
    STATIC_REQUIRE(formula::number_text(Measured<CoarseCurrent> { Rational { 37, 3 } }, evenApproximation)
                   == "\xe2\x89\x88" "12 mA");
    // An exact decimal is never cut short, whatever the places.
    STATIC_REQUIRE(formula::number_text(Measured<FineCurrent> { Rational { 1234, 100 } }, evenApproximation)
                   == "12.34 mA");
    STATIC_REQUIRE(formula::number_text(Measured<CoarseCurrent> { Rational { 1234, 100 } }, evenApproximation)
                   == "12.34 mA");
    // Padded to the places each declares.
    STATIC_REQUIRE(formula::number_text(Measured<FineCurrent> { Rational { 12 } }, paddedExact) == "12.0 mA");
    STATIC_REQUIRE(formula::number_text(Measured<CoarseCurrent> { Rational { 12 } }, paddedExact) == "12 mA");
}
