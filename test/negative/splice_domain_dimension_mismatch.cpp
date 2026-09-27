// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two curves spliced here have domains of different dimensions
// REJECT: have values of different dimensions
//
// A curve over openings in metres spliced with one over masses in grams: a
// splice sorts both curves' points into one domain, so they must measure one
// quantity. Refused once, naming both curves.
#include <formula-cpp/curve.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
{
};
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr auto spliced = formula::splice<formula::Monotone::NonDecreasing>(
    formula::curve(formula::series<Opening, 3>, formula::series<Passing, 3>),
    formula::curve(formula::series<Retained, 3>, formula::series<Passing, 3>));

int main()
{
    return static_cast<int>(decltype(spliced)::length);
}
