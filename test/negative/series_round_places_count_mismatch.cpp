// SPDX-License-Identifier: Apache-2.0
// EXPECT: this per-element rounding was given a different number of decimal places than its series has elements
// REJECT: this rounding node names a unit that does not measure the dimension of the expression it rounds
//
// Four granularities for a series of five: refused naming the table and the
// length. The unit is wrong too -- millimetres for a percentage -- and the
// REJECT pins that the count gates the unit check off: with the table
// already refused, the unit is a second message nobody needs yet.
#include <formula-cpp/series.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr formula::PlacesTable<4> places { formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 1 }, formula::DecimalPlaces { 1 } };

inline constexpr auto rounded =
    formula::rounded_elementwise<formula::unit::Millimetre, places, formula::RoundingMode::HalfEven>(formula::series<Passing, 5>);

int main()
{
    return static_cast<int>(decltype(rounded)::length);
}
