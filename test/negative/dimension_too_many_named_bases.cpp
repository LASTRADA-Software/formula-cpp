// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_dimension_has_too_many_named_bases
//
// A dimension holds at most four named base dimensions. A product of five
// distinct ones is refused where it is built, naming the limit it broke,
// rather than dropping one of them. Built as a constant rather than written
// straight into a template argument: there cl reports only an invalid template
// argument and does not name the function. This must not compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension FiveBases = formula::base_dimension("AcmeCredit") * formula::base_dimension("EUR")
                                              * formula::base_dimension("JPY") * formula::base_dimension("Token")
                                              * formula::base_dimension("Voucher");

int main()
{
    return FiveBases.length.numerator;
}
