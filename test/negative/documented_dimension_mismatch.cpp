// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
#include <formula-cpp/citation.hpp>

struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// Wrapping must not smuggle a dimensional error past the check: the wrapper
// forwards the dimension, so the addition is still refused.
inline constexpr auto broken = formula::documented(formula::var<Volume>, { .title = "A volume" }) + formula::var<Length>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
