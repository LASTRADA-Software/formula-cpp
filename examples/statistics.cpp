// SPDX-License-Identifier: Apache-2.0
//
// Statistics, outliers and precision: repeated determinations reduced to one
// reported value, exactly.
//
//   1. A sample and its statistics: the count, the mean, the variance and the
//      range of six masses, and the spread reported exactly by rounding the
//      variance's square root. The same masses as raw observations, whose
//      count is known only at run time. One mass not measured, and nothing is
//      reported.
//   2. Rejecting outliers: the mean re-run until nothing more is rejected, a
//      bound that turns one rejection too many into the author's verdict, a
//      tie that rejects both, and a sample too small to begin with.
//   3. The three criteria: a deviation from the mean, a deviation in standard
//      deviations -- decided exactly, by squares -- and the gap from an
//      extreme to its neighbour over the range, its limit read from the
//      author's own table of critical values by the pass's sample size.
//   4. Precision: two determinations checked against a limit that depends on
//      their own level, evaluated in two declared passes; the level rounded
//      first flips the verdict; the check joins a method's constraints.
//
// Every number, table and citation here is invented -- fictional Example
// Standard references, exactly as every other example in this repository is.
// No critical value here comes from any published table.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

/// @p value as `numerator/denominator`, or the whole number.
[[nodiscard]] std::string fraction_text(Rational value)
{
    std::string text = std::to_string(value.numerator());
    if (value.denominator() != 1)
        text += "/" + std::to_string(value.denominator());
    return text;
}

// ---- Quantities -------------------------------------------------------------------
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};
struct Spread: formula::Quantity<Spread, "s", "spread of the determinations", unit::Gram>
{
};
/// A gram squared, the unit a variance of masses in grams is stated in. The
/// library ships no squared mass unit; a method that reports a variance
/// declares its own.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};

// ---- 1. A sample -------------------------------------------------------------------
//
// Six determinations of one mass: 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g. A
// method that fixes how many determinations it takes reads them as a series.
inline constexpr auto sixMasses =
    formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { rat(402, 10) },
                                                        formula::Measured<Mass> { rat(398, 10) },
                                                        formula::Measured<Mass> { rat(405, 10) },
                                                        formula::Measured<Mass> { rat(44) },
                                                        formula::Measured<Mass> { rat(40) },
                                                        formula::Measured<Mass> { rat(433, 10) }));

inline constexpr auto determinations = formula::series<Mass, 6>;
inline constexpr auto mean = formula::sample_mean(determinations);
inline constexpr auto count = formula::sample_count(determinations);
inline constexpr auto variance = formula::sample_variance(determinations);
inline constexpr auto range = formula::sample_range(determinations);
/// The spread reported exactly: the variance's square root, rounded to 2 dp of g.
inline constexpr auto spread =
    formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(variance);

// ---- 2. Rejecting outliers ---------------------------------------------------------
inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation rejectionRule { .title = "Outliers",
                                                   .reference = "Example Standard 5:2022",
                                                   .section = "7.4" };

/// A determination more than 6 % of the pass's mean from it is an outlier.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
inline constexpr auto sixPercent = formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>);

inline constexpr auto withoutOutliers =
    formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
        determinations, sixPercent, repeatTest, rejectionRule);
// clang-format on

/// The same rule, allowed one rejection.
inline constexpr auto atMostOne = formula::
    without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
        determinations, sixPercent, repeatTest, rejectionRule);

// Five determinations with a tie: 40, 40, 44, 40 and 36 g. 44 and 36 g are
// equally far from the mean.
inline constexpr auto tiedMasses = formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { rat(40) },
                                                                                       formula::Measured<Mass> { rat(40) },
                                                                                       formula::Measured<Mass> { rat(44) },
                                                                                       formula::Measured<Mass> { rat(40) },
                                                                                       formula::Measured<Mass> { rat(36) }));
inline constexpr auto tieRejection = formula::
    without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<3>>(
        formula::series<Mass, 5>, sixPercent, repeatTest, rejectionRule);

// The rule over raw observations, room for eight: how many were made is data.
inline constexpr auto observedWithoutOutliers = formula::
    without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
        formula::observations<Mass, 8>, sixPercent, repeatTest, rejectionRule);

// ---- 3. The criteria -----------------------------------------------------------------
//
// Six determinations with two low values: 40.2, 39.8, 40.5, 45.2, 40.0 and 37.2 g.
inline constexpr auto spreadMasses =
    formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { rat(402, 10) },
                                                        formula::Measured<Mass> { rat(398, 10) },
                                                        formula::Measured<Mass> { rat(405, 10) },
                                                        formula::Measured<Mass> { rat(452, 10) },
                                                        formula::Measured<Mass> { rat(40) },
                                                        formula::Measured<Mass> { rat(372, 10) }));

/// More than 7/4 sample standard deviations from the pass's mean.
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(rat(7, 4)));

// The author's table of critical values, by sample size. Invented, and plainly
// so: a gap ratio never exceeds 1, and this table's first two limits, 9 and
// 7, could not be exceeded by any sample. The library reads the author's
// table; it holds none of its own.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
inline constexpr formula::SampleSizeTable<5> declaredSizes { 3, 4, 5, 6, 8 };
inline constexpr auto gapLimit = formula::gap_to_range(
    formula::critical_value<declaredSizes, unit::One>(formula::pass_count,
                                                      { rat(900), rat(700), rat(30), rat(45), rat(5) })
    * rat(1, 100));
// clang-format on

template <typename Criterion>
[[nodiscard]] constexpr auto rejectionBy(Criterion criterion)
{
    return formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<3>>(
            determinations, criterion, repeatTest, rejectionRule);
}

// ---- 4. Precision ----------------------------------------------------------------------
struct FirstResult: formula::Quantity<FirstResult, "x_A", "first determination", unit::Gram>
{
};
struct SecondResult: formula::Quantity<SecondResult, "x_B", "second determination", unit::Gram>
{
};
struct Tag
{
};

// Two determinations: 40.0 and 40.905 g, 0.905 g apart.
inline constexpr auto twoResults =
    formula::environment(formula::Measured<FirstResult> { rat(40) }, formula::Measured<SecondResult> { rat(40905, 1000) });

inline constexpr auto pairMean = (var<FirstResult> + var<SecondResult>) / rat(2);

/// The repeatability limit at a level: r = 0.1 g + level / 50. The level is a
/// placeholder; the precision limit binds it.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
inline constexpr auto limitAtLevel =
    formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<FirstResult>;

inline constexpr auto agreement = formula::constraint(
    formula::abs(var<FirstResult> - var<SecondResult>)
        <= formula::precision_limit<formula::PrecisionKind::Repeatability>(pairMean, limitAtLevel),
    formula::Verdict { "repeat the determinations" });
// clang-format on

inline constexpr auto agreementAtRoundedLevel = formula::constraint(
    formula::abs(var<FirstResult> - var<SecondResult>) <= formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::rounded<unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(pairMean),
        limitAtLevel),
    formula::Verdict { "repeat the determinations" });

inline constexpr auto pairMethod = formula::method(
    formula::variants(formula::variant<Tag>(pairMean)),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(agreement));

/// The trace of evaluating @p node for @p Result.
template <typename Result, typename Node, typename Env>
[[nodiscard]] std::string traceOf(Node const& node, Env const& environment)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Result>(node, environment, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 40 });
}
} // namespace

int main()
{
    bool allPassed = true;
    auto const check = [&allPassed](bool condition, char const* what) {
        if (!condition)
        {
            std::printf("CHECK FAILED: %s\n", what);
            allPassed = false;
        }
    };

    // ---- 1. A sample --------------------------------------------------------------------
    std::printf("== 1. A sample and its statistics ==\n\n");

    auto const meanValue = formula::checked_evaluate<Mass>(mean, sixMasses);
    auto const countValue = formula::checked_evaluate<Determinations>(count, sixMasses);
    auto const varianceValue = formula::checked_evaluate<MassVariance>(variance, sixMasses);
    auto const rangeValue = formula::checked_evaluate<Spread>(range, sixMasses);
    auto const spreadValue = formula::checked_evaluate<Spread>(spread, sixMasses);
    check(meanValue && countValue && varianceValue && rangeValue && spreadValue, "every statistic of six masses is a value");
    std::printf("%s = %s g\n", formula::render(mean).c_str(), fraction_text(meanValue->measurement().value()).c_str());
    std::printf("%s = %s\n", formula::render(count).c_str(), fraction_text(countValue->measurement().value()).c_str());
    std::printf(
        "%s = %s g2\n", formula::render(variance).c_str(), fraction_text(varianceValue->measurement().value()).c_str());
    std::printf("%s = %s g\n", formula::render(range).c_str(), fraction_text(rangeValue->measurement().value()).c_str());
    std::printf("%s = %s g\n", formula::render(spread).c_str(), fraction_text(spreadValue->measurement().value()).c_str());
    std::printf("LaTeX: %s\n\n", formula::render<formula::Dialect::LaTeX>(spread).c_str());
    check(meanValue->measurement().value() == rat(413, 10), "the mean is 41.3 g");
    check(varianceValue->measurement().value() == rat(427, 125), "the variance divides by n - 1: 427/125 g2");
    check(spreadValue->measurement().value() == rat(37, 20), "the spread, sqrt(427/125) = 1.848... g, reported as 1.85 g");

    std::printf("%s\n", traceOf<Spread>(spread, sixMasses).c_str());

    // The same six masses as observations, in room for eight: the capacity is
    // a bound, and every statistic reads the six made.
    auto const observed = formula::environment(
        formula::MeasuredObservations<Mass, 8>(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10)));
    auto const observedMean =
        formula::checked_evaluate<Mass>(formula::sample_mean(formula::observations<Mass, 8>), observed);
    auto const observedCount =
        formula::checked_evaluate<Determinations>(formula::sample_count(formula::observations<Mass, 8>), observed);
    check(observedMean && observedCount, "the observations' statistics are values");
    std::printf("observations of 8 at most, 6 made: mean %s g, count %s\n",
                fraction_text(observedMean->measurement().value()).c_str(),
                fraction_text(observedCount->measurement().value()).c_str());
    check(observedCount->measurement().value() == rat(6), "the count is the six made, not the capacity");

    // Nine for eight places: refused, never truncated to fit.
    std::array<Rational, 9> nine {};
    nine.fill(rat(40));
    auto const tooMany = formula::MeasuredObservations<Mass, 8>::from(nine);
    check(!tooMany.has_value() && tooMany.error() == formula::ObservationsOverCapacity { .given = 9, .capacity = 8 },
          "more observations than the capacity are refused, with both counts");
    std::printf("%zu observations for %zu places: refused\n\n", tooMany.error().given, tooMany.error().capacity);

    // One determination not made: no mean, and no count either.
    auto const oneMissing = formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { rat(402, 10) },
                                                                                formula::Measured<Mass> { rat(398, 10) },
                                                                                formula::Measured<Mass>::absent(),
                                                                                formula::Measured<Mass> { rat(44) },
                                                                                formula::Measured<Mass> { rat(40) },
                                                                                formula::Measured<Mass> { rat(433, 10) }));
    std::printf("%s\n", traceOf<Mass>(mean, oneMissing).c_str());
    check(formula::checked_evaluate<Mass>(mean, oneMissing)->is_empty(), "one missing determination, no mean");

    // ---- 2. Rejecting outliers ---------------------------------------------------------
    std::printf("== 2. Rejecting outliers ==\n\n");

    std::printf("%s\n\n", formula::render(withoutOutliers).c_str());
    auto const settled = formula::checked_evaluate_rejection<Mass>(withoutOutliers, sixMasses);
    check(settled.has_value(), "the rejection settles");
    std::printf("result: %s g, %zu rejected in %zu passes\n\n",
                fraction_text(settled->outcome().measurement().value()).c_str(),
                settled->rejected().size(),
                settled->passes());
    check(settled->outcome().measurement().value() == rat(321, 8), "the mean of the four kept, 321/8 g");
    check(settled->rejected().size() == 2 && settled->passes() == 3, "44.0 g in pass 1, then 43.3 g in pass 2");

    formula::Trace<> settledTrace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(withoutOutliers), sixMasses, formula::RecordingSink<> { settledTrace });
    std::string const settledText = formula::render_trace(settledTrace, { .maxSteps = 40 });
    std::printf("%s\n", settledText.c_str());

    formula::Trace<> abortedTrace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(atMostOne), sixMasses, formula::RecordingSink<> { abortedTrace });
    std::string const abortedText = formula::render_trace(abortedTrace, { .maxSteps = 40 });
    std::printf("%s\n", abortedText.c_str());
    check(formula::checked_evaluate_rejection<Mass>(atMostOne, sixMasses)->outcome().is_verdict(),
          "one rejection too many is the author's verdict");

    auto const tied = formula::checked_evaluate_rejection<Mass>(tieRejection, tiedMasses);
    check(tied && tied->rejected().size() == 2 && tied->rejected()[0].pass == tied->rejected()[1].pass,
          "a tie rejects both, in the same pass");
    std::printf("a tie: elements %zu and %zu rejected together in pass %zu, result %s g\n\n",
                tied->rejected()[0].position + 1,
                tied->rejected()[1].position + 1,
                tied->rejected()[0].pass,
                fraction_text(tied->outcome().measurement().value()).c_str());

    auto const threeMade = formula::environment(formula::MeasuredObservations<Mass, 8>(rat(40), rat(40), rat(41)));
    formula::Trace<> shortTrace {};
    (void) formula::checked_evaluate_rejection<Mass>(
        observedWithoutOutliers, threeMade, formula::RecordingSink<> { shortTrace });
    std::printf("%s\n", formula::render_trace(shortTrace, { .maxSteps = 20 }).c_str());
    check(formula::checked_evaluate_rejection<Mass>(observedWithoutOutliers, threeMade)->outcome().is_verdict(),
          "three made, to keep at least four: the verdict before pass 1");

    // ---- 3. The criteria ------------------------------------------------------------------
    std::printf("== 3. Three criteria ==\n\n");

    std::printf("%s\n", formula::render(rejectionBy(sevenQuarters)).c_str());
    formula::Trace<> stddevTrace {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionBy(sevenQuarters), spreadMasses, formula::RecordingSink<> { stddevTrace });
    std::printf("%s\n", formula::render_trace(stddevTrace, { .maxSteps = 40 }).c_str());

    std::printf("%s\n", formula::render(rejectionBy(gapLimit)).c_str());
    formula::Trace<> gapTrace {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionBy(gapLimit), spreadMasses, formula::RecordingSink<> { gapTrace });
    std::string const gapText = formula::render_trace(gapTrace, { .maxSteps = 40 });
    std::printf("%s\n", gapText.c_str());
    check(formula::checked_evaluate_rejection<Mass>(rejectionBy(gapLimit), spreadMasses)->outcome().measurement().value()
              == rat(321, 8),
          "the gap table's three passes settle at 321/8 g");

    // ---- 4. Precision -----------------------------------------------------------------------
    std::printf("== 4. Precision ==\n\n");

    std::printf("%s\n", formula::render(agreement).c_str());
    std::printf("LaTeX: %s\n\n", formula::render<formula::Dialect::LaTeX>(agreement).c_str());

    formula::Trace<> precisionTrace {};
    formula::ConstraintOutcome const atMean =
        formula::check(agreement, twoResults, formula::RecordingSink<> { precisionTrace });
    std::printf("%s\n", formula::render_trace(precisionTrace, { .maxSteps = 40 }).c_str());
    formula::ConstraintOutcome const atRoundedLevel = formula::check(agreementAtRoundedLevel, twoResults);
    std::printf("level = the mean: %s\nlevel = the mean rounded to 1 g: %s\n\n",
                atMean.is_satisfied() ? "satisfied" : "violated",
                atRoundedLevel.is_satisfied() ? "satisfied" : "violated");
    check(atMean.is_satisfied() && atRoundedLevel.is_violated(), "rounding the level first flips the verdict");

    auto const accepted = formula::check_method(pairMethod, twoResults);
    std::printf("the method's acceptance check: %s\n\n", accepted[0].is_satisfied() ? "satisfied" : "violated");
    check(accepted[0].is_satisfied(), "the precision check is one of the method's constraints");

    std::printf("all checks passed: %s\n", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
