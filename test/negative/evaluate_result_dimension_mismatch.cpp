// SPDX-License-Identifier: Apache-2.0
// EXPECT: does not measure the dimension this expression computes
#include <formula-cpp/evaluate.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Run: formula::Quantity<Run, "L", "horizontal distance covered", formula::unit::Millimetre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

int main()
{
    // The expression is dimensionless; `Length` is not.
    constexpr auto inputs = formula::environment(formula::Measured<Rise> { formula::Rational { 180 } },
                                                 formula::Measured<Run> { formula::Rational { 300 } });
    auto const broken = formula::checked_evaluate<Length>(formula::var<Rise> / formula::var<Run>, inputs);
    return broken.has_value() ? 0 : 1;
}
