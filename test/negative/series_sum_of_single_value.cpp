// SPDX-License-Identifier: Apache-2.0
// EXPECT: sum adds up the elements of a series, and this is a single value, not a series
// REJECT: no matching
//
// sum given a single value. A single value is its own total, so this is most
// likely a series read as var<Q> by mistake. Refused in this library's words,
// through an overload that takes a Node for no other purpose, rather than as
// a constraint nobody satisfied.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1250 } });

int main()
{
    return formula::checked_evaluate<TotalMass>(formula::sum(formula::var<TotalMass>), inputs).has_value() ? 0 : 1;
}
