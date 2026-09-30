// SPDX-License-Identifier: Apache-2.0
//
// Series and grading curves: one quantity at each point of a method's domain
// -- the mass retained on each screen of a screen analysis -- evaluated
// element by element, and the curve it makes.
//
//   1. A series is not a single value. It is marked in the formula, and a
//      reduction -- `sum` or `interpolate_at` -- brings it back to one value.
//      A sum and a range read in the series' unit, unless it has an offset.
//   2. "Map" is elementwise arithmetic: one trace step per operation, the
//      broadcast scalar read once. A series scaled by a pure number reads
//      in its series' unit.
//   3. Absence is decided at the size of what is produced: every row of the
//      table, run.
//   4. A failed element fails the whole series, and names itself.
//   5. Conformity judges each element against its own row of a limit
//      envelope, closed at both ends.
//   6. Snapping a single value to a permitted one, its tie rule and its miss;
//      and a splice of two curves, whichever is written first.
//   7. Binning raw observations into classes, and an observation in no class.
//
// Every number here is invented. Every size has three significant digits,
// none of them a preferred number, and none is a sieve size or designation in
// any unit: the screens are 103, 127, 163, 197 and 241 m. Nothing resembles a
// real specification.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstddef>
#include <optional>
#include <print>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- Quantities ---------------------------------------------------------------
using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", unit::Gram>;
using TotalMass = formula::Quantity<struct TotalMassTag, "m_t", "total dry mass", unit::Gram>;
using Passing = formula::Quantity<struct PassingTag, "p", "percentage passing a screen", unit::Percent>;
using Share = formula::Quantity<struct ShareTag, "s_r", "share of the total retained", unit::One>;
using Opening = formula::Quantity<struct OpeningTag, "d", "screen opening", unit::Metre>;
using ParticleSize = formula::Quantity<struct ParticleSizeTag, "s", "particle size", unit::Metre>;
using Count = formula::Quantity<struct CountTag, "n", "particles in a class", unit::One>;
using Reading = formula::Quantity<struct ReadingTag, "T_r", "a temperature reading", unit::Celsius>;
using Kelvins = formula::Quantity<struct KelvinsTag, "T_k", "a temperature in kelvins", unit::Kelvin>;

// ---- 1. A series, and the reductions that bring it back to one value ----------
//
// The screens, declared once, in metres, ascending.
inline constexpr formula::BreakpointTable<5> screens { formula::breakpoint(103),
                                                       formula::breakpoint(127),
                                                       formula::breakpoint(163),
                                                       formula::breakpoint(197),
                                                       formula::breakpoint(241) };

// The percentage passing each screen: everything not retained on it or on a
// coarser one. `cumulative<FromLast>` runs from the coarsest screen down.
inline constexpr auto passing = formula::yields<Passing>(
    formula::constant<unit::Percent>(100)
    - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>) / var<TotalMass>);

// 130, 210, 95, 340 and 28 g retained of 1250 g.
inline constexpr auto analysis = formula::environment(formula::measured_series<Retained>(130, 210, 95, 340, 28),
                                                      formula::Measured<TotalMass> { 1250 });

// A series reduced to one value: what the screens held in all.
inline constexpr auto retainedInAll = formula::yields<Retained>(formula::sum(formula::series<Retained, 5>));

// The grading curve: the percentage passing at each screen, and read at a
// point between two of them.
inline constexpr auto grading = formula::curve(formula::domain<unit::Metre, screens>, passing.expression);
inline constexpr auto passingAt173 =
    formula::yields<Passing>(formula::interpolate_at(grading, formula::constant<unit::Metre>(173)));

// ---- 3. Absence: the operations of the table --------------------------------
//
// Each element's share of the total, a plain fraction, and each mass rounded
// to a whole gram in grams.
inline constexpr auto shareOfTotal = formula::yields<Share>(formula::series<Retained, 5> / var<TotalMass>);
inline constexpr formula::DecimalRounding wholeGram { unit::Gram,
                                                      formula::DecimalPlaces { 0 },
                                                      formula::RoundingMode::HalfAwayFromZero };
inline constexpr auto roundedMasses = formula::rounded_elementwise<wholeGram>(formula::series<Retained, 5>);

// ---- 5. Conformity: each element against its own row --------------------------
//
// Invented limits, one row per screen, in percent, unevenly spaced so that
// they resemble no published envelope. In practice these are the product
// specification -- master data, registered per customer -- and never part of
// a formula.
inline constexpr formula::Envelope<5> gradingEnvelope {
    formula::LimitRow { formula::limit(31), formula::limit(43) },
    formula::LimitRow { formula::limit(47), formula::limit(59) },
    formula::LimitRow { formula::limit(62.96_r), formula::unbounded },
    formula::LimitRow { formula::limit(61), formula::limit(79) },
    formula::LimitRow { formula::limit(83), formula::limit(99) }
};
inline constexpr auto gradingCheck = formula::conformity<unit::Percent>(passing.expression,
                                                                        gradingEnvelope,
                                                                        formula::Verdict { "outside the grading envelope" });

// ---- 6. Snapping, and splicing two curves --------------------------------------
//
// The opening at which half the sample passes, read off the curve turned
// round, and snapped to the nearest declared screen.
inline constexpr auto halfPassing =
    formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(formula::interpolate_at(
        formula::curve(passing.expression, formula::domain<unit::Metre, screens>), formula::constant<unit::Percent>(50)));

// A coarse analysis and a fine one, at invented openings of their own.
inline constexpr formula::BreakpointTable<3> coarseScreens { formula::breakpoint(103),
                                                             formula::breakpoint(127),
                                                             formula::breakpoint(163) };
inline constexpr formula::BreakpointTable<3> fineScreens { formula::breakpoint(10.3_r),
                                                           formula::breakpoint(13.7_r),
                                                           formula::breakpoint(16.3_r) };
inline constexpr auto coarse = formula::curve(formula::domain<unit::Metre, coarseScreens>,
                                              formula::series_constant<unit::Percent>(35.76_r, 46.16_r, 62.96_r));
inline constexpr auto fine = formula::curve(formula::domain<unit::Metre, fineScreens>,
                                            formula::series_constant<unit::Percent>(3.1_r, 8.4_r, 14.2_r));
// The fine analysis as measured, for the absence table.
inline constexpr auto fineMeasured = formula::curve(formula::domain<unit::Metre, fineScreens>, formula::series<Passing, 3>);

// ---- 7. Binning ------------------------------------------------------------------
//
// Invented classes, half-open: 0 to under 127, 127 to under 197, 197 to under
// 331 m.
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 127),
                                                     formula::band(127, 197),
                                                     formula::band(197, 331) };
inline constexpr auto counted =
    formula::yields<Count>(formula::binned<unit::Metre, sizeClasses>(formula::observations<ParticleSize, 8>));
inline constexpr auto shares = counted.expression / formula::sum(counted.expression);

// ---- Printing --------------------------------------------------------------------

bool allPassed = true;

void check(bool holds, char const* what)
{
    if (!holds)
    {
        allPassed = false;
        std::println("CHECK FAILED: {}", what);
    }
}

// The last line of a derivation: the step that answered.
[[nodiscard]] std::string last_line(std::string const& derivation)
{
    std::size_t const end = derivation.size() - 1;
    std::size_t const start = derivation.rfind('\n', end - 1);
    return derivation.substr(start == std::string::npos ? 0 : start + 1, end - (start == std::string::npos ? 0 : start + 1));
}
} // namespace

int main()
{
    std::println("== 1. A series, and the reductions that bring it back to one value ==\n");
    std::println("{}", formula::render(passing));
    std::println("{}", formula::render(retainedInAll));
    std::println("{}\n", formula::render(passingAt173));
    check(formula::render(passing) == "100 % - cumulative(m_r(i), from last) / m_t", "the series marked, the total unmarked");

    auto const inAll = formula::checked_evaluate(retainedInAll, analysis);
    if (!inAll)
    {
        std::println("sum of the retained masses: {}", inAll.error());
        return 1;
    }
    check(formula::number_of(inAll) == 803_r, "sum: 803 g retained in all");
    auto const at173 = formula::checked_explain(passingAt173, analysis);
    if (!at173)
    {
        std::println("the curve read at 173 m: {}", at173.error().error);
        return 1;
    }
    check(formula::number_of(at173->outcome) == 27708_r / 425, "173 m reads 27708/425 %");
    std::println("{}", formula::render_trace(at173->trace, { .maxSteps = 80 }));

    formula::Documentation const page = formula::document(passing);
    for (formula::SymbolEntry const& row: page.symbols)
        std::println("  {}: {}, {} value(s)",
                     row.symbol,
                     row.shape == formula::ValueShape::Series ? "series" : "single value",
                     row.length);
    std::println("");

    // Celsius readings: a sum and a range are no readings, and read in the
    // coherent unit; a mean is one, and reads in degrees Celsius.
    auto const readings = formula::environment(formula::measured_series<Reading>(23.7_r, 41.3_r, 37.9_r));
    constexpr auto threeReadings = formula::series<Reading, 3>;
    std::string const sumTrace = formula::render_trace(
        formula::trace_of<Kelvins>(formula::sum(threeReadings), readings), { .maxSteps = 80 });
    std::string const readingsLine = sumTrace.substr(0, sumTrace.find('\n'));
    std::string const sumLine = last_line(sumTrace);
    std::string const rangeLine = last_line(
        formula::render_trace(formula::trace_of<Kelvins>(formula::sample_range(threeReadings), readings), { .maxSteps = 80 }));
    std::string const meanLine = last_line(
        formula::render_trace(formula::trace_of<Reading>(formula::sample_mean(threeReadings), readings), { .maxSteps = 80 }));
    std::println("the readings: {}\ntheir sum: {}\ntheir range: {}\ntheir mean: {}\n", readingsLine, sumLine, rangeLine, meanLine);
    check(sumLine == "2. sum(#1) = 18447/20", "922.35 K, no reading");
    check(rangeLine == "2. sample_range(#1) = 88/5", "17.6 K, no reading");
    check(meanLine == "2. sample_mean(#1) = 343/10 \xc2\xb0" "C", "a mean of readings is a reading, 34.3 degC");

    std::println("== 2. Elementwise arithmetic: one step per operation ==\n");
    auto const passingRun = formula::explain_series(passing, analysis);
    std::string const passingTrace = formula::render_trace(passingRun.trace, { .maxSteps = 40 });
    std::println("{}", passingTrace);
    check(passingTrace.contains("3. cumulative(#2, from last) = 803 g; 673 g; 463 g; 368 g; 28 g\n"),
          "the running total from the coarsest screen");
    // A computed step has no declared unit, so it reads in the coherent one:
    // 447/1250 is 35.76 %.
    check(passingTrace.ends_with("6. #1 - #5 = 447/1250; 577/1250; 787/1250; 441/625; 611/625\n"),
          "35.76, 46.16, 62.96, 70.56 and 97.76 % passing");
    std::println("the same, within a budget of 8:\n{}", formula::render_trace(passingRun.trace, { .maxSteps = 8 }));

    // A series scaled by a pure number is still in its series' unit.
    auto const threeScreens = formula::environment(formula::measured_series<Retained>(137, 213, 293));
    std::string const scaledTrace = formula::render_trace(
        formula::explain_series<Retained>(formula::series<Retained, 3> * formula::number(1.5_r), threeScreens).trace,
        { .maxSteps = 40 });
    std::println("{}", scaledTrace);
    check(scaledTrace.ends_with("3. #1 * #2 = 411/2 g; 639/2 g; 879/2 g\n"), "grams times 3/2 are grams");

    std::println("== 3. Absence, decided at the size of what is produced ==\n");
    // The third screen's mass was not recorded; in the last row, the total.
    auto const oneUnrecorded = formula::environment(
        formula::measured_series<Retained>(130, 210, formula::not_measured, 340, 28), formula::Measured<TotalMass> { 1250 });
    auto const noTotal = formula::environment(formula::measured_series<Retained>(130, 210, 95, 340, 28),
                                              formula::Measured<TotalMass>::absent());
    // The fine analysis with its second value unrecorded.
    auto const fineGap = formula::environment(formula::measured_series<Passing>(3.1_r, formula::not_measured, 14.2_r));

    std::string const elementwise =
        last_line(formula::render_trace(formula::explain_series(shareOfTotal, oneUnrecorded).trace, { .maxSteps = 40 }));
    std::string const rounding = last_line(
        formula::render_trace(formula::explain_series<Retained>(roundedMasses, oneUnrecorded).trace, { .maxSteps = 40 }));
    std::string const running = last_line(formula::render_trace(
        formula::explain_series<Retained>(
            formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>), oneUnrecorded)
            .trace,
        { .maxSteps = 40 }));
    std::string const reduced =
        last_line(formula::render_trace(formula::trace_of(retainedInAll, oneUnrecorded), { .maxSteps = 80 }));
    std::string const readOff =
        last_line(formula::render_trace(formula::trace_of(passingAt173, oneUnrecorded), { .maxSteps = 80 }));
    std::string const spliced = last_line(formula::render_trace(
        formula::explain_curve<Opening, Passing>(formula::splice<formula::Monotone::NonDecreasing>(coarse, fineMeasured),
                                                 fineGap)
            .trace,
        { .maxSteps = 40 }));
    auto const judgedWithGap = formula::check_conformity(gradingCheck, oneUnrecorded);
    std::string const broadcast =
        last_line(formula::render_trace(formula::explain_series(shareOfTotal, noTotal).trace, { .maxSteps = 40 }));
    std::println("m_r / m_t, third screen unrecorded: {}", elementwise);
    std::println("round to whole grams, third screen unrecorded: {}", rounding);
    std::println("running total from the last, third screen unrecorded: {}", running);
    std::println("sum, third screen unrecorded: {}", reduced);
    std::println("the curve read at 173 m, third screen unrecorded: {}", readOff);
    std::println("splice, the fine analysis's second value unrecorded: {}", spliced);
    std::print("conformity of the passing, third screen unrecorded:");
    for (formula::ConstraintOutcome const& outcome: judgedWithGap)
        std::print(" {};", outcome.kind());
    std::println("");
    std::println("m_r / m_t, the total unrecorded: {}\n", broadcast);
    check(elementwise == "3. #1 / #2 = 13/125; 21/125; (not measured); 34/125; 14/625", "only that element absent");
    check(rounding.starts_with("2. round(#1, to 0/0/0/0/0 dp of g) = 130 g; 210 g; (not measured); 340 g; 28 g"),
          "rounding: only that element absent, in grams");
    check(running == "2. cumulative(#1, from last) = (not measured); (not measured); (not measured); 368 g; 28 g",
          "that total and every later one absent");
    check(reduced.ends_with("sum(#1) = (not measured)"), "the whole sum absent");
    check(readOff.ends_with("interpolate(#8, at #9) = (not measured)"), "the curve absent, and no range stated");
    check(spliced.contains("= (not measured): (not measured);"), "the whole splice absent");
    check(judgedWithGap[0].is_not_checked() && judgedWithGap[2].is_not_checked() && judgedWithGap[3].is_satisfied(),
          "the passing at the three finest screens not checked, the rest judged");
    check(broadcast.ends_with("(not measured); (not measured); (not measured); (not measured); (not measured)"),
          "an absent total makes every element absent");

    std::println("== 4. A failed element fails the series, and names itself ==\n");
    // The coarsest screen held nothing: dividing the total by it fails there.
    auto const emptyScreen = formula::environment(formula::measured_series<Retained>(130, 210, 95, 340, 0),
                                                  formula::Measured<TotalMass> { 1250 });
    auto const failed = formula::explain_series<Count>(var<TotalMass> / formula::series<Retained, 5>, emptyScreen);
    std::string const failedTrace = formula::render_trace(failed.trace, { .maxSteps = 40 });
    std::println("{}", failedTrace);
    check(failedTrace.ends_with("3. #1 / #2 = division by zero at element 5\n"), "the fifth element, counted from one");
    check(!failed.outcome.has_value() && failed.outcome.error().element == std::optional<std::size_t> { 4 },
          "zero-based 4 in the API");
    if (!failed.outcome.has_value() && failed.outcome.error().element.has_value())
        std::println("SeriesFailure: division by zero, element {}, counted from zero, a result element: {}\n",
                     *failed.outcome.error().element,
                     failed.outcome.error().site == formula::FailureSite::ResultElement ? "yes" : "no");

    std::println("== 5. Conformity: each element against its own row ==\n");
    std::println("{}\n", formula::render(gradingCheck));
    auto const conformity = formula::explain_conformity(gradingCheck, analysis);
    for (std::size_t at = 0; at < conformity.outcome.size(); ++at)
        std::println("  screen {}: {}", at + 1, conformity.outcome[at].kind());
    std::println("");
    check(conformity.outcome[1].is_violated() && conformity.outcome[2].is_satisfied(),
          "46.16 % below 47 %; 62.96 % on its closed lower limit");
    std::println("{}\n", last_line(formula::render_trace(conformity.trace, { .maxSteps = 40 })));

    std::println("== 6. Snapping, and splicing two curves ==\n");
    std::println("{}", formula::render(halfPassing));
    std::string const snapTrace = formula::render_trace(formula::trace_of<Opening>(halfPassing, analysis), { .maxSteps = 80 });
    std::println("{}", snapTrace);
    check(snapTrace.contains("[127 m to 163 m; nearer 127 m]"), "4733/35 m snaps to 127 m, the nearer");

    auto const midway = formula::constant<unit::Metre>(145);
    std::string const towardLower = last_line(formula::render_trace(
        formula::trace_of<Opening>(formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(midway), analysis),
        { .maxSteps = 80 }));
    std::string const towardHigher = last_line(formula::render_trace(
        formula::trace_of<Opening>(formula::snapped<unit::Metre, screens, formula::SnapTie::TowardHigher>(midway), analysis),
        { .maxSteps = 80 }));
    std::string const beyond = last_line(formula::render_trace(
        formula::trace_of<Opening>(
            formula::snapped<unit::Metre, screens, formula::SnapTie::TowardHigher>(formula::constant<unit::Metre>(251)),
            analysis),
        { .maxSteps = 80 }));
    std::println("{}\n{}\n{}\n", towardLower, towardHigher, beyond);
    check(towardLower.ends_with("= 127 m [127 m to 163 m; tie, toward lower]"), "a tie, decided lower");
    check(towardHigher.ends_with("= 163 m [127 m to 163 m; tie, toward higher]"), "a tie, decided higher");
    check(beyond.ends_with("[outside the permitted set, 103 m to 241 m]"), "past the last screen, a miss");

    auto const coarseFirst = formula::splice<formula::Monotone::NonDecreasing>(coarse, fine);
    auto const fineFirst = formula::splice<formula::Monotone::NonDecreasing>(fine, coarse);
    std::string const coarseFirstLine = last_line(
        formula::render_trace(formula::explain_curve<Opening, Passing>(coarseFirst, analysis).trace, { .maxSteps = 40 }));
    std::string const fineFirstLine = last_line(
        formula::render_trace(formula::explain_curve<Opening, Passing>(fineFirst, analysis).trace, { .maxSteps = 40 }));
    std::println("{}\n{}", formula::render(coarseFirst), coarseFirstLine);
    std::println("{}\n{}\n", formula::render(fineFirst), fineFirstLine);
    check(coarseFirstLine.substr(coarseFirstLine.find('=')) == fineFirstLine.substr(fineFirstLine.find('=')),
          "the same curve, whichever is written first");

    // The fine analysis's last point raised to 40 %: each curve rises, the
    // union does not.
    auto const raised = formula::curve(formula::domain<unit::Metre, fineScreens>,
                                       formula::series_constant<unit::Percent>(3.1_r, 8.4_r, 40_r));
    std::string const brokenLine = last_line(formula::render_trace(
        formula::explain_curve<Opening, Passing>(formula::splice<formula::Monotone::NonDecreasing>(coarse, raised), analysis)
            .trace,
        { .maxSteps = 40 }));
    std::println("{}\n", brokenLine);
    check(brokenLine.ends_with("at element 4 [breaks non-decreasing at 103 m]"), "the union breaks where the curves join");

    std::println("== 7. Binning raw observations into classes ==\n");
    std::println("{}\n", formula::render(shares));
    auto const sample = formula::environment(
        formula::MeasuredObservations<ParticleSize, 8>(103_r, 127_r, 163_r, 277_r, 113_r, 197_r, 241_r));
    std::string const binningTrace = formula::render_trace(formula::explain_series(counted, sample).trace, { .maxSteps = 40 });
    std::println("{}", binningTrace);
    check(binningTrace.ends_with("2. bin(#1) = 2; 2; 3\n"), "127 and 197 m counted in the upper class");
    auto const shared = formula::checked_evaluate_series<Count>(shares, sample);
    if (!shared)
    {
        std::println("the shares of the classes: {}", shared.error().error);
        return 1;
    }
    check(formula::number_of(shared->element(2)) == 3_r / 7, "the coarsest class holds 3/7");

    auto const oneTooLarge = formula::environment(
        formula::MeasuredObservations<ParticleSize, 8>(103_r, 127_r, 163_r, 331_r, 113_r, 197_r, 241_r));
    std::string const missTrace = formula::render_trace(formula::explain_series(counted, oneTooLarge).trace, { .maxSteps = 40 });
    std::println("{}", missTrace);
    check(missTrace.ends_with("at observation 4 [331 m in no class; the classes cover 0 to under 331 m]\n"),
          "331 m, the last class's high bound, is in no class");

    std::println("all checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
