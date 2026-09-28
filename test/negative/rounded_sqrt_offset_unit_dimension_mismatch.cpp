// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand
//
// A unit wrong in both ways: degrees Celsius, which has an offset, over a
// variance of masses. The dimension is the first thing to fix -- no
// temperature unit, offset or not, is the root of a mass squared -- so this
// draws the dimension message alone, and the offset check, gated on the
// dimension matching, stays silent.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassSquared: formula::Quantity<MassSquared, "s2", "variance of the determinations", GramSquared>
{
};
} // namespace

int main()
{
    constexpr auto broken =
        formula::rounded_sqrt<formula::unit::Celsius, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::var<MassSquared>);
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
