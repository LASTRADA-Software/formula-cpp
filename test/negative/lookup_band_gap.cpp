// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this band table has a gap or overlap between two adjacent bands
//
// Proves the reuse, not merely the declaration: task 1's validation must be
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
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
        formula::band(25, 1, 35, 1), // gap: band[1]'s high (20) != band[2]'s low (25)
        formula::band(35, 1, 45, 1),
    };

    inline constexpr auto broken =
        formula::banded_lookup<formula::unit::Millimetre, GappedTable, formula::unit::One>(
            formula::var<Diameter>,
            { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
