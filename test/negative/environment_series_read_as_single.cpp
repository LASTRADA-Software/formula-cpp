// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a series for this quantity, not a single value
// REJECT: provides no value for this quantity
//
// The environment holds Retained as a series, one mass per screen, and the
// formula reads it as one number. It does hold the quantity, so "provides no
// value" would be false; the refusal says what is actually wrong, once.
#include <formula-cpp/evaluate.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } }));

inline constexpr auto doubled = formula::var<Retained> * formula::Rational { 2 };

int main()
{
    return formula::checked_evaluate<Retained>(doubled, inputs).has_value() ? 0 : 1;
}
