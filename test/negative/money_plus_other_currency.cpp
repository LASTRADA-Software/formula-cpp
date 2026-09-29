// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the two sides of this addition or subtraction measure different dimensions
//
// Euros plus yen: two currencies are two dimensions, and adding them has no
// meaning until an exchange rate -- data -- has turned one into the other.
// Both sides have the same SI exponents, all zero, so only the named base
// dimensions differ. Refused once. This must not compile.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };

struct PriceInEuros: formula::Quantity<PriceInEuros, "p_e", "a price in euros", Euro>
{
};
struct PriceInYen: formula::Quantity<PriceInYen, "p_y", "a price in yen", Yen>
{
};

inline constexpr auto broken = formula::var<PriceInEuros> + formula::var<PriceInYen>;

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
