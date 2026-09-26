// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: renames<Q>("") gives the quantity an empty symbol
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// An empty symbol: every formula reading Strength would render a blank.
inline constexpr auto broken = formula::vocabulary(formula::renames<Strength>(""));

int main()
{
    return formula::symbol_of<Strength>(broken).empty() ? 1 : 0;
}
