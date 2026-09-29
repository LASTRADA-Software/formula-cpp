// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_base_dimension_name_must_be_a_letter_then_letters_or_digits
//
// A base name is also its coherent unit's symbol, joined with '/' into
// compound unit text, so a name holding a '/' is refused. This must not
// compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension Refused = formula::base_dimension("EUR/kWh");

int main()
{
    return Refused.length.numerator;
}
