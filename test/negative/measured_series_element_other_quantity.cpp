// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an element of measured_series<Q> is a Measured<Q> of that one quantity
//
// A series of retained masses with one element that is a measurement of
// another quantity. It must not compile, and the mistake draws this one
// message, not a list of overloads or of conversions that do not exist.
#include <formula-cpp/environment.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Passing: formula::Quantity<Passing, "m_p", "mass passing a screen", formula::unit::Gram>
{
};

inline constexpr auto screens = formula::measured_series<Retained>(127, formula::Measured<Passing> { 139 });

int main()
{
    return screens.element(0).has_value() ? 0 : 1;
}
