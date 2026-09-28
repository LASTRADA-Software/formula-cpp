// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band table has a gap or overlap between two adjacent bands
// REJECT: must be initialized by a constant expression
//
// Proves the reuse, not merely the declaration: band.hpp's validation must be
// reachable through an actual `banded_lookup(...)` call, not only through
// `RequireValidBandTable` used directly (band_gap.cpp already pins that). A
// gap in the middle pair, not the first or the last, for the same reason
// band_gap.cpp's own table is shaped this way. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };
    struct SizeCorrection: formula::Quantity<SizeCorrection, "k", "size correction factor", formula::unit::One>
    {
    };

    inline constexpr formula::BandTable<4> GappedTable {
        formula::band(0, 1, 103, 1),
        formula::band(103, 1, 197, 1),
        formula::band(241, 1, 331, 1), // gap: band[1]'s high (197) != band[2]'s low (241)
        formula::band(331, 1, 421, 1),
    };

    inline constexpr auto broken =
        formula::banded_lookup<formula::unit::Millimetre, GappedTable, formula::unit::One>(
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
