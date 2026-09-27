// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this breakpoint table's breakpoints do not strictly ascend
// REJECT: key unit does not measure
// REJECT: permitted set is empty
// REJECT: must be initialized by a constant expression
//
// A permitted set out of order: refused by the shipped breakpoint table
// validation (RequireValidBreakpointTable, reused), in words that name both
// uses of a breakpoint table, and once on clang++ as on g++ and cl. The key
// unit check, wrong here too, is gated off behind it.
#include <formula-cpp/snap.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Millimetre>
{
};
struct Mass: formula::Quantity<Mass, "m", "mass", formula::unit::Gram>
{
};

inline constexpr formula::BreakpointTable<3> permitted { formula::breakpoint(163),
                                                         formula::breakpoint(127),
                                                         formula::breakpoint(197) };

inline constexpr auto snap =
    formula::snapped<formula::unit::Millimetre, permitted, formula::SnapTie::TowardLower>(formula::var<Mass>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
