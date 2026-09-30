// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: define<Q> takes an expression of one value, and this is a series
// REJECT: no matching
//
// A series bound to its result quantity, defined as a calculation's value. A
// calculation holds single values, so it is refused as define<Q> refuses a
// series itself, once, in the library's words -- not as a define nobody
// matched.
#include <formula-cpp/calculation.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

int main()
{
    [[maybe_unused]] constexpr auto defined = formula::define(formula::yields<Retained>(formula::series<Retained, 3>));
    return 0;
}
