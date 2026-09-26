// SPDX-License-Identifier: Apache-2.0
// EXPECT: is not a constant expression
#include <formula-cpp/vocabulary.hpp>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A symbol in a local buffer: a trace step keeping a view of it would dangle
// once the buffer went out of scope, so `renames` refuses it.
int main()
{
    char buffer[] = { 'R', '\0' };
    auto const entry = formula::renames<Strength>(buffer);
    return entry.symbol().size() == 1 ? 0 : 1;
}
