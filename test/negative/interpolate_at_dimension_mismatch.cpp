// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this curve is read at a point that does not measure the dimension of its domain
//
// A curve over openings read at a percentage: the point is located among the
// curve's points, so it must measure what they measure. Refused once, naming
// the curve and the point.
#include <formula-cpp/curve.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr auto read =
    formula::interpolate_at(formula::curve(formula::series<Opening, 3>, formula::series<Passing, 3>),
                            formula::constant<formula::unit::Percent>(formula::Rational { 50 }));

int main()
{
    return read.dimension == formula::unit::Percent.dimension ? 0 : 1;
}
