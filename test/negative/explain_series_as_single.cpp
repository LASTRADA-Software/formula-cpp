// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; explain it with explain_series
// REJECT: no matching
//
// A series handed to explain, which traces a single value. Refused in this
// library's words, pointing at explain_series, the verb that gives a
// series' derivation -- rather than as an overload nobody matched.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 95 } }));

int main()
{
    return formula::explain<Retained>(formula::series<Retained, 3>, inputs).trace.empty() ? 1 : 0;
}
