// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 12: series. One mix's strength at 7, 14 and 28 days is one
// quantity at three ages, and each is calculated as a share of the 28-day strength.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <array>
#include <cstddef>
#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;

// --8<-- [start:quantities]
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength at an age", unit::Megapascal>;
using FinalStrength =
    formula::Quantity<struct FinalStrengthTag, "f_c28", "compressive strength at 28 days", unit::Megapascal>;
using Share = formula::Quantity<struct ShareTag, "s", "share of the 28-day strength", unit::One>;
// --8<-- [end:quantities]

// --8<-- [start:share]
constexpr auto shareOfFinal = formula::yields<Share>(formula::series<Strength, 3> / var<FinalStrength>);
// --8<-- [end:share]

// --8<-- [start:report]
/// The ages the strengths are taken at, in days, one per element.
constexpr std::array<int, 3> ages { 7, 14, 28 };

/// Prints how each share of @p specimens was calculated, then each share
/// rounded for reading. False when the series failed.
template <typename Env>
bool report(Env const& specimens)
{
    auto const shares = formula::explain_series(shareOfFinal, specimens);
    std::print("{}", formula::render_trace(shares.trace, { .maxSteps = 10 }));
    if (!shares.outcome)
    {
        std::println("no shares: {}", shares.outcome.error().error);
        return false;
    }
    for (std::size_t at = 0; at < ages.size(); ++at)
        std::println("{} days: {} = {:~.3HalfEven}", ages[at], formula::symbol_of<Share>(), shares.outcome->element(at));
    return true;
}
// --8<-- [end:report]
} // namespace

int main()
{
    // --8<-- [start:render]
    std::println("{} = {}", formula::symbol_of<Share>(), formula::render(shareOfFinal));
    // --8<-- [end:render]

    // --8<-- [start:evaluate]
    auto const recorded =
        formula::environment(formula::measured_series<Strength>(21, 26, 30), formula::Measured<FinalStrength> { 30 });
    std::println("all three recorded:");
    if (!report(recorded))
        return 1;
    // --8<-- [end:evaluate]

    // --8<-- [start:absent]
    auto const oneUnrecorded = formula::environment(formula::measured_series<Strength>(21, formula::not_measured, 30),
                                                    formula::Measured<FinalStrength> { 30 });
    std::println("the 14-day strength not recorded:");
    if (!report(oneUnrecorded))
        return 1;
    // --8<-- [end:absent]
    return 0;
}
