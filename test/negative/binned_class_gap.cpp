// SPDX-License-Identifier: Apache-2.0
// EXPECT: this band table has a gap or overlap between two adjacent bands
// REJECT: this binning's key unit
// REJECT: declares no classes
//
// Classes 0 to under 127 m and 139 to under 197 m leave 127 to 139 m in none:
// the shipped band validation refuses it once, and the key check is gated off.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};

inline constexpr formula::BandTable<2> gapped { formula::band(0, 1, 127, 1), formula::band(139, 1, 197, 1) };

inline constexpr auto counted = formula::binned<formula::unit::Gram, gapped>(formula::observations<Size, 3>);

int main()
{
    return counted.length == 2 ? 0 : 1;
}
