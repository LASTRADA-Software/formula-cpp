// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lookup table's key unit does not measure the dimension of the expression whose value selects a row
//
// A kilogram key over a millimetre (length) operand: looking a mass up in a
// table of length bands is not a lookup miss, it is a category error, and it
// must be caught here rather than surfacing as a confusing checked_convert
// failure at evaluation time.
//
// The guard is `RequireLookupKeyMatches`, shared with the interpolating lookup
// -- one rule, one sentence -- so its message names a "row" rather than one
// kind's own word for one, exactly as the corrections-count guard does. The
// interpolating kind has its own file asserting this same string, because
// deleting the guard from either node alone would leave the other's case
// failing exactly as before. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BandTable<2> Bands {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
    };

    inline constexpr auto broken = formula::banded_lookup<formula::unit::Kilogram, Bands, formula::unit::One>(
        formula::var<Diameter>, { formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
