// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an exact lookup's keys must be enumerators of a scoped enumeration
//
// An UNSCOPED enumeration as a key type. It looks like it works -- the table
// declares three keys and the lookup names one of them -- but an unscoped
// enumerator converts to and from arithmetic silently, which destroys the one
// property the choice of an enumerated key was made for: that a mistyped key
// is a compile error at the offending token rather than a value that quietly
// selects the wrong row, or no row at all. `int` keys are refused for the same
// reason. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum SpecimenShape
    {
        Cube100,
        Cube150,
        CylinderShort,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 3> ShapeKeys { Cube100, Cube150, CylinderShort };

    inline constexpr auto broken = formula::exact_lookup<ShapeKeys, formula::unit::One>(
        Cube150, { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
