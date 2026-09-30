// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: formula: this formula names its result quantity with yields
//
// A series bound to its result quantity, handed to checked_evaluate, which
// answers with one value. Refused as the series itself is refused there,
// pointing at checked_evaluate_series; the bound quantity is the right one,
// so the Yields has nothing to add.
#include <formula-cpp/formula.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 97 } }));

int main()
{
    constexpr auto retained = formula::yields<Retained>(formula::series<Retained, 3>);
    return formula::checked_evaluate(retained, inputs).has_value() ? 0 : 1;
}
