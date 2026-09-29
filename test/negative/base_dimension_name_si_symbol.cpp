// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_base_dimension_name_is_an_si_base_unit_symbol
//
// A base named "kg" would read as kilograms: the symbols of the SI base
// units are refused as names. This must not compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension Refused = formula::base_dimension("kg");

int main()
{
    return Refused.length.numerator;
}
