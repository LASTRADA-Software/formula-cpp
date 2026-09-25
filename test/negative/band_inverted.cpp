// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band is not well-formed
//
// A table with bands 0-10, 10-5 and 5-15 mis-buckets silently: band 1 is
// inverted (its low bound, 10, is not below its high bound, 5), yet every
// adjacent pair still shares its boundary exactly (10 == 10, 5 == 5) -- a
// validator that only checked for gaps and overlaps would pass this table
// outright. The inversion sits in the middle band, not the first or the
// last, for the same reason the gap and overlap cases keep a middle-position
// test: a check exercised at only one position can silently be one that
// only works there. This must not compile.
#include <formula-cpp/band.hpp>

namespace
{
    inline constexpr formula::BandTable<3> InvertedMiddleBand {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 5, 1), // inverted: low (10) is not below high (5)
        formula::band(5, 1, 15, 1),
    };

    using Checked = formula::RequireValidBandTable<InvertedMiddleBand>;
} // namespace

int main()
{
    return Checked::value ? 1 : 0;
}
