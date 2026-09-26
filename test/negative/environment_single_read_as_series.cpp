// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a single value for this quantity, not a series
// REJECT: provides no value for this quantity
// REJECT: holds a series of a different length
//
// The environment holds one retained mass, and the formula reads it as a
// series of three. Neither a length nor "no value" is the mistake, so neither
// of those refusals may speak.
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::Measured<Retained> { formula::Rational { 130 } });

int main()
{
    return formula::checked_evaluate_series<Retained>(formula::series<Retained, 3>, inputs).has_value() ? 0 : 1;
}
