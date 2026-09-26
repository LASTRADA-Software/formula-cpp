// SPDX-License-Identifier: Apache-2.0
// EXPECT: renames_symbol_must_be_a_string_with_no_embedded_nul
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A NUL where the symbol's first character should be: every rendering would
// carry an invisible byte where the quantity stands.

int main()
{
    auto const entry = formula::renames<Strength>("\0");
    return entry.symbol().size() == 1 ? 0 : 1;
}
