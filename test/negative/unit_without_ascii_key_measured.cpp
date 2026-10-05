// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a unit whose symbol is not ASCII must declare an ASCII key
//
// A measurement of a quantity declared in micrograms per litre, written with a
// micro sign and no ASCII key, named through Measured alone and never read by
// a formula: it must not compile, and is refused where the quantity's
// description is asked for, once.
#include <formula-cpp/measured.hpp>

inline constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                   .magnitudeNumerator = 1,
                                                   .magnitudeDenominator = 1'000'000,
                                                   .symbolText = formula::symbol("\xc2\xb5g/L") };

struct Concentration: formula::Quantity<Concentration, "c", "an invented concentration", MicrogramPerLitre>
{
};

int main()
{
    formula::Measured<Concentration> const reading { formula::Rational { 50 } };
    (void) reading;
    return 0;
}
