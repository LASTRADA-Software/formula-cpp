// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// Four keys, but only three corrections. `std::array`'s own aggregate
// initialisation would silently value-initialise the missing fourth entry to
// Rational{} == 0/1, handing out a confident-looking zero correction for a row
// the author simply forgot to type -- exactly the "every answer is a lie"
// failure this node otherwise refuses on the miss side. The same hole, the
// same `Corrections<N>` wrapper and the same diagnostic as the banded case:
// see `lookup_short_corrections.cpp`, whose EXPECT line is deliberately the
// same string. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum class SpecimenShape
    {
        CubeSmall,
        CubeLarge,
        CylinderShort,
        CylinderTall,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 4> ShapeKeys {
        SpecimenShape::CubeSmall,
        SpecimenShape::CubeLarge,
        SpecimenShape::CylinderShort,
        SpecimenShape::CylinderTall,
    };

    // Only THREE corrections for FOUR keys.
    inline constexpr auto broken = formula::exact_lookup<ShapeKeys, formula::unit::One>(
        SpecimenShape::CubeSmall,
        { formula::Rational { 1093, 1000 }, formula::Rational { 1127, 1000 }, formula::Rational { 917, 1000 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
