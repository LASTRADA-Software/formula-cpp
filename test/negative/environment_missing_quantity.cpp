// SPDX-License-Identifier: Apache-2.0
// EXPECT: provides no value for this quantity
#include <formula-cpp/environment.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

// An environment that does not hold Ratio; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } });

int main()
{
    return env.get<Ratio>().has_value() ? 0 : 1;
}
