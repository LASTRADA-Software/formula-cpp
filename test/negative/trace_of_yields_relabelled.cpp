// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula names its result quantity with yields
// REJECT: no matching
//
// A formula bound to the water/cement ratio, traced for another dimensionless
// quantity. The dimensions agree, so nothing else could refuse it: the formula
// says which quantity it computes, and the call names another. Refused by the
// overload that takes a bound formula, in this library's words, rather than
// as an overload nobody matched.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", formula::unit::One>
{
};
struct AirContent: formula::Quantity<AirContent, "a", "air content", formula::unit::One>
{
};

inline constexpr auto ratio = formula::yields<WaterCementRatio>(formula::var<WaterVolume> / formula::var<CementVolume>);
inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    return formula::trace_of<AirContent>(ratio, inputs).empty() ? 1 : 0;
}
