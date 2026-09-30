// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this unsigned type can hold values above Rational's maximum
//
// A series element written as a 64-bit unsigned integer, which can hold values
// `Rational` cannot. `Rational` refuses it in its own words; the series' own
// check stays silent, so the one mistake is one message.
#include <formula-cpp/environment.hpp>

#include <cstdint>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto screens = formula::measured_series<Retained>(127, std::uint64_t { 139 });

int main()
{
    return screens.element(0).has_value() ? 0 : 1;
}
