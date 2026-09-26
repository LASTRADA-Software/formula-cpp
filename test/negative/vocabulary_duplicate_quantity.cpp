// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this vocabulary renames the same quantity twice
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// Two spellings for one quantity: nothing can say which the jurisdiction means.
inline constexpr auto broken = formula::vocabulary(formula::renames<Strength>("R"), formula::renames<Strength>("E"));

int main()
{
    return formula::symbol_of<Strength>(broken).size() == 1 ? 0 : 1;
}
