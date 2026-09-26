// SPDX-License-Identifier: Apache-2.0
// EXPECT: renames_symbol_must_not_hold_a_bracket_or_a_control_character
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A symbol writing a trace line's provenance clause itself: every trace
// would say an overlay fixed a quantity no overlay touched.
int main()
{
    auto const entry = formula::renames<Strength>("k_s [fixed by jurisdiction overlay]");
    return entry.symbol().empty() ? 1 : 0;
}
