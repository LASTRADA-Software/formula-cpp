// SPDX-License-Identifier: Apache-2.0
// EXPECT: provides no value for this quantity
// The refusal of environment_missing_quantity.cpp, for quantities declared by
// alias.
#include <formula-cpp/environment.hpp>

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using Ratio = formula::Quantity<struct RatioTag, "w/c", "water/cement ratio", formula::unit::One>;

// An environment that does not hold Ratio; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 183 } });

int main()
{
    return env.get<Ratio>().has_value() ? 0 : 1;
}
