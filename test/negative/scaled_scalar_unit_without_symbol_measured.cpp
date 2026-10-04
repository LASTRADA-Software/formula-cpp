// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a dimensionless unit with a scale must have a symbol
//
// A measurement of a quantity declared in hundredths with no symbol, named
// through Measured alone and never read by a formula: refused where the
// quantity's description is asked for, once.
#include <formula-cpp/measured.hpp>

inline constexpr formula::Unit Hundredth { .dimension = formula::dim::Scalar,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 100 };

struct Fraction: formula::Quantity<Fraction, "w", "an invented fraction", Hundredth>
{
};

int main()
{
    formula::Measured<Fraction> const half { formula::Rational { 50 } };
    (void) half;
    return 0;
}
