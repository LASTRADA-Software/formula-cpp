// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// Three bands, but only two corrections. `std::array`'s own aggregate
// initialisation would silently value-initialise the missing third entry to
// Rational{} == 0/1, handing out a confident-looking zero correction for a
// band the author simply forgot to type -- exactly the "every answer is a
// lie" failure this node otherwise refuses on the miss side. This must not
// compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BandTable<3> Bands {
        formula::band(0, 1, 103, 1),
        formula::band(103, 1, 197, 1),
        formula::band(197, 1, 293, 1),
    };

    // Only TWO corrections for THREE bands.
    inline constexpr auto broken = formula::banded_lookup<formula::unit::Millimetre, Bands, formula::unit::One>(
        formula::var<Diameter>, { formula::Rational { 863, 1000 }, formula::Rational { 1127, 1000 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
