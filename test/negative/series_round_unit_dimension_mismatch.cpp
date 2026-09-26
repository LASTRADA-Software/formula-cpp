// SPDX-License-Identifier: Apache-2.0
// EXPECT: this rounding node names a unit that does not measure the dimension of the expression it rounds
// REJECT: are not a PlacesTable<N> of one DecimalPlaces per element of its series
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
