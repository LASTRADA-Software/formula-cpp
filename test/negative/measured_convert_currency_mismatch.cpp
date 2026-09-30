// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two quantities measure different dimensions, so no conversion between them exists
//
// Euros do not convert into yen: each currency is its own named base
// dimension, and the two units' SI exponents -- all zero -- are the same, so
// only the named base can tell them apart. This must not compile, and draws one message.
#include <formula-cpp/measured.hpp>

inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };

struct PriceInEuros: formula::Quantity<PriceInEuros, "p", "a price in euros", Euro>
{
};
struct PriceInYen: formula::Quantity<PriceInYen, "p", "a price in yen", Yen>
{
};

int main()
{
    return formula::checked_convert_to<PriceInYen>(formula::Measured<PriceInEuros> { 10 }).has_value() ? 1 : 0;
}
