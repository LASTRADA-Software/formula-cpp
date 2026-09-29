// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_dimension_has_too_many_named_bases
//
// Dividing by a fifth named base dimension is refused as multiplying by one
// is: the quotient of four bases and a fifth needs five slots, and without the
// division's own guard the fifth would be silently dropped. Built as a
// constant, as dimension_too_many_named_bases.cpp explains. This must not
// compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension FourBases = formula::base_dimension("AcmeCredit") * formula::base_dimension("EUR")
                                              * formula::base_dimension("JPY") * formula::base_dimension("Voucher");
inline constexpr formula::Dimension Refused = FourBases / formula::base_dimension("Token");

int main()
{
    return Refused.length.numerator;
}
