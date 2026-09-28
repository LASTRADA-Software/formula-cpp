// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a critical-value lookup node can only be evaluated with Rep = Rational
//
// A critical-value lookup evaluated in `double`, which every lookup refuses.
// This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<2> Sizes { 3, 4 };
} // namespace

int main()
{
    constexpr auto lookup = formula::critical_value<Sizes, formula::unit::One>(
        formula::var<Specimens>, { formula::Rational { 70 }, formula::Rational { 20 } });
    auto const evaluated = formula::checked_evaluate_si<double>(
        lookup, formula::environment(formula::Measured<Specimens> { formula::Rational { 3 } }));
    return evaluated.has_value() ? 0 : 1;
}
