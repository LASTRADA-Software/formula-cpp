// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 8: tracing. The strength is calculated together with each
// step that reached it, and a strength entered by hand has no steps.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;
using namespace formula::literals;

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;

using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;

constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };

constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength =
    formula::yields<Strength>(formula::documented(formula::rounded<tenthMpa>(var<Load> / loadedArea),
                                                  { .title = "Compressive strength",
                                                    .reference = "Example Standard 12:2020",
                                                    .section = "6.1",
                                                    .equation = "(1)",
                                                    .text = "The maximum load divided by the area of the loaded face." }));
} // namespace

int main()
{
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150.2_r }, formula::Measured<SideB> { 149.8_r }, formula::Measured<Load> { 675.4_r });

    // --8<-- [start:explain]
    auto const explained = formula::checked_explain<Strength>(strength, specimen);
    if (!explained)
    {
        std::println("cannot explain the strength: {}", explained.error().error);
        std::print("{}", formula::render_trace(explained.error().trace, { .maxSteps = 10 }));
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), explained->outcome);
    std::print("{}", formula::render_trace(explained->trace, { .maxSteps = 10 }));
    // --8<-- [end:explain]

    // --8<-- [start:entered]
    auto const typedIn = formula::environment(formula::Measured<SideA> { 150.2_r },
                                              formula::Measured<SideB> { 149.8_r },
                                              formula::Measured<Load> { 675.4_r },
                                              formula::entered(formula::Measured<Strength> { 31 }));
    auto const asEntered = formula::checked_explain<Strength>(strength, typedIn);
    if (!asEntered)
    {
        std::println("cannot explain the strength: {}", asEntered.error().error);
        std::print("{}", formula::render_trace(asEntered.error().trace, { .maxSteps = 10 }));
        return 1;
    }
    if (!asEntered->outcome.is_overridden() || !asEntered->trace.empty())
    {
        std::println("the strength entered by hand was derived: {}", asEntered->outcome);
        return 1;
    }
    std::println("entered by hand: {} = {} ({}), steps traced: {}",
                 formula::symbol_of<Strength>(),
                 asEntered->outcome,
                 asEntered->outcome.source(),
                 asEntered->trace.steps.size());
    // --8<-- [end:entered]
    return 0;
}
