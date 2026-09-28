// SPDX-License-Identifier: Apache-2.0
//
// Series and grading curves: one quantity at each point of a method's domain
// -- the mass retained on each screen of a screen analysis -- evaluated
// element by element, and the curve it makes.
//
//   1. A series is not a single value. It is marked in the formula, and a
//      reduction -- `sum` or `interpolate_at` -- brings it back to one value.
//   2. "Map" is elementwise arithmetic: one trace step per operation, the
//      broadcast scalar read once.
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
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// ---- Quantities ---------------------------------------------------------------
using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", unit::Gram>;
using TotalMass = formula::Quantity<struct TotalMassTag, "m_t", "total dry mass", unit::Gram>;
using Passing = formula::Quantity<struct PassingTag, "p", "percentage passing a screen", unit::Percent>;
using Share = formula::Quantity<struct ShareTag, "s_r", "share of the total retained", unit::One>;
using Opening = formula::Quantity<struct OpeningTag, "d", "screen opening", unit::Metre>;
using ParticleSize = formula::Quantity<struct ParticleSizeTag, "s", "particle size", unit::Metre>;
using Count = formula::Quantity<struct CountTag, "n", "particles in a class", unit::One>;

template <typename Q>
[[nodiscard]] constexpr formula::Measured<Q> m(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Measured<Q> { rat(numerator, denominator) };
}

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
inline constexpr auto passing =
    formula::constant<unit::Percent>(rat(100))
    - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>) / var<TotalMass>;

// 130, 210, 95, 340 and 28 g retained of 1250 g.
inline constexpr auto analysis =
    formula::environment(formula::measured_series<Retained>(
                             m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)),
                         m<TotalMass>(1250));

// A series reduced to one value: what the screens held in all.
inline constexpr auto retainedInAll = formula::sum(formula::series<Retained, 5>);

// The grading curve: the percentage passing at each screen, and read at a
// point between two of them.
inline constexpr auto grading = formula::curve(formula::domain<unit::Metre, screens>, passing);
inline constexpr auto passingAt173 = formula::interpolate_at(grading, formula::constant<unit::Metre>(rat(173)));

// ---- 3. Absence: the operations of the table --------------------------------
//
// Each element's share of the total, a plain fraction, and each mass rounded
// to a whole gram in grams.
inline constexpr auto shareOfTotal = formula::series<Retained, 5> / var<TotalMass>;
inline constexpr formula::PlacesTable<5> wholeGrams { formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 } };
inline constexpr auto roundedMasses =
    formula::rounded_elementwise<unit::Gram, wholeGrams, formula::RoundingMode::HalfAwayFromZero>(
        formula::series<Retained, 5>);

// ---- 5. Conformity: each element against its own row --------------------------
//
// Invented limits, one row per screen, in percent, unevenly spaced so that
// they resemble no published envelope. In practice these are the product
// specification -- master data, registered per customer -- and never part of
// a formula.
inline constexpr formula::Envelope<5> gradingEnvelope {
    formula::LimitRow { formula::limit(rat(31)), formula::limit(rat(43)) },
    formula::LimitRow { formula::limit(rat(47)), formula::limit(rat(59)) },
    formula::LimitRow { formula::limit(rat(1574, 25)), formula::unbounded },
    formula::LimitRow { formula::limit(rat(61)), formula::limit(rat(79)) },
    formula::LimitRow { formula::limit(rat(83)), formula::limit(rat(99)) }
};
inline constexpr auto gradingCheck =
    formula::conformity<unit::Percent>(passing, gradingEnvelope, formula::Verdict { "outside the grading envelope" });

// ---- 6. Snapping, and splicing two curves --------------------------------------
//
// The opening at which half the sample passes, read off the curve turned
// round, and snapped to the nearest declared screen.
inline constexpr auto halfPassing =
    formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(formula::interpolate_at(
        formula::curve(passing, formula::domain<unit::Metre, screens>), formula::constant<unit::Percent>(rat(50))));

// A coarse analysis and a fine one, at invented openings of their own.
inline constexpr formula::BreakpointTable<3> coarseScreens { formula::breakpoint(103),
                                                             formula::breakpoint(127),
                                                             formula::breakpoint(163) };
inline constexpr formula::BreakpointTable<3> fineScreens { formula::breakpoint(103, 10),
                                                           formula::breakpoint(137, 10),
                                                           formula::breakpoint(163, 10) };
inline constexpr auto coarse =
    formula::curve(formula::domain<unit::Metre, coarseScreens>,
                   formula::series_constant<unit::Percent>(rat(894, 25), rat(1154, 25), rat(1574, 25)));
inline constexpr auto fine = formula::curve(formula::domain<unit::Metre, fineScreens>,
                                            formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(142, 10)));
// The fine analysis as measured, for the absence table.
inline constexpr auto fineMeasured = formula::curve(formula::domain<unit::Metre, fineScreens>, formula::series<Passing, 3>);

// ---- 7. Binning ------------------------------------------------------------------
//
// Invented classes, half-open: 0 to under 127, 127 to under 197, 197 to under
// 331 m.
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 127, 1),
                                                     formula::band(127, 1, 197, 1),
                                                     formula::band(197, 1, 331, 1) };
inline constexpr auto counted = formula::binned<unit::Metre, sizeClasses>(formula::observations<ParticleSize, 8>);
inline constexpr auto shares = counted / formula::sum(counted);

// ---- Printing --------------------------------------------------------------------

bool allPassed = true;

void check(bool holds, char const* what)
{
    if (!holds)
    {
        allPassed = false;
        std::printf("CHECK FAILED: %s\n", what);
    }
}

// The derivation of a series, a curve or a single value, as `render_trace`
// gives it.
template <typename Q, typename S, typename Env>
[[nodiscard]] std::string series_trace(S const& seriesExpression, Env const& inputs, std::size_t maxSteps = 40)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_series<Q>(seriesExpression, inputs, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = maxSteps });
}

template <typename Q, typename N, typename Env>
[[nodiscard]] std::string value_trace(N const& expression, Env const& inputs)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Q>(expression, inputs, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 80 });
}

template <typename D, typename V, typename C, typename Env>
[[nodiscard]] std::string curve_trace(C const& curveExpression, Env const& inputs)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<D, V>(curveExpression, inputs, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 40 });
}

// The last line of a derivation: the step that answered.
[[nodiscard]] std::string last_line(std::string const& derivation)
{
    std::size_t const end = derivation.size() - 1;
    std::size_t const start = derivation.rfind('\n', end - 1);
    return derivation.substr(start == std::string::npos ? 0 : start + 1, end - (start == std::string::npos ? 0 : start + 1));
}

[[nodiscard]] char const* outcome_word(formula::ConstraintOutcome const& outcome)
{
    if (outcome.is_satisfied())
        return "satisfied";
    if (outcome.is_violated())
        return "violated";
    if (outcome.is_not_checked())
        return "not checked";
    return "invalid";
}
} // namespace

int main()
{
    std::printf("== 1. A series, and the reductions that bring it back to one value ==\n\n");
    std::printf("%s\n", formula::render(passing).c_str());
    std::printf("%s\n", formula::render(retainedInAll).c_str());
    std::printf("%s\n\n", formula::render(passingAt173).c_str());
    check(formula::render(passing) == "100 % - cumulative(m_r(i), from last) / m_t",
          "the series marked, the total unmarked");

    auto const inAll = formula::checked_evaluate<Retained>(retainedInAll, analysis);
    check(inAll.has_value() && inAll->measurement().value() == rat(803), "sum: 803 g retained in all");
    auto const at173 = formula::checked_evaluate<Passing>(passingAt173, analysis);
    check(at173.has_value() && at173->measurement().value() == rat(27708, 425), "173 m reads 27708/425 %");
    std::printf("%s\n", value_trace<Passing>(passingAt173, analysis).c_str());

    formula::Documentation const page = formula::document(passing);
    for (formula::SymbolEntry const& row: page.symbols)
        std::printf("  %.*s: %s, %zu value(s)\n",
                    static_cast<int>(row.symbol.size()),
                    row.symbol.data(),
                    row.shape == formula::ValueShape::Series ? "series" : "single value",
                    row.length);
    std::printf("\n");

    std::printf("== 2. Elementwise arithmetic: one step per operation ==\n\n");
    std::string const passingTrace = series_trace<Passing>(passing, analysis);
    std::printf("%s\n", passingTrace.c_str());
    check(passingTrace.find("3. cumulative(#2, from last) = 803 g; 673 g; 463 g; 368 g; 28 g\n") != std::string::npos,
          "the running total from the coarsest screen");
    // A computed step has no declared unit, so it reads in the coherent one:
    // 447/1250 is 35.76 %.
    check(passingTrace.ends_with("6. #1 - #5 = 447/1250; 577/1250; 787/1250; 441/625; 611/625\n"),
          "35.76, 46.16, 62.96, 70.56 and 97.76 % passing");
    std::printf("the same, within a budget of 8:\n%s\n", series_trace<Passing>(passing, analysis, 8).c_str());

    std::printf("== 3. Absence, decided at the size of what is produced ==\n\n");
    // The third screen's mass was not recorded; in the last row, the total.
    auto const oneUnrecorded = formula::environment(
        formula::measured_series<Retained>(
            m<Retained>(130), m<Retained>(210), formula::Measured<Retained>::absent(), m<Retained>(340), m<Retained>(28)),
        m<TotalMass>(1250));
    auto const noTotal =
        formula::environment(formula::measured_series<Retained>(
                                 m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)),
                             formula::Measured<TotalMass>::absent());
    // The fine analysis with its second value unrecorded.
    auto const fineGap = formula::environment(
        formula::measured_series<Passing>(m<Passing>(31, 10), formula::Measured<Passing>::absent(), m<Passing>(142, 10)));

    std::string const elementwise = last_line(series_trace<Share>(shareOfTotal, oneUnrecorded));
    std::string const rounding = last_line(series_trace<Retained>(roundedMasses, oneUnrecorded));
    std::string const running = last_line(series_trace<Retained>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>), oneUnrecorded));
    std::string const reduced = last_line(value_trace<Retained>(retainedInAll, oneUnrecorded));
    std::string const readOff = last_line(value_trace<Passing>(passingAt173, oneUnrecorded));
    std::string const spliced = last_line(
        curve_trace<Opening, Passing>(formula::splice<formula::Monotone::NonDecreasing>(coarse, fineMeasured), fineGap));
    auto const judgedWithGap = formula::check_conformity(gradingCheck, oneUnrecorded);
    std::string const broadcast = last_line(series_trace<Share>(shareOfTotal, noTotal));
    std::printf("m_r / m_t, third screen unrecorded: %s\n", elementwise.c_str());
    std::printf("round to whole grams, third screen unrecorded: %s\n", rounding.c_str());
    std::printf("running total from the last, third screen unrecorded: %s\n", running.c_str());
    std::printf("sum, third screen unrecorded: %s\n", reduced.c_str());
    std::printf("the curve read at 173 m, third screen unrecorded: %s\n", readOff.c_str());
    std::printf("splice, the fine analysis's second value unrecorded: %s\n", spliced.c_str());
    std::printf("conformity of the passing, third screen unrecorded:");
    for (formula::ConstraintOutcome const& outcome: judgedWithGap)
        std::printf(" %s;", outcome_word(outcome));
    std::printf("\n");
    std::printf("m_r / m_t, the total unrecorded: %s\n\n", broadcast.c_str());
    check(elementwise == "3. #1 / #2 = 13/125; 21/125; (not measured); 34/125; 14/625", "only that element absent");
    check(rounding.starts_with("2. round(#1, to 0/0/0/0/0 dp of g) = 130 g; 210 g; (not measured); 340 g; 28 g"),
          "rounding: only that element absent, in grams");
    check(running == "2. cumulative(#1, from last) = (not measured); (not measured); (not measured); 368 g; 28 g",
          "that total and every later one absent");
    check(reduced.ends_with("sum(#1) = (not measured)"), "the whole sum absent");
    check(readOff.ends_with("interpolate(#8, at #9) = (not measured)"), "the curve absent, and no range stated");
    check(spliced.find("= (not measured): (not measured);") != std::string::npos, "the whole splice absent");
    check(judgedWithGap[0].is_not_checked() && judgedWithGap[2].is_not_checked() && judgedWithGap[3].is_satisfied(),
          "the passing at the three finest screens not checked, the rest judged");
    check(broadcast.ends_with("(not measured); (not measured); (not measured); (not measured); (not measured)"),
          "an absent total makes every element absent");

    std::printf("== 4. A failed element fails the series, and names itself ==\n\n");
    // The coarsest screen held nothing: dividing the total by it fails there.
    auto const emptyScreen =
        formula::environment(formula::measured_series<Retained>(
                                 m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(0)),
                             m<TotalMass>(1250));
    std::string const failedTrace = series_trace<Count>(var<TotalMass> / formula::series<Retained, 5>, emptyScreen);
    std::printf("%s\n", failedTrace.c_str());
    check(failedTrace.ends_with("3. #1 / #2 = division by zero at element 5\n"), "the fifth element, counted from one");
    auto const failed = formula::checked_evaluate_series<Count>(var<TotalMass> / formula::series<Retained, 5>, emptyScreen);
    check(!failed.has_value() && failed.error().element == std::optional<std::size_t> { 4 }, "zero-based 4 in the API");
    if (!failed.has_value() && failed.error().element.has_value())
        std::printf("SeriesFailure: division by zero, element %zu, counted from zero, a result element: %s\n\n",
                    *failed.error().element,
                    failed.error().site == formula::FailureSite::ResultElement ? "yes" : "no");

    std::printf("== 5. Conformity: each element against its own row ==\n\n");
    std::printf("%s\n\n", formula::render(gradingCheck).c_str());
    auto const judged = formula::check_conformity(gradingCheck, analysis);
    for (std::size_t at = 0; at < judged.size(); ++at)
        std::printf("  screen %zu: %s\n", at + 1, outcome_word(judged[at]));
    std::printf("\n");
    check(judged[1].is_violated() && judged[2].is_satisfied(), "46.16 % below 47 %; 62.96 % on its closed lower limit");
    formula::Trace<> conformityTrace {};
    (void) formula::check_conformity(gradingCheck, analysis, formula::RecordingSink<> { conformityTrace });
    std::string const conformityLine = last_line(formula::render_trace(conformityTrace, { .maxSteps = 40 }));
    std::printf("%s\n\n", conformityLine.c_str());

    std::printf("== 6. Snapping, and splicing two curves ==\n\n");
    std::printf("%s\n", formula::render(halfPassing).c_str());
    std::string const snapTrace = value_trace<Opening>(halfPassing, analysis);
    std::printf("%s\n", snapTrace.c_str());
    check(snapTrace.find("[127 m to 163 m; nearer 127 m]") != std::string::npos, "4733/35 m snaps to 127 m, the nearer");

    auto const midway = formula::constant<unit::Metre>(rat(145));
    std::string const towardLower = last_line(
        value_trace<Opening>(formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(midway), analysis));
    std::string const towardHigher = last_line(
        value_trace<Opening>(formula::snapped<unit::Metre, screens, formula::SnapTie::TowardHigher>(midway), analysis));
    std::string const beyond = last_line(value_trace<Opening>(
        formula::snapped<unit::Metre, screens, formula::SnapTie::TowardHigher>(formula::constant<unit::Metre>(rat(251))),
        analysis));
    std::printf("%s\n%s\n%s\n\n", towardLower.c_str(), towardHigher.c_str(), beyond.c_str());
    check(towardLower.ends_with("= 127 m [127 m to 163 m; tie, toward lower]"), "a tie, decided lower");
    check(towardHigher.ends_with("= 163 m [127 m to 163 m; tie, toward higher]"), "a tie, decided higher");
    check(beyond.ends_with("[outside the permitted set, 103 m to 241 m]"), "past the last screen, a miss");

    auto const coarseFirst = formula::splice<formula::Monotone::NonDecreasing>(coarse, fine);
    auto const fineFirst = formula::splice<formula::Monotone::NonDecreasing>(fine, coarse);
    std::string const coarseFirstLine = last_line(curve_trace<Opening, Passing>(coarseFirst, analysis));
    std::string const fineFirstLine = last_line(curve_trace<Opening, Passing>(fineFirst, analysis));
    std::printf("%s\n%s\n", formula::render(coarseFirst).c_str(), coarseFirstLine.c_str());
    std::printf("%s\n%s\n\n", formula::render(fineFirst).c_str(), fineFirstLine.c_str());
    check(coarseFirstLine.substr(coarseFirstLine.find('=')) == fineFirstLine.substr(fineFirstLine.find('=')),
          "the same curve, whichever is written first");

    // The fine analysis's last point raised to 40 %: each curve rises, the
    // union does not.
    auto const raised = formula::curve(formula::domain<unit::Metre, fineScreens>,
                                       formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(40)));
    std::string const brokenLine = last_line(
        curve_trace<Opening, Passing>(formula::splice<formula::Monotone::NonDecreasing>(coarse, raised), analysis));
    std::printf("%s\n\n", brokenLine.c_str());
    check(brokenLine.ends_with("at element 4 [breaks non-decreasing at 103 m]"), "the union breaks where the curves join");

    std::printf("== 7. Binning raw observations into classes ==\n\n");
    std::printf("%s\n\n", formula::render(shares).c_str());
    auto const sample = formula::environment(formula::MeasuredObservations<ParticleSize, 8>(
        rat(103), rat(127), rat(163), rat(277), rat(113), rat(197), rat(241)));
    std::string const binningTrace = series_trace<Count>(counted, sample);
    std::printf("%s\n", binningTrace.c_str());
    check(binningTrace.ends_with("2. bin(#1) = 2; 2; 3\n"), "127 and 197 m counted in the upper class");
    auto const shared = formula::checked_evaluate_series<Count>(shares, sample);
    check(shared.has_value() && shared->elements()[2].value() == rat(3, 7), "the coarsest class holds 3/7");

    auto const oneTooLarge = formula::environment(formula::MeasuredObservations<ParticleSize, 8>(
        rat(103), rat(127), rat(163), rat(331), rat(113), rat(197), rat(241)));
    std::string const missTrace = series_trace<Count>(counted, oneTooLarge);
    std::printf("%s\n", missTrace.c_str());
    check(missTrace.ends_with("at observation 4 [331 m in no class; the classes cover 0 to under 331 m]\n"),
          "331 m, the last class's high bound, is in no class");

    std::printf("all checks passed: %s\n", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
