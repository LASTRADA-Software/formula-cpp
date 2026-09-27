// SPDX-License-Identifier: Apache-2.0
// EXPECT: cumulative runs a total along a series, and this is a single value, not a series
// REJECT: sum adds up the elements of a series
// REJECT: no matching
//
// One mistake -- a series read as var<Q> -- under two reductions: the running
// total refuses it, and the sum over that refusal says nothing more, because
// the refusing cumulative returns a series already marked refused. Counted
// by hand: one library message on cl 19.51 and g++-14.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1250 } });

int main()
{
    return formula::checked_evaluate<TotalMass>(
               formula::sum(formula::cumulative<formula::CumulativeDirection::FromLast>(formula::var<TotalMass>)), inputs)
                   .has_value()
               ? 0
               : 1;
}
