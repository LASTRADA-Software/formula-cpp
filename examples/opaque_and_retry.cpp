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
//
// Every number here is invented, as in every other example in this
// repository; nothing here cites a standard.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

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

constexpr auto readings =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 127 } },
                                                           formula::Measured<Reading> { formula::Rational { 103 } },
                                                           formula::Measured<Reading> { formula::Rational { 191 } },
                                                           formula::Measured<Reading> { formula::Rational { 139 } }));

constexpr auto spanCall = formula::opaque<SeriesSpan>(
    { .title = "Spread of readings", .reference = "Example Standard 12", .section = "4.2" }, formula::series<Reading, 4>);
constexpr auto span = formula::opaque_output<"span">(spanCall);

// ---- 2. Two outputs, two runs ------------------------------------------------------

constexpr auto highestLessLowest = formula::opaque_output<"highest">(spanCall) - formula::opaque_output<"lowest">(spanCall);

// ---- 3. A least-squares line ---------------------------------------------------------

using Elapsed = formula::Quantity<struct ElapsedTag, "t", "an invented elapsed time", unit::Second>;
using Length = formula::Quantity<struct LengthTag, "L", "an invented length", unit::Millimetre>;
using Rate = formula::Quantity<struct RateTag, "v", "an invented rate of change", unit::MillimetrePerMinute>;

constexpr auto points =
    formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { formula::Rational { 1 } },
                                                           formula::Measured<Elapsed> { formula::Rational { 2 } },
                                                           formula::Measured<Elapsed> { formula::Rational { 4 } },
                                                           formula::Measured<Elapsed> { formula::Rational { 7 } }),
                         formula::measured_series<Length>(formula::Measured<Length> { formula::Rational { 102, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 109, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 121, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 143, 10 } }));

constexpr auto fit =
    formula::linear_least_squares(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>),
                                  { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto slope = formula::opaque_output<"slope">(fit);
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr auto roundedSlope = formula::rounded_output<"slope",
                                                      millimetrePerSecond,
                                                      formula::DecimalPlaces { 4 },
                                                      formula::RoundingMode::HalfEven>(fit);

/// Fifteen points, each on a different denominator: point k at
/// ((k + 1)/(k + 2) s, (2k + 3)/(k + 3) mm).
auto distinctDenominators()
{
    std::array<formula::Measured<Elapsed>, 15> times;
    std::array<formula::Measured<Length>, 15> lengths;
    for (std::size_t k = 0; k < 15; ++k)
    {
        auto const position = static_cast<std::int64_t>(k);
        times[k] = formula::Measured<Elapsed> { formula::Rational { position + 1, position + 2 } };
        lengths[k] = formula::Measured<Length> { formula::Rational { 2 * position + 3, position + 3 } };
    }
    return formula::environment(formula::MeasuredSeries<Elapsed, 15> { times },
                                formula::MeasuredSeries<Length, 15> { lengths });
}

// ---- 5. A retry ------------------------------------------------------------------------

using Estimate = formula::Quantity<struct EstimateTag, "w", "an invented iterated estimate", unit::Gram>;
using Tolerance = formula::Quantity<struct ToleranceTag, "t_w", "an invented tolerance", unit::Gram>;

// w(k) = 6.08 g + w(k-1) / 2, from 0 g: 6.08, 9.12, 10.64, 11.4 g, rising by
// 6.08, 3.04, 1.52, 0.76 g. Accepted when it rose by at most 0.76 g, written
// w(k-1) - w(k) >= -0.76 g, since the sequence rises.
constexpr auto halving = formula::constant<unit::Gram>(formula::Rational { 152, 25 })
                         + formula::previous_attempt<Estimate> / formula::Rational { 2 };
constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                         >= formula::constant<unit::Gram>(formula::Rational { -19, 25 });
constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(formula::Rational { 0 }));
constexpr formula::Verdict repeatDetermination { "repeat the determination" };
constexpr formula::Citation settledCitation { .title = "Settled estimate",
                                              .reference = "Example Standard 12",
                                              .section = "6" };

constexpr auto fourAttempts = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
    fromZero, halving, settled, repeatDetermination, settledCitation);
constexpr auto threeAttempts = formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(
    fromZero, halving, settled, repeatDetermination, settledCitation);

// ---- 6. Two successive results agree -----------------------------------------------------

using Determination = formula::Quantity<struct DeterminationTag, "d", "an invented determination", unit::Gram>;
using Agreed = formula::Quantity<struct AgreedTag, "d_a", "an invented agreed determination", unit::Gram>;

constexpr auto agree = formula::abs(formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>)
                       <= formula::constant<unit::Gram>(formula::Rational { 127, 100 });

constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Determination>,
    agree,
    formula::Verdict { "repeat the test" },
    { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });

constexpr formula::Measured<Determination> grams(std::int64_t tenths)
{
    return formula::Measured<Determination> { formula::Rational { tenths, 10 } };
}

/// How a retry ended, in the enumerator's own name.
std::string_view endName(formula::RetryEnd ended)
{
    switch (ended)
    {
        case formula::RetryEnd::Accepted:
            return "Accepted";
        case formula::RetryEnd::Exhausted:
            return "Exhausted";
        case formula::RetryEnd::NotJudgeable:
            return "NotJudgeable";
        case formula::RetryEnd::NotRecorded:
            return "NotRecorded";
        case formula::RetryEnd::Failed:
            return "Failed";
        case formula::RetryEnd::ManuallyEntered:
            return "ManuallyEntered";
    }
    return "unknown";
}

/// The last line of @p text, without its newline.
std::string lastLine(std::string const& text)
{
    std::string trimmed = text;
    while (!trimmed.empty() && trimmed.back() == '\n')
        trimmed.pop_back();
    std::size_t const lineStart = trimmed.rfind('\n');
    return lineStart == std::string::npos ? trimmed : trimmed.substr(lineStart + 1);
}

/// One line saying how @p retrying ended over @p environment, and the line
/// its trace ends with -- or the failure, when it failed.
template <typename Retrying, typename Env>
std::string ending(char const* label, Retrying const& retrying, Env const& environment)
{
    auto const explained = formula::explain_retry(retrying, environment);
    std::string line = std::string { label } + ": ";
    if (!explained.outcome.has_value())
    {
        formula::RetryFailure const failure = explained.outcome.error();
        line += "Failed, " + std::string { formula::describe(failure.error) }
                + (failure.attempt == formula::RetryFailure::atStartingValue
                       ? std::string { " at its starting value" }
                       : " at attempt " + std::to_string(failure.attempt + 1));
    }
    else
        line += std::string { endName(explained.outcome->end()) } + " after "
                + std::to_string(explained.outcome->attempts_made()) + " attempt(s)";
    std::string const traced = formula::render_trace(explained.trace, { .maxSteps = 200 });
    return line + (traced.empty() ? std::string { "; nothing traced" } : "\n  " + lastLine(traced));
}

// ---- 7. A line through observations -------------------------------------------------

using SlopeRate = formula::Quantity<struct SlopeRateTag, "v_s", "an invented slope", millimetrePerSecond>;
using StartLength = formula::Quantity<struct StartLengthTag, "L_0", "an invented starting length", unit::Millimetre>;
using FitQuality = formula::Quantity<struct FitQualityTag, "R2", "an invented coefficient of determination", unit::One>;

constexpr auto observedFit = formula::linear_least_squares(
    formula::observations<Elapsed, 64>,
    formula::observations<Length, 64>,
    { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto observedSlope = formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 },
                                                       formula::RoundingMode::HalfEven>(observedFit);
constexpr auto closeEnough = formula::constraint(
    formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(observedFit)
        >= formula::constant<unit::One>(formula::Rational { 998, 1000 }),
    formula::Verdict { "repeat the readings" });

constexpr auto observedPoints = formula::environment(
    formula::MeasuredObservations<Elapsed, 64>(formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 4 },
                                               formula::Rational { 7 }),
    formula::MeasuredObservations<Length, 64>(formula::Rational { 102, 10 }, formula::Rational { 109, 10 },
                                              formula::Rational { 121, 10 }, formula::Rational { 143, 10 }));

/// Fifty readings at four decimals: t = k + 1 + (7919 k mod 997) / 10^4 s and
/// L = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm, for k from 0.
auto fiftyReadings()
{
    std::array<formula::Rational, 50> times;
    std::array<formula::Rational, 50> lengths;
    for (std::size_t k = 0; k < 50; ++k)
    {
        auto const position = static_cast<std::int64_t>(k);
        times[k] = formula::Rational { 10'000 * (position + 1) + (7919 * position) % 997, 10'000 };
        lengths[k] = formula::Rational { 24'100'000 + 31'700 * position + (3217 * position) % 1009 - 504, 10'000 };
    }
    return formula::environment(*formula::MeasuredObservations<Elapsed, 64>::from(times),
                                *formula::MeasuredObservations<Length, 64>::from(lengths));
}

/// @p shown as its exact decimal, with its unit.
template <typename Q>
std::string decimalText(formula::Measured<Q> const& shown)
{
    // Held first: `view()` of a temporary is deleted, since the view would dangle.
    formula::NumberText const spelled = formula::number_text(shown, formula::NumberStyle::exact_decimal());
    return std::string { spelled.view() };
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

    std::printf("== 1. An opaque operation ==\n\n");

    std::printf("%s\n%s\n", formula::render(span).c_str(), formula::render<formula::Dialect::LaTeX>(span).c_str());
    auto const spread = formula::explain<Spread>(span, readings);
    std::printf("%s\n", formula::render_trace(spread.trace, { .maxSteps = 20 }).c_str());
    check(spread.outcome.measurement().value() == formula::Rational { 88 }, "191 g less 103 g");
    check(formula::render_trace(spread.trace, { .maxSteps = 20 }).find("[inside not shown]") != std::string::npos,
          "the trace says the operation's inside is not shown");

    formula::Documentation const page = formula::document(span);
    for (formula::OpaqueOperationEntry const& operation: page.opaqueOperations)
    {
        std::printf("operation: %.*s, outputs:", static_cast<int>(operation.name.size()), operation.name.data());
        for (std::string_view const output: operation.outputs)
            std::printf(" %.*s", static_cast<int>(output.size()), output.data());
        std::printf("\n");
    }
    std::printf("\n");
    check(page.opaqueOperations.size() == 1, "one operation on the page");

    std::printf("== 2. Two outputs, two runs ==\n\n");

    auto const twoOutputs = formula::explain<Spread>(highestLessLowest, readings);
    std::printf("%s\n", formula::render_trace(twoOutputs.trace, { .maxSteps = 40 }).c_str());
    std::size_t runs = 0;
    for (formula::Step<> const& recorded: twoOutputs.trace.steps)
        if (recorded.kind == formula::StepKind::OpaqueOperation)
            ++runs;
    std::printf("operation runs: %zu\n\n", runs);
    check(runs == 2, "two outputs, two runs");
    check(twoOutputs.outcome.measurement().value() == formula::Rational { 88 }, "the same 88 g");

    std::printf("== 3. A least-squares line ==\n\n");

    std::printf("%s\n", formula::render(slope).c_str());
    auto const rate = formula::explain<Rate>(slope, points);
    std::printf("%s\n", formula::render_trace(rate.trace, { .maxSteps = 20 }).c_str());
    check(rate.outcome.measurement().value() == formula::Rational { 285, 7 }, "19/28 mm/s is 285/7 mm/min");

    constexpr auto onePoint =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { formula::Rational { 3 } }),
                             formula::measured_series<Length>(formula::Measured<Length> { formula::Rational { 103, 10 } }));
    constexpr auto single = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 1>, formula::series<Length, 1>), { .reference = "Example Standard 12" });
    auto const noLine = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(single), onePoint);
    std::printf("one point: %s\n",
                noLine.has_value() ? "a line" : std::string { formula::describe(noLine.error()) }.c_str());
    check(!noLine.has_value() && noLine.error() == formula::ArithmeticError::DomainError, "no line through one point");

    constexpr auto fifteen = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 15>, formula::series<Length, 15>), { .reference = "Example Standard 12" });
    auto const tooWide = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fifteen), distinctDenominators());
    std::printf("fifteen distinct denominators: %s\n",
                tooWide.has_value() ? "a line" : std::string { formula::describe(tooWide.error()) }.c_str());
    check(!tooWide.has_value() && tooWide.error() == formula::ArithmeticError::Overflow, "Overflow, never a wrong line");
    std::printf("%s\n", formula::render(roundedSlope).c_str());
    auto const roundedRate = formula::explain<Rate>(roundedSlope, points);
    std::printf("%s\n", formula::render_trace(roundedRate.trace, { .maxSteps = 20 }).c_str());
    check(roundedRate.outcome.measurement().value() == formula::Rational { 10179, 250 }, "0.6786 mm/s is 40.716 mm/min");

    constexpr auto roundedFifteen = formula::rounded_output<"slope",
                                                            millimetrePerSecond,
                                                            formula::DecimalPlaces { 4 },
                                                            formula::RoundingMode::HalfEven>(fifteen);
    auto const roundedWide = formula::checked_evaluate<Rate>(roundedFifteen, distinctDenominators());
    check(roundedWide.has_value() && roundedWide->measurement().value() == formula::Rational { 14529, 125 },
          "rounded where used, fifteen distinct denominators answer: 1.9372 mm/s");
    if (roundedWide.has_value())
    {
        formula::NumberText const wideSlope =
            formula::number_text(roundedWide->measurement(), formula::NumberStyle::exact_decimal());
        std::printf("fifteen distinct denominators, rounded where used: %.*s\n\n", static_cast<int>(wideSlope.view().size()),
                    wideSlope.view().data());
    }

    std::printf("== 4. A citation is required ==\n\n");

    constexpr auto uncited = formula::opaque_output<"span">(formula::opaque<SeriesSpan>({}, formula::series<Reading, 4>));
    auto const uncitedSpread = formula::explain<Spread>(uncited, readings);
    std::string const uncitedTrace = formula::render_trace(uncitedSpread.trace, { .maxSteps = 20 });
    std::printf("%s\n", uncitedTrace.c_str());
    check(uncitedTrace.find("(no citation given)") != std::string::npos, "an empty citation says so");

    std::printf("== 5. A retry ends in one of six ways ==\n\n");

    std::printf("%s\n\n", formula::render(fourAttempts).c_str());

    auto const accepted = formula::explain_retry(fourAttempts, formula::environment());
    std::printf("%s\n", formula::render_trace(accepted.trace, { .maxSteps = 60 }).c_str());
    check(accepted.outcome.has_value() && accepted.outcome->end() == formula::RetryEnd::Accepted
              && accepted.outcome->outcome().measurement().value() == formula::Rational { 57, 5 },
          "accepted at the fourth attempt, 11.4 g");

    std::printf("%s\n", ending("allowed four", fourAttempts, formula::environment()).c_str());
    std::printf("%s\n", ending("allowed three", threeAttempts, formula::environment()).c_str());
    auto const exhausted = formula::checked_evaluate_retry(threeAttempts, formula::environment());
    check(exhausted.has_value() && exhausted->end() == formula::RetryEnd::Exhausted && exhausted->outcome().is_verdict()
              && exhausted->outcome().verdict_label() == repeatDetermination.label,
          "running out is the method's verdict");

    constexpr auto withinTolerance = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                                     >= formula::constant<unit::Gram>(formula::Rational { 0 }) - var<Tolerance>;
    constexpr auto againstTolerance = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        fromZero, halving, withinTolerance, repeatDetermination, settledCitation);
    constexpr auto noTolerance = formula::environment(formula::Measured<Tolerance>::absent());
    std::printf("%s\n", ending("tolerance not measured", againstTolerance, noTolerance).c_str());
    check(formula::checked_evaluate_retry(againstTolerance, noTolerance)->end() == formula::RetryEnd::NotJudgeable,
          "an absent comparison cannot tell");

    constexpr auto thirdMissing = formula::environment(formula::measured_series<Determination>(
        grams(413), grams(439), formula::Measured<Determination>::absent(), grams(457)));
    std::printf("%s\n", ending("third determination missing", successive, thirdMissing).c_str());
    check(formula::checked_evaluate_retry(successive, thirdMissing)->end() == formula::RetryEnd::NotRecorded,
          "a determination nobody recorded");

    constexpr auto dividing =
        formula::previous_attempt<Estimate> / formula::Rational { 2 }
        + formula::constant<unit::Gram>(formula::Rational { 1 }) / (formula::attempt_number - formula::Rational { 1 });
    constexpr auto failing = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        fromZero, dividing, settled, repeatDetermination, settledCitation);
    std::printf("%s\n", ending("divides by k - 1", failing, formula::environment()).c_str());
    check(!formula::checked_evaluate_retry(failing, formula::environment()).has_value(), "an arithmetic failure");

    constexpr auto typedIn =
        formula::environment(formula::entered(formula::Measured<Estimate> { formula::Rational { 113, 10 } }));
    std::printf("%s\n\n", ending("typed in by a person", fourAttempts, typedIn).c_str());
    check(formula::checked_evaluate_retry(fourAttempts, typedIn)->end() == formula::RetryEnd::ManuallyEntered,
          "a person's entry is never replaced");

    // No starting value, and previous_attempt read at the first attempt: the
    // author's mistake, which the trace names.
    constexpr auto noStart = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        halving, settled, repeatDetermination, settledCitation);
    auto const mistaken = formula::explain_retry(noStart, formula::environment());
    std::string const mistakenTrace = formula::render_trace(mistaken.trace, { .maxSteps = 20 });
    std::printf("%s\n", mistakenTrace.c_str());
    check(mistakenTrace.find("previous attempt: none before attempt 1") != std::string::npos, "no attempt before the first");

    std::printf("== 6. Two successive results agree ==\n\n");

    std::printf("%s\n", formula::render(successive).c_str());
    constexpr auto allFour =
        formula::environment(formula::measured_series<Determination>(grams(413), grams(439), grams(427), grams(457)));
    auto const agreed = formula::checked_evaluate_retry(successive, allFour);
    std::printf("%s\n\n", ending("41.3, 43.9, 42.7, 45.7 g", successive, allFour).c_str());
    check(agreed.has_value() && agreed->end() == formula::RetryEnd::Accepted
              && agreed->accepted_at() == std::optional<std::size_t> { 2 }
              && agreed->outcome().measurement().value() == formula::Rational { 427, 10 },
          "42.7 g, at the third attempt");

    std::printf("== 7. A line through observations ==\n\n");

    auto const exactLine = formula::explain<Rate>(formula::opaque_output<"slope">(observedFit), observedPoints);
    std::printf("%s\n", formula::render_trace(exactLine.trace, { .maxSteps = 30 }).c_str());
    check(exactLine.outcome.measurement().value() == formula::Rational { 285, 7 }, "19/28 mm/s through observations");

    std::printf("%s\n", formula::render(observedSlope).c_str());
    auto const roundedLine = formula::explain<SlopeRate>(observedSlope, observedPoints);
    std::printf("%s\n", formula::render_trace(roundedLine.trace, { .maxSteps = 30 }).c_str());
    check(roundedLine.outcome.measurement().value() == formula::Rational { 3393, 5000 }, "0.6786 mm/s");

    bool const fitAccepted = formula::check(closeEnough, observedPoints).is_satisfied();
    std::printf("r squared at 4 dp, floored, at least 0.998: %s\n", fitAccepted ? "satisfied" : "not satisfied");
    check(fitAccepted, "0.9981 is at least 0.998");

    constexpr auto flatLengths = formula::environment(
        formula::MeasuredObservations<Elapsed, 64>(formula::Rational { 1 }, formula::Rational { 2 },
                                                   formula::Rational { 4 }, formula::Rational { 7 }),
        formula::MeasuredObservations<Length, 64>(formula::Rational { 127, 10 }, formula::Rational { 127, 10 },
                                                  formula::Rational { 127, 10 }, formula::Rational { 127, 10 }));
    auto const flatLine = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(observedFit), flatLengths);
    std::printf("flat lengths: %s\n",
                flatLine.has_value() ? "a line" : std::string { formula::describe(flatLine.error()) }.c_str());
    check(!flatLine.has_value() && flatLine.error() == formula::ArithmeticError::DomainError, "a flat response has no R²");

    auto const fifty = fiftyReadings();
    auto const exactFifty = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(observedFit), fifty);
    std::printf("fifty readings at 4 decimals, exact: %s\n",
                exactFifty.has_value() ? "a line" : std::string { formula::describe(exactFifty.error()) }.c_str());
    auto const slopeOfFifty = formula::checked_evaluate<SlopeRate>(observedSlope, fifty);
    auto const startOfFifty = formula::checked_evaluate<StartLength>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            observedFit),
        fifty);
    auto const qualityOfFifty = formula::checked_evaluate<FitQuality>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
            observedFit),
        fifty);
    check(slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value(), "fifty readings, rounded");
    if (slopeOfFifty.has_value() && startOfFifty.has_value() && qualityOfFifty.has_value())
        std::printf("fifty readings at 4 decimals, rounded: slope %s, intercept %s, r squared %s\n\n",
                    decimalText(slopeOfFifty->measurement()).c_str(), decimalText(startOfFifty->measurement()).c_str(),
                    decimalText(qualityOfFifty->measurement()).c_str());
    check(slopeOfFifty.has_value() && slopeOfFifty->measurement().value() == formula::Rational { 31707, 10'000 },
          "3.1707 mm/s");
    std::printf("all checks passed: %s\n", allPassed ? "yes" : "no");
    return allPassed ? 0 : 1;
}
