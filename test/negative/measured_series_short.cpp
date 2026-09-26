// SPDX-License-Identifier: Apache-2.0
// EXPECT: this series was given a different number of elements than its length
//
// A series of five screens written as a braced list of three. `std::array`'s
// own aggregate initialisation would make the last two elements absent,
// silently: two screens nobody typed would read as "not measured". This must
// not compile, and the message names both counts (Given = 3, Length = 5).
#include <formula-cpp/environment.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr formula::MeasuredSeries<Retained, 5> screens({ formula::Measured<Retained> { formula::Rational { 130 } },
                                                                formula::Measured<Retained> { formula::Rational { 210 } },
                                                                formula::Measured<Retained> { formula::Rational { 95 } } });

int main()
{
    return screens.element(4).has_value() ? 0 : 1;
}
