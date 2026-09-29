// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this addition or subtraction measure different dimensions
//
// Euros are not a bare ratio: a price plus a pure number has no meaning. The
// two sides have the same SI exponents -- all zero -- so only the named base
// dimension can tell them apart, and it must. Refused once. This must not
// compile.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };

struct Price: formula::Quantity<Price, "p", "a price", Euro>
{
};

inline constexpr auto broken = formula::var<Price> + formula::Rational { 1, 2 };

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
