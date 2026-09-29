// SPDX-License-Identifier: Apache-2.0
//
// std::format of a Rational and a Measured (format.hpp): every form of the
// spec, every example the header's documentation states, the width in code
// points, each refusal at run time, and agreement with number_text for the
// same value and style.
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::Measured;
using formula::NumberStyle;
using formula::Rational;
using formula::RoundingMode;
using formula::detail::NumberFormatAlign;
using formula::detail::NumberFormatBody;
using formula::detail::parse_number_format;

struct ImpactWork: formula::Quantity<ImpactWork, "W_i", "impact work", unit::Kilojoule>
{
};
struct Reading: formula::Quantity<Reading, "T", "temperature reading", unit::Celsius>
{
};
struct Share: formula::Quantity<Share, "s_h", "a share", unit::One>
{
};

/// A unit declaring more decimals than `DecimalPlaces` spans: 19. Invented.
inline constexpr formula::Unit OverPrecise { .dimension = formula::dim::Length,
                                             .symbolText = formula::symbol("u"),
                                             .decimals = 19 };
struct OverPreciseLength: formula::Quantity<OverPreciseLength, "l_u", "a length in an over-precise unit", OverPrecise>
{
};

/// A unit declaring fewer decimals than `DecimalPlaces` spans: -19. Invented.
inline constexpr formula::Unit UnderPrecise { .dimension = formula::dim::Length,
                                              .symbolText = formula::symbol("v"),
                                              .decimals = -19 };
struct UnderPreciseLength: formula::Quantity<UnderPreciseLength, "l_v", "a length in an under-precise unit", UnderPrecise>
{
};

/// A unit declaring -3 decimals -- a value rounded at it is rounded to thousands. Invented.
inline constexpr formula::Unit Coarse { .dimension = formula::dim::Length,
                                        .symbolText = formula::symbol("ku"),
                                        .decimals = -3 };
struct CoarseLength: formula::Quantity<CoarseLength, "l_k", "a length in a coarse unit", Coarse>
{
};

/// The text of the `std::format_error` @p formatString throws formatting
/// @p shown through `std::vformat`, which checks the spec only at run time;
/// empty when it throws none.
template <typename T>
[[nodiscard]] std::string refusalOf(std::string_view formatString, T const& shown)
{
    try
    {
        (void) std::vformat(formatString, std::make_format_args(shown));
    }
    catch (std::format_error const& refusal)
    {
        return refusal.what();
    }
    return {};
}
} // namespace

TEST_CASE("a Rational formats as its exact decimal, else its fraction", "[format]")
{
    CHECK(std::format("{}", Rational { 3, 5 }) == "0.6");
    CHECK(std::format("{}", Rational { 1, 3 }) == "1/3");
    CHECK(std::format("{}", Rational { -7, 4 }) == "-1.75");
    CHECK(std::format("{}", Rational { 4 }) == "4");
    CHECK(std::format("{:/}", Rational { 3, 5 }) == "3/5");
    CHECK(std::format("{:/}", Rational { 1, 3 }) == "1/3");
}

TEST_CASE("a Rational rounds only to places and in a mode the spec names", "[format]")
{
    // 118.265 is a tie at two places: the mode decides it.
    CHECK(std::format("{:.2HalfEven}", Rational { 23653, 200 }) == "118.26");
    CHECK(std::format("{:.2HalfAwayFromZero}", Rational { 23653, 200 }) == "118.27");
    // Padded to the places, and never marked: the spec asked for a rounding.
    CHECK(std::format("{:.2HalfEven}", Rational { 4 }) == "4.00");
    CHECK(std::format("{:.3HalfEven}", Rational { 1, 3 }) == "0.333");
    CHECK(std::format("{:.0Ceiling}", Rational { 1, 3 }) == "1");
    CHECK(std::format("{:.2Floor}", Rational { -1, 1000 }) == "-0.01");

    // `~`: exact where exact, unpadded; otherwise rounded and marked.
    CHECK(std::format("{:~.3HalfEven}", Rational { 1, 3 }) == "\xe2\x89\x88" "0.333");
    CHECK(std::format("{:~.3HalfEven}", Rational { 3, 5 }) == "0.6");
    CHECK(std::format("{:~.1HalfEven}", Rational { 123, 1000 }) == "0.123"); // exact beats the places
    CHECK(std::format("{:~.2Ceiling}", Rational { 1, 3 }) == "\xe2\x89\x88" "0.34");
}

TEST_CASE("a formatted number is filled and aligned to a width counted in code points", "[format]")
{
    CHECK(std::format("{:>8}", Rational { 3, 5 }) == "     0.6");
    CHECK(std::format("{:8}", Rational { 3, 5 }) == "     0.6"); // right by default
    CHECK(std::format("{:<8}", Rational { 3, 5 }) == "0.6     ");
    CHECK(std::format("{:*^7}", Rational { 3, 5 }) == "**0.6**");
    CHECK(std::format("{:*^6}", Rational { 3, 5 }) == "*0.6**"); // the odd fill after
    CHECK(std::format("{:2}", Rational { 3, 5 }) == "0.6");      // never cut short
    // `≈` is three bytes and one code point: six characters, two of fill.
    CHECK(std::format("{:>8~.3HalfEven}", Rational { 1, 3 }) == "  \xe2\x89\x88" "0.333");
    // A fill of more than one byte is one code point too: two, three or four.
    CHECK(std::format("{:\xc2\xb7>6}", Rational { 3, 5 }) == "\xc2\xb7\xc2\xb7\xc2\xb7" "0.6");
    CHECK(std::format("{:\xe2\x80\xa6>5}", Rational { 3, 5 }) == "\xe2\x80\xa6\xe2\x80\xa6" "0.6");
    CHECK(std::format("{:\xf0\x90\x8d\x88<4}", Rational { 3, 5 }) == "0.6\xf0\x90\x8d\x88");
    // A fill is one Unicode scalar value: the lowest three-byte one, U+0800,
    // the highest below the surrogates, U+D7FF, and the highest, U+10FFFF.
    CHECK(std::format("{:\xe0\xa0\x80<4}", Rational { 3, 5 }) == "0.6\xe0\xa0\x80");
    CHECK(std::format("{:\xed\x9f\xbf<4}", Rational { 3, 5 }) == "0.6\xed\x9f\xbf");
    CHECK(std::format("{:\xf4\x8f\xbf\xbf<4}", Rational { 3, 5 }) == "0.6\xf4\x8f\xbf\xbf");
    // `°` is two bytes: 21.3 °C is seven characters, and one fill makes eight.
    CHECK(std::format("{:>8}", Measured<Reading> { Rational { 213, 10 } }) == " 21.3 \xc2\xb0" "C");
}

TEST_CASE("a Measured formats in its unit, with its symbol, or as not measured", "[format]")
{
    // The examples in format.hpp's documentation, in unit::Kilojoule (one
    // declared decimal).
    CHECK(std::format("{}", Measured<ImpactWork> { Rational { 26, 5 } }) == "5.2 kJ");
    CHECK(std::format("{}", Measured<ImpactWork> { Rational { 1, 3 } }) == "1/3 kJ");
    CHECK(std::format("{:/}", Measured<ImpactWork> { Rational { 26, 5 } }) == "26/5 kJ");
    CHECK(std::format("{:.3HalfEven}", Measured<ImpactWork> { Rational { 26, 5 } }) == "5.200 kJ");
    CHECK(std::format("{:~HalfEven}", Measured<ImpactWork> { Rational { 1, 3 } }) == "\xe2\x89\x88" "0.3 kJ");
    CHECK(std::format("{:~.3HalfEven}", Measured<ImpactWork> { Rational { 1, 3 } }) == "\xe2\x89\x88" "0.333 kJ");
    CHECK(std::format("{:>10}", Measured<ImpactWork> { Rational { 26, 5 } }) == "    5.2 kJ");
    CHECK(std::format("{}", Measured<ImpactWork>::absent()) == "(not measured)");
    // Absent whatever the body, and still filled.
    CHECK(std::format("{:.2HalfEven}", Measured<ImpactWork>::absent()) == "(not measured)");
    CHECK(std::format("{:>16}", Measured<ImpactWork>::absent()) == "  (not measured)");
    // A unit with no symbol writes none, and no space.
    CHECK(std::format("{}", Measured<Share> { Rational { 3, 5 } }) == "0.6");
    CHECK(std::format("{:~HalfEven}", Measured<Share> { Rational { 1, 3 } }) == "\xe2\x89\x88" "0.333");
}

TEST_CASE("std::format and number_text spell one value in one style alike", "[format]")
{
    // The same value, the same notation, the same unit: the two surfaces must
    // not drift apart. unit::One declares three decimals, which `~.3` names.
    for (Rational const shown : { Rational { 1, 3 }, Rational { 3, 5 }, Rational { -2, 7 }, Rational { 23653, 200 },
                                  Rational { 1, 262144 }, Rational { 1, 524288 } })
    {
        formula::NumberText const halfEven =
            formula::number_text(shown, NumberStyle::approximate_decimal(RoundingMode::HalfEven), unit::One);
        formula::NumberText const floored =
            formula::number_text(shown, NumberStyle::approximate_decimal(RoundingMode::Floor), unit::One);
        formula::NumberText const exact = formula::number_text(shown, NumberStyle::exact_decimal(), unit::One);
        formula::NumberText const fraction = formula::number_text(shown, NumberStyle::fraction(), unit::One);
        CHECK(std::format("{:~.3HalfEven}", shown) == halfEven.view());
        CHECK(std::format("{:~.3Floor}", shown) == floored.view());
        CHECK(std::format("{}", shown) == exact.view());
        CHECK(std::format("{:/}", shown) == fraction.view());
    }
    for (Rational const shown : { Rational { 1, 3 }, Rational { 26, 5 }, Rational { -47, 9 } })
    {
        Measured<ImpactWork> const measured { shown };
        formula::NumberText const halfEven =
            formula::number_text(measured, NumberStyle::approximate_decimal(RoundingMode::HalfEven));
        formula::NumberText const ceiling =
            formula::number_text(measured, NumberStyle::approximate_decimal(RoundingMode::Ceiling));
        formula::NumberText const exact = formula::number_text(measured, NumberStyle::exact_decimal());
        formula::NumberText const fraction = formula::number_text(measured, NumberStyle::fraction());
        CHECK(std::format("{:~HalfEven}", measured) == halfEven.view());
        CHECK(std::format("{:~Ceiling}", measured) == ceiling.view());
        CHECK(std::format("{}", measured) == exact.view());
        CHECK(std::format("{:/}", measured) == fraction.view());
    }
}

TEST_CASE("a spec the grammar does not allow is refused at run time, in the library's words", "[format]")
{
    // The same specs are compile errors in a literal format string -- see the
    // negative cases format_places_without_mode, format_places_out_of_range
    // and format_spec_not_understood. Through std::vformat they are checked
    // when used, and throw.
    Rational const third { 1, 3 };
    CHECK(refusalOf("{:.2}", third).starts_with("formula: this number format rounds but names no rounding mode"));
    CHECK(refusalOf("{:~.3}", third).starts_with("formula: this number format rounds but names no rounding mode"));
    CHECK(refusalOf("{:~}", third).starts_with("formula: this number format rounds but names no rounding mode"));
    CHECK(refusalOf("{:.19HalfEven}", third).starts_with("formula: a number format rounds to 0 to 18 decimal places"));
    CHECK(refusalOf("{:.99999999999999999999HalfEven}", third)
              .starts_with("formula: a number format rounds to 0 to 18 decimal places"));
    for (std::string_view const unknown : { "{:x}", "{:.2halfeven}", "{:.2Half}", "{:08}", "{:/2}", "{:.}", "{:~HalfEven}",
                                            "{:1234567890}", "{:{<5}" })
        CHECK(refusalOf(unknown, third).starts_with("formula: this number format is not one formula-cpp understands"));
    // A fill that is not one whole UTF-8 character: a two-byte lead, then a
    // letter where its second byte belongs.
    CHECK(refusalOf("{:\xc2" "A<5}", third).starts_with("formula: this number format is not one formula-cpp understands"));
    // Nor one that is no Unicode scalar value: overlong encodings of two,
    // three and four bytes, a surrogate (U+D800), one past U+10FFFF, a lead
    // byte no scalar value starts with, a three-byte one cut short, and a
    // three- and a four-byte one whose last byte continues nothing, an
    // alignment after it.
    for (std::string_view const notScalar : { "{:\xc0\x80<5}",
                                              "{:\xc1\xbf<5}",
                                              "{:\xe0\x9f\xbf<5}",
                                              "{:\xed\xa0\x80<5}",
                                              "{:\xf0\x8f\xbf\xbf<5}",
                                              "{:\xf4\x90\x80\x80<5}",
                                              "{:\xf5\x80\x80\x80<5}",
                                              "{:\xe2\x89<5}",
                                              "{:\xe2\x89" "A<5}",
                                              "{:\xf0\x90\x8d" "A<5}" })
        CHECK(refusalOf(notScalar, third).starts_with("formula: this number format is not one formula-cpp understands"));
    // A width from an argument is refused too.
    int const argumentWidth = 8;
    try
    {
        (void) std::vformat("{:{}}", std::make_format_args(third, argumentWidth));
        FAIL("a width from an argument was accepted");
    }
    catch (std::format_error const& refusal)
    {
        CHECK(std::string_view { refusal.what() }.starts_with("formula: this number format is not one formula-cpp"));
    }

    // `~Mode` rounds a Measured at its unit's decimals, which must lie within
    // -18 to 18.
    Measured<OverPreciseLength> const overPrecise { Rational { 1, 3 } };
    CHECK(refusalOf("{:~HalfEven}", overPrecise).starts_with("formula: a number format rounds to 0 to 18 decimal places"));
    CHECK(std::format("{:~.3HalfEven}", overPrecise) == "\xe2\x89\x88" "0.333 u");
    CHECK(refusalOf("{:~HalfEven}", Measured<UnderPreciseLength> { third })
              .starts_with("formula: a number format rounds to 0 to 18 decimal places"));
    CHECK(refusalOf("{:~HalfEven}", Measured<ImpactWork> { third }).empty());

    // `~Mode` at a unit's negative decimals divides the value by 10^3 in
    // exact arithmetic, which overflows for from_double_exact(0.1) -- its
    // denominator is 2^55 -- although the rounded value would be 0. It is
    // refused when written, with a literal format string and under
    // std::vformat alike, rather than spelled some other way.
    Rational const binaryTenth = Rational::from_double_exact(0.1).value();
    REQUIRE(binaryTenth == Rational { 3602879701896397, 36028797018963968 });
    Measured<CoarseLength> const coarseTenth { binaryTenth };
    CHECK(refusalOf("{:~HalfEven}", coarseTenth)
          == "formula: this number cannot be spelled as the format asks: overflow in exact arithmetic");
    CHECK_THROWS_AS(std::format("{:~HalfEven}", coarseTenth), std::format_error);
    // Its exact forms are still written; and in the same unit a value that
    // exact arithmetic can round rounds: 7501/3 is 2500.33..., 3000 to the
    // thousand.
    CHECK(std::format("{:/}", coarseTenth) == "3602879701896397/36028797018963968 ku");
    CHECK(std::format("{:~HalfEven}", Measured<CoarseLength> { Rational { 7501, 3 } }) == "\xe2\x89\x88" "3000 ku");
    // A value with an exact decimal of at most 18 places is written as it
    // is, never rounded, so never divided: 1/10^18, whose rounding at -3
    // places overflows, is written exactly.
    Rational const atto { 1, 1'000'000'000'000'000'000 };
    REQUIRE(!formula::checked_decimal_text(
        atto, formula::DecimalPlaces { -3 }, RoundingMode::HalfEven, formula::DecimalPadding::Trimmed));
    CHECK(std::format("{:~HalfEven}", Measured<CoarseLength> { atto }) == "0.000000000000000001 ku");
}

TEST_CASE("the spec parser reads each form, at compile time", "[format]")
{
    STATIC_REQUIRE(parse_number_format("").body == NumberFormatBody::ExactOrFraction);
    STATIC_REQUIRE(parse_number_format("").align == NumberFormatAlign::Right);
    STATIC_REQUIRE(parse_number_format("").minimumWidth == 0);
    STATIC_REQUIRE(parse_number_format("/").body == NumberFormatBody::Fraction);
    STATIC_REQUIRE(parse_number_format(".2HalfEven").body == NumberFormatBody::Rounded);
    STATIC_REQUIRE(parse_number_format(".2HalfEven").places == 2);
    STATIC_REQUIRE(parse_number_format(".18Floor").places == 18);
    STATIC_REQUIRE(parse_number_format(".18Floor").roundingMode == RoundingMode::Floor);
    STATIC_REQUIRE(parse_number_format("~.3HalfEven").body == NumberFormatBody::Approximated);
    STATIC_REQUIRE(parse_number_format("~.3HalfEven").places == 3);
    STATIC_REQUIRE(parse_number_format("~Ceiling").body == NumberFormatBody::Approximated);
    STATIC_REQUIRE(!parse_number_format("~Ceiling").places.has_value());
    STATIC_REQUIRE(parse_number_format("*^7").align == NumberFormatAlign::Centre);
    STATIC_REQUIRE(parse_number_format("*^7").minimumWidth == 7);
    STATIC_REQUIRE(parse_number_format("*^7").fill[0] == '*');
    STATIC_REQUIRE(parse_number_format("<12/").align == NumberFormatAlign::Left);
    STATIC_REQUIRE(parse_number_format("<12/").minimumWidth == 12);
    STATIC_REQUIRE(parse_number_format("<12/").body == NumberFormatBody::Fraction);
    STATIC_REQUIRE(parse_number_format("\xc2\xb7>6").fillLength == 2);
    STATIC_REQUIRE(parse_number_format("<<4").fill[0] == '<');
    // A fill cut short by the spec's end is no scalar value: its length is
    // checked before a continuation byte is read. Each view ends inside a
    // whole character, so a read past its end would find a valid
    // continuation byte and count the character whole.
    STATIC_REQUIRE(formula::detail::scalar_value_length(std::string_view { "\xe2\x89\x88", 2 }) == 0);
    STATIC_REQUIRE(formula::detail::scalar_value_length(std::string_view { "\xf0\x90\x8d\x88", 3 }) == 0);
    STATIC_REQUIRE(formula::detail::scalar_value_length(std::string_view { "\xe2\x89\x88", 3 }) == 3);

    // Every mode by its enumerator's name, and only by it.
    STATIC_REQUIRE(parse_number_format(".1HalfAwayFromZero").roundingMode == RoundingMode::HalfAwayFromZero);
    STATIC_REQUIRE(parse_number_format(".1HalfTowardZero").roundingMode == RoundingMode::HalfTowardZero);
    STATIC_REQUIRE(parse_number_format(".1HalfEven").roundingMode == RoundingMode::HalfEven);
    STATIC_REQUIRE(parse_number_format(".1Ceiling").roundingMode == RoundingMode::Ceiling);
    STATIC_REQUIRE(parse_number_format(".1Floor").roundingMode == RoundingMode::Floor);
    STATIC_REQUIRE(parse_number_format(".1TowardZero").roundingMode == RoundingMode::TowardZero);
    STATIC_REQUIRE(parse_number_format(".1AwayFromZero").roundingMode == RoundingMode::AwayFromZero);
}
