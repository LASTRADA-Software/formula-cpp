// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 13: statistics. The strengths of several specimens are
// reduced to a mean and a range, and an outlier is rejected before the mean.

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
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;
using StrengthRange = formula::Quantity<struct StrengthRangeTag, "R", "range of the strengths", unit::Megapascal>;
// --8<-- [end:quantities]

// --8<-- [start:statistics]
constexpr auto threeSpecimens = formula::series<Strength, 3>;
constexpr auto mean = formula::yields<Strength>(formula::sample_mean(threeSpecimens));
constexpr auto range = formula::yields<StrengthRange>(formula::sample_range(threeSpecimens));
// --8<-- [end:statistics]

// Kept out of clang-format's hands, which would split `formula::` from
// `without_outliers`.
// clang-format off
// --8<-- [start:rejection]
/// More than 3 MPa from the mean of the pass, the value itself included.
constexpr auto threeMegapascals = formula::deviation_from_mean(formula::constant<unit::Megapascal>(3));

constexpr auto withoutOutliers = formula::without_outliers<formula::PerPass::MostExtreme,
                                                           formula::OnLimit::Keep,
                                                           formula::AtMost<2>,
                                                           formula::KeepAtLeast<3>>(
    formula::series<Strength, 5>, threeMegapascals, formula::Verdict { "test further specimens" });

constexpr auto meanKept = formula::yields<Strength>(formula::sample_mean(withoutOutliers));
// --8<-- [end:rejection]
// clang-format on
} // namespace

int main()
{
    // --8<-- [start:evaluate]
    auto const three = formula::environment(formula::measured_series<Strength>(30.2_r, 29.8_r, 31));
    auto const meanValue = formula::checked_evaluate(mean, three);
    if (!meanValue)
    {
        std::println("no mean: {}", meanValue.error());
        return 1;
    }
    auto const rangeValue = formula::checked_evaluate(range, three);
    if (!rangeValue)
    {
        std::println("no range: {}", rangeValue.error());
        return 1;
    }
    std::println("three specimens:");
    std::println("{} = {}, for reading {:~.2HalfEven}", formula::render(mean), *meanValue, *meanValue);
    std::println("{} = {}", formula::render(range), *rangeValue);
    // --8<-- [end:evaluate]

    // --8<-- [start:reject]
    auto const five = formula::environment(formula::measured_series<Strength>(30.2_r, 29.8_r, 31, 30.4_r, 36));
    std::println("five specimens:");
    std::println("{}", formula::render(withoutOutliers));
    auto const kept = formula::checked_explain(meanKept, five);
    if (!kept)
    {
        std::println("no mean of the specimens kept: {}", kept.error().error);
        return 1;
    }
    std::print("{}", formula::render_trace(kept->trace, { .maxSteps = 20 }));
    std::println("mean of the specimens kept = {}", kept->outcome);
    // --8<-- [end:reject]
    return 0;
}
