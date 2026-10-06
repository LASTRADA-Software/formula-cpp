// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 9: calculations and worksheets. The whole test is one
// calculation, kept in a worksheet that recalculates only what a change reaches.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <cstddef>
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

// --8<-- [start:quantities]
using Height = formula::Quantity<struct HeightTag, "h", "height of the specimen", unit::Millimetre>;
using Mass = formula::Quantity<struct MassTag, "m", "mass of the specimen", unit::Kilogram>;
using Volume = formula::Quantity<struct VolumeTag, "V", "volume of the specimen", unit::CubicMillimetre>;
using Density = formula::Quantity<struct DensityTag, "rho", "density of the specimen", unit::KilogramPerCubicMetre>;
// --8<-- [end:quantities]

constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };

// --8<-- [start:calculation]
inline constexpr auto test = formula::calculation(
    formula::define<Area>(var<SideA> * var<SideB>),
    formula::define<Volume>(var<Area> * var<Height>),
    formula::define<Density>(var<Mass> / var<Volume>),
    formula::define<Strength>(formula::documented(formula::rounded<tenthMpa>(var<Load> / var<Area>),
                                                  { .title = "Compressive strength",
                                                    .reference = "Example Standard 12:2020",
                                                    .section = "6.1",
                                                    .equation = "(1)",
                                                    .text = "The maximum load divided by the area of the loaded face." })));
// --8<-- [end:calculation]

// --8<-- [start:report]
/// Asks @p sheet for the density and the strength, and prints both with how
/// many values the question recalculated and reused. False when either
/// calculation failed.
template <typename Sheet>
bool report(char const* step, Sheet& sheet)
{
    std::size_t const recomputedBefore = sheet.recomputed();
    std::size_t const reusedBefore = sheet.reused();
    auto const [density, strength] = sheet.template checked_calculate<Density, Strength>();
    if (!density)
    {
        std::println("cannot calculate the density: {}", density.error());
        return false;
    }
    if (!strength)
    {
        std::println("cannot calculate the strength: {}", strength.error());
        return false;
    }
    std::println("{:<22} {} = {:~.1HalfEven}, {} = {}, recomputed {}, reused {}",
                 step,
                 formula::symbol_of<Density>(),
                 *density,
                 formula::symbol_of<Strength>(),
                 *strength,
                 sheet.recomputed() - recomputedBefore,
                 sheet.reused() - reusedBefore);
    return true;
}
// --8<-- [end:report]
} // namespace

int main()
{
    // --8<-- [start:worksheet]
    auto sheet = formula::worksheet(test,
                                    formula::environment(formula::Measured<SideA> { 150 },
                                                         formula::Measured<SideB> { 150 },
                                                         formula::Measured<Height> { 150 },
                                                         formula::Measured<Mass> { 8.1_r },
                                                         formula::Measured<Load> { 675 }));
    if (!report("first run:", sheet))
        return 1;
    // --8<-- [end:worksheet]

    // --8<-- [start:change]
    sheet.set(formula::Measured<Load> { 676.125_r });
    if (!report("load 676.125 kN:", sheet))
        return 1;
    // --8<-- [end:change]

    // --8<-- [start:what-if]
    auto heavier = sheet.with(formula::Measured<Mass> { 8.25_r });
    if (!report("what if mass 8.25 kg:", heavier))
        return 1;
    if (!report("the worksheet itself:", sheet))
        return 1;
    // --8<-- [end:what-if]
    return 0;
}
