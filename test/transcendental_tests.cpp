// SPDX-License-Identifier: Apache-2.0
//
// The integer kernel behind the rounded logarithms and exponential (detail/transcendental.hpp),
// against an independent reference. Each reference row is the value's first 40 significant digits,
// truncated, so that |value| lies in [D, D + 1] * 10^-scale. The digits were computed once with
// Python 3.13's decimal module (libmpdec), whose ln, log10 and exp are correctly rounded, at 150
// significant digits:
//
//     from decimal import Decimal, getcontext, ROUND_FLOOR
//     getcontext().prec = 150
//     v = Decimal(a).ln() - Decimal(b).ln()          # log10: .log10(); exp: (Decimal(a) / Decimal(b)).exp()
//     scale = 40 - (abs(v).adjusted() + 1)
//     D = int((abs(v) * Decimal(10) ** scale).to_integral_value(rounding=ROUND_FLOOR))
//
// The published values the stored constants are checked against: ln 2 =
// 0.6931471805599453094172321214581765680755..., log10(e) = 0.4342944819032518276511289189166050822943...,
// and ln 2 to 60 digits, for the exponential's 192-bit constant:
// 0.693147180559945309417232121458176568075500134360255254120680....
//
// Every check that runs the kernel runs at run time, but one: a whole rounding costs a quarter of a
// compiler's default constant-evaluation budget, too near it to pin many. The last case keeps one at
// compile time, on purpose. The stored constants, which run no kernel, are checked at compile time.
#include <formula-cpp/detail/transcendental.hpp>
#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
namespace detail = formula::detail;
using formula::DecimalPlaces;
using formula::Rational;
using formula::RoundingMode;
using formula::Transcendental;
using Word = detail::KernelWord;
using Ratio = detail::WideRatio<detail::KernelLimbs>;

constexpr std::array<RoundingMode, 7> everyMode {
    RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven,
    RoundingMode::Ceiling,          RoundingMode::Floor,          RoundingMode::TowardZero,
    RoundingMode::AwayFromZero
};

/// The kernel's enclosure of @p function at @p argument.
[[nodiscard]] constexpr std::optional<detail::Enclosure> enclosure_of(Transcendental function, Rational argument)
{
    switch (function)
    {
        case Transcendental::NaturalLogarithm:
            return detail::natural_log_enclosure(argument);
        case Transcendental::DecimalLogarithm:
            return detail::decimal_log_enclosure(argument);
        case Transcendental::Exponential:
            return detail::exponential_enclosure(argument);
    }
    return std::nullopt;
}

/// @p function at @p argument rounded to @p places under @p roundingMode by the kernel's enclosure.
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> kernel_rounding(Transcendental function,
                                                                                          Rational argument,
                                                                                          int places,
                                                                                          RoundingMode roundingMode)
{
    std::optional<detail::Enclosure> const enclosure = enclosure_of(function, argument);
    if (!enclosure.has_value())
        return std::unexpected { formula::ArithmeticError::Overflow };
    return detail::decide_rounding(enclosure->lower, enclosure->upper, DecimalPlaces { places }, roundingMode);
}

/// A word of 16 limbs, wide enough for the checks' cross products.
using CheckWord = detail::WideUnsigned<16>;

/// @p narrow, the same value, in a `CheckWord`.
[[nodiscard]] constexpr CheckWord widened_word(Word const& narrow)
{
    std::array<std::uint32_t, 16> limbsCopied {};
    for (std::size_t limbAt = 0; limbAt < detail::KernelLimbs; ++limbAt)
        limbsCopied[limbAt] = narrow.limb(limbAt);
    return CheckWord::from_limbs(limbsCopied);
}

/// The decimal digits @p decimalDigits as a word of @p Limbs limbs.
template <std::size_t Limbs = detail::KernelLimbs>
[[nodiscard]] constexpr detail::WideUnsigned<Limbs> word_of(std::string_view decimalDigits)
{
    detail::WideUnsigned<Limbs> parsed {};
    for (char const each: decimalDigits)
        parsed = *detail::add_small_checked_or_none(*detail::mul_small_checked_or_none(parsed, 10U),
                                                    static_cast<std::uint32_t>(each - '0'));
    return parsed;
}

/// Whether @p left <= @p right, for two signed ratios with positive denominators.
[[nodiscard]] constexpr bool at_most(Ratio const& left, Ratio const& right)
{
    bool const leftNegative = left.negative && !left.numerator.is_zero();
    bool const rightNegative = right.negative && !right.numerator.is_zero();
    if (leftNegative != rightNegative)
        return leftNegative;
    CheckWord const leftCross = *detail::mul_checked_or_none(widened_word(left.numerator), widened_word(right.denominator));
    CheckWord const rightCross = *detail::mul_checked_or_none(widened_word(right.numerator), widened_word(left.denominator));
    return leftNegative ? rightCross <= leftCross : leftCross <= rightCross;
}

/// Whether @p stored is floor(v * 2^@p fractionBits) for the value v below one whose first significant digits
/// are @p published, as many as it has characters: (D - 1) * 2^bits >= stored * 10^digits and
/// (D + 1) * 2^bits <= (stored + 1) * 10^digits. That interval is 2 * 2^bits / 10^digits units wide -- under
/// 0.07 for 128 bits and 40 digits, under 0.013 for 192 bits and 60 -- so it pins the floor.
[[nodiscard]] constexpr bool pinned_by_published_digits(Word const& stored,
                                                        std::size_t fractionBits,
                                                        std::string_view published)
{
    CheckWord const digitsValue = word_of<16>(published);
    CheckWord const unit = *detail::shift_left_checked_or_none(CheckWord::from_u64(1), fractionBits);
    CheckWord const tenToDigits = *detail::pow10<16>(published.size());
    CheckWord const storedWide = widened_word(stored);
    return *detail::mul_checked_or_none(storedWide, tenToDigits)
               <= *detail::mul_checked_or_none(*detail::sub_checked_or_none(digitsValue, CheckWord::from_u64(1)), unit)
           && *detail::mul_checked_or_none(*detail::add_small_checked_or_none(digitsValue, 1U), unit)
                  <= *detail::mul_checked_or_none(*detail::add_small_checked_or_none(storedWide, 1U), tenToDigits);
}

/// One row of the reference table: |value| lies in [digits, digits + 1] * 10^-scale.
struct Reference
{
    Transcendental function;
    formula::Rational::Int numerator;
    formula::Rational::Int denominator;
    bool negative;
    std::string_view digits;
    std::size_t scale;
};

constexpr Rational::Int largestInt = std::numeric_limits<Rational::Int>::max();  // 2^127 - 1
constexpr Rational::Int smallestInt = std::numeric_limits<Rational::Int>::min(); // -2^127

// clang-format off
constexpr std::array<Reference, 51> references { {
    { Transcendental::NaturalLogarithm, 2, 1, false, "6931471805599453094172321214581765680755", 40 },
    { Transcendental::NaturalLogarithm, 1, 2, true, "6931471805599453094172321214581765680755", 40 },
    { Transcendental::NaturalLogarithm, 3, 1, false, "1098612288668109691395245236922525704647", 39 },
    { Transcendental::NaturalLogarithm, 10, 1, false, "2302585092994045684017991454684364207601", 39 },
    { Transcendental::NaturalLogarithm, 7, 5, false, "3364722366212129305045934102169920901114", 40 },
    { Transcendental::NaturalLogarithm, 4611686018427387904, 1, false, "4297512519471660918386839153040694722068", 38 },
    { Transcendental::NaturalLogarithm, 4611686018427387903, 2305843009213693952, false,
      "6931471805599453092003916869610756812504", 40 },
    { Transcendental::NaturalLogarithm, 9223372036854775807, 9223372036854775806, false,
      "1084202172485504434183776953493284837650", 58 },
    { Transcendental::NaturalLogarithm, 9223372036854775807, 1, false, "4366827237527655449317720343461657334534", 38 },
    { Transcendental::NaturalLogarithm, 1, 9223372036854775807, true, "4366827237527655449317720343461657334534", 38 },
    { Transcendental::NaturalLogarithm, 1000001, 1000000, false, "9999995000003333330833335333331666668095", 46 },
    { Transcendental::NaturalLogarithm, 999999, 1000000, true, "1000000500000333333583333533333500000142", 45 },
    { Transcendental::NaturalLogarithm, 22, 7, false, "1145132304303002548373822955980126138260", 39 },
    { Transcendental::NaturalLogarithm, 1, 20, true, "2995732273553990993435223576142540775676", 39 },
    { Transcendental::NaturalLogarithm, 355, 113, false, "1144729970763075227348082372123200064255", 39 },
    { Transcendental::NaturalLogarithm, 1000000000000000000, 1, false, "4144653167389282231232384618431855573681", 38 },
    { Transcendental::DecimalLogarithm, 2, 1, false, "3010299956639811952137388947244930267681", 40 },
    { Transcendental::DecimalLogarithm, 1, 2, true, "3010299956639811952137388947244930267681", 40 },
    { Transcendental::DecimalLogarithm, 3, 1, false, "4771212547196624372950279032551153092001", 40 },
    { Transcendental::DecimalLogarithm, 7, 1, false, "8450980400142568307122162585926361934835", 40 },
    { Transcendental::DecimalLogarithm, 20, 1, false, "1301029995663981195213738894724493026768", 39 },
    { Transcendental::DecimalLogarithm, 1, 3, true, "4771212547196624372950279032551153092001", 40 },
    { Transcendental::DecimalLogarithm, 999999999999999999, 1, false, "1799999999999999999956570551809674817213", 38 },
    { Transcendental::DecimalLogarithm, 4611686018427387904, 1, false, "1866385973116683410325181147291856765962", 38 },
    { Transcendental::DecimalLogarithm, 5, 1, false, "6989700043360188047862611052755069732318", 40 },
    { Transcendental::DecimalLogarithm, 1001, 1000, false, "4340774793186406689213877779888660200037", 43 },
    { Transcendental::Exponential, 1, 1, false, "2718281828459045235360287471352662497757", 39 },
    { Transcendental::Exponential, -1, 1, false, "3678794411714423215955237701614608674458", 40 },
    { Transcendental::Exponential, 43, 1, false, "4727839468229346561474457562744280370819", 21 },
    { Transcendental::Exponential, -43, 1, false, "2115131037591080486631401007022651470196", 58 },
    { Transcendental::Exponential, 1, 2, false, "1648721270700128146848650787814163571653", 39 },
    { Transcendental::Exponential, -1, 2, false, "6065306597126334236037995349911804534419", 40 },
    { Transcendental::Exponential, 10, 1, false, "2202646579480671651695790064528424436635", 35 },
    { Transcendental::Exponential, -10, 1, false, "4539992976248485153559151556055061023791", 44 },
    { Transcendental::Exponential, 1, 1000, false, "1001000500166708341668055753993058311563", 39 },
    { Transcendental::Exponential, 437, 10, false, "9520699529632624602822133077880255204738", 21 },
    { Transcendental::Exponential, 1, 4611686018427387904, false, "1000000000000000000216840434497100886825", 39 },
    { Transcendental::Exponential, -1, 4611686018427387904, false, "9999999999999999997831595655028991132220", 40 },
    { Transcendental::Exponential, 44, 1, false, "1285160011435930827580929963214309925780", 20 },
    { Transcendental::NaturalLogarithm, Rational::Int { 1 } << 70, 1, false, "4852030263919617165920624850207235976528", 38 },
    { Transcendental::NaturalLogarithm, largestInt, 1, false, "8802969193111305429598847942518842414558", 38 },
    { Transcendental::NaturalLogarithm, 1, largestInt, true, "8802969193111305429598847942518842414558", 38 },
    { Transcendental::NaturalLogarithm, (Rational::Int { 1 } << 126) + 1, Rational::Int { 1 } << 126, false,
      "1175494350822287507968736537222245677811", 77 },
    { Transcendental::DecimalLogarithm, Rational::Int { 1 } << 70, 1, false, "2107209969647868366496172263071451187377", 38 },
    { Transcendental::DecimalLogarithm, largestInt, 1, false, "3823080944932561179214483963001061439955", 38 },
    { Transcendental::DecimalLogarithm, 1, largestInt, true, "3823080944932561179214483963001061439955", 38 },
    { Transcendental::Exponential, 1, Rational::Int { 1 } << 64, false, "1000000000000000000054210108624275221701", 39 },
    { Transcendental::Exponential, 1, largestInt, false, "1000000000000000000000000000000000000005", 39 },
    { Transcendental::Exponential, smallestInt, largestInt, false, "3678794411714423215955237701614608674436", 40 },
    { Transcendental::Exponential, 45, 1, false, "3493427105748509534803479723340609953341", 20 },
    { Transcendental::Exponential, 877, 10, false, "1223562231638072508562388385422483006583", 1 },
} };
// clang-format on

/// The reference's two ends for @p row.
[[nodiscard]] constexpr std::array<Ratio, 2> reference_bounds(Reference const& row)
{
    Word const below = word_of(row.digits);
    Word const above = *detail::add_small_checked_or_none(below, 1U);
    Word const tenToScale = *detail::pow10<detail::KernelLimbs>(row.scale);
    if (row.negative)
        return { Ratio { true, above, tenToScale }, Ratio { true, below, tenToScale } };
    return { Ratio { false, below, tenToScale }, Ratio { false, above, tenToScale } };
}
} // namespace

TEST_CASE("transcendental kernel: the stored ln 2 and log10(e) are the published values", "[transcendental]")
{
    STATIC_REQUIRE(pinned_by_published_digits(detail::Ln2Lower, 128, "6931471805599453094172321214581765680755"));
    STATIC_REQUIRE(pinned_by_published_digits(detail::Log10eLower, 128, "4342944819032518276511289189166050822943"));
    STATIC_REQUIRE(detail::Ln2Upper == *detail::add_small_checked_or_none(detail::Ln2Lower, 1U));
    STATIC_REQUIRE(detail::Log10eUpper == *detail::add_small_checked_or_none(detail::Log10eLower, 1U));
    STATIC_REQUIRE(pinned_by_published_digits(detail::Ln2Lower192, 192,
                                              "693147180559945309417232121458176568075500134360255254120680"));
    STATIC_REQUIRE(detail::Ln2Upper192 == *detail::add_small_checked_or_none(detail::Ln2Lower192, 1U));
    // The exponential's ln 2 begins with the logarithms': floor(L192 / 2^64) = L128.
    STATIC_REQUIRE(detail::shift_right(detail::Ln2Lower192, 64) == detail::Ln2Lower);
}

TEST_CASE("transcendental kernel: the kernel re-derives its stored constants from its own series", "[transcendental]")
{
    // At run time: the kernel's series, like every check that runs it but one (see the last case).
    // ln 2 = 2 atanh(1/3), since (1 + 1/3) / (1 - 1/3) = 2: the series' enclosure of it meets the stored one.
    std::optional<Word> const atanhThird = detail::atanh_series_lower(
        detail::scaled_quotient<detail::KernelFractionBits>(detail::UInt128::from_u64(1), detail::UInt128::from_u64(3))
            .below);
    REQUIRE(atanhThird.has_value());
    Word const ln2Lower = *detail::add_checked_or_none(*atanhThird, *atanhThird);
    Word const ln2Upper = *detail::add_checked_or_none(ln2Lower, Word::from_u64(2 * detail::AtanhSlack));
    CHECK(ln2Lower <= detail::Ln2Upper);
    CHECK(detail::Ln2Lower <= ln2Upper);
    // log10(e) = 1 / ln 10, and ln 10 = 3 ln 2 + 2 atanh(1/9), since (1 + 1/9) / (1 - 1/9) = 10/8. The
    // stored M meets [2^256 / upper, 2^256 / lower] over the whole enclosure of ln 10 * 2^128.
    std::optional<Word> const atanhNinth = detail::atanh_series_lower(
        detail::scaled_quotient<detail::KernelFractionBits>(detail::UInt128::from_u64(1), detail::UInt128::from_u64(9))
            .below);
    REQUIRE(atanhNinth.has_value());
    Word const ln10Lower = *detail::add_checked_or_none(*detail::mul_small_checked_or_none(detail::Ln2Lower, 3U),
                                                        *detail::add_checked_or_none(*atanhNinth, *atanhNinth));
    Word const ln10Upper = *detail::add_checked_or_none(
        *detail::mul_small_checked_or_none(detail::Ln2Upper, 3U),
        *detail::mul_small_checked_or_none(*detail::add_small_checked_or_none(*atanhNinth, detail::AtanhSlack), 2U));
    Word const twoTo256 = *detail::shift_left_checked_or_none(Word::from_u64(1), 256);
    CHECK(*detail::mul_checked_or_none(detail::Log10eLower, ln10Lower) <= twoTo256);
    CHECK(twoTo256 <= *detail::mul_checked_or_none(detail::Log10eUpper, ln10Upper));
}

TEST_CASE("transcendental kernel: every reference value is enclosed and rounds as the reference does in every mode",
          "[transcendental]")
{
    constexpr std::array<int, 9> placesTried { -2, -1, 0, 1, 2, 4, 9, 17, 18 };
    std::size_t compared = 0;
    std::size_t undecided = 0;
    for (std::size_t rowAt = 0; rowAt < references.size(); ++rowAt)
    {
        Reference const& row = references[rowAt];
        INFO("row " << rowAt);
        std::optional<detail::Enclosure> const enclosure =
            enclosure_of(row.function, Rational { row.numerator, row.denominator });
        REQUIRE(enclosure.has_value());
        std::array<Ratio, 2> const referenceEnds = reference_bounds(row);
        // The value lies in both intervals, so they meet.
        CHECK(at_most(enclosure->lower, referenceEnds[1]));
        CHECK(at_most(referenceEnds[0], enclosure->upper));
        for (int const places: placesTried)
            for (RoundingMode const roundingMode: everyMode)
            {
                INFO("places " << places << ", mode " << formula::describe(roundingMode));
                std::expected<Rational, formula::ArithmeticError> const decided =
                    detail::decide_rounding(enclosure->lower, enclosure->upper, DecimalPlaces { places }, roundingMode);
                std::expected<Rational, formula::ArithmeticError> const referenceDecided =
                    detail::decide_rounding(referenceEnds[0], referenceEnds[1], DecimalPlaces { places }, roundingMode);
                // Where the kernel's enclosure is too wide to place the value on one side of a boundary
                // the tighter reference can, it says Overflow, never a guess.
                if (!decided.has_value() && referenceDecided.has_value())
                {
                    CHECK(decided.error() == formula::ArithmeticError::Overflow);
                    ++undecided;
                }
                else
                    CHECK(decided == referenceDecided);
                ++compared;
            }
    }
    // 51 rows, 9 places, 7 modes: a loop over nothing fails here.
    REQUIRE(compared == 3213);
    // None: an exponential's 192 fraction bits and a logarithm's 128 decide every row the reference's 40
    // digits decide. At 128 fraction bits the exponentials of 43 to 44 at 17 and 18 places were not.
    CHECK(undecided == 0);
    // Three of them written out, so that a reader sees the digits.
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, Rational { 2 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(693'147'180'559'945'309, -18));
    CHECK(kernel_rounding(Transcendental::DecimalLogarithm, Rational { 2 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(301'029'995'663'981'195, -18));
    CHECK(kernel_rounding(Transcendental::Exponential, Rational { 1 }, 18, RoundingMode::Floor)
          == Rational::from_decimal(2'718'281'828'459'045'235, -18));
}

TEST_CASE("transcendental kernel: an enclosure is at most 2^-120 wide for a logarithm and 2^-183 of the value for an "
          "exponential",
          "[transcendental]")
{
    // Absolute for the logarithms, whose ends share the denominator 2^128: at most 2^8 units apart (2^-120).
    // Relative for the exponential: upper - lower at most lower / 2^183, since its lower numerator is at least 2^192.
    Word const unit = *detail::shift_left_checked_or_none(Word::from_u64(1), 128);
    for (Reference const& row: references)
    {
        std::optional<detail::Enclosure> const enclosure =
            enclosure_of(row.function, Rational { row.numerator, row.denominator });
        REQUIRE(enclosure.has_value());
        Ratio const& nearer = enclosure->lower.negative ? enclosure->upper : enclosure->lower;
        Ratio const& farther = enclosure->lower.negative ? enclosure->lower : enclosure->upper;
        REQUIRE(nearer.denominator == farther.denominator);
        Word const width = *detail::sub_checked_or_none(farther.numerator, nearer.numerator);
        if (row.function == Transcendental::Exponential)
            CHECK(*detail::shift_left_checked_or_none(width, 183) <= nearer.numerator);
        else
        {
            CHECK(nearer.denominator == unit);
            CHECK(width <= Word::from_u64(256));
        }
    }
}

TEST_CASE("transcendental kernel: an enclosure that straddles a tie is Overflow and never a guess", "[transcendental]")
{
    // ln(1 + 1/(2 * 10^18)) = 5 * 10^-19 - 1.25 * 10^-37 + ...: 1.25 * 10^-37 below the tie between 0 and
    // 10^-18 at 18 places, nearer than the enclosure is wide (128 units of 2^-128, about 3.8 * 10^-37).
    // The half modes cannot be decided and say Overflow; the directed modes can, and answer. The true
    // rounding in the half modes is 0: Overflow here means more bits were needed, never a wrong number.
    constexpr Rational nearTie { 2'000'000'000'000'000'001, 2'000'000'000'000'000'000 };
    for (RoundingMode const roundingMode:
         { RoundingMode::HalfAwayFromZero, RoundingMode::HalfTowardZero, RoundingMode::HalfEven })
        CHECK(
            kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, roundingMode)
            == std::expected<Rational, formula::ArithmeticError> { std::unexpected { formula::ArithmeticError::Overflow } });
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::Floor) == Rational {});
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::TowardZero) == Rational {});
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::Ceiling)
          == Rational::from_decimal(1, -18));
    CHECK(kernel_rounding(Transcendental::NaturalLogarithm, nearTie, 18, RoundingMode::AwayFromZero)
          == Rational::from_decimal(1, -18));
}

TEST_CASE("transcendental kernel: a scaled quotient of 128-bit operands carries the bit its remainder shifts out",
          "[transcendental]")
{
    // The divisor 2^128 - 1 leaves a remainder above 2^127, whose doubling passes 2^128: the shifted-out bit
    // must still count. Checked against the general long division of the 384-bit word, a different route.
    detail::UInt128 const divisor { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } };
    detail::UInt128 const dividend { std::uint64_t { 1 } << 63, 5 };
    for (std::size_t const fractionBits: { std::size_t { 128 }, std::size_t { 192 } })
    {
        INFO("fraction bits " << fractionBits);
        detail::ScaledQuotient const scaled = fractionBits == 128 ? detail::scaled_quotient<128>(dividend, divisor)
                                                                  : detail::scaled_quotient<192>(dividend, divisor);
        detail::WideDivision<detail::KernelLimbs> const reference = detail::divmod(
            *detail::shift_left_checked_or_none(Word::from_u128(dividend), fractionBits), Word::from_u128(divisor));
        CHECK(scaled.below == reference.quotient);
        CHECK(scaled.exact == reference.remainder.is_zero());
        CHECK_FALSE(scaled.exact);
    }
    // An exact one: 3 * 2^100 / 2^100 is 3, to the last fraction bit.
    detail::UInt128 const twoTo100 { std::uint64_t { 1 } << 36, 0 };
    detail::ScaledQuotient const three = detail::scaled_quotient<192>(
        detail::UInt128 { std::uint64_t { 3 } << 36, 0 }, twoTo100);
    CHECK(three.exact);
    CHECK(three.below == *detail::shift_left_checked_or_none(Word::from_u64(3), 192));
}

TEST_CASE("transcendental kernel: the kernel answers at compile time", "[transcendental]")
{
    // The one deliberate compile-time check of the kernel. A whole rounding -- the enclosure and
    // decide_rounding -- in one constant evaluation, measured at about
    // 242 300 steps on cl 19.51.36257, against a default budget of about 1 049 000. Every other check that runs the kernel
    // runs at run time.
    STATIC_REQUIRE(kernel_rounding(Transcendental::DecimalLogarithm, Rational { 2 }, 3, RoundingMode::HalfEven)
                   == Rational { 301, 1000 });
}
