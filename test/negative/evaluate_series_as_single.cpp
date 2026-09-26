// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: holds a series for this quantity
//
// A series handed to checked_evaluate, which answers with one value. Refused
// in this library's words rather than as a constraint nobody satisfied, and
// before anything reads the environment: that read would be refused too, for
// the same one mistake.
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
    return formula::checked_evaluate<Retained>(formula::series<Retained, 3>, inputs).has_value() ? 0 : 1;
}
