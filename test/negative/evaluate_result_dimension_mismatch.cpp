// SPDX-License-Identifier: Apache-2.0
// EXPECT: does not measure the dimension this expression computes
#include <formula-cpp/evaluate.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

int main()
{
    // The expression is dimensionless; `Length` is not.
    constexpr auto inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                 formula::Measured<CementVolume> { formula::Rational { 300 } });
    auto const broken = formula::checked_evaluate<Length>(formula::var<WaterVolume> / formula::var<CementVolume>, inputs);
    return broken.has_value() ? 0 : 1;
}
