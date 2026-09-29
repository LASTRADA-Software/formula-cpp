// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this critical value's count is not dimensionless
// REJECT: sample-size table
// REJECT: stated in a unit other than a bare number
//
// An amount of money where the number of determinations belongs. A currency
// is a named base dimension, not a bare number, so it is refused as a mass
// is -- once, as not dimensionless, and not also as a bare number stated in
// the wrong unit. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };

struct Fee: formula::Quantity<Fee, "F", "a testing fee", Euro>
{
};

inline constexpr formula::SampleSizeTable<2> Sizes { 3, 4 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Sizes, formula::unit::One>(
        formula::var<Fee>, { formula::Rational { 70 }, formula::Rational { 20 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
