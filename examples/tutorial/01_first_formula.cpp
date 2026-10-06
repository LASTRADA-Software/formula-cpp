// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 1: a first formula. A specimen's compressive strength is
// the maximum load it carried over the area that carried it.

// --8<-- [start:includes]
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>
// --8<-- [end:includes]

namespace
{
// --8<-- [start:quantities]
using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", formula::unit::Megapascal>;
// --8<-- [end:quantities]

// --8<-- [start:formula]
constexpr auto strength = formula::yields<Strength>(formula::var<Load> / formula::var<Area>);
// --8<-- [end:formula]
} // namespace

int main()
{
    // --8<-- [start:environment]
    auto const specimen = formula::environment(formula::Measured<Load> { 675 }, formula::Measured<Area> { 22500 });
    // --8<-- [end:environment]

    // --8<-- [start:evaluate]
    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<Strength>(), *result, result->source());
    // --8<-- [end:evaluate]
    return 0;
}
