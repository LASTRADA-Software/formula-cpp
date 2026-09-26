// SPDX-License-Identifier: Apache-2.0
// EXPECT: renames_symbol_must_not_be_all_whitespace
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A symbol that is all whitespace leaves a blank where the quantity stands.

int main()
{
    auto const entry = formula::renames<Strength>("   ");
    return entry.symbol().size() == 1 ? 0 : 1;
}
