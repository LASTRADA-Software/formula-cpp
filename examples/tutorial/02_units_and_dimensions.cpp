// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 2: units and dimensions. The loaded area is calculated
// from the specimen's two sides, and the strength is stated in a second unit.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

namespace
{
// --8<-- [start:names]
using formula::var;
namespace unit = formula::unit;
using namespace formula::literals;
// --8<-- [end:names]

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;

// --8<-- [start:sides]
using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;
// --8<-- [end:sides]

// --8<-- [start:other-unit]
using StrengthInNewtons =
    formula::Quantity<struct StrengthInNewtonsTag, "f_c", "compressive strength", unit::NewtonPerSquareMillimetre>;
// --8<-- [end:other-unit]

// --8<-- [start:formulas]
constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(var<Load> / loadedArea.expression);
// --8<-- [end:formulas]
} // namespace

int main()
{
    // --8<-- [start:environment]
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 675 });
    // --8<-- [end:environment]

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

    // --8<-- [start:evaluate-other-unit]
    auto const inNewtons = formula::checked_evaluate<StrengthInNewtons>(strength.expression, specimen);
    if (!inNewtons)
    {
        std::println("cannot calculate the strength in N/mm2: {}", inNewtons.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<StrengthInNewtons>(), *inNewtons, inNewtons->source());
    // --8<-- [end:evaluate-other-unit]
    return 0;
}
