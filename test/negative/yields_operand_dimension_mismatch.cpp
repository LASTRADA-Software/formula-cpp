// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this addition or subtraction measure different dimensions
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
// REJECT: RequireResultDimension
//
// A bound area added to a length: refused once, in the words the formula it
// holds draws when added to that length, and the sum asks nothing more of the
// evaluation, the rendering or the documentation.
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", formula::unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", formula::unit::Millimetre>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;

inline constexpr auto inputs = formula::environment(formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 });

int main()
{
    constexpr auto loadedArea = formula::yields<Area>(formula::var<SideA> * formula::var<SideB>);
    constexpr auto misused = loadedArea + formula::var<SideA>;
    auto const shown = formula::render(misused);
    auto const written = formula::document(misused);
    return formula::checked_evaluate<Area>(misused, inputs).has_value() && !shown.empty() && !written.formula.empty() ? 0
                                                                                                                      : 1;
}
