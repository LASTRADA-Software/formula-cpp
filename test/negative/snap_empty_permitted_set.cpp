// SPDX-License-Identifier: Apache-2.0
// EXPECT: this snap's permitted set is empty
// REJECT: RequireBreakpoint
// REJECT: RequireValidBreakpointTable
// REJECT: key unit does not measure
// REJECT: no matching
//
// An empty permitted set: refused once. The breakpoint validation, which an
// empty table would pass vacuously, and the key unit check -- wrong here too,
// a mass snapped in millimetres -- are gated off behind the refusal.
#include <formula-cpp/snap.hpp>

struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Millimetre>
{
};
struct Mass: formula::Quantity<Mass, "m", "mass", formula::unit::Gram>
{
};

inline constexpr formula::BreakpointTable<0> permitted {};

inline constexpr auto snap =
    formula::snapped<formula::unit::Millimetre, permitted, formula::SnapTie::TowardLower>(formula::var<Mass>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
