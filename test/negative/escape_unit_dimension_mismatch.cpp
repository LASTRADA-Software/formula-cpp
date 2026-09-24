// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this numeric_value_of names a unit that does not measure the dimension
//
// "The numeric value of this strength in kilograms" is not a thing: a pressure
// has no reading in a mass unit. This must not compile.
#include <formula-cpp/escape.hpp>

struct Strength: formula::Quantity<Strength, "f", "material strength", formula::unit::Megapascal>
{
};

int main()
{
    constexpr auto broken = formula::numeric_value_of<formula::unit::Kilogram,
                                                      "Example Standard 9:2020 states this coefficient over "
                                                      "the numeric value in MPa">(formula::var<Strength>);
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
