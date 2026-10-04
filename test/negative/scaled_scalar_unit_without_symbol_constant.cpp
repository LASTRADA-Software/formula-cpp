// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
//
// A coefficient stated in hundredths with no symbol: refused where it is
// written, once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

inline constexpr auto half = formula::constant<Hundredth>(formula::Rational { 50 });

int main()
{
    return half.number == formula::Rational { 50 } ? 0 : 1;
}