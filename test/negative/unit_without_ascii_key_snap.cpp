// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a unit whose symbol is not ASCII must declare an ASCII key
// REJECT: key unit does not measure
// REJECT: breakpoints do not strictly ascend
//
// A snap keyed in micrograms per litre, written with a micro sign and no ASCII
// key: it must not compile. Refused where the snap is written, once, and by
// nothing else: the key measures what the operand does, and the permitted set
// ascends.
#include <formula-cpp/snap.hpp>

inline constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                   .magnitudeNumerator = 1,
                                                   .magnitudeDenominator = 1'000'000,
                                                   .symbolText = formula::symbol("\xc2\xb5g/L") };

struct Concentration: formula::Quantity<Concentration, "c", "an invented concentration", formula::unit::KilogramPerCubicMetre>
{
};

inline constexpr formula::BreakpointTable<2> permitted { formula::breakpoint(25), formula::breakpoint(50) };

inline constexpr auto snap =
    formula::snapped<MicrogramPerLitre, permitted, formula::SnapTie::TowardLower>(formula::var<Concentration>);

int main()
{
    return snap.tie == formula::SnapTie::TowardLower ? 0 : 1;
}
