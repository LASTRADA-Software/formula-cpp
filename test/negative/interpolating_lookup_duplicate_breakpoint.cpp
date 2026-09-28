// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this breakpoint table's breakpoints do not strictly ascend
// REJECT: must be initialized by a constant expression
//
// The same key typed on two rows. The table then claims two different values at
// one key, and the segment between those two rows has zero width for the
// interpolation to divide by -- so the author's curve is not merely imprecise
// there, it is not a function.
//
// The duplicate sits in the MIDDLE pair of four rows, neither the first pair
// nor the last, because a check confined to either end passes a table with a
// defect anywhere else. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<4> DuplicatedPoints {
        formula::breakpoint(0),
        formula::breakpoint(103),
        formula::breakpoint(103), // the same key as the row above
        formula::breakpoint(293),
    };

    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Millimetre, DuplicatedPoints, formula::unit::One>(
            formula::var<Diameter>,
            { formula::Rational { 1127, 1000 },
              formula::Rational { 853, 1000 },
              formula::Rational { 917, 1000 },
              formula::Rational { 1043, 1000 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
