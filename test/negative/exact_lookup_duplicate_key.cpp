// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this exact lookup table declares the same key twice
//
// A key declared twice makes the later row unreachable: a correction the
// author entered is silently never selected, and which of the two wins
// depends on nothing but the direction the table is scanned in. The duplicate
// sits in the MIDDLE of the table and in NON-ADJACENT rows -- an exact table
// has no declared order, so a check that compared only neighbours, or only
// the first or last pair, would wave this through. This must not compile.
//
// This file goes through `exact_lookup`. Its pair,
// `exact_lookup_duplicate_key_no_factory.cpp`, declares the same defect
// without the factory and differs from this file in nothing else; that is the
// one that pins the class-body placement of the validation, and this one does
// not. `exact_lookup_duplicate_key_two_rows.cpp` covers the smallest table
// that can carry a duplicate at all.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum class SpecimenShape
    {
        Cube100,
        Cube150,
        CylinderShort,
        Prism,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 5> DuplicatedKeys {
        SpecimenShape::Cube100,
        SpecimenShape::Cube150,
        SpecimenShape::CylinderShort,
        SpecimenShape::Cube150, // already declared two rows above
        SpecimenShape::Prism,
    };

    inline constexpr auto broken = formula::exact_lookup<DuplicatedKeys, formula::unit::One>(
        SpecimenShape::Cube100,
        { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 },
          formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
