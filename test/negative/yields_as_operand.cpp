// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a bound formula is not an operand; use its .expression
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
// REJECT: RequireResultDimension
//
// A formula bound to the water/cement ratio, used as an operand of another
// formula and evaluated: refused once, in this library's words, and the
// refused value asks nothing more. The formula it holds, .expression, is the
// operand to use.
#include <formula-cpp/formula.hpp>

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", formula::unit::Litre>;
using WaterCementRatio = formula::Quantity<struct RatioTag, "w/c", "ratio of water to cement", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    constexpr auto ratio = formula::yields<WaterCementRatio>(formula::var<WaterVolume> / formula::var<CementVolume>);
    return formula::checked_evaluate<WaterVolume>(formula::var<WaterVolume> * ratio, inputs).has_value() ? 0 : 1;
}
