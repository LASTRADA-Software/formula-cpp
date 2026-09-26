// SPDX-License-Identifier: Apache-2.0
// EXPECT: cumulative runs a total along a series, and this is a single value, not a series
// REJECT: no matching
//
// cumulative given a single value: there is nothing to run a total along.
// Refused in this library's words, through an overload that takes a Node for
// no other purpose, rather than as a constraint nobody satisfied.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1250 } });

int main()
{
    return formula::checked_evaluate<TotalMass>(
               formula::cumulative<formula::CumulativeDirection::FromLast>(formula::var<TotalMass>), inputs)
                   .has_value()
               ? 0
               : 1;
}
