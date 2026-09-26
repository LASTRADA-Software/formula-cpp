// SPDX-License-Identifier: Apache-2.0
// EXPECT: does not measure the dimension this expression computes
//
// A series of masses reported as a series of apertures. The result quantity
// is named, never deduced, and checked_evaluate_series holds it to the
// expression's dimension exactly as checked_evaluate does.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Aperture: formula::Quantity<Aperture, "d", "screen aperture", formula::unit::Millimetre>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } }));

int main()
{
    return formula::checked_evaluate_series<Aperture>(formula::series<Retained, 3>, inputs).has_value() ? 0 : 1;
}
