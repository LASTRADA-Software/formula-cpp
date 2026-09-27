// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a curve pairs a series of points with a series of values, and this is a single value
// REJECT: no matching
// REJECT: this curve pairs a domain and values of different lengths
// REJECT: this curve is read at a point that does not measure
//
// A single value given as a curve's values: there is no value at each point.
// Refused in this library's words, through an overload that takes a Node for
// no other purpose, rather than as a constraint nobody satisfied -- the task
// 5 ruling for sum and cumulative. The curve it returns is refused already,
// so neither its length check nor the interpolation over it -- read at a
// point of the wrong dimension too -- adds a message.
#include <formula-cpp/curve.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto read = formula::interpolate_at(formula::curve(formula::series<Opening, 3>, formula::var<TotalMass>),
                                                     formula::constant<formula::unit::Gram>(formula::Rational { 5 }));

int main()
{
    return read.dimension == formula::unit::Gram.dimension ? 0 : 1;
}
