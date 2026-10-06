// SPDX-License-Identifier: Apache-2.0
// EXPECT: provides no value for this quantity
// The refusal of environment_missing_quantity.cpp, for quantities declared by
// alias.
#include <formula-cpp/environment.hpp>

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;

// An environment that does not hold Gradient; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<Rise> { formula::Rational { 183 } });

int main()
{
    return env.get<Gradient>().has_value() ? 0 : 1;
}
