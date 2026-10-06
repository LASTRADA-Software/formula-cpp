// SPDX-License-Identifier: Apache-2.0
// The README's example: the compressive strength of a specimen, the maximum
// load it carried over the area that carried it. README.md shows it whole.

// --8<-- [start:program]
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <print>

// A quantity is a type: a symbol, a description and a unit.
using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", formula::unit::Megapascal>;

// The formula, written once with ordinary operators.
constexpr auto strength = formula::yields<Strength>(formula::var<Load> / formula::var<Area>);

int main()
{
    auto const specimen = formula::environment(formula::Measured<Load> { 675 }, formula::Measured<Area> { 22500 });

    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate: {}", result.error());
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), *result);
}
// --8<-- [end:program]
