// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: holds a series for this quantity
//
// The throwing spelling, evaluate, refuses a series as checked_evaluate does.
// A series has no throwing spelling of its own: an exception would drop the
// position of the element that failed.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } }));

int main()
{
    return formula::evaluate<Retained>(formula::series<Retained, 3>, inputs).is_value() ? 0 : 1;
}
