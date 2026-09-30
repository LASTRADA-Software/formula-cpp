// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this bound formula is not an expression of one value
// REJECT: no matching
//
// A comparison bound to a quantity as though it were a formula, and
// evaluated. A comparison is a predicate, not a value, and none of the
// library's other kinds: refused once, in general words, rather than as an
// overload nobody matched.
#include <formula-cpp/formula.hpp>

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", formula::unit::Litre>;
using WaterCementRatio = formula::Quantity<struct RatioTag, "w/c", "ratio of water to cement", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    constexpr auto compared = formula::yields<WaterCementRatio>(formula::var<WaterVolume> > formula::var<CementVolume>);
    return formula::checked_evaluate(compared, inputs).has_value() ? 0 : 1;
}
