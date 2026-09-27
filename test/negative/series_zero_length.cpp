// SPDX-License-Identifier: Apache-2.0
// EXPECT: this series has no elements
// REJECT: no matching
//
// A series over no points: measured_series<Q>() builds an empty entry, and
// series<Q, 0> would read it and evaluate to nothing at all. Refused once, in
// the library's words, as a series constant of no elements is.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto none = formula::series<Retained, 0>;

int main()
{
    return static_cast<int>(decltype(none)::length);
}
