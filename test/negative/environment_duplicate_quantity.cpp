// SPDX-License-Identifier: Apache-2.0
// EXPECT: supplies the same quantity more than once
#include <formula-cpp/environment.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};

// Two entries for one quantity: there is no defensible way to pick one.
inline constexpr auto broken = formula::environment(formula::Measured<Rise> { formula::Rational { 180 } },
                                                    formula::Measured<Rise> { formula::Rational { 200 } });

int main()
{
    return broken.get<Rise>().has_value() ? 0 : 1;
}
