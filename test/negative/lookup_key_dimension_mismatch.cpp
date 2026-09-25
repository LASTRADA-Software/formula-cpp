// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this banded lookup's key unit does not measure the dimension of the expression whose value selects a band
//
// A kilogram key over a millimetre (length) operand: looking a mass up in a
// table of length bands is not a lookup miss, it is a category error, and it
// must be caught here rather than surfacing as a confusing checked_convert
// failure at evaluation time. This must not compile.
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
