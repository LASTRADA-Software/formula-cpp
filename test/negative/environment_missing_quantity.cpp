// SPDX-License-Identifier: Apache-2.0
// EXPECT: provides no value for this quantity
#include <formula-cpp/environment.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Gradient: formula::Quantity<Gradient, "s", "road gradient", formula::unit::One>
{
};

// An environment that does not hold Gradient; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<Rise> { formula::Rational { 180 } });

int main()
{
    return env.get<Gradient>().has_value() ? 0 : 1;
}
