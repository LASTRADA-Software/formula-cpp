// SPDX-License-Identifier: Apache-2.0
// EXPECT: the two sides of this addition or subtraction measure different
// REJECT: the two series combined here have different lengths
//
// Masses plus apertures, element by element: refused with the scalar
// operators' own words (RequireAddendsAgree), since the rule is the same rule.
// The lengths agree, so the length refusal must not speak.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Aperture: formula::Quantity<Aperture, "d", "screen aperture", formula::unit::Millimetre>
{
};

inline constexpr auto broken = formula::series<Retained, 5> + formula::series<Aperture, 5>;

int main()
{
    return static_cast<int>(decltype(broken)::length);
}
