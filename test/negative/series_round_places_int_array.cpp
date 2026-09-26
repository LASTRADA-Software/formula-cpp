// SPDX-License-Identifier: Apache-2.0
// EXPECT: are not a PlacesTable<N> of one DecimalPlaces per element of its series
// REJECT: different number
//
// Five granularities of the right count but the wrong type, plain ints:
// refused as not a PlacesTable -- a message that is true of it, where "a
// different number of decimal places" would not be.
#include <formula-cpp/series.hpp>

#include <array>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 130 } }, formula::Measured<Retained> { formula::Rational { 210 } },
    formula::Measured<Retained> { formula::Rational { 95 } }, formula::Measured<Retained> { formula::Rational { 340 } },
    formula::Measured<Retained> { formula::Rational { 28 } }));

inline constexpr std::array<int, 5> places { 0, 0, 0, 1, 1 };

inline constexpr auto rounded = formula::rounded_elementwise<formula::unit::Gram, places, formula::RoundingMode::HalfEven>(
    formula::series<Retained, 5>);

int main()
{
    return static_cast<int>(decltype(rounded)::length);
}
