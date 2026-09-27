// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two curves spliced here have values of different dimensions
// REJECT: have domains of different dimensions
//
// Percentages passing spliced with masses retained: a splice rescales
// nothing, so putting one basis onto the other is the author's to write,
// before splicing. Refused once, naming both curves.
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
    formula::curve(formula::series<Opening, 3>, formula::series<Retained, 3>));

int main()
{
    return static_cast<int>(decltype(spliced)::length);
}
