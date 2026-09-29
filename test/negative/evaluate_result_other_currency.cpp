// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this result quantity does not measure the dimension this expression computes
//
// A sum of euros evaluated into a quantity in yen: the result would be a
// number of euros labelled yen. Both dimensions have the same SI exponents,
// all zero, so only the named base dimension can refuse it. This must not
// compile.
#include <formula-cpp/evaluate.hpp>

inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };

struct Fee: formula::Quantity<Fee, "F", "a fee in euros", Euro>
{
};
struct Surcharge: formula::Quantity<Surcharge, "S", "a surcharge in euros", Euro>
{
};
struct TotalInYen: formula::Quantity<TotalInYen, "T", "a total in yen", Yen>
{
};

int main()
{
    constexpr auto bill = formula::environment(formula::Measured<Fee> { formula::Rational { 10 } },
                                               formula::Measured<Surcharge> { formula::Rational { 3 } });
    auto const broken = formula::checked_evaluate<TotalInYen>(formula::var<Fee> + formula::var<Surcharge>, bill);
    return broken.has_value() ? 0 : 1;
}
