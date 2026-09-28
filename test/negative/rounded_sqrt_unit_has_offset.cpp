// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rounded_sqrt names a unit with an offset, such as degrees Celsius
//
// The spread of a set of temperatures, from their variance in K^2, rounded
// "to 0.1 degC". The dimension is right, so the dimension check passes; but
// degrees Celsius state points on a scale, and the root is a spread. Had this
// compiled, the node would return 2 K in SI while recording degC as its
// unit, and the trace and the typed result would both read -271.15 degC.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
inline constexpr formula::Unit KelvinSquared { .dimension = formula::dim::Temperature * formula::dim::Temperature,
                                               .symbolText = formula::symbol("K2"),
                                               .decimals = 2 };
struct TemperatureVariance: formula::Quantity<TemperatureVariance, "s2_T", "variance of the readings", KelvinSquared>
{
};
} // namespace

int main()
{
    constexpr auto broken =
        formula::rounded_sqrt<formula::unit::Celsius, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::var<TemperatureVariance>);
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
