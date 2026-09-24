// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
#include <formula-cpp/expression.hpp>

struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// A volume minus a length has no meaning, and must not compile.
inline constexpr auto broken = formula::var<Volume> - formula::var<Length>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
