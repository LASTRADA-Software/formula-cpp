// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 15: opaque operations and bounded retry. A least-squares line
// through load and displacement readings, and a retest repeated at most three times.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
namespace unit = formula::unit;
using namespace formula::literals;

// --8<-- [start:quantities]
/// Kilonewtons per millimetre: one is 1000000 N/m.
constexpr formula::Unit kilonewtonPerMillimetre { .dimension = formula::dim::ForcePerLength,
                                                  .magnitudeNumerator = 1'000'000,
                                                  .symbolText = formula::symbol("kN/mm"),
                                                  .decimals = 1 };

using Displacement = formula::Quantity<struct DisplacementTag, "s", "displacement", unit::Millimetre>;
using Load = formula::Quantity<struct LoadTag, "F", "load", unit::Kilonewton>;
using Stiffness = formula::Quantity<struct StiffnessTag, "k", "stiffness", kilonewtonPerMillimetre>;
using LoadAtZero = formula::Quantity<struct LoadAtZeroTag, "F_0", "load at zero displacement", unit::Kilonewton>;
using FitQuality = formula::Quantity<struct FitQualityTag, "R2", "coefficient of determination", unit::One>;
// --8<-- [end:quantities]

// --8<-- [start:fit]
constexpr auto fit =
    formula::linear_least_squares(formula::observations<Displacement, 8>,
                                  formula::observations<Load, 8>,
                                  { .title = "Stiffness", .reference = "Example Standard 12:2020", .section = "5.2" });

constexpr auto stiffness = formula::yields<Stiffness>(formula::opaque_output<"slope">(fit));
constexpr auto loadAtZero = formula::yields<LoadAtZero>(formula::opaque_output<"intercept">(fit));
constexpr auto fitQuality = formula::yields<FitQuality>(formula::opaque_output<"r squared">(fit));
// --8<-- [end:fit]

// --8<-- [start:retry]
using Retest = formula::Quantity<struct RetestTag, "f_t", "strength of one retest", unit::Megapascal>;
using AgreedStrength = formula::Quantity<struct AgreedStrengthTag, "f_a", "agreed strength", unit::Megapascal>;

constexpr auto agree = formula::abs(formula::this_attempt<AgreedStrength> - formula::previous_attempt<AgreedStrength>)
                       <= formula::constant<unit::Megapascal>(0.5_r);

constexpr auto retest = formula::retry<AgreedStrength, 3, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Retest>,
    agree,
    formula::Verdict { "test further specimens" },
    { .title = "Agreed strength", .reference = "Example Standard 12:2020", .section = "6" });
// --8<-- [end:retry]
} // namespace

int main()
{
    // --8<-- [start:evaluate-fit]
    auto const readings = formula::environment(formula::MeasuredObservations<Displacement, 8>(0.1_r, 0.2_r, 0.3_r, 0.4_r),
                                               formula::MeasuredObservations<Load, 8>(15_r, 25_r, 35_r, 45_r));

    std::println("{} = {}", formula::symbol_of<Stiffness>(), formula::render(stiffness));
    auto const explained = formula::checked_explain(stiffness, readings);
    if (!explained)
    {
        std::println("no stiffness: {}", explained.error().error);
        return 1;
    }
    std::print("{}", formula::render_trace(explained->trace, { .maxSteps = 20 }));
    std::println("{} = {}", formula::symbol_of<Stiffness>(), explained->outcome);
    // --8<-- [end:evaluate-fit]

    // --8<-- [start:other-outputs]
    auto const atZero = formula::checked_evaluate(loadAtZero, readings);
    if (!atZero)
    {
        std::println("no load at zero displacement: {}", atZero.error());
        return 1;
    }
    auto const quality = formula::checked_evaluate(fitQuality, readings);
    if (!quality)
    {
        std::println("no coefficient of determination: {}", quality.error());
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<LoadAtZero>(), *atZero);
    std::println("{} = {}", formula::symbol_of<FitQuality>(), *quality);
    // --8<-- [end:other-outputs]

    // --8<-- [start:evaluate-retry]
    std::println("{}", formula::render(retest));
    auto const results = formula::environment(formula::measured_series<Retest>(30_r, 31.2_r, 31_r));
    auto const retested = formula::explain_retry(retest, results);
    std::print("{}", formula::render_trace(retested.trace, { .maxSteps = 40 }));
    if (!retested.outcome)
    {
        std::println("the retest failed: {}", retested.outcome.error().error);
        return 1;
    }
    std::println("the retest: {} after {} attempt(s), {} = {}",
                 retested.outcome->end(),
                 retested.outcome->attempts_made(),
                 formula::symbol_of<AgreedStrength>(),
                 retested.outcome->outcome());
    // --8<-- [end:evaluate-retry]
    return 0;
}
