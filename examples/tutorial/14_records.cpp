// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 14: records. A specimen's strength is compared with a
// reference sample's, read from that sample's record by the role it plays.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;

// --8<-- [start:quantities]
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;
using StrengthRatio =
    formula::Quantity<struct StrengthRatioTag, "r", "strength as a share of the reference strength", unit::One>;

/// The role the reference sample plays.
struct Reference
{
};
// --8<-- [end:quantities]

// --8<-- [start:ratio]
constexpr auto ratio = formula::yields<StrengthRatio>(var<Strength> / formula::from_record<Reference>(var<Strength>));
// --8<-- [end:ratio]

// --8<-- [start:records]
constexpr auto specimen = formula::environment(formula::Measured<Strength> { 30 });
constexpr auto referenceSample = formula::environment(formula::Measured<Strength> { 32 });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), specimen),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), referenceSample));
// --8<-- [end:records]

// --8<-- [start:report]
/// Prints how the ratio was calculated over @p context, then the ratio.
/// False when it failed.
template <typename Context>
bool report(Context const& context)
{
    auto const explained = formula::checked_explain(ratio, context);
    if (!explained)
    {
        std::println("no ratio: {}", explained.error().error);
        return false;
    }
    std::print("{}", formula::render_trace(explained->trace, { .maxSteps = 10 }));
    std::println("{} = {}", formula::symbol_of<StrengthRatio>(), explained->outcome);
    return true;
}
// --8<-- [end:report]
} // namespace

int main()
{
    // --8<-- [start:render]
    std::println("{} = {}", formula::symbol_of<StrengthRatio>(), formula::render(ratio));
    // --8<-- [end:render]

    // --8<-- [start:evaluate]
    std::println("the reference sample tested:");
    if (!report(records))
        return 1;
    // --8<-- [end:evaluate]

    // --8<-- [start:not-yet-made]
    auto const notYetTested = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), specimen),
        formula::Record<Reference, decltype(referenceSample)>::unbound());
    std::println("the reference sample not yet tested:");
    if (!report(notYetTested))
        return 1;
    // --8<-- [end:not-yet-made]
    return 0;
}
