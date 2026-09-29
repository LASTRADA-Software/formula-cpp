// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula_dimension_has_too_many_named_bases
//
// Five quantities, each measured in a base dimension of its own, multiplied
// in one formula: the product's dimension would need five named bases, one
// more than a dimension holds, so the formula is refused where it is written.
// This must not compile.
#include <formula-cpp/expression.hpp>

inline constexpr formula::Unit Credit { .dimension = formula::base_dimension("AcmeCredit"),
                                        .symbolText = formula::symbol("AcmeCredit") };
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"), .symbolText = formula::symbol("EUR") };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"), .symbolText = formula::symbol("JPY") };
inline constexpr formula::Unit Token { .dimension = formula::base_dimension("Token"),
                                       .symbolText = formula::symbol("Token") };
inline constexpr formula::Unit Voucher { .dimension = formula::base_dimension("Voucher"),
                                         .symbolText = formula::symbol("Voucher") };

struct Credits: formula::Quantity<Credits, "C", "a credit balance", Credit>
{
};
struct EuroAmount: formula::Quantity<EuroAmount, "E", "an amount in euros", Euro>
{
};
struct YenAmount: formula::Quantity<YenAmount, "Y", "an amount in yen", Yen>
{
};
struct Tokens: formula::Quantity<Tokens, "T", "a token count", Token>
{
};
struct Vouchers: formula::Quantity<Vouchers, "V", "a voucher count", Voucher>
{
};

inline constexpr auto product = formula::var<Credits> * formula::var<EuroAmount> * formula::var<YenAmount>
                              * formula::var<Tokens> * formula::var<Vouchers>;

int main()
{
    return static_cast<int>(decltype(product)::dimension.length.numerator);
}
