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
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <print>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;
using namespace formula::literals;

// ---- Quantities -------------------------------------------------------------------
using Mass = formula::Quantity<struct MassTag, "m", "mass of a determination", unit::Gram>;
using Determinations = formula::Quantity<struct DeterminationsTag, "n", "number of determinations", unit::One>;
using Spread = formula::Quantity<struct SpreadTag, "s", "spread of the determinations", unit::Gram>;
/// A gram squared, the unit a variance of masses in grams is stated in. The
/// library ships no squared mass unit; a method that reports a variance
/// declares its own.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
using MassVariance = formula::Quantity<struct MassVarianceTag, "s2", "variance of the determinations", GramSquared>;

// ---- 1. A sample -------------------------------------------------------------------
//
// Six determinations of one mass: 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g. A
// method that fixes how many determinations it takes reads them as a series.
inline constexpr auto sixMasses =
    formula::environment(formula::measured_series<Mass>(40.2_r, 39.8_r, 40.5_r, 44, 40, 43.3_r));

inline constexpr auto determinations = formula::series<Mass, 6>;
inline constexpr auto mean = formula::yields<Mass>(formula::sample_mean(determinations));
inline constexpr auto count = formula::sample_count(determinations);
inline constexpr auto variance = formula::sample_variance(determinations);
inline constexpr auto range = formula::sample_range(determinations);
/// The spread is reported to 2 dp of g.
inline constexpr formula::DecimalRounding spreadRounding { unit::Gram,
                                                           formula::DecimalPlaces { 2 },
                                                           formula::RoundingMode::HalfAwayFromZero };
/// The spread reported exactly: the variance's square root, rounded to 2 dp of g.
inline constexpr auto spread = formula::rounded_sqrt<spreadRounding>(variance);

// ---- 2. Rejecting outliers ---------------------------------------------------------
inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation rejectionRule { .title = "Outliers",
                                                   .reference = "Example Standard 5:2022",
                                                   .section = "7.4" };

/// The method's rejection of @p criterion's outliers from @p sample: the most
/// extreme of each pass, a determination on the limit kept, at most `Rejected`
/// in all and at least `Kept` left -- and, when that cannot be kept, the
/// author's verdict and citation.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
template <typename Rejected, typename Kept, typename Sample, typename Criterion>
[[nodiscard]] constexpr auto rejecting(Sample sample, Criterion criterion)
{
    return formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, Rejected, Kept>(
        sample, criterion, repeatTest, rejectionRule);
}
// clang-format on

/// A determination more than 6 % of the pass's mean from it is an outlier.
inline constexpr auto sixPercent = formula::deviation_from_mean(0.06_r * formula::pass_mean<Mass>);

inline constexpr auto withoutOutliers =
    rejecting<formula::AtMost<2>, formula::KeepAtLeast<4>>(determinations, sixPercent);

/// The same rule, allowed one rejection.
inline constexpr auto atMostOne = rejecting<formula::AtMost<1>, formula::KeepAtLeast<4>>(determinations, sixPercent);

// Five determinations with a tie: 40, 40, 44, 40 and 36 g. 44 and 36 g are
// equally far from the mean.
inline constexpr auto tiedMasses = formula::environment(formula::measured_series<Mass>(40, 40, 44, 40, 36));
inline constexpr auto tieRejection =
    rejecting<formula::AtMost<2>, formula::KeepAtLeast<3>>(formula::series<Mass, 5>, sixPercent);

// The rule over raw observations, room for eight: how many were made is data.
inline constexpr auto observedWithoutOutliers =
    rejecting<formula::AtMost<2>, formula::KeepAtLeast<4>>(formula::observations<Mass, 8>, sixPercent);

// ---- 3. The criteria -----------------------------------------------------------------
//
// Six determinations with two low values: 40.2, 39.8, 40.5, 45.2, 40.0 and 37.2 g.
inline constexpr auto spreadMasses =
    formula::environment(formula::measured_series<Mass>(40.2_r, 39.8_r, 40.5_r, 45.2_r, 40, 37.2_r));

/// More than 7/4 sample standard deviations from the pass's mean.
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(7_r / 4));

// The author's table of critical values, by sample size. Invented, and plainly
// so: a gap ratio never exceeds 1, and this table's first two limits, 9 and
// 7, could not be exceeded by any sample. The library reads the author's
// table; it holds none of its own.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
inline constexpr formula::SampleSizeTable<5> declaredSizes { 3, 4, 5, 6, 8 };
inline constexpr auto gapLimit = formula::gap_to_range(
    formula::critical_value<declaredSizes, unit::One>(formula::pass_count, { 900_r, 700_r, 30_r, 45_r, 5_r })
    * 0.01_r);
// clang-format on

inline constexpr auto bySevenQuarters =
    rejecting<formula::AtMost<2>, formula::KeepAtLeast<3>>(determinations, sevenQuarters);
inline constexpr auto byGapToRange = rejecting<formula::AtMost<2>, formula::KeepAtLeast<3>>(determinations, gapLimit);

// ---- 4. Precision ----------------------------------------------------------------------
using FirstResult = formula::Quantity<struct FirstResultTag, "x_A", "first determination", unit::Gram>;
using SecondResult = formula::Quantity<struct SecondResultTag, "x_B", "second determination", unit::Gram>;
struct Tag
{
};

// Two determinations: 40.0 and 40.905 g, 0.905 g apart.
inline constexpr auto twoResults =
    formula::environment(formula::Measured<FirstResult> { 40 }, formula::Measured<SecondResult> { 40.905_r });

inline constexpr auto pairMean = (var<FirstResult> + var<SecondResult>) / 2_r;

/// The repeatability limit at a level: r = 0.1 g + level / 50. The level is a
/// placeholder; the precision limit binds it.
// Kept out of clang-format's hands: docs/statistics.md quotes it verbatim.
// clang-format off
inline constexpr auto limitAtLevel =
    formula::constant<unit::Gram>(0.1_r) + 0.02_r * formula::precision_level<FirstResult>;

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
} // namespace

int main()
{
    bool allPassed = true;
    auto const check = [&allPassed](bool condition, char const* what) {
        if (!condition)
        {
            std::println("CHECK FAILED: {}", what);
            allPassed = false;
        }
    };

    // ---- 1. A sample --------------------------------------------------------------------
    std::println("== 1. A sample and its statistics ==\n");

    constexpr auto meanValue = formula::checked_evaluate(mean, sixMasses);
    static_assert(meanValue.has_value());
    constexpr auto countValue = formula::checked_evaluate<Determinations>(count, sixMasses);
    static_assert(countValue.has_value());
    constexpr auto varianceValue = formula::checked_evaluate<MassVariance>(variance, sixMasses);
    static_assert(varianceValue.has_value());
    constexpr auto rangeValue = formula::checked_evaluate<Spread>(range, sixMasses);
    static_assert(rangeValue.has_value());
    auto const spreadValue = formula::checked_explain<Spread>(spread, sixMasses);
    if (!spreadValue)
    {
        std::println("the spread of six masses: {}", spreadValue.error().error);
        return 1;
    }
    std::println("{} = {:/}", formula::render(mean), meanValue->measurement());
    std::println("{} = {:/}", formula::render(count), countValue->measurement());
    std::println("{} = {:/}", formula::render(variance), varianceValue->measurement());
    std::println("{} = {:/}", formula::render(range), rangeValue->measurement());
    std::println("{} = {:/}", formula::render(spread), spreadValue->outcome.measurement());
    std::println("LaTeX: {}\n", formula::render<formula::Dialect::LaTeX>(spread));
    check(formula::number_of(meanValue) == 41.3_r, "the mean is 41.3 g");
    check(formula::number_of(varianceValue) == 3.416_r, "the variance divides by n - 1: 427/125 g2");
    check(formula::number_of(spreadValue->outcome) == 1.85_r,
          "the spread, sqrt(427/125) = 1.848... g, reported as 1.85 g");

    std::println("{}", formula::render_trace(spreadValue->trace, { .maxSteps = 40 }));

    // The same six masses as observations, in room for eight: the capacity is
    // a bound, and every statistic reads the six made.
    constexpr auto observed =
        formula::environment(formula::MeasuredObservations<Mass, 8>(40.2_r, 39.8_r, 40.5_r, 44_r, 40_r, 43.3_r));
    constexpr auto observedMean =
        formula::checked_evaluate<Mass>(formula::sample_mean(formula::observations<Mass, 8>), observed);
    static_assert(observedMean.has_value());
    constexpr auto observedCount =
        formula::checked_evaluate<Determinations>(formula::sample_count(formula::observations<Mass, 8>), observed);
    static_assert(observedCount.has_value());
    std::println("observations of 8 at most, 6 made: mean {:/}, count {:/}",
                 observedMean->measurement(),
                 observedCount->measurement());
    check(formula::number_of(observedCount) == 6_r, "the count is the six made, not the capacity");

    // Nine for eight places: refused, never truncated to fit.
    std::array<Rational, 9> nine {};
    nine.fill(40_r);
    auto const tooMany = formula::MeasuredObservations<Mass, 8>::from(nine);
    if (tooMany.has_value())
    {
        std::println("nine observations were taken for eight places");
        return 1;
    }
    check(tooMany.error() == formula::ObservationsOverCapacity { .given = 9, .capacity = 8 },
          "more observations than the capacity are refused, with both counts");
    std::println("{} observations for {} places: refused\n", tooMany.error().given, tooMany.error().capacity);

    // One determination not made: no mean, and no count either.
    auto const oneMissing =
        formula::environment(formula::measured_series<Mass>(40.2_r, 39.8_r, formula::not_measured, 44, 40, 43.3_r));
    auto const meanOfMissing = formula::checked_explain(mean, oneMissing);
    if (!meanOfMissing)
    {
        std::println("the mean with one determination missing: {}", meanOfMissing.error().error);
        return 1;
    }
    std::println("{}", formula::render_trace(meanOfMissing->trace, { .maxSteps = 40 }));
    check(meanOfMissing->outcome.is_empty(), "one missing determination, no mean");

    // ---- 2. Rejecting outliers ---------------------------------------------------------
    std::println("== 2. Rejecting outliers ==\n");

    std::println("{}\n", formula::render(withoutOutliers));
    auto const settled = formula::checked_evaluate_rejection<Mass>(withoutOutliers, sixMasses);
    if (!settled)
    {
        std::println("the rejection of six masses: {}", settled.error().error);
        return 1;
    }
    std::println("result: {:/}, {} rejected in {} passes\n",
                 settled->outcome().measurement(),
                 settled->rejected().size(),
                 settled->passes());
    check(formula::number_of(settled) == 40.125_r, "the mean of the four kept, 321/8 g");
    check(settled->rejected().size() == 2 && settled->passes() == 3, "44.0 g in pass 1, then 43.3 g in pass 2");

    std::println("{}",
                 formula::render_trace(formula::trace_of<Mass>(formula::sample_mean(withoutOutliers), sixMasses),
                                       { .maxSteps = 40 }));
    std::println("{}",
                 formula::render_trace(formula::trace_of<Mass>(formula::sample_mean(atMostOne), sixMasses),
                                       { .maxSteps = 40 }));
    auto const aborted = formula::checked_evaluate_rejection<Mass>(atMostOne, sixMasses);
    if (!aborted)
    {
        std::println("the rejection allowed one: {}", aborted.error().error);
        return 1;
    }
    check(aborted->outcome().is_verdict(), "one rejection too many is the author's verdict");

    auto const tied = formula::checked_evaluate_rejection<Mass>(tieRejection, tiedMasses);
    if (!tied)
    {
        std::println("the rejection of a tie: {}", tied.error().error);
        return 1;
    }
    check(tied->rejected().size() == 2 && tied->rejected()[0].pass == tied->rejected()[1].pass,
          "a tie rejects both, in the same pass");
    std::println("a tie: elements {} and {} rejected together in pass {}, result {:/}\n",
                 tied->rejected()[0].position + 1,
                 tied->rejected()[1].position + 1,
                 tied->rejected()[0].pass,
                 tied->outcome().measurement());

    auto const threeMade = formula::environment(formula::MeasuredObservations<Mass, 8>(40_r, 40_r, 41_r));
    auto const tooFew = formula::explain_rejection<Mass>(observedWithoutOutliers, threeMade);
    if (!tooFew.outcome)
    {
        std::println("the rejection of three observations: {}", tooFew.outcome.error().error);
        return 1;
    }
    std::println("{}", formula::render_trace(tooFew.trace, { .maxSteps = 20 }));
    check(tooFew.outcome->outcome().is_verdict(), "three made, to keep at least four: the verdict before pass 1");

    // ---- 3. The criteria ------------------------------------------------------------------
    std::println("== 3. Three criteria ==\n");

    std::println("{}", formula::render(bySevenQuarters));
    auto const byStddevs = formula::explain_rejection<Mass>(bySevenQuarters, spreadMasses);
    if (!byStddevs.outcome)
    {
        std::println("the rejection in standard deviations: {}", byStddevs.outcome.error().error);
        return 1;
    }
    std::println("{}", formula::render_trace(byStddevs.trace, { .maxSteps = 40 }));

    std::println("{}", formula::render(byGapToRange));
    auto const byGap = formula::explain_rejection<Mass>(byGapToRange, spreadMasses);
    if (!byGap.outcome)
    {
        std::println("the rejection by the gap to the range: {}", byGap.outcome.error().error);
        return 1;
    }
    std::println("{}", formula::render_trace(byGap.trace, { .maxSteps = 40 }));
    check(formula::number_of(byGap.outcome) == 40.125_r, "the gap table's three passes settle at 321/8 g");

    // ---- 4. Precision -----------------------------------------------------------------------
    std::println("== 4. Precision ==\n");

    std::println("{}", formula::render(agreement));
    std::println("LaTeX: {}\n", formula::render<formula::Dialect::LaTeX>(agreement));

    auto const atMean = formula::explain_check(agreement, twoResults);
    std::println("{}", formula::render_trace(atMean.trace, { .maxSteps = 40 }));
    auto const atRoundedLevel = formula::check(agreementAtRoundedLevel, twoResults);
    std::println(
        "level = the mean: {}\nlevel = the mean rounded to 1 g: {}\n", atMean.outcome.kind(), atRoundedLevel.kind());
    check(atMean.outcome.is_satisfied() && atRoundedLevel.is_violated(), "rounding the level first flips the verdict");

    auto const accepted = formula::check_method(pairMethod, twoResults);
    std::println("the method's acceptance check: {}\n", accepted[0].kind());
    check(accepted[0].is_satisfied(), "the precision check is one of the method's constraints");

    std::println("all checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
