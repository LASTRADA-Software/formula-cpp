// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an element of measured_series<Q> is a Measured<Q> of that one quantity
//
// A series element written as a bool. `Rational` is deliberately not built from
// a bool, so the series' own check refuses it, once; the conversion that would
// follow stays silent, so the one mistake is one message and no overload list.
#include <formula-cpp/environment.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto screens = formula::measured_series<Retained>(127, true);

int main()
{
    return screens.element(0).has_value() ? 0 : 1;
}
