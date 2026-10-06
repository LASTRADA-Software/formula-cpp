// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this comparison measure different dimensions
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
//
// A bound area compared with a length, as a constraint compares, then
// checked, rendered and documented: refused once, in the words the formula it
// holds draws when compared with that length, and the comparison asks nothing
// more of any of the three.
#include <formula-cpp/constraint.hpp>
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
    constexpr auto tooSmall =
        formula::constraint(loadedArea >= formula::var<SideA>, formula::Verdict { "too small a face to load" });
    auto const shown = formula::render(tooSmall);
    auto const written = formula::document(tooSmall);
    return formula::check(tooSmall, inputs).is_satisfied() && !shown.empty() && !written.formula.empty() ? 0 : 1;
}
