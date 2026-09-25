// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band table has a gap or overlap between two adjacent bands
//
// A table with bands 0-10, 10-20, 18-28 and 28-38 gives two answers at 19 --
// it falls in both the second and the third band. The overlap sits between
// the middle two bands (index 1 and index 2), not the last two: a validator
// that only checks the final adjacency would pass this table outright, so
// this is the case that actually pins gap-and-overlap validation rather than
// merely its shape. This must not compile.
#include <formula-cpp/band.hpp>

namespace
{
    inline constexpr formula::BandTable<4> OverlappingTable {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
        formula::band(18, 1, 28, 1), // overlap: band[2]'s low (18) < band[1]'s high (20)
        formula::band(28, 1, 38, 1),
    };

    using Checked = formula::RequireValidBandTable<OverlappingTable>;
} // namespace

int main()
{
    return Checked::value ? 1 : 0;
}
