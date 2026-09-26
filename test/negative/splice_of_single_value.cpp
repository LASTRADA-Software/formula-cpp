// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: splice joins two curves, and this is a single value, not a curve
// REJECT: no matching
// REJECT: have domains of different dimensions
// REJECT: have values of different dimensions
//
// A single value spliced with a curve, written first: there are no points to
// sort into the union. Refused in this library's words, once; the splice it
// returns holds a refused curve, so neither of the splice's own dimension
// checks -- both of which a mass against openings and percentages would fail
// -- adds a message.
#include <formula-cpp/curve.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto spliced = formula::splice<formula::Monotone::NonDecreasing>(
    formula::var<TotalMass>, formula::curve(formula::series<Opening, 3>, formula::series<Passing, 3>));

int main()
{
    return static_cast<int>(decltype(spliced)::length);
}
