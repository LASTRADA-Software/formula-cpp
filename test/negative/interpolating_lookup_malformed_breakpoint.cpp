// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this interpolating lookup table has a row whose key is not a rational number
//
// A row whose key has a zero denominator: it names no point on the curve at
// all. This is a rule of its own, separate from the ordering rule, and this
// file pins the guard for it rather than the ordering guard -- which took
// getting wrong once to find.
//
// **The table has exactly ONE row, and that is the point of the file.** The
// first draft put the malformed key in the middle of three, applying this
// phase's position rule mechanically. It was the wrong table: a key that is not
// a number is not below the key after it either, so both rules fire on such a
// table, and deleting `RequireBreakpointWellFormed` entirely still left that
// file failing to compile -- through `RequireBreakpointsAscend`. The file would
// have gone on passing with the guard it exists to pin deleted, which is the
// defect `exact_lookup_unscoped_key.cpp` was found to have.
//
// A one-row table has no adjacent pair, so the ordering sweep never runs and
// this guard is the only thing standing between an author and a table with a
// row that names nothing. Measured: with the `static_assert` in
// `RequireBreakpointWellFormed` removed, this file compiles and links. The
// position rule has nothing to say about a table with one row.
//
// What this file therefore does NOT cover: the message an author gets for a
// malformed key in a table that also has neighbours. Both rules fire there and
// the diagnostic carries both sentences; no test pins which, because a test for
// it could not be shown to pin this guard rather than the other one.
// This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<1> MalformedPoint {
        formula::breakpoint(10, 0), // a zero denominator names no number
    };

    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Millimetre, MalformedPoint, formula::unit::One>(
            formula::var<Diameter>, { formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
