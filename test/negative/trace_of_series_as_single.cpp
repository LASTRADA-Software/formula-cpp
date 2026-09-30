// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: no matching
//
// A series handed to trace_of, which traces a single value. Refused as the
// series itself is refused by checked_evaluate, pointing at
// checked_evaluate_series, rather than as an overload nobody matched.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { formula::Rational { 130 } },
                                                            formula::Measured<Retained> { formula::Rational { 210 } },
                                                            formula::Measured<Retained> { formula::Rational { 97 } }));

int main()
{
    return formula::trace_of<Retained>(formula::series<Retained, 3>, inputs).empty() ? 1 : 0;
}
