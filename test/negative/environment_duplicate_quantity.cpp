// SPDX-License-Identifier: Apache-2.0
// EXPECT: supplies the same quantity more than once
#include <formula-cpp/environment.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};

// Two entries for one quantity: there is no defensible way to pick one.
inline constexpr auto broken = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                    formula::Measured<WaterVolume> { formula::Rational { 200 } });

int main()
{
    return broken.get<WaterVolume>().has_value() ? 0 : 1;
}
