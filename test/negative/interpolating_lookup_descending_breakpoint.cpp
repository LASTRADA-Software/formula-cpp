// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this interpolating lookup table's breakpoints do not strictly ascend
//
// Two rows typed out of order -- the transposition an author makes copying a
// published curve row by row. It is the other half of the one rule
// `RequireBreakpointsAscend` states (`first < second`), and it shares that
// rule's one diagnostic deliberately, exactly as `band.hpp` gives an inverted
// band and a zero-width band one message: the message names both offending
// keys, so an author sees which case they have.
//
// A separate file from `interpolating_lookup_duplicate_breakpoint.cpp` because
// the two are different author typos and a guard could be broken for one
// without the other -- a comparison relaxed from `<` to `<=` still catches this
// file and stops catching that one. The defect sits in the MIDDLE pair of four
// rows, for the reason that file gives. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<4> DescendingPoints {
        formula::breakpoint(0),
        formula::breakpoint(20),
        formula::breakpoint(10), // below the row above it
        formula::breakpoint(30),
    };

    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Millimetre, DescendingPoints, formula::unit::One>(
            formula::var<Diameter>,
            { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
