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
// The factory's result is DISCARDED here on purpose. `ExactLookupNode`'s
// validation lives in the class body rather than in the factory, which is
// what makes a malformed table an error even when nothing is done with the
// node; moving it into the factory would leave this file compiling cleanly.
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
} // namespace

int main()
{
    (void) formula::exact_lookup<DuplicatedKeys, formula::unit::One>(
        SpecimenShape::Cube100,
        { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 },
          formula::Rational { 1 } });
    return 0;
}
