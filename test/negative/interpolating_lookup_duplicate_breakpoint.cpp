// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this interpolating lookup table's breakpoints do not strictly ascend
//
// The same key typed on two rows. The table then claims two different values at
// one key, and the segment between those two rows has zero width for the
// interpolation to divide by -- so the author's curve is not merely imprecise
// there, it is not a function.
//
// The duplicate sits in the MIDDLE pair of four rows, neither the first pair
// nor the last, because a check confined to either end passes a table with a
// defect anywhere else -- task 1's review finding, applied rather than merely
// recorded. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<4> DuplicatedPoints {
        formula::breakpoint(0),
        formula::breakpoint(10),
        formula::breakpoint(10), // the same key as the row above
        formula::breakpoint(30),
    };

    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Millimetre, DuplicatedPoints, formula::unit::One>(
            formula::var<Diameter>,
            { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
