// SPDX-License-Identifier: Apache-2.0
// EXPECT: this snap's key unit does not measure the dimension of the expression it snaps
// REJECT: breakpoints do not strictly ascend
// REJECT: permitted set is empty
//
// A mass snapped to a set of millimetres: refused once, in the library's
// words.
#include <formula-cpp/snap.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Millimetre>
{
};
struct Mass: formula::Quantity<Mass, "m", "mass", formula::unit::Gram>
{
};

inline constexpr formula::BreakpointTable<3> permitted { formula::breakpoint(19, 10), formula::breakpoint(33, 10), formula::breakpoint(71, 10) };

inline constexpr auto snap =
    formula::snapped<formula::unit::Millimetre, permitted, formula::SnapTie::TowardLower>(formula::var<Mass>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
