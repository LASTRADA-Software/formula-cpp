// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand
//
// The same mistake as `rounded_sqrt_unit_dimension_mismatch.cpp`, on the route
// that case cannot reach: a `RoundedRootNode` aggregate-initialised directly,
// with no call to `rounded_sqrt()`. Only this case tells a check in the class
// body from one in the factory. Here the radicand is a variance of masses and
// the unit a length, the other half of the mistake: the right radicand, the
// wrong unit.
//
// The radicand is named by `decltype` of a `constexpr` variable, so it is
// `const`-qualified, as it would be for anyone writing this out.
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

inline constexpr auto radicand = formula::var<MassSquared>;
inline constexpr formula::RoundedRootNode<formula::unit::Millimetre,
                                          formula::DecimalPlaces { 2 },
                                          formula::RoundingMode::HalfAwayFromZero,
                                          decltype(radicand)>
    broken { {}, radicand };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
