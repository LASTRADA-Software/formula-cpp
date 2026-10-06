// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 5: rounding. The method rounds the strength to a tenth of
// a megapascal, and the rounding mode decides a value exactly halfway.

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

// --8<-- [start:roundings]
constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };
constexpr formula::DecimalRounding tenthMpaHalfEven { unit::Megapascal,
                                                      formula::DecimalPlaces { 1 },
                                                      formula::RoundingMode::HalfEven };
// --8<-- [end:roundings]

// --8<-- [start:formulas]
constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(formula::rounded<tenthMpa>(var<Load> / loadedArea));
constexpr auto strengthHalfEven = formula::yields<Strength>(formula::rounded<tenthMpaHalfEven>(var<Load> / loadedArea));
// --8<-- [end:formulas]
} // namespace

int main()
{
    // --8<-- [start:evaluate]
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 675.4_r });
    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("675.4 kN, half away from zero: {} = {}", formula::symbol_of<Strength>(), *result);
    // --8<-- [end:evaluate]

    // --8<-- [start:halfway]
    auto const halfway = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 676.125_r });
    auto const awayFromZero = formula::checked_evaluate(strength, halfway);
    if (!awayFromZero)
    {
        std::println("cannot calculate the strength: {}", awayFromZero.error());
        return 1;
    }
    std::println("676.125 kN, half away from zero: {} = {}", formula::symbol_of<Strength>(), *awayFromZero);

    auto const toEven = formula::checked_evaluate(strengthHalfEven, halfway);
    if (!toEven)
    {
        std::println("cannot calculate the strength: {}", toEven.error());
        return 1;
    }
    std::println("676.125 kN, half to even:        {} = {}", formula::symbol_of<Strength>(), *toEven);
    // --8<-- [end:halfway]
    return 0;
}
