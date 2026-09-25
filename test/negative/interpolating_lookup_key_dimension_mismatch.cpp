// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lookup table's key unit does not measure the dimension of the expression whose value selects a row
//
// A kilogram key over a millimetre (length) operand, for the interpolating
// table this time. The guard is `RequireLookupKeyMatches`, the SAME one the
// banded table uses -- one rule, one sentence, one guard -- and this file
// exists to pin that the interpolating node actually instantiates it. Without
// this file, moving the guard off `InterpolatingLookupNode` would leave the
// whole suite green: `lookup_key_dimension_mismatch.cpp` would still fail
// exactly as before, because it names the banded node. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<2> Points {
        formula::breakpoint(0),
        formula::breakpoint(10),
    };

    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Kilogram, Points, formula::unit::One>(
            formula::var<Diameter>, { formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
