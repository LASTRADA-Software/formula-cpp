// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// Three breakpoints, but only two values. `std::array`'s own aggregate
// initialisation would silently value-initialise the missing third entry to
// Rational{} == 0/1 -- and for an interpolating table a forgotten row is worse
// than for a banded one: it does not merely answer 0 at its own key, it drags
// the whole segment either side of it down towards zero, so every value between
// the last two rows comes back wrong while looking perfectly ordinary. This
// must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<3> Points {
        formula::breakpoint(0),
        formula::breakpoint(10),
        formula::breakpoint(20),
    };

    // Only TWO values for THREE breakpoints.
    inline constexpr auto broken =
        formula::interpolating_lookup<formula::unit::Millimetre, Points, formula::unit::One>(
            formula::var<Diameter>, { formula::Rational { 95, 100 }, formula::Rational { 1 } });
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
