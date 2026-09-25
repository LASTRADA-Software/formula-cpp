// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// The route `lookup_short_corrections.cpp` does NOT pin: the node declared
// directly, with no factory call anywhere. `BandedLookupNode` is a public
// aggregate with public members, so a caller can write one out by hand -- and
// until `corrections` became a `Corrections<N>` rather than a bare
// `std::array<Rational, N>`, this compiled, linked, and answered `0` for the
// two bands whose corrections were never typed. Measured before the fix, on
// all three node kinds, against the installed package; the factory's parameter
// type could not see this call because there is no call.
//
// That is the same class of defect `exact_lookup_duplicate_key_no_factory.cpp`
// exists for, and it is pinned the same way: this file differs from
// `lookup_short_corrections.cpp` in precisely one thing, the absence of the
// factory. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BandTable<3> SizeBands {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
        formula::band(20, 1, 30, 1),
    };

    // Only ONE correction for THREE bands, handed straight to the aggregate.
    inline constexpr formula::BandedLookupNode<formula::unit::Millimetre,
                                               SizeBands,
                                               formula::unit::One,
                                               formula::VarNode<Diameter>>
        broken { {}, { formula::Rational { 95, 100 } }, formula::var<Diameter> };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
