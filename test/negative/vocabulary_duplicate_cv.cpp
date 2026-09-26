// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this vocabulary renames the same quantity twice
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// `Strength const` names the same quantity as `Strength`: `var<Strength>`
// would match both entries.
inline constexpr auto broken = formula::vocabulary(formula::renames<Strength>("R"), formula::renames<Strength const>("E"));

int main()
{
    return formula::symbol_of<Strength>(broken).size() == 1 ? 0 : 1;
}
