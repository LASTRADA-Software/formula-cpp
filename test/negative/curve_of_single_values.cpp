// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a curve pairs a series of points with a series of values, and this is a single value
// REJECT: no matching
//
// Single values for both halves of a curve: one mistake, one message, naming
// the first. The other half is refused along with it.
#include <formula-cpp/curve.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto paired = formula::curve(formula::var<Opening>, formula::var<TotalMass>);

int main()
{
    return static_cast<int>(decltype(paired)::length);
}
