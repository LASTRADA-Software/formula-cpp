// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
//
// A quantity declared in hundredths with no symbol: one half would be shown as
// 50, a number in a scale no line names. Refused where the quantity is read,
// once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

struct Fraction: formula::Quantity<Fraction, "w", "an invented fraction", Hundredth>
{
};

inline constexpr auto fraction = formula::var<Fraction>;

int main()
{
    return fraction.dimension == formula::dim::Scalar ? 0 : 1;
}