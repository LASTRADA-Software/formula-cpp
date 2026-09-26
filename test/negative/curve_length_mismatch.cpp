// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this curve pairs a domain and values of different lengths
// REJECT: this curve is read at a point that does not measure
//
// Five declared points paired with four values: a curve pairs point i with
// value i, so the two need one length. Refused once, naming both series; the
// interpolation over the refused curve -- read at a point of the wrong
// dimension too -- asks nothing more.
#include <formula-cpp/curve.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr formula::BreakpointTable<5> screens { formula::breakpoint(11), formula::breakpoint(29),
                                                       formula::breakpoint(41), formula::breakpoint(59),
                                                       formula::breakpoint(83) };

inline constexpr auto read = formula::interpolate_at(
    formula::curve(formula::domain<formula::unit::Metre, screens>(), formula::series<Passing, 4>),
    formula::constant<formula::unit::Percent>(formula::Rational { 50 }));

int main()
{
    return read.dimension == formula::unit::Percent.dimension ? 0 : 1;
}
