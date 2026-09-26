// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a sample statistic needs a sample (a series or observations), not a single value
//
// The variance of one quantity read once: a variance needs repeated
// determinations. Refused in this library's words, once. The refusing
// overload returns the value's square, so evaluating the result as a
// variance adds no dimension message, and the concept failure of the sample
// overload is never reported. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};
} // namespace

int main()
{
    auto const outcome =
        formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::var<Mass>),
                                                formula::environment(formula::Measured<Mass> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
