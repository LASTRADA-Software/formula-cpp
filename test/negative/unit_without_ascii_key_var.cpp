// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a unit whose symbol is not ASCII must declare an ASCII key
//
// A quantity declared in micrograms per litre, written with a micro sign and no
// ASCII key, so nothing could serialise it by a stable ASCII name: it must not
// compile, and is refused where the quantity is read, once.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit MicrogramPerLitre { .dimension = formula::dim::Mass / formula::dim::Volume,
                                                   .magnitudeNumerator = 1,
                                                   .magnitudeDenominator = 1'000'000,
                                                   .symbolText = formula::symbol("\xc2\xb5g/L") };

struct Concentration: formula::Quantity<Concentration, "c", "an invented concentration", MicrogramPerLitre>
{
};

inline constexpr auto concentration = formula::var<Concentration>;

int main()
{
    return concentration.dimension == formula::dim::Mass / formula::dim::Volume ? 0 : 1;
}
