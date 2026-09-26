// SPDX-License-Identifier: Apache-2.0
// EXPECT: renames_symbol_must_be_a_string_with_no_embedded_nul
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A character array with no terminating NUL: its last character would be
// dropped as if it were one.
inline constexpr char unterminated[] = { 'R', 'x' };

int main()
{
    auto const entry = formula::renames<Strength>(unterminated);
    return entry.symbol().size() == 1 ? 0 : 1;
}
