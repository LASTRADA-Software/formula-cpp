// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
// REJECT: key unit does not measure
// REJECT: breakpoints do not strictly ascend
//
// A snap keyed in hundredths with no symbol: its permitted values would be
// written as numbers in a scale no line names. Refused where the snap is
// written, once, and by nothing else: the key measures what the operand does,
// and the permitted set ascends.
#include <formula-cpp/snap.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

struct Share: formula::Quantity<Share, "s", "an invented share", formula::unit::Percent>
{
};

inline constexpr formula::BreakpointTable<2> permitted { formula::breakpoint(25), formula::breakpoint(50) };

inline constexpr auto snap =
    formula::snapped<Hundredth, permitted, formula::SnapTie::TowardLower>(formula::var<Share>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}