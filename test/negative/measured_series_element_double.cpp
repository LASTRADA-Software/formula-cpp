// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a floating-point value is not an exact rational
//
// A series element written as a double. `Rational` refuses it in its own
// words; the series' own check stays silent, so the one mistake is one message.
#include <formula-cpp/environment.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto screens = formula::measured_series<Retained>(127, 10.3);

int main()
{
    return screens.element(0).has_value() ? 0 : 1;
}
