// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 3: exact numbers. The specimen is measured as it really
// is, the strength is calculated exactly, and then rounded for reading.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

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

constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(var<Load> / loadedArea.expression);
} // namespace

int main()
{
    // --8<-- [start:measured]
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150.2_r }, formula::Measured<SideB> { 149.8_r }, formula::Measured<Load> { 675.4_r });
    // --8<-- [end:measured]

    // --8<-- [start:evaluate]
    auto const area = formula::checked_evaluate(loadedArea, specimen);
    if (!area)
    {
        std::println("cannot calculate the area: {}", area.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<Area>(), *area, area->source());

    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<Strength>(), *result, result->source());
    // --8<-- [end:evaluate]

    // --8<-- [start:for-reading]
    std::println("{}, for reading = {:~.2HalfEven}", formula::symbol_of<Strength>(), *result);
    // --8<-- [end:for-reading]

    // --8<-- [start:exact-sum]
    std::println("0.1 + 0.2 == 0.3: {}", 0.1_r + 0.2_r == 0.3_r ? "yes" : "no");
    // --8<-- [end:exact-sum]
    return 0;
}
