// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two series combined here have different lengths
// REJECT: the two sides of this addition or subtraction measure different
//
// Two series that disagree in length AND in dimension. The length is refused,
// and the dimension check is gated off behind it, so the additive message
// does not add a second refusal for the one expression: once the lengths
// disagree, no element pairs with another for a dimension to be compared.
// Removing the gate makes the REJECT fire -- the case that shows the gate is
// load bearing, which series_elementwise_length_mismatch, whose dimensions
// agree, cannot.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Aperture: formula::Quantity<Aperture, "d", "screen aperture", formula::unit::Millimetre>
{
};

inline constexpr auto broken = formula::series<Retained, 5> + formula::series<Aperture, 4>;

int main()
{
    return static_cast<int>(decltype(broken)::length);
}
