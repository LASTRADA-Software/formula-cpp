// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this breakpoint table's breakpoints do not strictly ascend
// REJECT: must be initialized by a constant expression
// REJECT: this curve pairs a domain and values of different lengths
//
// A declared domain out of order: refused by the shipped breakpoint table
// validation, whose words name a curve's domain among its uses, once on every
// compiler. The curve over it, whose values are one short, asks nothing more.
#include <formula-cpp/curve.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr formula::BreakpointTable<3> disordered { formula::breakpoint(103), formula::breakpoint(163),
                                                          formula::breakpoint(127) };

inline constexpr auto paired =
    formula::curve(formula::domain<formula::unit::Metre, disordered>(), formula::series<Passing, 2>);

int main()
{
    return static_cast<int>(decltype(paired)::length);
}
