// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band is not well-formed
//
// A table with bands 0-10, 10-10 and 10-20 declares a band that covers no
// value at all: band 1's low and high bound are the same rational, so no
// measurement can ever fall inside it. That is the same kind of author typo
// as an inverted band -- and, like an inverted band, every adjacent pair
// still shares its boundary exactly (10 == 10, 10 == 10), so only
// well-formedness catches it, not gap/overlap checking. The zero-width band
// sits in the middle, matching how the other defect classes here place
// theirs. This must not compile.
#include <formula-cpp/band.hpp>

namespace
{
    inline constexpr formula::BandTable<3> ZeroWidthMiddleBand {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 10, 1), // zero-width: low (10) equals high (10)
        formula::band(10, 1, 20, 1),
    };

    using Checked = formula::RequireValidBandTable<ZeroWidthMiddleBand>;
} // namespace

int main()
{
    return Checked::value ? 1 : 0;
}
