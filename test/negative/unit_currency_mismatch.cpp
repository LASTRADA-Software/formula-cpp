// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two units measure different dimensions
//
// Euros do not convert into yen: each currency is its own named base
// dimension, and the two units' SI exponents -- all zero -- are the same, so
// only the named base can tell them apart. This must not compile.
#include <formula-cpp/unit.hpp>

inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };

int main()
{
    return formula::RequireSameUnitDimension<Euro, Yen>::value ? 1 : 0;
}
