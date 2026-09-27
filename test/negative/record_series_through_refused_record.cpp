// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record holds its values in an environment(...), and this is not one
// REJECT: get_series
//
// A bare Measured where environment(...) was meant for this record's values,
// and a series then read through the context built over it. The refused
// record holds detail::AbsentEnvironment, and the context is that
// environment, so it must answer a series -- every element absent -- or the
// refusal of the values would be followed by the compiler's own error for a
// missing get_series (the task 4 review's note on phase 12's series members).
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

constexpr auto records = formula::record_context(formula::record<formula::ThisRecord>(
    formula::record_key(formula::sample_id(17), formula::test_id(5)), formula::Measured<Retained> { formula::Rational { 97 } }));
} // namespace

int main()
{
    constexpr auto read = formula::sum(formula::series<Retained, 2>);
    return formula::checked_evaluate_si<formula::Rational>(read, records).has_value() ? 0 : 1;
}
