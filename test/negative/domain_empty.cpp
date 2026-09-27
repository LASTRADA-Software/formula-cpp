// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this domain declares no points
// REJECT: this curve pairs a domain and values of different lengths
//
// A domain of no points: nothing to pair, interpolate or trace. Refused once;
// the curve over it, whose values have a point, asks nothing of its length.
#include <formula-cpp/curve.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr formula::BreakpointTable<0> nothing {};

inline constexpr auto paired =
    formula::curve(formula::domain<formula::unit::Metre, nothing>, formula::series<Passing, 1>);

int main()
{
    return static_cast<int>(decltype(paired)::length);
}
