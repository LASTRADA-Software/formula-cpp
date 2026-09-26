// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a rounding node cannot be evaluated with Rep = double
//
// A rounded square root evaluated in `double`. Under any representation but
// `Rational` the node takes that representation's own root and then its own
// rounding, `RepRounding<Rep>::round_in` -- and `RepRounding<double>` refuses,
// for every rounding node alike, because a decimal granularity is what binary
// floating point cannot honour. So this is refused with that specialisation's
// message, the one `rounded<...>` under `double` draws, and not with a second
// one of this node's own.
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
    constexpr auto spread =
        formula::rounded_sqrt<formula::unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::var<MassSquared>);
    auto const evaluated = formula::checked_evaluate_si<double>(
        spread, formula::environment(formula::Measured<MassSquared> { formula::Rational { 427, 125 } }));
    return evaluated.has_value() ? 0 : 1;
}
