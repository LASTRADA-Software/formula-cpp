// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_base_dimension_name_too_long
// REJECT: formula_unit_symbol_too_long
//
// A base name of 16 bytes does not fit a Symbol with its terminator. The
// base's own rule refuses it, before symbol() would refuse it in its own
// words. This must not compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension Refused = formula::base_dimension("AcmeLoyaltyUnits");

int main()
{
    return Refused.length.numerator;
}
