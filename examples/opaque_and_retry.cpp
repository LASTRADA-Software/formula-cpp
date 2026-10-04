// SPDX-License-Identifier: Apache-2.0
//
// Two things a method states that are not a formula over its inputs: a named
// operation whose inside the method does not spell out, such as a
// least-squares line, and a step repeated until it is accepted, a bounded
// number of times.
//
//   1. An opaque operation receives values and returns values. Its outputs
//      are nodes, so a formula uses them like any other; the trace says the
//      operation's inside is not shown, and never shows one.
//   2. Using two outputs of one call runs the operation twice.
//   3. A least-squares line is such an operation: exact in Rational, refused
//      for a degenerate set of points, and Overflow -- never a wrong line --
//      when its exact sums leave Rational's range. Rounded where it is used,
//      to the precision the method reports it at, it answers there too.
//   4. A citation is required, and an empty one says so on the page.
//   5. A retry ends in exactly one of six ways, each run below. Running out
//      of attempts is the method's verdict, not a missing value.
//   6. Judged from the second attempt, two successive results must agree,
//      written with `abs`.
//   7. A line through raw observations, whose number is data: exact, with R^2
//      and the number of points, and rounded where used when the exact
//      fractions no longer fit.
//   8. Several regressors at once, and a design that cannot be solved: two
//      regressors that measure the same thing, twice over, are refused.
//
// Every number here is invented, as in every other example in this
// repository; nothing here cites a standard.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "fifty_readings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- 1. An opaque operation ----------------------------------------------------

using Reading = formula::Quantity<struct ReadingTag, "r", "an invented reading", unit::Gram>;
using Spread = formula::Quantity<struct SpreadTag, "r_sp", "the spread of the readings", unit::Gram>;

// A consumer's operation: a name, the shape of each input, the name of each
// output, what each output measures given what the inputs measure, and the
// computation, over values in coherent units. It never sees the environment.
struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 3> outputs { "lowest", "highest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 3>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 3>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const difference = formula::RepTraits<Rep>::subtract(most, least);
        if (!difference.has_value())
            return std::unexpected { difference.error() };
        return std::array { least, most, *difference };
    }
};

constexpr auto readings = formula::environment(formula::measured_series<Reading>(127, 103, 191, 139));

constexpr auto spanCall = formula::opaque<SeriesSpan>(
    { .title = "Spread of readings", .reference = "Example Standard 12", .section = "4.2" }, formula::series<Reading, 4>);
constexpr auto span = formula::opaque_output<"span">(spanCall);

// ---- 2. Two outputs, two runs ------------------------------------------------------

constexpr auto highestLessLowest = formula::opaque_output<"highest">(spanCall) - formula::opaque_output<"lowest">(spanCall);

// ---- 3. A least-squares line ---------------------------------------------------------

using Elapsed = formula::Quantity<struct ElapsedTag, "t", "an invented elapsed time", unit::Second>;
using Length = formula::Quantity<struct LengthTag, "L", "an invented length", unit::Millimetre>;
using Rate = formula::Quantity<struct RateTag, "v", "an invented rate of change", unit::MillimetrePerMinute>;

constexpr auto points = formula::environment(formula::measured_series<Elapsed>(1, 2, 4, 7),
                                             formula::measured_series<Length>(10.2_r, 10.9_r, 12.1_r, 14.3_r));

constexpr auto fit =
    formula::linear_least_squares(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>),
                                  { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto slope = formula::opaque_output<"slope">(fit);
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr formula::DecimalRounding slopeRounding =
    formula::declared_rounding(millimetrePerSecond, formula::RoundingMode::HalfEven);
constexpr auto roundedSlope = formula::rounded_output<"slope", slopeRounding>(fit);

/// Twenty-seven points, each on a different denominator: point k at
/// ((k + 1)/(k + 2) s, (2k + 3)/(k + 3) mm).
auto distinctDenominators()
{
    std::array<formula::Measured<Elapsed>, 27> times;
    std::array<formula::Measured<Length>, 27> lengths;
    for (std::size_t k = 0; k < 27; ++k)
    {
        auto const position = static_cast<std::int64_t>(k);
        times[k] = formula::Measured<Elapsed> { formula::Rational { position + 1, position + 2 } };
        lengths[k] = formula::Measured<Length> { formula::Rational { 2 * position + 3, position + 3 } };
    }
    return formula::environment(formula::MeasuredSeries<Elapsed, 27> { times },
                                formula::MeasuredSeries<Length, 27> { lengths });
}

// ---- 5. A retry ------------------------------------------------------------------------

using Estimate = formula::Quantity<struct EstimateTag, "w", "an invented iterated estimate", unit::Gram>;
using Tolerance = formula::Quantity<struct ToleranceTag, "t_w", "an invented tolerance", unit::Gram>;

// w(k) = 6.08 g + w(k-1) / 2, from 0 g: 6.08, 9.12, 10.64, 11.4 g, rising by
// 6.08, 3.04, 1.52, 0.76 g. Accepted when it rose by at most 0.76 g, written
// w(k-1) - w(k) >= -0.76 g, since the sequence rises.
constexpr auto halving = formula::constant<unit::Gram>(6.08_r) + formula::previous_attempt<Estimate> / 2;
constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                         >= formula::constant<unit::Gram>(-0.76_r);
constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(0));
constexpr formula::Verdict repeatDetermination { "repeat the determination" };
constexpr formula::Citation settledCitation { .title = "Settled estimate",
                                              .reference = "Example Standard 12",
                                              .section = "6" };

/// The method's retry of the estimate: at most @p Max attempts, judged from the
/// first, the determination repeated when none is settled.
template <std::size_t Max, typename... Steps>
constexpr auto estimating(Steps... steps)
{
    return formula::retry<Estimate, Max, formula::FirstJudged::AtFirstAttempt>(
        steps..., repeatDetermination, settledCitation);
}

constexpr auto fourAttempts = estimating<4>(fromZero, halving, settled);
constexpr auto threeAttempts = estimating<3>(fromZero, halving, settled);

// ---- 6. Two successive results agree -----------------------------------------------------

using Determination = formula::Quantity<struct DeterminationTag, "d", "an invented determination", unit::Gram>;
using Agreed = formula::Quantity<struct AgreedTag, "d_a", "an invented agreed determination", unit::Gram>;

constexpr auto agree = formula::abs(formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>)
                       <= formula::constant<unit::Gram>(1.27_r);

constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Determination>,
    agree,
    formula::Verdict { "repeat the test" },
    { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });

/// The last line of @p text, without its newline.
std::string lastLine(std::string const& text)
{
    std::string trimmed = text;
    while (!trimmed.empty() && trimmed.back() == '\n')
        trimmed.pop_back();
    std::size_t const lineStart = trimmed.rfind('\n');
    return lineStart == std::string::npos ? trimmed : trimmed.substr(lineStart + 1);
}

/// One line saying how a retry ended, and the line its trace ends with -- or
/// the failure, when it failed.
template <typename R>
void printEnding(char const* label, formula::ExplainedRetry<R> const& explained)
{
    if (!explained.outcome.has_value())
    {
        formula::RetryFailure const failure = explained.outcome.error();
        std::print("{}: {}, {}", label, formula::RetryEnd::Failed, failure.error);
        if (failure.attempt == formula::RetryFailure::atStartingValue)
            std::print(" at its starting value");
        else
            std::print(" at attempt {}", failure.attempt + 1);
    }
    else
        std::print("{}: {} after {} attempt(s)", label, explained.outcome->end(), explained.outcome->attempts_made());
    std::string const traced = formula::render_trace(explained.trace, { .maxSteps = 200 });
    if (traced.empty())
        std::println("; nothing traced");
    else
        std::println("\n  {}", lastLine(traced));
}

// ---- 7. A line through observations -------------------------------------------------

using SlopeRate = formula::Quantity<struct SlopeRateTag, "v_s", "an invented slope", millimetrePerSecond>;
using StartLength = formula::Quantity<struct StartLengthTag, "L_0", "an invented starting length", unit::Millimetre>;
using FitQuality = formula::Quantity<struct FitQualityTag, "R2", "an invented coefficient of determination", unit::One>;

constexpr auto observedFit =
    formula::linear_least_squares(formula::observations<Elapsed, 64>,
                                  formula::observations<Length, 64>,
                                  { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto observedLine = formula::yields<Rate>(formula::opaque_output<"slope">(observedFit));
constexpr auto observedSlope = formula::yields<SlopeRate>(formula::rounded_output<"slope", slopeRounding>(observedFit));
constexpr auto closeEnough = formula::constraint(
    formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(observedFit)
        >= formula::constant<unit::One>(0.998_r),
    formula::Verdict { "repeat the readings" });

constexpr auto observedPoints =
    formula::environment(formula::MeasuredObservations<Elapsed, 64>(1_r, 2_r, 4_r, 7_r),
                         formula::MeasuredObservations<Length, 64>(10.2_r, 10.9_r, 12.1_r, 14.3_r));

/// Fifty readings at eight decimals (fifty_readings.hpp).
auto fiftyReadings()
{
    formula_examples::FiftyReadings const atEight = formula_examples::fifty_readings(10'000);
    return formula::MeasuredObservations<Elapsed, 64>::from(atEight.seconds).and_then([&](auto const& timesMade) {
        return formula::MeasuredObservations<Length, 64>::from(atEight.millimetres).transform(
            [&](auto const& lengthsMade) { return formula::environment(timesMade, lengthsMade); });
    });
}

// ---- 8. Several regressors ------------------------------------------------------------

using Temperature = formula::Quantity<struct TemperatureTag, "T", "an invented temperature", unit::Celsius>;
// "w" is the iterated estimate's symbol already (section 5).
using Content = formula::Quantity<struct ContentTag, "w_c", "an invented content", unit::Percent>;
using Delay = formula::Quantity<struct DelayTag, "t_d", "an invented delay", unit::Second>;

// Units the method states its coefficients in.
constexpr formula::Unit millimetrePerKelvin { .dimension = formula::dim::Length / formula::dim::Temperature,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/K"),
                                              .decimals = 4 };
constexpr formula::Unit millimetrePerPercent { .dimension = formula::dim::Length,
                                               .magnitudeNumerator = 1,
                                               .magnitudeDenominator = 10,
                                               .symbolText = formula::symbol("mm/%"),
                                               .decimals = 4 };
using Expansion = formula::Quantity<struct ExpansionTag, "k_T", "an invented length per kelvin", millimetrePerKelvin>;
using Swelling = formula::Quantity<struct SwellingTag, "k_w", "an invented length per percent", millimetrePerPercent>;

constexpr auto byTemperatureAndContent = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 64>, formula::observations<Content, 64>),
    formula::observations<Length, 64>,
    { .title = "Length by temperature and content", .reference = "Example Standard 12", .section = "5.3" });

constexpr auto sixRows = formula::environment(
    formula::MeasuredObservations<Temperature, 64>(11.3_r, 13.7_r, 17.9_r, 19.1_r, 23.3_r, 29.7_r),
    formula::MeasuredObservations<Content, 64>(2.3_r, 3.1_r, 2.9_r, 4.1_r, 3.7_r, 4.3_r),
    formula::MeasuredObservations<Length, 64>(103.52_r, 104.13_r, 104.33_r, 105.1_r, 105.21_r, 106_r));

// The length at 0 degC: the constant is the length at 0 K.
constexpr auto lengthAtZeroCelsius =
    formula::rounded<unit::Millimetre, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(
        formula::opaque_output<"constant">(byTemperatureAndContent)
        + formula::opaque_output<"coefficient 1">(byTemperatureAndContent)
              * formula::constant<unit::Kelvin>(273.15_r));
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

    std::println("== 1. An opaque operation ==\n");

    std::println("{}\n{}", formula::render(span), formula::render<formula::Dialect::LaTeX>(span));
    auto const spread = formula::explain<Spread>(span, readings);
    std::string const spreadTrace = formula::render_trace(spread.trace, { .maxSteps = 20 });
    std::println("{}", spreadTrace);
    check(formula::number_of(spread.outcome) == 88_r, "191 g less 103 g");
    check(spreadTrace.contains("[inside not shown]"),
          "the trace says the operation's inside is not shown");

    formula::Documentation const page = formula::document(span);
    for (formula::OpaqueOperationEntry const& operation: page.opaqueOperations)
    {
        std::print("operation: {}, outputs:", operation.name);
        for (std::string_view const output: operation.outputs)
            std::print(" {}", output);
        std::println("");
    }
    std::println("");
    check(page.opaqueOperations.size() == 1, "one operation on the page");

    std::println("== 2. Two outputs, two runs ==\n");

    auto const twoOutputs = formula::explain<Spread>(highestLessLowest, readings);
    std::println("{}", formula::render_trace(twoOutputs.trace, { .maxSteps = 40 }));
    std::size_t runs = 0;
    for (formula::Step<> const& recorded: twoOutputs.trace.steps)
        if (recorded.kind == formula::StepKind::OpaqueOperation)
            ++runs;
    std::println("operation runs: {}\n", runs);
    check(runs == 2, "two outputs, two runs");
    check(formula::number_of(twoOutputs.outcome) == 88_r, "the same 88 g");

    std::println("== 3. A least-squares line ==\n");

    std::println("{}", formula::render(slope));
    auto const rate = formula::explain<Rate>(slope, points);
    std::println("{}", formula::render_trace(rate.trace, { .maxSteps = 20 }));
    check(formula::number_of(rate.outcome) == formula::Rational { 285, 7 }, "19/28 mm/s is 285/7 mm/min");

    constexpr auto onePoint = formula::environment(formula::measured_series<Elapsed>(3),
                                                   formula::measured_series<Length>(10.3_r));
    constexpr auto single = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 1>, formula::series<Length, 1>), { .reference = "Example Standard 12" });
    auto const noLine = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(single), onePoint);
    std::println("one point: {}", noLine.has_value() ? "a line" : formula::describe(noLine.error()));
    check(!noLine.has_value() && noLine.error() == formula::ArithmeticError::DomainError, "no line through one point");

    constexpr auto twentySeven = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 27>, formula::series<Length, 27>), { .reference = "Example Standard 12" });
    auto const tooWide =
        formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(twentySeven), distinctDenominators());
    std::println("twenty-seven distinct denominators: {}",
                 tooWide.has_value() ? "a line" : formula::describe(tooWide.error()));
    check(!tooWide.has_value() && tooWide.error() == formula::ArithmeticError::Overflow, "Overflow, never a wrong line");
    std::println("{}", formula::render(roundedSlope));
    auto const roundedRate = formula::explain<Rate>(roundedSlope, points);
    std::println("{}", formula::render_trace(roundedRate.trace, { .maxSteps = 20 }));
    check(formula::number_of(roundedRate.outcome) == 40.716_r, "0.6786 mm/s is 40.716 mm/min");

    constexpr auto roundedTwentySeven = formula::rounded_output<"slope", slopeRounding>(twentySeven);
    auto const roundedWide = formula::checked_evaluate<Rate>(roundedTwentySeven, distinctDenominators());
    check(formula::number_of(roundedWide) == 122.238_r,
          "rounded where used, twenty-seven distinct denominators answer: 2.0373 mm/s");
    if (roundedWide.has_value())
        std::println("twenty-seven distinct denominators, rounded where used: {}\n", *roundedWide);

    std::println("== 4. A citation is required ==\n");

    constexpr auto uncited = formula::opaque_output<"span">(formula::opaque<SeriesSpan>({}, formula::series<Reading, 4>));
    std::string const uncitedTrace =
        formula::render_trace(formula::trace_of<Spread>(uncited, readings), { .maxSteps = 20 });
    std::println("{}", uncitedTrace);
    check(uncitedTrace.contains("(no citation given)"), "an empty citation says so");

    std::println("== 5. A retry ends in one of six ways ==\n");

    std::println("{}\n", formula::render(fourAttempts));

    auto const accepted = formula::explain_retry(fourAttempts, formula::environment());
    std::println("{}", formula::render_trace(accepted.trace, { .maxSteps = 60 }));
    check(accepted.outcome.has_value() && accepted.outcome->end() == formula::RetryEnd::Accepted
              && formula::number_of(accepted.outcome) == 11.4_r,
          "accepted at the fourth attempt, 11.4 g");

    printEnding("allowed four", accepted);
    auto const exhausted = formula::explain_retry(threeAttempts, formula::environment());
    printEnding("allowed three", exhausted);
    check(exhausted.outcome.has_value() && exhausted.outcome->end() == formula::RetryEnd::Exhausted
              && exhausted.outcome->outcome().is_verdict()
              && exhausted.outcome->outcome().verdict_label() == repeatDetermination.label,
          "running out is the method's verdict");

    constexpr auto withinTolerance = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                                     >= formula::constant<unit::Gram>(0) - var<Tolerance>;
    constexpr auto againstTolerance = estimating<4>(fromZero, halving, withinTolerance);
    constexpr auto noTolerance = formula::environment(formula::Measured<Tolerance>::absent());
    auto const untold = formula::explain_retry(againstTolerance, noTolerance);
    printEnding("tolerance not measured", untold);
    check(untold.outcome.has_value() && untold.outcome->end() == formula::RetryEnd::NotJudgeable,
          "an absent comparison cannot tell");

    constexpr auto thirdMissing =
        formula::environment(formula::measured_series<Determination>(41.3_r, 43.9_r, formula::not_measured, 45.7_r));
    auto const unrecorded = formula::explain_retry(successive, thirdMissing);
    printEnding("third determination missing", unrecorded);
    check(unrecorded.outcome.has_value() && unrecorded.outcome->end() == formula::RetryEnd::NotRecorded,
          "a determination nobody recorded");

    constexpr auto dividing =
        formula::previous_attempt<Estimate> / 2 + formula::constant<unit::Gram>(1) / (formula::attempt_number - 1);
    constexpr auto failing = estimating<4>(fromZero, dividing, settled);
    auto const divided = formula::explain_retry(failing, formula::environment());
    printEnding("divides by k - 1", divided);
    check(!divided.outcome.has_value(), "an arithmetic failure");

    constexpr auto typedIn = formula::environment(formula::entered(formula::Measured<Estimate> { 11.3_r }));
    auto const entered = formula::explain_retry(fourAttempts, typedIn);
    printEnding("typed in by a person", entered);
    std::println("");
    check(entered.outcome.has_value() && entered.outcome->end() == formula::RetryEnd::ManuallyEntered,
          "a person's entry is never replaced");

    // No starting value, and previous_attempt read at the first attempt: the
    // author's mistake, which the trace names.
    constexpr auto noStart = estimating<4>(halving, settled);
    auto const mistaken = formula::explain_retry(noStart, formula::environment());
    std::string const mistakenTrace = formula::render_trace(mistaken.trace, { .maxSteps = 20 });
    std::println("{}", mistakenTrace);
    check(mistakenTrace.contains("previous attempt: none before attempt 1"), "no attempt before the first");

    std::println("== 6. Two successive results agree ==\n");

    std::println("{}", formula::render(successive));
    constexpr auto allFour =
        formula::environment(formula::measured_series<Determination>(41.3_r, 43.9_r, 42.7_r, 45.7_r));
    auto const agreed = formula::explain_retry(successive, allFour);
    printEnding("41.3, 43.9, 42.7, 45.7 g", agreed);
    std::println("");
    check(agreed.outcome.has_value() && agreed.outcome->end() == formula::RetryEnd::Accepted
              && agreed.outcome->accepted_at() == std::optional<std::size_t> { 2 }
              && formula::number_of(agreed.outcome) == 42.7_r,
          "42.7 g, at the third attempt");

    std::println("== 7. A line through observations ==\n");

    auto const exactLine = formula::explain(observedLine, observedPoints);
    std::println("{}", formula::render_trace(exactLine.trace, { .maxSteps = 30 }));
    check(formula::number_of(exactLine.outcome) == formula::Rational { 285, 7 }, "19/28 mm/s through observations");

    std::println("{}", formula::render(observedSlope));
    auto const roundedLine = formula::explain(observedSlope, observedPoints);
    std::println("{}", formula::render_trace(roundedLine.trace, { .maxSteps = 30 }));
    check(formula::number_of(roundedLine.outcome) == 0.6786_r, "0.6786 mm/s");

    bool const fitAccepted = formula::check(closeEnough, observedPoints).is_satisfied();
    std::println("r squared at 4 dp, floored, at least 0.998: {}", fitAccepted ? "satisfied" : "not satisfied");
    check(fitAccepted, "0.9981 is at least 0.998");

    constexpr auto flatLengths = formula::environment(
        formula::MeasuredObservations<Elapsed, 64>(1_r, 2_r, 4_r, 7_r),
        formula::MeasuredObservations<Length, 64>(12.7_r, 12.7_r, 12.7_r, 12.7_r));
    auto const flatLine = formula::checked_evaluate(observedLine, flatLengths);
    std::println("flat lengths: {}", flatLine.has_value() ? "a line" : formula::describe(flatLine.error()));
    check(!flatLine.has_value() && flatLine.error() == formula::ArithmeticError::DomainError, "a flat response has no R²");

    auto const fifty = fiftyReadings();
    if (!fifty)
    {
        std::println("fifty readings: {} observations for {} places", fifty.error().given, fifty.error().capacity);
        return 1;
    }
    auto const exactFifty = formula::checked_evaluate(observedLine, *fifty);
    std::println("fifty readings at 8 decimals, exact: {}",
                 exactFifty.has_value() ? "a line" : formula::describe(exactFifty.error()));
    auto const slopeOfFifty = formula::checked_evaluate(observedSlope, *fifty);
    auto const startOfFifty = formula::checked_evaluate<StartLength>(
        formula::
            rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
                observedFit),
        *fifty);
    auto const qualityOfFifty = formula::checked_evaluate<FitQuality>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
            observedFit),
        *fifty);
    check(slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value(), "fifty readings, rounded");
    if (slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value())
        std::println("fifty readings at 8 decimals, rounded: slope {}, intercept {}, r squared {}\n",
                     *slopeOfFifty,
                     *startOfFifty,
                     *qualityOfFifty);
    check(formula::number_of(slopeOfFifty) == 3.1707_r, "3.1707 mm/s");

    std::println("== 8. Several regressors ==\n");

    std::println("{}",
                 formula::render_trace(
                     formula::trace_of<Expansion>(formula::opaque_output<"coefficient 1">(byTemperatureAndContent), sixRows),
                     { .maxSteps = 40 }));

    auto const perKelvin = formula::checked_evaluate<Expansion>(
        formula::rounded_output<"coefficient 1", formula::declared_rounding(millimetrePerKelvin, formula::RoundingMode::HalfEven)>(
            byTemperatureAndContent),
        sixRows);
    auto const perPercent = formula::checked_evaluate<Swelling>(
        formula::rounded_output<"coefficient 2", formula::declared_rounding(millimetrePerPercent, formula::RoundingMode::HalfEven)>(
            byTemperatureAndContent),
        sixRows);
    auto const atZero = formula::checked_evaluate<StartLength>(lengthAtZeroCelsius, sixRows);
    check(perKelvin.has_value() && perPercent.has_value() && atZero.has_value(), "two regressors, rounded");
    if (perKelvin.has_value() && perPercent.has_value() && atZero.has_value())
        std::println("coefficient 1: {}, coefficient 2: {}, length at 0 degrees Celsius: {}",
                     *perKelvin,
                     *perPercent,
                     *atZero);
    check(formula::number_of(perPercent) == 0.5557_r, "0.5557 mm per percent");

    constexpr auto collinear = formula::multiple_least_squares(
        formula::regressors(formula::observations<Elapsed, 64>, formula::observations<Delay, 64>),
        formula::observations<Length, 64>,
        { .reference = "Example Standard 12" });
    constexpr auto twiceAsLate = formula::environment(
        formula::MeasuredObservations<Elapsed, 64>(1_r, 2_r, 4_r, 7_r),
        formula::MeasuredObservations<Delay, 64>(2_r, 4_r, 8_r, 14_r),
        formula::MeasuredObservations<Length, 64>(10.2_r, 10.9_r, 12.1_r, 14.3_r));
    auto const unsolvable = formula::checked_evaluate<Length>(formula::opaque_output<"constant">(collinear), twiceAsLate);
    std::println("a delay twice the elapsed time on every row: {}\n",
                 unsolvable.has_value() ? "a fit" : formula::describe(unsolvable.error()));
    check(!unsolvable.has_value() && unsolvable.error() == formula::ArithmeticError::DomainError,
          "a singular design is refused");
    std::println("all checks passed: {}", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
