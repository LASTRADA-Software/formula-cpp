// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band table has a gap or overlap between two adjacent bands
//
// A table with bands 0-103, 103-197, 241-331 and 331-421 leaves the value 223
// undefined -- exactly the kind of typo a real published table contains. The
// gap sits between the middle two bands (index 1 and index 2), not the last
// two: a validator that only checks the final adjacency would pass this
// table outright, so this is the case that actually pins gap-and-overlap
// validation rather than merely its shape. This must not compile.
#include <formula-cpp/band.hpp>

namespace
{
    inline constexpr formula::BandTable<4> GappedTable {
        formula::band(0, 1, 103, 1),
        formula::band(103, 1, 197, 1),
        formula::band(241, 1, 331, 1), // gap: band[1]'s high (197) != band[2]'s low (241)
        formula::band(331, 1, 421, 1),
    };

    using Checked = formula::RequireValidBandTable<GappedTable>;
} // namespace

int main()
{
    return Checked::value ? 1 : 0;
}
