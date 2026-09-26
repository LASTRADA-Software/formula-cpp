// SPDX-License-Identifier: Apache-2.0
// EXPECT: this rounding node names a unit that does not measure the dimension of the expression it rounds
// REJECT: this per-element rounding was given a different number of decimal places than its series has elements
//
// A percentage rounded "to 0 dp of mm": RoundNode's own refusal, in its own
// words (RequireRoundingUnitMatches), and no count message, since the table
// has one granularity per element.
#include <formula-cpp/series.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr formula::PlacesTable<5> places { formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 1 }, formula::DecimalPlaces { 1 } };

inline constexpr auto rounded =
    formula::rounded_elementwise<formula::unit::Millimetre, places, formula::RoundingMode::HalfEven>(formula::series<Passing, 5>);

int main()
{
    return static_cast<int>(decltype(rounded)::length);
}
