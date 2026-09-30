// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

/// A determination, in grams.
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
/// How many determinations: a bare number.
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};
/// A relative tolerance a jurisdiction may tighten.
struct Tolerance: formula::Quantity<Tolerance, "t", "relative tolerance", unit::One>
{
};
/// A temperature reading, on the Celsius scale's offset.
struct Reading: formula::Quantity<Reading, "T", "temperature reading", unit::Celsius>
{
};
/// A thousandth of a kilogram the author gave no symbol.
inline constexpr formula::Unit UnnamedGram { .dimension = formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000,
                                             .decimals = 1 };
/// A determination in that unit: a value shown in it could not say what
/// scale it is on.
struct UnnamedMass: formula::Quantity<UnnamedMass, "m_u", "mass in an unnamed unit", UnnamedGram>
{
};
/// A determination in kilograms, for sums that leave 64 bits.
struct Heavy: formula::Quantity<Heavy, "m_h", "heavy mass", unit::Kilogram>
{
};
/// An absolute limit an author may leave unmeasured.
struct Band: formula::Quantity<Band, "b", "absolute band", unit::Gram>
{
};
/// A gram squared, invented here: the library has no squared mass unit.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
/// A variance of the determinations.
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};

[[nodiscard]] constexpr formula::Measured<Mass> grams(Rational value)
{
    return formula::Measured<Mass> { value };
}

template <typename... Values>
[[nodiscard]] constexpr auto sampleOf(Values... values)
{
    return formula::environment(formula::measured_series<Mass>(grams(values)...));
}

// Two invented samples, A and B; every number below was checked with
// Python's fractions against a mirror of the loop.
// A, "re-running matters": 40.2, 39.8, 40.5, 44.0, 40.0, 43.3 g.
inline constexpr auto fixtureA = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10));
// B, "policy matters": 40.2, 39.8, 40.5, 45.2, 40.0, 37.2 g.
inline constexpr auto fixtureB = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(452, 10), rat(40), rat(372, 10));
// C, "tie": 40, 40, 44, 40, 36 g, and the same reversed.
inline constexpr auto fixtureC = sampleOf(rat(40), rat(40), rat(44), rat(40), rat(36));
inline constexpr auto fixtureCReversed = sampleOf(rat(36), rat(40), rat(44), rat(40), rat(40));
// D, "on the limit": 40, 40, 40, 40, 42.5 g.
inline constexpr auto fixtureD = sampleOf(rat(40), rat(40), rat(40), rat(40), rat(425, 10));
// E, "all equal": 40 g five times.
inline constexpr auto fixtureE = sampleOf(rat(40), rat(40), rat(40), rat(40), rat(40));

inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation exampleCited { .title = "Example Standard", .section = "7.4" };
/// Where a jurisdiction states its tighter tolerance.
inline constexpr formula::Citation jurisdictionCited { .reference = "Example Standard 1:2020 NA", .section = "NA.2" };

/// A 6 % relative deviation from the pass's mean.
inline constexpr auto sixPercent = formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>);
/// 7/4 sample standard deviations.
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(rat(7, 4)));

template <formula::PerPass P, formula::OnLimit L, std::size_t K, std::size_t M, std::size_t N, typename Criterion>
[[nodiscard]] constexpr auto rejectionOf(Criterion criterion)
{
    return formula::without_outliers<P, L, formula::AtMost<K>, formula::KeepAtLeast<M>>(
        formula::series<Mass, N>, criterion, repeatTest, exampleCited);
}

constexpr auto MostExtreme = formula::PerPass::MostExtreme;
constexpr auto EveryExceeding = formula::PerPass::EveryExceeding;
constexpr auto Keep = formula::OnLimit::Keep;
constexpr auto Reject = formula::OnLimit::Reject;

inline constexpr auto rejectionA = rejectionOf<MostExtreme, Keep, 2, 4, 6>(sixPercent);

// Invented critical-value tables; the expected values below were computed
// with them as they stand. Sizes 3, 4, 5, 6 and 8 -- no 7, so a
// seven-element pass misses.
inline constexpr formula::SampleSizeTable<5> Sizes { 3, 4, 5, 6, 8 };
/// The deviation table, read at each pass's n and scaled by 1/10: limits 9,
/// 1, 2, 3/2 and 6 standard deviations.
inline constexpr auto deviationTable = formula::deviation_in_stddevs(
    formula::critical_value<Sizes, unit::One>(formula::pass_count, { rat(90), rat(10), rat(20), rat(15), rat(60) })
    * rat(1, 10));
/// The gap table, read at each pass's n and scaled by 1/100: limits 9, 7,
/// 3/10, 9/20 and 1/20 -- the first two beyond any gap ratio.
inline constexpr auto gapTable = formula::gap_to_range(
    formula::critical_value<Sizes, unit::One>(formula::pass_count, { rat(900), rat(700), rat(30), rat(45), rat(5) })
    * rat(1, 100));
inline constexpr auto rejectionA1 = rejectionOf<MostExtreme, Keep, 1, 4, 6>(sixPercent);
} // namespace

TEST_CASE("rejection re-runs the aggregate, and a value inside at first can be rejected later (fixture A)", "[rejection]")
{
    // Pass 1 (mean 41.3 g, limit 1239/500 g = 2.478 g) rejects element 3
    // (deviation 2.7 g); element 5 is inside (2.0 g). Pass 2 (mean 40.76 g,
    // limit 2.4456 g) rejects element 5 (2.54 g). Pass 3 (mean 321/8 g)
    // settles. A single-pass rejection settles at 40.76 g with element 3
    // alone removed. (A limit evaluated once, at the pass-1 mean, is 2.478 g
    // and still rejects element 5 here: fixture A' below is the one that
    // tells the two apart.)
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA);
    STATIC_REQUIRE(out->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(out->outcome().source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(out->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(out->passes() == 3);
    STATIC_REQUIRE(out->survivors().size() == 4);
    STATIC_REQUIRE(out->survivors()[0] == 0);
    STATIC_REQUIRE(out->survivors()[3] == 4);
    // The survivors feed every statistic: their mean is the outcome's, their
    // count 4.
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), fixtureA)->measurement().value()
                   == rat(321, 8));
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(rejectionA), fixtureA)->measurement().value()
        == rat(4));
}

TEST_CASE("the limit is evaluated again at each pass's mean (fixture A')", "[rejection]")
{
    // Fixture A with 43.2 g for 43.3 g. Pass 1 (mean 2477/60 g, limit
    // 2.477 g) rejects element 3 (2.7167 g). Pass 2's mean is 40.74 g and
    // element 5 deviates 2.46 g: past pass 2's own limit, 2.4444 g, so it
    // goes and pass 3 settles at 321/8 g. A limit evaluated once, at the
    // pass-1 mean, keeps it (2.46 g < 2.477 g) and settles at 2037/50 g.
    constexpr auto fixtureAPrime = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(432, 10));
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureAPrime);
    STATIC_REQUIRE(out->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[1] == formula::RejectedElement { 5, 2 });
}

TEST_CASE("the bound turns one rejection too many into the author's verdict, never a value", "[rejection]")
{
    // Fixture A under AtMost<1>: pass 2 would reject element 5 as the second
    // rejection. Aborted: the verdict, element 3 rejected, and anything
    // reduced from the rejection a domain error.
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA);
    STATIC_REQUIRE(out->outcome().is_verdict());
    STATIC_REQUIRE(out->outcome().verdict_label() == repeatTest.label);
    STATIC_REQUIRE(out->rejected().size() == 1);
    STATIC_REQUIRE(out->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(out->passes() == 2);
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA1), fixtureA).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("most-extreme and every-exceeding differ, and so does the bound (fixture B)", "[rejection]")
{
    // Pass 1: mean 2429/60 g, limit 2.429 g; elements 3 (4.7167 g) and 5
    // (3.2833 g) both exceed.
    // MostExtreme, AtMost<1>: rejects 3; pass 2 (mean 39.54 g, limit
    // 2.3724 g) finds element 5 inside (2.34 g) and settles at 1977/50 g.
    constexpr auto most =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(most->outcome().measurement().value() == rat(1977, 50));
    STATIC_REQUIRE(most->rejected().size() == 1);
    STATIC_REQUIRE(most->rejected()[0] == formula::RejectedElement { 3, 1 });
    // EveryExceeding, AtMost<1>: two in pass 1 is one too many -- aborted,
    // nothing rejected.
    constexpr auto everyOne =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<EveryExceeding, Keep, 1, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(everyOne->outcome().is_verdict());
    STATIC_REQUIRE(everyOne->rejected().empty());
    STATIC_REQUIRE(everyOne->passes() == 1);
    // EveryExceeding, AtMost<2>: both in pass 1, settling at 321/8 g.
    constexpr auto everyTwo =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<EveryExceeding, Keep, 2, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(everyTwo->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(everyTwo->rejected().size() == 2);
    STATIC_REQUIRE(everyTwo->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(everyTwo->rejected()[1] == formula::RejectedElement { 5, 1 });
    STATIC_REQUIRE(everyTwo->passes() == 2);
}

TEST_CASE("deviation in standard deviations decides exactly, by squares (fixture B)", "[rejection]")
{
    // deviation_in_stddevs(7/4), decided as (x - mean)^2 against
    // (7/4)^2 * s^2 = 49/16 * s^2, the candidate inside s^2 (n - 1).
    //  - pass 1: element 3's z^2 = 80089/24342 = 3.290 > 3.0625: rejected.
    //  - pass 2 (mean 1977/50, s^2 = 889/500): element 5's z^2 =
    //    13689/4445 = 3.0796 > 3.0625, by a hair: rejected.
    //  - pass 3 (n = 4, s^2 = 107/1200): max z^2 = 675/428 = 1.577: settles.
    // AtMost<2> gives 321/8 g, {3, 5}; AtMost<1> aborts in pass 2 on 5.
    // The statistic compared unsquared (z = 1.7549 against 49/16) finds
    // element 5 inside in pass 2 and settles at 1977/50 g. (A population
    // variance, or one without the candidate, raises z^2 -- 3.85 and more
    // -- and does not flip this decision; the brief's claim that it would
    // is not so.)
    constexpr auto two =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 4, 6>(sevenQuarters), fixtureB);
    STATIC_REQUIRE(two->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(two->rejected().size() == 2);
    STATIC_REQUIRE(two->rejected()[1] == formula::RejectedElement { 5, 2 });
    constexpr auto one =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 4, 6>(sevenQuarters), fixtureB);
    STATIC_REQUIRE(one->outcome().is_verdict());
    STATIC_REQUIRE(one->passes() == 2);
}

TEST_CASE("two equally extreme values are rejected together, whatever order they were entered in (fixture C)", "[rejection]")
{
    // Mean 40 g, limit 2.4 g: 44 g and 36 g are both 4 g away. Both go in
    // pass 1 and it settles at 40 g -- in either order of entry, the same
    // values at mirrored positions. AtMost<1> aborts on both.
    constexpr auto both = rejectionOf<MostExtreme, Keep, 2, 3, 5>(sixPercent);
    constexpr auto forward = formula::checked_evaluate_rejection<Mass>(both, fixtureC);
    constexpr auto reversed = formula::checked_evaluate_rejection<Mass>(both, fixtureCReversed);
    STATIC_REQUIRE(forward->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(reversed->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(forward->rejected().size() == 2);
    STATIC_REQUIRE(forward->rejected()[0] == formula::RejectedElement { 2, 1 });
    STATIC_REQUIRE(forward->rejected()[1] == formula::RejectedElement { 4, 1 });
    STATIC_REQUIRE(reversed->rejected()[0] == formula::RejectedElement { 0, 1 });
    STATIC_REQUIRE(reversed->rejected()[1] == formula::RejectedElement { 2, 1 });

    constexpr auto oneOnly = rejectionOf<MostExtreme, Keep, 1, 3, 5>(sixPercent);
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(oneOnly, fixtureC)->outcome().is_verdict());
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(oneOnly, fixtureCReversed)->outcome().is_verdict());
}

TEST_CASE("an element exactly on the limit is kept or rejected as declared (fixture D)", "[rejection]")
{
    // Mean 40.5 g; element 4 is exactly 2 g away, on a 2 g limit.
    constexpr auto twoGrams = formula::deviation_from_mean(formula::constant<unit::Gram>(rat(2)));
    constexpr auto kept =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(twoGrams), fixtureD);
    STATIC_REQUIRE(kept->outcome().measurement().value() == rat(81, 2));
    STATIC_REQUIRE(kept->rejected().empty());
    constexpr auto rejected =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Reject, 2, 3, 5>(twoGrams), fixtureD);
    STATIC_REQUIRE(rejected->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(rejected->rejected().size() == 1);
    STATIC_REQUIRE(rejected->rejected()[0] == formula::RejectedElement { 4, 1 });
}

TEST_CASE("a sample of equal values has no outlier and settles in one pass (fixture E)", "[rejection]")
{
    // Never a division by zero, and never every element "on" a zero limit:
    // under deviation_in_stddevs with OnLimit::Reject, 0 >= 0 would reject
    // every determination.
    constexpr auto relative =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(sixPercent), fixtureE);
    STATIC_REQUIRE(relative->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(relative->passes() == 1);
    STATIC_REQUIRE(relative->rejected().empty());
    constexpr auto inStddevs =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Reject, 2, 3, 5>(sevenQuarters), fixtureE);
    STATIC_REQUIRE(inStddevs->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(inStddevs->rejected().empty());
}

TEST_CASE("a rejection that would leave fewer than KeepAtLeast aborts", "[rejection]")
{
    // Fixture A with KeepAtLeast<5>: pass 2 would leave 4 of at least 5.
    constexpr auto out =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 5, 6>(sixPercent), fixtureA);
    STATIC_REQUIRE(out->outcome().is_verdict());
    STATIC_REQUIRE(out->rejected().size() == 1);
    STATIC_REQUIRE(out->passes() == 2);
}

TEST_CASE("an absent determination runs no pass and yields an empty outcome", "[rejection]")
{
    constexpr auto missing = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                 grams(rat(398, 10)),
                                                                                 formula::Measured<Mass>::absent(),
                                                                                 grams(rat(44)),
                                                                                 grams(rat(40)),
                                                                                 grams(rat(433, 10))));
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, missing);
    STATIC_REQUIRE(out->outcome().is_empty());
    STATIC_REQUIRE(out->passes() == 0);
    STATIC_REQUIRE(out->rejected().empty());
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), missing)->is_empty());

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), missing, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; (not measured); 44 g; 40 g; 433/10 g\n"
             "2. sample_mean(#1) = (not measured)\n");
}

TEST_CASE("pass n reads the current pass's size", "[rejection]")
{
    // A limit of 7/24 * pass n standard deviations: 7/4 at n = 6, and the
    // trace names each pass's n as it was read.
    constexpr auto scaled = rejectionOf<MostExtreme, Keep, 1, 4, 6>(
        formula::deviation_in_stddevs(formula::number(rat(7, 24)) * formula::pass_count));
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(scaled, fixtureB, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 7/24\n"
             "3. pass n = 6\n"
             "4. #2 * #3 = 7/4\n"
             "5. pass 1: 6 values, mean 2429/60 g\n"
             "6. rejected element 4 of 6 (226/5 g) in pass 1: (x - mean)^2 = 80089/3600 g2 > limit^2 * s^2 = 198793/9600 g2 "
             "(deviation in standard deviations)\n"
             "7. 7/24\n"
             "8. pass n = 5\n"
             "9. #7 * #8 = 35/24\n"
             "10. pass 2: 5 values, mean 1977/50 g\n"
             "11. element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");
}

TEST_CASE("a deviation from the mean of Celsius readings reads in the coherent unit, not as a reading",
          "[rejection][trace-render]")
{
    // 23.7, 24.1, 29.3, 23.9 and 24.3 degC, against 3 K either side of the
    // mean. The determinations and the means are points on the Celsius scale;
    // a deviation is a difference, 4.24 K, which shown in degrees Celsius
    // would read -268.91 degC, off by the offset.
    constexpr auto readings = formula::environment(formula::measured_series<Reading>(
        formula::Measured<Reading> { rat(237, 10) },
        formula::Measured<Reading> { rat(241, 10) },
        formula::Measured<Reading> { rat(293, 10) },
        formula::Measured<Reading> { rat(239, 10) },
        formula::Measured<Reading> { rat(243, 10) }));
    constexpr auto threeKelvin = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
        formula::series<Reading, 5>,
        formula::deviation_from_mean(formula::constant<unit::Kelvin>(rat(3))),
        repeatTest,
        exampleCited);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Reading>(threeKelvin, readings, formula::RecordingSink<> { trace });
    std::string const degreesCelsius = "\xc2\xb0" "C";
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. T = 237/10 " + degreesCelsius + "; 241/10 " + degreesCelsius + "; 293/10 " + degreesCelsius + "; 239/10 "
                 + degreesCelsius + "; 243/10 " + degreesCelsius + "\n"
                 + "2. 3 K\n"
                   "3. pass 1: 5 values, mean 1253/50 " + degreesCelsius + "\n"
                 + "4. rejected element 3 of 5 (293/10 " + degreesCelsius
                 + ") in pass 1: abs(x - mean) = 106/25 > 3 (deviation from mean)\n"
                   "5. 3 K\n"
                   "6. pass 2: 4 values, mean 24 " + degreesCelsius + "\n"
                 + "7. settled: 1 rejected, 4 remain\n");

    // Squared, as deviation_in_stddevs compares them: 17.9776 K2 against
    // (7/4)^2 * 5.668 K2, in the coherent unit's square as the deviation is.
    formula::Trace<> squared {};
    (void) formula::checked_evaluate_rejection<Reading>(
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::series<Reading, 5>, sevenQuarters, repeatTest, exampleCited),
        readings,
        formula::RecordingSink<> { squared });
    CHECK(formula::render_trace(squared, { .maxSteps = 40 }).find(
              "4. rejected element 3 of 5 (293/10 " + degreesCelsius
              + ") in pass 1: (x - mean)^2 = 11236/625 > limit^2 * s^2 = 69433/4000 (deviation in standard deviations)\n")
          != std::string::npos);
}

TEST_CASE("a rejection over a unit with no symbol reads its means and deviations in the coherent unit",
          "[rejection][trace-render]")
{
    // 40.2, 39.8, 40.5 and 44 thousandths of a kilogram, against 2 g either
    // side of the mean. A mean, a rejected value and a deviation borrow no
    // unit that could not say what scale it is on: each reads in kilograms.
    constexpr auto unnamed = formula::environment(formula::measured_series<UnnamedMass>(
        formula::Measured<UnnamedMass> { rat(402, 10) },
        formula::Measured<UnnamedMass> { rat(398, 10) },
        formula::Measured<UnnamedMass> { rat(405, 10) },
        formula::Measured<UnnamedMass> { rat(44) }));
    constexpr auto twoGrams = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
        formula::series<UnnamedMass, 4>,
        formula::deviation_from_mean(formula::constant<unit::Gram>(rat(2))),
        repeatTest,
        exampleCited);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<UnnamedMass>(twoGrams, unnamed, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. m_u = 201/5; 199/5; 81/2; 44\n"
             "2. 2 g\n"
             "3. pass 1: 4 values, mean 329/8000\n"
             "4. rejected element 4 of 4 (11/250) in pass 1: abs(x - mean) = 23/8000 > 1/500 (deviation from mean)\n"
             "5. 2 g\n"
             "6. pass 2: 3 values, mean 241/6000\n"
             "7. settled: 1 rejected, 3 remain\n");
}

TEST_CASE("explain_rejection: the rejection's outcome and the trace a RecordingSink records", "[rejection][trace]")
{
    // rejectionA settles after two rejections; rejectionA1 is stopped by its
    // limit after one, so the outcome and the steps differ between the two.
    formula::Trace<> handBuilt {};
    auto const direct =
        formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA, formula::RecordingSink<> { handBuilt });
    REQUIRE(direct.has_value());
    auto const explained = formula::explain_rejection<Mass>(rejectionA, fixtureA);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->outcome() == direct->outcome());
    CHECK(explained.outcome->rejected().size() == 2);
    CHECK(explained.outcome->rejected().size() == direct->rejected().size());
    CHECK(explained.outcome->passes() == direct->passes());
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 100 }) == formula::render_trace(handBuilt, { .maxSteps = 100 }));
    CHECK(!explained.trace.empty());

    auto const stopped = formula::explain_rejection<Mass>(rejectionA1, fixtureA);
    REQUIRE(stopped.outcome.has_value());
    CHECK(stopped.outcome->rejected().size() == 1);
    CHECK(formula::render_trace(stopped.trace, { .maxSteps = 100 }) != formula::render_trace(explained.trace, { .maxSteps = 100 }));
}

TEST_CASE("only the library builds a RejectionOutcome", "[rejection]")
{
    using Built = formula::RejectionOutcome<Mass, 6>;
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<Built>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Built, formula::Outcome<Mass>>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Built>);
}

TEST_CASE("every rejection is a step naming the value, the statistic, the limit and the criterion",
          "[rejection][trace-render]")
{
    formula::Trace<> settled {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA, formula::RecordingSink<> { settled });
    CHECK(
        formula::render_trace(settled, { .maxSteps = 40 })
        == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
           "2. 3/50\n"
           "3. pass mean = 413/10 g\n"
           "4. #2 * #3 = 1239/500000\n"
           "5. pass 1: 6 values, mean 413/10 g\n"
           "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
           "7. 3/50\n"
           "8. pass mean = 1019/25 g\n"
           "9. #7 * #8 = 3057/1250000\n"
           "10. pass 2: 5 values, mean 1019/25 g\n"
           "11. rejected element 6 of 6 (433/10 g) in pass 2: abs(x - mean) = 127/50 g > 3057/1250 g (deviation from mean)\n"
           "12. 3/50\n"
           "13. pass mean = 321/8 g\n"
           "14. #12 * #13 = 963/400000\n"
           "15. pass 3: 4 values, mean 321/8 g\n"
           "16. settled: 2 rejected, 4 remain\n");

    formula::Trace<> aborted {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA, formula::RecordingSink<> { aborted });
    CHECK(formula::render_trace(aborted, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. 3/50\n"
             "3. pass mean = 413/10 g\n"
             "4. #2 * #3 = 1239/500000\n"
             "5. pass 1: 6 values, mean 413/10 g\n"
             "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
             "7. 3/50\n"
             "8. pass mean = 1019/25 g\n"
             "9. #7 * #8 = 3057/1250000\n"
             "10. pass 2: 5 values, mean 1019/25 g\n"
             "11. element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");

    // A budget of 4 truncates, and says how many steps it left out.
    CHECK(formula::render_trace(settled, { .maxSteps = 4 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; ... 3 more\n"
             "... 15 further steps not shown\n");

    // Every exceeding one at once, aborted: both named.
    formula::Trace<> both {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<EveryExceeding, Keep, 1, 4, 6>(sixPercent), fixtureB, formula::RecordingSink<> { both });
    CHECK(formula::render_trace(both, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 3/50\n"
             "3. pass mean = 2429/60 g\n"
             "4. #2 * #3 = 2429/1000000\n"
             "5. pass 1: 6 values, mean 2429/60 g\n"
             "6. elements 4 and 6 of 6 would be rejections 1 and 2 of at most 1: discard the determinations and repeat the "
             "test [Example Standard, 7.4]\n");

    // In standard deviations, the exact comparison by squares.
    formula::Trace<> squares {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(sevenQuarters), fixtureB, formula::RecordingSink<> { squares });
    CHECK(formula::render_trace(squares, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 7/4\n"
             "3. pass 1: 6 values, mean 2429/60 g\n"
             "4. rejected element 4 of 6 (226/5 g) in pass 1: (x - mean)^2 = 80089/3600 g2 > limit^2 * s^2 = 198793/9600 g2 "
             "(deviation in standard deviations)\n"
             "5. 7/4\n"
             "6. pass 2: 5 values, mean 1977/50 g\n"
             "7. rejected element 6 of 6 (186/5 g) in pass 2: (x - mean)^2 = 13689/2500 g2 > limit^2 * s^2 = 43561/8000 g2 "
             "(deviation in standard deviations)\n"
             "8. 7/4\n"
             "9. pass 3: 4 values, mean 321/8 g\n"
             "10. settled: 2 rejected, 4 remain\n");

    // Below KeepAtLeast.
    formula::Trace<> kept {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 5, 6>(sixPercent), fixtureA, formula::RecordingSink<> { kept });
    CHECK(formula::render_trace(kept, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. 3/50\n"
             "3. pass mean = 413/10 g\n"
             "4. #2 * #3 = 1239/500000\n"
             "5. pass 1: 6 values, mean 413/10 g\n"
             "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
             "7. 3/50\n"
             "8. pass mean = 1019/25 g\n"
             "9. #7 * #8 = 3057/1250000\n"
             "10. pass 2: 5 values, mean 1019/25 g\n"
             "11. element 6 of 6 would leave 4 of at least 5: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");
}

TEST_CASE("an aborted rejection's verdict and citation are escaped as a step's own author text", "[rejection][trace-render]")
{
    // A verdict and a citation holding the characters a trace line is built
    // from: each is escaped, so neither can close the line's clause early or
    // open a clause of its own.
    constexpr auto bracketed = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
        formula::series<Mass, 6>,
        sixPercent,
        formula::Verdict { "discard; repeat [now]" },
        formula::Citation { .title = "Example Standard [draft]", .section = "7.4" });
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(bracketed, fixtureA, formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(rendered.ends_with("11. element 6 of 6 would be rejection 2 of at most 1: discard\\; repeat \\[now\\] "
                             "[Example Standard \\[draft\\], 7.4]\n"));
}

TEST_CASE("a rejection renders with every parameter stated, and never a bar outside LaTeX", "[rejection][render]")
{
    CHECK(formula::render(rejectionA)
          == "without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on limit; at most "
             "2; keep at least 4)");
    std::string const markdown = formula::render<formula::Dialect::Markdown>(rejectionA);
    CHECK(markdown
          == "without outliers(`m(i)`; abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on limit; at most "
             "2; keep at least 4)");
    CHECK(markdown.find('|') == std::string::npos);
    CHECK(markdown.find('[') == std::string::npos);
    CHECK(formula::render<formula::Dialect::LaTeX>(rejectionA)
          == "\\operatorname{without\\ outliers}({m}_{i};\\allowbreak \\left\\lvert x - \\bar{x}_{\\text{pass}}\\right\\rvert > "
             "3/50 \\cdot \\bar{x}_{\\text{pass}};\\allowbreak \\text{most extreme per pass};\\allowbreak \\text{keep on "
             "limit};\\allowbreak \\text{at most }2;\\allowbreak \\text{keep at least }4)");
    CHECK(formula::render(rejectionOf<EveryExceeding, Reject, 1, 3, 6>(sevenQuarters))
          == "without outliers(m(i); abs(x - pass mean) / s >= 7/4; every exceeding per pass; reject on limit; at most 1; "
             "keep at least 3)");
    CHECK(formula::render<formula::Dialect::LaTeX>(rejectionOf<EveryExceeding, Reject, 1, 3, 6>(sevenQuarters))
          == "\\operatorname{without\\ outliers}({m}_{i};\\allowbreak \\frac{\\left\\lvert x - \\bar{x}_{\\text{pass}}\\right\\rvert}"
             "{s} \\geq 7/4;\\allowbreak \\text{every exceeding per pass};\\allowbreak \\text{reject on limit};"
             "\\allowbreak \\text{at most }1;\\allowbreak \\text{keep at least }3)");
    // The pass placeholders, as a limit expression reads them.
    CHECK(formula::render(formula::number(rat(7, 24)) * formula::pass_count) == "7/24 * pass n");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::number(rat(7, 24)) * formula::pass_count)
          == "7/24 \\cdot n_{\\text{pass}}");
    CHECK(formula::render(formula::sample_mean(rejectionA))
          == "sample_mean(without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on "
             "limit; at most 2; keep at least 4))");
}

TEST_CASE("a rejection's page states its criterion, limit, bounds, verdict and citation", "[rejection][document]")
{
    formula::Documentation const page = formula::document(formula::sample_mean(rejectionA));
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "m");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    REQUIRE(page.rejections.size() == 1);
    formula::RejectionEntry const& entry = page.rejections[0];
    CHECK(entry.criterion == formula::CriterionKind::DeviationFromMean);
    CHECK(entry.limit == "3/50 * pass mean");
    CHECK(entry.perPass == formula::PerPass::MostExtreme);
    CHECK(entry.onLimit == formula::OnLimit::Keep);
    CHECK(entry.atMost == 2);
    CHECK(entry.keepAtLeast == 4);
    CHECK(entry.verdict == repeatTest);
    CHECK(entry.citation.section == "7.4");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].title == "Example Standard");
}

namespace
{
struct MeanOfSurvivors
{
};

/// A method reporting the mean of fixture-shaped determinations after a
/// rejection at a jurisdiction-set relative tolerance.
inline constexpr auto survivorsMethod = formula::method(
    formula::variants(formula::variant<MeanOfSurvivors>(formula::sample_mean(
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(formula::deviation_from_mean(var<Tolerance> * formula::pass_mean<Mass>))))),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto abortingMethod = formula::method(
    formula::variants(formula::variant<MeanOfSurvivors>(formula::sample_mean(rejectionA1))),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

TEST_CASE("a rejection joins a method: evaluated, overlaid, rendered, documented and traced", "[rejection][method]")
{
    // Fixture B at 6 %: pass 2 finds element 5 inside (2.34 g < 2.3724 g),
    // so 1977/50 g = 39.54 g. At a jurisdiction's tighter 3 %, pass 2's limit
    // is 1.1862 g, element 5 goes too, and pass 3 settles at 321/8 g =
    // 40.125 g.
    constexpr auto inputs = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                grams(rat(398, 10)),
                                                                                grams(rat(405, 10)),
                                                                                grams(rat(452, 10)),
                                                                                grams(rat(40)),
                                                                                grams(rat(372, 10))),
                                                 formula::Measured<Tolerance> { rat(6, 100) });
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSurvivors>(survivorsMethod, inputs) == rat(3954, 100000));
    constexpr auto tighter =
        formula::apply(formula::overlay(formula::with_constant<Tolerance>(rat(3, 100), jurisdictionCited)), survivorsMethod);
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSurvivors>(tighter, inputs) == rat(40125, 1000000));

    constexpr auto jurisdiction = formula::vocabulary(formula::renames<Mass>("x_m"));
    constexpr auto variantExpression = std::get<0>(tighter.variantSet.cases).expression;
    CHECK(formula::render(variantExpression, jurisdiction)
          == "sample_mean(without outliers(x_m(i); abs(x - pass mean) > t * pass mean; most extreme per pass; keep on "
             "limit; at most 2; keep at least 4))");
    formula::Documentation const page = formula::document(variantExpression, jurisdiction);
    CHECK(page.symbols.size() == 2);
    REQUIRE(page.rejections.size() == 1);
    CHECK(page.rejections[0].limit == "t * pass mean");

    formula::Trace<> trace {};
    (void) formula::evaluate_method<MeanOfSurvivors>(tighter, inputs, formula::RecordingSink { trace, jurisdiction });
    CHECK(
        formula::render_trace(trace, { .maxSteps = 60 })
        == "1. x_m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
           "2. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "3. pass mean = 2429/60 g\n"
           "4. #2 * #3 = 2429/2000000\n"
           "5. pass 1: 6 values, mean 2429/60 g\n"
           "6. rejected element 4 of 6 (226/5 g) in pass 1: abs(x - mean) = 283/60 g > 2429/2000 g (deviation from mean)\n"
           "7. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "8. pass mean = 1977/50 g\n"
           "9. #7 * #8 = 5931/5000000\n"
           "10. pass 2: 5 values, mean 1977/50 g\n"
           "11. rejected element 6 of 6 (186/5 g) in pass 2: abs(x - mean) = 117/50 g > 5931/5000 g (deviation from mean)\n"
           "12. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "13. pass mean = 321/8 g\n"
           "14. #12 * #13 = 963/800000\n"
           "15. pass 3: 4 values, mean 321/8 g\n"
           "16. settled: 2 rejected, 4 remain\n"
           "17. sample_mean(#16) = 321/8 g\n"
           "18. round(#17, in g) = 321/8 g [rounded to 3 dp (method default); nearest, ties away from zero]\n"
           "19. #18 = 321/8 g [variant MeanOfSurvivors (1st of 1), selected by tag]\n");

    // An aborted rejection inside a method: DomainError, and the trace names
    // the verdict.
    STATIC_REQUIRE(formula::evaluate_method<MeanOfSurvivors>(abortingMethod, fixtureA).error()
                   == formula::ArithmeticError::DomainError);
    formula::Trace<> abortedTrace {};
    (void) formula::evaluate_method<MeanOfSurvivors>(abortingMethod, fixtureA, formula::RecordingSink<> { abortedTrace });
    CHECK(formula::render_trace(abortedTrace, { .maxSteps = 60 })
              .find("would be rejection 2 of at most 1: discard the "
                    "determinations and repeat the test")
          != std::string::npos);
}

TEST_CASE("a negative limit is no rule, under either criterion", "[rejection]")
{
    // abs(x - mean) / s > -7/4 would make every determination an outlier,
    // and squaring the limit would decide it as +7/4 -- fixture B would
    // settle at 321/8 g, rejecting {3, 5}, exactly as +7/4 does. Both
    // criteria refuse it, in the first pass, as DomainError.
    constexpr auto negativeStddevs =
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(formula::deviation_in_stddevs(formula::number(rat(-7, 4))));
    constexpr auto inStddevs = formula::checked_evaluate_rejection<Mass>(negativeStddevs, fixtureB);
    STATIC_REQUIRE(!inStddevs.has_value());
    STATIC_REQUIRE(inStddevs.error().error == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(!inStddevs.error().element.has_value());
    constexpr auto negativeGrams =
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(formula::deviation_from_mean(formula::constant<unit::Gram>(rat(-1))));
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(negativeGrams, fixtureB).error().error
                   == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(negativeGrams), fixtureB).error()
                   == formula::ArithmeticError::DomainError);
    // A limit of zero is a rule: every determination not at the mean is a
    // candidate.
    constexpr auto zero =
        rejectionOf<MostExtreme, Keep, 2, 3, 5>(formula::deviation_from_mean(formula::constant<unit::Gram>(rat(0))));
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(zero, fixtureD)->rejected().size() == 1);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(negativeStddevs), fixtureB, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. -7/4\n"
             "3. pass 1: 6 values, mean 2429/60 g\n"
             "4. failed in pass 1: the limit is negative, which no deviation can be compared with\n"
             "5. sample_mean(#4) = argument outside the domain of the operation\n");
}

TEST_CASE("a pass that fails says what failed, and the rejection claims its steps", "[rejection][trace-render]")
{
    // The mean: INT64_MAX kg and 1 kg leave 64 bits at element 2.
    constexpr auto heavyMean = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<std::int64_t>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    constexpr auto heavyRejection =
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Heavy, 3>, formula::deviation_from_mean(formula::constant<unit::Kilogram>(rat(1))), repeatTest);
    constexpr auto meanFailed = formula::checked_evaluate_rejection<Heavy>(heavyRejection, heavyMean);
    STATIC_REQUIRE(meanFailed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(*meanFailed.error().element == 1);
    formula::Trace<> meanTrace {};
    (void) formula::checked_evaluate<Heavy>(
        formula::sample_mean(heavyRejection), heavyMean, formula::RecordingSink<> { meanTrace });
    CHECK(formula::render_trace(meanTrace, { .maxSteps = 20 })
          == "1. m_h = 9223372036854775807 kg; 1 kg; 2 kg\n"
             "2. pass 1: 3 values, mean overflow in exact arithmetic\n"
             "3. failed in pass 1: the mean: overflow in exact arithmetic at element 2 of 3\n"
             "4. sample_mean(#3) = overflow in exact arithmetic\n");

    // The variance: the census's six-decimal sample (40.053270 ... 40.131659 g)
    // under 7/4 standard deviations. The mean fits; the squared deviations'
    // total does not, at element 1 -- not the mean, which the pass line
    // shows.
    constexpr auto fine = sampleOf(rat(40053270, 1000000),
                                   rat(39475922, 1000000),
                                   rat(39025798, 1000000),
                                   rat(40615904, 1000000),
                                   rat(39418416, 1000000),
                                   rat(40131659, 1000000));
    constexpr auto fineRejection = rejectionOf<MostExtreme, Keep, 2, 4, 6>(sevenQuarters);
    constexpr auto varianceFailed = formula::checked_evaluate_rejection<Mass>(fineRejection, fine);
    STATIC_REQUIRE(varianceFailed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(varianceFailed.error().element.has_value());
    formula::Trace<> varianceTrace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(fineRejection), fine, formula::RecordingSink<> { varianceTrace });
    CHECK(formula::render_trace(varianceTrace, { .maxSteps = 20 })
          == "1. m = 4005327/100000 g; 19737961/500000 g; 19512899/500000 g; 1269247/31250 g; 2463651/62500 g; "
             "40131659/1000000 g\n"
             "2. pass 1: 6 values, mean 238720969/6000000 g\n"
             "3. failed in pass 1: the variance: overflow in exact arithmetic at element 1 of 6\n"
             "4. sample_mean(#3) = overflow in exact arithmetic\n");

    // The limit: 1 kg / (pass n - 3) is a division by zero in a pass of 3.
    constexpr auto smallHeavy = formula::environment(formula::measured_series<Heavy>(
        formula::Measured<Heavy> { rat(0) }, formula::Measured<Heavy> { rat(1) }, formula::Measured<Heavy> { rat(5) }));
    constexpr auto limitRejection =
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Heavy, 3>,
            formula::deviation_from_mean(formula::constant<unit::Kilogram>(rat(1))
                                         / (formula::pass_count - formula::number(rat(3)))),
            repeatTest);
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Heavy>(limitRejection, smallHeavy).error().error
                   == formula::ArithmeticError::DivisionByZero);
    formula::Trace<> limitTrace {};
    (void) formula::checked_evaluate<Heavy>(
        formula::sample_mean(limitRejection), smallHeavy, formula::RecordingSink<> { limitTrace });
    CHECK(formula::render_trace(limitTrace, { .maxSteps = 20 })
          == "1. m_h = 0 kg; 1 kg; 5 kg\n"
             "2. 1 kg\n"
             "3. pass n = 3\n"
             "4. 3\n"
             "5. #3 - #4 = 0\n"
             "6. #2 / #5 = division by zero\n"
             "7. pass 1: 3 values, mean 2 kg\n"
             "8. failed in pass 1: the limit: division by zero\n"
             "9. sample_mean(#8) = division by zero\n");

    // limit^2 * s^2: 4 * 10^9 standard deviations squared leaves 64 bits.
    constexpr auto thresholdRejection =
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::series<Heavy, 3>, formula::deviation_in_stddevs(formula::number(rat(4'000'000'000))), repeatTest);
    constexpr auto thresholdFailed = formula::checked_evaluate_rejection<Heavy>(thresholdRejection, smallHeavy);
    STATIC_REQUIRE(thresholdFailed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(!thresholdFailed.error().element.has_value());
    formula::Trace<> thresholdTrace {};
    (void) formula::checked_evaluate_rejection<Heavy>(
        thresholdRejection, smallHeavy, formula::RecordingSink<> { thresholdTrace });
    CHECK(formula::render_trace(thresholdTrace, { .maxSteps = 20 })
          == "1. m_h = 0 kg; 1 kg; 5 kg\n"
             "2. 4000000000\n"
             "3. pass 1: 3 values, mean 2 kg\n"
             "4. failed in pass 1: limit^2 * s^2: overflow in exact arithmetic\n");

    // A deviation: 4e18, 4e18 and -4e18 kg have a mean that fits, and
    // 4e18 - 4e18/3 does not, at element 1.
    constexpr auto wide =
        formula::environment(formula::measured_series<Heavy>(formula::Measured<Heavy> { rat(4'000'000'000'000'000'000) },
                                                             formula::Measured<Heavy> { rat(4'000'000'000'000'000'000) },
                                                             formula::Measured<Heavy> { rat(-4'000'000'000'000'000'000) }));
    constexpr auto wideRejection = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
        formula::series<Heavy, 3>,
        formula::deviation_from_mean(formula::constant<unit::Kilogram>(rat(3'000'000'000'000'000'000))),
        repeatTest);
    constexpr auto statisticFailed = formula::checked_evaluate_rejection<Heavy>(wideRejection, wide);
    STATIC_REQUIRE(statisticFailed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(*statisticFailed.error().element == 0);
    formula::Trace<> statisticTrace {};
    (void) formula::checked_evaluate<Heavy>(
        formula::sample_mean(wideRejection), wide, formula::RecordingSink<> { statisticTrace });
    CHECK(formula::render_trace(statisticTrace, { .maxSteps = 20 })
          == "1. m_h = 4000000000000000000 kg; 4000000000000000000 kg; -4000000000000000000 kg\n"
             "2. 3000000000000000000 kg\n"
             "3. pass 1: 3 values, mean 4000000000000000000/3 kg\n"
             "4. failed in pass 1: the deviation: overflow in exact arithmetic at element 1 of 3\n"
             "5. sample_mean(#4) = overflow in exact arithmetic\n");
}

TEST_CASE("an absent limit decides nothing, and the outcome is empty", "[rejection]")
{
    // Strict absence: a limit nobody measured keeps no determination
    // and rejects none -- the outcome is empty, never the unfiltered mean.
    constexpr auto banded = rejectionOf<MostExtreme, Keep, 2, 4, 6>(formula::deviation_from_mean(var<Band>));
    constexpr auto unmeasured = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                    grams(rat(398, 10)),
                                                                                    grams(rat(405, 10)),
                                                                                    grams(rat(44)),
                                                                                    grams(rat(40)),
                                                                                    grams(rat(433, 10))),
                                                     formula::Measured<Band>::absent());
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(banded, unmeasured);
    STATIC_REQUIRE(out->outcome().is_empty());
    STATIC_REQUIRE(out->rejected().empty());
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(banded), unmeasured)->is_empty());
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(formula::sample_mean(banded), unmeasured, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. b = (not measured)\n"
             "3. pass 1: 6 values, mean 413/10 g\n"
             "4. no decision in pass 1: the limit is not measured\n"
             "5. sample_mean(#4) = (not measured)\n");
}

TEST_CASE("an aborted rejection keeps its survivors, and names both bounds when it would pass both", "[rejection]")
{
    // Fixture A under AtMost<1>: element 3 went in pass 1, and the abort in
    // pass 2 leaves the other five.
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA);
    STATIC_REQUIRE(out->survivors().size() == 5);
    STATIC_REQUIRE(out->survivors()[2] == 2);
    STATIC_REQUIRE(out->survivors()[3] == 4);
    STATIC_REQUIRE(out->survivors()[4] == 5);

    // Fixture B, every exceeding one, AtMost<1> and KeepAtLeast<5>: two in
    // pass 1 would be one too many, and would leave 4 of at least 5.
    formula::Trace<> both {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<EveryExceeding, Keep, 1, 5, 6>(sixPercent), fixtureB, formula::RecordingSink<> { both });
    CHECK(formula::render_trace(both, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 3/50\n"
             "3. pass mean = 2429/60 g\n"
             "4. #2 * #3 = 2429/1000000\n"
             "5. pass 1: 6 values, mean 2429/60 g\n"
             "6. elements 4 and 6 of 6 would be rejections 1 and 2 of at most 1 and would leave 4 of at least 5: discard "
             "the determinations and repeat the test [Example Standard, 7.4]\n");
}

TEST_CASE("AtMost of the largest count still runs every pass it needs", "[rejection]")
{
    // k + 1 would wrap to 0 and run no pass at all. With a 1/10 g limit and
    // KeepAtLeast<1>, fixture B rejects one determination a pass until one
    // remains -- exactly as AtMost<5>, the most it could reject, does.
    constexpr auto tight = formula::deviation_from_mean(formula::constant<unit::Gram>(rat(1, 10)));
    constexpr auto largest = formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, std::numeric_limits<std::size_t>::max(), 1, 6>(tight), fixtureB);
    constexpr auto five =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 5, 1, 6>(tight), fixtureB);
    STATIC_REQUIRE(largest->passes() == five->passes());
    STATIC_REQUIRE(largest->passes() > 1);
    STATIC_REQUIRE(largest->rejected().size() == five->rejected().size());
    STATIC_REQUIRE(largest->outcome().measurement().value() == five->outcome().measurement().value());
}

TEST_CASE("a rejection record that contradicts itself is refused, not printed", "[rejection][trace-render]")
{
    // Trace and its records are public aggregates. A hand-built record with
    // a count that would wrap, an abort naming nobody, or a position past
    // the sample prints "(its record is invalid)" -- never a number no
    // evaluation produced.
    formula::Trace<> aborted {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA, formula::RecordingSink<> { aborted });
    REQUIRE(aborted.rejectionRecords.size() >= 2);
    std::size_t const abortRecord = aborted.rejectionRecords.size() - 1;
    std::size_t const outlierRecord = 1; // pass 1, then its rejection
    REQUIRE(aborted.steps[aborted.rejectionRecords[outlierRecord].step].kind == formula::StepKind::OutlierRejected);

    formula::Trace<> wrapped = aborted;
    wrapped.rejectionRecords[abortRecord].remaining = 0;
    wrapped.rejectionRecords[abortRecord].pastAtMost = false;
    wrapped.rejectionRecords[abortRecord].belowKeepAtLeast = true;
    CHECK(formula::render_trace(wrapped, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));

    formula::Trace<> nobody = aborted;
    nobody.rejectionRecords[abortRecord].wouldReject.clear();
    CHECK(formula::render_trace(nobody, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));

    formula::Trace<> pastSample = aborted;
    pastSample.rejectionRecords[abortRecord].wouldReject = { 8 };
    CHECK(formula::render_trace(pastSample, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));

    formula::Trace<> outlierPast = aborted;
    outlierPast.rejectionRecords[outlierRecord].position = 8;
    CHECK(formula::render_trace(outlierPast, { .maxSteps = 20 }).find("rejected element (its record is invalid)\n")
          != std::string::npos);

    // An abort that passes neither bound.
    formula::Trace<> neitherBound = aborted;
    neitherBound.rejectionRecords[abortRecord].pastAtMost = false;
    neitherBound.rejectionRecords[abortRecord].belowKeepAtLeast = false;
    CHECK(formula::render_trace(neitherBound, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));

    // A pass that never ran, or more passes than determinations.
    formula::Trace<> passZero = aborted;
    passZero.rejectionRecords[outlierRecord].pass = 0;
    CHECK(formula::render_trace(passZero, { .maxSteps = 20 }).find("rejected element (its record is invalid)\n")
          != std::string::npos);
    formula::Trace<> passBeyond = aborted;
    passBeyond.rejectionRecords[0].pass = 99;
    CHECK(formula::render_trace(passBeyond, { .maxSteps = 20 }).find("pass (its record is invalid)\n") != std::string::npos);
    formula::Trace<> passLarger = aborted;
    passLarger.rejectionRecords[0].sampleSize = 7;
    CHECK(formula::render_trace(passLarger, { .maxSteps = 20 }).find("pass (its record is invalid)\n") != std::string::npos);
    formula::Trace<> abortCounts = aborted;
    abortCounts.rejectionRecords[abortRecord].rejectedCount = 2;
    CHECK(formula::render_trace(abortCounts, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));

    // A settled rejection whose counts do not add up to the sample, or that
    // ended in pass 0.
    formula::Trace<> settled {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA, formula::RecordingSink<> { settled });
    REQUIRE((!settled.rejectionRecords.empty() && !settled.steps.empty()));
    REQUIRE(settled.steps.back().kind == formula::StepKind::RejectionSettled);
    CHECK(formula::render_trace(settled, { .maxSteps = 40 }).ends_with("settled: 2 rejected, 4 remain\n"));
    formula::Trace<> settledCounts = settled;
    settledCounts.rejectionRecords.back().remaining = 40;
    CHECK(formula::render_trace(settledCounts, { .maxSteps = 40 }).ends_with("rejection settled (its record is invalid)\n"));
    formula::Trace<> settledPassZero = settled;
    settledPassZero.rejectionRecords.back().pass = 0;
    CHECK(
        formula::render_trace(settledPassZero, { .maxSteps = 40 }).ends_with("rejection settled (its record is invalid)\n"));

    // A failed pass whose position is past the sample, or that names an
    // element for the range, which no element owns, or in pass 0.
    formula::Trace<> failed {};
    constexpr auto heavyMean = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<std::int64_t>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    (void) formula::checked_evaluate_rejection<Heavy>(
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Heavy, 3>, formula::deviation_from_mean(formula::constant<unit::Kilogram>(rat(1))), repeatTest),
        heavyMean,
        formula::RecordingSink<> { failed });
    REQUIRE((!failed.rejectionRecords.empty() && !failed.steps.empty()));
    REQUIRE(failed.steps.back().kind == formula::StepKind::RejectionFailed);
    CHECK(formula::render_trace(failed, { .maxSteps = 20 })
              .ends_with("failed in pass 1: the mean: overflow in exact arithmetic at element 2 of 3\n"));
    formula::Trace<> failedPast = failed;
    failedPast.rejectionRecords.back().position = 3;
    CHECK(formula::render_trace(failedPast, { .maxSteps = 20 }).ends_with("rejection failed (its record is invalid)\n"));
    formula::Trace<> rangeAtElement = failed;
    rangeAtElement.rejectionRecords.back().failurePoint = formula::detail::RejectionFailurePoint::Range;
    CHECK(formula::render_trace(rangeAtElement, { .maxSteps = 20 }).ends_with("rejection failed (its record is invalid)\n"));
    formula::Trace<> failedPassZero = failed;
    failedPassZero.rejectionRecords.back().pass = 0;
    CHECK(formula::render_trace(failedPassZero, { .maxSteps = 20 }).ends_with("rejection failed (its record is invalid)\n"));
}

TEST_CASE("a gap_to_range overflow is the range's, at no element; too few for standard deviations says so",
          "[rejection][trace-render]")
{
    // 5e18, 0 and -5e18 kg: the mean fits (0), the range, 1e19, does not.
    // It is the range's failure, and the range belongs to no element.
    constexpr auto wide = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { 5'000'000'000'000'000'000 } },
                                        formula::Measured<Heavy> { rat(0) },
                                        formula::Measured<Heavy> { Rational { -5'000'000'000'000'000'000 } }));
    constexpr auto gapped = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
        formula::series<Heavy, 3>, formula::gap_to_range(formula::number(rat(1, 10))), repeatTest);
    constexpr auto overflowed = formula::checked_evaluate_rejection<Heavy>(gapped, wide);
    STATIC_REQUIRE(overflowed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(!overflowed.error().element.has_value());
    formula::Trace<> rangeTrace {};
    (void) formula::checked_evaluate_rejection<Heavy>(gapped, wide, formula::RecordingSink<> { rangeTrace });
    CHECK(formula::render_trace(rangeTrace, { .maxSteps = 20 })
              .ends_with("failed in pass 1: the range: overflow in exact arithmetic\n"));

    // Fixture B at 1/10 standard deviations, keeping at least three -- the
    // fewest a deviation in standard deviations may keep: every pass
    // rejects until the next would leave two, and that is the author's
    // verdict, not a failure.
    constexpr auto tenth = formula::deviation_in_stddevs(formula::number(rat(1, 10)));
    constexpr auto kept =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 5, 3, 6>(tenth), fixtureB);
    STATIC_REQUIRE(kept.has_value());
    STATIC_REQUIRE(kept->outcome().is_verdict());
}

TEST_CASE("a critical value is read at each pass's own sample size (fixture B, the deviation table)", "[rejection]")
{
    // Current n (correct):
    //  - pass 1, n = 6: limit 3/2, limit^2 9/4; element 3's z^2 = 80089/24342
    //    = 3.290 > 2.25, rejected.
    //  - pass 2, n = 5: limit 2, limit^2 4; element 5's z^2 = 13689/4445 =
    //    3.0796 < 4: settles at 1977/50 g with {3}, under AtMost<1> as
    //    under AtMost<2>.
    // Read at the original n = 6 in every pass, pass 2's limit^2 would stay
    // 9/4 and reject element 5; pass 3 (n = 4, max z^2 675/428 = 1.577)
    // would settle at 321/8 g -- and AtMost<1> would abort in pass 2.
    constexpr auto two =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 6>(deviationTable), fixtureB);
    STATIC_REQUIRE(two->outcome().measurement().value() == rat(1977, 50));
    STATIC_REQUIRE(two->rejected().size() == 1);
    STATIC_REQUIRE(two->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(two->passes() == 2);
    constexpr auto one =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 3, 6>(deviationTable), fixtureB);
    STATIC_REQUIRE(one->outcome().measurement().value() == rat(1977, 50));
}

TEST_CASE("gap to range examines the two extremes, pass by pass (fixture B, the gap table)", "[rejection]")
{
    // Pass 1 (n = 6, limit 9/20): the high gap, 45.2 - 40.5 = 4.7 g over a
    // range of 8 g, is 47/80 = 0.5875 > 9/20: element 3 goes. Pass 2 (n = 5,
    // limit 3/10): the low gap, 39.8 - 37.2 = 2.6 g over 3.3 g, is 26/33 >
    // 3/10: element 5 goes. Pass 3 (n = 4, limit 7): the largest ratio is
    // 3/7 < 7, and it settles at 321/8 g. Under AtMost<1>, pass 2 aborts.
    constexpr auto three =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable), fixtureB);
    STATIC_REQUIRE(three->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(three->rejected().size() == 2);
    STATIC_REQUIRE(three->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(three->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(three->passes() == 3);
    constexpr auto aborted =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 3, 6>(gapTable), fixtureB);
    STATIC_REQUIRE(aborted->outcome().is_verdict());
    STATIC_REQUIRE(aborted->passes() == 2);

    // A tie between the two extremes rejects both: 36 and 44 g around three
    // 40 g are 4/8 each, past a 1/4 limit.
    constexpr auto quarter = formula::gap_to_range(formula::number(rat(1, 4)));
    constexpr auto tied =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(quarter), fixtureC);
    STATIC_REQUIRE(tied->rejected().size() == 2);
    STATIC_REQUIRE(tied->outcome().measurement().value() == rat(40));

    // A duplicated extreme is its own neighbour: 36, 36, 44, 45 and 46 g at
    // 1/4 have a low gap of 0 and a high gap of 1/10, so nothing goes and
    // it settles at 207/5 g in one pass. A gap to the next distinct value
    // (8/10 for 36 g) would reject both 36 g values.
    constexpr auto duplicated = formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 2, 5>(quarter), sampleOf(rat(36), rat(36), rat(44), rat(45), rat(46)));
    STATIC_REQUIRE(duplicated->outcome().measurement().value() == rat(207, 5));
    STATIC_REQUIRE(duplicated->rejected().empty());
    STATIC_REQUIRE(duplicated->passes() == 1);

    // On the limit, kept: fixture C's gaps are 4/8 each, exactly 1/2, so
    // Keep rejects neither and it settles at 40 g in one pass. Treated as
    // Reject, or with the range measured as max - mean (4 g, ratios of 1),
    // both would go.
    constexpr auto half = formula::gap_to_range(formula::number(rat(1, 2)));
    constexpr auto onLimit =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(half), fixtureC);
    STATIC_REQUIRE(onLimit->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(onLimit->rejected().empty());
    STATIC_REQUIRE(onLimit->passes() == 1);

    // Fixture H, "masked pair": 30, 31, 40, 40, 40 and 41 g under the gap
    // table. Each extreme is 1 g from its neighbour, 1/11 of the 11 g range,
    // under 9/20: nothing goes, 37 g in one pass, under AtMost<1> as under
    // AtMost<2>. Measured to the mean (37 g), 30 g's gap would be 7/11 >
    // 9/20, then 31 g's 37/50 > 3/10 at n = 5, settling at 161/4 g -- and
    // AtMost<1> would abort. Two low outliers mask each other, which is what
    // the neighbour gap is for.
    constexpr auto fixtureH = sampleOf(rat(30), rat(31), rat(40), rat(40), rat(40), rat(41));
    constexpr auto masked =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable), fixtureH);
    STATIC_REQUIRE(masked->outcome().measurement().value() == rat(37));
    STATIC_REQUIRE(masked->rejected().empty());
    STATIC_REQUIRE(masked->passes() == 1);
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 3, 6>(gapTable), fixtureH)
                       ->outcome()
                       .measurement()
                       .value()
                   == rat(37));

    // All equal: the range is zero, and no candidate -- never a division by
    // zero.
    constexpr auto equal =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(gapTable), fixtureE);
    STATIC_REQUIRE(equal->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(equal->passes() == 1);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable), fixtureB, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. pass n = 6\n"
             "3. critical(#2) = 45 [critical value at n = 6]\n"
             "4. 1/100\n"
             "5. #3 * #4 = 9/20\n"
             "6. pass 1: 6 values, mean 2429/60 g\n"
             "7. rejected element 4 of 6 (226/5 g) in pass 1: gap / range = 47/80 > 9/20 (gap to range)\n"
             "8. pass n = 5\n"
             "9. critical(#8) = 30 [critical value at n = 5]\n"
             "10. 1/100\n"
             "11. #9 * #10 = 3/10\n"
             "12. pass 2: 5 values, mean 1977/50 g\n"
             "13. rejected element 6 of 6 (186/5 g) in pass 2: gap / range = 26/33 > 3/10 (gap to range)\n"
             "14. pass n = 4\n"
             "15. critical(#14) = 700 [critical value at n = 4]\n"
             "16. 1/100\n"
             "17. #15 * #16 = 7\n"
             "18. pass 3: 4 values, mean 321/8 g\n"
             "19. settled: 2 rejected, 4 remain\n");
    CHECK(formula::render(rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable))
          == "without outliers(m(i); gap to range > critical(pass n, at 3, 4, 5, 6, 8) * 1/100; most extreme per pass; keep "
             "on limit; at most 2; keep at least 3)");
    CHECK(formula::render<formula::Dialect::LaTeX>(rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable))
          == "\\operatorname{without\\ outliers}({m}_{i};\\allowbreak \\text{gap to range} > "
             "\\operatorname{critical}(n_{\\text{pass}},\\allowbreak \\mathrm{at\\ }3,\\allowbreak 4,\\allowbreak "
             "5,\\allowbreak 6,\\allowbreak 8) \\cdot 1/100;\\allowbreak \\text{most extreme per pass};\\allowbreak "
             "\\text{keep on limit};\\allowbreak \\text{at most }2;\\allowbreak \\text{keep at least }3)");
    formula::Documentation const page =
        formula::document(formula::sample_mean(rejectionOf<MostExtreme, Keep, 2, 3, 6>(gapTable)));
    REQUIRE(page.rejections.size() == 1);
    CHECK(page.rejections[0].criterion == formula::CriterionKind::GapToRange);
    CHECK(page.rejections[0].limit == "critical(pass n, at 3, 4, 5, 6, 8) * 1/100");
}

TEST_CASE("a pass whose size the table does not declare is a miss, never a default (fixture G)", "[rejection]")
{
    // Seven determinations: n = 7 is no declared size, so pass 1's limit
    // misses. The failure is the limit's, and no element's.
    constexpr auto fixtureG = sampleOf(rat(40), rat(41), rat(39), rat(40), rat(42), rat(38), rat(40));
    constexpr auto missed =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 7>(gapTable), fixtureG);
    STATIC_REQUIRE(!missed.has_value());
    STATIC_REQUIRE(missed.error().error == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(!missed.error().element.has_value());
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(formula::sample_mean(rejectionOf<MostExtreme, Keep, 2, 3, 7>(gapTable)),
                                           fixtureG,
                                           formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. m = 40 g; 41 g; 39 g; 40 g; 42 g; 38 g; 40 g\n"
             "2. pass n = 7\n"
             "3. critical(#2) = argument outside the domain of the operation [no row for n = 7 (declared: 3, 4, 5, 6, 8)]\n"
             "4. #3 * (not evaluated) = argument outside the domain of the operation\n"
             "5. pass 1: 7 values, mean 40 g\n"
             "6. failed in pass 1: the limit: argument outside the domain of the operation\n"
             "7. sample_mean(#6) = argument outside the domain of the operation\n");
}

TEST_CASE("a sample of binned counts relays a binning's failure without the observation's position", "[rejection]")
{
    // Invented classes, 0 to under 41 and 41 to under 47 g: 50 g, the
    // second observation, is in neither, so the binning fails at observation
    // 2. That position is an observation's, not a count's: as a sample of
    // two counts it would name the second count, which is not at fault.
    constexpr formula::BandTable<2> massClasses { formula::band(0, 1, 41, 1), formula::band(41, 1, 47, 1) };
    constexpr auto weighedMasses = formula::environment(formula::MeasuredObservations<Mass, 3>(rat(40), rat(50), rat(41)));
    constexpr auto countedMasses = formula::binned<unit::Gram, massClasses>(formula::observations<Mass, 3>);
    constexpr auto binnedFailure = formula::checked_evaluate_series<Determinations>(countedMasses, weighedMasses);
    STATIC_REQUIRE(*binnedFailure.error().element == 1);
    constexpr auto sampled = formula::detail::dispatch_sample<Rational>(countedMasses, weighedMasses, formula::NullSink {});
    STATIC_REQUIRE(sampled.error().error == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(!sampled.error().element.has_value());
    constexpr auto rejected = formula::checked_evaluate_rejection<Determinations>(
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<1>>(
            countedMasses, formula::deviation_from_mean(formula::number(rat(1))), repeatTest, exampleCited),
        weighedMasses);
    STATIC_REQUIRE(rejected.error().error == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(!rejected.error().element.has_value());
}

namespace
{
/// Room for eight determinations, however many were made.
inline constexpr auto observedMasses = formula::observations<Mass, 8>;

template <typename... Values>
[[nodiscard]] constexpr auto observedOf(Values... values)
{
    return formula::environment(formula::MeasuredObservations<Mass, 8>(values...));
}

template <formula::PerPass P, formula::OnLimit L, std::size_t K, std::size_t M, typename Criterion>
[[nodiscard]] constexpr auto observedRejectionOf(Criterion criterion)
{
    return formula::without_outliers<P, L, formula::AtMost<K>, formula::KeepAtLeast<M>>(
        observedMasses, criterion, repeatTest, exampleCited);
}
} // namespace

TEST_CASE("a rejection of observations decides as the rejection of the same series (fixture A)", "[rejection]")
{
    // Six of eight places filled; the two unfilled are no determinations,
    // neither zeros nor absent ones: 321/8 g, {3, 5}, three passes, as the
    // series gives.
    constexpr auto observedA = observedOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10));
    constexpr auto rejection = observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent);
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejection, observedA);
    constexpr auto asSeries = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA);
    STATIC_REQUIRE(out->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(out->outcome().measurement().value() == asSeries->outcome().measurement().value());
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(out->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(out->passes() == 3);
    STATIC_REQUIRE(out->survivors().size() == 4);
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(rejection), observedA)->measurement().value()
        == rat(4));
}

TEST_CASE("a rejection of observations reads a critical value at the count made, not the capacity (fixture B)",
          "[rejection]")
{
    // Six made of eight: pass 1 reads n = 6, 9/20, and the gap table goes on
    // as for the series -- 321/8 g, {3, 5}, three passes. Read at the
    // capacity, n = 8 gives 1/20 in every pass.
    constexpr auto observedB = observedOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(452, 10), rat(40), rat(372, 10));
    constexpr auto three =
        formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 3>(gapTable), observedB);
    STATIC_REQUIRE(three->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(three->rejected().size() == 2);
    STATIC_REQUIRE(three->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(three->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(three->passes() == 3);
    // The deviation table, at n = 6 then 5: 1977/50 g with {3}.
    constexpr auto two =
        formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 3>(deviationTable), observedB);
    STATIC_REQUIRE(two->outcome().measurement().value() == rat(1977, 50));
    STATIC_REQUIRE(two->passes() == 2);

    // The observations' step is the sample's, and each rejected element is
    // one of the six made -- not of the eight places.
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(
        observedRejectionOf<MostExtreme, Keep, 2, 3>(gapTable), observedB, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. pass n = 6\n"
             "3. critical(#2) = 45 [critical value at n = 6]\n"
             "4. 1/100\n"
             "5. #3 * #4 = 9/20\n"
             "6. pass 1: 6 values, mean 2429/60 g\n"
             "7. rejected observation 4 of 6 (226/5 g) in pass 1: gap / range = 47/80 > 9/20 (gap to range)\n"
             "8. pass n = 5\n"
             "9. critical(#8) = 30 [critical value at n = 5]\n"
             "10. 1/100\n"
             "11. #9 * #10 = 3/10\n"
             "12. pass 2: 5 values, mean 1977/50 g\n"
             "13. rejected observation 6 of 6 (186/5 g) in pass 2: gap / range = 26/33 > 3/10 (gap to range)\n"
             "14. pass n = 4\n"
             "15. critical(#14) = 700 [critical value at n = 4]\n"
             "16. 1/100\n"
             "17. #15 * #16 = 7\n"
             "18. pass 3: 4 values, mean 321/8 g\n"
             "19. settled: 2 rejected, 4 remain\n");
}

TEST_CASE("a rejection of observations names an observation where it fails, or would reject",
          "[rejection][trace-render]")
{
    // 1 kg and 2^62 - 1 kg total 2^62 kg; the third, 2^62 + 9 kg, takes the
    // total past 2^63 - 1: the mean fails at observation 3.
    constexpr auto heavyObserved = formula::environment(formula::MeasuredObservations<Heavy, 3>(
        rat(1), Rational { 4611686018427387903 }, Rational { 4611686018427387913 }));
    constexpr auto heavyRejection =
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::observations<Heavy, 3>,
            formula::deviation_from_mean(formula::constant<unit::Kilogram>(rat(1))),
            repeatTest);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Heavy>(
        formula::sample_mean(heavyRejection), heavyObserved, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m_h = 1 kg; 4611686018427387903 kg; 4611686018427387913 kg\n"
             "2. pass 1: 3 values, mean overflow in exact arithmetic\n"
             "3. failed in pass 1: the mean: overflow in exact arithmetic at observation 3 of 3\n"
             "4. sample_mean(#3) = overflow in exact arithmetic\n");

    // An abort over observations names the observation it would reject --
    // and, every exceeding one at once, the observations.
    constexpr auto fixtureAObserved =
        observedOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10));
    formula::Trace<> aborted {};
    (void) formula::checked_evaluate_rejection<Mass>(
        observedRejectionOf<MostExtreme, Keep, 1, 3>(sixPercent), fixtureAObserved, formula::RecordingSink<> { aborted });
    CHECK(formula::render_trace(aborted, { .maxSteps = 40 })
              .ends_with("11. observation 6 of 6 would be rejection 2 of at most 1: discard the determinations and "
                         "repeat the test [Example Standard, 7.4]\n"));
    formula::Trace<> both {};
    (void) formula::checked_evaluate_rejection<Mass>(
        observedRejectionOf<EveryExceeding, Keep, 1, 4>(sixPercent),
        observedOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(452, 10), rat(40), rat(372, 10)),
        formula::RecordingSink<> { both });
    CHECK(formula::render_trace(both, { .maxSteps = 40 })
              .ends_with("6. observations 4 and 6 of 6 would be rejections 1 and 2 of at most 1: discard the "
                         "determinations and repeat the test [Example Standard, 7.4]\n"));
}

TEST_CASE("a statistic of a rejection that fails names the determination the rejection was given", "[rejection][trace-render]")
{
    // The census's sample A at 6 dp, 40.053270 ... 40.131659 g. Nothing lies
    // 2 g from the mean, so all six survive; their squared deviations then
    // overflow. The rejection's step lists no elements: the position counts
    // the sample the rejection was given -- elements of a series, and
    // observations of observations.
    constexpr auto withinTwoGrams = formula::deviation_from_mean(formula::constant<unit::Gram>(rat(2)));
    constexpr auto kept = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
        formula::series<Mass, 6>, withinTwoGrams, repeatTest);
    formula::Trace<> overSeries {};
    (void) formula::checked_evaluate<MassVariance>(formula::sample_variance(kept),
                                                   sampleOf(rat(40053270, 1000000),
                                                            rat(39475922, 1000000),
                                                            rat(39025798, 1000000),
                                                            rat(40615904, 1000000),
                                                            rat(39418416, 1000000),
                                                            rat(40131659, 1000000)),
                                                   formula::RecordingSink<> { overSeries });
    CHECK(formula::render_trace(overSeries, { .maxSteps = 20 }) == "1. m = 4005327/100000 g; 19737961/500000 g; 19512899/500000 g; 1269247/31250 g; 2463651/62500 g; 40131659/1000000 g\n"
                                                                   "2. 2 g\n"
                                                                   "3. pass 1: 6 values, mean 238720969/6000000 g\n"
                                                                   "4. settled: 0 rejected, 6 remain\n"
                                                                   "5. sample_variance(#4) = overflow in exact arithmetic at element 1\n");

    constexpr auto keptObserved =
        formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
            observedMasses, withinTwoGrams, repeatTest);
    formula::Trace<> overObservations {};
    (void) formula::checked_evaluate<MassVariance>(formula::sample_variance(keptObserved),
                                                   observedOf(rat(40053270, 1000000),
                                                              rat(39475922, 1000000),
                                                              rat(39025798, 1000000),
                                                              rat(40615904, 1000000),
                                                              rat(39418416, 1000000),
                                                              rat(40131659, 1000000)),
                                                   formula::RecordingSink<> { overObservations });
    CHECK(formula::render_trace(overObservations, { .maxSteps = 20 }) == "1. m = 4005327/100000 g; 19737961/500000 g; 19512899/500000 g; 1269247/31250 g; 2463651/62500 g; 40131659/1000000 g\n"
                                                                         "2. 2 g\n"
                                                                         "3. pass 1: 6 values, mean 238720969/6000000 g\n"
                                                                         "4. settled: 0 rejected, 6 remain\n"
                                                                         "5. sample_variance(#4) = overflow in exact arithmetic at observation 1\n");

    // With 30 g entered first, 30 g is rejected, and the overflow at the
    // first survivor is at the second determination as entered -- not the
    // survivors' first.
    constexpr auto afterOne = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
        formula::series<Mass, 7>, withinTwoGrams, repeatTest);
    formula::Trace<> afterRejected {};
    (void) formula::checked_evaluate<MassVariance>(formula::sample_variance(afterOne),
                                                   sampleOf(rat(30),
                                                            rat(40053270, 1000000),
                                                            rat(39475922, 1000000),
                                                            rat(39025798, 1000000),
                                                            rat(40615904, 1000000),
                                                            rat(39418416, 1000000),
                                                            rat(40131659, 1000000)),
                                                   formula::RecordingSink<> { afterRejected });
    CHECK(formula::render_trace(afterRejected, { .maxSteps = 20 }) == "1. m = 30 g; 4005327/100000 g; 19737961/500000 g; 19512899/500000 g; 1269247/31250 g; 2463651/62500 g; 40131659/1000000 g\n"
                                                                      "2. 2 g\n"
                                                                      "3. pass 1: 7 values, mean 268720969/7000000 g\n"
                                                                      "4. rejected element 1 of 7 (30 g) in pass 1: abs(x - mean) = 58720969/7000000 g > 2 g (deviation from mean)\n"
                                                                      "5. 2 g\n"
                                                                      "6. pass 2: 6 values, mean 238720969/6000000 g\n"
                                                                      "7. settled: 1 rejected, 6 remain\n"
                                                                      "8. sample_variance(#7) = overflow in exact arithmetic at element 2\n");
}

TEST_CASE("observations fewer than KeepAtLeast give the verdict before pass 1, outlier or none", "[rejection]")
{
    // KeepAtLeast<4> is the fewest that may remain, so three made do not meet
    // the method's precondition: the author's verdict, in no pass, whether
    // 44 g is past 6 % of the mean or 41 g is not.
    constexpr auto withOutlier = formula::checked_evaluate_rejection<Mass>(
        observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent), observedOf(rat(40), rat(40), rat(44)));
    STATIC_REQUIRE(withOutlier->outcome().is_verdict());
    STATIC_REQUIRE(withOutlier->rejected().empty());
    STATIC_REQUIRE(withOutlier->passes() == 0);
    constexpr auto withoutOutlier = formula::checked_evaluate_rejection<Mass>(
        observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent), observedOf(rat(40), rat(40), rat(41)));
    STATIC_REQUIRE(withoutOutlier->outcome().is_verdict());
    STATIC_REQUIRE(withoutOutlier->passes() == 0);
    STATIC_REQUIRE(withoutOutlier->survivors().size() == 3);
    STATIC_REQUIRE(
        formula::checked_evaluate<Mass>(formula::sample_mean(observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent)),
                                        observedOf(rat(40), rat(40), rat(41)))
            .error()
        == formula::ArithmeticError::DomainError);
    // None made: the verdict too, never a division by zero.
    constexpr auto none =
        formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent), observedOf());
    STATIC_REQUIRE(none->outcome().is_verdict());
    STATIC_REQUIRE(none->passes() == 0);
    // Exactly four made meet it, and the rejection runs.
    constexpr auto four = formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent),
                                                                    observedOf(rat(40), rat(40), rat(41), rat(40)));
    STATIC_REQUIRE(four->outcome().measurement().value() == rat(161, 4));
    STATIC_REQUIRE(four->passes() == 1);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 4>(sixPercent),
                                                     observedOf(rat(40), rat(40), rat(41)),
                                                     formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 40 g; 40 g; 41 g\n"
             "2. 3 values, fewer than the at least 4 to keep: discard the determinations and repeat the test "
             "[Example Standard, 7.4]\n");

    // A forged record: a short start in a pass, or with enough kept.
    REQUIRE(trace.rejectionRecords.size() == 1);
    formula::Trace<> inPass = trace;
    inPass.rejectionRecords.back().pass = 1;
    CHECK(formula::render_trace(inPass, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));
    formula::Trace<> enough = trace;
    enough.rejectionRecords.back().keepAtLeast = 3;
    CHECK(formula::render_trace(enough, { .maxSteps = 20 }).ends_with("rejection aborted (its record is invalid)\n"));
}

TEST_CASE("an observation that cannot be read fails a rejection at its own position", "[rejection]")
{
    // 1/INT64_MAX g has no kilogram value: the read fails at observation 2,
    // which is the sample's own second determination, and the rejection
    // relays it there.
    constexpr auto unreadable =
        observedOf(rat(40), Rational { 1, std::numeric_limits<std::int64_t>::max() }, rat(41), rat(40));
    constexpr auto failed =
        formula::checked_evaluate_rejection<Mass>(observedRejectionOf<MostExtreme, Keep, 2, 3>(sixPercent), unreadable);
    STATIC_REQUIRE(failed.error().error == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(*failed.error().element == 1);
}

TEST_CASE("number_of a rejection is the mean of the survivors, and nothing for its verdict", "[rejection]")
{
    constexpr auto settled = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA);
    constexpr auto aborted = formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA);
    STATIC_REQUIRE(formula::number_of(*settled) == rat(321, 8));
    STATIC_REQUIRE(formula::number_of(settled) == rat(321, 8));
    STATIC_REQUIRE(!formula::number_of(*aborted).has_value());
}
