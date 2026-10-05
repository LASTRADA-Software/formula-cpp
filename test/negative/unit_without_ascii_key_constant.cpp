// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a unit whose symbol is not ASCII must declare an ASCII key
//
// A coefficient stated in micrograms per litre, written with a micro sign and
// no ASCII key: it must not compile, and is refused where it is written, once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                   .magnitudeNumerator = 1,
                                                   .magnitudeDenominator = 1'000'000,
                                                   .symbolText = formula::symbol("\xc2\xb5g/L") };

inline constexpr auto limit = formula::constant<MicrogramPerLitre>(formula::Rational { 50 });

int main()
{
    return limit.number == formula::Rational { 50 } ? 0 : 1;
}
