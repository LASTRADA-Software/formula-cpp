// SPDX-License-Identifier: Apache-2.0
// EXPECT: cumulative runs a total along a series, and this is a single value, not a series
// REJECT: no matching
// REJECT: this expression is a series, not a single value
//
// A running total of a single value, evaluated where a running total is
// evaluated: through checked_evaluate_series. cl 19.51 used to print only its
// own "no matching overloaded function" here, the library's words lost; the
// refusal is now reached during the call, and the series it returns is
// accepted without a second message.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1250 } });

int main()
{
    return formula::checked_evaluate_series<TotalMass>(
               formula::cumulative<formula::CumulativeDirection::FromLast>(formula::var<TotalMass>), inputs)
                   .has_value()
               ? 0
               : 1;
}
