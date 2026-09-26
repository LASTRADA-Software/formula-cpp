// SPDX-License-Identifier: Apache-2.0
// EXPECT: breakpoints do not strictly ascend
// REJECT: key unit does not measure
// REJECT: permitted set is empty
//
// A permitted set out of order: refused by the shipped breakpoint table
// validation (RequireValidBreakpointTable, reused, whose words name an
// interpolating lookup table -- the same table type). The key unit check,
// wrong here too, is gated off behind it.
#include <formula-cpp/snap.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Millimetre>
{
};
struct Mass: formula::Quantity<Mass, "m", "mass", formula::unit::Gram>
{
};

inline constexpr formula::BreakpointTable<3> permitted { formula::breakpoint(33, 10), formula::breakpoint(19, 10), formula::breakpoint(71, 10) };

inline constexpr auto snap =
    formula::snapped<formula::unit::Millimetre, permitted, formula::SnapTie::TowardLower>(formula::var<Mass>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
